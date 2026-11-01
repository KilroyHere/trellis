// compile-fail: self-test fixture: within expect-max-lines-nfr07, which the verdict reports beside the line count
// expect-id: TRL-E000
// expect-regex: fixture: reports TRL-E000
// expect-max-lines-nfr07: 20
static_assert(sizeof(char) == 2, "fixture: reports TRL-E000");
