#include "ct_lua54.hpp"

constexpr auto table_runtime = ct_lua54::interpreter()
  .with_libraries<ct_lua54::LIB_BASE | ct_lua54::LIB_TABLE>();

constexpr double r_table = table_runtime.run_number<fixed_string{R"lua(
local function FAIL(msg) __THIS_FUNCTION_DOES_NOT_EXIST__(msg) end
local function CHECK(c,msg) if not c then FAIL(msg) end end
local function EQ(a,b,msg) if a ~= b then FAIL(msg) end end

local li = {1,3}
table.insert(li, 2, 2)
table.insert(li, 4)
EQ(#li, 4, "insert size")
EQ(li[2], 2, "insert pos")
EQ(li[4], 4, "insert tail")

local rv = table.remove(li, 2)
EQ(rv, 2, "remove value")
EQ(#li, 3, "remove size")
EQ(li[2], 3, "remove shift")

local ss = {5,1,4,2,3}
table.sort(ss)
EQ(ss[1], 1, "sort asc 1")
EQ(ss[5], 5, "sort asc 5")
table.sort(ss, function(a,b) return a>b end)
EQ(ss[1], 5, "sort cmp 1")
EQ(ss[5], 1, "sort cmp 5")

EQ(table.concat({"a","b","c"}), "abc", "concat default")
EQ(table.concat({"a","b","c"}, "-"), "a-b-c", "concat sep")
EQ(table.concat({1,2,3}, ",", 2, 3), "2,3", "concat range")

local pk = table.pack("x", "y", nil, "z")
EQ(pk.n, 4, "pack n")
local p1,p2,p3,p4 = table.unpack(pk, 1, 4)
EQ(p1, "x", "unpack p1")
EQ(p2, "y", "unpack p2")
CHECK(p3 == nil, "unpack p3 nil")
EQ(p4, "z", "unpack p4")

local dst = {}
local ret = table.move({11,22,33}, 1, 3, 1, dst)
EQ(ret, dst, "move return")
EQ(dst[1], 11, "move dst1")
EQ(dst[3], 33, "move dst3")

local ov = {1,2,3,4}
table.move(ov, 1, 3, 2, ov)
EQ(ov[1], 1, "move overlap 1")
EQ(ov[2], 1, "move overlap 2")
EQ(ov[4], 3, "move overlap 4")

return 1
)lua"}>();

static_assert(r_table == 1.0, "table example failed");

int main() { return 0; }

