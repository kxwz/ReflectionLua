#pragma once

namespace ct_lua54 {

constexpr Multi VM::nf_table_insert(VM& vm, const Value* a, std::size_t n) {
  if (n<2 || a[0].tag!=Tag::Table || n>3) throw "Lua: table.insert(list [,pos], value)";
  TableId t=a[0].t;
  std::int64_t len=to_int(vm.v_len(Value::table(t)));
  std::int64_t pos=(n==2)? (len+1) : to_int(a[1]);
  Value val=(n==2)? a[1] : a[2];
  if (pos<1 || pos>len+1) throw "Lua: table.insert position out of bounds";
  for (std::int64_t i=len; i>=pos; --i) {
    Value cur=vm.table_get(t, Value::integer(i));
    vm.table_set(t, Value::integer(i+1), cur);
  }
  vm.table_set(t, Value::integer(pos), val);
  return Multi::none();
}

constexpr Multi VM::nf_table_remove(VM& vm, const Value* a, std::size_t n) {
  if (n<1 || a[0].tag!=Tag::Table || n>2) throw "Lua: table.remove(list [,pos])";
  TableId t=a[0].t;
  std::int64_t len=to_int(vm.v_len(Value::table(t)));
  if (len<=0) return Multi::one(Value::nil());
  std::int64_t pos=(n==1)? len : to_int(a[1]);
  if (pos<1 || pos>len) throw "Lua: table.remove position out of bounds";
  Value out=vm.table_get(t, Value::integer(pos));
  for (std::int64_t i=pos; i<len; ++i) {
    vm.table_set(t, Value::integer(i), vm.table_get(t, Value::integer(i+1)));
  }
  vm.table_set(t, Value::integer(len), Value::nil());
  return Multi::one(out);
}

constexpr Multi VM::nf_table_sort(VM& vm, const Value* a, std::size_t n) {
  if (n<1 || a[0].tag!=Tag::Table || n>2) throw "Lua: table.sort(list [,comp])";
  TableId t=a[0].t;
  Value comp=(n>=2)? a[1] : Value::nil();
  std::int64_t len=to_int(vm.v_len(Value::table(t)));
  auto less = [&](const Value& x, const Value& y) constexpr -> bool {
    if (!comp.is_nil()) {
      vm.tmp_args[0]=x; vm.tmp_args[1]=y;
      return truthy(vm.first(vm.call_value(comp, vm.tmp_args.data(), 2)));
    }
    return vm.v_lt(x,y);
  };
  for (std::int64_t i=2; i<=len; ++i) {
    Value key=vm.table_get(t, Value::integer(i));
    std::int64_t j=i-1;
    while (j>=1) {
      Value cur=vm.table_get(t, Value::integer(j));
      if (!less(key,cur)) break;
      vm.table_set(t, Value::integer(j+1), cur);
      --j;
    }
    vm.table_set(t, Value::integer(j+1), key);
  }
  return Multi::none();
}

constexpr Multi VM::nf_table_concat(VM& vm, const Value* a, std::size_t n) {
  if (n<1 || a[0].tag!=Tag::Table || n>4) throw "Lua: table.concat(list [,sep [,i [,j]]])";
  TableId t=a[0].t;

  std::string_view sep{};
  if (n>=2) {
    if (a[1].tag==Tag::Str) sep=vm.H.sp.view(a[1].s);
    else if (a[1].tag==Tag::Int) sep=vm.H.sp.view(vm.int_to_string(a[1].i));
    else if (a[1].tag==Tag::Num) sep=vm.H.sp.view(vm.num_to_string(a[1].n));
    else throw "Lua: bad separator for table.concat";
  }

  std::int64_t i=(n>=3)? to_int(a[2]) : 1;
  std::int64_t j=(n>=4)? to_int(a[3]) : to_int(vm.v_len(Value::table(t)));
  if (j<i) return Multi::one(Value::string(vm.H.sp.intern("")));

  std::array<char, 8192> buf{};
  std::size_t w=0;
  auto append = [&](std::string_view s) constexpr {
    if (w + s.size() > buf.size()) throw "Lua: table.concat result too long";
    for (char c: s) buf[w++]=c;
  };
  auto to_sv = [&](const Value& v) constexpr -> std::string_view {
    if (v.tag==Tag::Str) return vm.H.sp.view(v.s);
    if (v.tag==Tag::Int) return vm.H.sp.view(vm.int_to_string(v.i));
    if (v.tag==Tag::Num) return vm.H.sp.view(vm.num_to_string(v.n));
    throw "Lua: invalid value in table.concat";
  };

  for (std::int64_t k=i; k<=j; ++k) {
    if (k!=i) append(sep);
    append(to_sv(vm.table_get(t, Value::integer(k))));
  }
  return Multi::one(Value::string(vm.H.sp.intern(std::string_view(buf.data(), w))));
}

constexpr Multi VM::nf_table_pack(VM& vm, const Value* a, std::size_t n) {
  TableId t=vm.H.new_table_pow2(7);
  for (std::size_t i=0;i<n;++i) vm.table_set(t, Value::integer((std::int64_t)i+1), a[i]);
  vm.table_set(t, Value::string(vm.H.sp.intern("n")), Value::integer((std::int64_t)n));
  return Multi::one(Value::table(t));
}

constexpr Multi VM::nf_table_unpack(VM& vm, const Value* a, std::size_t n) {
  if (n<1 || a[0].tag!=Tag::Table || n>3) throw "Lua: table.unpack(list [,i [,j]])";
  TableId t=a[0].t;
  std::int64_t i=(n>=2)? to_int(a[1]) : 1;
  std::int64_t j=(n>=3)? to_int(a[2]) : to_int(vm.v_len(Value::table(t)));
  if (j<i) return Multi::none();
  Multi out{};
  std::uint8_t w=0;
  for (std::int64_t k=i; k<=j && w<MAX_RET; ++k) out.v[w++]=vm.table_get(t, Value::integer(k));
  out.n=w;
  return out;
}

constexpr Multi VM::nf_table_move(VM& vm, const Value* a, std::size_t n) {
  if (n<4 || n>5 || a[0].tag!=Tag::Table) throw "Lua: table.move(a1, f, e, t [,a2])";
  TableId src=a[0].t;
  std::int64_t f=to_int(a[1]);
  std::int64_t e=to_int(a[2]);
  std::int64_t tpos=to_int(a[3]);
  TableId dst=src;
  if (n==5) {
    if (a[4].tag!=Tag::Table) throw "Lua: table.move target must be table";
    dst=a[4].t;
  }
  if (e>=f) {
    std::int64_t cnt=e-f+1;
    if (dst.id==src.id && tpos>f && tpos<=e) {
      for (std::int64_t i=cnt-1; i>=0; --i) {
        vm.table_set(dst, Value::integer(tpos+i), vm.table_get(src, Value::integer(f+i)));
      }
    } else {
      for (std::int64_t i=0; i<cnt; ++i) {
        vm.table_set(dst, Value::integer(tpos+i), vm.table_get(src, Value::integer(f+i)));
      }
    }
  }
  return Multi::one(Value::table(dst));
}

consteval void VM::open_table() {
  TableId tmod=H.new_table_pow2(6);

  auto reg = [&](std::string_view name, NativeFn fn) {
    std::uint32_t id=reg_native(name, fn);
    table_set(tmod, Value::string(H.sp.intern(name)), mk_native(id));
  };

  reg("insert", &VM::nf_table_insert);
  reg("remove", &VM::nf_table_remove);
  reg("sort",   &VM::nf_table_sort);
  reg("concat", &VM::nf_table_concat);
  reg("pack",   &VM::nf_table_pack);
  reg("unpack", &VM::nf_table_unpack);
  reg("move",   &VM::nf_table_move);

  table_set(G, Value::string(H.sp.intern("table")), Value::table(tmod));
}

} // namespace ct_lua54
