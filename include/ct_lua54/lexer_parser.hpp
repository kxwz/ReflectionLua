#pragma once

// ---------------- Lexer ----------------
enum class TK : std::uint16_t {
  End,
  Name, Number, String,
  And, Break, Do, Else, ElseIf, EndKw, False, For, Function, Goto, If, In, Local, Nil, Not, Or, Repeat, Return, Then, True, Until, While,
  Plus, Minus, Mul, Div, Idiv, Mod, Pow, Len,
  BitAnd, BitOr, BitXor, Shl, Shr,
  Eq, Ne, Lt, Le, Gt, Ge,
  Assign, LParen, RParen, LBrace, RBrace, LBrack, RBrack,
  Semi, Comma, Dot, DotDot, DotDotDot, Colon, DColon
};

struct Tok { TK k{TK::End}; std::string_view t{}; double num{0}; std::int64_t i{0}; bool raw{false}; };

struct Lexer {
  std::string_view s{};
  std::size_t i{0};

  constexpr char peek(std::size_t off=0) const { return (i+off < s.size()) ? s[i+off] : '\0'; }
  constexpr char get() { return (i < s.size()) ? s[i++] : '\0'; }

  static constexpr bool is_alpha(char c){ return (c>='a'&&c<='z')||(c>='A'&&c<='Z')||c=='_'; }
  static constexpr bool is_digit(char c){ return (c>='0'&&c<='9'); }
  static constexpr bool is_alnum(char c){ return is_alpha(c)||is_digit(c); }
  static constexpr bool is_hex(char c){
    return (c>='0'&&c<='9')||(c>='a'&&c<='f')||(c>='A'&&c<='F');
  }
  static constexpr int hex_val(char c){
    if (c>='0'&&c<='9') return c-'0';
    if (c>='a'&&c<='f') return 10 + (c-'a');
    if (c>='A'&&c<='F') return 10 + (c-'A');
    return -1;
  }
  static constexpr bool is_num_sep(char c){ return c=='_'; }

  constexpr int long_bracket_level(std::size_t pos) const {
    if (pos>=s.size() || s[pos]!='[') return -1;
    std::size_t p=pos+1;
    int lvl=0;
    while (p<s.size() && s[p]=='=') { ++lvl; ++p; }
    if (p<s.size() && s[p]=='[') return lvl;
    return -1;
  }

  constexpr std::string_view read_long_bracket(int lvl, const char* unfinished_err) {
    get(); // '['
    for (int k=0;k<lvl;++k) if (get()!='=') throw "Lua lex: malformed long bracket";
    if (get()!='[') throw "Lua lex: malformed long bracket";

    // Lua ignores a first newline right after the opening delimiter.
    if (peek()=='\r') { get(); if (peek()=='\n') get(); }
    else if (peek()=='\n') get();

    std::size_t st=i;
    for (;;) {
      char c=peek();
      if (!c) throw unfinished_err;
      if (c==']') {
        std::size_t p=i+1;
        int eq=0;
        while (eq<lvl && p<s.size() && s[p]=='=') { ++eq; ++p; }
        if (eq==lvl && p<s.size() && s[p]==']') {
          std::size_t en=i;
          i=p+1;
          return s.substr(st,en-st);
        }
      }
      get();
    }
  }

  constexpr void skip_ws_and_comments() {
    for (;;) {
      char c = peek();
      if (!c) return;
      if (c==' '||c=='\t'||c=='\r'||c=='\n'||c=='\f'||c=='\v') { get(); continue; }
      if (c=='-' && peek(1)=='-') {
        get(); get();
        if (peek()=='[') {
          int lvl=long_bracket_level(i);
          if (lvl>=0) { (void)read_long_bracket(lvl, "Lua lex: unfinished long comment"); continue; }
        }
        while (peek() && peek()!='\n') get();
        continue;
      }
      return;
    }
  }

  static constexpr double parse_dec(std::string_view t) {
    std::size_t k=0;
    double sign=1.0;
    if (k<t.size() && (t[k]=='+'||t[k]=='-')) { if (t[k]=='-') sign=-1.0; ++k; }
    double v=0.0;
    while (k<t.size() && (is_digit(t[k])||is_num_sep(t[k]))) {
      if (is_digit(t[k])) v = v*10.0 + (t[k]-'0');
      ++k;
    }
    if (k<t.size() && t[k]=='.') {
      ++k;
      double place=0.1;
      while (k<t.size() && (is_digit(t[k])||is_num_sep(t[k]))) {
        if (is_digit(t[k])) { v += (t[k]-'0')*place; place*=0.1; }
        ++k;
      }
    }
    if (k<t.size() && (t[k]=='e'||t[k]=='E')) {
      ++k;
      int es=1;
      if (k<t.size() && (t[k]=='+'||t[k]=='-')) { if (t[k]=='-') es=-1; ++k; }
      int e=0;
      while (k<t.size() && (is_digit(t[k])||is_num_sep(t[k]))) {
        if (is_digit(t[k])) e=e*10+(t[k]-'0');
        ++k;
      }
      int p=es*e;
      double pow10=1.0;
      int ap=p<0?-p:p;
      for (int z=0;z<ap;++z) pow10*=10.0;
      v = (p<0)? (v/pow10) : (v*pow10);
    }
    return sign*v;
  }

