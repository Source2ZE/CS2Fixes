#!/bin/bash
mkdir dockerbuild
cd dockerbuild
pwd
python ../configure.py --enable-optimize --symbol-files --sdks cs2
ambuild