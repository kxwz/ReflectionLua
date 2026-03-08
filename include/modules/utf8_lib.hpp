#pragma once

namespace ct_lua54 {

namespace utf8_detail {
  static constexpr std::string_view arg_to_sv(VM& vm, const Value& v, const char* err){
    if (v.tag!=Tag::Str) throw err;
    return vm.H.sp.view(v.s);
  }

  static constexpr std::int64_t posrelat(std::int64_t pos, std::int64_t len){
    if (pos<0) pos += len + 1;
    return pos;
  }

  static constexpr bool is_cont(unsigned char b){
    return (b & 0xC0u) == 0x80u;
  }

  static constexpr bool decode_one(std::string_view s, std::size_t pos, std::size_t limit, std::uint32_t& cp, std::size_t& next){
    if (pos>=limit) return false;
    unsigned char b0=(unsigned char)s[pos];
    if (b0<=0x7Fu) {
      cp=(std::uint32_t)b0;
      next=pos+1;
      return true;
    }

    if (b0>=0xC2u && b0<=0xDFu) {
      if (pos+1>=limit) return false;
      unsigned char b1=(unsigned char)s[pos+1];
      if (!is_cont(b1)) return false;
      cp=((std::uint32_t)(b0&0x1Fu)<<6) | (std::uint32_t)(b1&0x3Fu);
      next=pos+2;
      return true;
    }

    if (b0>=0xE0u && b0<=0xEFu) {
      if (pos+2>=limit) return false;
      unsigned char b1=(unsigned char)s[pos+1];
      unsigned char b2=(unsigned char)s[pos+2];
      if (!is_cont(b2)) return false;
      if (b0==0xE0u) {
        if (b1<0xA0u || b1>0xBFu) return false;
      } else if (b0==0xEDu) {
        if (b1<0x80u || b1>0x9Fu) return false;
      } else if (!is_cont(b1)) {
        return false;
      }
      cp=((std::uint32_t)(b0&0x0Fu)<<12) |
         ((std::uint32_t)(b1&0x3Fu)<<6) |
         (std::uint32_t)(b2&0x3Fu);
      next=pos+3;
      return true;
    }

    if (b0>=0xF0u && b0<=0xF4u) {
      if (pos+3>=limit) return false;
      unsigned char b1=(unsigned char)s[pos+1];
      unsigned char b2=(unsigned char)s[pos+2];
      unsigned char b3=(unsigned char)s[pos+3];
      if (!is_cont(b2) || !is_cont(b3)) return false;
      if (b0==0xF0u) {
        if (b1<0x90u || b1>0xBFu) return false;
      } else if (b0==0xF4u) {
        if (b1<0x80u || b1>0x8Fu) return false;
      } else if (!is_cont(b1)) {
        return false;
      }
      cp=((std::uint32_t)(b0&0x07u)<<18) |
         ((std::uint32_t)(b1&0x3Fu)<<12) |
         ((std::uint32_t)(b2&0x3Fu)<<6) |
         (std::uint32_t)(b3&0x3Fu);
      next=pos+4;
      return true;
    }

    return false;
  }