  static constexpr std::int64_t parse_int10(std::string_view t) {
    std::size_t k=0;
    std::int64_t sign=1;
    if (k<t.size() && (t[k]=='+'||t[k]=='-')) { if (t[k]=='-') sign=-1; ++k; }
    std::int64_t v=0;
    while (k<t.size() && (is_digit(t[k])||is_num_sep(t[k]))) {
      if (is_digit(t[k])) v=v*10+(t[k]-'0');
      ++k;
    }
    return sign*v;
  }

  static constexpr std::int64_t parse_int16(std::string_view t) {
    std::size_t k=0;
    std::int64_t sign=1;
    if (k<t.size() && (t[k]=='+'||t[k]=='-')) { if (t[k]=='-') sign=-1; ++k; }
    if (k+1<t.size() && t[k]=='0' && (t[k+1]=='x'||t[k+1]=='X')) k+=2;
    std::int64_t v=0;
    while (k<t.size() && (is_hex(t[k])||is_num_sep(t[k]))) {
      if (is_hex(t[k])) v = v*16 + hex_val(t[k]);
      ++k;
    }
    return sign*v;
  }

  static constexpr double parse_hex(std::string_view t) {
    std::size_t k=0;
    double sign=1.0;
    if (k<t.size() && (t[k]=='+'||t[k]=='-')) { if (t[k]=='-') sign=-1.0; ++k; }
    if (k+1<t.size() && t[k]=='0' && (t[k+1]=='x'||t[k+1]=='X')) k+=2;

    double v=0.0;
    while (k<t.size() && (is_hex(t[k])||is_num_sep(t[k]))) {
      if (is_hex(t[k])) v = v*16.0 + (double)hex_val(t[k]);
      ++k;
    }

    if (k<t.size() && t[k]=='.') {
      ++k;
      double place = 1.0 / 16.0;
      while (k<t.size() && (is_hex(t[k])||is_num_sep(t[k]))) {
        if (is_hex(t[k])) { v += (double)hex_val(t[k]) * place; place /= 16.0; }
        ++k;
      }
    }

    int exp2=0;
    if (k<t.size() && (t[k]=='p'||t[k]=='P')) {
      ++k;
      int es=1;
      if (k<t.size() && (t[k]=='+'||t[k]=='-')) { if (t[k]=='-') es=-1; ++k; }
      int e=0;
      while (k<t.size() && (is_digit(t[k])||is_num_sep(t[k]))) {
        if (is_digit(t[k])) e=e*10 + (t[k]-'0');
        ++k;
      }
      exp2 = es*e;
    }

    if (exp2>0) for (int z=0; z<exp2; ++z) v*=2.0;
    if (exp2<0) for (int z=0; z<-exp2; ++z) v*=0.5;
    return sign*v;
  }

