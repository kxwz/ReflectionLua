#include "ct_lua54.hpp"

constexpr auto regressions_runtime = ct_lua54::interpreter()
  .with_libraries<ct_lua54::LIB_BASE | ct_lua54::LIB_MATH>();

constexpr double r_regressions = regressions_runtime.run_number<fixed_string{R"lua(
local function keep(a, b, c, d)
  if a ~= "A" then return 201 end
  if b ~= 1 then return 202 end
  if c ~= "nil" then return 203 end
  if d ~= "D" then return 204 end
  return 0
end

if keep("A", 1, type(nil), "D") ~= 0 then return 210 end

do
  local ok1, e1 = pcall(function() assert(false) end)
  if ok1 ~= false or type(e1) ~= "string" then return 220 end

  local ok2, e2 = pcall(function() assert(false) end)
  if ok2 ~= false or type(e2) ~= "string" then return 221 end
end

if tostring(math.mininteger) ~= "-9223372036854775808" then return 230 end

do
  local x = 1
  local x = 2
  if x ~= 2 then return 240 end
  x = 3
  if x ~= 3 then return 241 end
end

do
  local t = {}
  t[1] = "int"
  if t[1.0] ~= "int" then return 250 end
  t[1.0] = "num"
  if t[1] ~= "num" then return 251 end
  rawset(t, 1.0, "raw")
  if rawget(t, 1) ~= "raw" then return 252 end
  local k, v = next(t)
  if k ~= 1 or v ~= "raw" then return 253 end
  if next(t, k) ~= nil then return 254 end
  t[1] = nil
  if next(t) ~= nil then return 255 end
end

if rawequal(9007199254740993, 9007199254740992.0) then return 260 end

do
  local t = {}
  t[9007199254740993] = "big"
  if t[9007199254740992.0] ~= nil then return 261 end
end

return 1
)lua"}>();

static_assert(r_regressions == 1.0, "regression example failed");

constexpr bool rawset_rejects_nan_table_key() {
  ct_lua54::Heap h{};
  ct_lua54::TableId t = h.new_table_pow2();
  constexpr double nan = std::bit_cast<double>(0x7ff8000000000000ull);
  try {
    h.rawset(t, ct_lua54::Value::number(nan), ct_lua54::Value::integer(1));
  } catch (const char*) {
    return true;
  }
  return false;
}

static_assert(rawset_rejects_nan_table_key(), "rawset should reject NaN table keys");

int main() { return 0; }
