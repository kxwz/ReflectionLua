#pragma once

template <class T>
consteval bool native_value_type_supported() {
  using U = std::remove_cvref_t<T>;
  if constexpr (std::same_as<U, Value> || std::same_as<U, bool> || std::same_as<U, std::string_view>) return true;
  else if constexpr (std::integral<U> && !std::same_as<U, bool>) return true;
  else if constexpr (std::floating_point<U>) return true;
  else if constexpr (std::is_enum_v<U>) return meta::has_identifier(^^U);
  else if constexpr (std::is_class_v<U>) return meta::has_identifier(^^U);
  else return false;
}

template <class T>
consteval bool native_arg_type_supported() {
  if constexpr (std::is_reference_v<T>) {
    if constexpr (std::is_lvalue_reference_v<T> && !std::is_const_v<std::remove_reference_t<T>>) return false;
  }
  return native_value_type_supported<std::remove_cvref_t<T>>();
}

template <class T>
consteval bool native_ret_type_supported() {
  return std::same_as<std::remove_cvref_t<T>, void> || native_value_type_supported<std::remove_cvref_t<T>>();
}

template <class T>
consteval bool reflected_field_type_supported() {
  if constexpr (std::is_reference_v<T>) return false;
  using U = std::remove_cvref_t<T>;
  if constexpr (std::same_as<U, bool> || std::same_as<U, std::string_view>) return true;
  else if constexpr (std::integral<U> && !std::same_as<U, bool>) return true;
  else if constexpr (std::floating_point<U>) return true;
  else if constexpr (std::is_enum_v<U>) return meta::has_identifier(^^U);
  else return false;
}

// ---------------- VM ----------------
struct VM {
  Heap H{};
  Arena A{};

  std::array<NativeFn, 4096> natives{};
  std::array<StrId,   4096> native_names{};
  std::uint32_t native_count{0};

  TableId G{0};

  // metanames
  StrId s__index{}, s__newindex{}, s__call{}, s__add{}, s__sub{}, s__mul{}, s__div{}, s__idiv{}, s__mod{}, s__pow{};
  StrId s__unm{}, s__len{}, s__concat{}, s__eq{}, s__lt{}, s__le{}, s__pairs{}, s__tostring{}, s__metatable{};
  StrId s__band{}, s__bor{}, s__bxor{}, s__shl{}, s__shr{}, s__bnot{};
  StrId s__ct_methods{}, s__ct_setters{}, s_new{};

  std::size_t steps{0};
  constexpr void tick(){ if (++steps > STEP_LIMIT) throw "Lua: step limit exceeded"; }
  std::uint64_t rng_state{0x9E3779B97F4A7C15ull};

  struct VarArgs { const Value* p{nullptr}; std::size_t n{0}; };
  std::array<char, MAX_PRINT_BYTES> print_buf{};
  std::size_t print_n{0};
  bool print_truncated{false};
  bool warn_enabled{true};
  std::uint32_t protected_depth{0};
  const char* pending_error{nullptr};

  std::array<StrId, 256> reflected_type_names{};
  std::array<TableId, 256> reflected_type_mts{};
  std::uint32_t reflected_type_count{0};

  static constexpr bool is_number(const Value& v){ return v.tag==Tag::Int || v.tag==Tag::Num; }
  static constexpr double to_num(const Value& v){
    if (v.tag==Tag::Num) return v.n;
    if (v.tag==Tag::Int) return (double)v.i;
    throw "Lua: expected number";
  }

  static constexpr std::int64_t to_int(const Value& v){
    if (v.tag==Tag::Int) return v.i;
    if (v.tag==Tag::Num) {
      double x=v.n;
      if (!(x==x)) throw "Lua: expected integer";
      if (x < -9223372036854775808.0 || x > 9223372036854775807.0) throw "Lua: expected integer";
      std::int64_t i=(std::int64_t)x;
      if ((double)i != x) throw "Lua: expected integer";
      return i;
    }
    throw "Lua: expected integer";
  }

  static constexpr bool to_int_maybe(const Value& v, std::int64_t& out){
    if (v.tag==Tag::Int) { out=v.i; return true; }
    if (v.tag==Tag::Num) return as_exact_i64(v.n, out);
    return false;
  }

  static constexpr double floor_num(double x){
    std::int64_t i=(std::int64_t)x;
    double d=(double)i;
    if (d>x) d-=1.0;
    return d;
  }

  static constexpr bool as_exact_i64(double x, std::int64_t& out){
    return ct_lua54::as_exact_i64(x, out);
  }

  static constexpr double exp_num(double x){
    if (x==0.0) return 1.0;
    bool neg=false;
    if (x<0.0) { neg=true; x=-x; }

    int k=0;
    while (x>0.5 && k<2048) { x*=0.5; ++k; }

    double term=1.0;
    double sum=1.0;
    for (int n=1;n<60;++n) {
      term *= x / (double)n;
      sum += term;
    }

    while (k-->0) sum*=sum;
    return neg ? (1.0/sum) : sum;
  }

  static constexpr double ln_num(double x){
    if (x<=0.0) throw "Lua: pow domain error";
    constexpr double LN2 = 0.693147180559945309417232121458176568;
    int k=0;
    while (x>2.0) { x*=0.5; ++k; }
    while (x<0.5) { x*=2.0; --k; }
    double y=(x-1.0)/(x+1.0);
    double y2=y*y;
    double term=y;
    double s=0.0;
    for (int n=1;n<=59;n+=2) {
      s += term/(double)n;
      term *= y2;
    }
    return 2.0*s + (double)k*LN2;
  }

  static constexpr double pow_num(double a, double b){
    if (a==0.0) {
      if (b>0.0) return 0.0;
      throw "Lua: pow domain error";
    }

    std::int64_t bi=0;
    if (as_exact_i64(b, bi)) {
      bool neg=bi<0;
      std::uint64_t e = neg ? ((std::uint64_t)(-(bi+1)) + 1u) : (std::uint64_t)bi;
      double base=a;
      double out=1.0;
      while (e) {
        if (e & 1u) out*=base;
        e >>= 1u;
        if (e) base*=base;
      }
      return neg ? (1.0/out) : out;
    }

    if (a<0.0) throw "Lua: pow domain error";
    return exp_num(b * ln_num(a));
  }

  static constexpr std::int64_t floor_div_int(std::int64_t a, std::int64_t b){
    if (b==0) throw "Lua: division by zero";
    std::int64_t q=a/b;
    std::int64_t r=a%b;
    if (r!=0 && ((r>0)!=(b>0))) --q;
    return q;
  }

  static constexpr std::uint64_t as_u64(std::int64_t x){ return (std::uint64_t)x; }
  static constexpr std::int64_t bit_not(std::int64_t x){ return (std::int64_t)(~as_u64(x)); }
  static constexpr std::int64_t bit_and(std::int64_t a, std::int64_t b){ return (std::int64_t)(as_u64(a) & as_u64(b)); }
  static constexpr std::int64_t bit_or (std::int64_t a, std::int64_t b){ return (std::int64_t)(as_u64(a) | as_u64(b)); }
  static constexpr std::int64_t bit_xor(std::int64_t a, std::int64_t b){ return (std::int64_t)(as_u64(a) ^ as_u64(b)); }

  static constexpr std::int64_t shift_left(std::int64_t x, std::int64_t n){
    if (n<0) {
      std::uint64_t sh=(std::uint64_t)(-(n+1)) + 1u;
      if (sh>=64u) return 0;
      return (std::int64_t)(as_u64(x) >> (unsigned)sh);
    }
    if (n>=64) return 0;
    return (std::int64_t)(as_u64(x) << (unsigned)n);
  }
  static constexpr std::int64_t shift_right(std::int64_t x, std::int64_t n){
    if (n<0) {
      std::uint64_t sh=(std::uint64_t)(-(n+1)) + 1u;
      if (sh>=64u) return 0;
      return (std::int64_t)(as_u64(x) << (unsigned)sh);
    }
    if (n>=64) return 0;
    return (std::int64_t)(as_u64(x) >> (unsigned)n);
  }

  constexpr Value first(const Multi& m){ return m.n? m.v[0] : Value::nil(); }

  template <std::size_t N>
  constexpr Multi call_with(const Value& callee, const std::array<Value, N>& args, std::size_t argc=N) {
    return call_value(callee, args.data(), argc);
  }

  template <std::size_t N>
  constexpr Value call_with_first(const Value& callee, const std::array<Value, N>& args, std::size_t argc=N) {
    return first(call_with(callee, args, argc));
  }

  template <std::size_t N>
  constexpr bool call_with_truthy(const Value& callee, const std::array<Value, N>& args, std::size_t argc=N) {
    return truthy(call_with_first(callee, args, argc));
  }

  constexpr void print_append_char(char c){
    if (print_n < print_buf.size()) { print_buf[print_n++]=c; return; }
    print_truncated=true;
  }

  constexpr void print_append_sv(std::string_view s){
    for (char c: s) print_append_char(c);
  }

  constexpr std::string_view print_view() const {
    return std::string_view(print_buf.data(), print_n);
  }

  constexpr TableId metatable_of(const Value& v) {
    if (v.tag==Tag::Table) return H.tables[v.t.id].mt;
    if (v.tag==Tag::UData) return H.udata[v.u.id].mt;
    return H.type_mt[(std::size_t)v.tag];
  }

  constexpr Value rawget_mt(TableId mt, StrId key){
    if (mt.id==0) return Value::nil();
    return H.rawget(mt, Value::string(key));
  }

  constexpr Value table_get(TableId t, const Value& key) {
    Value v=H.rawget(t,key);
    if (!v.is_nil()) return v;
    TableId mt=H.tables[t.id].mt;
    if (mt.id==0) return Value::nil();
    Value idx=rawget_mt(mt,s__index);
    if (idx.is_nil()) return Value::nil();
    if (idx.tag==Tag::Table) return table_get(idx.t,key);
    std::array<Value, 2> args{Value::table(t), key};
    return call_with_first(idx, args);
  }

  constexpr bool raw_has_int_key(TableId t, std::int64_t k) const {
    if (k<=0) return false;
    return !H.rawget(t, Value::integer(k)).is_nil();
  }

  constexpr std::int64_t raw_len_table(TableId t) const {
    // Lua-like border search over raw integer keys (ignores __index).
    if (!raw_has_int_key(t, 1)) return 0;

    constexpr std::uint64_t KMAX = 9223372036854775807ull;
    std::uint64_t i=1;
    std::uint64_t j=2;

    while (j<=KMAX && raw_has_int_key(t, (std::int64_t)j)) {
      i=j;
      if (j > (KMAX/2)) { j=KMAX; break; }
      j*=2;
    }
    if (j==KMAX && raw_has_int_key(t, (std::int64_t)KMAX)) return (std::int64_t)KMAX;

    while (j-i>1) {
      std::uint64_t m=i + ((j-i)>>1);
      if (raw_has_int_key(t, (std::int64_t)m)) i=m;
      else j=m;
    }
    return (std::int64_t)i;
  }

