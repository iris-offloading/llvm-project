#!/bin/bash
# Reduced reproducer for llvm/llvm-project#217858.
#
#   ./repro.sh [clang++] [target-triple]
#
# Compiling main.cpp crashes clang.  Nothing from the standard library is
# needed, and the crash is not specific to the Microsoft ABI: it reproduces
# with x86_64-pc-linux-gnu as well.
set -x
CXX=${1:-clang++}
TGT=${2:-x86_64-pc-windows-msvc}
F="--target=$TGT -std=c++23"
rm -f Lib.pcm Mod.pcm main.o
$CXX $F -x c++-module lib.cppm --precompile -o Lib.pcm
$CXX $F -fmodule-file=Lib=Lib.pcm -x c++-module mod.cppm --precompile -o Mod.pcm
$CXX $F -fmodule-file=Lib=Lib.pcm -fmodule-file=Mod=Mod.pcm -c main.cpp -o main.o
