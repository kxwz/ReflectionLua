#include "ct_lua54.hpp"

constexpr double r_utf8 = ct_lua54::run_number<fixed_string{R"lua(
local function FAIL(msg) __THIS_FUNCTION_DOES_NOT_EXIST__(msg) end
local function CHECK(c,msg) if not c then FAIL(msg) end end
local function EQ(a,b,msg) if a ~= b then FAIL(msg) end end

local s = utf8.char(0x41, 0xE9, 0x20AC)
local c1,c2,c3 = utf8.codepoint(s, 1, #s)
EQ(c1, 0x41, "utf8.codepoint 1")
EQ(c2, 0xE9, "utf8.codepoint 2")
EQ(c3, 0x20AC, "utf8.codepoint 3")

EQ(utf8.len(s), 3, "utf8.len")
local bad, pos = utf8.len("\xC3\x28")
EQ(bad, nil, "utf8.len invalid nil")
EQ(pos, 1, "utf8.len invalid pos")

local it, st, var = utf8.codes(s)
local i1,v1 = it(st,var)
EQ(i1, 1, "utf8.codes i1")
EQ(v1, 0x41, "utf8.codes v1")
local i2,v2 = it(st,i1)
EQ(i2, 2, "utf8.codes i2")
EQ(v2, 0xE9, "utf8.codes v2")
local i3,v3 = it(st,i2)
EQ(i3, 4, "utf8.codes i3")
EQ(v3, 0x20AC, "utf8.codes v3")
local i4,v4 = it(st,i3)
EQ(i4, nil, "utf8.codes end")

EQ(utf8.offset(s, 1, 1), 1, "utf8.offset +1")
EQ(utf8.offset(s, 2, 1), 2, "utf8.offset +2")
EQ(utf8.offset(s, 3, 1), 4, "utf8.offset +3")
EQ(utf8.offset(s, -1, #s + 1), 4, "utf8.offset -1")
EQ(utf8.offset(s, 0, 3), 2, "utf8.offset 0")

CHECK(utf8.charpattern ~= nil, "utf8.charpattern")

return 1
)lua"}, ct_lua54::LIB_BASE | ct_lua54::LIB_UTF8>();

static_assert(r_utf8 == 1.0, "utf8 example failed");

int main() { return 0; }
