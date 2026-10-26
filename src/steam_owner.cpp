// Steam Owner - AMX Mod X module
//
// (Re)HLDS authenticates players with SteamGameServer()->SendUserConnectAndAuthenticate(), and Steam
// answers with the GSClientApprove_t callback. Current Steamworks SDK defines it as
//     struct GSClientApprove_t { CSteamID m_SteamID; CSteamID m_OwnerSteamID; };
// where m_OwnerSteamID is the owner of the game license (differs from m_SteamID with Family Sharing).
// The engine was built against an older SDK that only knows m_SteamID, so the owner is ignored.
//
// This module registers its own listener for the same callback (Steam allows several listeners per
// callback), reads m_OwnerSteamID and passes it to plugins through the client_steam_owner() forward.

#include "amxxmodule.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

typedef unsigned char uint8_t_;
typedef unsigned long long uint64_t_;

// Must match the binary layout of CCallbackBase in the steam_api library used by (Re)HLDS
// (rehlds/public/steam/steam_api.h): vtable with Run(void*), Run(void*, bool, SteamAPICall_t),
// GetCallbackSizeBytes(), then uint8 flags and int callback id
class CCallbackBase
{
public:
	CCallbackBase() { m_nCallbackFlags = 0; m_iCallback = 0; }
	virtual void Run(void *pvParam) = 0;
	virtual void Run(void *pvParam, bool bIOFailure, uint64_t_ hSteamAPICall) = 0;
	virtual int GetCallbackSizeBytes() = 0;

protected:
	enum { k_ECallbackFlagsRegistered = 0x01, k_ECallbackFlagsGameServer = 0x02 };
	uint8_t_ m_nCallbackFlags;
	int m_iCallback;
};

extern "C" void SteamAPI_RegisterCallback(CCallbackBase *pCallback, int iCallback);
extern "C" void SteamAPI_UnregisterCallback(CCallbackBase *pCallback);

// k_iSteamGameServerCallbacks (200) + 1
#define GSCLIENTAPPROVE_CALLBACK	201

struct GSClientApprove_t
{
	uint64_t_ m_SteamID;
	uint64_t_ m_OwnerSteamID;
};

// Last approvals, so plugins can also ask for the owner later through a native
// Big enough that a player who stays connected isn't pushed out by others joining (16 bytes per entry)
#define OWNER_CACHE_SIZE	256

struct OwnerEntry
{
	uint64_t_ player;
	uint64_t_ owner;
};

static OwnerEntry g_Cache[OWNER_CACHE_SIZE];
static int g_iCacheNext = 0;

static int g_fwdOwner = -1;
static bool g_bRegistered = false;

// Public universe, individual account, desktop instance: the upper 32 bits of a normal player's SteamID64
static bool IsIndividualSteamId(uint64_t_ id)
{
	return (id >> 32) == 0x01100001ULL && (id & 0xFFFFFFFFULL) != 0;
}

static void SteamIdToAuthId(uint64_t_ id, char *out, size_t len)
{
	unsigned int account = (unsigned int)(id & 0xFFFFFFFFULL);
	snprintf(out, len, "STEAM_0:%u:%u", account & 1, account >> 1);
}

// "STEAM_0:X:Y" -> SteamID64, 0 on failure. Only STEAM_0: real Steam players in GoldSrc always have it,
// while with Reunion STEAM_1:X:Y is a generated non-steam id that belongs to someone else.
// Parsed by hand: sscanf from a new glibc would make the module require GLIBC_2.38 on the server
static uint64_t_ AuthIdToSteamId(const char *auth)
{
	if (strncmp(auth, "STEAM_0:", 8) != 0
		|| (auth[8] != '0' && auth[8] != '1') || auth[9] != ':' || auth[10] < '0' || auth[10] > '9')
		return 0;

	uint64_t_ y = 0;

	for (const char *p = auth + 10; *p; p++)
	{
		if (*p < '0' || *p > '9' || y > 0x7FFFFFFFULL)
			return 0;

		y = y * 10 + (*p - '0');
	}

	// The last digit can still push Y past 31 bits
	if (y > 0x7FFFFFFFULL)
		return 0;

	return 0x0110000100000000ULL + y * 2 + (auth[8] - '0');
}

