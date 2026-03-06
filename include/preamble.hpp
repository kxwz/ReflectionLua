#pragma once


// ct_lua54_single.cpp
// C++26 reflection (<meta>/<experimental/meta>) + consteval Lua core.
// No -fexpansion-statements required.

#include <array>
#include <cstddef>
#include <cstdint>
#include <concepts>
#include <initializer_list>
#include <iostream>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>

#if __has_include(<meta>)
  #include <meta>
  namespace meta = std::meta;
#elif __has_include(<experimental/meta>)
  #include <experimental/meta>
  namespace meta = std::experimental::meta;
#else
  #error "Need C++26 reflection header (<meta> or <experimental/meta>)."
#endif

// ---------- fixed_string (NTTP) ----------
template <std::size_t N>
struct fixed_string {
  char s[N]{};
  constexpr fixed_string(char const (&in)[N]) {
    for (std::size_t i = 0; i < N; ++i) s[i] = in[i];
  }
  constexpr std::string_view view() const { return {s, N - 1}; }
};

// ---------- API exposed to Lua (via reflection) ----------
namespace api {}

