// compile-fail: self-test fixture: TRL-E001 must not match inside the longer token TRL-E0012
// expect-id: TRL-E001
static_assert(sizeof(char) == 2, "fixture: reports TRL-E0012, a different ID");
