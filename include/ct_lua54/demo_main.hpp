#pragma once


// ---------------- demo ----------------
int main() {
  // no libs selected: core language still works
  constexpr double r_core = ct_lua54::run_number<fixed_string{R"lua(return 1)lua"}, 0u>();

  // split tests to stay below clang constexpr per-expression step budget
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

  constexpr double r_flow = ct_lua54::run_number<fixed_string{R"lua(
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

  constexpr double r_table = ct_lua54::run_number<fixed_string{R"lua(
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
  )lua"}, ct_lua54::LIB_BASE | ct_lua54::LIB_TABLE>();

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

  // base + api lib mask (api is currently empty, but selection path is exercised)
  constexpr double r_all = ct_lua54::run_number<
    fixed_string{R"lua(return type(1) == "number" and 1 or 0)lua"},
    ct_lua54::LIB_ALL
  >();

  std::cout << "lua result (core) = " << r_core << "\n";
  std::cout << "lua result (base expr) = " << r_expr << "\n";
  std::cout << "lua result (base flow) = " << r_flow << "\n";
  std::cout << "lua result (base meta) = " << r_meta << "\n";
  std::cout << "lua result (base+table) = " << r_table << "\n";
  std::cout << "lua result (base+math) = " << r_math << "\n";
  std::cout << "lua result (base+string) = " << r_string << "\n";
  std::cout << "lua result (all) = " << r_all << "\n";
}
