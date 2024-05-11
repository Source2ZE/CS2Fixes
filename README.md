# RampbugFix
Minimizes rampbugs. This plugin isn't perfect and rampbugs will continue to occur until Valve decides to finally fix them.

Fix taken from [CS2KZ's rampbug fix](https://gist.github.com/zer0k-z/2eb0c230c8f2c62b5c46d36353cf8d8d)

## Installation

- Install [Metamod](https://cs2.poggu.me/metamod/installation/)
- Build the plugin using the [compilation instructions below](https://github.com/Interesting-exe/CS2Fixes-RampbugFix/tree/main?tab=readme-ov-file#instructions)
- Extract the package contents into `game/csgo` on your server

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