  constexpr void table_set(TableId t, const Value& key, const Value& val) {
    Value cur=H.rawget(t,key);
    TableId mt=H.tables[t.id].mt;
    if (!cur.is_nil() || mt.id==0) { H.rawset(t,key,val); return; }
    Value ni=rawget_mt(mt,s__newindex);
    if (ni.is_nil()) { H.rawset(t,key,val); return; }
    if (ni.tag==Tag::Table) { table_set(ni.t,key,val); return; }
    std::array<Value, 3> args{Value::table(t), key, val};
    (void)call_with(ni, args);
  }

  constexpr Value meta_bin(const Value& a, const Value& b, StrId mm, const char* err){
    Value mma=rawget_mt(metatable_of(a),mm);
    if (!mma.is_nil()) {
      std::array<Value, 2> args{a, b};
      return call_with_first(mma, args);
    }
    Value mmb=rawget_mt(metatable_of(b),mm);
    if (!mmb.is_nil()) {
      std::array<Value, 2> args{a, b};
      return call_with_first(mmb, args);
    }
    throw err;
  }

  constexpr Value meta_un(const Value& a, StrId mm, const char* err){
    Value mmv=rawget_mt(metatable_of(a),mm);
    if (!mmv.is_nil()) {
      std::array<Value, 1> args{a};
      return call_with_first(mmv, args);
    }
    throw err;
  }

  static constexpr bool raw_eq_nometa(const Value& a, const Value& b){
    if (a.tag==Tag::Int || a.tag==Tag::Num || b.tag==Tag::Int || b.tag==Tag::Num) return numeric_eq_exact(a, b);
    if (a.tag!=b.tag) return false;
    switch (a.tag) {
      case Tag::Nil:   return true;
      case Tag::Bool:  return a.b==b.b;
      case Tag::Int:   return false;
      case Tag::Num:   return false;
      case Tag::Str:   return a.s.id==b.s.id;
      case Tag::Table: return a.t.id==b.t.id;
      case Tag::UData: return a.u.id==b.u.id;
      case Tag::Func:  return a.f.id==b.f.id && a.f.is_native==b.f.is_native;
    }
    return false;
  }

  constexpr bool v_eq(const Value& a, const Value& b){
    if (a.tag!=b.tag) return raw_eq_nometa(a,b);
    switch (a.tag) {
      case Tag::Nil: return true;
      case Tag::Bool:return a.b==b.b;
      case Tag::Int: return a.i==b.i;
      case Tag::Num: return a.n==b.n;
      case Tag::Str: return a.s.id==b.s.id;
      case Tag::Func: return a.f.id==b.f.id && a.f.is_native==b.f.is_native;
      case Tag::Table: {
        if (a.t.id==b.t.id) return true;
        Value mma=rawget_mt(metatable_of(a),s__eq);
        Value mmb=rawget_mt(metatable_of(b),s__eq);
        if (mma.is_nil() || mmb.is_nil()) return false;
        if (!raw_eq_nometa(mma,mmb)) return false;
        std::array<Value, 2> args{a, b};
        return call_with_truthy(mma, args);
      }
      case Tag::UData: {
        if (a.u.id==b.u.id) return true;
        Value mma=rawget_mt(metatable_of(a),s__eq);
        Value mmb=rawget_mt(metatable_of(b),s__eq);
        if (mma.is_nil() || mmb.is_nil()) return false;
        if (!raw_eq_nometa(mma,mmb)) return false;
        std::array<Value, 2> args{a, b};
        return call_with_truthy(mma, args);
      }
    }
    return false;
  }

  constexpr bool v_lt(const Value& a, const Value& b){
    if (is_number(a)&&is_number(b)) return to_num(a)<to_num(b);
    if (a.tag==Tag::Str && b.tag==Tag::Str) return H.sp.view(a.s) < H.sp.view(b.s);
    Value mmv=rawget_mt(metatable_of(a),s__lt);
    if (mmv.is_nil()) mmv=rawget_mt(metatable_of(b),s__lt);
    if (!mmv.is_nil()) {
      std::array<Value, 2> args{a, b};
      return call_with_truthy(mmv, args);
    }
    throw "Lua: attempt to compare";
  }

  constexpr bool v_le(const Value& a, const Value& b){
    if (is_number(a)&&is_number(b)) return to_num(a)<=to_num(b);
    if (a.tag==Tag::Str && b.tag==Tag::Str) return !(H.sp.view(b.s) < H.sp.view(a.s));
    Value mmv=rawget_mt(metatable_of(a),s__le);
    if (mmv.is_nil()) mmv=rawget_mt(metatable_of(b),s__le);
    if (!mmv.is_nil()) {
      std::array<Value, 2> args{a, b};
      return call_with_truthy(mmv, args);
    }
    return !v_lt(b,a);
  }

  constexpr StrId int_to_string(std::int64_t x){
    std::array<char, 32> buf{};
    std::size_t w=0;
    std::uint64_t v=0;
    if (x<0) {
      buf[w++]='-';
      v=(std::uint64_t)(-(x+1)) + 1u;
    } else {
      v=(std::uint64_t)x;
    }
    std::array<char, 32> rev{};
    std::size_t p=0;
    do { rev[p++]=char('0'+(v%10u)); v/=10u; } while (v && p<rev.size());
    for (std::size_t i=0;i<p;++i) buf[w++]=rev[p-1-i];
    return H.sp.intern(std::string_view(buf.data(), w));
  }

  constexpr StrId num_to_string(double x){
    std::array<char, 64> buf{};
    std::size_t w=0;
    std::int64_t iv=(std::int64_t)x;
    auto si=int_to_string(iv);
    auto s1=H.sp.view(si);
    for (char c: s1) buf[w++]=c;
    buf[w++]='.';
    double frac=x-(double)iv; if (frac<0) frac=-frac;
    for (int k=0;k<6;++k){ frac*=10.0; int d=(int)frac; frac-=d; buf[w++]=char('0'+d); }
    return H.sp.intern(std::string_view(buf.data(), w));
  }

  constexpr StrId value_tostring(const Value& v){
    Value mmv=rawget_mt(metatable_of(v), s__tostring);
    if (!mmv.is_nil()) {
      std::array<Value, 1> args{v};
      Value rv=call_with_first(mmv, args);
      if (rv.tag!=Tag::Str) throw "Lua: '__tostring' must return a string";
      return rv.s;
    }

    switch (v.tag) {
      case Tag::Nil:   return H.sp.intern("nil");
      case Tag::Bool:  return H.sp.intern(v.b ? "true" : "false");
      case Tag::Int:   return int_to_string(v.i);
      case Tag::Num:   return num_to_string(v.n);
      case Tag::Str:   return v.s;
      case Tag::Table: return H.sp.intern("table");
      case Tag::Func:  return H.sp.intern("function");
      case Tag::UData: return H.udata[v.u.id].type_name;
    }
    throw "Lua: bad value tag";
  }

  constexpr Value v_concat(const Value& a, const Value& b){
    auto to_sv = [&](const Value& x)->std::string_view{
      if (x.tag==Tag::Str) return H.sp.view(x.s);
      if (x.tag==Tag::Int) return H.sp.view(int_to_string(x.i));
      if (x.tag==Tag::Num) return H.sp.view(num_to_string(x.n));
      throw "Lua: concat expects string/number";
    };
    if ((a.tag==Tag::Str||is_number(a)) && (b.tag==Tag::Str||is_number(b))) {
      auto sa=to_sv(a), sb=to_sv(b);
      std::array<char, 2048> tmp{};
      if (sa.size()+sb.size()>tmp.size()) throw "Lua: concat too big";
      std::size_t w=0;
      for (char c: sa) tmp[w++]=c;
      for (char c: sb) tmp[w++]=c;
      return Value::string(H.sp.intern(std::string_view(tmp.data(), w)));
    }
    return meta_bin(a,b,s__concat,"Lua: attempt to concatenate");
  }

  constexpr Value v_len(const Value& a){
    if (a.tag==Tag::Str) return Value::integer((std::int64_t)H.sp.view(a.s).size());
    TableId mt=metatable_of(a);
    Value mmv=rawget_mt(mt,s__len);
    if (!mmv.is_nil()) {
      std::array<Value, 1> args{a};
      return call_with_first(mmv, args);
    }
    if (a.tag==Tag::Table) {
      return Value::integer(raw_len_table(a.t));
    }
    throw "Lua: length of unsupported type";
  }

  // reflection wrapper for api free functions
  template <class> struct fn_traits;
  template <class R, class... Args>
  struct fn_traits<R(Args...)> { using ret=R; using args_tuple=std::tuple<Args...>; static constexpr std::size_t arity=sizeof...(Args); };

  static constexpr std::uint32_t invalid_native_id = UINT32_MAX;

  static constexpr std::uint32_t table_pow2_for(std::size_t min_cap) {
    std::uint32_t pow2 = 4;
    std::uint32_t cap = 16;
    while (cap < min_cap) {
      if (pow2 >= 30) throw "Lua: reflected table too large";
      cap <<= 1u;
      ++pow2;
    }
    return pow2;
  }

  template <class T>
  static consteval std::string_view reflected_type_name_sv() {
    using U = std::remove_cvref_t<T>;
    constexpr auto R = meta::dealias(^^U);
    static_assert(meta::has_identifier(R), "Lua: reflected type must be named");
    return meta::identifier_of(R);
  }

  template <class T>
  constexpr StrId reflected_type_name() {
    return H.sp.intern(reflected_type_name_sv<T>());
  }

  template <class T>
  constexpr void register_reflected_type(TableId mt) {
    StrId name = reflected_type_name<T>();
    for (std::uint32_t i=0;i<reflected_type_count;++i) {
      if (reflected_type_names[i].id == name.id) throw "Lua: duplicate reflected type name";
    }
    if (reflected_type_count >= reflected_type_names.size()) throw "Lua: too many reflected types";
    reflected_type_names[reflected_type_count] = name;
    reflected_type_mts[reflected_type_count] = mt;
    ++reflected_type_count;
  }

  constexpr TableId reflected_type_mt(StrId name) const {
    for (std::uint32_t i=0;i<reflected_type_count;++i) {
      if (reflected_type_names[i].id == name.id) return reflected_type_mts[i];
    }
    return TableId{0};
  }

  template <class T>
  constexpr TableId reflected_type_mt() {
    TableId mt = reflected_type_mt(reflected_type_name<T>());
    if (mt.id==0) throw "Lua: reflected type is not bound";
    return mt;
  }

