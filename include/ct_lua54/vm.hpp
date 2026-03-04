#pragma once

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
  StrId s__unm{}, s__len{}, s__concat{}, s__eq{}, s__lt{}, s__le{};

  std::size_t steps{0};
  constexpr void tick(){ if (++steps > STEP_LIMIT) throw "Lua: step limit exceeded"; }
  std::uint64_t rng_state{0x9E3779B97F4A7C15ull};

  struct VarArgs { const Value* p{nullptr}; std::size_t n{0}; };
  std::array<Value, MAX_ARGS> tmp_args{};

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

  static constexpr double floor_num(double x){
    std::int64_t i=(std::int64_t)x;
    double d=(double)i;
    if (d>x) d-=1.0;
    return d;
  }

  static constexpr bool as_exact_i64(double x, std::int64_t& out){
    if (!(x==x)) return false;
    if (x < -9223372036854775808.0 || x > 9223372036854775807.0) return false;
    std::int64_t i=(std::int64_t)x;
    if ((double)i != x) return false;
    out=i;
    return true;
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
    tmp_args[0]=Value::table(t); tmp_args[1]=key;
    return first(call_value(idx,tmp_args.data(),2));
  }

  constexpr void table_set(TableId t, const Value& key, const Value& val) {
    Value cur=H.rawget(t,key);
    TableId mt=H.tables[t.id].mt;
    if (!cur.is_nil() || mt.id==0) { H.rawset(t,key,val); return; }
    Value ni=rawget_mt(mt,s__newindex);
    if (ni.is_nil()) { H.rawset(t,key,val); return; }
    if (ni.tag==Tag::Table) { table_set(ni.t,key,val); return; }
    tmp_args[0]=Value::table(t); tmp_args[1]=key; tmp_args[2]=val;
    (void)call_value(ni,tmp_args.data(),3);
  }

  constexpr Value meta_bin(const Value& a, const Value& b, StrId mm, const char* err){
    Value mma=rawget_mt(metatable_of(a),mm);
    if (!mma.is_nil()) { tmp_args[0]=a; tmp_args[1]=b; return first(call_value(mma,tmp_args.data(),2)); }
    Value mmb=rawget_mt(metatable_of(b),mm);
    if (!mmb.is_nil()) { tmp_args[0]=a; tmp_args[1]=b; return first(call_value(mmb,tmp_args.data(),2)); }
    throw err;
  }

  constexpr bool v_eq(const Value& a, const Value& b){
    if (a.tag!=b.tag) {
      if (a.tag==Tag::Int && b.tag==Tag::Num) return (double)a.i==b.n;
      if (a.tag==Tag::Num && b.tag==Tag::Int) return a.n==(double)b.i;
      return false;
    }
    switch (a.tag) {
      case Tag::Nil: return true;
      case Tag::Bool:return a.b==b.b;
      case Tag::Int: return a.i==b.i;
      case Tag::Num: return a.n==b.n;
      case Tag::Str: return a.s.id==b.s.id;
      case Tag::Table:
      case Tag::UData:
      case Tag::Func: {
        Value mmv=rawget_mt(metatable_of(a),s__eq);
        if (!mmv.is_nil()) { tmp_args[0]=a; tmp_args[1]=b; return truthy(first(call_value(mmv,tmp_args.data(),2))); }
        if (a.tag==Tag::Table) return a.t.id==b.t.id;
        if (a.tag==Tag::UData) return a.u.id==b.u.id;
        return a.f.id==b.f.id && a.f.is_native==b.f.is_native;
      }
    }
    return false;
  }

  constexpr bool v_lt(const Value& a, const Value& b){
    if (is_number(a)&&is_number(b)) return to_num(a)<to_num(b);
    if (a.tag==Tag::Str && b.tag==Tag::Str) return H.sp.view(a.s) < H.sp.view(b.s);
    Value mmv=rawget_mt(metatable_of(a),s__lt);
    if (mmv.is_nil()) mmv=rawget_mt(metatable_of(b),s__lt);
    if (!mmv.is_nil()) { tmp_args[0]=a; tmp_args[1]=b; return truthy(first(call_value(mmv,tmp_args.data(),2))); }
    throw "Lua: attempt to compare";
  }

  constexpr bool v_le(const Value& a, const Value& b){
    if (is_number(a)&&is_number(b)) return to_num(a)<=to_num(b);
    if (a.tag==Tag::Str && b.tag==Tag::Str) return !(H.sp.view(b.s) < H.sp.view(a.s));
    Value mmv=rawget_mt(metatable_of(a),s__le);
    if (mmv.is_nil()) mmv=rawget_mt(metatable_of(b),s__le);
    if (!mmv.is_nil()) { tmp_args[0]=a; tmp_args[1]=b; return truthy(first(call_value(mmv,tmp_args.data(),2))); }
    return !v_lt(b,a);
  }

  constexpr StrId int_to_string(std::int64_t x){
    std::array<char, 32> buf{};
    std::size_t w=0;
    std::int64_t v=x;
    bool neg=v<0; if (neg) v=-v;
    std::array<char, 32> rev{};
    std::size_t p=0;
    do { rev[p++]=char('0'+(v%10)); v/=10; } while (v && p<rev.size());
    if (neg) buf[w++]='-';
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
    if (!mmv.is_nil()) { tmp_args[0]=a; return first(call_value(mmv,tmp_args.data(),1)); }
    if (a.tag==Tag::Table) {
      std::int64_t n=0;
      for (std::int64_t k=1;;++k){ if (table_get(a.t, Value::integer(k)).is_nil()) break; n=k; }
      return Value::integer(n);
    }
    throw "Lua: length of unsupported type";
  }

  // reflection wrapper for api free functions
  template <class> struct fn_traits;
  template <class R, class... Args>
  struct fn_traits<R(Args...)> { using ret=R; using args_tuple=std::tuple<Args...>; static constexpr std::size_t arity=sizeof...(Args); };

  template <class T>
  static constexpr T arg_as(VM&, const Value& v) {
    if constexpr (std::is_same_v<T,double>) return to_num(v);
    else if constexpr (std::is_same_v<T,bool>) { if (v.tag!=Tag::Bool) throw "Lua: expected boolean"; return v.b; }
    else throw "Lua: unsupported native arg type";
  }

  template <class R>
  static constexpr Multi ret_as(VM&, R r) {
    if constexpr (std::is_void_v<R>) return Multi::none();
    else if constexpr (std::is_same_v<R,double>) return Multi::one(Value::number(r));
    else if constexpr (std::is_same_v<R,bool>) return Multi::one(Value::boolean(r));
    else throw "Lua: unsupported native return";
  }

  template <meta::info F>
  static constexpr Multi call_wrapped(VM& vm, const Value* args, std::size_t argc) {
    using FnRef  = decltype(([:F:]));
    using FnType = std::remove_reference_t<FnRef>;
    using Tr     = fn_traits<FnType>;
    using R      = typename Tr::ret;
    if (argc != Tr::arity) throw "Lua: arity mismatch (native)";
    auto inv = [&]<std::size_t...Is>(std::index_sequence<Is...>) -> Multi {
      if constexpr (std::is_void_v<R>) {
        [:F:]( arg_as<std::tuple_element_t<Is, typename Tr::args_tuple>>(vm, args[Is])... );
        return Multi::none();
      } else {
        R r = [:F:]( arg_as<std::tuple_element_t<Is, typename Tr::args_tuple>>(vm, args[Is])... );
        return ret_as<R>(vm, r);
      }
    };
    return inv(std::make_index_sequence<Tr::arity>{});
  }

  constexpr std::uint32_t reg_native(std::string_view name, NativeFn f) {
    if (native_count >= natives.size()) throw "Lua: too many natives";
    native_names[native_count] = H.sp.intern(name);
    natives[native_count] = f;
    return native_count++;
  }
  constexpr Value mk_native(std::uint32_t id){ return Value::func_native(id); }

  // --- base natives ---
  static constexpr Multi nf_type(VM& vm, const Value* a, std::size_t n);
  static constexpr Multi nf_setmetatable(VM& vm, const Value* a, std::size_t n);
  static constexpr Multi nf_getmetatable(VM& vm, const Value* a, std::size_t n);
  static constexpr Multi nf_rawget(VM& vm, const Value* a, std::size_t n);
  static constexpr Multi nf_rawset(VM& vm, const Value* a, std::size_t n);
  static constexpr Multi nf_next(VM& vm, const Value* a, std::size_t n);
  static constexpr Multi nf_pairs(VM& vm, const Value* a, std::size_t n);

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
  static constexpr Multi nf_string_gsub(VM& vm, const Value* a, std::size_t n);
  static constexpr Multi nf_string_byte(VM& vm, const Value* a, std::size_t n);
  static constexpr Multi nf_string_char(VM& vm, const Value* a, std::size_t n);
  static constexpr Multi nf_string_upper(VM& vm, const Value* a, std::size_t n);
  static constexpr Multi nf_string_lower(VM& vm, const Value* a, std::size_t n);
  static constexpr Multi nf_string_rep(VM& vm, const Value* a, std::size_t n);
  static constexpr Multi nf_string_reverse(VM& vm, const Value* a, std::size_t n);
  static constexpr Multi nf_string_format(VM& vm, const Value* a, std::size_t n);

  std::uint32_t id_next{0};

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

  template <meta::info F>
  consteval void bind_one_api() {
    static_assert(meta::is_function(F) && meta::has_identifier(F), "api must contain only free functions");
    constexpr std::string_view nm = meta::identifier_of(F);
    std::uint32_t id = reg_native(nm, &api_tramp<F>);
    table_set(G, Value::string(H.sp.intern(nm)), mk_native(id));
  }

  template <std::size_t I>
  consteval void bind_api_rec() {
    constexpr auto ctx = meta::access_context::current();
    static constexpr auto mems = std::define_static_array(meta::members_of(^^api, ctx));
    if constexpr (I < mems.size()) {
      bind_one_api<mems[I]>();
      bind_api_rec<I+1>();
    }
  }

  consteval void bind_api_namespace() { bind_api_rec<0>(); }

  // init/run
  static constexpr std::uint32_t LIB_BASE = 1u << 0;
  static constexpr std::uint32_t LIB_API  = 1u << 1;
  static constexpr std::uint32_t LIB_TABLE= 1u << 2;
  static constexpr std::uint32_t LIB_MATH = 1u << 3;
  static constexpr std::uint32_t LIB_STRING = 1u << 4;
  static constexpr std::uint32_t LIB_ALL  = LIB_BASE | LIB_API | LIB_TABLE | LIB_MATH | LIB_STRING;

  consteval void open_base();
  consteval void open_table();
  consteval void open_math();
  consteval void open_string();
  consteval void open_api();
  consteval void init(std::uint32_t libs);
  consteval void init() { init(LIB_BASE); }
  constexpr Multi run_chunk(std::string_view src);
};

