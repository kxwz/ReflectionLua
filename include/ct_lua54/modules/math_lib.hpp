#pragma once

namespace ct_lua54 {

namespace math_detail {
  static constexpr double k_pi     = 3.141592653589793238462643383279502884;
  static constexpr double k_two_pi = 6.283185307179586476925286766559005768;
  static constexpr double k_ln2    = 0.693147180559945309417232121458176568;

  static constexpr bool fits_i64(double x){
    return x >= -9223372036854775808.0 && x <= 9223372036854775807.0;
  }

  static constexpr Value num_or_int(double x){
    if (fits_i64(x)) {
      std::int64_t i=(std::int64_t)x;
      if ((double)i==x) return Value::integer(i);
    }
    return Value::number(x);
  }

  static constexpr std::uint64_t rng_next(VM& vm) {
    std::uint64_t x=vm.rng_state;
    if (x==0) x=0xA4093822299F31D0ull;
    x ^= x >> 12;
    x ^= x << 25;
    x ^= x >> 27;
    vm.rng_state=x;
    return x * 2685821657736338717ull;
  }

  static constexpr double rng_unit(VM& vm) {
    std::uint64_t r=rng_next(vm);
    std::uint64_t m=r>>11; // 53 bits
    return (double)m * (1.0/9007199254740992.0);
  }

  static constexpr double absd(double x){ return (x<0.0)? -x : x; }

  static constexpr double ceild(double x){
    double f=VM::floor_num(x);
    return (f<x)? (f+1.0) : f;
  }

  static constexpr double sqrtd(double x){
    if (x<0.0) throw "Lua: math.sqrt domain error";
    if (x==0.0) return 0.0;
    double g=(x>1.0)? x : 1.0;
    for (int i=0;i<40;++i) g=0.5*(g + x/g);
    return g;
  }

  static constexpr double lnd(double x){
    if (x<=0.0) throw "Lua: math.log domain error";
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
    return 2.0*s + (double)k*k_ln2;
  }

  static constexpr double wrap_pi(double x){
    while (x>k_pi) x-=k_two_pi;
    while (x<-k_pi) x+=k_two_pi;
    return x;
  }

  static constexpr double sind(double x){
    x=wrap_pi(x);
    double term=x;
    double s=x;
    double xx=x*x;
    for (int n=1;n<12;++n) {
      double d=(double)((2*n)*(2*n+1));
      term *= -xx/d;
      s += term;
    }
    return s;
  }

  static constexpr double cosd(double x){
    x=wrap_pi(x);
    double term=1.0;
    double s=1.0;
    double xx=x*x;
    for (int n=1;n<12;++n) {
      double d=(double)((2*n-1)*(2*n));
      term *= -xx/d;
      s += term;
    }
    return s;
  }

