#pragma once

namespace ct_lua54 {

namespace base_detail {
  static constexpr bool is_space(char c){
    return c==' ' || c=='\t' || c=='\r' || c=='\n' || c=='\f' || c=='\v';
  }

  static constexpr std::string_view trim(std::string_view s){
    std::size_t b=0, e=s.size();
    while (b<e && is_space(s[b])) ++b;
    while (e>b && is_space(s[e-1])) --e;
    return s.substr(b,e-b);
  }

  static constexpr bool token_is_int_literal(std::string_view t){
    bool is_hex = t.size()>=2 && t[0]=='0' && (t[1]=='x'||t[1]=='X');
    for (char ch: t) {
      if (ch=='.') return false;
      if (!is_hex && (ch=='e'||ch=='E')) return false;
      if (is_hex && (ch=='p'||ch=='P')) return false;
    }
    return true;
  }

  static constexpr bool parse_lua_number(std::string_view in, Value& out){
    auto s=trim(in);
    if (s.empty()) return false;

    bool neg=false;
    if (s[0]=='+' || s[0]=='-') {
      neg=(s[0]=='-');
      s=s.substr(1);
      if (s.empty()) return false;
    }

    Lexer L{s};
    Tok t=L.next();
    if (t.k!=TK::Number) return false;
    Tok e=L.next();
    if (e.k!=TK::End) return false;

    if (token_is_int_literal(t.t)) {
      std::int64_t iv=t.i;
      out=Value::integer(neg? -iv : iv);
      return true;
    }

    double nv=t.num;
    out=Value::number(neg? -nv : nv);
    return true;
  }

  static constexpr int digit_val(char c){
    if (c>='0'&&c<='9') return c-'0';
    if (c>='a'&&c<='z') return 10 + (c-'a');
    if (c>='A'&&c<='Z') return 10 + (c-'A');
    return -1;
  }

  static constexpr bool parse_base_int(std::string_view in, std::int64_t base, std::int64_t& out){
    auto s=trim(in);
    if (s.empty()) return false;

    std::size_t p=0;
    bool neg=false;
    if (s[p]=='+' || s[p]=='-') { neg=(s[p]=='-'); ++p; }

    if (base==16 && p+1<s.size() && s[p]=='0' && (s[p+1]=='x' || s[p+1]=='X')) p+=2;

    std::uint64_t v=0;
    bool any=false;
    for (; p<s.size(); ++p) {
      int d=digit_val(s[p]);
      if (d<0 || d>=base) break;
      any=true;
      std::uint64_t ub=(std::uint64_t)base;
      std::uint64_t ud=(std::uint64_t)d;
      if (v > (18446744073709551615ull - ud)/ub) return false;
      v = v*ub + ud;
    }
    if (!any) return false;
    while (p<s.size() && is_space(s[p])) ++p;
    if (p!=s.size()) return false;

    if (!neg) {
      if (v>9223372036854775807ull) return false;
      out=(std::int64_t)v;
      return true;
    }
    if (v>9223372036854775808ull) return false;
    if (v==9223372036854775808ull) out=(-9223372036854775807ll-1ll);
    else out=-(std::int64_t)v;
    return true;
  }

