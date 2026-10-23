#!/bin/bash
# Builds steam_owner_amxx_i386.so (32-bit, like the server). Needs: sudo apt install g++-multilib
# Usage: ./build.sh [debug]   - "debug" logs every Steam approval to the AMXX log
#
# The 32-bit toolchain can't read files from /mnt/* under WSL, so the build runs in a temp directory.

SRC_DIR="$(cd "$(dirname "$0")" && pwd)"
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT

cp "$SRC_DIR"/src/steam_owner.cpp "$SRC_DIR"/src/moduleconfig.h "$TMP"/ || exit 1
cp -r "$SRC_DIR"/sdk "$TMP"/sdk || exit 1
cd "$TMP" || exit 1

DEFS=""
[ "$1" = "debug" ] && DEFS="-DSTEAMOWNER_DEBUG"

# libsteam_api.so is the one shipped with ReHLDS; at runtime the engine's already loaded copy is used.
# libstdc++/libgcc are linked statically, so the server doesn't need matching versions of them.
g++ -m32 -O2 -fPIC -shared -fno-exceptions -fno-rtti -Wall -Wno-unused-function \
	-I. -Isdk $DEFS \
	steam_owner.cpp sdk/amxxmodule.cpp \
	-Lsdk -lsteam_api -static-libstdc++ -static-libgcc \
	-Wl,--no-undefined -o steam_owner_amxx_i386.so || exit 1

cp steam_owner_amxx_i386.so "$SRC_DIR"/ && echo "OK -> $SRC_DIR/steam_owner_amxx_i386.so"
