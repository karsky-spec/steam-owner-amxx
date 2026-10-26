/*
	Example plugin for the Steam Owner module.

	Logs, for every connecting Steam player, whether they run their own copy of the game
	or a copy shared through Steam Family Sharing (and by whom).

	This is a minimal demo - plug your own logic (ban-evasion checks, alerts, ...) where marked.
*/

#include <amxmodx>
#include <steam_owner>

public plugin_init()
{
	register_plugin("Steam Owner Example", "1.0.0", "Soul");
}

public client_steam_owner(id, const szAuthId[], const szOwnerAuthId[], const szOwnerSteamId64[])
{
	// Steam did not report an owner - nothing to do
	if(!szOwnerAuthId[0])
	{
		log_amx("%n (%s): owner unknown", id, szAuthId);
		return;
	}

	// Own copy: the license owner is the player themselves
	if(equal(szOwnerAuthId, szAuthId))
	{
		log_amx("%n (%s) plays their own copy", id, szAuthId);
		return;
	}

	// Family Sharing: the game belongs to another account
	log_amx("%n (%s) plays a copy shared by %s (%s)", id, szAuthId, szOwnerAuthId, szOwnerSteamId64);

	// --- your logic here, e.g. if szOwnerAuthId is banned:
	// server_cmd("kick #%d ^"Konto wlasciciela gry jest zbanowane^"", get_user_userid(id))
}
