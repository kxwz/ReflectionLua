#include "ct_lua54.hpp"

constexpr double r_meta = ct_lua54::run_number<fixed_string{R"lua(
local function FAIL(msg) __THIS_FUNCTION_DOES_NOT_EXIST__(msg) end
local function CHECK(c,msg) if not c then FAIL(msg) end end
local function EQ(a,b,msg) if a ~= b then FAIL(msg) end end

local o = setmetatable({}, { __index = { z = 99 } })
EQ(o.z, 99, "__index table")
EQ(type(getmetatable(o)), "table", "getmetatable")

local o2 = setmetatable({}, {
  __newindex = function(tt, k, v)
    rawset(tt, k, v*2)
  end
})
o2.a = 5
EQ(o2.a, 10, "__newindex fn")

local A = setmetatable({}, { __add = function(x,y) return 123 end })
local B = {}
EQ(A + B, 123, "__add")

local P = setmetatable({}, { __pow = function(x,y) return 9 end })
EQ(P ^ 0, 9, "__pow")

local F = setmetatable({}, { __call = function(self, v) return v + 1 end })
EQ(F(41), 42, "__call")

local ptab = { k=7, [1]=11, [2]=22 }
local k0, v0 = next(ptab, nil)
CHECK(k0 ~= nil, "next key")
CHECK(v0 ~= nil, "next val")
local sum = 0
for k,v in pairs(ptab) do
  if type(k) == "number" then
    sum = sum + v
  end
end
EQ(sum, 33, "pairs/for-in")
rawset(ptab, "q", 8)
EQ(rawget(ptab, "q"), 8, "rawget/rawset")

return 1
)lua"}, ct_lua54::LIB_BASE>();

static_assert(r_meta == 1.0, "meta example failed");

int main() { return 0; }


