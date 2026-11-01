// compile-fail: self-test fixture: expect-max-lines-nfr07 needs a positive number
// expect-regex: static assertion failed
// expect-max-lines-nfr07: 0
static_assert(sizeof(char) == 2, "fixture: the budget is not a positive number");
