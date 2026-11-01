// compile-fail: self-test fixture: on cl, expect-max-lines-msvc replaces expect-max-lines
// expect-id: TRL-E000
// expect-regex: fixture: reports TRL-E000
// expect-max-lines: 1
// expect-max-lines-msvc: 60
static_assert(sizeof(char) == 2, "fixture: reports TRL-E000");
