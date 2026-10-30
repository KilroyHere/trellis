// compile-fail: self-test fixture: the first error line lacks the expected error ID
// expect-id: TRL-E999
static_assert(sizeof(char) == 2, "fixture: reports TRL-E000 only");