  constexpr Tok next() {
    skip_ws_and_comments();
    char c=peek();
    if (!c) return {TK::End};

    if (is_alpha(c)) {
      std::size_t st=i;
      while (is_alnum(peek())) get();
      auto t=s.substr(st,i-st);
      if (t=="and") return {TK::And,t};
      if (t=="break") return {TK::Break,t};
      if (t=="do") return {TK::Do,t};
      if (t=="else") return {TK::Else,t};
      if (t=="elseif") return {TK::ElseIf,t};
      if (t=="end") return {TK::EndKw,t};
      if (t=="false") return {TK::False,t};
      if (t=="for") return {TK::For,t};
      if (t=="function") return {TK::Function,t};
      if (t=="goto") return {TK::Goto,t};
      if (t=="if") return {TK::If,t};
      if (t=="in") return {TK::In,t};
      if (t=="local") return {TK::Local,t};
      if (t=="nil") return {TK::Nil,t};
      if (t=="not") return {TK::Not,t};
      if (t=="or") return {TK::Or,t};
      if (t=="repeat") return {TK::Repeat,t};
      if (t=="return") return {TK::Return,t};
      if (t=="then") return {TK::Then,t};
      if (t=="true") return {TK::True,t};
      if (t=="until") return {TK::Until,t};
      if (t=="while") return {TK::While,t};
      return {TK::Name,t};
    }

    if (is_digit(c) || (c=='.' && is_digit(peek(1)))) {
      std::size_t st=i;
      if (c=='0' && (peek(1)=='x' || peek(1)=='X')) {
        get(); get(); // 0x
        bool saw_hex=false;
        bool is_float=false;

        while (is_hex(peek()) || is_num_sep(peek())) { if (is_hex(peek())) saw_hex=true; get(); }
        if (peek()=='.') {
          is_float=true;
          get();
          while (is_hex(peek()) || is_num_sep(peek())) { if (is_hex(peek())) saw_hex=true; get(); }
        }
        if (!saw_hex) throw "Lua lex: malformed hex number";

        if (peek()=='p' || peek()=='P') {
          is_float=true;
          get();
          if (peek()=='+' || peek()=='-') get();
          bool saw_exp_digit=false;
          while (is_digit(peek()) || is_num_sep(peek())) { if (is_digit(peek())) saw_exp_digit=true; get(); }
          if (!saw_exp_digit) throw "Lua lex: malformed hex exponent";
        }

        auto t=s.substr(st,i-st);
        if (is_float) return {TK::Number,t,parse_hex(t),0};
        auto iv=parse_int16(t);
        return {TK::Number,t,(double)iv,iv};
      }

      while (is_digit(peek()) || is_num_sep(peek())) get();
      if (peek()=='.' && (is_digit(peek(1)) || is_num_sep(peek(1)))) {
        get();
        while (is_digit(peek()) || is_num_sep(peek())) get();
      }
      if (peek()=='e'||peek()=='E') {
        get();
        if (peek()=='+'||peek()=='-') get();
        bool saw_exp_digit=false;
        while (is_digit(peek()) || is_num_sep(peek())) { if (is_digit(peek())) saw_exp_digit=true; get(); }
        if (!saw_exp_digit) throw "Lua lex: malformed exponent";
      }
      auto t=s.substr(st,i-st);
      bool is_int=true;
      for (char ch: t) if (ch=='.'||ch=='e'||ch=='E') { is_int=false; break; }
      if (is_int) return {TK::Number,t,(double)parse_int10(t),parse_int10(t)};
      return {TK::Number,t,parse_dec(t),0};
    }

    if (c=='[') {
      int lvl=long_bracket_level(i);
      if (lvl>=0) {
        Tok t; t.k=TK::String; t.t=read_long_bracket(lvl, "Lua lex: unfinished long string"); t.raw=true;
        return t;
      }
    }

    if (c=='\'' || c=='"') {
      char q=get();
      std::size_t st=i;
      while (peek() && peek()!=q) { if (peek()=='\\'){ get(); if (peek()) get(); continue; } get(); }
      if (peek()!=q) throw "Lua lex: unfinished string";
      std::size_t en=i;
      get();
      return {TK::String, s.substr(st,en-st)};
    }

    get();
    switch (c) {
      case '+': return {TK::Plus};
      case '-': return {TK::Minus};
      case '*': return {TK::Mul};
      case '/': if (peek()=='/'){ get(); return {TK::Idiv}; } return {TK::Div};
      case '%': return {TK::Mod};
      case '^': return {TK::Pow};
      case '#': return {TK::Len};
      case '&': return {TK::BitAnd};
      case '|': return {TK::BitOr};
      case '=': if (peek()=='='){ get(); return {TK::Eq}; } return {TK::Assign};
      case '~': if (peek()=='='){ get(); return {TK::Ne}; } return {TK::BitXor};
      case '<':
        if (peek()=='<') { get(); return {TK::Shl}; }
        if (peek()=='='){ get(); return {TK::Le}; }
        return {TK::Lt};
      case '>':
        if (peek()=='>') { get(); return {TK::Shr}; }
        if (peek()=='='){ get(); return {TK::Ge}; }
        return {TK::Gt};
      case '(': return {TK::LParen};
      case ')': return {TK::RParen};
      case '{': return {TK::LBrace};
      case '}': return {TK::RBrace};
      case '[': return {TK::LBrack};
      case ']': return {TK::RBrack};
      case ';': return {TK::Semi};
      case ',': return {TK::Comma};
      case ':': if (peek()==':') { get(); return {TK::DColon}; } return {TK::Colon};
      case '.':
        if (peek()=='.') { get(); if (peek()=='.'){ get(); return {TK::DotDotDot}; } return {TK::DotDot}; }
        return {TK::Dot};
      default: throw "Lua lex: unexpected char";
    }
  }
};

// ---------------- AST arena ----------------
using ExprId = std::uint32_t;
using StmtId = std::uint32_t;

enum class EKind : std::uint8_t { Nil, Bool, Int, Num, Str, Name, Paren, VarArg, Unary, Binary, TableCtor, FuncExpr, Call, Index, Field, Method };
enum class SKind : std::uint8_t { Do, While, Repeat, If, ForNum, ForIn, Local, LocalFunc, FuncStmt, Assign, Return, Break, ExprStmt, Label, Goto };
enum class FieldK : std::uint8_t { Array, Name, Key };

struct Expr {
  EKind k{EKind::Nil};
  TK op{TK::End};
  ExprId a{0}, b{0};
  Range r{};
  StrId s{};
  double num{0};
  std::int64_t i{0};
  bool bo{false};
  ProtoId proto{0};
};

struct Field { FieldK k{FieldK::Array}; StrId name{}; ExprId key{0}; ExprId val{0}; };

struct Stmt {
  SKind k{SKind::Do};
  ExprId e0{0}, e1{0}, e2{0}; // condition / lhs / rhs etc.
  Range  r0{}, r1{}, r2{};    // expr-id lists (namelist, explist, varlist, elseif cond list, elseif-block-id list)
  BRange b0{};                // primary block list (then-block, loop body, do-block)
  StrId  name{};              // for loop var / local function name
  bool flag{false};           // if has else
};

struct Arena {
  std::array<Expr,  MAX_EXPRS> expr{};
  std::uint32_t ne{0};

  std::array<Stmt,  MAX_STMTS> st{};
  std::uint32_t ns{0};

  std::array<ExprId, MAX_LIST> list{};
  std::uint32_t nl{0};

  std::array<StmtId, MAX_LIST> blist{};
  std::uint32_t nbl{0};

  std::array<Field, MAX_FIELDS> fields{};
  std::uint32_t nf{0};

