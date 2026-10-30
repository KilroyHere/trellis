// compile-fail: self-test fixture: it fails to compile, but not with the expected message
// expect-regex: this text never appears in the compiler output
static_assert(sizeof(char) == 2, "fixture: regex mismatch");