// ---- VM impl ----
constexpr Multi VM::call_value(const Value& callee, const Value* args, std::size_t argc) {
  tick();
  if (callee.tag==Tag::Func && callee.f.is_native) {
    auto id=callee.f.id;
    if (id>=native_count) throw "Lua: bad native id";
    return natives[id](*this,args,argc);
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
      return eval_expr(e.a,env,vargs,multret);
    case EKind::Unary: {
      Value x=first(eval_expr(e.a,env,vargs,false));
      if (e.op==TK::Not) return Multi::one(Value::boolean(!truthy(x)));
      if (e.op==TK::Minus) {
        if (x.tag==Tag::Int) return Multi::one(Value::integer(-x.i));
        if (x.tag==Tag::Num) return Multi::one(Value::number(-x.n));
        return Multi::one(meta_bin(x,Value::nil(),s__unm,"Lua: unary minus"));
      }
      if (e.op==TK::BitXor) return Multi::one(Value::integer(bit_not(to_int(x))));
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
        case TK::BitAnd:
          return Multi::one(Value::integer(bit_and(to_int(a),to_int(b))));
        case TK::BitOr:
          return Multi::one(Value::integer(bit_or(to_int(a),to_int(b))));
        case TK::BitXor:
          return Multi::one(Value::integer(bit_xor(to_int(a),to_int(b))));
        case TK::Shl:
          return Multi::one(Value::integer(shift_left(to_int(a),to_int(b))));
        case TK::Shr:
          return Multi::one(Value::integer(shift_right(to_int(a),to_int(b))));
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
      tmp_args[0]=obj; tmp_args[1]=key;
      return Multi::one(first(call_value(idx,tmp_args.data(),2)));
    }
    case EKind::Field: {
      Value obj=first(eval_expr(e.a,env,vargs,false));
      Value key=Value::string(e.s);
      if (obj.tag==Tag::Table) return Multi::one(table_get(obj.t,key));
      TableId mt=metatable_of(obj);
      Value idx=rawget_mt(mt,s__index);
      if (idx.is_nil()) throw "Lua: field on non-table";
      if (idx.tag==Tag::Table) return Multi::one(table_get(idx.t,key));
      tmp_args[0]=obj; tmp_args[1]=key;
      return Multi::one(first(call_value(idx,tmp_args.data(),2)));
    }
    case EKind::Call: {
      Value fn=first(eval_expr(e.a,env,vargs,false));
      std::size_t argc=0;
      for (std::uint16_t i=0;i<e.r.n;++i) {
        bool last=(i+1==e.r.n);
        Multi mv=eval_expr(A.list[e.r.off+i],env,vargs,last);
        if (last && mv.n>1) for (std::uint8_t k=0;k<mv.n && argc<MAX_ARGS;++k) tmp_args[argc++]=mv.v[k];
        else tmp_args[argc++]=first(mv);
      }
      Multi ret=call_value(fn,tmp_args.data(),argc);
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
        else { tmp_args[0]=obj; tmp_args[1]=Value::string(e.s); mfn=first(call_value(idx,tmp_args.data(),2)); }
      }
      std::size_t argc=0;
      tmp_args[argc++]=obj;
      for (std::uint16_t i=0;i<e.r.n;++i) {
        bool last=(i+1==e.r.n);
        Multi mv=eval_expr(A.list[e.r.off+i],env,vargs,last);
        if (last && mv.n>1) for (std::uint8_t k=0;k<mv.n && argc<MAX_ARGS;++k) tmp_args[argc++]=mv.v[k];
        else tmp_args[argc++]=first(mv);
      }
      Multi ret=call_value(mfn,tmp_args.data(),argc);
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
    StmtId sid = A.blist[blk.off + pc];
    Exec r=exec_stmt(A.st[sid],env,vargs);
    if (r.has_ret || r.is_break) return r;
    if (r.is_goto) {
      bool found=false;
      for (std::uint16_t k=0;k<nlabels;++k) {
        if (labels[k].id==r.goto_name.id) {
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
        tmp_args[0]=st; tmp_args[1]=var;
        Multi r=call_value(f,tmp_args.data(),2);
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
      std::array<Value, MAX_ARGS> rhs{};
      std::size_t outn=0;
      for (std::uint16_t i=0;i<s.r1.n;++i) {
        bool last=(i+1==s.r1.n);
        Multi mv=eval_expr(A.list[s.r1.off+i],env,vargs,last);
        if (last && mv.n>1) for (std::uint8_t k=0;k<mv.n && outn<MAX_ARGS;++k) rhs[outn++]=mv.v[k];
        else rhs[outn++]=first(mv);
      }

      for (std::uint16_t i=0;i<s.r0.n;++i) {
        const Expr& v=A.expr[A.list[s.r0.off+i]];
        Value val=(i<outn)? rhs[i] : Value::nil();

        if (v.k==EKind::Name) {
          CellId c=H.env_find(env,v.s);
          if (c.id!=UINT32_MAX) H.cells[c.id].v=val;
          else table_set(H.envs[env.id].env_table,Value::string(v.s),val);
        } else if (v.k==EKind::Field) {
          Value obj=first(eval_expr(v.a,env,vargs,false));
          Value key=Value::string(v.s);
          if (obj.tag==Tag::Table) table_set(obj.t,key,val);
          else {
            TableId mt=metatable_of(obj);
            Value ni=rawget_mt(mt,s__newindex);
            if (ni.is_nil()) throw "Lua: set field on non-table";
            if (ni.tag==Tag::Table) table_set(ni.t,key,val);
            else { tmp_args[0]=obj; tmp_args[1]=key; tmp_args[2]=val; (void)call_value(ni,tmp_args.data(),3); }
          }
        } else if (v.k==EKind::Index) {
          Value obj=first(eval_expr(v.a,env,vargs,false));
          Value key=first(eval_expr(v.b,env,vargs,false));
          if (obj.tag==Tag::Table) table_set(obj.t,key,val);
          else {
            TableId mt=metatable_of(obj);
            Value ni=rawget_mt(mt,s__newindex);
            if (ni.is_nil()) throw "Lua: set index on non-table";
            if (ni.tag==Tag::Table) table_set(ni.t,key,val);
            else { tmp_args[0]=obj; tmp_args[1]=key; tmp_args[2]=val; (void)call_value(ni,tmp_args.data(),3); }
          }
        } else throw "Lua: invalid assignment target";
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
          else { tmp_args[0]=obj; tmp_args[1]=key; tmp_args[2]=fn; (void)call_value(ni,tmp_args.data(),3); }
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
          else { tmp_args[0]=obj; tmp_args[1]=key; tmp_args[2]=fn; (void)call_value(ni,tmp_args.data(),3); }
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

consteval void VM::init(std::uint32_t libs) {
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
  s__len     = H.sp.intern("__len");
  s__concat  = H.sp.intern("__concat");
  s__eq      = H.sp.intern("__eq");
  s__lt      = H.sp.intern("__lt");
  s__le      = H.sp.intern("__le");

  G=H.new_table_pow2(8);
  H.envs[0].env_table=G;

  if (libs & LIB_BASE) open_base();
  if (libs & LIB_TABLE) open_table();
  if (libs & LIB_MATH) open_math();
  if (libs & LIB_STRING) open_string();
  if (libs & LIB_API)  open_api();
}

constexpr Multi VM::run_chunk(std::string_view src) {
  Parser P(H,A,src);
  ProtoId mainp=P.parse_chunk();
  std::uint32_t cid=H.new_closure(mainp,EnvId{0},G);
  Value mainf=Value::func_closure(cid);
  return call_value(mainf,nullptr,0);
}

// ---------------- compile-time user API ----------------
template <fixed_string Script, std::uint32_t Libs = VM::LIB_BASE>
consteval Value run1() {
  VM vm;
  vm.init(Libs);
  Multi r=vm.run_chunk(Script.view());
  return r.n? r.v[0] : Value::nil();
}

template <fixed_string Script, std::uint32_t Libs = VM::LIB_BASE>
consteval double run_number() {
  Value v=run1<Script, Libs>();
  if (v.tag==Tag::Num) return v.n;
  if (v.tag==Tag::Int) return (double)v.i;
  throw "Lua: expected numeric result";
}

// Library masks for run1/run_number, e.g. run_number<script, LIB_ALL>().
inline constexpr std::uint32_t LIB_BASE = VM::LIB_BASE;
inline constexpr std::uint32_t LIB_API  = VM::LIB_API;
inline constexpr std::uint32_t LIB_TABLE= VM::LIB_TABLE;
inline constexpr std::uint32_t LIB_MATH = VM::LIB_MATH;
inline constexpr std::uint32_t LIB_STRING = VM::LIB_STRING;
inline constexpr std::uint32_t LIB_ALL  = VM::LIB_ALL;

} // namespace ct_lua54
