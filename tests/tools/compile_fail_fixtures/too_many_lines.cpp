// compile-fail: self-test fixture: the error output is longer than expect-max-lines allows
// expect-regex: static assertion failed
// expect-max-lines: 1
static_assert(sizeof(char) == 2, "fixture: more than one line of output");
