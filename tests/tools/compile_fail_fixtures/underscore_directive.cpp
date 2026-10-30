// compile-fail: self-test fixture: a directive name spelt with an underscore is an error
// expect_regex: static assertion failed
static_assert(sizeof(char) == 2, "fixture: misspelt directive");
