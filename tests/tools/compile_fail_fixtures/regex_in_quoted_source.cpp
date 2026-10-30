// compile-fail: self-test fixture: the regex matches only quoted source, which doesn't count
// expect-regex: gadget
static_assert(sizeof(char) == 2 /* gadget */, "fixture: the word is only in the source");
