#include "ct_lua54.hpp"

constexpr auto core_runtime = ct_lua54::interpreter();
constexpr double r_core = core_runtime.run_number<fixed_string{R"lua(return 1)lua"}>();
static_assert(r_core == 1.0, "core example failed");

int main() { return 0; }

