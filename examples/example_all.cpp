#include "ct_lua54.hpp"

constexpr double r_all = ct_lua54::run_number<
  fixed_string{R"lua(
local function FAIL(msg) error(msg) end
local function CHECK(c,msg) if not c then FAIL(msg) end end
local function EQ(a,b,msg) if a ~= b then FAIL(msg) end end

local U = setmetatable({}, { __unm = function(...) return select("#", ...) end })
EQ(-U, 1, "__unm arity")

local ax, ay = assert(11, 22)
EQ(ax, 11, "assert returns first")
EQ(ay, 22, "assert returns second")
EQ(rawequal(1, 1.0), true, "rawequal num/int")
EQ(rawequal({}, {}), false, "rawequal table identity")
EQ(rawlen("hello"), 5, "rawlen string")
local rl = setmetatable({1,2}, { __len = function() return 99 end, __index = function() return 77 end })
EQ(rawlen(rl), 2, "rawlen table raw")
EQ(type(error), "function", "error exists")
EQ(type(ipairs), "function", "ipairs exists")
EQ(type(select), "function", "select exists")
EQ(type(tonumber), "function", "tonumber exists")
EQ(type(pcall), "function", "pcall exists")
EQ(type(xpcall), "function", "xpcall exists")
EQ(type(warn), "function", "warn exists")

local isum = 0
for k,v in ipairs({10,20,nil,30}) do
  isum = isum + v
end
EQ(isum, 30, "ipairs sum")

local s1,s2 = select(2, "a", "b", "c")
EQ(s1, "b", "select from index")
EQ(s2, "c", "select tail")
EQ(select("#", 1,2,3), 3, "select count")
local n1,n2 = select(-2, 7,8,9)
EQ(n1, 8, "select negative 1")
EQ(n2, 9, "select negative 2")

EQ(tonumber("42"), 42, "tonumber dec")
EQ(tonumber("-0x10"), -16, "tonumber hex sign")
EQ(tonumber("101", 2), 5, "tonumber base2")
CHECK(tonumber("2", 2) == nil, "tonumber base invalid digit")
CHECK(tonumber("nope") == nil, "tonumber invalid")

local okp, p1, p2 = pcall(function(x,y) return x+y, x*y end, 3, 4)
EQ(okp, true, "pcall ok status")
EQ(p1, 7, "pcall ret1")
EQ(p2, 12, "pcall ret2")
local okpe, pe = pcall(function() error("boom") end)
EQ(okpe, false, "pcall err status")
CHECK(type(pe) == "string", "pcall err value")

local okx, xv = xpcall(function(a,b) return a-b end, function(e) return e end, 9, 2)
EQ(okx, true, "xpcall ok status")
EQ(xv, 7, "xpcall ok ret")
local okxe, xe = xpcall(function() error("boom") end, function(e) return "handled:"..tostring(e) end)
EQ(okxe, false, "xpcall err status")
CHECK(type(xe) == "string", "xpcall err value")

CHECK(warn("hello") == nil, "warn nil")
warn("@off")
warn("muted")
warn("@on")
warn("visible")

print("compile-time", 123)
return (type(1) == "number" and type(print) == "function" and print("ok") == nil) and 1 or 0
)lua"},
  ct_lua54::LIB_ALL
>();
static_assert(r_all == 1.0, "all-libs example failed");

int main() { return 0; }


