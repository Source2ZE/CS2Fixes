#!/bin/bash
mkdir dockerbuild
cd dockerbuild
pwd
python3 ../configure.py -s cs2 --hl2sdk-manifests hl2sdk-manifests
ambuild