#include "ct_lua54.hpp"

constexpr auto string_runtime = ct_lua54::interpreter()
  .with_libraries<ct_lua54::LIB_BASE | ct_lua54::LIB_STRING>();

constexpr double r_string = string_runtime.run_number<fixed_string{R"lua(
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
EQ(string.match("(a(b)c)d", "%b()"), "(a(b)c)", "string.match %b")
EQ(string.match("!", "%g"), "!", "string.match %g")
EQ(string.match("_", "%w"), nil, "string.match %w excludes underscore")

local fi, fj, fk, fv = string.find("alpha=123", "(%a+)=(%d+)")
EQ(fi, 1, "string.find capture i")
EQ(fj, 9, "string.find capture j")
EQ(fk, "alpha", "string.find capture 1")
EQ(fv, "123", "string.find capture 2")

local ffi, ffj = string.find("abc", "%f[%a]")
EQ(ffi, 1, "string.find frontier start i")
EQ(ffj, 0, "string.find frontier start j")
local fei, fej = string.find("abc", "%f[%A]")
EQ(fei, 4, "string.find frontier end i")
EQ(fej, 3, "string.find frontier end j")
EQ(string.match("]", "[]]"), "]", "string.match set leading close bracket")
EQ(string.match("-", "[-]"), "-", "string.match set dash literal")
EQ(string.match("x", "[^%d]"), "x", "string.match negated set class")

local mk, mv = string.match("beta=456", "(%a+)=(%d+)")
EQ(mk, "beta", "string.match capture 1")
EQ(mv, "456", "string.match capture 2")

local gm = ""
for k, v in string.gmatch("x=1;y=22", "(%a)=(%d+)") do
  gm = gm .. k .. v .. ","
end
EQ(gm, "x1,y22,", "string.gmatch captures")

local gm2 = ""
for d in string.gmatch("a1 b22 c333", "%d+") do
  gm2 = gm2 .. d .. ","
end
EQ(gm2, "1,22,333,", "string.gmatch whole match")

local gm3 = ""
for w in string.gmatch("one,two;3", "%f[%a]%a+") do
  gm3 = gm3 .. w .. ","
end
EQ(gm3, "one,two,", "string.gmatch frontier words")

local gm4 = ""
for pos in string.gmatch("abc", "()") do
  gm4 = gm4 .. pos .. ","
end
EQ(gm4, "1,2,3,4,", "string.gmatch empty positions")

local gs, gc = string.gsub("a1 b22 c333", "%d+", "#")
EQ(gs, "a# b# c#", "string.gsub out")
EQ(gc, 3, "string.gsub count")

local gs2, gc2 = string.gsub("foo=42", "(%a+)=(%d+)", "%2:%1:%0")
EQ(gs2, "42:foo:foo=42", "string.gsub capture template")
EQ(gc2, 1, "string.gsub capture template count")

local gs3, gc3 = string.gsub("a1 b22 c333", "(%d+)", function(d) return "[" .. d .. "]" end)
EQ(gs3, "a[1] b[22] c[333]", "string.gsub function")
EQ(gc3, 3, "string.gsub function count")

local map = { cat = "meow", dog = "woof" }
local gs4, gc4 = string.gsub("cat dog eel", "%a+", map)
EQ(gs4, "meow woof eel", "string.gsub table")
EQ(gc4, 3, "string.gsub table count")

local gs5, gc5 = string.gsub("one two", "%f[%a]", "|")
EQ(gs5, "|one |two", "string.gsub frontier empty")
EQ(gc5, 2, "string.gsub frontier empty count")
local gs6, gc6 = string.gsub("abc", "", "|")
EQ(gs6, "|a|b|c|", "string.gsub empty pattern")
EQ(gc6, 4, "string.gsub empty pattern count")

local ok1, err1 = pcall(function() return string.match("a", "%") end)
CHECK(not ok1 and string.find(err1, "malformed pattern", 1, true), "string malformed trailing percent")
local ok2, err2 = pcall(function() return string.match("a", "%1") end)
CHECK(not ok2 and string.find(err2, "invalid capture index", 1, true), "string invalid capture index")
local ok3, err3 = pcall(function() return string.match("a", "%f") end)
CHECK(not ok3 and string.find(err3, "malformed pattern", 1, true), "string malformed frontier")

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
EQ(string.format("%+05d", 12), "+0012", "string.format signed zero pad")
EQ(string.format("%#.0o", 0), "0", "string.format alt octal zero")
EQ(string.format("%.2e", 12.5), "1.25e+01", "string.format scientific")
EQ(string.format("%.4g", 12.34), "12.34", "string.format general fixed")
EQ(string.format("%.4g", 12345.0), "1.235e+04", "string.format general scientific")
EQ(string.format("%a", 1.5), "0x1.8p+0", "string.format hexfloat lower")
EQ(string.format("%A", 1.5), "0X1.8P+0", "string.format hexfloat upper")
CHECK(string.sub(string.format("%p", {}), 1, 2) == "0x", "string.format pointer")
EQ(string.format("%s", true), "true", "string.format tostring")
EQ(string.format("%q", "a\nb"), "\"a\\nb\"", "string.format quoted")

EQ(string.packsize("<i2I2c3"), 7, "string.packsize fixed")
EQ(string.packsize("!4bi4"), 8, "string.packsize aligned")
local pb = string.pack(">I2", 0x1234)
local pb1, pb2 = string.byte(pb, 1, 2)
EQ(pb1, 0x12, "string.pack big-endian byte1")
EQ(pb2, 0x34, "string.pack big-endian byte2")

local ps = string.pack("<i2I2c5z", -2, 513, "Lua", "hi")
EQ(string.len(ps), 12, "string.pack mixed len")
local ua, ub, uc, ud, up = string.unpack("<i2I2c5z", ps)
EQ(ua, -2, "string.unpack sint")
EQ(ub, 513, "string.unpack uint")
EQ(uc, "Lua" .. string.char(0,0), "string.unpack fixed string")
EQ(ud, "hi", "string.unpack z string")
EQ(up, 13, "string.unpack next pos")

local ls = string.pack("<s2", "lua")
local ls1, ls2 = string.byte(ls, 1, 2)
EQ(ls1, 3, "string.pack length-prefixed byte1")
EQ(ls2, 0, "string.pack length-prefixed byte2")
local luv, lup = string.unpack("<s2", ls)
EQ(luv, "lua", "string.unpack length-prefixed string")
EQ(lup, 6, "string.unpack length-prefixed next pos")

local fb = string.pack("<fd", 1.5, 2.25)
local fvu, dvu, fpu = string.unpack("<fd", fb)
EQ(fvu, 1.5, "string.unpack float")
EQ(dvu, 2.25, "string.unpack double")
EQ(fpu, string.len(fb) + 1, "string.unpack float/double next pos")

local okps, errps = pcall(function() return string.packsize("z") end)
CHECK(not okps and string.find(errps, "variable-length format", 1, true), "string.packsize variable length error")
EQ(("abc"):upper(), "ABC", "string metatable index")

return 1
)lua"}>();

static_assert(r_string == 1.0, "string example failed");

int main() { return 0; }
