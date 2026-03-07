#include "ct_lua54.hpp"

constexpr double r_all = ct_lua54::run_number<
  fixed_string{R"lua(
if rawequal(1, 1.0) ~= true then return 103 end
if rawequal({}, {}) ~= false then return 104 end
if rawlen("hello") ~= 5 then return 105 end

do
  local rl = setmetatable({1,2}, {
    __len = function() return 99 end,
    __index = function() return 77 end
  })
  if rawlen(rl) ~= 2 then return 106 end
end

if type(error) ~= "function" then return 107 end
if type(ipairs) ~= "function" then return 108 end
if type(select) ~= "function" then return 109 end
if type(tonumber) ~= "function" then return 110 end
if type(pcall) ~= "function" then return 111 end
if type(xpcall) ~= "function" then return 112 end
if type(warn) ~= "function" then return 113 end

do
  local ok, v = pcall(function()
    local U = setmetatable({}, { __unm = function(...) return select("#", ...) end })
    return -U
  end)
  if not ok or v ~= 1 then return 101 end
end

do
  local ok, a, b = pcall(function()
    return assert(11, 22)
  end)
  if not ok or a ~= 11 or b ~= 22 then return 102 end
end

do
  local ok, sum = pcall(function()
    local isum = 0
    for _, v in ipairs({10,20,nil,30}) do
      isum = isum + v
    end
    return isum
  end)
  if not ok or sum ~= 30 then return 120 end
end

do
  local ok, s1, s2, n = pcall(function()
    local a, b = select(2, "a", "b", "c")
    return a, b, select("#", 1, 2, 3)
  end)
  if not ok or s1 ~= "b" or s2 ~= "c" or n ~= 3 then return 130 end
end

do
  local ok, a, b, c, d = pcall(function()
    return tonumber("42"), tonumber("-0x10"), tonumber("101", 2), tonumber("nope")
  end)
  if not ok or a ~= 42 or b ~= -16 or c ~= 5 or d ~= nil then return 140 end
end

do
  local ok, okp, p1, p2 = pcall(function()
    return pcall(function(x, y) return x + y, x * y end, 3, 4)
  end)
  if not ok or okp ~= true or p1 ~= 7 or p2 ~= 12 then return 150 end
end

do
  local ok, okp, pe = pcall(function()
    return pcall(function() error("boom") end)
  end)
  if not ok or okp ~= false or type(pe) ~= "string" then return 151 end
end

do
  local ok, okx, xv = pcall(function()
    return xpcall(function(a, b) return a - b end, function(e) return e end, 9, 2)
  end)
  if not ok or okx ~= true or xv ~= 7 then return 160 end
end

do
  local ok, okx, xe = pcall(function()
    return xpcall(
      function() error("boom") end,
      function(e) return "handled:" .. tostring(e) end
    )
  end)
  if not ok or okx ~= false or type(xe) ~= "string" then return 161 end
end

do
  local ok, a, b, c, d = pcall(function()
    local warn_nil = (warn("hello") == nil)
    warn("@off")
    warn("muted")
    warn("@on")
    warn("visible")
    print("compile-time", 123)
    return warn_nil, (print("ok") == nil), type(print), type(table)
  end)
  if not ok or a ~= true or b ~= true or c ~= "function" or d ~= "table" then return 170 end
end

do
  local ok, a, b, c, d = pcall(function()
    return table.concat(table.pack("a", "b"), ",", 1, 2), string.upper("ab"), math.floor(1.75), utf8.len("A")
  end)
  if not ok or a ~= "a,b" or b ~= "AB" or c ~= 1 or d ~= 1 then return 180 end
end

return 1
)lua"},
  ct_lua54::LIB_ALL
>();

template <int Code>
struct example_all_code;

template <>
struct example_all_code<1> {};

constexpr example_all_code<(int)r_all> example_all_status{};
static_assert(r_all == 1.0, "all-libs example failed");

int main() { return 0; }