  constexpr ExprId add_expr(const Expr& x){ if (ne>=MAX_EXPRS) throw "Lua: too many expr"; expr[ne]=x; return ne++; }
  constexpr StmtId add_stmt(const Stmt& x){ if (ns>=MAX_STMTS) throw "Lua: too many stmt"; st[ns]=x; return ns++; }

  constexpr Range add_list_span(const ExprId* xs, std::size_t n){
    if (nl + n > MAX_LIST) throw "Lua: list overflow";
    Range r{nl,(std::uint16_t)n};
    for (std::size_t i=0;i<n;++i) list[nl++]=xs[i];
    return r;
  }

  constexpr BRange add_block_span(const StmtId* xs, std::size_t n){
    if (nbl + n > MAX_LIST) throw "Lua: block list overflow";
    BRange r{nbl,(std::uint16_t)n};
    for (std::size_t i=0;i<n;++i) blist[nbl++]=xs[i];
    return r;
  }

  constexpr Range add_fields_span(const Field* fs, std::size_t n){
    if (nf + n > MAX_FIELDS) throw "Lua: fields overflow";
    Range r{nf,(std::uint16_t)n};
    for (std::size_t i=0;i<n;++i) fields[nf++]=fs[i];
    return r;
  }
};

// ---------------- parser ----------------
struct Parser {
  Heap& H;
  Arena& A;
  Lexer L;
  Tok cur{};
  bool allow_vararg{false};

  constexpr Parser(Heap& h, Arena& a, std::string_view src) : H(h), A(a), L{src} { cur=L.next(); }

  constexpr void eat(TK k){ if (cur.k!=k) throw "Lua parse error"; cur=L.next(); }
  constexpr bool accept(TK k){ if (cur.k==k){ cur=L.next(); return true; } return false; }

  constexpr StrId decode_string(std::string_view raw){
    std::array<char, 1024> tmp{};
    std::size_t w=0;

    auto emit = [&](char c) constexpr {
      if (w>=tmp.size()) throw "Lua: string too long";
      tmp[w++]=c;
    };

    auto hexv = [](char c) constexpr -> int {
      if (c>='0'&&c<='9') return c-'0';
      if (c>='a'&&c<='f') return 10 + (c-'a');
      if (c>='A'&&c<='F') return 10 + (c-'A');
      return -1;
    };

    auto is_ws = [](char c) constexpr {
      return c==' '||c=='\t'||c=='\r'||c=='\n'||c=='\f'||c=='\v';
    };

    auto emit_utf8 = [&](std::uint32_t cp) constexpr {
      if (cp > 0x10FFFFu || (cp>=0xD800u && cp<=0xDFFFu)) throw "Lua: bad unicode escape";
      if (cp <= 0x7Fu) { emit((char)cp); return; }
      if (cp <= 0x7FFu) {
        emit((char)(0xC0u | (cp >> 6)));
        emit((char)(0x80u | (cp & 0x3Fu)));
        return;
      }
      if (cp <= 0xFFFFu) {
        emit((char)(0xE0u | (cp >> 12)));
        emit((char)(0x80u | ((cp >> 6) & 0x3Fu)));
        emit((char)(0x80u | (cp & 0x3Fu)));
        return;
      }
      emit((char)(0xF0u | (cp >> 18)));
      emit((char)(0x80u | ((cp >> 12) & 0x3Fu)));
      emit((char)(0x80u | ((cp >> 6) & 0x3Fu)));
      emit((char)(0x80u | (cp & 0x3Fu)));
    };

    for (std::size_t i=0;i<raw.size();) {
      char c=raw[i++];
      if (c!='\\') { emit(c); continue; }
      if (i>=raw.size()) throw "Lua: bad string escape";
      char d=raw[i++];
      switch (d) {
        case 'n': emit('\n'); break;
        case 't': emit('\t'); break;
        case 'r': emit('\r'); break;
        case 'a': emit('\a'); break;
        case 'b': emit('\b'); break;
        case 'f': emit('\f'); break;
        case 'v': emit('\v'); break;
        case '\\': emit('\\'); break;
        case '"': emit('"'); break;
        case '\'': emit('\''); break;
        case 'x': {
          if (i+1>=raw.size()) throw "Lua: bad \\x escape";
          int h1=hexv(raw[i]), h2=hexv(raw[i+1]);
          if (h1<0 || h2<0) throw "Lua: bad \\x escape";
          emit((char)((h1<<4) | h2));
          i += 2;
          break;
        }
        case 'u': {
          if (i>=raw.size() || raw[i]!='{') throw "Lua: bad \\u escape";
          ++i;
          std::uint32_t cp=0;
          std::size_t nd=0;
          while (i<raw.size() && raw[i]!='}') {
            int h=hexv(raw[i]);
            if (h<0) throw "Lua: bad \\u escape";
            cp = (cp<<4) | (std::uint32_t)h;
            ++i; ++nd;
            if (nd>6) throw "Lua: bad \\u escape";
          }
          if (i>=raw.size() || raw[i]!='}' || nd==0) throw "Lua: bad \\u escape";
          ++i;
          emit_utf8(cp);
          break;
        }
        case 'z': {
          while (i<raw.size() && is_ws(raw[i])) ++i;
          break;
        }
        case '0': case '1': case '2': case '3': case '4':
        case '5': case '6': case '7': case '8': case '9': {
          int v = d - '0';
          int nd=1;
          while (nd<3 && i<raw.size() && raw[i]>='0' && raw[i]<='9') {
            v = v*10 + (raw[i]-'0');
            ++i; ++nd;
          }
          if (v>255) throw "Lua: decimal escape too large";
          emit((char)v);
          break;
        }
        default:
          emit(d);
          break;
      }
    }
    return H.sp.intern(std::string_view(tmp.data(), w));
  }