  template <class T>
  static constexpr T narrow_integer(std::int64_t x) {
    if constexpr (std::same_as<T, std::int64_t>) {
      return x;
    } else if constexpr (std::is_signed_v<T>) {
      if (x < static_cast<std::int64_t>(std::numeric_limits<T>::min()) ||
          x > static_cast<std::int64_t>(std::numeric_limits<T>::max())) {
        throw "Lua: integer out of range";
      }
      return static_cast<T>(x);
    } else {
      if (x < 0) throw "Lua: integer out of range";
      using Lim = std::numeric_limits<T>;
      if (static_cast<std::uint64_t>(x) > static_cast<std::uint64_t>(Lim::max())) {
        throw "Lua: integer out of range";
      }
      return static_cast<T>(x);
    }
  }

  template <class T>
  static constexpr Value value_from_cpp(VM& vm, const T& r) {
    using U = std::remove_cvref_t<T>;
    static_assert(native_value_type_supported<U>(), "Lua: unsupported native value type");
    if constexpr (std::same_as<U, Value>) {
      return r;
    } else if constexpr (std::same_as<U, bool>) {
      return Value::boolean(r);
    } else if constexpr (std::integral<U> && !std::same_as<U, bool>) {
      if constexpr (std::is_unsigned_v<U>) {
        if (static_cast<std::uint64_t>(r) > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())) {
          throw "Lua: integer out of range";
        }
      }
      return Value::integer(static_cast<std::int64_t>(r));
    } else if constexpr (std::floating_point<U>) {
      return Value::number(static_cast<double>(r));
    } else if constexpr (std::same_as<U, std::string_view>) {
      return Value::string(vm.H.sp.intern(r));
    } else if constexpr (std::is_enum_v<U>) {
      using Raw = std::underlying_type_t<U>;
      return value_from_cpp<Raw>(vm, static_cast<Raw>(r));
    } else {
      return make_reflected_object<U>(vm, r);
    }
  }

  template <class T>
  static constexpr std::remove_cvref_t<T> arg_as(VM& vm, const Value& v) {
    using U = std::remove_cvref_t<T>;
    static_assert(native_arg_type_supported<T>(), "Lua: unsupported native arg type");
    if constexpr (std::same_as<U, Value>) {
      return v;
    } else if constexpr (std::same_as<U, bool>) {
      if (v.tag!=Tag::Bool) throw "Lua: expected boolean";
      return v.b;
    } else if constexpr (std::integral<U> && !std::same_as<U, bool>) {
      return narrow_integer<U>(to_int(v));
    } else if constexpr (std::floating_point<U>) {
      return static_cast<U>(to_num(v));
    } else if constexpr (std::same_as<U, std::string_view>) {
      if (v.tag!=Tag::Str) throw "Lua: expected string";
      return vm.H.sp.view(v.s);
    } else if constexpr (std::is_enum_v<U>) {
      using Raw = std::underlying_type_t<U>;
      return static_cast<U>(arg_as<Raw>(vm, v));
    } else {
      return reflected_object_from_value<U>(vm, v);
    }
  }

  template <class R>
  static constexpr Multi ret_as(VM& vm, R r) {
    using U = std::remove_cvref_t<R>;
    static_assert(native_ret_type_supported<R>(), "Lua: unsupported native return");
    if constexpr (std::same_as<U, void>) {
      return Multi::none();
    } else {
      return Multi::one(value_from_cpp<U>(vm, r));
    }
  }

  template <meta::info M>
  static consteval bool bindable_reflected_field_member() {
    return meta::is_nonstatic_data_member(M) && meta::is_public(M) && meta::has_identifier(M);
  }

  template <meta::info M>
  static consteval bool bindable_reflected_method_member() {
    return meta::is_function(M) &&
           meta::is_public(M) &&
           meta::has_identifier(M) &&
           !meta::is_static_member(M) &&
           !meta::is_deleted(M) &&
           !meta::is_operator_function(M) &&
           !meta::is_conversion_function(M);
  }

  template <class T, std::size_t I=0>
  static consteval std::size_t reflected_field_count() {
    constexpr auto ctx = meta::access_context::current();
    static constexpr auto mems = std::define_static_array(meta::members_of(^^T, ctx));
    if constexpr (I >= mems.size()) return 0;
    else return (bindable_reflected_field_member<mems[I]>() ? 1u : 0u) + reflected_field_count<T, I+1>();
  }

  template <class T, std::size_t I=0>
  static consteval std::size_t reflected_method_count() {
    constexpr auto ctx = meta::access_context::current();
    static constexpr auto mems = std::define_static_array(meta::members_of(^^T, ctx));
    if constexpr (I >= mems.size()) return 0;
    else return (bindable_reflected_method_member<mems[I]>() ? 1u : 0u) + reflected_method_count<T, I+1>();
  }

  template <meta::info M, std::size_t I=0>
  static consteval void validate_reflected_method_params() {
    static constexpr auto params = std::define_static_array(meta::parameters_of(M));
    if constexpr (I < params.size()) {
      using P = [:meta::type_of(params[I]):];
      static_assert(native_arg_type_supported<P>(), "Lua: unsupported reflected method parameter");
      validate_reflected_method_params<M, I+1>();
    }
  }

  template <class T, std::size_t I=0>
  static consteval void validate_reflected_type_members() {
    constexpr auto ctx = meta::access_context::current();
    static constexpr auto mems = std::define_static_array(meta::members_of(^^T, ctx));
    if constexpr (I < mems.size()) {
      constexpr auto M = mems[I];
      if constexpr (bindable_reflected_field_member<M>()) {
        using FieldT = std::remove_cvref_t<decltype(std::declval<T&>().[:M:])>;
        static_assert(!meta::is_const(M), "Lua: reflected fields must be writable");
        static_assert(reflected_field_type_supported<FieldT>(), "Lua: unsupported reflected field type");
      } else if constexpr (bindable_reflected_method_member<M>()) {
        using R = [:meta::return_type_of(M):];
        static_assert(native_ret_type_supported<R>(), "Lua: unsupported reflected method return");
        validate_reflected_method_params<M>();
      } else if constexpr (meta::is_nonstatic_data_member(M) && meta::is_public(M) && meta::has_identifier(M)) {
        static_assert(bindable_reflected_field_member<M>(), "Lua: unsupported reflected field");
      }
      validate_reflected_type_members<T, I+1>();
    }
  }

  template <class T>
  static consteval void validate_reflected_type() {
    constexpr auto ctx = meta::access_context::current();
    static constexpr auto bases = std::define_static_array(meta::bases_of(^^T, ctx));
    static_assert(meta::has_identifier(^^T), "Lua: reflected type must be named");
    static_assert(std::is_default_constructible_v<T>, "Lua: reflected types must be default constructible");
    static_assert(!meta::has_inaccessible_nonstatic_data_members(^^T, ctx), "Lua: reflected types must expose all data members");
    static_assert(bases.size() == 0, "Lua: reflected base classes are not supported yet");
    validate_reflected_type_members<T>();
  }

  template <class T, std::size_t I=0>
  static constexpr void object_to_state(VM& vm, const T& obj, TableId state) {
    constexpr auto ctx = meta::access_context::current();
    static constexpr auto mems = std::define_static_array(meta::members_of(^^T, ctx));
    if constexpr (I < mems.size()) {
      constexpr auto M = mems[I];
      if constexpr (bindable_reflected_field_member<M>()) {
        using FieldT = std::remove_cvref_t<decltype(std::declval<T&>().[:M:])>;
        StrId key = vm.H.sp.intern(meta::identifier_of(M));
        vm.H.rawset(state, Value::string(key), value_from_cpp<FieldT>(vm, obj.[:M:]));
      }
      object_to_state<T, I+1>(vm, obj, state);
    }
  }

  template <class T, std::size_t I=0>
  static constexpr void state_to_object(VM& vm, T& obj, TableId state) {
    constexpr auto ctx = meta::access_context::current();
    static constexpr auto mems = std::define_static_array(meta::members_of(^^T, ctx));
    if constexpr (I < mems.size()) {
      constexpr auto M = mems[I];
      if constexpr (bindable_reflected_field_member<M>()) {
        using FieldT = std::remove_cvref_t<decltype(std::declval<T&>().[:M:])>;
        StrId key = vm.H.sp.intern(meta::identifier_of(M));
        obj.[:M:] = arg_as<FieldT>(vm, vm.H.rawget(state, Value::string(key)));
      }
      state_to_object<T, I+1>(vm, obj, state);
    }
  }

  template <class T>
  static constexpr Value make_reflected_object(VM& vm, const T& obj) {
    StrId type_name = vm.reflected_type_name<T>();
    TableId mt = vm.reflected_type_mt(type_name);
    if (mt.id==0) throw "Lua: reflected type is not bound";
    TableId state = vm.H.new_table_pow2(table_pow2_for(reflected_field_count<T>() * 2u + 1u));
    object_to_state<T>(vm, obj, state);
    return Value::udata(vm.H.new_udata(mt, state, type_name));
  }

  template <class T>
  static constexpr T reflected_object_from_value(VM& vm, const Value& v) {
    if (v.tag!=Tag::UData) throw "Lua: expected reflected object";
    StrId expected = vm.reflected_type_name<T>();
    const UData& ud = vm.H.udata[v.u.id];
    if (ud.type_name.id != expected.id) throw "Lua: reflected object type mismatch";
    T obj{};
    state_to_object<T>(vm, obj, ud.state);
    return obj;
  }

  template <class T>
  static constexpr void sync_reflected_object(VM& vm, const Value& self, const T& obj) {
    if (self.tag!=Tag::UData) throw "Lua: expected reflected object";
    UData& ud = vm.H.udata[self.u.id];
    StrId expected = vm.reflected_type_name<T>();
    if (ud.type_name.id != expected.id) throw "Lua: reflected object type mismatch";
    object_to_state<T>(vm, obj, ud.state);
  }

  template <class T, std::size_t I=0>
  static constexpr void init_reflected_object_args(VM& vm, T& obj, const Value* a, std::size_t argc, std::size_t& next) {
    constexpr auto ctx = meta::access_context::current();
    static constexpr auto mems = std::define_static_array(meta::members_of(^^T, ctx));
    if constexpr (I < mems.size()) {
      constexpr auto M = mems[I];
      if constexpr (bindable_reflected_field_member<M>()) {
        using FieldT = std::remove_cvref_t<decltype(std::declval<T&>().[:M:])>;
        if (next < argc) obj.[:M:] = arg_as<FieldT>(vm, a[next++]);
      }
      init_reflected_object_args<T, I+1>(vm, obj, a, argc, next);
    }
  }

  template <meta::info T>
  static constexpr Multi api_ctor_tramp(VM& vm, const Value* a, std::size_t n) {
    using U = [:T:];
    std::size_t start = (n > 0 && a[0].tag == Tag::Table) ? 1u : 0u;
    U obj{};
    std::size_t used = 0;
    init_reflected_object_args<U>(vm, obj, a + start, n - start, used);
    if (used != n - start) throw "Lua: too many reflected constructor args";
    return Multi::one(make_reflected_object<U>(vm, obj));
  }

  template <meta::info T, meta::info M>
  static constexpr Multi api_field_setter_tramp(VM& vm, const Value* a, std::size_t n) {
    using U = [:T:];
    using FieldT = std::remove_cvref_t<decltype(std::declval<U&>().[:M:])>;
    if (n!=2 || a[0].tag!=Tag::UData) throw "Lua: reflected field set expects (self, value)";
    UData& ud = vm.H.udata[a[0].u.id];
    StrId expected = vm.reflected_type_name<U>();
    if (ud.type_name.id != expected.id) throw "Lua: reflected object type mismatch";
    StrId key = vm.H.sp.intern(meta::identifier_of(M));
    vm.H.rawset(ud.state, Value::string(key), value_from_cpp<FieldT>(vm, arg_as<FieldT>(vm, a[1])));
    return Multi::none();
  }

  template <meta::info T, meta::info M, std::size_t... Is>
  static constexpr Multi api_method_call_impl(VM& vm, const Value* a, std::index_sequence<Is...>) {
    using U = [:T:];
    using R = [:meta::return_type_of(M):];
    static constexpr auto params = std::define_static_array(meta::parameters_of(M));

    U obj = reflected_object_from_value<U>(vm, a[0]);
    auto arg = [&]<std::size_t I>() {
      using P = [:meta::type_of(params[I]):];
      return arg_as<P>(vm, a[I+1]);
    };

    if constexpr (std::is_void_v<R>) {
      obj.[:M:](arg.template operator()<Is>()...);
      sync_reflected_object<U>(vm, a[0], obj);
      return Multi::none();
    } else {
      R r = obj.[:M:](arg.template operator()<Is>()...);
      sync_reflected_object<U>(vm, a[0], obj);
      return ret_as<R>(vm, r);
    }
  }

  template <meta::info T, meta::info M>
  static constexpr Multi api_method_tramp(VM& vm, const Value* a, std::size_t n) {
    static constexpr auto params = std::define_static_array(meta::parameters_of(M));
    if (n != params.size() + 1u || a[0].tag!=Tag::UData) throw "Lua: reflected method arity mismatch";
    return api_method_call_impl<T, M>(vm, a, std::make_index_sequence<params.size()>{});
  }

  template <meta::info F, class Tr, class R, std::size_t... Is>
  static constexpr Multi call_wrapped_impl(VM& vm, const Value* args, std::index_sequence<Is...>) {
    static_assert((native_arg_type_supported<std::tuple_element_t<Is, typename Tr::args_tuple>>() && ...),
      "Lua: unsupported native arg type");
    if constexpr (std::is_void_v<R>) {
      [:F:]( arg_as<std::tuple_element_t<Is, typename Tr::args_tuple>>(vm, args[Is])... );
      return Multi::none();
    } else {
      R r = [:F:]( arg_as<std::tuple_element_t<Is, typename Tr::args_tuple>>(vm, args[Is])... );
      return ret_as<R>(vm, r);
    }
  }

  template <meta::info F>
  static constexpr Multi call_wrapped(VM& vm, const Value* args, std::size_t argc) {
    using FnRef  = decltype(([:F:]));
    using FnType = std::remove_reference_t<FnRef>;
    using Tr     = fn_traits<FnType>;
    using R      = typename Tr::ret;
    static_assert(native_ret_type_supported<R>(), "Lua: unsupported native return");
    if (argc != Tr::arity) throw "Lua: arity mismatch (native)";
    return call_wrapped_impl<F, Tr, R>(vm, args, std::make_index_sequence<Tr::arity>{});
  }

  constexpr std::uint32_t reg_native(std::string_view name, NativeFn f) {
    if (native_count >= natives.size()) throw "Lua: too many natives";
    native_names[native_count] = H.sp.intern(name);
    natives[native_count] = f;
    return native_count++;
  }
  constexpr Value mk_native(std::uint32_t id){ return Value::func_native(id); }

  // --- base natives ---
  static constexpr Multi nf_print(VM& vm, const Value* a, std::size_t n);
  static constexpr Multi nf_assert(VM& vm, const Value* a, std::size_t n);
  static constexpr Multi nf_error(VM& vm, const Value* a, std::size_t n);
  static constexpr Multi nf_pcall(VM& vm, const Value* a, std::size_t n);
  static constexpr Multi nf_xpcall(VM& vm, const Value* a, std::size_t n);
  static constexpr Multi nf_warn(VM& vm, const Value* a, std::size_t n);
  static constexpr Multi nf_rawequal(VM& vm, const Value* a, std::size_t n);
  static constexpr Multi nf_rawlen(VM& vm, const Value* a, std::size_t n);
  static constexpr Multi nf_tostring(VM& vm, const Value* a, std::size_t n);
  static constexpr Multi nf_type(VM& vm, const Value* a, std::size_t n);
  static constexpr Multi nf_setmetatable(VM& vm, const Value* a, std::size_t n);
  static constexpr Multi nf_getmetatable(VM& vm, const Value* a, std::size_t n);
  static constexpr Multi nf_rawget(VM& vm, const Value* a, std::size_t n);
  static constexpr Multi nf_rawset(VM& vm, const Value* a, std::size_t n);
  static constexpr Multi nf_next(VM& vm, const Value* a, std::size_t n);
  static constexpr Multi nf_pairs(VM& vm, const Value* a, std::size_t n);
  static constexpr Multi nf_ipairs(VM& vm, const Value* a, std::size_t n);
  static constexpr Multi nf_ipairs_iter(VM& vm, const Value* a, std::size_t n);
  static constexpr Multi nf_select(VM& vm, const Value* a, std::size_t n);
  static constexpr Multi nf_tonumber(VM& vm, const Value* a, std::size_t n);
  static constexpr Multi nf_load(VM& vm, const Value* a, std::size_t n);
  static constexpr Multi nf_reflect_udata_index(VM& vm, const Value* a, std::size_t n);
  static constexpr Multi nf_reflect_udata_newindex(VM& vm, const Value* a, std::size_t n);
  static constexpr Multi nf_reflect_udata_tostring(VM& vm, const Value* a, std::size_t n);
  static constexpr Multi nf_reflect_udata_pairs(VM& vm, const Value* a, std::size_t n);
  static constexpr Multi nf_reflect_udata_pairs_iter(VM& vm, const Value* a, std::size_t n);

  // --- table module ---
  static constexpr Multi nf_table_insert(VM& vm, const Value* a, std::size_t n);
  static constexpr Multi nf_table_remove(VM& vm, const Value* a, std::size_t n);
  static constexpr Multi nf_table_sort(VM& vm, const Value* a, std::size_t n);
  static constexpr Multi nf_table_concat(VM& vm, const Value* a, std::size_t n);
  static constexpr Multi nf_table_pack(VM& vm, const Value* a, std::size_t n);
  static constexpr Multi nf_table_unpack(VM& vm, const Value* a, std::size_t n);
  static constexpr Multi nf_table_move(VM& vm, const Value* a, std::size_t n);

  // --- math module ---
  static constexpr Multi nf_math_floor(VM& vm, const Value* a, std::size_t n);
  static constexpr Multi nf_math_ceil(VM& vm, const Value* a, std::size_t n);
  static constexpr Multi nf_math_abs(VM& vm, const Value* a, std::size_t n);
  static constexpr Multi nf_math_min(VM& vm, const Value* a, std::size_t n);
  static constexpr Multi nf_math_max(VM& vm, const Value* a, std::size_t n);
  static constexpr Multi nf_math_random(VM& vm, const Value* a, std::size_t n);
  static constexpr Multi nf_math_randomseed(VM& vm, const Value* a, std::size_t n);
  static constexpr Multi nf_math_acos(VM& vm, const Value* a, std::size_t n);
  static constexpr Multi nf_math_asin(VM& vm, const Value* a, std::size_t n);
  static constexpr Multi nf_math_atan(VM& vm, const Value* a, std::size_t n);
  static constexpr Multi nf_math_deg(VM& vm, const Value* a, std::size_t n);
  static constexpr Multi nf_math_rad(VM& vm, const Value* a, std::size_t n);
  static constexpr Multi nf_math_exp(VM& vm, const Value* a, std::size_t n);
  static constexpr Multi nf_math_fmod(VM& vm, const Value* a, std::size_t n);
  static constexpr Multi nf_math_modf(VM& vm, const Value* a, std::size_t n);
  static constexpr Multi nf_math_tointeger(VM& vm, const Value* a, std::size_t n);
  static constexpr Multi nf_math_type(VM& vm, const Value* a, std::size_t n);
  static constexpr Multi nf_math_ult(VM& vm, const Value* a, std::size_t n);
  static constexpr Multi nf_math_sin(VM& vm, const Value* a, std::size_t n);
  static constexpr Multi nf_math_cos(VM& vm, const Value* a, std::size_t n);
  static constexpr Multi nf_math_tan(VM& vm, const Value* a, std::size_t n);
  static constexpr Multi nf_math_sqrt(VM& vm, const Value* a, std::size_t n);
  static constexpr Multi nf_math_log(VM& vm, const Value* a, std::size_t n);

  // --- string module ---
  static constexpr Multi nf_string_len(VM& vm, const Value* a, std::size_t n);
  static constexpr Multi nf_string_sub(VM& vm, const Value* a, std::size_t n);
  static constexpr Multi nf_string_find(VM& vm, const Value* a, std::size_t n);
  static constexpr Multi nf_string_match(VM& vm, const Value* a, std::size_t n);
  static constexpr Multi nf_string_gmatch(VM& vm, const Value* a, std::size_t n);
  static constexpr Multi nf_string_gmatch_iter(VM& vm, const Value* a, std::size_t n);
  static constexpr Multi nf_string_gsub(VM& vm, const Value* a, std::size_t n);
  static constexpr Multi nf_string_byte(VM& vm, const Value* a, std::size_t n);
  static constexpr Multi nf_string_char(VM& vm, const Value* a, std::size_t n);
  static constexpr Multi nf_string_upper(VM& vm, const Value* a, std::size_t n);
  static constexpr Multi nf_string_lower(VM& vm, const Value* a, std::size_t n);
  static constexpr Multi nf_string_rep(VM& vm, const Value* a, std::size_t n);
  static constexpr Multi nf_string_reverse(VM& vm, const Value* a, std::size_t n);
  static constexpr Multi nf_string_format(VM& vm, const Value* a, std::size_t n);
  static constexpr Multi nf_string_pack(VM& vm, const Value* a, std::size_t n);
  static constexpr Multi nf_string_packsize(VM& vm, const Value* a, std::size_t n);
  static constexpr Multi nf_string_unpack(VM& vm, const Value* a, std::size_t n);

  // --- utf8 module ---
  static constexpr Multi nf_utf8_char(VM& vm, const Value* a, std::size_t n);
  static constexpr Multi nf_utf8_codepoint(VM& vm, const Value* a, std::size_t n);
  static constexpr Multi nf_utf8_codes(VM& vm, const Value* a, std::size_t n);
  static constexpr Multi nf_utf8_codes_iter(VM& vm, const Value* a, std::size_t n);
  static constexpr Multi nf_utf8_len(VM& vm, const Value* a, std::size_t n);
  static constexpr Multi nf_utf8_offset(VM& vm, const Value* a, std::size_t n);

  std::uint32_t id_next{invalid_native_id};
  std::uint32_t id_ipairs_iter{invalid_native_id};
  std::uint32_t id_utf8_codes_iter{invalid_native_id};
  std::uint32_t id_string_gmatch_iter{invalid_native_id};
  std::uint32_t id_reflect_udata_index{invalid_native_id};
  std::uint32_t id_reflect_udata_newindex{invalid_native_id};
  std::uint32_t id_reflect_udata_tostring{invalid_native_id};
  std::uint32_t id_reflect_udata_pairs{invalid_native_id};
  std::uint32_t id_reflect_udata_pairs_iter{invalid_native_id};

  // call / eval / exec
  constexpr Multi call_value(const Value& callee, const Value* args, std::size_t argc);
  struct Exec {
    bool is_break{false};
    bool has_ret{false};
    bool is_goto{false};
    StrId goto_name{};
    Multi ret{};
  };
  constexpr Multi eval_expr(ExprId id, EnvId env, VarArgs vargs, bool multret);
  constexpr Exec  exec_block(BRange blk, EnvId env, VarArgs vargs);
  constexpr Exec  exec_stmt(const Stmt& s, EnvId env, VarArgs vargs);

  // reflection binding (consteval-only)
  template <meta::info F>
  static constexpr Multi api_tramp(VM& vm, const Value* a, std::size_t n) { return call_wrapped<F>(vm,a,n); }

  consteval void bind_api_global(std::string_view name, Value value) {
    StrId sid = H.sp.intern(name);
    if (!H.rawget(G, Value::string(sid)).is_nil()) throw "Lua: api global name conflict";
    table_set(G, Value::string(sid), value);
  }

  template <meta::info F>
  consteval void bind_one_api_function() {
    static_assert(meta::is_function(F) && meta::has_identifier(F), "api functions must be named");
    constexpr std::string_view nm = meta::identifier_of(F);
    std::uint32_t id = reg_native(nm, &api_tramp<F>);
    bind_api_global(nm, mk_native(id));
  }

  template <meta::info E, std::size_t I=0>
  consteval void bind_one_api_enum_members(TableId tab) {
    using U = [:E:];
    static constexpr auto enums = std::define_static_array(meta::enumerators_of(E));
    if constexpr (I < enums.size()) {
      constexpr auto V = enums[I];
      StrId key = H.sp.intern(meta::identifier_of(V));
      if (!H.rawget(tab, Value::string(key)).is_nil()) throw "Lua: duplicate enum value name";
      H.rawset(tab, Value::string(key), value_from_cpp<U>(*this, [:V:]));
      bind_one_api_enum_members<E, I+1>(tab);
    }
  }

  template <meta::info E>
  consteval void bind_one_api_enum() {
    static_assert(meta::is_enum_type(E) && meta::has_identifier(E), "api enums must be named");
    static constexpr auto enums = std::define_static_array(meta::enumerators_of(E));
    TableId tab = H.new_table_pow2(table_pow2_for(enums.size() * 2u + 1u));
    bind_one_api_enum_members<E>(tab);
    bind_api_global(meta::identifier_of(E), Value::table(tab));
  }

  template <meta::info T, std::size_t I=0>
  consteval void bind_one_api_type_members(TableId methods, TableId setters) {
    static constexpr auto ctx = meta::access_context::current();
    static constexpr auto mems = std::define_static_array(meta::members_of(T, ctx));
    if constexpr (I < mems.size()) {
      constexpr auto M = mems[I];
      if constexpr (bindable_reflected_field_member<M>()) {
        StrId key = H.sp.intern(meta::identifier_of(M));
        if (!H.rawget(methods, Value::string(key)).is_nil() || !H.rawget(setters, Value::string(key)).is_nil()) {
          throw "Lua: duplicate reflected member name";
        }
        std::uint32_t id = reg_native(meta::identifier_of(M), &api_field_setter_tramp<T, M>);
        H.rawset(setters, Value::string(key), mk_native(id));
      } else if constexpr (bindable_reflected_method_member<M>()) {
        StrId key = H.sp.intern(meta::identifier_of(M));
        if (!H.rawget(methods, Value::string(key)).is_nil() || !H.rawget(setters, Value::string(key)).is_nil()) {
          throw "Lua: duplicate reflected member name";
        }
        std::uint32_t id = reg_native(meta::identifier_of(M), &api_method_tramp<T, M>);
        H.rawset(methods, Value::string(key), mk_native(id));
      }
      bind_one_api_type_members<T, I+1>(methods, setters);
    }
  }

  template <meta::info T>
  consteval void bind_one_api_type() {
    using U = [:T:];
    static_assert(meta::is_class_type(T) && meta::has_identifier(T), "api reflected types must be named classes");
    validate_reflected_type<U>();

    static constexpr std::size_t field_count = reflected_field_count<U>();
    static constexpr std::size_t method_count = reflected_method_count<U>();

    TableId type_table = H.new_table_pow2(table_pow2_for(4));
    TableId type_mt = H.new_table_pow2(table_pow2_for(2));
    TableId inst_mt = H.new_table_pow2(table_pow2_for(8));
    TableId methods = H.new_table_pow2(table_pow2_for(method_count * 2u + 1u));
    TableId setters = H.new_table_pow2(table_pow2_for(field_count * 2u + 1u));

    constexpr std::string_view nm = meta::identifier_of(T);
    std::uint32_t ctor_id = reg_native(nm, &api_ctor_tramp<T>);
    table_set(type_table, Value::string(s_new), mk_native(ctor_id));
    H.rawset(type_mt, Value::string(s__call), mk_native(ctor_id));
    H.tables[type_table.id].mt = type_mt;

    H.rawset(inst_mt, Value::string(s__index), mk_native(id_reflect_udata_index));
    H.rawset(inst_mt, Value::string(s__newindex), mk_native(id_reflect_udata_newindex));
    H.rawset(inst_mt, Value::string(s__tostring), mk_native(id_reflect_udata_tostring));
    H.rawset(inst_mt, Value::string(s__pairs), mk_native(id_reflect_udata_pairs));
    H.rawset(inst_mt, Value::string(s__ct_methods), Value::table(methods));
    H.rawset(inst_mt, Value::string(s__ct_setters), Value::table(setters));

    bind_one_api_type_members<T>(methods, setters);
    register_reflected_type<U>(inst_mt);
    bind_api_global(nm, Value::table(type_table));
  }

  template <meta::info M>
  consteval void bind_one_api_member() {
    if constexpr (meta::is_function(M) && meta::has_identifier(M)) {
      bind_one_api_function<M>();
    } else if constexpr (meta::is_class_type(M) && meta::has_identifier(M)) {
      bind_one_api_type<M>();
    } else if constexpr (meta::is_enum_type(M) && meta::has_identifier(M)) {
      bind_one_api_enum<M>();
    } else if constexpr (meta::has_identifier(M)) {
      static_assert(meta::is_function(M) || meta::is_class_type(M) || meta::is_enum_type(M),
        "api namespace may contain only free functions, class types, and enums");
    }
  }

  template <meta::info Ns, std::size_t I>
  consteval void bind_namespace_rec() {
    constexpr auto ctx = meta::access_context::current();
    static constexpr auto mems = std::define_static_array(meta::members_of(Ns, ctx));
    if constexpr (I < mems.size()) {
      bind_one_api_member<mems[I]>();
      bind_namespace_rec<Ns, I+1>();
    }
  }

  template <meta::info Ns>
  consteval void bind_namespace() { bind_namespace_rec<Ns, 0>(); }

  consteval void bind_api_namespace() { bind_namespace<^^api>(); }

  // init/run
  static constexpr std::uint32_t LIB_BASE = 1u << 0;
  static constexpr std::uint32_t LIB_API  = 1u << 1;
  static constexpr std::uint32_t LIB_TABLE= 1u << 2;
  static constexpr std::uint32_t LIB_MATH = 1u << 3;
  static constexpr std::uint32_t LIB_STRING = 1u << 4;
  static constexpr std::uint32_t LIB_UTF8 = 1u << 5;
  static constexpr std::uint32_t LIB_ALL  = LIB_BASE | LIB_API | LIB_TABLE | LIB_MATH | LIB_STRING | LIB_UTF8;

  consteval void open_base();
  consteval void open_table();
  consteval void open_math();
  consteval void open_string();
  consteval void open_utf8();
  consteval void open_api_support();
  consteval void init_runtime(std::uint32_t libs);
  consteval void init(std::uint32_t libs);
  consteval void init() { init(LIB_BASE); }
  constexpr Value compile_chunk(std::string_view src, TableId env_table);
  constexpr Multi run_chunk(std::string_view src);
};

