// compile-fail: self-test fixture: on cl, a test with expect-fail-msvc that meets every expectation fails as XPASS; GCC and Clang ignore it
// expect-id: TRL-E000
// expect-regex: fixture: reports TRL-E000
// expect-fail-msvc: fixture: a known cl limitation
static_assert(sizeof(char) == 2, "fixture: reports TRL-E000");
