#pragma once

namespace ct_lua54 {

struct RuntimeError {
  const char* msg{nullptr};
};

// ---------------- budgets ----------------
static constexpr std::size_t MAX_STRINGS   = 2048;
static constexpr std::size_t MAX_STR_BYTES = 128 * 1024;

static constexpr std::size_t MAX_TABLES    = 512;
static constexpr std::size_t MAX_ENTRIES   = 16384;

static constexpr std::size_t MAX_ENVS      = 2048;
static constexpr std::size_t MAX_BINDINGS  = 128;
static constexpr std::size_t MAX_CELLS     = 16384;

static constexpr std::size_t MAX_PROTOS    = 1024;
static constexpr std::size_t MAX_EXPRS     = 65536;
static constexpr std::size_t MAX_STMTS     = 32768;

static constexpr std::size_t MAX_LIST      = 65536;
static constexpr std::size_t MAX_FIELDS    = 32768;

static constexpr std::size_t MAX_RET       = 32;
static constexpr std::size_t MAX_ARGS      = 32;
static constexpr std::size_t MAX_PRINT_BYTES = 32 * 1024;

static constexpr std::size_t STEP_LIMIT    = 2'000'000;

// ---------------- string pool ----------------
struct StrId { std::uint32_t id{0}; };

struct StringPool {
  std::array<std::uint32_t, MAX_STRINGS> off{};
  std::array<std::uint32_t, MAX_STRINGS> len{};
  std::array<std::uint32_t, MAX_STRINGS> hash{};
  std::array<char, MAX_STR_BYTES> bytes{};

  std::uint32_t count{0};
  std::uint32_t used{0};

  static constexpr std::uint32_t fnv1a(std::string_view s) {
    std::uint32_t h = 2166136261u;
    for (unsigned char c : s) { h ^= c; h *= 16777619u; }
    return h ? h : 1u;
  }

  constexpr std::string_view view(StrId sid) const {
    return { bytes.data() + off[sid.id], len[sid.id] };
  }

  constexpr const char* c_str(StrId sid) const {
    return bytes.data() + off[sid.id];
  }

  constexpr StrId intern(std::string_view s) {
    std::uint32_t h = fnv1a(s);
    for (std::uint32_t i = 0; i < count; ++i) {
      if (hash[i] == h && len[i] == s.size()) {
        auto ex = std::string_view(bytes.data() + off[i], len[i]);
        if (ex == s) return {i};
      }
    }
    if (count >= MAX_STRINGS) throw "Lua: string pool overflow";
    if (used + s.size() + 1 > MAX_STR_BYTES) throw "Lua: string bytes overflow";
    std::uint32_t id = count++;
    off[id] = used;
    len[id] = (std::uint32_t)s.size();
    hash[id] = h;
    for (std::size_t k = 0; k < s.size(); ++k) bytes[used + (std::uint32_t)k] = s[k];
    bytes[used + (std::uint32_t)s.size()] = '\0';
    used += (std::uint32_t)s.size() + 1u;
    return {id};
  }
};

// ---------------- value model (constexpr-safe, no union) ----------------
struct TableId { std::uint32_t id{0}; };
struct EnvId   { std::uint32_t id{0}; };
struct CellId  { std::uint32_t id{0}; };
struct ProtoId { std::uint32_t id{0}; };
struct UDataId { std::uint32_t id{0}; };

enum class Tag : std::uint8_t { Nil, Bool, Int, Num, Str, Table, Func, UData };
struct FuncRef { bool is_native{false}; std::uint32_t id{0}; };

struct Value {
  Tag tag{Tag::Nil};
  bool        b{false};
  std::int64_t i{0};
  double      n{0.0};
  StrId       s{};
  TableId     t{};
  FuncRef     f{};
  UDataId     u{};

