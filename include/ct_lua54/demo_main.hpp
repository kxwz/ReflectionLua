#pragma once

// All example checks are in include/ct_lua54/examples/*.hpp.
// Keep aggregate asserts here so a missing include/order issue fails clearly.
static_assert(ct_lua54::examples::r_core == 1.0, "core aggregate check failed");
static_assert(ct_lua54::examples::r_expr == 1.0, "expr aggregate check failed");
static_assert(ct_lua54::examples::r_flow == 1.0, "flow aggregate check failed");
static_assert(ct_lua54::examples::r_meta == 1.0, "meta aggregate check failed");
static_assert(ct_lua54::examples::r_table == 1.0, "table aggregate check failed");
static_assert(ct_lua54::examples::r_math == 1.0, "math aggregate check failed");
static_assert(ct_lua54::examples::r_string == 1.0, "string aggregate check failed");
static_assert(ct_lua54::examples::r_all == 1.0, "all aggregate check failed");

int main() { return 0; }

