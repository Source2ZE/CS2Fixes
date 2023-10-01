#!/bin/bash

mkdir temp
mkdir generated

for proto in *.proto; do
    sed '1i\syntax = "proto2";' "$proto" > "temp/$proto"
done

cd temp
../protoc/protoc --cpp_out=../generated *.proto

rm -rf temp