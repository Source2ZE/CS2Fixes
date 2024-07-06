# CS2Fixes

CS2Fixes is a Metamod plugin with fixes and features aimed but not limited to zombie escape. This project also serves as a good example and help for source2mod and other developers.

## Installation

- Install [Metamod](https://cs2.poggu.me/metamod/installation/)
- Download the [latest release package](https://github.com/Source2ZE/CS2Fixes/releases/latest) for your OS
- Extract the package contents into `game/csgo` on your server
- Configure the plugin cvars as desired in `cfg/cs2fixes/cs2fixes.cfg`, many features are disabled by default
- OPTIONAL: If you want to setup admins, rename `admins.cfg.example` to `admins.cfg` which can be found in `addons/cs2fixes/configs` and follow the instructions within to add admins

## Fixes and Features
You can find the documentation of the fixes and features [here](../../wiki/Home).

## Why is this all one plugin? Why "CS2Fixes"?

Reimplementing all these features as standalone plugins would duplicate quite a lot of code between each. Metamod is not much more than a loader & hook manager, so many common modding features need a fair bit of boilerplate to work with. And since our primary goal is developing CS2Fixes for all zombie escape servers, there is not necessarily a drawback to distributing our work in this form at the moment.

The CS2Fixes name comes from the CSSFixes and CSGOFixes projects, which were primarily aimed at low-level bug fixes and improvements for their respective games. Long term, we see this plugin slimming down and becoming more similar to them. Since as the CS2 modding scene matures, common things like an admin system and RTV become more feasible in source2mod or a similar modding platform.

## Compilation

```
git clone https://github.com/Source2ZE/CS2Fixes/ && cd CS2Fixes
git submodule update --init --recursive
```
### Docker (easiest)

Requires Docker to be installed. Produces Linux builds only.

```
docker compose up
```

Copy the contents of `dockerbuild/package/` to your server's `game/csgo/` directory.

### Manual

#### Requirements
- [Metamod:Source](https://github.com/alliedmodders/metamod-source)
- [AMBuild](https://wiki.alliedmods.net/Ambuild)

#### Linux
```bash
export MMSOURCE112=/path/to/metamod
export HL2SDKCS2=/path/to/sdk/submodule

mkdir build && cd build
python3 ../configure.py --enable-optimize --sdks cs2
ambuild
```

#### Windows

Make sure to run in "x64 Native Tools Command Prompt for VS"

```bash
set MMSOURCE112=\path\to\metamod
set HL2SDKCS2=\path\to\sdk\submodule

mkdir build && cd build
py ../configure.py --enable-optimize --sdks cs2
ambuild
```

Copy the contents of `build/package/` to your server's `game/csgo/` directory.
