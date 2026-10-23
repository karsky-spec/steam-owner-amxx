#if defined _steam_owner_included
	#endinput
#endif
#define _steam_owner_included

#pragma reqlib steam_owner
#if !defined AMXMODX_NOAUTOLOAD
	#pragma loadlib steam_owner
#endif

/**
 * Called when Steam approves a connecting player. Steam reports the owner of the game license:
 * the player's own SteamID for their own copy, someone else's with Steam Family Sharing.
 *
 * @note Arrives a moment after the player starts connecting, usually after client_authorized().
 *
 * @param authid            Player's STEAM_0:X:Y
 * @param owner_authid      License owner's STEAM_0:X:Y, empty if Steam didn't report it
 * @param owner_steamid64   License owner's SteamID64, empty if Steam didn't report it
 */
forward client_steam_owner(const authid[], const owner_authid[], const owner_steamid64[]);

/**
 * Gets the license owner of a recently approved player.
 *
 * @param authid            Player's STEAM_0:X:Y (STEAM_1:X:Y is a Reunion non-steam id, never found)
 * @param owner_authid      Buffer for the owner's STEAM_0:X:Y
 * @param len               Buffer size
 *
 * @return                  1 if known, 0 otherwise
 */
native steam_get_owner(const authid[], owner_authid[], len);