  static constexpr Value nil() { return {}; }
  static constexpr Value boolean(bool x){ Value v; v.tag=Tag::Bool; v.b=x; return v; }
  static constexpr Value integer(std::int64_t x){ Value v; v.tag=Tag::Int; v.i=x; return v; }
  static constexpr Value number(double x){ Value v; v.tag=Tag::Num; v.n=x; return v; }
  static constexpr Value string(StrId x){ Value v; v.tag=Tag::Str; v.s=x; return v; }
  static constexpr Value table(TableId x){ Value v; v.tag=Tag::Table; v.t=x; return v; }
  static constexpr Value func_native(std::uint32_t nid){ Value v; v.tag=Tag::Func; v.f={true,nid}; return v; }
  static constexpr Value func_closure(std::uint32_t cid){ Value v; v.tag=Tag::Func; v.f={false,cid}; return v; }
  static constexpr Value udata(UDataId x){ Value v; v.tag=Tag::UData; v.u=x; return v; }

  constexpr bool is_nil() const { return tag==Tag::Nil; }
};

static constexpr bool as_exact_i64(double x, std::int64_t& out) {
  if (!(x==x)) return false;
  if (x < -9223372036854775808.0 || x > 9223372036854775807.0) return false;
  std::int64_t i=(std::int64_t)x;
  if ((double)i != x) return false;
  out=i;
  return true;
}

static constexpr Value canonical_table_key(const Value& key) {
  if (key.tag != Tag::Num) return key;
  std::int64_t i=0;
  if (as_exact_i64(key.n, i)) return Value::integer(i);
  return key;
}

static constexpr bool numeric_eq_exact(const Value& a, const Value& b) {
  if (a.tag==Tag::Int && b.tag==Tag::Int) return a.i==b.i;
  if (a.tag==Tag::Num && b.tag==Tag::Num) return a.n==b.n;
  if (a.tag==Tag::Int && b.tag==Tag::Num) {
    std::int64_t bi=0;
    return as_exact_i64(b.n, bi) && a.i==bi;
  }
  if (a.tag==Tag::Num && b.tag==Tag::Int) {
    std::int64_t ai=0;
    return as_exact_i64(a.n, ai) && ai==b.i;
  }
  return false;
}

static constexpr bool truthy(const Value& v) {
  if (v.tag==Tag::Nil) return false;
  if (v.tag==Tag::Bool) return v.b;
  return true;
}

struct Multi {
  std::array<Value, MAX_RET> v{};
  std::uint8_t n{0};
  static constexpr Multi none(){ return {}; }
  static constexpr Multi one(Value x){ Multi m; m.v[0]=x; m.n=1; return m; }
};

struct VM; // fwd
using NativeFn = Multi(*)(VM&, const Value*, std::size_t);

// ---------------- heap: tables, envs, cells, closures, userdata ----------------
struct Entry { Value k{}; Value v{}; bool used{false}; bool tomb{false}; };
struct TableObj { std::uint32_t off{0}; std::uint32_t cap{0}; std::uint32_t size{0}; TableId mt{0}; };

struct Cell { Value v{}; };
struct Binding { StrId name{}; CellId cell{}; };

struct EnvObj {
  EnvId outer{0};
  std::array<Binding, MAX_BINDINGS> b{};
  std::uint16_t n{0};
  TableId env_table{0}; // _ENV
};

struct UData {
  TableId mt{0};
  TableId state{0};
  StrId type_name{};
};

struct BRange { std::uint32_t off{0}; std::uint16_t n{0}; }; // block stmt-id list
struct Range  { std::uint32_t off{0}; std::uint16_t n{0}; }; // expr-id / field list

struct Proto {
  BRange block{};                 // list of StmtId for the block
  std::uint32_t params_off{0};    // A.list (expr ids) of param-name Exprs (EKind::Str)
  std::uint16_t params_n{0};
  bool is_vararg{false};
};

struct Closure {
  ProtoId proto{0};
  EnvId   def_env{0};
  TableId env_table{0}; // captured _ENV
};

struct Heap {
  StringPool sp{};