// The forward carries the player's slot (1-32), so plugins can act on them right away. Steam's approval
// and AMXX's authorization of the same player come in either order: whichever comes second fires it.
#define MAX_CLIENTS			32
#define PENDING_SIZE		64
#define PENDING_TIMEOUT		120	// seconds an approval waits for its player to be authorized

// SteamID64 of the player AMXX authorized in each slot (0: none, bot, LAN or non-Steam)
static uint64_t_ g_PlayerSteamId[MAX_CLIENTS + 1];

// Approvals whose player isn't authorized in a slot yet
struct PendingApproval
{
	uint64_t_ player;
	uint64_t_ owner;
	time_t at;
};

static PendingApproval g_Pending[PENDING_SIZE];

// forward client_steam_owner(id, const authid[], const owner_authid[], const owner_steamid64[]);
static void FireOwnerForward(int id, uint64_t_ player, uint64_t_ owner)
{
	if (g_fwdOwner == -1)
		return;

	char szAuth[32], szOwnerAuth[32] = "", szOwner64[24] = "";
	SteamIdToAuthId(player, szAuth, sizeof(szAuth));

	// Garbage or 0 would mean Steam sent the old, shorter callback without the owner field
	if (IsIndividualSteamId(owner))
	{
		SteamIdToAuthId(owner, szOwnerAuth, sizeof(szOwnerAuth));
		snprintf(szOwner64, sizeof(szOwner64), "%llu", owner);
	}

	MF_ExecuteForward(g_fwdOwner, (cell)id, szAuth, szOwnerAuth, szOwner64);
}

// Slot of a connecting or connected player with this SteamID, 0 if none
static int FindPlayer(uint64_t_ steamId)
{
	for (int id = 1; id <= MAX_CLIENTS; id++)
	{
		if (g_PlayerSteamId[id] == steamId && (MF_IsPlayerIngame(id) || MF_IsPlayerConnecting(id)))
			return id;
	}

	return 0;
}

static void AddPending(uint64_t_ player, uint64_t_ owner)
{
	time_t now = time(NULL);
	int slot = -1;

	// The player's own entry, else a free or expired one, else the oldest
	for (int i = 0; i < PENDING_SIZE; i++)
	{
		if (g_Pending[i].player == player)
		{
			slot = i;
			break;
		}

		if (slot == -1 && (!g_Pending[i].player || now - g_Pending[i].at > PENDING_TIMEOUT))
			slot = i;
	}

	if (slot == -1)
	{
		slot = 0;

		for (int i = 1; i < PENDING_SIZE; i++)
		{
			if (g_Pending[i].at < g_Pending[slot].at)
				slot = i;
		}
	}

	g_Pending[slot].player = player;
	g_Pending[slot].owner = owner;
	g_Pending[slot].at = now;
}

// AMXX authorized a player (client_authorized): remember their SteamID and fire a waiting approval
static void OnClientAuthorized(int id, const char *authstring)
{
	if (id < 1 || id > MAX_CLIENTS)
		return;

	uint64_t_ steamId = authstring ? AuthIdToSteamId(authstring) : 0;
	g_PlayerSteamId[id] = steamId;

	if (!steamId)
		return;

	time_t now = time(NULL);

	for (int i = 0; i < PENDING_SIZE; i++)
	{
		if (g_Pending[i].player != steamId)
			continue;

		g_Pending[i].player = 0;

		if (now - g_Pending[i].at <= PENDING_TIMEOUT)
		{
#ifdef STEAMOWNER_DEBUG
			MF_Log("Authorized %s in slot %d, firing its approval", authstring, id);
#endif
			FireOwnerForward(id, steamId, g_Pending[i].owner);
		}
		break;
	}
}