  static constexpr int prec(TK k){
    switch (k) {
      case TK::Or: return 1;
      case TK::And: return 2;
      case TK::Eq: case TK::Ne: case TK::Lt: case TK::Le: case TK::Gt: case TK::Ge: return 3;
      case TK::BitOr: return 4;
      case TK::BitXor: return 5;
      case TK::BitAnd: return 6;
      case TK::Shl: case TK::Shr: return 7;
      case TK::DotDot: return 8;
      case TK::Plus: case TK::Minus: return 9;
      case TK::Mul: case TK::Div: case TK::Idiv: case TK::Mod: return 10;
      case TK::Pow: return 12;
      default: return -1;
    }
  }
  static constexpr bool right_assoc(TK k){ return k==TK::Pow || k==TK::DotDot; }

  constexpr ExprId parse_expr(int minp=0);
  constexpr ExprId parse_prefixexp();
  constexpr ExprId parse_simple();
  constexpr Range  parse_explist();
  constexpr Range  parse_namelist();
  constexpr BRange parse_block_until(std::initializer_list<TK> stops);
  constexpr StmtId parse_stmt();
  constexpr ProtoId parse_chunk();

  constexpr ExprId parse_funcbody(bool add_self=false); // parses: '(' [params|...] ')' block 'end'
};

constexpr BRange Parser::parse_block_until(std::initializer_list<TK> stops) {
  auto is_stop = [&](TK k) constexpr {
    for (auto s: stops) if (k==s) return true;
    return false;
  };
  std::array<StmtId, 2048> ids{};
  std::size_t n=0;
  while (!is_stop(cur.k)) {
    if (n>=ids.size()) throw "Lua: block too many statements";
    ids[n++] = parse_stmt();
  }
  return A.add_block_span(ids.data(), n);
}

constexpr ExprId Parser::parse_funcbody(bool add_self) {
  eat(TK::LParen);

  std::array<ExprId, MAX_ARGS> params{};
  std::size_t pn=0;
  bool vararg=false;

  if (add_self) {
    if (pn>=MAX_ARGS) throw "Lua: too many params";
    Expr selfp; selfp.k=EKind::Str; selfp.s=H.sp.intern("self");
    params[pn++]=A.add_expr(selfp);
  }

  if (!accept(TK::RParen)) {
    while (true) {
      if (cur.k==TK::Name) {
        if (pn>=MAX_ARGS) throw "Lua: too many params";
        StrId nm=H.sp.intern(cur.t);
        cur=L.next();
        Expr pe; pe.k=EKind::Str; pe.s=nm;
        params[pn++]=A.add_expr(pe);
        if (accept(TK::Comma)) continue;
        eat(TK::RParen);
        break;
      }
      if (accept(TK::DotDotDot)) {
        vararg=true;
        eat(TK::RParen);
        break;
      }
      throw "Lua: bad param list";
    }
  }

  Range pr=A.add_list_span(params.data(), pn);
  bool prev_allow_vararg=allow_vararg;
  allow_vararg=vararg;
  BRange blk=parse_block_until({TK::EndKw});
  allow_vararg=prev_allow_vararg;
  eat(TK::EndKw);

  ProtoId pid=H.new_proto();
  H.protos[pid.id].params_off=pr.off;
  H.protos[pid.id].params_n=pr.n;
  H.protos[pid.id].is_vararg=vararg;
  H.protos[pid.id].block=blk;

  Expr e; e.k=EKind::FuncExpr; e.proto=pid;
  return A.add_expr(e);
}

