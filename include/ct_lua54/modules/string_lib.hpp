#pragma once

namespace ct_lua54 {

namespace string_detail {
  static constexpr bool is_alpha(char c){
    return (c>='a'&&c<='z') || (c>='A'&&c<='Z');
  }
  static constexpr bool is_digit(char c){
    return (c>='0'&&c<='9');
  }
  static constexpr bool is_alnum(char c){
    return is_alpha(c) || is_digit(c);
  }
  static constexpr bool is_space(char c){
    return c==' ' || c=='\t' || c=='\r' || c=='\n' || c=='\f' || c=='\v';
  }
  static constexpr bool is_hex(char c){
    return is_digit(c) || (c>='a'&&c<='f') || (c>='A'&&c<='F');
  }
  static constexpr char to_upper(char c){
    return (c>='a'&&c<='z') ? (char)(c-'a'+'A') : c;
  }
  static constexpr char to_lower(char c){
    return (c>='A'&&c<='Z') ? (char)(c-'A'+'a') : c;
  }

  static constexpr std::string_view arg_to_sv(VM& vm, const Value& v, const char* err){
    if (v.tag==Tag::Str) return vm.H.sp.view(v.s);
    if (v.tag==Tag::Int) return vm.H.sp.view(vm.int_to_string(v.i));
    if (v.tag==Tag::Num) return vm.H.sp.view(vm.num_to_string(v.n));
    throw err;
  }

  static constexpr std::int64_t posrelat(std::int64_t pos, std::int64_t len){
    if (pos<0) pos += len + 1;
    return pos;
  }

  static constexpr bool find_plain(std::string_view s, std::string_view p, std::size_t start, std::size_t& out_b, std::size_t& out_e){
    if (start > s.size()) return false;
    if (p.empty()) { out_b=start; out_e=start; return true; }
    if (p.size() > s.size()) return false;
    for (std::size_t i=start; i + p.size() <= s.size(); ++i) {
      bool ok=true;
      for (std::size_t k=0; k<p.size(); ++k) {
        if (s[i+k]!=p[k]) { ok=false; break; }
      }
      if (ok) { out_b=i; out_e=i+p.size(); return true; }
    }
    return false;
  }

  struct PTok {
    std::uint8_t kind{0}; // 0 literal, 1 any, 2 class
    char lit{0};
    char cls{0};
    std::size_t n{0};
  };

  static constexpr bool class_match(char cls, char c){
    switch (cls) {
      case 'a': return is_alpha(c);
      case 'd': return is_digit(c);
      case 's': return is_space(c);
      case 'w': return is_alnum(c) || c=='_';
      case 'l': return (c>='a'&&c<='z');
      case 'u': return (c>='A'&&c<='Z');
      case 'x': return is_hex(c);
      case 'c': return (unsigned char)c < 32u || (unsigned char)c==127u;
      case 'p': return !is_alnum(c) && !is_space(c);
      default: return c==cls;
    }
  }

  static constexpr bool parse_token(std::string_view p, std::size_t pi, PTok& t){
    if (pi>=p.size()) return false;
    char c=p[pi];
    if (c=='.') { t.kind=1; t.n=1; return true; }
    if (c=='%' && pi+1<p.size()) {
      char e=p[pi+1];
      if (e=='a'||e=='d'||e=='s'||e=='w'||e=='l'||e=='u'||e=='x'||e=='c'||e=='p') {
        t.kind=2; t.cls=e; t.n=2; return true;
      }
      t.kind=0; t.lit=e; t.n=2; return true;
    }
    t.kind=0; t.lit=c; t.n=1; return true;
  }

  static constexpr bool tok_match(const PTok& t, char c){
    if (t.kind==1) return true;
    if (t.kind==2) return class_match(t.cls,c);
    return c==t.lit;
  }

