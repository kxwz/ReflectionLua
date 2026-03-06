#include "ct_lua54.hpp"

constexpr double r_smoke = ct_lua54::run_number<fixed_string{R"lua(return 1)lua"}, ct_lua54::LIB_BASE>();
static_assert(r_smoke == 1.0, "library smoke test failed");

int main() { return 0; }

