#pragma once

namespace ct_lua54::examples {

inline constexpr double r_all = ct_lua54::run_number<
  fixed_string{R"lua(return type(1) == "number" and 1 or 0)lua"},
  ct_lua54::LIB_ALL
>();
static_assert(r_all == 1.0, "all-libs example failed");

} // namespace ct_lua54::examples