  static constexpr bool match_here(std::string_view s, std::size_t si, std::string_view p, std::size_t pi, std::size_t& out_end){
    if (pi>=p.size()) { out_end=si; return true; }
    if (p[pi]=='$' && pi+1==p.size()) {
      if (si==s.size()) { out_end=si; return true; }
      return false;
    }

    PTok t{};
    if (!parse_token(p,pi,t)) return false;
    std::size_t qi=pi+t.n;
    char q=(qi<p.size())?p[qi]:'\0';

    if (q=='*') {
      std::size_t k=si;
      while (k<s.size() && tok_match(t,s[k])) ++k;
      for (;;) {
        if (match_here(s,k,p,qi+1,out_end)) return true;
        if (k==si) break;
        --k;
      }
      return false;
    }
    if (q=='+') {
      std::size_t k=si;
      if (k>=s.size() || !tok_match(t,s[k])) return false;
      ++k;
      while (k<s.size() && tok_match(t,s[k])) ++k;
      for (;;) {
        if (match_here(s,k,p,qi+1,out_end)) return true;
        if (k==si+1) break;
        --k;
      }
      return false;
    }
    if (q=='?') {
      if (si<s.size() && tok_match(t,s[si])) {
        if (match_here(s,si+1,p,qi+1,out_end)) return true;
      }
      return match_here(s,si,p,qi+1,out_end);
    }

    if (si<s.size() && tok_match(t,s[si])) return match_here(s,si+1,p,pi+t.n,out_end);
    return false;
  }

  static constexpr bool find_pattern(std::string_view s, std::string_view p, std::size_t start, std::size_t& out_b, std::size_t& out_e){
    if (start > s.size()) return false;
    if (p.empty()) { out_b=start; out_e=start; return true; }
    bool anchored=false;
    if (!p.empty() && p[0]=='^') { anchored=true; p=p.substr(1); }
    if (anchored) {
      std::size_t e=0;
      if (match_here(s,start,p,0,e)) { out_b=start; out_e=e; return true; }
      return false;
    }
    for (std::size_t i=start; i<=s.size(); ++i) {
      std::size_t e=0;
      if (match_here(s,i,p,0,e)) { out_b=i; out_e=e; return true; }
    }
    return false;
  }

  static constexpr void append_ch(std::array<char, 16384>& out, std::size_t& w, char c){
    if (w>=out.size()) throw "Lua: string result too long";
    out[w++]=c;
  }

  static constexpr void append_sv(std::array<char, 16384>& out, std::size_t& w, std::string_view s){
    if (w + s.size() > out.size()) throw "Lua: string result too long";
    for (char c: s) out[w++]=c;
  }

  static constexpr std::size_t u64_to_dec(char* dst, std::uint64_t v){
    char rev[32]{};
    std::size_t n=0;
    do { rev[n++]=char('0'+(v%10u)); v/=10u; } while (v && n<sizeof(rev));
    for (std::size_t i=0;i<n;++i) dst[i]=rev[n-1-i];
    return n;
  }

  static constexpr std::size_t i64_to_dec(char* dst, std::int64_t v){
    std::size_t w=0;
    std::uint64_t u=0;
    if (v<0) {
      dst[w++]='-';
      u=(std::uint64_t)(-(v+1)) + 1u;
    } else u=(std::uint64_t)v;
    return w + u64_to_dec(dst+w, u);
  }

  static constexpr void append_i64(std::array<char, 16384>& out, std::size_t& w, std::int64_t v){
    char tmp[64]{};
    std::size_t n=i64_to_dec(tmp,v);
    append_sv(out,w,std::string_view(tmp,n));
  }

  static constexpr void append_u64_hex(std::array<char, 16384>& out, std::size_t& w, std::uint64_t v, bool upper){
    char rev[32]{};
    std::size_t n=0;
    do {
      unsigned d=(unsigned)(v & 0xFu);
      rev[n++]=(char)((d<10)?('0'+d):(upper?('A'+(d-10)):('a'+(d-10))));
      v >>= 4u;
    } while (v && n<sizeof(rev));
    if (w+n>out.size()) throw "Lua: string result too long";
    for (std::size_t i=0;i<n;++i) out[w++]=rev[n-1-i];
  }