  static constexpr void encode_one(std::uint32_t cp, std::array<char, 8192>& out, std::size_t& w){
    if (cp<=0x7Fu) {
      if (w+1>out.size()) throw "Lua: utf8.char result too long";
      out[w++]=(char)cp;
      return;
    }
    if (cp<=0x7FFu) {
      if (w+2>out.size()) throw "Lua: utf8.char result too long";
      out[w++]=(char)(0xC0u | ((cp>>6)&0x1Fu));
      out[w++]=(char)(0x80u | (cp&0x3Fu));
      return;
    }
    if (cp>=0xD800u && cp<=0xDFFFu) throw "Lua: utf8.char value out of range";
    if (cp<=0xFFFFu) {
      if (w+3>out.size()) throw "Lua: utf8.char result too long";
      out[w++]=(char)(0xE0u | ((cp>>12)&0x0Fu));
      out[w++]=(char)(0x80u | ((cp>>6)&0x3Fu));
      out[w++]=(char)(0x80u | (cp&0x3Fu));
      return;
    }
    if (cp<=0x10FFFFu) {
      if (w+4>out.size()) throw "Lua: utf8.char result too long";
      out[w++]=(char)(0xF0u | ((cp>>18)&0x07u));
      out[w++]=(char)(0x80u | ((cp>>12)&0x3Fu));
      out[w++]=(char)(0x80u | ((cp>>6)&0x3Fu));
      out[w++]=(char)(0x80u | (cp&0x3Fu));
      return;
    }
    throw "Lua: utf8.char value out of range";
  }
}

constexpr Multi VM::nf_utf8_char(VM& vm, const Value* a, std::size_t n) {
  std::array<char, 8192> out{};
  std::size_t w=0;
  for (std::size_t i=0;i<n;++i) {
    std::int64_t iv=to_int(a[i]);
    if (iv<0) throw "Lua: utf8.char value out of range";
    utf8_detail::encode_one((std::uint32_t)iv, out, w);
  }
  return Multi::one(Value::string(vm.H.sp.intern(std::string_view(out.data(), w))));
}

constexpr Multi VM::nf_utf8_codepoint(VM& vm, const Value* a, std::size_t n) {
  if (n<1 || n>3) throw "Lua: utf8.codepoint(s [,i [,j]])";
  auto s=utf8_detail::arg_to_sv(vm,a[0],"Lua: utf8.codepoint expects string");
  std::int64_t len=(std::int64_t)s.size();
  std::int64_t i=utf8_detail::posrelat((n>=2)?to_int(a[1]):1, len);
  std::int64_t j=utf8_detail::posrelat((n>=3)?to_int(a[2]):i, len);
  if (i<1 || j<1 || i>len || j>len) throw "Lua: utf8.codepoint out of range";
  if (i>j) return Multi::none();

  std::size_t pos=(std::size_t)(i-1);
  std::size_t limit=(std::size_t)j;
  Multi m{};
  std::uint8_t w=0;
  while (pos<limit && w<MAX_RET) {
    std::uint32_t cp=0;
    std::size_t next=0;
    if (!utf8_detail::decode_one(s,pos,limit,cp,next)) throw "Lua: invalid UTF-8 code";
    m.v[w++]=Value::integer((std::int64_t)cp);
    pos=next;
  }
  if (pos!=limit) throw "Lua: invalid UTF-8 code";
  m.n=w;
  return m;
}

constexpr Multi VM::nf_utf8_codes_iter(VM& vm, const Value* a, std::size_t n) {
  if (n<1 || a[0].tag!=Tag::Str) throw "Lua: utf8.codes iterator state";
  auto s=vm.H.sp.view(a[0].s);
  std::size_t len=s.size();
  std::size_t pos=0;

  if (n>=2 && !a[1].is_nil()) {
    std::int64_t prev=to_int(a[1]);
    if (prev<1 || (std::size_t)prev>len) return Multi::one(Value::nil());
    std::size_t p0=(std::size_t)(prev-1);
    std::uint32_t c0=0;
    std::size_t n0=0;
    if (!utf8_detail::decode_one(s,p0,len,c0,n0)) throw "Lua: invalid UTF-8 code";
    pos=n0;
  }

  if (pos>=len) return Multi::one(Value::nil());
  std::uint32_t cp=0;
  std::size_t next=0;
  if (!utf8_detail::decode_one(s,pos,len,cp,next)) throw "Lua: invalid UTF-8 code";
  Multi m{}; m.n=2;
  m.v[0]=Value::integer((std::int64_t)pos+1);
  m.v[1]=Value::integer((std::int64_t)cp);
  return m;
}

constexpr Multi VM::nf_utf8_codes(VM& vm, const Value* a, std::size_t n) {
  if (n<1 || a[0].tag!=Tag::Str) throw "Lua: utf8.codes(s)";
  Multi m{}; m.n=3;
  m.v[0]=vm.mk_native(vm.id_utf8_codes_iter);
  m.v[1]=a[0];
  m.v[2]=Value::nil();
  return m;
}

constexpr Multi VM::nf_utf8_len(VM& vm, const Value* a, std::size_t n) {
  if (n<1 || n>3) throw "Lua: utf8.len(s [,i [,j]])";
  auto s=utf8_detail::arg_to_sv(vm,a[0],"Lua: utf8.len expects string");
  std::int64_t len=(std::int64_t)s.size();
  std::int64_t i=utf8_detail::posrelat((n>=2)?to_int(a[1]):1, len);
  std::int64_t j=utf8_detail::posrelat((n>=3)?to_int(a[2]):-1, len);
  if (i<1) i=1;
  if (j>len) j=len;
  if (i>j) return Multi::one(Value::integer(0));

  std::size_t pos=(std::size_t)(i-1);
  std::size_t limit=(std::size_t)j;
  std::int64_t count=0;
  while (pos<limit) {
    std::uint32_t cp=0;
    std::size_t next=0;
    if (!utf8_detail::decode_one(s,pos,limit,cp,next)) {
      Multi m{}; m.n=2;
      m.v[0]=Value::nil();
      m.v[1]=Value::integer((std::int64_t)pos+1);
      return m;
    }
    ++count;
    pos=next;
  }
  return Multi::one(Value::integer(count));
}

constexpr Multi VM::nf_utf8_offset(VM& vm, const Value* a, std::size_t n) {
  if (n<2 || n>3) throw "Lua: utf8.offset(s, n [,i])";
  auto s=utf8_detail::arg_to_sv(vm,a[0],"Lua: utf8.offset expects string");
  std::int64_t step=to_int(a[1]);
  std::int64_t len=(std::int64_t)s.size();
  std::int64_t i=(n>=3)? to_int(a[2]) : (step>=0? 1 : len+1);
  if (i<0) i += len + 1;
  if (i<1 || i>len+1) return Multi::one(Value::nil());

  auto is_cont_at = [&](std::size_t p) constexpr -> bool {
    return p<s.size() && utf8_detail::is_cont((unsigned char)s[p]);
  };

  std::size_t pos=(std::size_t)(i-1);

  if (step==0) {
    if (pos==s.size()) return Multi::one(Value::integer((std::int64_t)s.size()+1));
    while (pos>0 && is_cont_at(pos)) --pos;
    std::uint32_t cp=0; std::size_t next=0;
    if (!utf8_detail::decode_one(s,pos,s.size(),cp,next)) return Multi::one(Value::nil());
    return Multi::one(Value::integer((std::int64_t)pos+1));
  }

  if (pos<s.size() && is_cont_at(pos)) return Multi::one(Value::nil());

  if (step>0) {
    std::size_t p=pos;
    for (std::int64_t k=1;k<step;++k) {
      if (p>=s.size()) return Multi::one(Value::nil());
      std::uint32_t cp=0; std::size_t next=0;
      if (!utf8_detail::decode_one(s,p,s.size(),cp,next)) return Multi::one(Value::nil());
      p=next;
    }
    if (p>=s.size()) return Multi::one(Value::nil());
    return Multi::one(Value::integer((std::int64_t)p+1));
  }

  std::size_t p=pos;
  for (std::int64_t k=0;k<(-step);++k) {
    if (p==0) return Multi::one(Value::nil());
    --p;
    while (p>0 && is_cont_at(p)) --p;
    std::uint32_t cp=0; std::size_t next=0;
    if (!utf8_detail::decode_one(s,p,s.size(),cp,next)) return Multi::one(Value::nil());
  }
  return Multi::one(Value::integer((std::int64_t)p+1));
}

consteval void VM::open_utf8() {
  TableId u=H.new_table_pow2(5);

  auto reg = [&](std::string_view name, NativeFn fn) {
    std::uint32_t id=reg_native(name, fn);
    table_set(u, Value::string(H.sp.intern(name)), mk_native(id));
  };

  id_utf8_codes_iter=reg_native("utf8._codes_iter",&VM::nf_utf8_codes_iter);

  reg("char", &VM::nf_utf8_char);
  reg("codepoint", &VM::nf_utf8_codepoint);
  reg("codes", &VM::nf_utf8_codes);
  reg("len", &VM::nf_utf8_len);
  reg("offset", &VM::nf_utf8_offset);

  table_set(u, Value::string(H.sp.intern("charpattern")),
    Value::string(H.sp.intern("[\\0-\\x7F\\xC2-\\xF4][\\x80-\\xBF]*")));

  table_set(G, Value::string(H.sp.intern("utf8")), Value::table(u));
}

} // namespace ct_lua54
