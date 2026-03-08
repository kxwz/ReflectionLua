#include "ct_lua54.hpp"

namespace math_api {

constexpr double inc(double x) {
  return x + 1.0;
}

} // namespace math_api

namespace text_api {

constexpr std::string_view word() {
  return "hello";
}

} // namespace text_api

constexpr auto math_runtime = ct_lua54::interpreter()
  .with_libraries<ct_lua54::LIB_BASE | ct_lua54::LIB_MATH>()
  .with_namespace<^^math_api>();

constexpr auto text_runtime = ct_lua54::interpreter()
  .with_mask<ct_lua54::LIB_BASE | ct_lua54::LIB_STRING>()
  .with_namespace<^^text_api>();

constexpr auto combined_runtime = ct_lua54::interpreter()
  .with_libraries<ct_lua54::LIB_BASE | ct_lua54::LIB_MATH | ct_lua54::LIB_STRING>()
  .with_namespace<^^math_api>()
  .with_namespace<^^text_api>();

constexpr double r_runtime_math = math_runtime.run_number<fixed_string{R"lua(
if type(inc) ~= "function" then return 101 end
if inc(41) ~= 42 then return 102 end
if type(word) ~= "nil" then return 103 end
if type(math) ~= "table" then return 104 end
if type(string) ~= "nil" then return 105 end
return 1
)lua"}>();

constexpr double r_runtime_text = text_runtime.run_number<fixed_string{R"lua(
if type(word) ~= "function" then return 201 end
if word() ~= "hello" then return 202 end
if string.upper(word()) ~= "HELLO" then return 203 end
if type(inc) ~= "nil" then return 204 end
if type(math) ~= "nil" then return 205 end
return 1
)lua"}>();

constexpr double r_runtime_combined = combined_runtime.run_number<fixed_string{R"lua(
if inc(9) ~= 10 then return 301 end
if string.reverse(word()) ~= "olleh" then return 302 end
return 1
)lua"}>();

static_assert(r_runtime_math == 1.0, "math runtime example failed");
static_assert(r_runtime_text == 1.0, "text runtime example failed");
static_assert(r_runtime_combined == 1.0, "combined runtime example failed");

int main() { return 0; }