// ---- VM impl ----
constexpr Multi VM::call_value(const Value& callee, const Value* args, std::size_t argc) {
  tick();
  if (protected_depth && pending_error) return Multi::none();
  if (callee.tag==Tag::Func && callee.f.is_native) {
    auto id=callee.f.id;
    if (id>=native_count) throw "Lua: bad native id";
    Multi ret=natives[id](*this,args,argc);
    if (protected_depth && pending_error) return Multi::none();
    return ret;
  }
  if (callee.tag==Tag::Func && !callee.f.is_native) {
    const Closure& C=H.closures[callee.f.id];
    const Proto& P=H.protos[C.proto.id];

    EnvId fenv=H.new_env(C.def_env, C.env_table);

    for (std::uint16_t i=0;i<P.params_n;++i) {
      StrId nm=A.expr[A.list[P.params_off+i]].s;
      Value v=(i<argc)? args[i] : Value::nil();
      H.env_add(fenv,nm,v);
    }

    std::array<Value, MAX_ARGS> varbuf{};
    std::size_t vn=0;
    if (P.is_vararg && argc>P.params_n) {
      vn=argc-P.params_n;
      if (vn>MAX_ARGS) vn=MAX_ARGS;
      for (std::size_t i=0;i<vn;++i) varbuf[i]=args[P.params_n+i];
    }
    VarArgs va{varbuf.data(), vn};

    Exec r=exec_block(P.block, fenv, va);
    if (protected_depth && pending_error) return Multi::none();
    if (r.is_break) throw "Lua: break outside loop";
    if (r.is_goto) throw "Lua: no visible label for goto";
    return r.has_ret ? r.ret : Multi::none();
  }

  TableId mt=metatable_of(callee);
  Value mm=rawget_mt(mt,s__call);
  if (!mm.is_nil()) {
    if (argc+1>MAX_ARGS) throw "Lua: too many args";
    std::array<Value, MAX_ARGS> a2{};
    a2[0]=callee;
    for (std::size_t i=0;i<argc;++i) a2[i+1]=args[i];
    return call_value(mm,a2.data(),argc+1);
  }
  throw "Lua: attempt to call a non-callable value";
}

