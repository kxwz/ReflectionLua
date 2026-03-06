#include "ct_lua54.hpp"

constexpr double r_all = ct_lua54::run_number<
  fixed_string{R"lua(
print("compile-time", 123)
return (type(1) == "number" and type(print) == "function" and print("ok") == nil) and 1 or 0
)lua"},
  ct_lua54::LIB_ALL
>();
static_assert(r_all == 1.0, "all-libs example failed");

int main() { return 0; }


