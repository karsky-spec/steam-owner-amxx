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
 * @note Fires once the player is both approved by Steam and authorized by AMXX (client_authorized()),
 *       so id is always valid: kick, ban or message the player right here. The player may still be
 *       connecting (is_user_connecting()), not yet in game.
 * @note Changed in 1.1.0: id was added as the first parameter. Recompile plugins with this include.
 *
 * @param id                Player index (1-32)
 * @param authid            Player's STEAM_0:X:Y
 * @param owner_authid      License owner's STEAM_0:X:Y, empty if Steam didn't report it
 * @param owner_steamid64   License owner's SteamID64, empty if Steam didn't report it
 */
forward client_steam_owner(id, const authid[], const owner_authid[], const owner_steamid64[]);

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
