#include "ct_lua54.hpp"

constexpr double r_smoke = ct_lua54::run_number<fixed_string{R"lua(return 1)lua"}, ct_lua54::LIB_BASE>();
static_assert(r_smoke == 1.0, "library smoke test failed");

constexpr auto print_smoke = ct_lua54::run_capture<fixed_string{R"lua(
print("smoke", 123)
return 1
)lua"}, ct_lua54::LIB_BASE>();

static_assert(print_smoke.value.tag == ct_lua54::Tag::Int && print_smoke.value.i == 1, "print smoke failed");

int main() {
  ct_lua54::print_buffer(print_smoke);
  return 0;
}

