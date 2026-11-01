// compile-fail: self-test fixture: GCC and Clang ignore expect-regex-msvc, which cl checks instead
// expect-id: TRL-E000
// expect-regex: fixture: reports TRL-E000
// expect-regex-msvc: this text never appears in the compiler output
static_assert(sizeof(char) == 2, "fixture: reports TRL-E000");