  static constexpr double tand(double x){
    double c=cosd(x);
    if (absd(c) < 1e-15) throw "Lua: math.tan singularity";
    return sind(x)/c;
  }
}

constexpr Multi VM::nf_math_floor(VM&, const Value* a, std::size_t n) {
  if (n<1) throw "Lua: math.floor(x)";
  if (a[0].tag==Tag::Int) return Multi::one(a[0]);
  double x=to_num(a[0]);
  return Multi::one(math_detail::num_or_int(floor_num(x)));
}

constexpr Multi VM::nf_math_ceil(VM&, const Value* a, std::size_t n) {
  if (n<1) throw "Lua: math.ceil(x)";
  if (a[0].tag==Tag::Int) return Multi::one(a[0]);
  double x=to_num(a[0]);
  return Multi::one(math_detail::num_or_int(math_detail::ceild(x)));
}

constexpr Multi VM::nf_math_abs(VM&, const Value* a, std::size_t n) {
  if (n<1) throw "Lua: math.abs(x)";
  if (a[0].tag==Tag::Int) {
    if (a[0].i==(-9223372036854775807ll-1ll)) return Multi::one(Value::number(9223372036854775808.0));
    return Multi::one(Value::integer((a[0].i<0)? -a[0].i : a[0].i));
  }
  return Multi::one(Value::number(math_detail::absd(to_num(a[0]))));
}

constexpr Multi VM::nf_math_min(VM&, const Value* a, std::size_t n) {
  if (n<1) throw "Lua: math.min expects at least 1 argument";
  double m=to_num(a[0]);
  for (std::size_t i=1;i<n;++i) {
    double x=to_num(a[i]);
    if (x<m) m=x;
  }
  return Multi::one(math_detail::num_or_int(m));
}

constexpr Multi VM::nf_math_max(VM&, const Value* a, std::size_t n) {
  if (n<1) throw "Lua: math.max expects at least 1 argument";
  double m=to_num(a[0]);
  for (std::size_t i=1;i<n;++i) {
    double x=to_num(a[i]);
    if (x>m) m=x;
  }
  return Multi::one(math_detail::num_or_int(m));
}

constexpr Multi VM::nf_math_random(VM& vm, const Value* a, std::size_t n) {
  if (n==0) {
    return Multi::one(Value::number(math_detail::rng_unit(vm)));
  }
  if (n==1) {
    std::int64_t u=to_int(a[0]);
    if (u<1) throw "Lua: math.random upper bound must be >= 1";
    std::uint64_t r=math_detail::rng_next(vm);
    std::uint64_t out=(r % (std::uint64_t)u) + 1u;
    return Multi::one(Value::integer((std::int64_t)out));
  }
  if (n==2) {
    std::int64_t l=to_int(a[0]);
    std::int64_t u=to_int(a[1]);
    if (l>u) throw "Lua: math.random interval is empty";
    std::uint64_t lu=(std::uint64_t)l;
    std::uint64_t uu=(std::uint64_t)u;
    std::uint64_t span=uu - lu + 1u;
    std::uint64_t r=math_detail::rng_next(vm);
    std::uint64_t out=lu + (span ? (r % span) : r);
    return Multi::one(Value::integer((std::int64_t)out));
  }
  throw "Lua: math.random([m [,n]])";
}

constexpr Multi VM::nf_math_randomseed(VM& vm, const Value* a, std::size_t n) {
  std::uint64_t x=0xC2B2AE3D27D4EB4Full;
  std::uint64_t y=0x165667B19E3779F9ull;
  if (n>=1) x=(std::uint64_t)to_int(a[0]);
  if (n>=2) y=(std::uint64_t)to_int(a[1]);
  if (n>2) throw "Lua: math.randomseed([x [,y]])";
  std::uint64_t s=x;
  s ^= y + 0x9E3779B97F4A7C15ull + (s<<6) + (s>>2);
  if (s==0) s=0xA4093822299F31D0ull;
  vm.rng_state=s;
  return Multi::none();
}

constexpr Multi VM::nf_math_sin(VM&, const Value* a, std::size_t n) {
  if (n<1) throw "Lua: math.sin(x)";
  return Multi::one(Value::number(math_detail::sind(to_num(a[0]))));
}

constexpr Multi VM::nf_math_cos(VM&, const Value* a, std::size_t n) {
  if (n<1) throw "Lua: math.cos(x)";
  return Multi::one(Value::number(math_detail::cosd(to_num(a[0]))));
}

constexpr Multi VM::nf_math_tan(VM&, const Value* a, std::size_t n) {
  if (n<1) throw "Lua: math.tan(x)";
  return Multi::one(Value::number(math_detail::tand(to_num(a[0]))));
}

constexpr Multi VM::nf_math_sqrt(VM&, const Value* a, std::size_t n) {
  if (n<1) throw "Lua: math.sqrt(x)";
  return Multi::one(Value::number(math_detail::sqrtd(to_num(a[0]))));
}

constexpr Multi VM::nf_math_log(VM&, const Value* a, std::size_t n) {
  if (n<1 || n>2) throw "Lua: math.log(x [,base])";
  double lx=math_detail::lnd(to_num(a[0]));
  if (n==1) return Multi::one(Value::number(lx));
  double b=to_num(a[1]);
  if (b<=0.0 || b==1.0) throw "Lua: math.log invalid base";
  return Multi::one(Value::number(lx / math_detail::lnd(b)));
}

consteval void VM::open_math() {
  TableId m=H.new_table_pow2(6);

  auto reg = [&](std::string_view name, NativeFn fn) {
    std::uint32_t id=reg_native(name, fn);
    table_set(m, Value::string(H.sp.intern(name)), mk_native(id));
  };

  reg("floor", &VM::nf_math_floor);
  reg("ceil",  &VM::nf_math_ceil);
  reg("abs",   &VM::nf_math_abs);
  reg("min",   &VM::nf_math_min);
  reg("max",   &VM::nf_math_max);
  reg("random", &VM::nf_math_random);
  reg("randomseed", &VM::nf_math_randomseed);
  reg("sin",   &VM::nf_math_sin);
  reg("cos",   &VM::nf_math_cos);
  reg("tan",   &VM::nf_math_tan);
  reg("sqrt",  &VM::nf_math_sqrt);
  reg("log",   &VM::nf_math_log);

  table_set(m, Value::string(H.sp.intern("pi")),   Value::number(math_detail::k_pi));
  table_set(m, Value::string(H.sp.intern("huge")), Value::number(1e308));

  table_set(G, Value::string(H.sp.intern("math")), Value::table(m));
}

} // namespace ct_lua54