constexpr ExprId Parser::parse_simple() {
  if (accept(TK::Nil)) return A.add_expr(Expr{EKind::Nil});
  if (accept(TK::True)) { Expr e; e.k=EKind::Bool; e.bo=true; return A.add_expr(e); }
  if (accept(TK::False)){ Expr e; e.k=EKind::Bool; e.bo=false; return A.add_expr(e); }

  if (cur.k==TK::Number) {
    Tok t=cur; cur=L.next();
    bool is_hex = t.t.size()>=2 && t.t[0]=='0' && (t.t[1]=='x'||t.t[1]=='X');
    bool is_int=true;
    for (char ch: t.t) {
      if (ch=='.') { is_int=false; break; }
      if (!is_hex && (ch=='e'||ch=='E')) { is_int=false; break; }
      if (is_hex && (ch=='p'||ch=='P')) { is_int=false; break; }
    }
    if (is_int) { Expr e; e.k=EKind::Int; e.i=t.i; return A.add_expr(e); }
    Expr e; e.k=EKind::Num; e.num=t.num; return A.add_expr(e);
  }

  if (cur.k==TK::String) {
    Tok t=cur; cur=L.next();
    Expr e; e.k=EKind::Str; e.s=t.raw ? H.sp.intern(t.t) : decode_string(t.t); return A.add_expr(e);
  }

  if (accept(TK::DotDotDot)) {
    if (!allow_vararg) throw "Lua: cannot use '...' outside a vararg function";
    Expr e; e.k=EKind::VarArg; return A.add_expr(e);
  }

  if (accept(TK::Function)) {
    return parse_funcbody(); // function expression
  }

  if (accept(TK::LBrace)) {
    std::array<Field, 512> fs{};
    std::size_t fn=0;
    auto pushf=[&](Field f){ if (fn>=fs.size()) throw "Lua: too many ctor fields"; fs[fn++]=f; };

    if (!accept(TK::RBrace)) {
      while (true) {
        if (accept(TK::LBrack)) {
          ExprId key=parse_expr(0);
          eat(TK::RBrack);
          eat(TK::Assign);
          ExprId val=parse_expr(0);
          pushf(Field{FieldK::Key,{},key,val});
        } else {
          // Parse full expression fields (e.g. mr()).
          // If followed by '=', only a bare name is accepted as name-field syntax.
          ExprId lhs=parse_expr(0);
          if (accept(TK::Assign)) {
            const Expr& le=A.expr[lhs];
            if (le.k!=EKind::Name) throw "Lua: expected name in table field";
            ExprId val=parse_expr(0);
            pushf(Field{FieldK::Name,le.s,0,val});
          } else {
            pushf(Field{FieldK::Array,{},0,lhs});
          }
        }

        if (accept(TK::Comma) || accept(TK::Semi)) {
          if (accept(TK::RBrace)) break;
          continue;
        }
        eat(TK::RBrace);
        break;
      }
    }
    Range fr=A.add_fields_span(fs.data(), fn);
    Expr e; e.k=EKind::TableCtor; e.r=fr;
    return A.add_expr(e);
  }

  if (accept(TK::LParen)) {
    ExprId inner=parse_expr(0);
    eat(TK::RParen);
    Expr e; e.k=EKind::Paren; e.a=inner;
    return A.add_expr(e);
  }

  if (cur.k==TK::Name) {
    StrId nm=H.sp.intern(cur.t);
    cur=L.next();
    Expr e; e.k=EKind::Name; e.s=nm;
    return A.add_expr(e);
  }

  throw "Lua: expected expression";
}

constexpr ExprId Parser::parse_prefixexp() {
  ExprId e=parse_simple();

  for (;;) {
    if (accept(TK::Dot)) {
      if (cur.k!=TK::Name) throw "Lua: expected field name";
      StrId fn=H.sp.intern(cur.t);
      cur=L.next();
      Expr f; f.k=EKind::Field; f.a=e; f.s=fn;
      e=A.add_expr(f);
      continue;
    }
    if (accept(TK::LBrack)) {
      ExprId k=parse_expr(0);
      eat(TK::RBrack);
      Expr ix; ix.k=EKind::Index; ix.a=e; ix.b=k;
      e=A.add_expr(ix);
      continue;
    }
    if (accept(TK::Colon)) {
      if (cur.k!=TK::Name) throw "Lua: expected method name";
      StrId mn=H.sp.intern(cur.t);
      cur=L.next();

      Range args{};
      if (accept(TK::LParen)) {
        if (!accept(TK::RParen)) { args=parse_explist(); eat(TK::RParen); }
      } else if (cur.k==TK::String) {
        Tok t=cur; cur=L.next();
        Expr se; se.k=EKind::Str; se.s=t.raw ? H.sp.intern(t.t) : decode_string(t.t);
        ExprId sid=A.add_expr(se);
        ExprId tmp[1]{sid};
        args=A.add_list_span(tmp,1);
      } else if (cur.k==TK::LBrace) {
        ExprId tid=parse_simple();
        ExprId tmp[1]{tid};
        args=A.add_list_span(tmp,1);
      } else throw "Lua: expected args";

      Expr m; m.k=EKind::Method; m.a=e; m.s=mn; m.r=args;
      e=A.add_expr(m);
      continue;
    }

    if (cur.k==TK::LParen || cur.k==TK::String || cur.k==TK::LBrace) {
      Range args{};
      if (accept(TK::LParen)) {
        if (!accept(TK::RParen)) { args=parse_explist(); eat(TK::RParen); }
      } else if (cur.k==TK::String) {
        Tok t=cur; cur=L.next();
        Expr se; se.k=EKind::Str; se.s=t.raw ? H.sp.intern(t.t) : decode_string(t.t);
        ExprId sid=A.add_expr(se);
        ExprId tmp[1]{sid};
        args=A.add_list_span(tmp,1);
      } else {
        ExprId tid=parse_simple();
        ExprId tmp[1]{tid};
        args=A.add_list_span(tmp,1);
      }
      Expr c; c.k=EKind::Call; c.a=e; c.r=args;
      e=A.add_expr(c);
      continue;
    }

    break;
  }
  return e;
}

