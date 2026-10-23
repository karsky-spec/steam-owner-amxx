# Steam Owner

An AMX Mod X module that exposes the **Steam game-license owner** of a connecting player to plugins —
the piece of information that reveals **Steam Family Sharing**, which the CS 1.6 engine receives but
throws away.

Works for **every Steam player, regardless of profile privacy**, and needs **no Steam Web API key**.

## What it does

When a player connects, the engine (ReHLDS) asks Steam to validate their auth ticket. Steam's answer
(the `GSClientApprove_t` callback) carries both the player's SteamID **and the SteamID of the
game-license owner** (`m_OwnerSteamID`):

- own copy → owner == player
- Family Sharing → owner is the account that actually bought the game

The CS 1.6 engine ignores that owner field (its code predates Family Sharing). This module registers
its own listener for the same callback, reads the owner, and passes it to plugins.

It makes **no network requests** — it only reads the reply Steam already sends during the normal
connection handshake. That's why it needs no API key and doesn't depend on the player's profile
being public.

## API

```pawn
#include <steam_owner>

/**
 * Called when Steam approves a connecting player.
 *   owner_authid == authid  -> own copy
 *   owner_authid != authid  -> Family Sharing, owner_authid is the license owner
 *   owner_authid == ""       -> Steam did not report the owner
 */
forward client_steam_owner(const authid[], const owner_authid[], const owner_steamid64[]);

/**
 * Look up the owner of a recently approved player.
 * @return 1 if known, 0 otherwise
 */
native steam_get_owner(const authid[], owner_authid[], len);
```

## Example

```pawn
#include <amxmodx>
#include <steam_owner>

public plugin_init()
    register_plugin("Owner Example", "1.0", "you");

public client_steam_owner(const szAuthId[], const szOwnerAuthId[], const szOwner64[])
{
    // own copy or no data: nothing to do
    if(!szOwnerAuthId[0] || equal(szOwnerAuthId, szAuthId))
        return;

    // the player runs a copy shared by szOwnerAuthId -> plug your own logic here
    log_amx("%s plays a copy shared by %s (%s)", szAuthId, szOwnerAuthId, szOwner64);
}
```

A ready-to-compile example is in [`examples/owner_example.sma`](examples/owner_example.sma).

The module is ban-system agnostic: use the forward (or `steam_get_owner`) to build whatever you
need — ban-evasion checks, logging, alerts.

## Requirements

- **Linux** server (the module ships as `.so`).
- **glibc >= 2.1.3** (practically any Linux; libstdc++/libgcc are linked statically).
- **ReHLDS** (the module reads the `GSClientApprove_t` callback from its `steam_api` library).
- **Reunion** recommended, so you can skip non-steam clients (`is_user_steam`).

## Installation

1. Copy `steam_owner_amxx_i386.so` to `addons/amxmodx/modules/`.
2. Add a line `steam_owner` to `addons/amxmodx/configs/modules.ini`.
3. Copy `steam_owner.inc` to `addons/amxmodx/scripting/include/` (needed to compile plugins).
4. **Restart the server** (modules load only at startup).

Check `amxx modules` — **Steam Owner** should be `running`.

## Building

Needs a 32-bit C++ toolchain: `sudo apt install g++-multilib`.

```bash
./build.sh          # release
./build.sh debug    # also logs every Steam approval to the AMXX log
```

`sdk/` holds the AMXX module SDK headers and the ReHLDS `steam_api` library the build links against.

## Notes & limitations

- **Steam players only.** Non-steam clients (Reunion) have no Steam license to report.
- **The owner is always a real Steam account** (`STEAM_0:`). Reunion's generated `STEAM_1:` ids are
  not Steam accounts.
- **Linux only.** A Windows server needs a `.dll` build.
- The forward fires a moment **after** `client_authorized`, once Steam's reply arrives — not in
  `client_connect`.
- Relies on Steam populating `m_OwnerSteamID` in the approval callback — confirmed working on ReHLDS
  with current Steam.

## Author

Soul.
