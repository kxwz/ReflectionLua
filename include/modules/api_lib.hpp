#pragma once

namespace ct_lua54 {

constexpr Multi VM::nf_reflect_udata_index(VM& vm, const Value* a, std::size_t n) {
  if (n < 2 || a[0].tag != Tag::UData) throw "Lua: reflected object index expects (self, key)";
  const UData& ud = vm.H.udata[a[0].u.id];
  if (a[1].tag != Tag::Str) return Multi::one(Value::nil());

  Value v = vm.H.rawget(ud.state, a[1]);
  if (!v.is_nil()) return Multi::one(v);

  Value methods = vm.H.rawget(ud.mt, Value::string(vm.s__ct_methods));
  if (methods.tag == Tag::Table) return Multi::one(vm.H.rawget(methods.t, a[1]));
  return Multi::one(Value::nil());
}

constexpr Multi VM::nf_reflect_udata_newindex(VM& vm, const Value* a, std::size_t n) {
  if (n < 3 || a[0].tag != Tag::UData || a[1].tag != Tag::Str) {
    throw "Lua: reflected object assignment expects (self, string_key, value)";
  }
  const UData& ud = vm.H.udata[a[0].u.id];
  Value setters = vm.H.rawget(ud.mt, Value::string(vm.s__ct_setters));
  if (setters.tag != Tag::Table) throw "Lua: reflected type is not writable";
  Value setter = vm.H.rawget(setters.t, a[1]);
  if (setter.is_nil()) throw "Lua: unknown reflected field";
  std::array<Value, 2> args{a[0], a[2]};
  (void)vm.call_value(setter, args.data(), 2);
  return Multi::none();
}

constexpr Multi VM::nf_reflect_udata_tostring(VM& vm, const Value* a, std::size_t n) {
  if (n < 1 || a[0].tag != Tag::UData) throw "Lua: tostring expects reflected object";
  return Multi::one(Value::string(vm.H.udata[a[0].u.id].type_name));
}

constexpr Multi VM::nf_reflect_udata_pairs(VM& vm, const Value* a, std::size_t n) {
  if (n < 1 || a[0].tag != Tag::UData) throw "Lua: pairs expects reflected object";
  Multi m{};
  m.n = 3;
  m.v[0] = vm.mk_native(vm.id_reflect_udata_pairs_iter);
  m.v[1] = Value::table(vm.H.udata[a[0].u.id].state);
  m.v[2] = Value::nil();
  return m;
}

constexpr Multi VM::nf_reflect_udata_pairs_iter(VM& vm, const Value* a, std::size_t n) {
  return VM::nf_next(vm, a, n);
}

consteval void VM::open_api_support() {
  id_reflect_udata_index = reg_native("api._udata_index", &VM::nf_reflect_udata_index);
  id_reflect_udata_newindex = reg_native("api._udata_newindex", &VM::nf_reflect_udata_newindex);
  id_reflect_udata_tostring = reg_native("api._udata_tostring", &VM::nf_reflect_udata_tostring);
  id_reflect_udata_pairs = reg_native("api._udata_pairs", &VM::nf_reflect_udata_pairs);
  id_reflect_udata_pairs_iter = reg_native("api._udata_pairs_iter", &VM::nf_reflect_udata_pairs_iter);
}

} // namespace ct_lua54