constexpr ExprId Parser::parse_expr(int minp) {
  if (cur.k==TK::Not || cur.k==TK::Minus || cur.k==TK::Len || cur.k==TK::BitXor) {
    TK op=cur.k; cur=L.next();
    ExprId rhs=parse_expr(11);
    Expr u; u.k=EKind::Unary; u.op=op; u.a=rhs;
    ExprId left=A.add_expr(u);

    for (;;) {
      int p=prec(cur.k);
      if (p<minp) break;
      TK bop=cur.k; cur=L.next();
      int nextp = p + (right_assoc(bop)?0:1);
      ExprId right=parse_expr(nextp);
      Expr b; b.k=EKind::Binary; b.op=bop; b.a=left; b.b=right;
      left=A.add_expr(b);
    }
    return left;
  }

  ExprId left=parse_prefixexp();
  for (;;) {
    int p=prec(cur.k);
    if (p<minp) break;
    TK op=cur.k; cur=L.next();
    int nextp = p + (right_assoc(op)?0:1);
    ExprId right=parse_expr(nextp);
    Expr b; b.k=EKind::Binary; b.op=op; b.a=left; b.b=right;
    left=A.add_expr(b);
  }
  return left;
}

constexpr Range Parser::parse_explist() {
  std::array<ExprId, MAX_ARGS> xs{};
  std::size_t n=0;
  while (true) {
    if (n>=MAX_ARGS) throw "Lua: explist too long";
    xs[n++]=parse_expr(0);
    if (!accept(TK::Comma)) break;
  }
  return A.add_list_span(xs.data(), n);
}

constexpr Range Parser::parse_namelist() {
  std::array<ExprId, MAX_ARGS> xs{};
  std::size_t n=0;
  while (true) {
    if (cur.k!=TK::Name) throw "Lua: expected name";
    if (n>=MAX_ARGS) throw "Lua: namelist too long";
    StrId nm=H.sp.intern(cur.t);
    cur=L.next();
    Expr e; e.k=EKind::Str; e.s=nm;
    xs[n++]=A.add_expr(e);
    if (!accept(TK::Comma)) break;
  }
  return A.add_list_span(xs.data(), n);
}