constexpr Multi VM::eval_expr(ExprId id, EnvId env, VarArgs vargs, bool multret) {
  tick();
  const Expr& e=A.expr[id];
  switch (e.k) {
    case EKind::Nil: return Multi::one(Value::nil());
    case EKind::Bool:return Multi::one(Value::boolean(e.bo));
    case EKind::Int: return Multi::one(Value::integer(e.i));
    case EKind::Num: return Multi::one(Value::number(e.num));
    case EKind::Str: return Multi::one(Value::string(e.s));
    case EKind::VarArg: {
      if (!multret) return Multi::one(vargs.n? vargs.p[0] : Value::nil());
      Multi m; m.n=(std::uint8_t)((vargs.n>MAX_RET)?MAX_RET:vargs.n);
      for (std::uint8_t i=0;i<m.n;++i) m.v[i]=vargs.p[i];
      return m;
    }
    case EKind::Name: {
      CellId c=H.env_find(env,e.s);
      if (c.id!=UINT32_MAX) return Multi::one(H.cells[c.id].v);
      return Multi::one(table_get(H.envs[env.id].env_table, Value::string(e.s)));
    }
    case EKind::Paren:
      // Lua adjustment: parenthesized expressions always produce one value.
      return Multi::one(first(eval_expr(e.a,env,vargs,false)));
    case EKind::Unary: {
      Value x=first(eval_expr(e.a,env,vargs,false));
      if (e.op==TK::Not) return Multi::one(Value::boolean(!truthy(x)));
      if (e.op==TK::Minus) {
        if (x.tag==Tag::Int) return Multi::one(Value::integer(-x.i));
        if (x.tag==Tag::Num) return Multi::one(Value::number(-x.n));
        return Multi::one(meta_un(x,s__unm,"Lua: unary minus"));
      }
      if (e.op==TK::BitXor) {
        std::int64_t xi=0;
        if (to_int_maybe(x, xi)) return Multi::one(Value::integer(bit_not(xi)));
        return Multi::one(meta_un(x,s__bnot,"Lua: bitwise not"));
      }
      if (e.op==TK::Len) return Multi::one(v_len(x));
      throw "Lua: bad unary";
    }
    case EKind::Binary: {
      if (e.op==TK::And) {
        Value a=first(eval_expr(e.a,env,vargs,false));
        if (!truthy(a)) return Multi::one(a);
        return eval_expr(e.b,env,vargs,multret);
      }
      if (e.op==TK::Or) {
        Value a=first(eval_expr(e.a,env,vargs,false));
        if (truthy(a)) return Multi::one(a);
        return eval_expr(e.b,env,vargs,multret);
      }

      Value a=first(eval_expr(e.a,env,vargs,false));
      Value b=first(eval_expr(e.b,env,vargs,false));

      switch (e.op) {
        case TK::Plus:
          if (is_number(a)&&is_number(b)) {
            if (a.tag==Tag::Int && b.tag==Tag::Int) return Multi::one(Value::integer(a.i+b.i));
            return Multi::one(Value::number(to_num(a)+to_num(b)));
          }
          return Multi::one(meta_bin(a,b,s__add,"Lua: add"));
        case TK::Minus:
          if (is_number(a)&&is_number(b)) {
            if (a.tag==Tag::Int && b.tag==Tag::Int) return Multi::one(Value::integer(a.i-b.i));
            return Multi::one(Value::number(to_num(a)-to_num(b)));
          }
          return Multi::one(meta_bin(a,b,s__sub,"Lua: sub"));
        case TK::Mul:
          if (is_number(a)&&is_number(b)) {
            if (a.tag==Tag::Int && b.tag==Tag::Int) return Multi::one(Value::integer(a.i*b.i));
            return Multi::one(Value::number(to_num(a)*to_num(b)));
          }
          return Multi::one(meta_bin(a,b,s__mul,"Lua: mul"));
        case TK::Div:
          if (is_number(a)&&is_number(b)) return Multi::one(Value::number(to_num(a)/to_num(b)));
          return Multi::one(meta_bin(a,b,s__div,"Lua: div"));
        case TK::Idiv:
          if (is_number(a)&&is_number(b)) {
            if (a.tag==Tag::Int && b.tag==Tag::Int) {
              return Multi::one(Value::integer(floor_div_int(a.i,b.i)));
            }
            double bv=to_num(b);
            if (bv==0.0) throw "Lua: division by zero";
            return Multi::one(Value::number(floor_num(to_num(a)/bv)));
          }
          return Multi::one(meta_bin(a,b,s__idiv,"Lua: idiv"));
        case TK::Mod:
          if (is_number(a)&&is_number(b)) {
            if (a.tag==Tag::Int && b.tag==Tag::Int) {
              std::int64_t q=floor_div_int(a.i,b.i);
              return Multi::one(Value::integer(a.i - q*b.i));
            }
            double av=to_num(a), bv=to_num(b);
            if (bv==0.0) throw "Lua: modulo by zero";
            return Multi::one(Value::number(av - floor_num(av/bv)*bv));
          }
          return Multi::one(meta_bin(a,b,s__mod,"Lua: mod"));
        case TK::BitAnd: {
          std::int64_t ai=0, bi=0;
          if (to_int_maybe(a, ai) && to_int_maybe(b, bi)) return Multi::one(Value::integer(bit_and(ai,bi)));
          return Multi::one(meta_bin(a,b,s__band,"Lua: bitwise and"));
        }
        case TK::BitOr: {
          std::int64_t ai=0, bi=0;
          if (to_int_maybe(a, ai) && to_int_maybe(b, bi)) return Multi::one(Value::integer(bit_or(ai,bi)));
          return Multi::one(meta_bin(a,b,s__bor,"Lua: bitwise or"));
        }
        case TK::BitXor: {
          std::int64_t ai=0, bi=0;
          if (to_int_maybe(a, ai) && to_int_maybe(b, bi)) return Multi::one(Value::integer(bit_xor(ai,bi)));
          return Multi::one(meta_bin(a,b,s__bxor,"Lua: bitwise xor"));
        }
        case TK::Shl: {
          std::int64_t ai=0, bi=0;
          if (to_int_maybe(a, ai) && to_int_maybe(b, bi)) return Multi::one(Value::integer(shift_left(ai,bi)));
          return Multi::one(meta_bin(a,b,s__shl,"Lua: bitwise shift left"));
        }
        case TK::Shr: {
          std::int64_t ai=0, bi=0;
          if (to_int_maybe(a, ai) && to_int_maybe(b, bi)) return Multi::one(Value::integer(shift_right(ai,bi)));
          return Multi::one(meta_bin(a,b,s__shr,"Lua: bitwise shift right"));
        }
        case TK::Pow:
          if (is_number(a)&&is_number(b)) return Multi::one(Value::number(pow_num(to_num(a),to_num(b))));
          return Multi::one(meta_bin(a,b,s__pow,"Lua: pow"));
        case TK::DotDot:
          return Multi::one(v_concat(a,b));
        case TK::Eq: return Multi::one(Value::boolean(v_eq(a,b)));
        case TK::Ne: return Multi::one(Value::boolean(!v_eq(a,b)));
        case TK::Lt: return Multi::one(Value::boolean(v_lt(a,b)));
        case TK::Le: return Multi::one(Value::boolean(v_le(a,b)));
        case TK::Gt: return Multi::one(Value::boolean(v_lt(b,a)));
        case TK::Ge: return Multi::one(Value::boolean(v_le(b,a)));
        default: throw "Lua: binary op not implemented";
      }
    }
    case EKind::TableCtor: {
      TableId t=H.new_table_pow2(7);
      std::int64_t ai=1;
      for (std::uint16_t k=0;k<e.r.n;++k) {
        const Field& f=A.fields[e.r.off+k];
        if (f.k==FieldK::Array) {
          bool last=(k+1==e.r.n);
          Multi mv=eval_expr(f.val,env,vargs,last);
          if (last && mv.n>1) for (std::uint8_t i=0;i<mv.n;++i) table_set(t,Value::integer(ai++),mv.v[i]);
          else table_set(t,Value::integer(ai++),first(mv));
        } else if (f.k==FieldK::Name) {
          table_set(t,Value::string(f.name),first(eval_expr(f.val,env,vargs,false)));
        } else {
          Value key=first(eval_expr(f.key,env,vargs,false));
          Value vv=first(eval_expr(f.val,env,vargs,false));
          table_set(t,key,vv);
        }
      }
      return Multi::one(Value::table(t));
    }
    case EKind::FuncExpr: {
      std::uint32_t cid=H.new_closure(e.proto, env, H.envs[env.id].env_table);
      return Multi::one(Value::func_closure(cid));
    }
    case EKind::Index: {
      Value obj=first(eval_expr(e.a,env,vargs,false));
      Value key=first(eval_expr(e.b,env,vargs,false));
      if (obj.tag==Tag::Table) return Multi::one(table_get(obj.t,key));
      TableId mt=metatable_of(obj);
      Value idx=rawget_mt(mt,s__index);
      if (idx.is_nil()) throw "Lua: index on non-table";
      if (idx.tag==Tag::Table) return Multi::one(table_get(idx.t,key));
      std::array<Value, 2> args{obj, key};
      return Multi::one(call_with_first(idx, args));
    }
    case EKind::Field: {
      Value obj=first(eval_expr(e.a,env,vargs,false));
      Value key=Value::string(e.s);
      if (obj.tag==Tag::Table) return Multi::one(table_get(obj.t,key));
      TableId mt=metatable_of(obj);
      Value idx=rawget_mt(mt,s__index);
      if (idx.is_nil()) throw "Lua: field on non-table";
      if (idx.tag==Tag::Table) return Multi::one(table_get(idx.t,key));
      std::array<Value, 2> args{obj, key};
      return Multi::one(call_with_first(idx, args));
    }
    case EKind::Call: {
      Value fn=first(eval_expr(e.a,env,vargs,false));
      std::array<Value, MAX_ARGS> args{};
      std::size_t argc=0;
      for (std::uint16_t i=0;i<e.r.n;++i) {
        bool last=(i+1==e.r.n);
        Multi mv=eval_expr(A.list[e.r.off+i],env,vargs,last);
        if (last && mv.n>1) for (std::uint8_t k=0;k<mv.n && argc<MAX_ARGS;++k) args[argc++]=mv.v[k];
        else args[argc++]=first(mv);
      }
      Multi ret=call_value(fn,args.data(),argc);
      return multret ? ret : Multi::one(first(ret));
    }
    case EKind::Method: {
      Value obj=first(eval_expr(e.a,env,vargs,false));
      Value mfn{};
      if (obj.tag==Tag::Table) mfn=table_get(obj.t,Value::string(e.s));
      else {
        TableId mt=metatable_of(obj);
        Value idx=rawget_mt(mt,s__index);
        if (idx.tag==Tag::Table) mfn=table_get(idx.t,Value::string(e.s));
        else {
          std::array<Value, 2> recv_args{obj, Value::string(e.s)};
          mfn=call_with_first(idx, recv_args);
        }
      }
      std::array<Value, MAX_ARGS> args{};
      std::size_t argc=0;
      args[argc++]=obj;
      for (std::uint16_t i=0;i<e.r.n;++i) {
        bool last=(i+1==e.r.n);
        Multi mv=eval_expr(A.list[e.r.off+i],env,vargs,last);
        if (last && mv.n>1) for (std::uint8_t k=0;k<mv.n && argc<MAX_ARGS;++k) args[argc++]=mv.v[k];
        else args[argc++]=first(mv);
      }
      Multi ret=call_value(mfn,args.data(),argc);
      return multret ? ret : Multi::one(first(ret));
    }
  }
  throw "Lua: missing expr kind";
}