  std::array<TableObj, MAX_TABLES> tables{};
  std::uint32_t table_count{0};

  std::array<Entry, MAX_ENTRIES> entries{};
  std::uint32_t entry_used{0};

  std::array<EnvObj, MAX_ENVS> envs{};
  std::uint32_t env_count{0};

  std::array<Cell, MAX_CELLS> cells{};
  std::uint32_t cell_count{0};

  std::array<Proto, MAX_PROTOS> protos{};
  std::uint32_t proto_count{0};

  std::array<Closure, MAX_PROTOS> closures{};
  std::uint32_t closure_count{0};

  std::array<UData, MAX_CELLS> udata{};
  std::uint32_t udata_count{0};

  std::array<TableId, 16> type_mt{};

  static constexpr std::uint32_t mix(std::uint32_t x){
    x ^= x >> 16; x *= 0x7feb352du;
    x ^= x >> 15; x *= 0x846ca68bu;
    x ^= x >> 16; return x ? x : 1u;
  }

  constexpr TableId new_table_pow2(std::uint32_t pow2=8) {
    if (table_count >= MAX_TABLES) throw "Lua: too many tables";
    std::uint32_t cap = 1u << pow2;
    if (entry_used + cap > MAX_ENTRIES) throw "Lua: table entry pool overflow";
    TableId tid{table_count++};
    TableObj& t = tables[tid.id];
    t.off=entry_used; t.cap=cap; t.size=0; t.mt=TableId{0};
    for (std::uint32_t i=0;i<cap;++i) entries[entry_used+i]=Entry{};
    entry_used += cap;
    return tid;
  }

  constexpr Entry* ent(TableId t){ return entries.data() + tables[t.id].off; }
  constexpr const Entry* ent(TableId t) const { return entries.data() + tables[t.id].off; }

  constexpr std::uint32_t hash_key(const Value& k) const {
    Value key = canonical_table_key(k);
    switch (key.tag) {
      case Tag::Bool: return mix(key.b ? 0xB001u : 0xB000u);
      case Tag::Int:  return mix((std::uint32_t)key.i ^ (std::uint32_t)(key.i>>32));
      case Tag::Num:  {
        double n = (key.n==0.0) ? 0.0 : key.n;
        std::uint64_t bits = std::bit_cast<std::uint64_t>(n);
        return mix((std::uint32_t)bits ^ (std::uint32_t)(bits>>32));
      }
      case Tag::Str:  return mix(sp.hash[key.s.id]);
      case Tag::Table:return mix(0x71000000u + key.t.id);
      case Tag::Func: return mix(0x72000000u + key.f.id + (key.f.is_native?123u:0u));
      case Tag::UData:return mix(0x73000000u + key.u.id);
      case Tag::Nil:  return 0;
    }
    return 1u;
  }

  static constexpr bool key_eq(const Value& a, const Value& b) {
    if (a.tag==Tag::Int || a.tag==Tag::Num || b.tag==Tag::Int || b.tag==Tag::Num) return numeric_eq_exact(a, b);
    if (a.tag != b.tag) return false;
    switch (a.tag) {
      case Tag::Nil:  return true;
      case Tag::Bool: return a.b==b.b;
      case Tag::Int:  return false;
      case Tag::Num:  return false;
      case Tag::Str:  return a.s.id==b.s.id;
      case Tag::Table:return a.t.id==b.t.id;
      case Tag::Func: return a.f.id==b.f.id && a.f.is_native==b.f.is_native;
      case Tag::UData:return a.u.id==b.u.id;
    }
    return false;
  }

