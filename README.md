# RampbugFix
Minimizes rampbugs. This plugin isn't perfect and rampbugs will continue to occur until Valve decides to finally fix them

# CS2Fixes

CS2Fixes is a Metamod plugin with fixes and features aimed but not limited to zombie escape. This project also serves as a good example and help for source2mod and other developers.

## Installation

- Install [Metamod](https://cs2.poggu.me/metamod/installation/)
- Build the plugin using the compilation instructions below
- Extract the package contents into `game/csgo` on your server
- Configure the plugin cvars as desired in `cfg/cs2fixes/cs2fixes.cfg`, many features are disabled by default
- OPTIONAL: If you want to setup admins, rename `admins.cfg.example` to `admins.cfg` which can be found in `addons/cs2fixes/configs` and follow the instructions within to add admins

## Compilation

### Requirements

- [Metamod:Source](https://www.sourcemm.net/downloads.php/?branch=master) (build 1219 or higher)
- [AMBuild](https://wiki.alliedmods.net/Ambuild)

### Instructions

Follow the instructions below to compile CS2Fixes.

```bash

git clone https://github.com/Interesting-exe/CS2Fixes-RampbugFix && cd CS2Fixes-RampbugFix
git submodule update --init --recursive

export MMSOURCE112=/path/to/metamod/
export HL2SDKCS2=/path/to/sdk/submodule

mkdir build && cd build
python3 ../configure.py --enable-optimize --symbol-files --sdks cs2
ambuild
```

Copy the contents of package/ to your server's csgo/ directory.
