#include "ct_lua54.hpp"

constexpr double r_math = ct_lua54::run_number<fixed_string{R"lua(
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
NEAR(math.asin(1), math.pi/2, 1e-6, "asin")
NEAR(math.acos(1), 0, 1e-6, "acos")
NEAR(math.atan(1), math.pi/4, 1e-6, "atan 1arg")
NEAR(math.atan(1,1), math.pi/4, 1e-6, "atan 2arg")
NEAR(math.atan(0,-1), math.pi, 1e-6, "atan quadrants")
NEAR(math.deg(math.pi), 180, 1e-6, "deg")
NEAR(math.rad(180), math.pi, 1e-6, "rad")
NEAR(math.exp(1), 2.718281828, 1e-5, "exp")
EQ(math.fmod(7,3), 1, "fmod +")
EQ(math.fmod(-7,3), -1, "fmod -")
local ip, fp = math.modf(-3.25)
EQ(ip, -3, "modf int")
NEAR(fp, -0.25, 1e-9, "modf frac")
EQ(math.tointeger(3.0), 3, "tointeger ok")
CHECK(math.tointeger(3.25) == nil, "tointeger nil")
EQ(math.type(3), "integer", "math.type int")
EQ(math.type(3.0), "float", "math.type float")
CHECK(math.type("x") == nil, "math.type nil")
CHECK(math.ult(0, -1), "ult true")
CHECK(not math.ult(-1, 0), "ult false")

NEAR(math.sqrt(9), 3, 1e-9, "sqrt")
NEAR(math.log(8, 2), 3, 1e-6, "log base")
NEAR(math.sin(math.pi/2), 1, 1e-6, "sin")
NEAR(math.cos(0), 1, 1e-6, "cos")
NEAR(math.tan(0), 0, 1e-9, "tan")

CHECK(math.pi > 3 and math.pi < 4, "pi")
CHECK(math.huge > 1e300, "huge")
CHECK(math.mininteger < 0 and math.maxinteger > 0, "min/max integer signs")
CHECK(math.maxinteger > math.mininteger, "min/max integer order")

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

int main() { return 0; }