  constexpr Value rawget(TableId t, const Value& key) const {
    if (key.tag==Tag::Nil) return Value::nil();
    Value lookup_key = canonical_table_key(key);
    const TableObj& T = tables[t.id];
    const Entry* es = ent(t);
    std::uint32_t mask = T.cap - 1u;
    std::uint32_t h = hash_key(lookup_key);
    for (std::uint32_t probe=0; probe<T.cap; ++probe) {
      std::uint32_t idx=(h+probe)&mask;
      const Entry& e = es[idx];
      if (!e.used) { if (!e.tomb) return Value::nil(); }
      else if (key_eq(e.k,lookup_key)) return e.v;
    }
    return Value::nil();
  }

  constexpr void rawset(TableId t, const Value& key, const Value& val) {
    if (key.tag==Tag::Nil) throw "Lua: table index is nil";
    if (key.tag==Tag::Num && !(key.n==key.n)) throw "Lua: table index is NaN";
    Value store_key = canonical_table_key(key);
    TableObj& T = tables[t.id];
    Entry* es = ent(t);
    std::uint32_t mask = T.cap - 1u;
    std::uint32_t h = hash_key(store_key);
    std::int32_t first_tomb=-1;

    for (std::uint32_t probe=0; probe<T.cap; ++probe) {
      std::uint32_t idx=(h+probe)&mask;
      Entry& e = es[idx];
      if (!e.used) {
        if (e.tomb) { if (first_tomb<0) first_tomb=(std::int32_t)idx; continue; }
        std::uint32_t put = first_tomb>=0 ? (std::uint32_t)first_tomb : idx;
        Entry& d = es[put];
        if (val.tag==Tag::Nil) { d.used=false; d.tomb=true; return; }
        d.k=store_key; d.v=val; d.used=true; d.tomb=false; ++T.size; return;
      }
      if (key_eq(e.k,store_key)) {
        if (val.tag==Tag::Nil) { e.used=false; e.tomb=true; --T.size; }
        else e.v=val;
        return;
      }
    }
    throw "Lua: table full (no rehash in this build)";
  }

  constexpr CellId new_cell(Value v) {
    if (cell_count >= MAX_CELLS) throw "Lua: too many cells";
    CellId id{cell_count++};
    cells[id.id].v=v;
    return id;
  }

  constexpr EnvId new_env(EnvId outer, TableId env_table) {
    if (env_count >= MAX_ENVS) throw "Lua: too many envs";
    EnvId id{env_count++};
    envs[id.id] = EnvObj{};
    envs[id.id].outer = outer;
    envs[id.id].env_table = env_table;
    return id;
  }

  constexpr void env_add(EnvId e, StrId name, Value init) {
    EnvObj& E = envs[e.id];
    if (E.n >= MAX_BINDINGS) throw "Lua: too many locals in block";
    E.b[E.n++] = Binding{name, new_cell(init)};
  }

  constexpr CellId env_find(EnvId e, StrId name) const {
    EnvId cur = e;
    for (;;) {
      const EnvObj& E = envs[cur.id];
      for (std::uint16_t i=E.n; i>0; --i) {
        if (E.b[i-1].name.id==name.id) return E.b[i-1].cell;
      }
      if (cur.id==0) break;
      cur = E.outer;
    }
    return CellId{UINT32_MAX};
  }

  constexpr ProtoId new_proto() {
    if (proto_count >= MAX_PROTOS) throw "Lua: too many prototypes";
    ProtoId id{proto_count++};
    protos[id.id] = Proto{};
    return id;
  }

  constexpr std::uint32_t new_closure(ProtoId p, EnvId def, TableId envt) {
    if (closure_count >= closures.size()) throw "Lua: too many closures";
    std::uint32_t id = closure_count++;
    closures[id] = Closure{p, def, envt};
    return id;
  }

  constexpr UDataId new_udata(TableId mt, TableId state, StrId type_name) {
    if (udata_count >= udata.size()) throw "Lua: too many userdata";
    UDataId id{udata_count++};
    udata[id.id] = UData{mt, state, type_name};
    return id;
  }

};