  static constexpr Value err_value(VM& vm, const char* msg){
    return Value::string(vm.H.sp.intern((msg && *msg)? msg : "Lua: error"));
  }
}

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

constexpr Multi VM::nf_assert(VM&, const Value* a, std::size_t n) {
  if (n<1 || !truthy(a[0])) throw "Lua: assertion failed!";
  Multi m{};
  m.n=(std::uint8_t)((n>MAX_RET)?MAX_RET:n);
  for (std::uint8_t i=0;i<m.n;++i) m.v[i]=a[i];
  return m;
}

constexpr Multi VM::nf_error(VM& vm, const Value* a, std::size_t n) {
  if (n>=1) {
    StrId sid=vm.value_tostring(a[0]);
    throw vm.H.sp.c_str(sid);
  }
  throw "Lua: error";
}

constexpr Multi VM::nf_pcall(VM& vm, const Value* a, std::size_t n) {
  if (n<1) throw "Lua: pcall(f, ...)";
  Value fn=a[0];
  std::array<Value, MAX_ARGS> args{};
  std::size_t argc=(n>1)?(n-1):0;
  if (argc>MAX_ARGS) argc=MAX_ARGS;
  for (std::size_t i=0;i<argc;++i) args[i]=a[i+1];
  try {
    Multi r=vm.call_value(fn, argc?args.data():nullptr, argc);
    Multi out{};
    std::size_t wn=1u + (std::size_t)r.n;
    if (wn>MAX_RET) wn=MAX_RET;
    out.n=(std::uint8_t)wn;
    out.v[0]=Value::boolean(true);
    for (std::size_t i=1;i<wn;++i) out.v[(std::uint8_t)i]=r.v[(std::uint8_t)(i-1)];
    return out;
  } catch (const char* msg) {
    Multi out{}; out.n=2;
    out.v[0]=Value::boolean(false);
    out.v[1]=base_detail::err_value(vm,msg);
    return out;
  } catch (...) {
    Multi out{}; out.n=2;
    out.v[0]=Value::boolean(false);
    out.v[1]=Value::string(vm.H.sp.intern("Lua: error"));
    return out;
  }
}

constexpr Multi VM::nf_xpcall(VM& vm, const Value* a, std::size_t n) {
  if (n<2) throw "Lua: xpcall(f, msgh, ...)";
  Value fn=a[0];
  Value msgh=a[1];
  std::array<Value, MAX_ARGS> args{};
  std::size_t argc=(n>2)?(n-2):0;
  if (argc>MAX_ARGS) argc=MAX_ARGS;
  for (std::size_t i=0;i<argc;++i) args[i]=a[i+2];
  try {
    Multi r=vm.call_value(fn, argc?args.data():nullptr, argc);
    Multi out{};
    std::size_t wn=1u + (std::size_t)r.n;
    if (wn>MAX_RET) wn=MAX_RET;
    out.n=(std::uint8_t)wn;
    out.v[0]=Value::boolean(true);
    for (std::size_t i=1;i<wn;++i) out.v[(std::uint8_t)i]=r.v[(std::uint8_t)(i-1)];
    return out;
  } catch (const char* msg) {
    Value err=base_detail::err_value(vm,msg);
    Value handled=err;
    try {
      vm.tmp_args[0]=err;
      handled=vm.first(vm.call_value(msgh, vm.tmp_args.data(), 1));
    } catch (const char* msg2) {
      handled=base_detail::err_value(vm,msg2);
    } catch (...) {
      handled=Value::string(vm.H.sp.intern("Lua: error"));
    }
    Multi out{}; out.n=2;
    out.v[0]=Value::boolean(false);
    out.v[1]=handled;
    return out;
  } catch (...) {
    Multi out{}; out.n=2;
    out.v[0]=Value::boolean(false);
    out.v[1]=Value::string(vm.H.sp.intern("Lua: error"));
    return out;
  }
}

constexpr Multi VM::nf_warn(VM& vm, const Value* a, std::size_t n) {
  if (n==0) return Multi::none();
  if (n==1 && a[0].tag==Tag::Str) {
    auto s=vm.H.sp.view(a[0].s);
    if (s=="@off") { vm.warn_enabled=false; return Multi::none(); }
    if (s=="@on")  { vm.warn_enabled=true;  return Multi::none(); }
  }
  if (!vm.warn_enabled) return Multi::none();

  vm.print_append_sv("warning: ");
  for (std::size_t i=0;i<n;++i) {
    if (a[i].tag!=Tag::Str) throw "Lua: warn expects strings";
    vm.print_append_sv(vm.H.sp.view(a[i].s));
  }
  vm.print_append_char('\n');
  return Multi::none();
}

constexpr Multi VM::nf_rawequal(VM&, const Value* a, std::size_t n) {
  if (n<2) throw "Lua: rawequal(v1,v2)";
  return Multi::one(Value::boolean(raw_eq_nometa(a[0],a[1])));
}

constexpr Multi VM::nf_rawlen(VM& vm, const Value* a, std::size_t n) {
  if (n<1) throw "Lua: rawlen(v)";
  if (a[0].tag==Tag::Str) return Multi::one(Value::integer((std::int64_t)vm.H.sp.view(a[0].s).size()));
  if (a[0].tag==Tag::Table) return Multi::one(Value::integer(vm.raw_len_table(a[0].t)));
  throw "Lua: rawlen expects string/table";
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

constexpr Multi VM::nf_ipairs_iter(VM& vm, const Value* a, std::size_t n) {
  if (n<1 || a[0].tag!=Tag::Table) throw "Lua: ipairs iterator state";
  std::int64_t i=0;
  if (n>=2 && !a[1].is_nil()) i=to_int(a[1]);
  std::int64_t next=i+1;
  Value v=vm.H.rawget(a[0].t, Value::integer(next));
  if (v.is_nil()) return Multi::one(Value::nil());
  Multi m{}; m.n=2;
  m.v[0]=Value::integer(next);
  m.v[1]=v;
  return m;
}

constexpr Multi VM::nf_ipairs(VM& vm, const Value* a, std::size_t n) {
  if (n<1 || a[0].tag!=Tag::Table) throw "Lua: ipairs(t)";
  Multi m{}; m.n=3;
  m.v[0]=vm.mk_native(vm.id_ipairs_iter);
  m.v[1]=a[0];
  m.v[2]=Value::integer(0);
  return m;
}

constexpr Multi VM::nf_select(VM& vm, const Value* a, std::size_t n) {
  if (n<1) throw "Lua: select(index, ...)";
  if (a[0].tag==Tag::Str && vm.H.sp.view(a[0].s)=="#") {
    return Multi::one(Value::integer((std::int64_t)(n-1)));
  }

  std::int64_t idx=to_int(a[0]);
  if (idx==0) throw "Lua: bad argument #1 to 'select' (index out of range)";

  std::int64_t total=(std::int64_t)(n-1);
  if (idx<0) idx=total+idx+1;
  if (idx<1) idx=1;
  if (idx>total) return Multi::none();

  Multi m{};
  std::size_t start=(std::size_t)idx;
  m.n=(std::uint8_t)(((n-start)>MAX_RET)?MAX_RET:(n-start));
  for (std::uint8_t i=0;i<m.n;++i) m.v[i]=a[start+i];
  return m;
}

constexpr Multi VM::nf_tonumber(VM& vm, const Value* a, std::size_t n) {
  if (n==0) return Multi::one(Value::nil());

  if (n==1) {
    if (a[0].tag==Tag::Int || a[0].tag==Tag::Num) return Multi::one(a[0]);
    if (a[0].tag!=Tag::Str) return Multi::one(Value::nil());
    Value out{};
    if (!base_detail::parse_lua_number(vm.H.sp.view(a[0].s), out)) return Multi::one(Value::nil());
    return Multi::one(out);
  }

  std::int64_t base=to_int(a[1]);
  if (base<2 || base>36) throw "Lua: base out of range";
  if (a[0].tag!=Tag::Str) return Multi::one(Value::nil());

  std::int64_t iv=0;
  if (!base_detail::parse_base_int(vm.H.sp.view(a[0].s), base, iv)) return Multi::one(Value::nil());
  return Multi::one(Value::integer(iv));
}

consteval void VM::open_base() {
  id_ipairs_iter=reg_native("ipairs._iter",&VM::nf_ipairs_iter);

  auto id_print=reg_native("print",&VM::nf_print);
  table_set(G,Value::string(H.sp.intern("print")),mk_native(id_print));

  auto id_assert=reg_native("assert",&VM::nf_assert);
  table_set(G,Value::string(H.sp.intern("assert")),mk_native(id_assert));

  auto id_error=reg_native("error",&VM::nf_error);
  table_set(G,Value::string(H.sp.intern("error")),mk_native(id_error));

  auto id_pcall=reg_native("pcall",&VM::nf_pcall);
  table_set(G,Value::string(H.sp.intern("pcall")),mk_native(id_pcall));

  auto id_xpcall=reg_native("xpcall",&VM::nf_xpcall);
  table_set(G,Value::string(H.sp.intern("xpcall")),mk_native(id_xpcall));

  auto id_warn=reg_native("warn",&VM::nf_warn);
  table_set(G,Value::string(H.sp.intern("warn")),mk_native(id_warn));

  auto id_rawequal=reg_native("rawequal",&VM::nf_rawequal);
  table_set(G,Value::string(H.sp.intern("rawequal")),mk_native(id_rawequal));

  auto id_rawlen=reg_native("rawlen",&VM::nf_rawlen);
  table_set(G,Value::string(H.sp.intern("rawlen")),mk_native(id_rawlen));

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

  auto id_ipairs=reg_native("ipairs",&VM::nf_ipairs);
  table_set(G,Value::string(H.sp.intern("ipairs")),mk_native(id_ipairs));

  auto id_select=reg_native("select",&VM::nf_select);
  table_set(G,Value::string(H.sp.intern("select")),mk_native(id_select));

  auto id_tonumber=reg_native("tonumber",&VM::nf_tonumber);
  table_set(G,Value::string(H.sp.intern("tonumber")),mk_native(id_tonumber));
}

} // namespace ct_lua54
