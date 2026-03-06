#include "ct_lua54.hpp"

constexpr double r_core = ct_lua54::run_number<fixed_string{R"lua(return 1)lua"}, 0u>();
static_assert(r_core == 1.0, "core example failed");

int main() { return 0; }


