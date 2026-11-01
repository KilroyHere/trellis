// compile-fail: self-test fixture: GCC and Clang ignore expect-max-lines-msvc, which cl checks instead
// expect-id: TRL-E000
// expect-regex: fixture: reports TRL-E000
// expect-max-lines-msvc: 1
static_assert(sizeof(char) == 2, "fixture: reports TRL-E000");