constexpr VM::Exec VM::exec_block(BRange blk, EnvId env, VarArgs vargs) {
  Exec ex{};
  std::array<StrId, 2048> labels{};
  std::array<std::uint16_t, 2048> label_pc{};
  std::uint16_t nlabels=0;

  auto jump_crosses_new_local = [&](std::uint16_t from_pc, std::uint16_t to_pc) constexpr -> bool {
    if (to_pc <= from_pc) return false; // only forward jumps can enter not-yet-active local scopes
    for (std::uint16_t i=(std::uint16_t)(from_pc+1); i<=to_pc; ++i) {
      StmtId sid2=A.blist[blk.off+i];
      const Stmt& s2=A.st[sid2];
      if (s2.k==SKind::Local && s2.r0.n>0) return true;
      if (s2.k==SKind::LocalFunc) return true;
    }
    return false;
  };

  for (std::uint16_t i=0;i<blk.n;++i) {
    StmtId sid=A.blist[blk.off+i];
    const Stmt& s=A.st[sid];
    if (s.k!=SKind::Label) continue;
    if (nlabels>=static_cast<std::uint16_t>(labels.size())) throw "Lua: too many labels in block";
    for (std::uint16_t k=0;k<nlabels;++k) {
      if (labels[k].id==s.name.id) throw "Lua: duplicate label in block";
    }
    labels[nlabels]=s.name;
    label_pc[nlabels]=i;
    ++nlabels;
  }

  std::uint16_t pc=0;
  while (pc<blk.n) {
    tick();
    if (protected_depth && pending_error) return ex;
    StmtId sid = A.blist[blk.off + pc];
    Exec r=exec_stmt(A.st[sid],env,vargs);
    if (protected_depth && pending_error) return ex;
    if (r.has_ret || r.is_break) return r;
    if (r.is_goto) {
      bool found=false;
      for (std::uint16_t k=0;k<nlabels;++k) {
        if (labels[k].id==r.goto_name.id) {
          if (jump_crosses_new_local(pc, label_pc[k])) throw "Lua: cannot jump into the scope of a local";
          pc=label_pc[k];
          found=true;
          break;
        }
      }
      if (!found) return r;
      continue;
    }
    ++pc;
  }
  return ex;
}

