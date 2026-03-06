#include "ct_lua54.hpp"

constexpr double r_string = ct_lua54::run_number<fixed_string{R"lua(
local function FAIL(msg) __THIS_FUNCTION_DOES_NOT_EXIST__(msg) end
local function CHECK(c,msg) if not c then FAIL(msg) end end
local function EQ(a,b,msg) if a ~= b then FAIL(msg) end end

EQ(string.len("hello"), 5, "string.len")
EQ(string.sub("abcdef", 2, 4), "bcd", "string.sub range")
EQ(string.sub("abcdef", -3, -1), "def", "string.sub neg")

local i,j = string.find("hello world", "world")
EQ(i, 7, "string.find i")
EQ(j, 11, "string.find j")
local p,q = string.find("a.b", ".", 1, true)
EQ(p, 2, "string.find plain i")
EQ(q, 2, "string.find plain j")

EQ(string.match("abc123def", "%d+"), "123", "string.match class+")

local gs, gc = string.gsub("a1 b22 c333", "%d+", "#")
EQ(gs, "a# b# c#", "string.gsub out")
EQ(gc, 3, "string.gsub count")

local b1,b2,b3 = string.byte("ABC", 1, 3)
EQ(b1, 65, "string.byte 1")
EQ(b2, 66, "string.byte 2")
EQ(b3, 67, "string.byte 3")
EQ(string.char(65,66,67), "ABC", "string.char")

EQ(string.upper("Abc!"), "ABC!", "string.upper")
EQ(string.lower("AbC!"), "abc!", "string.lower")
EQ(string.rep("ab", 3), "ababab", "string.rep")
EQ(string.rep("ab", 3, ","), "ab,ab,ab", "string.rep sep")
EQ(string.reverse("stressed"), "desserts", "string.reverse")

EQ(string.format("x=%d y=%.2f %s %%", 7, 2.5, "ok"), "x=7 y=2.50 ok %", "string.format")
EQ(("abc"):upper(), "ABC", "string metatable index")

return 1
)lua"}, ct_lua54::LIB_BASE | ct_lua54::LIB_STRING>();

static_assert(r_string == 1.0, "string example failed");

int main() { return 0; }


