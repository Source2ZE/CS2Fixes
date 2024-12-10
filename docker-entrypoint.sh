#!/bin/bash
mkdir dockerbuild
cd dockerbuild
pwd
python ../configure.py --enable-optimize --sdks cs2
ambuild