constexpr VM::Exec VM::exec_stmt(const Stmt& s, EnvId env, VarArgs vargs) {
  tick();
  Exec ex{};
  switch (s.k) {
    case SKind::Do: {
      EnvId benv=H.new_env(env,H.envs[env.id].env_table);
      return exec_block(s.b0,benv,vargs);
    }
    case SKind::While: {
      while (truthy(first(eval_expr(s.e0,env,vargs,false)))) {
        EnvId benv=H.new_env(env,H.envs[env.id].env_table);
        Exec r=exec_block(s.b0,benv,vargs);
        if (r.has_ret) return r;
        if (r.is_goto) return r;
        if (r.is_break) break;
      }
      return ex;
    }
    case SKind::Repeat: {
      for (;;) {
        EnvId benv=H.new_env(env,H.envs[env.id].env_table);
        Exec r=exec_block(s.b0,benv,vargs);
        if (r.has_ret) return r;
        if (r.is_goto) return r;
        if (truthy(first(eval_expr(s.e0,env,vargs,false)))) break;
        if (r.is_break) break;
      }
      return ex;
    }
    case SKind::If: {
      if (truthy(first(eval_expr(s.e0,env,vargs,false)))) {
        EnvId benv=H.new_env(env,H.envs[env.id].env_table);
        return exec_block(s.b0,benv,vargs);
      }
      for (std::uint16_t i=0;i<s.r1.n;++i) {
        ExprId ci=A.list[s.r1.off+i];
        StmtId bi=(StmtId)A.expr[A.list[s.r2.off+i]].i;
        if (truthy(first(eval_expr(ci,env,vargs,false)))) {
          EnvId benv=H.new_env(env,H.envs[env.id].env_table);
          return exec_block(A.st[bi].b0,benv,vargs);
        }
      }
      if (s.flag) {
        StmtId eb=(StmtId)s.e1;
        EnvId benv=H.new_env(env,H.envs[env.id].env_table);
        return exec_block(A.st[eb].b0,benv,vargs);
      }
      return ex;
    }
    case SKind::ForNum: {
      double start=to_num(first(eval_expr(s.e0,env,vargs,false)));
      double limit=to_num(first(eval_expr(s.e1,env,vargs,false)));
      double step =to_num(first(eval_expr(s.e2,env,vargs,false)));

      EnvId lenv=H.new_env(env,H.envs[env.id].env_table);
      H.env_add(lenv,s.name,Value::number(start));

      for (double x=start; (step>=0? x<=limit : x>=limit); x+=step) {
        CellId c=H.env_find(lenv,s.name);
        H.cells[c.id].v=Value::number(x);
        EnvId benv=H.new_env(lenv,H.envs[env.id].env_table);
        Exec r=exec_block(s.b0,benv,vargs);
        if (r.has_ret) return r;
        if (r.is_goto) return r;
        if (r.is_break) break;
      }
      return ex;
    }
    case SKind::ForIn: {
      // Lua generic-for control tuple:
      // local f, st, var = explist
      std::array<Value, MAX_ARGS> rhs{};
      std::size_t outn=0;
      for (std::uint16_t i=0;i<s.r1.n;++i) {
        bool last=(i+1==s.r1.n);
        Multi mv=eval_expr(A.list[s.r1.off+i],env,vargs,last);
        if (last && mv.n>1) for (std::uint8_t k=0;k<mv.n && outn<MAX_ARGS;++k) rhs[outn++]=mv.v[k];
        else rhs[outn++]=first(mv);
      }

      Value f   = (outn>0)? rhs[0] : Value::nil();
      Value st  = (outn>1)? rhs[1] : Value::nil();
      Value var = (outn>2)? rhs[2] : Value::nil();

      EnvId lenv=H.new_env(env,H.envs[env.id].env_table);
      for (std::uint16_t i=0;i<s.r0.n;++i) {
        StrId nm=A.expr[A.list[s.r0.off+i]].s;
        H.env_add(lenv,nm,Value::nil());
      }

      for (;;) {
        std::array<Value, 2> args{st, var};
        Multi r=call_value(f,args.data(),2);
        Value a0 = r.n? r.v[0] : Value::nil();
        var=a0;
        if (a0.is_nil()) break;

        for (std::uint16_t i=0;i<s.r0.n;++i) {
          StrId nm=A.expr[A.list[s.r0.off+i]].s;
          CellId c=H.env_find(lenv,nm);
          H.cells[c.id].v = (i<r.n)? r.v[i] : Value::nil();
        }

        EnvId benv=H.new_env(lenv,H.envs[env.id].env_table);
        Exec rr=exec_block(s.b0,benv,vargs);
        if (rr.has_ret) return rr;
        if (rr.is_goto) return rr;
        if (rr.is_break) break;
      }
      return ex;
    }
    case SKind::Local: {
      std::array<Value, MAX_ARGS> rhs{};
      std::size_t outn=0;
      for (std::uint16_t i=0;i<s.r1.n;++i) {
        bool last=(i+1==s.r1.n);
        Multi mv=eval_expr(A.list[s.r1.off+i],env,vargs,last);
        if (last && mv.n>1) for (std::uint8_t k=0;k<mv.n && outn<MAX_ARGS;++k) rhs[outn++]=mv.v[k];
        else rhs[outn++]=first(mv);
      }
      for (std::uint16_t i=0;i<s.r0.n;++i) {
        StrId nm=A.expr[A.list[s.r0.off+i]].s;
        Value iv=(i<outn)? rhs[i] : Value::nil();
        H.env_add(env,nm,iv);
      }
      return ex;
    }
    case SKind::LocalFunc: {
      H.env_add(env,s.name,Value::nil());
      Value fn=first(eval_expr(s.e0,env,vargs,false));
      CellId c=H.env_find(env,s.name);
      H.cells[c.id].v=fn;
      return ex;
    }
    case SKind::Assign: {
      enum class LhsK : std::uint8_t { NameLocal, NameGlobal, IndexLike };
      struct LhsTarget {
        LhsK k{LhsK::NameGlobal};
        CellId cell{UINT32_MAX};
        StrId name{};
        Value obj{};
        Value key{};
      };

      std::array<LhsTarget, MAX_ARGS> lhs{};
      for (std::uint16_t i=0;i<s.r0.n;++i) {
        const Expr& v=A.expr[A.list[s.r0.off+i]];
        LhsTarget t{};
        if (v.k==EKind::Name) {
          CellId c=H.env_find(env,v.s);
          if (c.id!=UINT32_MAX) { t.k=LhsK::NameLocal; t.cell=c; }
          else { t.k=LhsK::NameGlobal; t.name=v.s; }
        } else if (v.k==EKind::Field) {
          t.k=LhsK::IndexLike;
          t.obj=first(eval_expr(v.a,env,vargs,false));
          t.key=Value::string(v.s);
        } else if (v.k==EKind::Index) {
          t.k=LhsK::IndexLike;
          t.obj=first(eval_expr(v.a,env,vargs,false));
          t.key=first(eval_expr(v.b,env,vargs,false));
        } else {
          throw "Lua: invalid assignment target";
        }
        lhs[i]=t;
      }

      std::array<Value, MAX_ARGS> rhs{};
      std::size_t outn=0;
      for (std::uint16_t i=0;i<s.r1.n;++i) {
        bool last=(i+1==s.r1.n);
        Multi mv=eval_expr(A.list[s.r1.off+i],env,vargs,last);
        if (last && mv.n>1) for (std::uint8_t k=0;k<mv.n && outn<MAX_ARGS;++k) rhs[outn++]=mv.v[k];
        else if (outn<MAX_ARGS) rhs[outn++]=first(mv);
      }

      for (std::uint16_t i=0;i<s.r0.n;++i) {
        const LhsTarget& t=lhs[i];
        Value val=(i<outn)? rhs[i] : Value::nil();

        if (t.k==LhsK::NameLocal) {
          H.cells[t.cell.id].v=val;
        } else if (t.k==LhsK::NameGlobal) {
          table_set(H.envs[env.id].env_table,Value::string(t.name),val);
        } else {
          if (t.obj.tag==Tag::Table) table_set(t.obj.t,t.key,val);
          else {
            TableId mt=metatable_of(t.obj);
            Value ni=rawget_mt(mt,s__newindex);
            if (ni.is_nil()) throw "Lua: set index on non-table";
            if (ni.tag==Tag::Table) table_set(ni.t,t.key,val);
            else {
              std::array<Value, 3> args{t.obj, t.key, val};
              (void)call_value(ni,args.data(),3);
            }
          }
        }
      }
      return ex;
    }
    case SKind::ExprStmt: {
      (void)eval_expr(s.e0,env,vargs,false);
      return ex;
    }
    case SKind::FuncStmt: {
      Value fn=first(eval_expr(s.e1,env,vargs,false));
      const Expr& lhs=A.expr[s.e0];
      if (lhs.k==EKind::Name) {
        CellId c=H.env_find(env,lhs.s);
        if (c.id!=UINT32_MAX) H.cells[c.id].v=fn;
        else table_set(H.envs[env.id].env_table,Value::string(lhs.s),fn);
      } else if (lhs.k==EKind::Field) {
        Value obj=first(eval_expr(lhs.a,env,vargs,false));
        Value key=Value::string(lhs.s);
        if (obj.tag==Tag::Table) table_set(obj.t,key,fn);
        else {
          TableId mt=metatable_of(obj);
          Value ni=rawget_mt(mt,s__newindex);
          if (ni.is_nil()) throw "Lua: set field on non-table";
          if (ni.tag==Tag::Table) table_set(ni.t,key,fn);
          else {
            std::array<Value, 3> args{obj, key, fn};
            (void)call_value(ni,args.data(),3);
          }
        }
      } else if (lhs.k==EKind::Index) {
        Value obj=first(eval_expr(lhs.a,env,vargs,false));
        Value key=first(eval_expr(lhs.b,env,vargs,false));
        if (obj.tag==Tag::Table) table_set(obj.t,key,fn);
        else {
          TableId mt=metatable_of(obj);
          Value ni=rawget_mt(mt,s__newindex);
          if (ni.is_nil()) throw "Lua: set index on non-table";
          if (ni.tag==Tag::Table) table_set(ni.t,key,fn);
          else {
            std::array<Value, 3> args{obj, key, fn};
            (void)call_value(ni,args.data(),3);
          }
        }
      } else throw "Lua: invalid function name";
      return ex;
    }
    case SKind::Return: {
      Multi out{};
      if (s.r0.n==0) { out.n=0; }
      else {
        std::uint8_t w=0;
        for (std::uint16_t i=0;i<s.r0.n && w<MAX_RET;++i) {
          bool last=(i+1==s.r0.n);
          Multi mv=eval_expr(A.list[s.r0.off+i],env,vargs,last);
          if (last && mv.n>1) for (std::uint8_t k=0;k<mv.n && w<MAX_RET;++k) out.v[w++]=mv.v[k];
          else out.v[w++]=first(mv);
        }
        out.n=w;
      }
      ex.has_ret=true; ex.ret=out;
      return ex;
    }
    case SKind::Label: return ex;
    case SKind::Goto: ex.is_goto=true; ex.goto_name=s.name; return ex;
    case SKind::Break: ex.is_break=true; return ex;
  }
  throw "Lua: missing stmt kind";
}

