// compile-fail: self-test fixture: a directive after the first line of code is an error
// expect-regex: static assertion failed
static_assert(sizeof(char) == 2, "fixture: reports TRL-E000");
// expect-id: TRL-E000
