// compile-fail: self-test fixture: a misspelt directive is an error, not silently ignored
// expect-regexp: static assertion failed
static_assert(sizeof(char) == 2, "fixture: misspelt directive");
