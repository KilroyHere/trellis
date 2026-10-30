// compile-fail: self-test fixture: without expect-id or expect-regex, any error at all would pass
static_assert(sizeof(char) == 2, "fixture: no expectation");
