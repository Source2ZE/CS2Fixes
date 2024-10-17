#!/bin/bash

mkdir -p temp/google/protobuf
mkdir generated

for proto in *.proto; do
    sed '1i\syntax = "proto2";' "$proto" > "temp/$proto"
done

cd temp
cp ../../sdk/devtools/bin/linux/protoc .
cp ../include/descriptor.proto google/protobuf

./protoc --cpp_out=../generated *.proto

cd ..
rm -rf temp