// compile-fail: self-test fixture: on cl, expect-fail-msvc expects a miss, here of expect-regex, so the test passes as XFAIL; GCC and Clang ignore it
// expect-id: TRL-E000
// expect-regex: this text never appears in the compiler output
// expect-fail-msvc: fixture: a known cl limitation
static_assert(sizeof(char) == 2, "fixture: reports TRL-E000");
