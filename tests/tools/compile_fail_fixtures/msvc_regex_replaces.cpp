// compile-fail: self-test fixture: on cl, expect-regex-msvc replaces every expect-regex line
// expect-id: TRL-E000
// expect-regex: this text never appears in the compiler output
// expect-regex-msvc: fixture: reports TRL-E000
static_assert(sizeof(char) == 2, "fixture: reports TRL-E000");
