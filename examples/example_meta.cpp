#include "ct_lua54.hpp"

constexpr auto meta_runtime = ct_lua54::interpreter().with_libraries<ct_lua54::LIB_BASE>();

constexpr double r_meta = meta_runtime.run_number<fixed_string{R"lua(
local function FAIL(msg) __THIS_FUNCTION_DOES_NOT_EXIST__(msg) end
local function CHECK(c,msg) if not c then FAIL(msg) end end
local function EQ(a,b,msg) if a ~= b then FAIL(msg) end end

local o = setmetatable({}, { __index = { z = 99 } })
EQ(o.z, 99, "__index table")
EQ(type(getmetatable(o)), "table", "getmetatable")

do
  local mt = { __index = { z = 7 }, __metatable = "locked" }
  local obj = setmetatable({}, mt)
  EQ(getmetatable(obj), "locked", "__metatable hides metatable")
  local ok1, e1 = pcall(function() setmetatable(obj, {}) end)
  CHECK(ok1 == false and type(e1) == "string", "__metatable blocks replace")
  EQ(obj.z, 7, "__metatable protection preserves mt")
end

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

local TS = setmetatable({}, { __tostring = function(self) return "TS" end })
EQ(tostring(TS), "TS", "__tostring")
CHECK(print(TS) == nil, "print uses tostring path")

local se = setmetatable({}, { __eq = function(a,b) return false end })
EQ(se == se, true, "__eq not used for identical object")

local ea = setmetatable({}, { __eq = function(a,b) return true end })
local eb = {}
EQ(ea == eb, false, "__eq requires both operands")

local eqf = function(a,b) return true end
local em = { __eq = eqf }
local e1 = setmetatable({}, em)
local e2 = setmetatable({}, em)
EQ(e1 == e2, true, "__eq shared metamethod")

local e3 = setmetatable({}, { __eq = function(a,b) return true end })
local e4 = setmetatable({}, { __eq = function(a,b) return true end })
EQ(e3 == e4, false, "__eq different metamethods")

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

local P2 = setmetatable({}, {
  __pairs = function(self)
    local i = 0
    local function it(st, var)
      i = i + 1
      if i == 1 then return "a", 10 end
      if i == 2 then return "b", 20 end
      return nil
    end
    return it, self, nil
  end
})
local ps = 0
for k,v in pairs(P2) do
  ps = ps + v
end
EQ(ps, 30, "__pairs")

local BW = setmetatable({}, {
  __band = function(a,b) return 11 end,
  __bor  = function(a,b) return 12 end,
  __bxor = function(a,b) return 13 end,
  __shl  = function(a,b) return 14 end,
  __shr  = function(a,b) return 15 end,
  __bnot = function(a)   return 16 end
})
EQ(BW & 1, 11, "__band")
EQ(BW | 1, 12, "__bor")
EQ(BW ~ 1, 13, "__bxor")
EQ(BW << 1, 14, "__shl")
EQ(BW >> 1, 15, "__shr")
EQ(~BW, 16, "__bnot")

return 1
)lua"}>();

static_assert(r_meta == 1.0, "meta example failed");

constexpr double r_meta_protected_clear = meta_runtime.run_number<fixed_string{R"lua(
local mt = { __index = { z = 7 }, __metatable = "locked" }
local obj = setmetatable({}, mt)
local ok, e = pcall(function() setmetatable(obj, nil) end)
if ok ~= false or type(e) ~= "string" then return 1 end
if getmetatable(obj) ~= "locked" then return 2 end
if obj.z ~= 7 then return 3 end
return 0
)lua"}>();

static_assert(r_meta_protected_clear == 0.0, "protected metatable clear failed");

int main() { return 0; }