constexpr StmtId Parser::parse_stmt() {
  while (accept(TK::Semi)) {}

  auto is_assign_target = [&](ExprId id) constexpr {
    EKind k=A.expr[id].k;
    return k==EKind::Name || k==EKind::Field || k==EKind::Index;
  };

  if (accept(TK::Return)) {
    Range r{};
    if (cur.k!=TK::EndKw && cur.k!=TK::Else && cur.k!=TK::ElseIf && cur.k!=TK::Until && cur.k!=TK::End) {
      if (cur.k!=TK::Semi) r=parse_explist();
    }
    accept(TK::Semi);
    Stmt s; s.k=SKind::Return; s.r0=r;
    return A.add_stmt(s);
  }

  if (accept(TK::Break)) { accept(TK::Semi); Stmt s; s.k=SKind::Break; return A.add_stmt(s); }

  if (accept(TK::Goto)) {
    if (cur.k!=TK::Name) throw "Lua: expected label name after goto";
    Stmt s; s.k=SKind::Goto; s.name=H.sp.intern(cur.t);
    cur=L.next();
    accept(TK::Semi);
    return A.add_stmt(s);
  }

  if (accept(TK::DColon)) {
    if (cur.k!=TK::Name) throw "Lua: expected label name";
    Stmt s; s.k=SKind::Label; s.name=H.sp.intern(cur.t);
    cur=L.next();
    eat(TK::DColon);
    return A.add_stmt(s);
  }

  if (accept(TK::Do)) {
    BRange blk=parse_block_until({TK::EndKw});
    eat(TK::EndKw);
    Stmt s; s.k=SKind::Do; s.b0=blk;
    return A.add_stmt(s);
  }

  if (accept(TK::While)) {
    ExprId cond=parse_expr(0);
    eat(TK::Do);
    BRange blk=parse_block_until({TK::EndKw});
    eat(TK::EndKw);
    Stmt s; s.k=SKind::While; s.e0=cond; s.b0=blk;
    return A.add_stmt(s);
  }

  if (accept(TK::Repeat)) {
    BRange blk=parse_block_until({TK::Until});
    eat(TK::Until);
    ExprId cond=parse_expr(0);
    Stmt s; s.k=SKind::Repeat; s.e0=cond; s.b0=blk;
    return A.add_stmt(s);
  }

  if (accept(TK::If)) {
    ExprId c0=parse_expr(0);
    eat(TK::Then);
    BRange b0=parse_block_until({TK::ElseIf, TK::Else, TK::EndKw});

    std::array<ExprId, 64> conds{};
    std::array<StmtId,  64> blocks{};
    std::size_t n=0;

    while (accept(TK::ElseIf)) {
      if (n>=64) throw "Lua: too many elseif";
      ExprId ci=parse_expr(0);
      eat(TK::Then);
      BRange bi=parse_block_until({TK::ElseIf, TK::Else, TK::EndKw});
      Stmt ds; ds.k=SKind::Do; ds.b0=bi;
      blocks[n]=A.add_stmt(ds);
      conds[n]=ci;
      ++n;
    }

    StmtId else_block=StmtId(UINT32_MAX);
    if (accept(TK::Else)) {
      BRange be=parse_block_until({TK::EndKw});
      Stmt ds; ds.k=SKind::Do; ds.b0=be;
      else_block=A.add_stmt(ds);
    }
    eat(TK::EndKw);

    std::array<ExprId, 64> cond_ids{};
    std::array<ExprId, 64> blk_ids{};
    for (std::size_t i=0;i<n;++i) {
      cond_ids[i]=conds[i];
      Expr ei; ei.k=EKind::Int; ei.i=(std::int64_t)blocks[i]; // store stmt-id in Expr.i
      blk_ids[i]=A.add_expr(ei);
    }
    Range rc=A.add_list_span(cond_ids.data(), n);
    Range rb=A.add_list_span(blk_ids.data(), n);

    Stmt s; s.k=SKind::If; s.e0=c0; s.b0=b0; s.r1=rc; s.r2=rb;
    if (else_block!=StmtId(UINT32_MAX)) { s.flag=true; s.e1=(ExprId)else_block; }
    return A.add_stmt(s);
  }

  if (accept(TK::For)) {
    if (cur.k!=TK::Name) throw "Lua: expected for name";
    StrId var=H.sp.intern(cur.t);
    cur=L.next();

    if (accept(TK::Assign)) {
      ExprId e1=parse_expr(0); eat(TK::Comma);
      ExprId e2=parse_expr(0);
      ExprId e3{};
      if (accept(TK::Comma)) e3=parse_expr(0);
      else { Expr one; one.k=EKind::Int; one.i=1; e3=A.add_expr(one); }
      eat(TK::Do);
      BRange blk=parse_block_until({TK::EndKw});
      eat(TK::EndKw);
      Stmt s; s.k=SKind::ForNum; s.name=var; s.e0=e1; s.e1=e2; s.e2=e3; s.b0=blk;
      return A.add_stmt(s);
    }

    std::array<ExprId, MAX_ARGS> names{};
    std::size_t nn=0;
    { Expr ne; ne.k=EKind::Str; ne.s=var; names[nn++]=A.add_expr(ne); }
    while (accept(TK::Comma)) {
      if (cur.k!=TK::Name) throw "Lua: expected name";
      if (nn>=MAX_ARGS) throw "Lua: too many for-in vars";
      Expr ne; ne.k=EKind::Str; ne.s=H.sp.intern(cur.t);
      cur=L.next();
      names[nn++]=A.add_expr(ne);
    }
    eat(TK::In);
    Range expl=parse_explist();
    eat(TK::Do);
    BRange blk=parse_block_until({TK::EndKw});
    eat(TK::EndKw);
    Range nlr=A.add_list_span(names.data(), nn);
    Stmt s; s.k=SKind::ForIn; s.r0=nlr; s.r1=expl; s.b0=blk;
    return A.add_stmt(s);
  }

  if (accept(TK::Local)) {
    if (accept(TK::Function)) {
      if (cur.k!=TK::Name) throw "Lua: expected local function name";
      StrId fn=H.sp.intern(cur.t);
      cur=L.next();
      ExprId fexpr=parse_funcbody(); // next token is '('
      Stmt s; s.k=SKind::LocalFunc; s.name=fn; s.e0=fexpr;
      return A.add_stmt(s);
    }
    Range names=parse_namelist();
    Range exps{};
    if (accept(TK::Assign)) exps=parse_explist();
    Stmt s; s.k=SKind::Local; s.r0=names; s.r1=exps;
    return A.add_stmt(s);
  }

  if (accept(TK::Function)) {
    if (cur.k!=TK::Name) throw "Lua: expected function name";
    Expr lhs; lhs.k=EKind::Name; lhs.s=H.sp.intern(cur.t);
    ExprId lhs_id=A.add_expr(lhs);
    cur=L.next();

    while (accept(TK::Dot)) {
      if (cur.k!=TK::Name) throw "Lua: expected field name";
      Expr f; f.k=EKind::Field; f.a=lhs_id; f.s=H.sp.intern(cur.t);
      lhs_id=A.add_expr(f);
      cur=L.next();
    }

    bool method=false;
    if (accept(TK::Colon)) {
      if (cur.k!=TK::Name) throw "Lua: expected method name";
      Expr f; f.k=EKind::Field; f.a=lhs_id; f.s=H.sp.intern(cur.t);
      lhs_id=A.add_expr(f);
      cur=L.next();
      method=true;
    }

    ExprId fexpr=parse_funcbody(method); // next token is '('

    Stmt s; s.k=SKind::FuncStmt; s.e0=lhs_id; s.e1=fexpr;
    return A.add_stmt(s);
  }

  ExprId pfx=parse_prefixexp();
  if (A.expr[pfx].k==EKind::Call || A.expr[pfx].k==EKind::Method) {
    Stmt s; s.k=SKind::ExprStmt; s.e0=pfx; accept(TK::Semi); return A.add_stmt(s);
  }
  if (!is_assign_target(pfx)) throw "Lua: invalid assignment target";

  std::array<ExprId, MAX_ARGS> vars{};
  std::size_t vn=0;
  vars[vn++]=pfx;
  while (accept(TK::Comma)) {
    if (vn>=MAX_ARGS) throw "Lua: too many vars";
    ExprId v=parse_prefixexp();
    if (!is_assign_target(v)) throw "Lua: invalid assignment target";
    vars[vn++]=v;
  }
  eat(TK::Assign);
  Range rhs=parse_explist();
  Range lhs=A.add_list_span(vars.data(), vn);
  Stmt s; s.k=SKind::Assign; s.r0=lhs; s.r1=rhs;
  accept(TK::Semi);
  return A.add_stmt(s);
}

constexpr ProtoId Parser::parse_chunk() {
  BRange blk=parse_block_until({TK::End});
  ProtoId pid=H.new_proto();
  H.protos[pid.id].block=blk;
  return pid;
}

