// compile-fail: self-test fixture: expect-fail-msvc without its reason is a bad directive, with every compiler
// expect-id: TRL-E000
// expect-fail-msvc:
static_assert(sizeof(char) == 2, "fixture: reports TRL-E000");
