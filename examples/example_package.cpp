#include "ct_lua54.hpp"

constexpr auto package_runtime = ct_lua54::interpreter()
  .with_libraries<ct_lua54::LIB_BASE | ct_lua54::LIB_PACKAGE>()
  .with_module<fixed_string{"embedded.answer"}, fixed_string{R"lua(
return { value = 42 }
)lua"}>()
  .with_module<fixed_string{"embedded.initonly"}, fixed_string{R"lua(
_G.init_hits = (_G.init_hits or 0) + 1
)lua"}>()
  .with_module<fixed_string{"cycle.a"}, fixed_string{R"lua(
local b = require("cycle.b")
return { name = "a", dep = b }
)lua"}>()
  .with_module<fixed_string{"cycle.b"}, fixed_string{R"lua(
local a = require("cycle.a")
return { name = "b", dep = a }
)lua"}>();

constexpr double r_package = package_runtime.run_number<
  fixed_string{R"lua(
if type(require) ~= "function" then return 101 end
if type(package) ~= "table" then return 102 end
if type(package.loaded) ~= "table" or type(package.preload) ~= "table" or type(package.searchers) ~= "table" then return 103 end
if type(rawget(package.searchers, 1)) ~= "function" then return 104 end
if type(rawget(package.searchers, 2)) ~= "function" then return 105 end
if rawget(package.searchers, 3) ~= nil then return 106 end

local preload_hits = 0
package.preload["preloaded"] = function(name, marker)
  preload_hits = preload_hits + 1
  return { name = name, marker = marker }
end

local p1 = require("preloaded")
local p2 = require("preloaded")
if p1 ~= p2 or preload_hits ~= 1 or p1.name ~= "preloaded" then return 107 end

local e1 = require("embedded.answer")
local e2 = require("embedded.answer")
if e1 ~= e2 or e1.value ~= 42 then return 108 end

local i1 = require("embedded.initonly")
local i2 = require("embedded.initonly")
if i1 ~= true or i2 ~= true or init_hits ~= 1 then return 109 end

local ok_cycle, err_cycle = pcall(require, "cycle.a")
if ok_cycle or type(err_cycle) ~= "string" then return 110 end
if rawget(package.loaded, "cycle.a") ~= nil or rawget(package.loaded, "cycle.b") ~= nil then return 111 end

local ok_missing, err_missing = pcall(require, "missing.module")
if ok_missing or type(err_missing) ~= "string" then return 112 end

return 1
)lua"}
>();

template <int Code>
struct example_package_code;

template <>
struct example_package_code<1> {};

constexpr example_package_code<(int)r_package> example_package_status{};
static_assert(r_package == 1.0, "package example failed");

int main() { return 0; }
