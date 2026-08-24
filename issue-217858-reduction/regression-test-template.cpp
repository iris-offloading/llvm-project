// Single-file (split-file) form of the reduction, ready to drop into
// clang/test/Modules/ once the crash is fixed.

// RUN: rm -rf %t
// RUN: split-file %s %t
//
// RUN: %clang_cc1 -std=c++23 -triple x86_64-pc-windows-msvc \
// RUN:   -emit-module-interface %t/lib.cppm -o %t/Lib.pcm
// RUN: %clang_cc1 -std=c++23 -triple x86_64-pc-windows-msvc \
// RUN:   -fmodule-file=Lib=%t/Lib.pcm -emit-module-interface %t/mod.cppm \
// RUN:   -o %t/Mod.pcm
// RUN: %clang_cc1 -std=c++23 -triple x86_64-pc-windows-msvc \
// RUN:   -fmodule-file=Lib=%t/Lib.pcm -fmodule-file=Mod=%t/Mod.pcm \
// RUN:   -emit-llvm -o - %t/main.cpp | FileCheck %t/main.cpp

//--- lib.cppm
export module Lib;

export template <class Callback> auto make_closure(Callback &cb) {
  return [&cb](auto &arg) noexcept(noexcept(cb(arg))) { cb(arg); };
}

export template <class Callback, class Arg>
void for_each(Callback &&cb, Arg &arg) noexcept(noexcept(make_closure(cb)(arg))) {}

export template <class It> struct iterator {
  It cur;
  void operator++() noexcept(noexcept(for_each([](auto &i) { ++i; }, cur))) {}
};

//--- mod.cppm
export module Mod;
import Lib;

export inline int test() {
  int a[4]{};
  iterator<int *> it{a};
  ++it;
  return static_cast<int>(it.cur - a);
}

//--- main.cpp
import Mod;
int main() { return test(); }

// CHECK: define {{.*}}main
