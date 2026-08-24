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
