## DISCLAIMER: This plugin is *not* meant to be used on a server yet, it is very much a work in progress and thus we are *not* responsible for any issues or breakages caused by the plugin. In addition, we will *not* provide support for building or usage unless you plan on contributing to the project yourself.

# CS2Fixes

CS2Fixes is a collection of experimental fixes and features aimed but not limited to zombie escape. This project is not production ready and serves as a proof of concept and help for source2mod and other developers.

## Features
- [x] Metamod implementation
- [x] Windows and Linux support
- [x] Schema system
  - [x] Property access
  - [x] Property updating
- [x] Event hooking
- [x] Coloring console chat messages
- [x] Movement unlocker
- [x] VScript unlocker
- [x] Preventing console logs
- [x] Buying weapons through chat
  - [x] Limit weapon buy count
- [x] Unlocking commands and convars
- [x] Memory patching system
- [x] Detour system
- [x] Player Manager
  - [x] Storing players
  - [x] Player authorization
  - [x] Userid Target lookup
  - [x] Admin system
    - [x] Admin commands
    - [ ] Infractions
      - [x] Kick
      - [x] Ban
      - [x] Mute
      - [x] Gag
      - [x] Removing infractions
- [x] Timers
- [x] Blocking weapon sounds/decals
  - [x] Hooking fire bullets tempent
  - [x] Implement stopsound
  - [x] Implement toggledecals
- [x] Legacy event listener
- [x] Weapon pickup crash fix
- [x] trigger_push fix
- [x] Remove player collisions
- [x] Ztele
- [x] Water fix
- [ ] Hide command
  - [x] Transmit hook
  - [] Rewrite detour hook into sourcehook (Interface definition available)

## Compilation

### Requirements

- [Metamod:Source](https://www.sourcemm.net/downloads.php/?branch=master) (build 1219 or higher)
- [AMBuild](https://wiki.alliedmods.net/Ambuild)

### Instructions

Follow the instructions below to compile CS2Fixes.

```bash
git clone https://github.com/Source2ZE/CS2Fixes/ && cd CS2Fixes
git submodule update --init --recursive

export MMSOURCE112=/path/to/metamod/
export HL2SDKCS2=/path/to/sdk/submodule

mkdir build && cd build
CC=gcc CXX=g++ python3 ../configure.py -s cs2
ambuild
```

Copy the contents of package/ to your server's csgo/ directory.

## Authors
- [@xen-000](https://github.com/xen-000)
- [@poggicek](https://github.com/poggicek)
