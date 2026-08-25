#!/bin/bash
# Reduced reproducer for the X86 vector-element-index miscompile.
#   ./repro.sh [clang] [llc]
set -x
CC=${1:-clang}
LLC=${2:-llc}

# 1. Source level: runs cleanly at -O0 and with plain SSE2, dies with SSE4.1+.
$CC -O0 repro.c -o repro-O0 && ./repro-O0; echo "  -> $?"
$CC -O2 repro.c -o repro-sse2 && ./repro-sse2; echo "  -> $?"
$CC -O2 -msse4.1 repro.c -o repro-sse41 && ./repro-sse41; echo "  -> $?"

# 2. Backend only: look at the index register of the stack store.
#    Correct output masks it ("andl $3, %eax"); the bug uses the raw 64-bit
#    value produced by "movq %xmm0, %rax".
$LLC -mtriple=x86_64-- -mattr=+sse2 repro.ll -o -
$LLC -mtriple=x86_64-- -mattr=+sse4.1 repro.ll -o -
