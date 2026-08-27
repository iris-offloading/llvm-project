// Reproducer: clang crashes when an importing TU emits the body of an inline
// function that was deserialized from a reduced BMI and calls
// __builtin_operator_new.
//
// A reduced BMI does not carry the implicitly-declared global allocation
// functions of the module unit's translation unit. When the importer emits
// the deserialized definition of 'g', the lookup of '::operator new' in
// CodeGenFunction::EmitBuiltinNewDeleteCall finds no candidate and runs into
// llvm_unreachable("predeclared global operator new/delete is missing")
// (clang/lib/CodeGen/CGExprCXX.cpp). Release builds die with SIGSEGV while
// "Generating code for declaration" of the deserialized function.
//
// With -stdlib=libc++ this is triggered by any allocating inline or template
// code that is instantiated in a module built with a reduced BMI (the default
// for -fmodule-output) and then emitted by an importing TU at any optimization
// level, e.g. through std::__libcpp_allocate whose body calls
// __builtin_operator_new:
//
//   clang++ -std=c++20 -stdlib=libc++ -x c++-module a.cppm \
//       -fmodule-output=a.pcm -c -o a.o
//   clang++ -std=c++20 -stdlib=libc++ -fmodule-file=a=a.pcm -c b.cpp -o b.o
//
// RUN: rm -rf %t
// RUN: split-file %s %t
//
// Crashes:
// RUN: %clang_cc1 -triple %itanium_abi_triple -std=c++20 %t/a.cppm \
// RUN:   -emit-reduced-module-interface -o %t/a.pcm
// RUN: %clang_cc1 -triple %itanium_abi_triple -std=c++20 -fmodule-file=a=%t/a.pcm \
// RUN:   %t/b.cpp -emit-llvm -o - | FileCheck %t/b.cpp
//
// Works with a full BMI:
// RUN: %clang_cc1 -triple %itanium_abi_triple -std=c++20 %t/a.cppm \
// RUN:   -emit-module-interface -o %t/a.full.pcm
// RUN: %clang_cc1 -triple %itanium_abi_triple -std=c++20 -fmodule-file=a=%t/a.full.pcm \
// RUN:   %t/b.cpp -emit-llvm -o - | FileCheck %t/b.cpp

//--- a.cppm
export module a;
export inline void *g() { return __builtin_operator_new(1); }

//--- b.cpp
import a;
void *f() { return g(); }

// CHECK: call {{.*}} @_Znw
