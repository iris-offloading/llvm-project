export module Mod;
import Lib;

export inline int test() {
  int a[4]{};
  iterator<int *> it{a};
  ++it;
  return static_cast<int>(it.cur - a);
}
