#pragma once

namespace ct_lua54 {

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

  constexpr StrId intern(std::string_view s) {
    std::uint32_t h = fnv1a(s);
    for (std::uint32_t i = 0; i < count; ++i) {
      if (hash[i] == h && len[i] == s.size()) {
        auto ex = std::string_view(bytes.data() + off[i], len[i]);
        if (ex == s) return {i};
      }
    }
    if (count >= MAX_STRINGS) throw "Lua: string pool overflow";
    if (used + s.size() > MAX_STR_BYTES) throw "Lua: string bytes overflow";
    std::uint32_t id = count++;
    off[id] = used;
    len[id] = (std::uint32_t)s.size();
    hash[id] = h;
    for (std::size_t k = 0; k < s.size(); ++k) bytes[used + (std::uint32_t)k] = s[k];
    used += (std::uint32_t)s.size();
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

struct UData { TableId mt{0}; };

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
    switch (k.tag) {
      case Tag::Bool: return mix(k.b ? 0xB001u : 0xB000u);
      case Tag::Int:  return mix((std::uint32_t)k.i ^ (std::uint32_t)(k.i>>32));
      case Tag::Num:  { std::int64_t y = (std::int64_t)(k.n * 1000003.0); return mix((std::uint32_t)y ^ (std::uint32_t)(y>>32)); }
      case Tag::Str:  return mix(sp.hash[k.s.id]);
      case Tag::Table:return mix(0x71000000u + k.t.id);
      case Tag::Func: return mix(0x72000000u + k.f.id + (k.f.is_native?123u:0u));
      case Tag::UData:return mix(0x73000000u + k.u.id);
      case Tag::Nil:  return 0;
    }
    return 1u;
  }

  static constexpr bool key_eq(const Value& a, const Value& b) {
    if (a.tag != b.tag) {
      if (a.tag==Tag::Int && b.tag==Tag::Num) return (double)a.i == b.n;
      if (a.tag==Tag::Num && b.tag==Tag::Int) return a.n == (double)b.i;
      return false;
    }
    switch (a.tag) {
      case Tag::Nil:  return true;
      case Tag::Bool: return a.b==b.b;
      case Tag::Int:  return a.i==b.i;
      case Tag::Num:  return a.n==b.n;
      case Tag::Str:  return a.s.id==b.s.id;
      case Tag::Table:return a.t.id==b.t.id;
      case Tag::Func: return a.f.id==b.f.id && a.f.is_native==b.f.is_native;
      case Tag::UData:return a.u.id==b.u.id;
    }
    return false;
  }

  constexpr Value rawget(TableId t, const Value& key) const {
    if (key.tag==Tag::Nil) return Value::nil();
    const TableObj& T = tables[t.id];
    const Entry* es = ent(t);
    std::uint32_t mask = T.cap - 1u;
    std::uint32_t h = hash_key(key);
    for (std::uint32_t probe=0; probe<T.cap; ++probe) {
      std::uint32_t idx=(h+probe)&mask;
      const Entry& e = es[idx];
      if (!e.used) { if (!e.tomb) return Value::nil(); }
      else if (key_eq(e.k,key)) return e.v;
    }
    return Value::nil();
  }

  constexpr void rawset(TableId t, const Value& key, const Value& val) {
    if (key.tag==Tag::Nil) throw "Lua: table index is nil";
    TableObj& T = tables[t.id];
    Entry* es = ent(t);
    std::uint32_t mask = T.cap - 1u;
    std::uint32_t h = hash_key(key);
    std::int32_t first_tomb=-1;

    for (std::uint32_t probe=0; probe<T.cap; ++probe) {
      std::uint32_t idx=(h+probe)&mask;
      Entry& e = es[idx];
      if (!e.used) {
        if (e.tomb) { if (first_tomb<0) first_tomb=(std::int32_t)idx; continue; }
        std::uint32_t put = first_tomb>=0 ? (std::uint32_t)first_tomb : idx;
        Entry& d = es[put];
        if (val.tag==Tag::Nil) { d.used=false; d.tomb=true; return; }
        d.k=key; d.v=val; d.used=true; d.tomb=false; ++T.size; return;
      }
      if (key_eq(e.k,key)) {
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
      for (std::uint16_t i=0;i<E.n;++i) if (E.b[i].name.id==name.id) return E.b[i].cell;
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

};