static void OnClientApprove(const GSClientApprove_t *pApprove)
{
	if (!pApprove || !IsIndividualSteamId(pApprove->m_SteamID))
		return;

	// Garbage or 0 here would mean Steam sent the old, shorter callback without the owner field
	uint64_t_ owner = pApprove->m_OwnerSteamID;
	bool bOwnerKnown = IsIndividualSteamId(owner);

	// Reuse the player's slot on reconnect, so one player never holds more than one entry
	int slot = -1;

	for (int i = 0; i < OWNER_CACHE_SIZE; i++)
	{
		if (g_Cache[i].player == pApprove->m_SteamID)
		{
			slot = i;
			break;
		}
	}

	if (bOwnerKnown)
	{
		if (slot == -1)
		{
			slot = g_iCacheNext;
			g_iCacheNext = (g_iCacheNext + 1) % OWNER_CACHE_SIZE;
		}

		g_Cache[slot].player = pApprove->m_SteamID;
		g_Cache[slot].owner = owner;
	}
	else if (slot != -1)
	{
		// Owner unknown this time: drop the old entry instead of answering with a stale owner
		g_Cache[slot].player = 0;
		g_Cache[slot].owner = 0;
	}

	// Already authorized: fire now with their slot, otherwise when AMXX authorizes them
	int id = FindPlayer(pApprove->m_SteamID);

#ifdef STEAMOWNER_DEBUG
	char szAuth[32], szOwnerAuth[32] = "unknown";
	SteamIdToAuthId(pApprove->m_SteamID, szAuth, sizeof(szAuth));
	if (bOwnerKnown)
		SteamIdToAuthId(owner, szOwnerAuth, sizeof(szOwnerAuth));
	MF_Log("Approved %s (%llu), owner field raw %llu -> %s, %s", szAuth, pApprove->m_SteamID, owner, szOwnerAuth,
		id ? "player already authorized, firing" : "waiting for authorization");
#endif

	if (id)
		FireOwnerForward(id, pApprove->m_SteamID, owner);
	else
		AddPending(pApprove->m_SteamID, owner);
}

class CClientApproveCallback : public CCallbackBase
{
public:
	CClientApproveCallback()
	{
		// Same as STEAM_GAMESERVER_CALLBACK: listen on the game server pipe, not the client one
		m_nCallbackFlags |= k_ECallbackFlagsGameServer;
		m_iCallback = GSCLIENTAPPROVE_CALLBACK;
	}

	virtual void Run(void *pvParam) { OnClientApprove((const GSClientApprove_t *)pvParam); }
	virtual void Run(void *pvParam, bool, uint64_t_) { OnClientApprove((const GSClientApprove_t *)pvParam); }
	virtual int GetCallbackSizeBytes() { return sizeof(GSClientApprove_t); }
};

static CClientApproveCallback g_ApproveCallback;

// native steam_get_owner(const authid[], owner_authid[], len);
// Returns 1 and the owner's STEAM_0:X:Y if the player was approved recently, 0 if unknown
static cell AMX_NATIVE_CALL steam_get_owner(AMX *amx, cell *params)
{
	int len;
	const char *auth = MF_GetAmxString(amx, params[1], 0, &len);
	uint64_t_ player = AuthIdToSteamId(auth);

	if (!player)
		return 0;

	for (int i = 0; i < OWNER_CACHE_SIZE; i++)
	{
		if (g_Cache[i].player == player)
		{
			char szOwnerAuth[32];
			SteamIdToAuthId(g_Cache[i].owner, szOwnerAuth, sizeof(szOwnerAuth));
			MF_SetAmxString(amx, params[2], szOwnerAuth, params[3]);
			return 1;
		}
	}

	return 0;
}

static AMX_NATIVE_INFO g_Natives[] =
{
	{ "steam_get_owner", steam_get_owner },
	{ NULL, NULL }
};

void OnAmxxAttach()
{
	MF_AddNatives(g_Natives);

	SteamAPI_RegisterCallback(&g_ApproveCallback, GSCLIENTAPPROVE_CALLBACK);
	g_bRegistered = true;

	MF_RegAuthFunc(OnClientAuthorized);
}

void OnAmxxDetach()
{
	MF_UnregAuthFunc(OnClientAuthorized);

	if (g_bRegistered)
	{
		SteamAPI_UnregisterCallback(&g_ApproveCallback);
		g_bRegistered = false;
	}
}

void OnPluginsLoaded()
{
	// forward client_steam_owner(id, const authid[], const owner_authid[], const owner_steamid64[]);
	g_fwdOwner = MF_RegisterForward("client_steam_owner", ET_IGNORE, FP_CELL, FP_STRING, FP_STRING, FP_STRING, FP_DONE);
}

void OnPluginsUnloaded()
{
	// Forward ids are invalid until plugins are loaded again on the next map
	g_fwdOwner = -1;
}
