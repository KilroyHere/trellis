// compile-fail: self-test fixture: compilation fails and every expectation holds, so it passes
// expect-id: TRL-E000
// expect-regex: static assertion failed
// expect-regex: fixture: reports TRL-E000
// expect-max-lines: 20
static_assert(sizeof(char) == 2, "fixture: reports TRL-E000");
