# CS2Fixes

CS2Fixes is a Metamod plugin with fixes and features aimed but not limited to zombie escape. This project also serves as a good example and help for source2mod and other developers.

## Installation

- Install [Metamod](https://www.sourcemm.net/downloads.php?branch=dev)
- Download the [latest release package](https://github.com/Source2ZE/CS2Fixes/releases/latest) for your OS
- Extract the package contents into `game/csgo` on your server
- Configure the plugin cvars as desired in `cfg/cs2fixes/cs2fixes.cfg`, many features are disabled by default
- OPTIONAL: If you want to setup admins, rename `admins.cfg.example` to `admins.cfg` which can be found in `addons/cs2fixes/configs` and follow the instructions within to add admins

## Features
- Windows and Linux support
- Schema system
  - Property access
  - Property updating
- Event hooking
- Coloring console chat messages
- Movement unlocker
- VScript unlocker
- Preventing console logs
- Buying weapons through chat
  - Limit weapon buy count
- Unlocking commands and convars
- Dumping all convar values
- Memory patching system
- Detour system
- Player Manager
  - Storing players
  - Player authorization
  - Userid Target lookup
  - Admin system
    - Admin commands
    - Admin chat
    - Infractions
      - Kick
      - Ban
      - Mute
      - Gag
      - Removing infractions
- Timers
- Blocking weapon sounds/decals
  - Hooking fire bullets tempent
  - Stopsound
  - Silenced sounds
  - Toggledecals
- Legacy event listener
- trigger_push fix
- Remove player collisions
- Ztele
- Water fix
- Hide command
  - Transmit hook
- Rcon
- HTTP REST API
- EntFire
- Fix blast damage crashes
- Block self molotov damage
- Anti chat flood
- Rock the vote
- Vote extends
- Top defenders

## Why is this all one plugin? Why "CS2Fixes"?

Reimplementing all these features as standalone plugins would duplicate quite a lot of code between each. Metamod is not much more than a loader & hook manager, so many common modding features need a fair bit of boilerplate to work with. And since our primary goal is developing CS2Fixes for all zombie escape servers, there is not necessarily a drawback to distributing our work in this form at the moment.

The CS2Fixes name comes from the CSSFixes and CSGOFixes projects, which were primarily aimed at low-level bug fixes and improvements for their respective games. Long term, we see this plugin slimming down and becoming more similar to them. Since as the CS2 modding scene matures, common things like an admin system and RTV become more feasible in source2mod or a similar modding platform.

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
python3 ../configure.py --enable-optimize --symbol-files --sdks cs2
ambuild
```

Copy the contents of package/ to your server's csgo/ directory.