consteval void VM::init_runtime(std::uint32_t libs) {
  H.env_count=1;
  H.envs[0]=EnvObj{};
  H.envs[0].outer=EnvId{0};

  s__index   = H.sp.intern("__index");
  s__newindex= H.sp.intern("__newindex");
  s__call    = H.sp.intern("__call");
  s__add     = H.sp.intern("__add");
  s__sub     = H.sp.intern("__sub");
  s__mul     = H.sp.intern("__mul");
  s__div     = H.sp.intern("__div");
  s__idiv    = H.sp.intern("__idiv");
  s__mod     = H.sp.intern("__mod");
  s__pow     = H.sp.intern("__pow");
  s__unm     = H.sp.intern("__unm");
  s__bnot    = H.sp.intern("__bnot");
  s__band    = H.sp.intern("__band");
  s__bor     = H.sp.intern("__bor");
  s__bxor    = H.sp.intern("__bxor");
  s__shl     = H.sp.intern("__shl");
  s__shr     = H.sp.intern("__shr");
  s__len     = H.sp.intern("__len");
  s__concat  = H.sp.intern("__concat");
  s__eq      = H.sp.intern("__eq");
  s__lt      = H.sp.intern("__lt");
  s__le      = H.sp.intern("__le");
  s__pairs   = H.sp.intern("__pairs");
  s__tostring= H.sp.intern("__tostring");
  s__metatable = H.sp.intern("__metatable");
  s__ct_methods = H.sp.intern("__ct_methods");
  s__ct_setters = H.sp.intern("__ct_setters");
  s_new      = H.sp.intern("new");

  G=H.new_table_pow2(8);
  H.envs[0].env_table=G;

  if (libs & LIB_BASE) open_base();
  if (libs & LIB_TABLE) open_table();
  if (libs & LIB_MATH) open_math();
  if (libs & LIB_STRING) open_string();
  if (libs & LIB_UTF8) open_utf8();
  if (libs & LIB_API)  open_api_support();
}

consteval void VM::init(std::uint32_t libs) {
  init_runtime(libs);
  if (libs & LIB_API) bind_api_namespace();
}

constexpr Value VM::compile_chunk(std::string_view src, TableId env_table) {
  Parser P(H,A,src);
  ProtoId mainp=P.parse_chunk();
  std::uint32_t cid=H.new_closure(mainp,EnvId{0},env_table);
  return Value::func_closure(cid);
}

constexpr Multi VM::run_chunk(std::string_view src) {
  Value mainf=compile_chunk(src, G);
  return call_value(mainf,nullptr,0);
}

// ---------------- compile-time user API ----------------
struct RunCapture {
  Value value{};
  std::array<char, MAX_PRINT_BYTES> print{};
  std::size_t print_n{0};
  bool print_truncated{false};
};

template <std::uint32_t Libs = 0u, meta::info... Namespaces>
struct Interpreter {
  static constexpr std::uint32_t libs = Libs;

  template <std::uint32_t MoreLibs>
  consteval auto with_libraries() const -> Interpreter<Libs | MoreLibs, Namespaces...> { return {}; }

  template <std::uint32_t MoreLibs>
  consteval auto with_mask() const -> Interpreter<Libs | MoreLibs, Namespaces...> { return {}; }

  consteval auto with_base() const -> Interpreter<Libs | VM::LIB_BASE, Namespaces...> { return with_libraries<VM::LIB_BASE>(); }
  consteval auto with_api() const -> Interpreter<Libs | VM::LIB_API, Namespaces...> { return with_libraries<VM::LIB_API>(); }
  consteval auto with_table() const -> Interpreter<Libs | VM::LIB_TABLE, Namespaces...> { return with_libraries<VM::LIB_TABLE>(); }
  consteval auto with_math() const -> Interpreter<Libs | VM::LIB_MATH, Namespaces...> { return with_libraries<VM::LIB_MATH>(); }
  consteval auto with_string() const -> Interpreter<Libs | VM::LIB_STRING, Namespaces...> { return with_libraries<VM::LIB_STRING>(); }
  consteval auto with_utf8() const -> Interpreter<Libs | VM::LIB_UTF8, Namespaces...> { return with_libraries<VM::LIB_UTF8>(); }
  consteval auto with_all() const -> Interpreter<Libs | VM::LIB_ALL, Namespaces...> { return with_libraries<VM::LIB_ALL>(); }

  template <meta::info Ns>
  consteval auto with_namespace() const -> Interpreter<Libs | VM::LIB_API, Namespaces..., Ns> { return {}; }

  static consteval void configure(VM& vm) {
    vm.init_runtime(Libs);
    if constexpr (sizeof...(Namespaces) != 0) {
      static_assert((Libs & VM::LIB_API) != 0u, "Lua: interpreter namespace bindings require LIB_API");
      (vm.bind_namespace<Namespaces>(), ...);
    }
  }

  template <fixed_string Script>
  consteval RunCapture run_capture() const {
    VM vm;
    configure(vm);
    Multi r=vm.run_chunk(Script.view());

    RunCapture out{};
    out.value = r.n? r.v[0] : Value::nil();
    out.print_n = vm.print_n;
    out.print_truncated = vm.print_truncated;
    for (std::size_t i=0;i<vm.print_n;++i) out.print[i]=vm.print_buf[i];
    return out;
  }

  template <fixed_string Script>
  consteval RunCapture run() const {
    return run_capture<Script>();
  }

  template <fixed_string Script>
  consteval Value run1() const {
    VM vm;
    configure(vm);
    Multi r=vm.run_chunk(Script.view());
    return r.n? r.v[0] : Value::nil();
  }

  template <fixed_string Script>
  consteval double run_number() const {
    Value v=run1<Script>();
    if (v.tag==Tag::Num) return v.n;
    if (v.tag==Tag::Int) return (double)v.i;
    throw "Lua: expected numeric result";
  }

  template <fixed_string Script>
  static inline void print_buffer() {
    constexpr RunCapture out = Interpreter<Libs, Namespaces...>{}.template run_capture<Script>();
    if (out.print_n) {
      std::cout.write(out.print.data(), static_cast<std::streamsize>(out.print_n));
    }
    if (out.print_truncated) {
      std::cout << "[ct_lua54] print buffer truncated\n";
    }
  }
};

consteval auto interpreter() {
  return Interpreter<>{};
}

inline void print_buffer(const RunCapture& out) {
  if (out.print_n) {
    std::cout.write(out.print.data(), static_cast<std::streamsize>(out.print_n));
  }
  if (out.print_truncated) {
    std::cout << "[ct_lua54] print buffer truncated\n";
  }
}

template <fixed_string Script, std::uint32_t Libs = VM::LIB_BASE>
consteval RunCapture run_capture() = delete;

template <fixed_string Script, std::uint32_t Libs = VM::LIB_BASE>
consteval Value run1() = delete;

template <fixed_string Script, std::uint32_t Libs = VM::LIB_BASE>
consteval double run_number() = delete;

template <fixed_string Script, std::uint32_t Libs = VM::LIB_BASE>
inline void print_buffer() = delete;

// Library masks for interpreter().with_libraries<...>().
inline constexpr std::uint32_t LIB_BASE = VM::LIB_BASE;
inline constexpr std::uint32_t LIB_API  = VM::LIB_API;
inline constexpr std::uint32_t LIB_TABLE= VM::LIB_TABLE;
inline constexpr std::uint32_t LIB_MATH = VM::LIB_MATH;
inline constexpr std::uint32_t LIB_STRING = VM::LIB_STRING;
inline constexpr std::uint32_t LIB_UTF8 = VM::LIB_UTF8;
inline constexpr std::uint32_t LIB_ALL  = VM::LIB_ALL;

} // namespace ct_lua54
