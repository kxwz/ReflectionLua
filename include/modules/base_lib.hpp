#pragma once

namespace ct_lua54 {

constexpr Multi VM::nf_print(VM& vm, const Value* a, std::size_t n) {
  // Compile-time engine: capture prints into VM buffer.
  for (std::size_t i=0;i<n;++i) {
    if (i) vm.print_append_char('\t');
    StrId s=vm.value_tostring(a[i]);
    vm.print_append_sv(vm.H.sp.view(s));
  }
  vm.print_append_char('\n');
  return Multi::none();
}

constexpr Multi VM::nf_tostring(VM& vm, const Value* a, std::size_t n) {
  Value v = (n==0) ? Value::nil() : a[0];
  return Multi::one(Value::string(vm.value_tostring(v)));
}

constexpr Multi VM::nf_type(VM& vm, const Value* a, std::size_t n) {
  auto t = (n==0) ? Tag::Nil : a[0].tag;
  std::string_view s =
    t==Tag::Nil ? "nil" :
    t==Tag::Bool? "boolean" :
    (t==Tag::Int||t==Tag::Num) ? "number" :
    t==Tag::Str ? "string" :
    t==Tag::Table ? "table" :
    t==Tag::Func ? "function" : "userdata";
  return Multi::one(Value::string(vm.H.sp.intern(s)));
}

constexpr Multi VM::nf_setmetatable(VM& vm, const Value* a, std::size_t n) {
  if (n<2 || a[0].tag!=Tag::Table || (a[1].tag!=Tag::Table && a[1].tag!=Tag::Nil)) throw "Lua: setmetatable(t,mt)";
  vm.H.tables[a[0].t.id].mt = (a[1].tag==Tag::Table)? a[1].t : TableId{0};
  return Multi::one(a[0]);
}

constexpr Multi VM::nf_getmetatable(VM& vm, const Value* a, std::size_t n) {
  if (n<1) return Multi::one(Value::nil());
  TableId mt = vm.metatable_of(a[0]);
  return mt.id? Multi::one(Value::table(mt)) : Multi::one(Value::nil());
}

constexpr Multi VM::nf_rawget(VM& vm, const Value* a, std::size_t n) {
  if (n<2 || a[0].tag!=Tag::Table) throw "Lua: rawget(t,k)";
  return Multi::one(vm.H.rawget(a[0].t, a[1]));
}

constexpr Multi VM::nf_rawset(VM& vm, const Value* a, std::size_t n) {
  if (n<3 || a[0].tag!=Tag::Table) throw "Lua: rawset(t,k,v)";
  vm.H.rawset(a[0].t, a[1], a[2]);
  return Multi::one(a[0]);
}

constexpr Multi VM::nf_next(VM& vm, const Value* a, std::size_t n) {
  if (n<1 || a[0].tag!=Tag::Table) throw "Lua: next(t[,k])";
  TableId t=a[0].t;
  const TableObj& T=vm.H.tables[t.id];
  const Entry* es=vm.H.ent(t);
  bool want_next = (n<2 || a[1].tag==Tag::Nil);
  for (std::uint32_t i=0;i<T.cap;++i) {
    const Entry& e=es[i];
    if (!e.used) continue;
    if (want_next) { Multi m; m.n=2; m.v[0]=e.k; m.v[1]=e.v; return m; }
    if (Heap::key_eq(e.k,a[1])) want_next=true;
  }
  return Multi::one(Value::nil());
}

constexpr Multi VM::nf_pairs(VM& vm, const Value* a, std::size_t n) {
  if (n<1) throw "Lua: pairs(t)";
  Value mm=vm.rawget_mt(vm.metatable_of(a[0]), vm.s__pairs);
  if (!mm.is_nil()) {
    vm.tmp_args[0]=a[0];
    Multi r=vm.call_value(mm, vm.tmp_args.data(), 1);
    Multi out{}; out.n=3;
    out.v[0]=(r.n>0)? r.v[0] : Value::nil();
    out.v[1]=(r.n>1)? r.v[1] : Value::nil();
    out.v[2]=(r.n>2)? r.v[2] : Value::nil();
    return out;
  }
  if (a[0].tag!=Tag::Table) throw "Lua: pairs(t)";
  Multi m; m.n=3;
  m.v[0]=vm.mk_native(vm.id_next);
  m.v[1]=a[0];
  m.v[2]=Value::nil();
  return m;
}

consteval void VM::open_base() {
  auto id_print=reg_native("print",&VM::nf_print);
  table_set(G,Value::string(H.sp.intern("print")),mk_native(id_print));

  auto id_tostring=reg_native("tostring",&VM::nf_tostring);
  table_set(G,Value::string(H.sp.intern("tostring")),mk_native(id_tostring));

  auto id_type=reg_native("type",&VM::nf_type);
  table_set(G,Value::string(H.sp.intern("type")),mk_native(id_type));

  auto id_setmt=reg_native("setmetatable",&VM::nf_setmetatable);
  table_set(G,Value::string(H.sp.intern("setmetatable")),mk_native(id_setmt));

  auto id_getmt=reg_native("getmetatable",&VM::nf_getmetatable);
  table_set(G,Value::string(H.sp.intern("getmetatable")),mk_native(id_getmt));

  auto id_rawget=reg_native("rawget",&VM::nf_rawget);
  table_set(G,Value::string(H.sp.intern("rawget")),mk_native(id_rawget));

  auto id_rawset=reg_native("rawset",&VM::nf_rawset);
  table_set(G,Value::string(H.sp.intern("rawset")),mk_native(id_rawset));

  id_next=reg_native("next",&VM::nf_next);
  table_set(G,Value::string(H.sp.intern("next")),mk_native(id_next));

  auto id_pairs=reg_native("pairs",&VM::nf_pairs);
  table_set(G,Value::string(H.sp.intern("pairs")),mk_native(id_pairs));
}

} // namespace ct_lua54
