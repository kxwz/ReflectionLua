#pragma once

namespace ct_lua54 {

namespace string_detail {
  static constexpr std::size_t MAX_CAPTURES = 16;

  static constexpr bool is_alpha(char c){
    return (c>='a'&&c<='z') || (c>='A'&&c<='Z');
  }
  static constexpr bool is_digit(char c){
    return (c>='0'&&c<='9');
  }
  static constexpr bool is_alnum(char c){
    return is_alpha(c) || is_digit(c);
  }
  static constexpr bool is_graph(char c){
    unsigned uc=(unsigned char)c;
    return uc>=33u && uc<=126u;
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

  static constexpr void append_ch(std::array<char, 16384>& out, std::size_t& w, char c);
  static constexpr void append_sv(std::array<char, 16384>& out, std::size_t& w, std::string_view s);
  static constexpr void append_i64(std::array<char, 16384>& out, std::size_t& w, std::int64_t v);

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

  struct Capture {
    std::size_t b{0};
    std::size_t e{0};
    bool is_pos{false};
    bool open{false};
  };

  struct MatchState {
    std::size_t begin{0};
    std::size_t end{0};
    std::array<Capture, MAX_CAPTURES> caps{};
    std::uint8_t cap_n{0};
  };

  struct PTok {
    std::uint8_t kind{0}; // 0 literal, 1 any, 2 class, 3 set, 4 backref, 5 frontier, 6 balanced
    char lit{0};
    char cls{0};
    std::uint8_t ref{0};
    std::size_t set_b{0};
    std::size_t set_e{0};
    bool set_neg{false};
    std::size_t n{0};
  };

  static constexpr bool class_match(char cls, char c){
    bool neg=false;
    if (cls>='A'&&cls<='Z') {
      neg=true;
      cls=(char)(cls-'A'+'a');
    }

    bool ok=false;
    switch (cls) {
      case 'a': ok=is_alpha(c); break;
      case 'd': ok=is_digit(c); break;
      case 'g': ok=is_graph(c); break;
      case 's': ok=is_space(c); break;
      case 'w': ok=is_alnum(c); break;
      case 'l': ok=(c>='a'&&c<='z'); break;
      case 'u': ok=(c>='A'&&c<='Z'); break;
      case 'x': ok=is_hex(c); break;
      case 'c': ok=(unsigned char)c < 32u || (unsigned char)c==127u; break;
      case 'p': ok=!is_alnum(c) && !is_space(c); break;
      case 'z': ok=((unsigned char)c)==0u; break;
      default: ok=(c==cls); break;
    }
    return neg ? !ok : ok;
  }

  static constexpr std::size_t find_set_end(std::string_view p, std::size_t pi){
    if (pi>=p.size() || p[pi]!='[') throw "Lua: malformed pattern";
    std::size_t j=pi+1;
    if (j<p.size() && p[j]=='^') ++j;
    if (j<p.size() && p[j]==']') ++j;
    for (; j<p.size(); ++j) {
      if (p[j]=='%') {
        ++j;
        if (j>=p.size()) throw "Lua: malformed pattern";
        continue;
      }
      if (p[j]==']') return j;
    }
    throw "Lua: malformed pattern";
  }

  static constexpr bool parse_token(std::string_view p, std::size_t pi, PTok& t){
    if (pi>=p.size()) return false;
    char c=p[pi];
    if (c=='*' || c=='+' || c=='-' || c=='?') throw "Lua: malformed pattern";
    if (c=='[') {
      std::size_t end=find_set_end(p, pi);
      t.kind=3;
      t.set_b=pi+1;
      t.set_e=end;
      t.set_neg=false;
      if (t.set_b<t.set_e && p[t.set_b]=='^') {
        t.set_neg=true;
        ++t.set_b;
      }
      t.n=(end-pi)+1;
      return true;
    }
    if (c=='.') { t.kind=1; t.n=1; return true; }
    if (c=='%') {
      if (pi+1>=p.size()) throw "Lua: malformed pattern";
      char e=p[pi+1];
      if (e=='b') {
        if (pi+3>=p.size()) throw "Lua: malformed pattern";
        t.kind=6; t.lit=p[pi+2]; t.cls=p[pi+3]; t.n=4; return true;
      }
      if (e=='f') {
        if (pi+2>=p.size() || p[pi+2]!='[') throw "Lua: malformed pattern";
        std::size_t end=find_set_end(p, pi+2);
        t.kind=5;
        t.set_b=pi+3;
        t.set_e=end;
        t.set_neg=false;
        if (t.set_b<t.set_e && p[t.set_b]=='^') {
          t.set_neg=true;
          ++t.set_b;
        }
        t.n=(end-pi)+1;
        return true;
      }
      if (e=='a'||e=='d'||e=='s'||e=='w'||e=='l'||e=='u'||e=='x'||e=='c'||e=='p'||e=='z'||
          e=='g'||e=='A'||e=='D'||e=='S'||e=='W'||e=='L'||e=='U'||e=='X'||e=='C'||e=='P'||e=='Z'||e=='G') {
        t.kind=2; t.cls=e; t.n=2; return true;
      }
      if (e>='1' && e<='9') {
        t.kind=4; t.ref=(std::uint8_t)(e-'1'); t.n=2; return true;
      }
      t.kind=0; t.lit=e; t.n=2; return true;
    }
    t.kind=0; t.lit=c; t.n=1; return true;
  }

  static constexpr bool set_match(std::string_view p, std::size_t b, std::size_t e, bool neg, char c){
    bool ok=false;
    std::size_t j=b;
    if (j<e && p[j]==']') {
      if (c==']') ok=true;
      ++j;
    }
    for (; j<e;) {
      if (p[j]=='%' && j+1<e) {
        char esc=p[j+1];
        if (class_match(esc,c)) ok=true;
        else if (c==esc) ok=true;
        j+=2;
        continue;
      }
      if (p[j]=='%') throw "Lua: malformed pattern";
      if (p[j]!='-' && j+2<e && p[j+1]=='-') {
        char lo=p[j];
        char hi=p[j+2];
        if (lo<=c && c<=hi) ok=true;
        j+=3;
        continue;
      }
      if (p[j]==c) ok=true;
      ++j;
    }
    return neg ? !ok : ok;
  }

  static constexpr bool tok_match(const PTok& t, char c){
    if (t.kind==1) return true;
    if (t.kind==2) return class_match(t.cls,c);
    return c==t.lit;
  }

  static constexpr int last_open_capture(const MatchState& ms){
    for (int i=(int)ms.cap_n-1; i>=0; --i) {
      if (ms.caps[(std::size_t)i].open) return i;
    }
    return -1;
  }

  static constexpr bool match_one(std::string_view s, std::size_t si, std::string_view p, const PTok& t, const MatchState& ms, std::size_t& out_next){
    if (t.kind==4) {
      if (t.ref >= ms.cap_n) throw "Lua: invalid capture index";
      const Capture& cap=ms.caps[t.ref];
      if (cap.open || cap.is_pos) throw "Lua: invalid capture index";
      std::size_t len=cap.e-cap.b;
      if (si+len > s.size()) return false;
      for (std::size_t i=0;i<len;++i) {
        if (s[si+i]!=s[cap.b+i]) return false;
      }
      out_next=si+len;
      return true;
    }

    if (t.kind==5) {
      char prev=(si==0) ? '\0' : s[si-1];
      char cur=(si<s.size()) ? s[si] : '\0';
      if (set_match(p, t.set_b, t.set_e, t.set_neg, prev)) return false;
      if (!set_match(p, t.set_b, t.set_e, t.set_neg, cur)) return false;
      out_next=si;
      return true;
    }

    if (t.kind==6) {
      if (si>=s.size() || s[si]!=t.lit) return false;
      std::size_t depth=1;
      for (std::size_t i=si+1;i<s.size();++i) {
        if (s[i]==t.lit) ++depth;
        else if (s[i]==t.cls) {
          if (--depth==0) {
            out_next=i+1;
            return true;
          }
        }
      }
      return false;
    }

    if (si>=s.size()) return false;
    bool ok=false;
    if (t.kind==3) ok=set_match(p, t.set_b, t.set_e, t.set_neg, s[si]);
    else ok=tok_match(t,s[si]);
    if (!ok) return false;
    out_next=si+1;
    return true;
  }

  static constexpr bool match_here(std::string_view s, std::size_t si, std::string_view p, std::size_t pi, const MatchState& ms, MatchState& out);

  static constexpr bool token_repeatable(const PTok& t){
    return t.kind<=3;
  }

  static constexpr bool max_expand(std::string_view s, std::size_t si, std::string_view p,
                                   std::size_t qi, const PTok& t,
                                   const MatchState& ms, MatchState& out,
                                   std::size_t min_count){
    std::size_t start_si=si;
    std::size_t count=0;
    std::size_t next_si=si;
    while (match_one(s, si, p, t, ms, next_si) && next_si>si) {
      si=next_si;
      ++count;
    }
    if (count<min_count) return false;
    for (std::size_t use=count;;--use) {
      std::size_t try_si=start_si;
      for (std::size_t i=0;i<use;++i) {
        if (!match_one(s, try_si, p, t, ms, next_si) || next_si<=try_si) return false;
        try_si=next_si;
      }
      if (match_here(s, try_si, p, qi+1, ms, out)) return true;
      if (use==min_count) break;
    }
    return false;
  }

  static constexpr bool min_expand(std::string_view s, std::size_t si, std::string_view p,
                                   std::size_t qi, const PTok& t,
                                   const MatchState& ms, MatchState& out){
    if (match_here(s, si, p, qi+1, ms, out)) return true;
    std::size_t next_si=si;
    if (match_one(s, si, p, t, ms, next_si) && next_si>si) {
      return min_expand(s, next_si, p, qi, t, ms, out);
    }
    return false;
  }

  static constexpr bool match_here(std::string_view s, std::size_t si, std::string_view p, std::size_t pi, const MatchState& ms, MatchState& out){
    if (pi>=p.size()) {
      if (last_open_capture(ms)>=0) throw "Lua: invalid pattern capture";
      out=ms;
      out.end=si;
      return true;
    }
    if (p[pi]=='$' && pi+1==p.size()) {
      if (si==s.size()) { out=ms; out.end=si; return true; }
      return false;
    }

    if (p[pi]=='(') {
      if (ms.cap_n>=MAX_CAPTURES) throw "Lua: too many captures";
      MatchState next=ms;
      if (pi+1<p.size() && p[pi+1]==')') {
        next.caps[next.cap_n++] = Capture{si, si, true, false};
        return match_here(s, si, p, pi+2, next, out);
      }
      next.caps[next.cap_n++] = Capture{si, si, false, true};
      return match_here(s, si, p, pi+1, next, out);
    }

    if (p[pi]==')') {
      int idx=last_open_capture(ms);
      if (idx<0) throw "Lua: invalid pattern capture";
      MatchState next=ms;
      next.caps[(std::size_t)idx].e=si;
      next.caps[(std::size_t)idx].open=false;
      return match_here(s, si, p, pi+1, next, out);
    }

    PTok t{};
    if (!parse_token(p,pi,t)) throw "Lua: malformed pattern";
    std::size_t qi=pi+t.n;
    char q=(qi<p.size())?p[qi]:'\0';

    if (token_repeatable(t) && q=='*') return max_expand(s, si, p, qi, t, ms, out, 0);
    if (token_repeatable(t) && q=='+') return max_expand(s, si, p, qi, t, ms, out, 1);
    if (token_repeatable(t) && q=='?') {
      std::size_t next_si=si;
      if (match_one(s, si, p, t, ms, next_si)) {
        if (match_here(s, next_si, p, qi+1, ms, out)) return true;
      }
      return match_here(s, si, p, qi+1, ms, out);
    }
    if (token_repeatable(t) && q=='-') return min_expand(s, si, p, qi, t, ms, out);

    std::size_t next_si=si;
    if (match_one(s, si, p, t, ms, next_si)) return match_here(s, next_si, p, qi, ms, out);
    return false;
  }

  static constexpr bool find_pattern(std::string_view s, std::string_view p, std::size_t start, MatchState& out){
    if (start > s.size()) return false;
    if (p.empty()) { out=MatchState{}; out.begin=start; out.end=start; return true; }
    bool anchored=false;
    if (!p.empty() && p[0]=='^') { anchored=true; p=p.substr(1); }
    if (anchored) {
      MatchState ms{}; ms.begin=start;
      if (match_here(s,start,p,0,ms,out)) { out.begin=start; return true; }
      return false;
    }
    for (std::size_t i=start; i<=s.size(); ++i) {
      MatchState ms{}; ms.begin=i;
      if (match_here(s,i,p,0,ms,out)) { out.begin=i; return true; }
    }
    return false;
  }

  static constexpr Value capture_value(VM& vm, std::string_view s, const MatchState& ms, std::size_t idx){
    const Capture& cap=ms.caps[idx];
    if (cap.is_pos) return Value::integer((std::int64_t)cap.b + 1);
    return Value::string(vm.H.sp.intern(s.substr(cap.b, cap.e-cap.b)));
  }

  static constexpr Value match_value(VM& vm, std::string_view s, const MatchState& ms){
    return Value::string(vm.H.sp.intern(s.substr(ms.begin, ms.end-ms.begin)));
  }

  static constexpr std::size_t result_count(const MatchState& ms){
    return ms.cap_n ? (std::size_t)ms.cap_n : 1u;
  }

  static constexpr Value result_value(VM& vm, std::string_view s, const MatchState& ms, std::size_t idx){
    if (ms.cap_n==0) return match_value(vm, s, ms);
    return capture_value(vm, s, ms, idx);
  }

  static constexpr void append_match_text(std::array<char, 16384>& out, std::size_t& w, std::string_view s, const MatchState& ms){
    append_sv(out, w, s.substr(ms.begin, ms.end-ms.begin));
  }

  static constexpr void append_capture_text(VM& vm, std::array<char, 16384>& out, std::size_t& w, std::string_view s, const MatchState& ms, std::size_t idx){
    const Capture& cap=ms.caps[idx];
    if (cap.is_pos) {
      append_i64(out, w, (std::int64_t)cap.b + 1);
      return;
    }
    append_sv(out, w, s.substr(cap.b, cap.e-cap.b));
  }

  static constexpr void append_replacement_template(VM& vm, std::array<char, 16384>& out, std::size_t& w, std::string_view repl, std::string_view s, const MatchState& ms){
    for (std::size_t i=0;i<repl.size();++i) {
      char c=repl[i];
      if (c!='%') { append_ch(out,w,c); continue; }
      if (i+1>=repl.size()) { append_ch(out,w,'%'); break; }
      char esc=repl[++i];
      if (esc=='%') { append_ch(out,w,'%'); continue; }
      if (esc=='0') { append_match_text(out,w,s,ms); continue; }
      if (esc>='1' && esc<='9') {
        std::size_t idx=(std::size_t)(esc-'1');
        if (idx>=ms.cap_n) throw "Lua: invalid capture index";
        append_capture_text(vm,out,w,s,ms,idx);
        continue;
      }
      append_ch(out,w,esc);
    }
  }

  static constexpr bool replacement_is_passthrough(const Value& v){
    return v.tag==Tag::Nil || (v.tag==Tag::Bool && !v.b);
  }

  static constexpr void append_replacement_value(VM& vm, std::array<char, 16384>& out, std::size_t& w, const Value& v){
    if (v.tag==Tag::Str || v.tag==Tag::Int || v.tag==Tag::Num) {
      append_sv(out, w, arg_to_sv(vm, v, "Lua: string.gsub replacement"));
      return;
    }
    throw "Lua: string.gsub replacement must be string or number";
  }

  static constexpr Multi match_results(VM& vm, std::string_view s, const MatchState& ms){
    Multi m{};
    std::size_t n=result_count(ms);
    if (n>MAX_RET) n=MAX_RET;
    m.n=(std::uint8_t)n;
    for (std::size_t i=0;i<n;++i) m.v[(std::uint8_t)i]=result_value(vm, s, ms, i);
    return m;
  }

  static constexpr Multi find_results(VM& vm, std::string_view s, const MatchState& ms){
    Multi m{};
    std::size_t n=2u + (std::size_t)ms.cap_n;
    if (n>MAX_RET) n=MAX_RET;
    m.n=(std::uint8_t)n;
    m.v[0]=Value::integer((std::int64_t)ms.begin + 1);
    m.v[1]=Value::integer((std::int64_t)ms.end);
    for (std::size_t i=2;i<n;++i) m.v[(std::uint8_t)i]=capture_value(vm, s, ms, i-2u);
    return m;
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

  struct FormatSpec {
    bool left{false};
    bool plus{false};
    bool space{false};
    bool alt{false};
    bool zero{false};
    bool upper{false};
    int width{-1};
    int prec{-1};
    char conv{0};
  };

  static constexpr bool native_little_endian = (std::endian::native == std::endian::little);

  static constexpr bool is_nan(double x){
    std::uint64_t bits=std::bit_cast<std::uint64_t>(x);
    return ((bits >> 52u) & 0x7ffu) == 0x7ffu && (bits & 0x000fffffffffffffull) != 0u;
  }

  static constexpr bool is_inf(double x){
    std::uint64_t bits=std::bit_cast<std::uint64_t>(x) & 0x7fffffffffffffffull;
    return bits == 0x7ff0000000000000ull;
  }

  static constexpr bool is_neg_zero(double x){
    return std::bit_cast<std::uint64_t>(x) == 0x8000000000000000ull;
  }

  static constexpr std::uint64_t abs_i64_u(std::int64_t v){
    return (v<0) ? ((std::uint64_t)(-(v+1)) + 1u) : (std::uint64_t)v;
  }

  static constexpr int clamp_float_prec(int prec){
    if (prec < 0) return 6;
    if (prec > 17) return 17;
    return prec;
  }

  static constexpr std::size_t u64_to_base(char* dst, std::uint64_t v, unsigned base, bool upper){
    char rev[64]{};
    std::size_t n=0;
    do {
      unsigned d=(unsigned)(v % (std::uint64_t)base);
      rev[n++]=(char)((d<10u) ? ('0'+d) : ((upper?'A':'a') + (d-10u)));
      v /= (std::uint64_t)base;
    } while (v && n<sizeof(rev));
    for (std::size_t i=0;i<n;++i) dst[i]=rev[n-1-i];
    return n;
  }

  static constexpr std::uint64_t pow10_u(int prec){
    std::uint64_t p=1u;
    for (int i=0;i<prec;++i) p*=10u;
    return p;
  }

  static constexpr int decimal_exp10(double x){
    if (x==0.0) return 0;
    int e=0;
    while (x>=10.0 && e<4096) { x/=10.0; ++e; }
    while (x<1.0 && e>-4096) { x*=10.0; --e; }
    return e;
  }

  static constexpr std::size_t format_fixed_body(char* dst, double x, int prec, bool alt){
    if (is_nan(x)) {
      dst[0]='n'; dst[1]='a'; dst[2]='n';
      return 3;
    }
    if (is_inf(x)) {
      dst[0]='i'; dst[1]='n'; dst[2]='f';
      return 3;
    }
    prec=clamp_float_prec(prec);
    std::uint64_t iv=(std::uint64_t)x;
    double frac=x-(double)iv;
    if (frac<0.0) frac=-frac;
    std::uint64_t p10=pow10_u(prec);
    std::uint64_t fv=(std::uint64_t)(frac*(double)p10 + 0.5);
    if (fv>=p10) { ++iv; fv-=p10; }

    std::size_t w=u64_to_dec(dst, iv);
    if (prec==0 && !alt) return w;
    dst[w++]='.';
    if (prec==0) return w;
    for (int i=prec-1;i>=0;--i) { dst[w+(std::size_t)i]=char('0'+(fv%10u)); fv/=10u; }
    return w + (std::size_t)prec;
  }

  static constexpr std::size_t format_scientific_body(char* dst, double x, int prec, bool upper, bool alt){
    if (is_nan(x)) {
      dst[0]=upper?'N':'n'; dst[1]=upper?'A':'a'; dst[2]=upper?'N':'n';
      return 3;
    }
    if (is_inf(x)) {
      dst[0]=upper?'I':'i'; dst[1]=upper?'N':'n'; dst[2]=upper?'F':'f';
      return 3;
    }

    prec=clamp_float_prec(prec);
    int exp10=0;
    double m=x;
    if (m!=0.0) {
      exp10=decimal_exp10(m);
      while (m>=10.0) m/=10.0;
      while (m<1.0) m*=10.0;
    }

    std::uint64_t p10=pow10_u(prec);
    std::uint64_t scaled=(std::uint64_t)(m*(double)p10 + 0.5);
    if (scaled >= 10u*p10) {
      scaled/=10u;
      ++exp10;
    }

    std::uint64_t lead = (prec==0) ? scaled : (scaled / p10);
    std::uint64_t frac = (prec==0) ? 0u : (scaled % p10);

    std::size_t w=0;
    dst[w++]=char('0'+lead);
    if (prec>0 || alt) dst[w++]='.';
    if (prec>0) {
      for (int i=prec-1;i>=0;--i) { dst[w+(std::size_t)i]=char('0'+(frac%10u)); frac/=10u; }
      w += (std::size_t)prec;
    }
    dst[w++]=upper?'E':'e';
    if (exp10<0) { dst[w++]='-'; exp10=-exp10; }
    else dst[w++]='+';
    char eb[16]{};
    std::size_t en=u64_to_dec(eb, (std::uint64_t)exp10);
    if (en<2) dst[w++]='0';
    for (std::size_t i=0;i<en;++i) dst[w++]=eb[i];
    return w;
  }

  static constexpr std::size_t trim_fraction_zeros(char* dst, std::size_t n, bool alt){
    if (alt) return n;
    std::size_t exp_pos=n;
    for (std::size_t i=0;i<n;++i) {
      if (dst[i]=='e' || dst[i]=='E' || dst[i]=='p' || dst[i]=='P') { exp_pos=i; break; }
    }
    std::size_t dot_pos=exp_pos;
    for (std::size_t i=0;i<exp_pos;++i) {
      if (dst[i]=='.') { dot_pos=i; break; }
    }
    if (dot_pos==exp_pos) return n;
    std::size_t end=exp_pos;
    while (end>dot_pos+1 && dst[end-1]=='0') --end;
    if (end==dot_pos+1) --end;
    if (exp_pos==n) return end;
    std::size_t keep=n-exp_pos;
    for (std::size_t i=0;i<keep;++i) dst[end+i]=dst[exp_pos+i];
    return end+keep;
  }

  static constexpr std::size_t format_general_body(char* dst, double x, int prec, bool upper, bool alt){
    if (is_nan(x)) {
      dst[0]=upper?'N':'n'; dst[1]=upper?'A':'a'; dst[2]=upper?'N':'n';
      return 3;
    }
    if (is_inf(x)) {
      dst[0]=upper?'I':'i'; dst[1]=upper?'N':'n'; dst[2]=upper?'F':'f';
      return 3;
    }

    if (prec<0) prec=6;
    if (prec==0) prec=1;
    if (prec>17) prec=17;

    int exp10 = (x==0.0) ? 0 : decimal_exp10(x);
    if (exp10 < -4 || exp10 >= prec) {
      std::size_t n=format_scientific_body(dst, x, prec-1, upper, alt);
      return trim_fraction_zeros(dst, n, alt);
    }
    int frac_prec=prec - (exp10 + 1);
    if (frac_prec<0) frac_prec=0;
    std::size_t n=format_fixed_body(dst, x, frac_prec, alt);
    return trim_fraction_zeros(dst, n, alt);
  }

  static constexpr std::size_t format_hex_float_body(char* dst, double x, int prec, bool upper, bool alt){
    if (is_nan(x)) {
      dst[0]=upper?'N':'n'; dst[1]=upper?'A':'a'; dst[2]=upper?'N':'n';
      return 3;
    }
    if (is_inf(x)) {
      dst[0]=upper?'I':'i'; dst[1]=upper?'N':'n'; dst[2]=upper?'F':'f';
      return 3;
    }

    std::uint64_t bits=std::bit_cast<std::uint64_t>(x);
    bits &= 0x7fffffffffffffffull;

    char digs[13]{};
    char basech=upper?'X':'x';
    char expch=upper?'P':'p';
    std::size_t w=0;
    dst[w++]='0';
    dst[w++]=basech;

    if (bits==0u) {
      dst[w++]='0';
      if (prec!=0 || alt || prec>0) {
        dst[w++]='.';
        int pd=(prec<0)?1:prec;
        for (int i=0;i<pd;++i) dst[w++]='0';
      }
      dst[w++]=expch;
      dst[w++]='+';
      dst[w++]='0';
      return w;
    }

    std::uint64_t mant=bits & 0x000fffffffffffffull;
    std::uint64_t expbits=(bits >> 52u) & 0x7ffu;
    int exp2=0;
    char lead='1';
    if (expbits==0u) {
      exp2=-1022;
      lead='0';
    } else {
      exp2=(int)expbits - 1023;
    }

    for (int i=0;i<13;++i) {
      unsigned d=(unsigned)((mant >> (std::uint64_t)((12-i)*4)) & 0xFu);
      digs[(std::size_t)i]=(char)((d<10u)?('0'+d):((upper?'A':'a')+(d-10u)));
    }

    int out_prec=prec;
    if (out_prec<0) {
      out_prec=13;
      while (out_prec>0 && digs[(std::size_t)(out_prec-1)]=='0') --out_prec;
    } else if (out_prec>13) out_prec=13;

    dst[w++]=lead;
    if (out_prec>0 || alt) dst[w++]='.';
    for (int i=0;i<out_prec;++i) dst[w++]=digs[(std::size_t)i];
    dst[w++]=expch;
    if (exp2<0) { dst[w++]='-'; exp2=-exp2; }
    else dst[w++]='+';
    char eb[16]{};
    std::size_t en=u64_to_dec(eb, (std::uint64_t)exp2);
    for (std::size_t i=0;i<en;++i) dst[w++]=eb[i];
    return w;
  }

  template <std::size_t N>
  static constexpr void append_padded_field(std::array<char, N>& out, std::size_t& w,
                                            std::string_view prefix, std::string_view body,
                                            const FormatSpec& spec, bool zero_pad){
    std::size_t total=prefix.size()+body.size();
    std::size_t pad=(spec.width>=0 && (std::size_t)spec.width>total) ? ((std::size_t)spec.width-total) : 0u;
    if (!spec.left && !zero_pad) for (std::size_t i=0;i<pad;++i) append_ch(out,w,' ');
    append_sv(out,w,prefix);
    if (!spec.left && zero_pad) for (std::size_t i=0;i<pad;++i) append_ch(out,w,'0');
    append_sv(out,w,body);
    if (spec.left) for (std::size_t i=0;i<pad;++i) append_ch(out,w,' ');
  }

  static constexpr FormatSpec parse_format_spec(std::string_view fmt, std::size_t& i){
    FormatSpec sp{};
    while (i<fmt.size()) {
      char f=fmt[i];
      if (f=='-') sp.left=true;
      else if (f=='+') sp.plus=true;
      else if (f==' ') sp.space=true;
      else if (f=='#') sp.alt=true;
      else if (f=='0') sp.zero=true;
      else break;
      ++i;
    }
    if (i<fmt.size() && fmt[i]=='*') throw "Lua: unsupported format specifier";
    if (i<fmt.size() && fmt[i]>='0' && fmt[i]<='9') {
      sp.width=0;
      while (i<fmt.size() && fmt[i]>='0' && fmt[i]<='9') { sp.width = sp.width*10 + (fmt[i]-'0'); ++i; }
    }
    if (i<fmt.size() && fmt[i]=='.') {
      ++i;
      if (i<fmt.size() && fmt[i]=='*') throw "Lua: unsupported format specifier";
      sp.prec=0;
      while (i<fmt.size() && fmt[i]>='0' && fmt[i]<='9') { sp.prec = sp.prec*10 + (fmt[i]-'0'); ++i; }
    }
    if (i<fmt.size() && (fmt[i]=='h' || fmt[i]=='l' || fmt[i]=='L' || fmt[i]=='n')) {
      throw "Lua: unsupported format specifier";
    }
    if (i>=fmt.size()) throw "Lua: bad format string";
    sp.conv=fmt[i++];
    sp.upper = (sp.conv>='A' && sp.conv<='Z');
    return sp;
  }

  static constexpr std::size_t format_pointer_body(char* dst, const Value& v, bool upper){
    std::uint64_t id=0;
    switch (v.tag) {
      case Tag::Nil:   id=0u; break;
      case Tag::Bool:  id=0x1000000000000000ull | (std::uint64_t)(v.b?1u:0u); break;
      case Tag::Int:   id=0x2000000000000000ull ^ std::uint64_t(v.i); break;
      case Tag::Num:   id=0x3000000000000000ull ^ std::bit_cast<std::uint64_t>(v.n); break;
      case Tag::Str:   id=0x4000000000000000ull | (std::uint64_t)v.s.id; break;
      case Tag::Table: id=0x5000000000000000ull | (std::uint64_t)v.t.id; break;
      case Tag::Func:  id=0x6000000000000000ull | ((std::uint64_t)v.f.id<<1u) | (v.f.is_native?1u:0u); break;
      case Tag::UData: id=0x7000000000000000ull | (std::uint64_t)v.u.id; break;
    }
    dst[0]='0';
    dst[1]=upper?'X':'x';
    return 2u + u64_to_base(dst+2, id, 16u, upper);
  }

  struct PackState {
    bool little{native_little_endian};
    std::size_t max_align{1};
  };

  enum class PackKind : std::uint8_t {
    None,
    SInt,
    UInt,
    F32,
    F64,
    FixedString,
    ZString,
    LenString,
    PadByte,
    AlignPad
  };

  struct PackItem {
    PackKind kind{PackKind::None};
    std::size_t size{0};
    std::size_t align{1};
  };

  static constexpr std::size_t limited_align(std::size_t align, std::size_t max_align){
    if (align<1) align=1;
    if (max_align<1) max_align=1;
    return (align<max_align) ? align : max_align;
  }

  template <class T>
  static constexpr std::size_t native_align_for(const PackState& st){
    return limited_align(alignof(T), st.max_align);
  }

  static constexpr std::size_t explicit_align_for(std::size_t size, const PackState& st){
    return limited_align(size, st.max_align);
  }

  static constexpr std::size_t align_pad(std::size_t pos, std::size_t align){
    if (align<=1) return 0;
    std::size_t rem=pos % align;
    return rem ? (align-rem) : 0;
  }

  static constexpr std::size_t parse_pack_digits(std::string_view fmt, std::size_t& i, bool required, std::size_t def=0){
    std::size_t v=0;
    bool any=false;
    while (i<fmt.size() && fmt[i]>='0' && fmt[i]<='9') {
      any=true;
      v = v*10u + (std::size_t)(fmt[i]-'0');
      ++i;
    }
    if (!any) {
      if (required) throw "Lua: invalid format option";
      return def;
    }
    return v;
  }

  static constexpr bool next_pack_item(std::string_view fmt, std::size_t& i, PackState& st, PackItem& out){
    while (i<fmt.size()) {
      char c=fmt[i++];
      if (c==' ') continue;
      switch (c) {
        case '<': st.little=true; continue;
        case '>': st.little=false; continue;
        case '=': st.little=native_little_endian; continue;
        case '!': {
          std::size_t n=parse_pack_digits(fmt, i, true);
          if (n==0u || n>64u) throw "Lua: invalid format option";
          st.max_align=n;
          continue;
        }
        case 'b': out={PackKind::SInt, 1u, 1u}; return true;
        case 'B': out={PackKind::UInt, 1u, 1u}; return true;
        case 'h': out={PackKind::SInt, sizeof(short), native_align_for<short>(st)}; return true;
        case 'H': out={PackKind::UInt, sizeof(unsigned short), native_align_for<unsigned short>(st)}; return true;
        case 'l': out={PackKind::SInt, sizeof(long), native_align_for<long>(st)}; return true;
        case 'L': out={PackKind::UInt, sizeof(unsigned long), native_align_for<unsigned long>(st)}; return true;
        case 'j': out={PackKind::SInt, sizeof(std::int64_t), native_align_for<std::int64_t>(st)}; return true;
        case 'J': out={PackKind::UInt, sizeof(std::uint64_t), native_align_for<std::uint64_t>(st)}; return true;
        case 'T': out={PackKind::UInt, sizeof(std::size_t), native_align_for<std::size_t>(st)}; return true;
        case 'i': {
          std::size_t n=parse_pack_digits(fmt, i, false, sizeof(int));
          if (n==0u || n>8u) throw "Lua: unsupported pack integer size";
          out={PackKind::SInt, n, explicit_align_for(n, st)};
          return true;
        }
        case 'I': {
          std::size_t n=parse_pack_digits(fmt, i, false, sizeof(unsigned));
          if (n==0u || n>8u) throw "Lua: unsupported pack integer size";
          out={PackKind::UInt, n, explicit_align_for(n, st)};
          return true;
        }
        case 'f': out={PackKind::F32, sizeof(float), native_align_for<float>(st)}; return true;
        case 'd':
        case 'n': out={PackKind::F64, sizeof(double), native_align_for<double>(st)}; return true;
        case 'c': {
          std::size_t n=parse_pack_digits(fmt, i, true);
          if (n==0u) throw "Lua: invalid format option";
          out={PackKind::FixedString, n, 1u};
          return true;
        }
        case 'z': out={PackKind::ZString, 0u, 1u}; return true;
        case 's': {
          std::size_t n=parse_pack_digits(fmt, i, false, sizeof(std::size_t));
          if (n==0u || n>8u) throw "Lua: unsupported pack integer size";
          out={PackKind::LenString, n, explicit_align_for(n, st)};
          return true;
        }
        case 'x': out={PackKind::PadByte, 1u, 1u}; return true;
        case 'X': {
          PackItem inner{};
          if (!next_pack_item(fmt, i, st, inner)) throw "Lua: invalid format option";
          out={PackKind::AlignPad, 0u, inner.align};
          return true;
        }
        default: throw "Lua: invalid format option";
      }
    }
    return false;
  }

  static constexpr bool fits_signed_bytes(std::int64_t v, std::size_t size){
    if (size>=8u) return true;
    unsigned bits=(unsigned)(size*8u);
    std::int64_t minv=-(std::int64_t(1) << (bits-1u));
    std::int64_t maxv=(std::int64_t(1) << (bits-1u)) - 1;
    return v>=minv && v<=maxv;
  }

  static constexpr bool fits_unsigned_bytes(std::uint64_t v, std::size_t size){
    if (size>=8u) return true;
    return (v >> (unsigned)(size*8u)) == 0u;
  }

  static constexpr std::uint64_t pack_unsigned_arg(const Value& v){
    if (v.tag==Tag::Int) {
      if (v.i<0) throw "Lua: unsigned overflow";
      return (std::uint64_t)v.i;
    }
    if (v.tag==Tag::Num) {
      std::int64_t i=0;
      if (!VM::as_exact_i64(v.n, i) || i<0) throw "Lua: unsigned overflow";
      return (std::uint64_t)i;
    }
    throw "Lua: expected integer";
  }

  static constexpr std::string_view pack_str_arg(VM& vm, const Value& v, const char* err){
    if (v.tag!=Tag::Str) throw err;
    return vm.H.sp.view(v.s);
  }

  template <std::size_t N>
  static constexpr void pack_put_byte(std::array<char, N>& out, std::size_t& w, unsigned byte){
    if (w>=out.size()) throw "Lua: string result too long";
    out[w++]=(char)(unsigned char)byte;
  }

  template <std::size_t N>
  static constexpr void pack_put_pad(std::array<char, N>& out, std::size_t& w, std::size_t n){
    for (std::size_t i=0;i<n;++i) pack_put_byte(out, w, 0u);
  }

  template <std::size_t N>
  static constexpr void pack_put_uint(std::array<char, N>& out, std::size_t& w, std::uint64_t v, std::size_t size, bool little){
    for (std::size_t i=0;i<size;++i) {
      std::size_t shift=little ? i : (size-1u-i);
      unsigned byte=(unsigned)((v >> (shift*8u)) & 0xffu);
      pack_put_byte(out, w, byte);
    }
  }

  static constexpr std::uint64_t unpack_get_uint(std::string_view s, std::size_t pos, std::size_t size, bool little){
    if (pos+size > s.size()) throw "Lua: data string too short";
    std::uint64_t v=0u;
    if (little) {
      for (std::size_t i=0;i<size;++i) v |= (std::uint64_t)(unsigned char)s[pos+i] << (unsigned)(i*8u);
    } else {
      for (std::size_t i=0;i<size;++i) v = (v<<8u) | (std::uint64_t)(unsigned char)s[pos+i];
    }
    return v;
  }

  static constexpr std::int64_t unpack_signed_value(std::uint64_t v, std::size_t size){
    if (size>=8u) return (std::int64_t)v;
    unsigned bits=(unsigned)(size*8u);
    std::uint64_t signbit=std::uint64_t(1) << (bits-1u);
    if ((v & signbit)==0u) return (std::int64_t)v;
    std::uint64_t mask=(~0ull) << bits;
    return (std::int64_t)(v | mask);
  }

  static constexpr Value unpack_unsigned_value(std::uint64_t v){
    if (v <= (std::uint64_t)std::numeric_limits<std::int64_t>::max()) return Value::integer((std::int64_t)v);
    return Value::number((double)v);
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
  if (plain) {
    std::size_t b=0,e=0;
    if (!string_detail::find_plain(s,p,(std::size_t)(init-1),b,e)) return Multi::one(Value::nil());
    Multi m{}; m.n=2;
    m.v[0]=Value::integer((std::int64_t)b+1);
    m.v[1]=Value::integer((std::int64_t)e);
    return m;
  }

  string_detail::MatchState ms{};
  if (!string_detail::find_pattern(s,p,(std::size_t)(init-1),ms)) return Multi::one(Value::nil());
  return string_detail::find_results(vm, s, ms);
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
  string_detail::MatchState ms{};
  if (!string_detail::find_pattern(s,p,(std::size_t)(init-1),ms)) return Multi::one(Value::nil());
  return string_detail::match_results(vm, s, ms);
}

constexpr Multi VM::nf_string_gmatch_iter(VM& vm, const Value* a, std::size_t n) {
  if (n<1 || a[0].tag!=Tag::Table) throw "Lua: string.gmatch iterator state";
  TableId st=a[0].t;
  StrId key_s=vm.H.sp.intern("_s");
  StrId key_p=vm.H.sp.intern("_p");
  StrId key_i=vm.H.sp.intern("_i");

  Value sv=vm.H.rawget(st, Value::string(key_s));
  Value pv=vm.H.rawget(st, Value::string(key_p));
  Value iv=vm.H.rawget(st, Value::string(key_i));
  if (sv.tag!=Tag::Str || pv.tag!=Tag::Str || (iv.tag!=Tag::Int && iv.tag!=Tag::Num)) {
    throw "Lua: string.gmatch iterator state";
  }

  auto s=vm.H.sp.view(sv.s);
  auto p=vm.H.sp.view(pv.s);
  std::int64_t pos=VM::to_int(iv);
  if (pos<0) pos=0;
  if ((std::size_t)pos > s.size()) return Multi::one(Value::nil());

  string_detail::MatchState ms{};
  if (!string_detail::find_pattern(s,p,(std::size_t)pos,ms)) return Multi::one(Value::nil());

  std::size_t next = (ms.end==ms.begin)
    ? ((ms.begin < s.size()) ? (ms.begin + 1u) : (s.size() + 1u))
    : ms.end;
  vm.H.rawset(st, Value::string(key_i), Value::integer((std::int64_t)next));
  return string_detail::match_results(vm, s, ms);
}

constexpr Multi VM::nf_string_gmatch(VM& vm, const Value* a, std::size_t n) {
  if (n<2 || n>3) throw "Lua: string.gmatch(s, pattern [,init])";
  auto s=string_detail::arg_to_sv(vm,a[0],"Lua: string.gmatch expects string");
  auto p=string_detail::arg_to_sv(vm,a[1],"Lua: string.gmatch expects pattern string");
  std::int64_t len=(std::int64_t)s.size();
  std::int64_t init=(n>=3)?to_int(a[2]):1;
  init=string_detail::posrelat(init,len);
  if (init<1) init=1;
  if (init>len+1) init=len+1;

  TableId st=vm.H.new_table_pow2(3);
  vm.H.rawset(st, Value::string(vm.H.sp.intern("_s")), Value::string(vm.H.sp.intern(s)));
  vm.H.rawset(st, Value::string(vm.H.sp.intern("_p")), Value::string(vm.H.sp.intern(p)));
  vm.H.rawset(st, Value::string(vm.H.sp.intern("_i")), Value::integer(init-1));

  Multi out{}; out.n=3;
  out.v[0]=vm.mk_native(vm.id_string_gmatch_iter);
  out.v[1]=Value::table(st);
  out.v[2]=Value::nil();
  return out;
}

constexpr Multi VM::nf_string_gsub(VM& vm, const Value* a, std::size_t n) {
  if (n<3 || n>4) throw "Lua: string.gsub(s, pattern, repl [,n])";
  auto s=string_detail::arg_to_sv(vm,a[0],"Lua: string.gsub expects string");
  auto p=string_detail::arg_to_sv(vm,a[1],"Lua: string.gsub expects pattern string");
  std::int64_t maxrep=(n>=4)?to_int(a[3]):0x7fffffff;
  if (maxrep<=0) {
    Multi m{}; m.n=2; m.v[0]=Value::string(vm.H.sp.intern(s)); m.v[1]=Value::integer(0); return m;
  }

  std::array<char, 16384> out{};
  std::size_t w=0;
  std::size_t pos=0;
  std::int64_t cnt=0;

  while (pos<=s.size() && cnt<maxrep) {
    string_detail::MatchState ms{};
    if (!string_detail::find_pattern(s,p,pos,ms)) break;
    if (ms.begin<pos) break;
    string_detail::append_sv(out,w,s.substr(pos,ms.begin-pos));

    if (a[2].tag==Tag::Str) {
      string_detail::append_replacement_template(vm,out,w,vm.H.sp.view(a[2].s),s,ms);
    } else if (a[2].tag==Tag::Func) {
      std::array<Value, string_detail::MAX_CAPTURES> args{};
      std::size_t argc=string_detail::result_count(ms);
      for (std::size_t i=0;i<argc;++i) args[i]=string_detail::result_value(vm, s, ms, i);
      Value rv=vm.first(vm.call_value(a[2], args.data(), argc));
      if (string_detail::replacement_is_passthrough(rv)) {
        string_detail::append_match_text(out,w,s,ms);
      } else {
        string_detail::append_replacement_value(vm,out,w,rv);
      }
    } else if (a[2].tag==Tag::Table) {
      Value key=string_detail::result_value(vm, s, ms, 0);
      Value rv=vm.table_get(a[2].t, key);
      if (string_detail::replacement_is_passthrough(rv)) {
        string_detail::append_match_text(out,w,s,ms);
      } else {
        string_detail::append_replacement_value(vm,out,w,rv);
      }
    } else {
      throw "Lua: string.gsub replacement must be string, function, or table";
    }

    ++cnt;
    if (ms.end==ms.begin) {
      if (ms.begin<s.size()) {
        string_detail::append_ch(out,w,s[ms.begin]);
        pos=ms.begin + 1u;
      } else break;
    } else pos=ms.end;
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

  for (std::size_t i=0;i<fmt.size();) {
    char c=fmt[i++];
    if (c!='%') { string_detail::append_ch(out,w,c); continue; }
    if (i<fmt.size() && fmt[i]=='%') { ++i; string_detail::append_ch(out,w,'%'); continue; }
    auto sp=string_detail::parse_format_spec(fmt, i);
    if (ai>=n) throw "Lua: string.format missing value";
    Value v=a[ai++];

    char prefix[4]{};
    std::size_t pn=0;
    char body[512]{};
    std::size_t bn=0;
    bool zero_pad=false;

    auto set_sign = [&](bool neg) constexpr {
      if (neg) prefix[pn++]='-';
      else if (sp.plus) prefix[pn++]='+';
      else if (sp.space) prefix[pn++]=' ';
    };

    auto apply_int_prec = [&](std::string_view digits) constexpr -> std::string_view {
      if (sp.prec==0 && digits.size()==1 && digits[0]=='0') return {};
      if (sp.prec<=0 || (std::size_t)sp.prec<=digits.size()) return digits;
      std::size_t pad=(std::size_t)sp.prec - digits.size();
      for (std::size_t k=0;k<pad;++k) body[k]='0';
      for (std::size_t k=0;k<digits.size();++k) body[pad+k]=digits[k];
      return std::string_view(body, pad+digits.size());
    };

    switch (sp.conv) {
      case 's': {
        auto sv=vm.H.sp.view(vm.value_tostring(v));
        if (sp.prec>=0 && (std::size_t)sp.prec<sv.size()) sv=sv.substr(0,(std::size_t)sp.prec);
        string_detail::append_padded_field(out, w, {}, sv, sp, false);
        break;
      }
      case 'q': {
        std::array<char, 16384> tmp{};
        std::size_t tw=0;
        if (v.tag==Tag::Str) string_detail::append_quoted(tmp, tw, vm.H.sp.view(v.s));
        else if (v.tag==Tag::Nil || v.tag==Tag::Bool || v.tag==Tag::Int || v.tag==Tag::Num) {
          string_detail::append_sv(tmp, tw, vm.H.sp.view(vm.value_tostring(v)));
        } else {
          throw "Lua: string.format %q expects string/number/bool/nil";
        }
        string_detail::append_padded_field(out, w, {}, std::string_view(tmp.data(), tw), sp, false);
        break;
      }
      case 'c': {
        std::int64_t x=to_int(v);
        if (x<0 || x>255) throw "Lua: string.format %c out of range";
        body[0]=(char)(unsigned char)x;
        string_detail::append_padded_field(out, w, {}, std::string_view(body,1), sp, false);
        break;
      }
      case 'd':
      case 'i': {
        std::int64_t x=to_int(v);
        set_sign(x<0);
        char digs[64]{};
        std::size_t dn=string_detail::u64_to_dec(digs, string_detail::abs_i64_u(x));
        auto body_sv=apply_int_prec(std::string_view(digs, dn));
        zero_pad=sp.zero && !sp.left && sp.prec<0;
        string_detail::append_padded_field(out, w, std::string_view(prefix,pn), body_sv, sp, zero_pad);
        break;
      }
      case 'u': {
        char digs[64]{};
        std::size_t dn=string_detail::u64_to_dec(digs, (std::uint64_t)to_int(v));
        auto body_sv=apply_int_prec(std::string_view(digs, dn));
        zero_pad=sp.zero && !sp.left && sp.prec<0;
        string_detail::append_padded_field(out, w, {}, body_sv, sp, zero_pad);
        break;
      }
      case 'o': {
        char digs[64]{};
        std::size_t dn=string_detail::u64_to_base(digs, (std::uint64_t)to_int(v), 8u, false);
        auto body_sv=apply_int_prec(std::string_view(digs, dn));
        if (sp.alt && (body_sv.empty() || body_sv[0] != '0')) prefix[pn++]='0';
        zero_pad=sp.zero && !sp.left && sp.prec<0;
        string_detail::append_padded_field(out, w, std::string_view(prefix,pn), body_sv, sp, zero_pad);
        break;
      }
      case 'x':
      case 'X': {
        char digs[64]{};
        std::size_t dn=string_detail::u64_to_base(digs, (std::uint64_t)to_int(v), 16u, sp.upper);
        auto body_sv=apply_int_prec(std::string_view(digs, dn));
        if (sp.alt && !body_sv.empty()) {
          prefix[pn++]='0';
          prefix[pn++]=sp.upper?'X':'x';
        }
        zero_pad=sp.zero && !sp.left && sp.prec<0;
        string_detail::append_padded_field(out, w, std::string_view(prefix,pn), body_sv, sp, zero_pad);
        break;
      }
      case 'f':
      case 'e':
      case 'E':
      case 'g':
      case 'G':
      case 'a':
      case 'A': {
        double x=to_num(v);
        bool neg=(x<0.0) || string_detail::is_neg_zero(x);
        if (neg) x=-x;
        set_sign(neg);
        if (sp.conv=='f') bn=string_detail::format_fixed_body(body, x, sp.prec, sp.alt);
        else if (sp.conv=='e' || sp.conv=='E') bn=string_detail::format_scientific_body(body, x, sp.prec, sp.upper, sp.alt);
        else if (sp.conv=='g' || sp.conv=='G') bn=string_detail::format_general_body(body, x, sp.prec, sp.upper, sp.alt);
        else bn=string_detail::format_hex_float_body(body, x, sp.prec, sp.upper, sp.alt);
        zero_pad=sp.zero && !sp.left;
        string_detail::append_padded_field(out, w, std::string_view(prefix,pn), std::string_view(body,bn), sp, zero_pad);
        break;
      }
      case 'p': {
        bn=string_detail::format_pointer_body(body, v, sp.upper);
        zero_pad=sp.zero && !sp.left;
        string_detail::append_padded_field(out, w, {}, std::string_view(body,bn), sp, zero_pad);
        break;
      }
      default:
        throw "Lua: unsupported format specifier";
    }
  }

  return Multi::one(Value::string(vm.H.sp.intern(std::string_view(out.data(),w))));
}

constexpr Multi VM::nf_string_pack(VM& vm, const Value* a, std::size_t n) {
  if (n<1) throw "Lua: string.pack(fmt, ...)";
  auto fmt=string_detail::pack_str_arg(vm, a[0], "Lua: string.pack expects format string");
  std::array<char, MAX_STR_BYTES> out{};
  std::size_t w=0;
  std::size_t fi=0;
  std::size_t ai=1;
  string_detail::PackState st{};

  string_detail::PackItem item{};
  while (string_detail::next_pack_item(fmt, fi, st, item)) {
    string_detail::pack_put_pad(out, w, string_detail::align_pad(w, item.align));
    switch (item.kind) {
      case string_detail::PackKind::AlignPad:
        break;
      case string_detail::PackKind::PadByte:
        string_detail::pack_put_byte(out, w, 0u);
        break;
      case string_detail::PackKind::SInt: {
        if (ai>=n) throw "Lua: string.pack missing value";
        std::int64_t v=to_int(a[ai++]);
        if (!string_detail::fits_signed_bytes(v, item.size)) throw "Lua: integer overflow";
        string_detail::pack_put_uint(out, w, (std::uint64_t)v, item.size, st.little);
        break;
      }
      case string_detail::PackKind::UInt: {
        if (ai>=n) throw "Lua: string.pack missing value";
        std::uint64_t v=string_detail::pack_unsigned_arg(a[ai++]);
        if (!string_detail::fits_unsigned_bytes(v, item.size)) throw "Lua: unsigned overflow";
        string_detail::pack_put_uint(out, w, v, item.size, st.little);
        break;
      }
      case string_detail::PackKind::F32: {
        if (ai>=n) throw "Lua: string.pack missing value";
        float f=(float)to_num(a[ai++]);
        string_detail::pack_put_uint(out, w, (std::uint64_t)std::bit_cast<std::uint32_t>(f), item.size, st.little);
        break;
      }
      case string_detail::PackKind::F64: {
        if (ai>=n) throw "Lua: string.pack missing value";
        double d=to_num(a[ai++]);
        string_detail::pack_put_uint(out, w, std::bit_cast<std::uint64_t>(d), item.size, st.little);
        break;
      }
      case string_detail::PackKind::FixedString: {
        if (ai>=n) throw "Lua: string.pack missing value";
        auto s=string_detail::pack_str_arg(vm, a[ai++], "Lua: string.pack expects string");
        if (s.size()>item.size) throw "Lua: string longer than given size";
        for (char ch: s) string_detail::pack_put_byte(out, w, (unsigned char)ch);
        string_detail::pack_put_pad(out, w, item.size-s.size());
        break;
      }
      case string_detail::PackKind::ZString: {
        if (ai>=n) throw "Lua: string.pack missing value";
        auto s=string_detail::pack_str_arg(vm, a[ai++], "Lua: string.pack expects string");
        for (char ch: s) {
          if (ch=='\0') throw "Lua: string contains zeros";
          string_detail::pack_put_byte(out, w, (unsigned char)ch);
        }
        string_detail::pack_put_byte(out, w, 0u);
        break;
      }
      case string_detail::PackKind::LenString: {
        if (ai>=n) throw "Lua: string.pack missing value";
        auto s=string_detail::pack_str_arg(vm, a[ai++], "Lua: string.pack expects string");
        if (!string_detail::fits_unsigned_bytes((std::uint64_t)s.size(), item.size)) throw "Lua: string length overflow";
        string_detail::pack_put_uint(out, w, (std::uint64_t)s.size(), item.size, st.little);
        for (char ch: s) string_detail::pack_put_byte(out, w, (unsigned char)ch);
        break;
      }
      case string_detail::PackKind::None:
        break;
    }
  }

  return Multi::one(Value::string(vm.H.sp.intern(std::string_view(out.data(), w))));
}

constexpr Multi VM::nf_string_packsize(VM& vm, const Value* a, std::size_t n) {
  if (n<1) throw "Lua: string.packsize(fmt)";
  auto fmt=string_detail::pack_str_arg(vm, a[0], "Lua: string.packsize expects format string");
  std::size_t fi=0;
  std::size_t total=0;
  string_detail::PackState st{};
  string_detail::PackItem item{};
  while (string_detail::next_pack_item(fmt, fi, st, item)) {
    total += string_detail::align_pad(total, item.align);
    switch (item.kind) {
      case string_detail::PackKind::AlignPad: break;
      case string_detail::PackKind::PadByte: total += 1u; break;
      case string_detail::PackKind::SInt:
      case string_detail::PackKind::UInt:
      case string_detail::PackKind::F32:
      case string_detail::PackKind::F64:
      case string_detail::PackKind::FixedString:
        total += item.size;
        break;
      case string_detail::PackKind::LenString:
      case string_detail::PackKind::ZString:
        throw "Lua: variable-length format";
      case string_detail::PackKind::None:
        break;
    }
  }
  return Multi::one(Value::integer((std::int64_t)total));
}

constexpr Multi VM::nf_string_unpack(VM& vm, const Value* a, std::size_t n) {
  if (n<2 || n>3) throw "Lua: string.unpack(fmt, s [,pos])";
  auto fmt=string_detail::pack_str_arg(vm, a[0], "Lua: string.unpack expects format string");
  auto s=string_detail::pack_str_arg(vm, a[1], "Lua: string.unpack expects string");
  std::int64_t len=(std::int64_t)s.size();
  std::int64_t pos=(n>=3) ? to_int(a[2]) : 1;
  pos=string_detail::posrelat(pos, len);
  if (pos<1 || pos>len+1) throw "Lua: initial position out of string";

  std::size_t cursor=(std::size_t)(pos-1);
  std::size_t offset=0;
  std::size_t fi=0;
  std::uint8_t ri=0;
  Multi m{};
  string_detail::PackState st{};
  string_detail::PackItem item{};

  while (string_detail::next_pack_item(fmt, fi, st, item)) {
    std::size_t pad=string_detail::align_pad(offset, item.align);
    if (cursor+pad > s.size()) throw "Lua: data string too short";
    cursor += pad;
    offset += pad;

    auto push = [&](Value v) constexpr {
      if (ri+1 >= MAX_RET) throw "Lua: too many unpack results";
      m.v[ri++]=v;
    };

    switch (item.kind) {
      case string_detail::PackKind::AlignPad:
        break;
      case string_detail::PackKind::PadByte:
        if (cursor>=s.size()) throw "Lua: data string too short";
        ++cursor;
        ++offset;
        break;
      case string_detail::PackKind::SInt: {
        std::uint64_t u=string_detail::unpack_get_uint(s, cursor, item.size, st.little);
        cursor += item.size;
        offset += item.size;
        push(Value::integer(string_detail::unpack_signed_value(u, item.size)));
        break;
      }
      case string_detail::PackKind::UInt: {
        std::uint64_t u=string_detail::unpack_get_uint(s, cursor, item.size, st.little);
        cursor += item.size;
        offset += item.size;
        push(string_detail::unpack_unsigned_value(u));
        break;
      }
      case string_detail::PackKind::F32: {
        std::uint32_t u=(std::uint32_t)string_detail::unpack_get_uint(s, cursor, item.size, st.little);
        cursor += item.size;
        offset += item.size;
        push(Value::number((double)std::bit_cast<float>(u)));
        break;
      }
      case string_detail::PackKind::F64: {
        std::uint64_t u=string_detail::unpack_get_uint(s, cursor, item.size, st.little);
        cursor += item.size;
        offset += item.size;
        push(Value::number(std::bit_cast<double>(u)));
        break;
      }
      case string_detail::PackKind::FixedString: {
        if (cursor+item.size > s.size()) throw "Lua: data string too short";
        push(Value::string(vm.H.sp.intern(s.substr(cursor, item.size))));
        cursor += item.size;
        offset += item.size;
        break;
      }
      case string_detail::PackKind::ZString: {
        std::size_t end=cursor;
        while (end<s.size() && s[end]!='\0') ++end;
        if (end>=s.size()) throw "Lua: data string too short";
        push(Value::string(vm.H.sp.intern(s.substr(cursor, end-cursor))));
        offset += (end-cursor) + 1u;
        cursor = end + 1u;
        break;
      }
      case string_detail::PackKind::LenString: {
        std::uint64_t sz=string_detail::unpack_get_uint(s, cursor, item.size, st.little);
        cursor += item.size;
        offset += item.size;
        if (sz > s.size() - cursor) throw "Lua: data string too short";
        push(Value::string(vm.H.sp.intern(s.substr(cursor, (std::size_t)sz))));
        cursor += (std::size_t)sz;
        offset += (std::size_t)sz;
        break;
      }
      case string_detail::PackKind::None:
        break;
    }
  }

  m.v[ri++]=Value::integer((std::int64_t)cursor + 1);
  m.n=ri;
  return m;
}

consteval void VM::open_string() {
  TableId smod=H.new_table_pow2(6);

  auto reg = [&](std::string_view name, NativeFn fn) {
    std::uint32_t id=reg_native(name, fn);
    table_set(smod, Value::string(H.sp.intern(name)), mk_native(id));
  };

  id_string_gmatch_iter=reg_native("string._gmatch_iter",&VM::nf_string_gmatch_iter);

  reg("len",     &VM::nf_string_len);
  reg("sub",     &VM::nf_string_sub);
  reg("find",    &VM::nf_string_find);
  reg("match",   &VM::nf_string_match);
  reg("gmatch",  &VM::nf_string_gmatch);
  reg("gsub",    &VM::nf_string_gsub);
  reg("byte",    &VM::nf_string_byte);
  reg("char",    &VM::nf_string_char);
  reg("upper",   &VM::nf_string_upper);
  reg("lower",   &VM::nf_string_lower);
  reg("rep",     &VM::nf_string_rep);
  reg("reverse", &VM::nf_string_reverse);
  reg("format",  &VM::nf_string_format);
  reg("pack",    &VM::nf_string_pack);
  reg("packsize",&VM::nf_string_packsize);
  reg("unpack",  &VM::nf_string_unpack);

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
