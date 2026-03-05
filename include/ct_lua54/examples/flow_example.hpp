#pragma once

namespace ct_lua54::examples {

inline constexpr double r_flow = ct_lua54::run_number<fixed_string{R"lua(
local function FAIL(msg) __THIS_FUNCTION_DOES_NOT_EXIST__(msg) end
local function CHECK(c,msg) if not c then FAIL(msg) end end
local function EQ(a,b,msg) if a ~= b then FAIL(msg) end end

local x = 1
do
  local x = 2
end
EQ(x, 1, "do scope")

local s = 0
local i = 1
while i <= 5 do
  s = s + i
  i = i + 1
end
EQ(s, 15, "while")

local r = 0
repeat
  r = r + 1
until r == 3
EQ(r, 3, "repeat")

function tri(n)
  local t = 0
  for j = 1, n do
    t = t + j
  end
  return t
end
EQ(tri(10), 55, "for num")

function va(...) return ... end
local a,b,c = va(1,2,3)
EQ(a, 1, "va a")
EQ(b, 2, "va b")
EQ(c, 3, "va c")

function mr() return 10,20,30 end
local t = { 1, mr() }
EQ(#t, 4, "ctor multret len")
EQ(t[1], 1, "t1")
EQ(t[2], 10, "t2")
EQ(t[4], 30, "t4")
local u = { x=1, ["y"]=2, [3]=4 }
EQ(u.x, 1, "u.x")
EQ(u["y"], 2, "u[y]")
EQ(u[3], 4, "u[3]")
EQ(#"hello", 5, "len str")
EQ(#t, 4, "len table")
EQ("a".."b", "ab", "concat ss")
EQ("a"..1, "a1", "concat sn")

local M = { base=10, inc=function(self, v) return self.base + v end }
EQ(M:inc(5), 15, "method call")

local NS = { a = {} }
function NS.a.b() return 77 end
EQ(NS.a.b(), 77, "fn a.b.c")

local O = { base = 40 }
function O:add(v) return self.base + v end
EQ(O:add(2), 42, "fn a:b")

local g = 0
goto after_forward
g = 99
::after_forward::
EQ(g, 0, "goto forward")

local n = 0
if n == 0 then goto set_two end
n = 99
::set_two::
n = n + 2
EQ(n, 2, "goto branch")

return 1
)lua"}, ct_lua54::LIB_BASE>();
static_assert(r_flow == 1.0, "flow example failed");

} // namespace ct_lua54::examples

