#pragma once

namespace ct_lua54::examples {

inline constexpr double r_math = ct_lua54::run_number<fixed_string{R"lua(
local function FAIL(msg) __THIS_FUNCTION_DOES_NOT_EXIST__(msg) end
local function CHECK(c,msg) if not c then FAIL(msg) end end
local function EQ(a,b,msg) if a ~= b then FAIL(msg) end end
local function NEAR(a,b,eps,msg) if math.abs(a-b) > eps then FAIL(msg) end end

EQ(math.floor(2.9), 2, "floor +")
EQ(math.floor(-2.1), -3, "floor -")
EQ(math.ceil(2.1), 3, "ceil +")
EQ(math.ceil(-2.9), -2, "ceil -")
EQ(math.abs(-7), 7, "abs")
EQ(math.min(4,2,8,-1), -1, "min")
EQ(math.max(4,2,8,-1), 8, "max")

NEAR(math.sqrt(9), 3, 1e-9, "sqrt")
NEAR(math.log(8, 2), 3, 1e-6, "log base")
NEAR(math.sin(math.pi/2), 1, 1e-6, "sin")
NEAR(math.cos(0), 1, 1e-6, "cos")
NEAR(math.tan(0), 0, 1e-9, "tan")

CHECK(math.pi > 3 and math.pi < 4, "pi")
CHECK(math.huge > 1e300, "huge")

math.randomseed(12345)
local f1 = math.random()
local i1 = math.random(10)
local j1 = math.random(5,9)
CHECK(f1 >= 0 and f1 < 1, "random float range")
CHECK(i1 >= 1 and i1 <= 10, "random int range")
CHECK(j1 >= 5 and j1 <= 9, "random int2 range")

math.randomseed(12345)
local f2 = math.random()
local i2 = math.random(10)
local j2 = math.random(5,9)
NEAR(f1, f2, 1e-15, "randomseed float repeat")
EQ(i1, i2, "randomseed int repeat")
EQ(j1, j2, "randomseed int2 repeat")

return 1
)lua"}, ct_lua54::LIB_BASE | ct_lua54::LIB_MATH>();
static_assert(r_math == 1.0, "math example failed");

} // namespace ct_lua54::examples

