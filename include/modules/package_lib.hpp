#pragma once

namespace ct_lua54 {

namespace package_detail {
  static constexpr void append_char(std::array<char, 1024>& buf, std::size_t& n, char c) {
    if (n < buf.size()) buf[n++] = c;
  }

  static constexpr void append_sv(std::array<char, 1024>& buf, std::size_t& n, std::string_view s) {
    for (char c: s) append_char(buf, n, c);
  }

  static constexpr StrId module_message(VM& vm, std::string_view prefix, std::string_view name, std::string_view suffix = {}) {
    std::array<char, 1024> buf{};
    std::size_t n=0;
    append_sv(buf, n, prefix);
    append_sv(buf, n, name);
    append_sv(buf, n, suffix);
    return vm.H.sp.intern(std::string_view(buf.data(), n));
  }

  static constexpr const char* module_not_found(VM& vm, StrId name, std::string_view detail) {
    std::array<char, 1024> buf{};
    std::size_t n=0;
    append_sv(buf, n, "Lua: module '");
    append_sv(buf, n, vm.H.sp.view(name));
    append_sv(buf, n, "' not found");
    append_sv(buf, n, detail);
    return vm.H.sp.c_str(vm.H.sp.intern(std::string_view(buf.data(), n)));
  }

  static constexpr const char* recursive_require(VM& vm, StrId name) {
    return vm.H.sp.c_str(module_message(vm, "Lua: recursive require for module '", vm.H.sp.view(name), "'"));
  }
}

constexpr Multi VM::nf_package_searcher_preload(VM& vm, const Value* a, std::size_t n) {
  if (n<1 || a[0].tag!=Tag::Str) throw "Lua: package preload searcher expects module name";
  TableId preload = vm.package_preload_table();
  Value loader = vm.H.rawget(preload, a[0]);
  if (!loader.is_nil()) {
    Multi out{}; out.n=2;
    out.v[0]=loader;
    out.v[1]=Value::string(vm.H.sp.intern(":preload:"));
    return out;
  }
  return Multi::one(Value::string(package_detail::module_message(
    vm, "\n\tno field package.preload['", vm.H.sp.view(a[0].s), "']")));
}

constexpr Multi VM::nf_package_searcher_embedded(VM& vm, const Value* a, std::size_t n) {
  if (n<1 || a[0].tag!=Tag::Str) throw "Lua: embedded searcher expects module name";
  StrId source{};
  if (!vm.find_embedded_module_source(a[0].s, source)) {
    return Multi::one(Value::string(package_detail::module_message(
      vm, "\n\tno embedded module '", vm.H.sp.view(a[0].s), "'")));
  }
  Value loader = vm.compile_chunk(vm.H.sp.view(source), vm.G);
  Multi out{}; out.n=2;
  out.v[0]=loader;
  out.v[1]=a[0];
  return out;
}

constexpr Multi VM::nf_require(VM& vm, const Value* a, std::size_t n) {
  if (n<1 || a[0].tag!=Tag::Str) throw "Lua: require(name)";

  TableId loaded = vm.package_loaded_table();
  Value cur = vm.H.rawget(loaded, a[0]);
  if (!cur.is_nil()) {
    if (vm.is_package_loading_sentinel(cur)) throw package_detail::recursive_require(vm, a[0].s);
    return Multi::one(cur);
  }

  TableId searchers = vm.package_searchers_table();
  Value loader = Value::nil();
  Value extra = Value::nil();
  std::array<char, 1024> errs{};
  std::size_t err_n=0;

  for (std::int64_t i=1;;++i) {
    Value searcher = vm.H.rawget(searchers, Value::integer(i));
    if (searcher.is_nil()) break;
    std::array<Value, 1> args{a[0]};
    Multi result = vm.call_value(searcher, args.data(), 1);
    if (vm.protected_depth && vm.pending_error) return Multi::none();

    Value head = result.n ? result.v[0] : Value::nil();
    if (head.tag == Tag::Func) {
      loader = head;
      extra = (result.n > 1) ? result.v[1] : Value::nil();
      break;
    }
    if (head.tag == Tag::Str) {
      package_detail::append_sv(errs, err_n, vm.H.sp.view(head.s));
    }
  }

  if (loader.is_nil()) {
    throw package_detail::module_not_found(vm, a[0].s, std::string_view(errs.data(), err_n));
  }

  vm.H.rawset(loaded, a[0], Value::table(vm.package_loading_sentinel));
  try {
    std::array<Value, 2> loader_args{};
    std::size_t argc=1;
    loader_args[0]=a[0];
    if (!extra.is_nil()) loader_args[argc++]=extra;

    Multi loaded_result = vm.call_value(loader, loader_args.data(), argc);
    if (vm.protected_depth && vm.pending_error) {
      vm.H.rawset(loaded, a[0], Value::nil());
      return Multi::none();
    }

    Value final_value = vm.H.rawget(loaded, a[0]);
    if (vm.is_package_loading_sentinel(final_value) || final_value.is_nil()) {
      final_value = loaded_result.n ? loaded_result.v[0] : Value::nil();
      if (final_value.is_nil()) final_value = Value::boolean(true);
      vm.H.rawset(loaded, a[0], final_value);
    }
    return Multi::one(final_value);
  } catch (...) {
    vm.H.rawset(loaded, a[0], Value::nil());
    throw;
  }
}

consteval void VM::open_package() {
  package_table = H.new_table_pow2(table_pow2_for(8));
  TableId loaded = H.new_table_pow2(table_pow2_for(8));
  TableId preload = H.new_table_pow2(table_pow2_for(8));
  TableId searchers = H.new_table_pow2(table_pow2_for(4));
  package_loading_sentinel = H.new_table_pow2(2);

  id_package_searcher_preload = reg_native("package.searcher.preload", &VM::nf_package_searcher_preload);
  id_package_searcher_embedded = reg_native("package.searcher.embedded", &VM::nf_package_searcher_embedded);

  auto id_require = reg_native("require", &VM::nf_require);

  H.rawset(searchers, Value::integer(1), mk_native(id_package_searcher_preload));
  H.rawset(searchers, Value::integer(2), mk_native(id_package_searcher_embedded));

  H.rawset(package_table, Value::string(s_loaded), Value::table(loaded));
  H.rawset(package_table, Value::string(s_preload), Value::table(preload));
  H.rawset(package_table, Value::string(s_searchers), Value::table(searchers));

  H.rawset(loaded, Value::string(H.sp.intern("package")), Value::table(package_table));

  H.rawset(G, Value::string(H.sp.intern("package")), Value::table(package_table));
  H.rawset(G, Value::string(H.sp.intern("require")), mk_native(id_require));
}

} // namespace ct_lua54
