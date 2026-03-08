#include "ct_lua54.hpp"

namespace api {

enum class Color : std::int64_t {
  Red = 1,
  Blue = 2,
};

struct Vec2 {
  double x{};
  double y{};

  constexpr double sum() const { return x + y; }
  constexpr void translate(double dx, double dy) {
    x += dx;
    y += dy;
  }
};

constexpr Vec2 make_vec(double x, double y) {
  Vec2 v{};
  v.x = x;
  v.y = y;
  return v;
}

constexpr double sum_vec(const Vec2& v) {
  return v.x + v.y;
}

constexpr bool is_blue(Color c) {
  return c == Color::Blue;
}

} // namespace api

constexpr auto api_runtime = ct_lua54::interpreter()
  .with_libraries<ct_lua54::LIB_BASE | ct_lua54::LIB_API>()
  .with_namespace<^^api>();

constexpr double r_api = api_runtime.run_number<fixed_string{R"lua(
local v = Vec2(3, 4)
if type(v) ~= "userdata" then return 101 end
if tostring(v) ~= "Vec2" then return 102 end
if v.x ~= 3 or v.y ~= 4 then return 103 end

v:translate(1, 2)
if v.x ~= 4 or v.y ~= 6 then return 104 end
if v:sum() ~= 10 then return 105 end

local w = make_vec(7, 8)
if w:sum() ~= 15 then return 106 end
if sum_vec(w) ~= 15 then return 107 end

if Color.Red ~= 1 or Color.Blue ~= 2 then return 108 end
if is_blue(Color.Blue) ~= true then return 109 end
if is_blue(Color.Red) ~= false then return 110 end

local fields = {}
for k, vv in pairs(v) do
  fields[k] = vv
end
if fields.x ~= 4 or fields.y ~= 6 then return 111 end

v.x = 9
if v:sum() ~= 15 then return 112 end

return 1
)lua"}>();

static_assert(r_api == 1.0, "api reflection example failed");

int main() { return 0; }
