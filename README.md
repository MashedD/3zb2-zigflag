# ZigFlag - Custom 3rd Zigock Bot II for Quake II

**Pull Requests are welcomed!**

# Fixes in this fork

- fixed selecting next item in menu (`]` by default)
- fixed player spawn point (now it is random on map start)

# Changes

- new: added `store` command to save player position, similar as in Jump mod
- new: added `recall` command to restore player position. If player dies then moves to spawn point
- added showing remaining time for all game modes
- add basic "low effort version" of TeamDeathmatch option (`set tdm 1; set ctf 1`)
- replaced configs

# Notes

- loading `exec addbot.cfg` is useful for spawning bots with key letters or commands - like `spawn1`, or `despawn1`
    - use Numpad Plus key to increase number of bots and Numpad Enter spawn them or use command like `spawn1`
- spawning sound if different than usual
- to play CTF: `exec config-ctf`
- when `set zigrapple 1` is set, use `use grapple` to change weapon to grappling hook
- use `kill` command to die. There's a cool off period. Useful for learning spawn points
- use `drop tech` to drop tech
- use `drop flag` to drop flag
- some models are missing (like grappling hook). You need to copy pak from `ctf` mod

# Building

## Prerequisites

Dependencies might be missing and some are probably excessive.
I didn't optimize this as it's time/cost not effective for me.
Best might be to use Docker for the job.

```bash
# Tested on CachyOS
sudo pacman -S cmake gcc

# For cross compilation
sudo pacman -S \
    mingw-w64-tools \
    mingw-w64-binutils \
    mingw-w64-crt \
    mingw-w64-gcc \
    mingw-w64-headers \
    mingw-w64-winpthreads
paru -S \
    mingw-w64-zlib \
    mingw-w64-zlib-ng \
    mingw-w64-ffmpeg \
    mingw-w64-pkg-config \
    mingw-w64-libpng \
    mingw-w64-libjpeg-turbo \
    mingw-w64-openal \
    mingw-w64-zstd
```

## Compilation

Review scripts before executing them.

```bash
./build-lin64.sh
./clean.sh
./build-win32.sh
./clean.sh
./build-win64.sh
```

# TODO

- fix compilation warnings
- README.md: update
- TDM: teammates shouldn't retaliate on (rocket or any) damage

# Old part of README.md below

This is a custom port of the 3rd Zigock Bot II to Quake II - Yamagi Quake II is recommended.  \
All warnings (up to GCC9) and unused variables have been addressed in the original source. \
The code also has handpicked backport fixes, enhancements and features applied from various \
sources: `tastyspleen`, `yquake2`, `OpenTDM`, `OpenFFA` and many custom.

Also works with Vulkan Quake II (vkQuake2): https://github.com/kondrak/vkQuake2/issues/103

