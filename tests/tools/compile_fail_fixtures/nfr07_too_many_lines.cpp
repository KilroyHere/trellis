// compile-fail: self-test fixture: two errors are more than expect-max-lines-nfr07 allows, even without the quoted source
// expect-regex: static assertion failed
// expect-max-lines-nfr07: 1
static_assert(sizeof(char) == 2, "fixture: the first error");
static_assert(sizeof(char) == 3, "fixture: the second error");
