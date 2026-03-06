#include "ct_lua54.hpp"

constexpr double r_expr = ct_lua54::run_number<fixed_string{R"lua(
local function FAIL(msg) __THIS_FUNCTION_DOES_NOT_EXIST__(msg) end
local function CHECK(c,msg) if not c then FAIL(msg) end end
local function EQ(a,b,msg) if a ~= b then FAIL(msg) end end

EQ(type(nil), "nil", "type nil")
EQ(type(true), "boolean", "type bool")
EQ(type(1), "number", "type int")
EQ(type(1.25), "number", "type num")
EQ(type("hi"), "string", "type str")
EQ(type({}), "table", "type table")
EQ(type(function() end), "function", "type fn")

EQ(1 + 2*3, 7, "precedence")
EQ(7//2, 3, "idiv")
EQ(7%4, 3, "mod")
EQ(10/4, 2.5, "div")
EQ(-5 + 6, 1, "unary minus")
EQ(2^3, 8, "pow simple")
EQ(2^3^2, 512, "pow right assoc")
EQ(-2^2, -4, "pow precedence unary")

EQ(6 & 3, 2, "bit and")
EQ(6 | 3, 7, "bit or")
EQ(6 ~ 3, 5, "bit xor")
EQ(1 << 4, 16, "shl")
EQ(16 >> 2, 4, "shr")
EQ(~0, -1, "bit not")

EQ(5.5 % 2, 1.5, "float mod")
EQ(-5.5 % 2, 0.5, "float mod neg")
EQ(0x10, 16, "hex int")
EQ(0xff, 255, "hex int lower")
EQ(0x1p4, 16, "hex float")
EQ(0x1.8p1, 3, "hex float frac")
EQ(1_000, 1000, "underscore dec")
EQ(0xFF_FF, 65535, "underscore hex")

local esc = "A\x42\u{43}\z
  D"
EQ(esc, "ABCD", "escapes")
EQ("\065\066\067", "ABC", "dec escapes")
--[[ long
comment
block ]]
--[=[
long comment with equals
]=]
EQ(1, 1, "long comments")

local lb1 = [[hello
world]]
EQ(lb1, "hello\nworld", "long bracket 1")
local lb2 = [=[a [[b]] c]=]
EQ(lb2, "a [[b]] c", "long bracket 2")

CHECK(3 < 4 and 4 <= 4 and 5 > 4 and 5 >= 5, "cmp num")
CHECK("a" < "b" and "b" > "a", "cmp str")
EQ(1 == 1, true, "eq")
EQ(1 ~= 2, true, "ne")
EQ(false and 7, false, "and")
EQ(nil or 9, 9, "or")
EQ(0 and 9, 9, "truthy 0")
EQ(not nil, true, "not")

return 1
)lua"}, ct_lua54::LIB_BASE>();

static_assert(r_expr == 1.0, "expr example failed");

int main() { return 0; }