This was modified for my own use and driven by nostalgia for the Quake II servers of the 90's. \
There are many heavily modified versions of the Quake II engine, this mod tries to keep the look and feel of \
the original game deathmatch, but allows a multiplayer experience with some of the best bots for the Quake II\
engine. I couldn't locate any `Capture and Hold` servers on [q2servers](http://q2servers.com), so this offered the opportunity to rekindle \
a firm favourite via the `zigmode` modification.

Tip of the hat to `Ponpoko`, original mod author and bot creator.

Bot chaining routes are supplied, further routes can be (re)created via the mod command `chedit` (See `CONFIG.txt`)

### ZigMode ZigFlag (Capture and Hold) - https://zigflag.net

The premise is simple: **Get the flag and keep it** - *plays on standard Deathmatch maps*.

The original `zigmode` was released belated, buggy and only half implemented, I attempted to make this feature a little \
more refined, just for fun. I was trying to keep the look and feel of the original deathmatch, but with a few bells and \
whistles. However `zigflag` turned into a fairly customised game.

* Simple HUD enhancements.
* Automatic bot control.
* Autospawn bots at level start.
* Visual/Audio notifications to Flagholder.
* Customised dogtags displayed on scoreboard.
* Optional Flag respawn feature.
* Optional Flagholder frag bonus.
* Optional Flag sucks health from subdued holder.
* Optional auto weapon switching on upgrade.
* Optional identified generic gameplay fixes.
* Optional respawn protection.
* Optional spawn bots at distance.
* Optional grapple.
* Optional HUD playerid.
* Optional enhanced HUD.

... and many bugfixes was the final outcome of playing around with the code.

A ZigFlag server can sometimes be found running at `quake2://quake.zigflag.net:27910` 

The mod also supports skin and model teams with appropriate bonuses and penalties on Flag possession and `FRIENDLY_FIRE`.

The changes subtly alter the game dynamics and improve on the original zigmode game element, IMHO. \
The original gameplay, with bugfixes, can still be enabled by disabling the new elements via cvars.

`Capture and Hold` plays best on smaller level maps with a timelimit, no fraglimit and a couple of bots.

### Pak file (Flag model) and Route Chaining files

`ZigMode` requires the included small `.pak` file, for the flag model, and route chaining files for the maps. \
Many popular maps are included, further route chaining `.chn` files can be created via the mod `chedit` function.

#### Example config file for ZigFlag:

```
exec addbot.cfg
set zigmode 1
set zigspawn 1
set zigkiller 1
set zigrapple 0
set zigintro 0
set ctf 0
set aimfix 1
set combathud 1
set spawnbotfar 1
set killerflag 1
set fixflaws 1
set playerid 1
set weaponswap 1
set botlist default
set autospawn 3
set autobot 0
set vwep 1
set maxclients 16
set respawn_protection 1
set dmflags 16384
set fraglimit 30
set timelimit 10
set maplist q2dm
map q2dm1
```

See <CONFIG.md> for further details.

## Bot commands (See full details in CONFIG.md)

By default use: `KP_PLUS`, `KP_MINUS` & `KP_ENTER` for bot control

`Console`

Spawn `$` bots via:

    sv spb $

Remove `$` bots via:

    sv rmb $

## Other features (See full details in CONFIG.txt)

Improved aim, enable `1` (default) or disable `0` via:

    aimfix 1

Fix noted Quake 2 gameplay flaws (opentdm), enable `1` (default) or disable `0` via:

    fixflaws 1

Flag takes health from a subdued holder, anti-camping, enable `1` (default) or disable `0`:

    killerflag 1

**Stay-in-the-fray** to avoid penalty.

Identify player in the crosshair, enable `1` or disable `0` (default)

    playerid 1

Display extra (rank, timer) information in HUD:

    combathud 1

Auto switch to upgraded weapon on pickup, enable `1` or disable `0` (default):

    weaponswap 1

Option to add grapple to the fray - CTF `pak0.pak` required.

    zigrapple 1

Add spectator mode to game start (server mode)

    zigintro 1

`Capture and Hold (ZigFlag)` mode for Deathmatch/Team games:

    zigmode 1
    zigspawn 1
    zigkiller 1

Broadcast the summary of the top six players, in console, at level intermission:

    ----------------
    | q2dm1 | ~ 02 |
    ------------------------------------------------------
    | X | Player           |  S  |  P  |  T  |  F  |  A  |
    ------------------------------------------------------
    | * | [BOT]Batty       | 30  | 0   | 8   | +8  | +0  |
    |   | [BOT]Chews       | 26  | 0   | 8   | +0  | +2  |
    | F | [BOT]Lupin       | 21  | 0   | 8   | +3  | +2  |
    |   | _GONZO_          | 6   | 19  | 8   | +6  | +0  |
    ------------------------------------------------------

**S** - *Score* \
**P** - *Ping* \
**T** - *Time* \
**F** - *Flag Possession bonuses* \
**A** - *Assassinations of Flagholder* \
**~** - *Flag bounce occurrences*

## Errata

The mod has a random issues using `gamemap`, resulting in a lockup or segfault. Bot tracing in \
`SV_RunThink()` seems to be the related area, but I have yet to track down. Use the `map` command which \
causes a full level reset to overcome the issue. Note: **Q2Pro** attempts to enforce the use of \
`gamemap`, use `sv_allow_map 1` in Q2Pro to overcome this:

    map q2dm1


## 獄

![captureandhold](screenshot/screenshot.png)
![captureandhold](screenshot/screenshot2.png)