  static constexpr void append_fixed(VM& vm, std::array<char, 16384>& out, std::size_t& w, double x, int prec){
    if (prec<0) prec=6;
    if (prec>12) prec=12;
    if (!(x==x)) { append_sv(out,w,"nan"); return; }
    if (x<0.0) { append_ch(out,w,'-'); x=-x; }
    if (x>9.0e18) { append_sv(out,w, vm.H.sp.view(vm.num_to_string(x))); return; }

    std::int64_t iv=(std::int64_t)x;
    double frac=x-(double)iv;
    std::uint64_t p10=1u;
    for (int i=0;i<prec;++i) p10*=10u;
    std::uint64_t fv=(std::uint64_t)(frac*(double)p10 + 0.5);
    if (fv>=p10) { ++iv; fv-=p10; }

    append_i64(out,w,iv);
    if (prec==0) return;
    append_ch(out,w,'.');
    char digs[32]{};
    for (int i=prec-1;i>=0;--i) { digs[i]=char('0'+(fv%10u)); fv/=10u; }
    append_sv(out,w,std::string_view(digs,(std::size_t)prec));
  }

  static constexpr void append_quoted(std::array<char, 16384>& out, std::size_t& w, std::string_view s){
    append_ch(out,w,'"');
    for (char c: s) {
      if (c=='\\' || c=='"') { append_ch(out,w,'\\'); append_ch(out,w,c); continue; }
      if (c=='\n') { append_sv(out,w,"\\n"); continue; }
      if (c=='\r') { append_sv(out,w,"\\r"); continue; }
      if (c=='\t') { append_sv(out,w,"\\t"); continue; }
      unsigned uc=(unsigned char)c;
      if (uc<32u || uc==127u) {
        append_sv(out,w,"\\x");
        unsigned hi=(uc>>4u)&0xFu, lo=uc&0xFu;
        append_ch(out,w,(char)(hi<10?('0'+hi):('A'+(hi-10))));
        append_ch(out,w,(char)(lo<10?('0'+lo):('A'+(lo-10))));
        continue;
      }
      append_ch(out,w,c);
    }
    append_ch(out,w,'"');
  }
}

constexpr Multi VM::nf_string_len(VM& vm, const Value* a, std::size_t n) {
  if (n<1) throw "Lua: string.len(s)";
  auto s=string_detail::arg_to_sv(vm,a[0],"Lua: string.len expects string");
  return Multi::one(Value::integer((std::int64_t)s.size()));
}

constexpr Multi VM::nf_string_sub(VM& vm, const Value* a, std::size_t n) {
  if (n<2 || n>3) throw "Lua: string.sub(s, i [,j])";
  auto s=string_detail::arg_to_sv(vm,a[0],"Lua: string.sub expects string");
  std::int64_t len=(std::int64_t)s.size();
  std::int64_t i=string_detail::posrelat(to_int(a[1]),len);
  std::int64_t j=string_detail::posrelat((n>=3)?to_int(a[2]):-1,len);
  if (i<1) i=1;
  if (j>len) j=len;
  if (i>j || i>len) return Multi::one(Value::string(vm.H.sp.intern("")));
  std::size_t b=(std::size_t)(i-1);
  std::size_t e=(std::size_t)j;
  return Multi::one(Value::string(vm.H.sp.intern(s.substr(b,e-b))));
}

constexpr Multi VM::nf_string_find(VM& vm, const Value* a, std::size_t n) {
  if (n<2 || n>4) throw "Lua: string.find(s, pattern [,init [,plain]])";
  auto s=string_detail::arg_to_sv(vm,a[0],"Lua: string.find expects string");
  auto p=string_detail::arg_to_sv(vm,a[1],"Lua: string.find expects pattern string");
  std::int64_t len=(std::int64_t)s.size();
  std::int64_t init=(n>=3)?to_int(a[2]):1;
  init=string_detail::posrelat(init,len);
  if (init<1) init=1;
  if (init>len+1) return Multi::one(Value::nil());
  bool plain=(n>=4)? truthy(a[3]) : false;
  std::size_t b=0,e=0;
  bool ok=plain
    ? string_detail::find_plain(s,p,(std::size_t)(init-1),b,e)
    : string_detail::find_pattern(s,p,(std::size_t)(init-1),b,e);
  if (!ok) return Multi::one(Value::nil());
  Multi m{}; m.n=2;
  m.v[0]=Value::integer((std::int64_t)b+1);
  m.v[1]=Value::integer((std::int64_t)e);
  return m;
}

constexpr Multi VM::nf_string_match(VM& vm, const Value* a, std::size_t n) {
  if (n<2 || n>3) throw "Lua: string.match(s, pattern [,init])";
  auto s=string_detail::arg_to_sv(vm,a[0],"Lua: string.match expects string");
  auto p=string_detail::arg_to_sv(vm,a[1],"Lua: string.match expects pattern string");
  std::int64_t len=(std::int64_t)s.size();
  std::int64_t init=(n>=3)?to_int(a[2]):1;
  init=string_detail::posrelat(init,len);
  if (init<1) init=1;
  if (init>len+1) return Multi::one(Value::nil());
  std::size_t b=0,e=0;
  if (!string_detail::find_pattern(s,p,(std::size_t)(init-1),b,e)) return Multi::one(Value::nil());
  return Multi::one(Value::string(vm.H.sp.intern(s.substr(b,e-b))));
}

constexpr Multi VM::nf_string_gsub(VM& vm, const Value* a, std::size_t n) {
  if (n<3 || n>4) throw "Lua: string.gsub(s, pattern, repl [,n])";
  auto s=string_detail::arg_to_sv(vm,a[0],"Lua: string.gsub expects string");
  auto p=string_detail::arg_to_sv(vm,a[1],"Lua: string.gsub expects pattern string");
  auto r=string_detail::arg_to_sv(vm,a[2],"Lua: string.gsub expects replacement string");
  std::int64_t maxrep=(n>=4)?to_int(a[3]):0x7fffffff;
  if (maxrep<=0 || p.empty()) {
    Multi m{}; m.n=2; m.v[0]=Value::string(vm.H.sp.intern(s)); m.v[1]=Value::integer(0); return m;
  }

  std::array<char, 16384> out{};
  std::size_t w=0;
  std::size_t pos=0;
  std::int64_t cnt=0;

  while (pos<=s.size() && cnt<maxrep) {
    std::size_t b=0,e=0;
    if (!string_detail::find_pattern(s,p,pos,b,e)) break;
    if (b<pos) break;
    string_detail::append_sv(out,w,s.substr(pos,b-pos));
    string_detail::append_sv(out,w,r);
    ++cnt;
    if (e==b) {
      if (pos<s.size()) {
        string_detail::append_ch(out,w,s[pos]);
        ++pos;
      } else break;
    } else pos=e;
  }
  if (pos<s.size()) string_detail::append_sv(out,w,s.substr(pos));

  Multi m{}; m.n=2;
  m.v[0]=Value::string(vm.H.sp.intern(std::string_view(out.data(),w)));
  m.v[1]=Value::integer(cnt);
  return m;
}

constexpr Multi VM::nf_string_byte(VM& vm, const Value* a, std::size_t n) {
  if (n<1 || n>3) throw "Lua: string.byte(s [,i [,j]])";
  auto s=string_detail::arg_to_sv(vm,a[0],"Lua: string.byte expects string");
  std::int64_t len=(std::int64_t)s.size();
  std::int64_t i=string_detail::posrelat((n>=2)?to_int(a[1]):1,len);
  std::int64_t j=string_detail::posrelat((n>=3)?to_int(a[2]):i,len);
  if (i<1) i=1;
  if (j>len) j=len;
  if (i>j) return Multi::none();
  Multi m{};
  std::uint8_t w=0;
  for (std::int64_t k=i; k<=j && w<MAX_RET; ++k) {
    unsigned char c=(unsigned char)s[(std::size_t)(k-1)];
    m.v[w++]=Value::integer((std::int64_t)c);
  }
  m.n=w;
  return m;
}

constexpr Multi VM::nf_string_char(VM& vm, const Value* a, std::size_t n) {
  std::array<char, 8192> out{};
  std::size_t w=0;
  for (std::size_t i=0;i<n;++i) {
    std::int64_t v=to_int(a[i]);
    if (v<0 || v>255) throw "Lua: string.char value out of range";
    if (w>=out.size()) throw "Lua: string.char result too long";
    out[w++]=(char)(unsigned char)v;
  }
  return Multi::one(Value::string(vm.H.sp.intern(std::string_view(out.data(),w))));
}

constexpr Multi VM::nf_string_upper(VM& vm, const Value* a, std::size_t n) {
  if (n<1) throw "Lua: string.upper(s)";
  auto s=string_detail::arg_to_sv(vm,a[0],"Lua: string.upper expects string");
  std::array<char, 8192> out{};
  if (s.size()>out.size()) throw "Lua: string.upper result too long";
  for (std::size_t i=0;i<s.size();++i) out[i]=string_detail::to_upper(s[i]);
  return Multi::one(Value::string(vm.H.sp.intern(std::string_view(out.data(),s.size()))));
}

constexpr Multi VM::nf_string_lower(VM& vm, const Value* a, std::size_t n) {
  if (n<1) throw "Lua: string.lower(s)";
  auto s=string_detail::arg_to_sv(vm,a[0],"Lua: string.lower expects string");
  std::array<char, 8192> out{};
  if (s.size()>out.size()) throw "Lua: string.lower result too long";
  for (std::size_t i=0;i<s.size();++i) out[i]=string_detail::to_lower(s[i]);
  return Multi::one(Value::string(vm.H.sp.intern(std::string_view(out.data(),s.size()))));
}

constexpr Multi VM::nf_string_rep(VM& vm, const Value* a, std::size_t n) {
  if (n<2 || n>3) throw "Lua: string.rep(s, n [,sep])";
  auto s=string_detail::arg_to_sv(vm,a[0],"Lua: string.rep expects string");
  std::int64_t reps=to_int(a[1]);
  auto sep=(n>=3)? string_detail::arg_to_sv(vm,a[2],"Lua: string.rep sep must be string") : std::string_view{};
  if (reps<=0) return Multi::one(Value::string(vm.H.sp.intern("")));
  std::array<char, 16384> out{};
  std::size_t w=0;
  for (std::int64_t i=0;i<reps;++i) {
    if (i>0) string_detail::append_sv(out,w,sep);
    string_detail::append_sv(out,w,s);
  }
  return Multi::one(Value::string(vm.H.sp.intern(std::string_view(out.data(),w))));
}

constexpr Multi VM::nf_string_reverse(VM& vm, const Value* a, std::size_t n) {
  if (n<1) throw "Lua: string.reverse(s)";
  auto s=string_detail::arg_to_sv(vm,a[0],"Lua: string.reverse expects string");
  std::array<char, 8192> out{};
  if (s.size()>out.size()) throw "Lua: string.reverse result too long";
  for (std::size_t i=0;i<s.size();++i) out[i]=s[s.size()-1-i];
  return Multi::one(Value::string(vm.H.sp.intern(std::string_view(out.data(),s.size()))));
}

constexpr Multi VM::nf_string_format(VM& vm, const Value* a, std::size_t n) {
  if (n<1) throw "Lua: string.format(fmt, ...)";
  auto fmt=string_detail::arg_to_sv(vm,a[0],"Lua: string.format expects format string");
  std::array<char, 16384> out{};
  std::size_t w=0;
  std::size_t ai=1;

  auto append_padded = [&](std::string_view sv, int width, bool left) constexpr {
    if (width<0 || (std::size_t)width<=sv.size()) { string_detail::append_sv(out,w,sv); return; }
    std::size_t pad=(std::size_t)width - sv.size();
    if (!left) for (std::size_t i=0;i<pad;++i) string_detail::append_ch(out,w,' ');
    string_detail::append_sv(out,w,sv);
    if (left) for (std::size_t i=0;i<pad;++i) string_detail::append_ch(out,w,' ');
  };

  for (std::size_t i=0;i<fmt.size();) {
    char c=fmt[i++];
    if (c!='%') { string_detail::append_ch(out,w,c); continue; }
    if (i<fmt.size() && fmt[i]=='%') { ++i; string_detail::append_ch(out,w,'%'); continue; }

    bool left=false;
    while (i<fmt.size()) {
      char f=fmt[i];
      if (f=='-') { left=true; ++i; continue; }
      if (f=='+'||f==' '||f=='#'||f=='0') { ++i; continue; }
      break;
    }

    int width=-1;
    if (i<fmt.size() && fmt[i]>='0' && fmt[i]<='9') {
      width=0;
      while (i<fmt.size() && fmt[i]>='0' && fmt[i]<='9') { width=width*10 + (fmt[i]-'0'); ++i; }
    }

    int prec=-1;
    if (i<fmt.size() && fmt[i]=='.') {
      ++i;
      prec=0;
      while (i<fmt.size() && fmt[i]>='0' && fmt[i]<='9') { prec=prec*10 + (fmt[i]-'0'); ++i; }
    }

    while (i<fmt.size() && (fmt[i]=='l'||fmt[i]=='h'||fmt[i]=='j'||fmt[i]=='z'||fmt[i]=='t'||fmt[i]=='L')) ++i;
    if (i>=fmt.size()) throw "Lua: bad format string";
    char sp=fmt[i++];

    if (sp=='%') { string_detail::append_ch(out,w,'%'); continue; }
    if (ai>=n) throw "Lua: string.format missing value";
    Value v=a[ai++];

    if (sp=='s') {
      std::string_view sv;
      if (v.tag==Tag::Bool) sv=v.b?"true":"false";
      else if (v.tag==Tag::Nil) sv="nil";
      else sv=string_detail::arg_to_sv(vm,v,"Lua: string.format %s expects string/number");
      if (prec>=0 && (std::size_t)prec<sv.size()) sv=sv.substr(0,(std::size_t)prec);
      append_padded(sv,width,left);
      continue;
    }

    std::array<char, 256> tmp{};
    std::size_t tw=0;
    auto append_tmp = [&]() constexpr { append_padded(std::string_view(tmp.data(),tw), width, left); };

    if (sp=='d' || sp=='i') {
      tw=string_detail::i64_to_dec(tmp.data(), to_int(v));
      append_tmp();
      continue;
    }
    if (sp=='u') {
      tw=string_detail::u64_to_dec(tmp.data(), (std::uint64_t)to_int(v));
      append_tmp();
      continue;
    }
    if (sp=='x' || sp=='X') {
      std::array<char, 16384> t2{};
      std::size_t t2w=0;
      string_detail::append_u64_hex(t2,t2w,(std::uint64_t)to_int(v), sp=='X');
      append_padded(std::string_view(t2.data(),t2w), width, left);
      continue;
    }
    if (sp=='f') {
      std::array<char, 16384> t2{};
      std::size_t t2w=0;
      string_detail::append_fixed(vm,t2,t2w,to_num(v),prec);
      append_padded(std::string_view(t2.data(),t2w), width, left);
      continue;
    }
    if (sp=='c') {
      std::int64_t x=to_int(v);
      if (x<0 || x>255) throw "Lua: string.format %c out of range";
      tmp[0]=(char)(unsigned char)x; tw=1;
      append_tmp();
      continue;
    }
    if (sp=='q') {
      std::array<char, 16384> t2{};
      std::size_t t2w=0;
      auto sv=string_detail::arg_to_sv(vm,v,"Lua: string.format %q expects string/number");
      string_detail::append_quoted(t2,t2w,sv);
      append_padded(std::string_view(t2.data(),t2w), width, left);
      continue;
    }

    throw "Lua: unsupported format specifier";
  }

  return Multi::one(Value::string(vm.H.sp.intern(std::string_view(out.data(),w))));
}

consteval void VM::open_string() {
  TableId smod=H.new_table_pow2(6);

  auto reg = [&](std::string_view name, NativeFn fn) {
    std::uint32_t id=reg_native(name, fn);
    table_set(smod, Value::string(H.sp.intern(name)), mk_native(id));
  };

  reg("len",     &VM::nf_string_len);
  reg("sub",     &VM::nf_string_sub);
  reg("find",    &VM::nf_string_find);
  reg("match",   &VM::nf_string_match);
  reg("gsub",    &VM::nf_string_gsub);
  reg("byte",    &VM::nf_string_byte);
  reg("char",    &VM::nf_string_char);
  reg("upper",   &VM::nf_string_upper);
  reg("lower",   &VM::nf_string_lower);
  reg("rep",     &VM::nf_string_rep);
  reg("reverse", &VM::nf_string_reverse);
  reg("format",  &VM::nf_string_format);

  table_set(G, Value::string(H.sp.intern("string")), Value::table(smod));

  // Enable method syntax on strings, e.g. ("abc"):upper().
  TableId smt=H.type_mt[(std::size_t)Tag::Str];
  if (smt.id==0) {
    smt=H.new_table_pow2(4);
    H.type_mt[(std::size_t)Tag::Str]=smt;
  }
  table_set(smt, Value::string(H.sp.intern("__index")), Value::table(smod));
}

} // namespace ct_lua54
