### Recommended set (`min`): form by compiler

| Parameter | gcc13 | gcc14 | clang18 | clang20 | clang18-libcxx |
|---|---|---|---|---|---|
| `X` | `T()` → `X` | `T()` → `X` | `T()` → `X` | `T()` → `X` | `T()` → `X` |
| `const X` | `T()` → `X` | `T()` → `X` | `T()` → `X` | `T()` → `X` | `T()` → `X` |
| `X&` | `T&()` → `X&` | `T&()` → `X&` | `T&()` → `X&` | `T&()` → `X&` | `T&()` → `X&` |
| `const X&` | `T&()` → `const X&` | `T&()` → `const X&` | `T&()` → `const X&` | `T&()` → `const X&` | `T&()` → `const X&` |
| `X&&` | ✗ picks T&() for an rvalue ref | `T()` → `X` (differs) | `T()` → `X` (differs) | `T()` → `X` (differs) | `T()` → `X` (differs) |
| `const X&&` | ✗ picks T&() for an rvalue ref | `T()` → `X` (differs) | `T()` → `X` (differs) | `T()` → `X` (differs) | `T()` → `X` (differs) |
| `X*` | `T()` → `X*` | `T()` → `X*` | `T()` → `X*` | `T()` → `X*` | `T()` → `X*` |
| `const X*` | `T()` → `const X*` | `T()` → `const X*` | `T()` → `const X*` | `T()` → `const X*` | `T()` → `const X*` |
| `X* const` | `T()` → `X*` | `T()` → `X*` | `T()` → `X*` | `T()` → `X*` | `T()` → `X*` |
| `unique_ptr<X>` | `T()` → `unique_ptr<X>` | `T()` → `unique_ptr<X>` | `T()` → `unique_ptr<X>` | `T()` → `unique_ptr<X>` | `T()` → `unique_ptr<X>` |
| `unique_ptr<X>&&` | ✗ picks T&() for an rvalue ref | `T()` → `unique_ptr<X>` (differs) | `T()` → `unique_ptr<X>` (differs) | `T()` → `unique_ptr<X>` (differs) | `T()` → `unique_ptr<X>` (differs) |
| `shared_ptr<X>` | `T()` → `shared_ptr<X>` | `T()` → `shared_ptr<X>` | `T()` → `shared_ptr<X>` | `T()` → `shared_ptr<X>` | `T()` → `shared_ptr<X>` |
| `const shared_ptr<X>&` | `T&()` → `const shared_ptr<X>&` | `T&()` → `const shared_ptr<X>&` | `T&()` → `const shared_ptr<X>&` | `T&()` → `const shared_ptr<X>&` | `T&()` → `const shared_ptr<X>&` |
| `shared_ptr<const X>` | `T()` → `shared_ptr<const X>` | `T()` → `shared_ptr<const X>` | `T()` → `shared_ptr<const X>` | `T()` → `shared_ptr<const X>` | `T()` → `shared_ptr<const X>` |
| `weak_ptr<X>` | `T()` → `weak_ptr<X>` | `T()` → `weak_ptr<X>` | `T()` → `weak_ptr<X>` | `T()` → `weak_ptr<X>` | `T()` → `weak_ptr<X>` |
| `function<int()>` | `T()` → `function<int()>` | `T()` → `function<int()>` | `T()` → `function<int()>` | `T()` → `function<int()>` | `T()` → `function<int()>` |
| `function<unique_ptr<I>(int)>` | `T()` → `function<unique_ptr<I>(int)>` | `T()` → `function<unique_ptr<I>(int)>` | `T()` → `function<unique_ptr<I>(int)>` | `T()` → `function<unique_ptr<I>(int)>` | `T()` → `function<unique_ptr<I>(int)>` |
| `span<X>` | `T()` → `span<X>` | `T()` → `span<X>` | `T()` → `span<X>` | `T()` → `span<X>` | `T()` → `span<X>` |
| `span<const X>` | `T()` → `span<const X>` | `T()` → `span<const X>` | `T()` → `span<const X>` | `T()` → `span<const X>` | `T()` → `span<const X>` |
| `optional<X>` | ✗ ambiguous | ✗ ambiguous | ✗ ambiguous | ✗ ambiguous | ✗ ambiguous |
| `int` | `T()` → `int` | `T()` → `int` | `T()` → `int` | `T()` → `int` | `T()` → `int` |
| `const int&` | `T&()` → `const int&` | `T&()` → `const int&` | `T&()` → `const int&` | `T&()` → `const int&` | `T&()` → `const int&` |
| `string` | `T()` → `string` | `T()` → `string` | `T()` → `string` | `T()` → `string` | `T()` → `string` |
| `string_view` | `T()` → `string_view` | `T()` → `string_view` | `T()` → `string_view` | `T()` → `string_view` | `T()` → `string_view` |
| `vector<X>` | `T()` → `vector<X>` | `T()` → `vector<X>` | `T()` → `vector<X>` | `T()` → `vector<X>` | `T()` → `vector<X>` |
| `I&` | `T&()` → `I&` | `T&()` → `I&` | `T&()` → `I&` | `T&()` → `I&` | `T&()` → `I&` |
| `const I&` | `T&()` → `const I&` | `T&()` → `const I&` | `T&()` → `const I&` | `T&()` → `const I&` | `T&()` → `const I&` |
| `I*` | `T()` → `I*` | `T()` → `I*` | `T()` → `I*` | `T()` → `I*` | `T()` → `I*` |
| `unique_ptr<I>` | `T()` → `unique_ptr<I>` | `T()` → `unique_ptr<I>` | `T()` → `unique_ptr<I>` | `T()` → `unique_ptr<I>` | `T()` → `unique_ptr<I>` |
| `shared_ptr<I>` | `T()` → `shared_ptr<I>` | `T()` → `shared_ptr<I>` | `T()` → `shared_ptr<I>` | `T()` → `shared_ptr<I>` | `T()` → `shared_ptr<I>` |
| `reference_wrapper<X>` | ✗ ambiguous | `T()` → `reference_wrapper<X>` | `T()` → `reference_wrapper<X>` | `T()` → `reference_wrapper<X>` | `T()` → `reference_wrapper<X>` |
| `lazy<X&>` | `T()` → `lazy<X&>` | `T()` → `lazy<X&>` | `T()` → `lazy<X&>` | `T()` → `lazy<X&>` | `T()` → `lazy<X&>` |
| `tuple<X>` | ✗ ambiguous | ✗ ambiguous | ✗ ambiguous | ✗ ambiguous | ✗ ambiguous |
| `any` | ✗ ambiguous | ✗ ambiguous | ✗ ambiguous | ✗ ambiguous | ✗ ambiguous |
| `variant<X,int>` | `T()` → `variant<X,int>` | `T()` → `variant<X,int>` | `T()` → `variant<X,int>` | `T()` → `variant<X,int>` | `T()` → `variant<X,int>` |

### Every operator set: form by set (all five configurations agree unless a cell says otherwise)

| Parameter | `min` | `min3` | `min_rr` | `refq` | `boost` | `plan` | `rref_first` | `value` |
|---|---|---|---|---|---|---|---|---|
| `X` | `T()` → `X` | `T()` → `X` | `T()` → `X` | `T()` → `X` | `T()` → `X` | ✗ ambiguous | `T&&()` → `X&&` (differs) | `T()` → `X` |
| `const X` | `T()` → `X` | `T()` → `X` | `T()` → `X` | `T()` → `X` | `T()` → `X` | ✗ ambiguous | `T&&()` → `X&&` (differs) | `T()` → `X` |
| `X&` | `T&()` → `X&` | `T&()` → `X&` | `T&()` → `X&` | `T&()` → `X&` | `T&()` → `X&` | `T&()` → `X&` | `T&()` → `X&` | gcc13: ✗ cannot bind; gcc14: ✗ cannot bind; clang18: ✗ no viable conversion; clang20: ✗ no viable conversion; clang18-libcxx: ✗ no viable conversion |
| `const X&` | `T&()` → `const X&` | `const T&()` → `const X&` | `T&()` → `const X&` | `T&()` → `const X&` | `const T&()` → `const X&` | `T&()` → `const X&` | gcc13: `T&&()` → `const X&&` (differs); gcc14: `T&&()` → `const X&&` (differs); clang18: `T&()` → `const X&`; clang20: `T&()` → `const X&`; clang18-libcxx: `T&()` → `const X&` | `T()` → `X` (differs) |
| `X&&` | gcc13: ✗ picks T&() for an rvalue ref; gcc14: `T()` → `X` (differs); clang18: `T()` → `X` (differs); clang20: `T()` → `X` (differs); clang18-libcxx: `T()` → `X` (differs) | gcc13: ✗ picks T&() for an rvalue ref; gcc14: `T()` → `X` (differs); clang18: `T()` → `X` (differs); clang20: `T()` → `X` (differs); clang18-libcxx: `T()` → `X` (differs) | gcc13: `T&&()` → `X&&`; gcc14: `T&&()` → `X&&`; clang18: `T()` → `X` (differs); clang20: `T()` → `X` (differs); clang18-libcxx: `T()` → `X` (differs) | gcc13: ✗ picks T&() for an rvalue ref; gcc14: `T()` → `X` (differs); clang18: `T()` → `X` (differs); clang20: `T()` → `X` (differs); clang18-libcxx: `T()` → `X` (differs) | gcc13: `T&&()` → `X&&`; gcc14: `T&&()` → `X&&`; clang18: `T()` → `X` (differs); clang20: `T()` → `X` (differs); clang18-libcxx: `T()` → `X` (differs) | gcc13: ✗ ambiguous; gcc14: ✗ ambiguous; clang18: `T()` → `X` (differs); clang20: `T()` → `X` (differs); clang18-libcxx: `T()` → `X` (differs) | `T&&()` → `X&&` | `T()` → `X` (differs) |
| `const X&&` | gcc13: ✗ picks T&() for an rvalue ref; gcc14: `T()` → `X` (differs); clang18: `T()` → `X` (differs); clang20: `T()` → `X` (differs); clang18-libcxx: `T()` → `X` (differs) | gcc13: ✗ picks T&() for an rvalue ref; gcc14: `T()` → `X` (differs); clang18: `T()` → `X` (differs); clang20: `T()` → `X` (differs); clang18-libcxx: `T()` → `X` (differs) | gcc13: `T&&()` → `const X&&`; gcc14: `T&&()` → `const X&&`; clang18: `T()` → `X` (differs); clang20: `T()` → `X` (differs); clang18-libcxx: `T()` → `X` (differs) | gcc13: ✗ picks T&() for an rvalue ref; gcc14: `T()` → `X` (differs); clang18: `T()` → `X` (differs); clang20: `T()` → `X` (differs); clang18-libcxx: `T()` → `X` (differs) | gcc13: `T&&()` → `const X&&`; gcc14: `T&&()` → `const X&&`; clang18: `T()` → `X` (differs); clang20: `T()` → `X` (differs); clang18-libcxx: `T()` → `X` (differs) | gcc13: ✗ ambiguous; gcc14: ✗ ambiguous; clang18: `T()` → `X` (differs); clang20: `T()` → `X` (differs); clang18-libcxx: `T()` → `X` (differs) | `T&&()` → `const X&&` | `T()` → `X` (differs) |
| `X*` | `T()` → `X*` | `T()` → `X*` | `T()` → `X*` | `T()` → `X*` | `T()` → `X*` | ✗ ambiguous | `T&&()` → `X*&&` (differs) | `T()` → `X*` |
| `const X*` | `T()` → `const X*` | `T()` → `const X*` | `T()` → `const X*` | `T()` → `const X*` | `T()` → `const X*` | ✗ ambiguous | `T&&()` → `const X*&&` (differs) | `T()` → `const X*` |
| `X* const` | `T()` → `X*` | `T()` → `X*` | `T()` → `X*` | `T()` → `X*` | `T()` → `X*` | ✗ ambiguous | `T&&()` → `X*&&` (differs) | `T()` → `X*` |
| `unique_ptr<X>` | `T()` → `unique_ptr<X>` | `T()` → `unique_ptr<X>` | `T()` → `unique_ptr<X>` | `T()` → `unique_ptr<X>` | `T()` → `unique_ptr<X>` | gcc13: ✗ picks T&() for an rvalue ref; gcc14: ✗ ambiguous; clang18: ✗ ambiguous; clang20: ✗ ambiguous; clang18-libcxx: ✗ ambiguous | `T&&()` → `unique_ptr<X>&&` (differs) | `T()` → `unique_ptr<X>` |
| `unique_ptr<X>&&` | gcc13: ✗ picks T&() for an rvalue ref; gcc14: `T()` → `unique_ptr<X>` (differs); clang18: `T()` → `unique_ptr<X>` (differs); clang20: `T()` → `unique_ptr<X>` (differs); clang18-libcxx: `T()` → `unique_ptr<X>` (differs) | gcc13: ✗ picks T&() for an rvalue ref; gcc14: `T()` → `unique_ptr<X>` (differs); clang18: `T()` → `unique_ptr<X>` (differs); clang20: `T()` → `unique_ptr<X>` (differs); clang18-libcxx: `T()` → `unique_ptr<X>` (differs) | gcc13: `T&&()` → `unique_ptr<X>&&`; gcc14: `T&&()` → `unique_ptr<X>&&`; clang18: `T()` → `unique_ptr<X>` (differs); clang20: `T()` → `unique_ptr<X>` (differs); clang18-libcxx: `T()` → `unique_ptr<X>` (differs) | gcc13: ✗ picks T&() for an rvalue ref; gcc14: `T()` → `unique_ptr<X>` (differs); clang18: `T()` → `unique_ptr<X>` (differs); clang20: `T()` → `unique_ptr<X>` (differs); clang18-libcxx: `T()` → `unique_ptr<X>` (differs) | gcc13: `T&&()` → `unique_ptr<X>&&`; gcc14: `T&&()` → `unique_ptr<X>&&`; clang18: `T()` → `unique_ptr<X>` (differs); clang20: `T()` → `unique_ptr<X>` (differs); clang18-libcxx: `T()` → `unique_ptr<X>` (differs) | gcc13: ✗ picks T&() for an rvalue ref; gcc14: ✗ ambiguous; clang18: `T()` → `unique_ptr<X>` (differs); clang20: `T()` → `unique_ptr<X>` (differs); clang18-libcxx: `T()` → `unique_ptr<X>` (differs) | `T&&()` → `unique_ptr<X>&&` | `T()` → `unique_ptr<X>` (differs) |
| `shared_ptr<X>` | `T()` → `shared_ptr<X>` | `T()` → `shared_ptr<X>` | `T()` → `shared_ptr<X>` | `T()` → `shared_ptr<X>` | `T()` → `shared_ptr<X>` | ✗ ambiguous | `T&&()` → `shared_ptr<X>&&` (differs) | `T()` → `shared_ptr<X>` |
| `const shared_ptr<X>&` | `T&()` → `const shared_ptr<X>&` | `const T&()` → `const shared_ptr<X>&` | `T&()` → `const shared_ptr<X>&` | `T&()` → `const shared_ptr<X>&` | `const T&()` → `const shared_ptr<X>&` | `T&()` → `const shared_ptr<X>&` | gcc13: `T&&()` → `const shared_ptr<X>&&` (differs); gcc14: `T&&()` → `const shared_ptr<X>&&` (differs); clang18: `T&()` → `const shared_ptr<X>&`; clang20: `T&()` → `const shared_ptr<X>&`; clang18-libcxx: `T&()` → `const shared_ptr<X>&` | `T()` → `shared_ptr<X>` (differs) |
| `shared_ptr<const X>` | `T()` → `shared_ptr<const X>` | `T()` → `shared_ptr<const X>` | `T()` → `shared_ptr<const X>` | `T()` → `shared_ptr<const X>` | `T()` → `shared_ptr<const X>` | ✗ ambiguous | `T&&()` → `shared_ptr<const X>&&` (differs) | `T()` → `shared_ptr<const X>` |
| `weak_ptr<X>` | `T()` → `weak_ptr<X>` | `T()` → `weak_ptr<X>` | `T()` → `weak_ptr<X>` | `T()` → `weak_ptr<X>` | `T()` → `weak_ptr<X>` | ✗ ambiguous | `T&&()` → `weak_ptr<X>&&` (differs) | `T()` → `weak_ptr<X>` |
| `function<int()>` | `T()` → `function<int()>` | `T()` → `function<int()>` | `T()` → `function<int()>` | `T()` → `function<int()>` | `T()` → `function<int()>` | ✗ ambiguous | `T&&()` → `function<int()>&&` (differs) | `T()` → `function<int()>` |
| `function<unique_ptr<I>(int)>` | `T()` → `function<unique_ptr<I>(int)>` | `T()` → `function<unique_ptr<I>(int)>` | `T()` → `function<unique_ptr<I>(int)>` | `T()` → `function<unique_ptr<I>(int)>` | `T()` → `function<unique_ptr<I>(int)>` | ✗ ambiguous | `T&&()` → `function<unique_ptr<I>(int)>&&` (differs) | `T()` → `function<unique_ptr<I>(int)>` |
| `span<X>` | `T()` → `span<X>` | `T()` → `span<X>` | `T()` → `span<X>` | `T()` → `span<X>` | `T()` → `span<X>` | ✗ ambiguous | `T&&()` → `span<X>&&` (differs) | `T()` → `span<X>` |
| `span<const X>` | `T()` → `span<const X>` | `T()` → `span<const X>` | `T()` → `span<const X>` | `T()` → `span<const X>` | `T()` → `span<const X>` | ✗ ambiguous | `T&&()` → `span<const X>&&` (differs) | `T()` → `span<const X>` |
| `optional<X>` | ✗ ambiguous | ✗ ambiguous | gcc13: `T()` → `optional<X>`; gcc14: `T()` → `optional<X>`; clang18: ✗ ambiguous; clang20: ✗ ambiguous; clang18-libcxx: ✗ ambiguous | ✗ ambiguous | gcc13: `T()` → `optional<X>`; gcc14: `T()` → `optional<X>`; clang18: ✗ ambiguous; clang20: ✗ ambiguous; clang18-libcxx: ✗ ambiguous | ✗ ambiguous | gcc13: `T&&()` → `optional<X>&&` (differs); gcc14: `T&&()` → `optional<X>&&` (differs); clang18: ✗ ambiguous; clang20: ✗ ambiguous; clang18-libcxx: ✗ ambiguous | ✗ ambiguous |
| `int` | `T()` → `int` | `T()` → `int` | `T()` → `int` | `T()` → `int` | `T()` → `int` | ✗ ambiguous | `T&&()` → `int&&` (differs) | `T()` → `int` |
| `const int&` | `T&()` → `const int&` | `const T&()` → `const int&` | `T&()` → `const int&` | `T&()` → `const int&` | `const T&()` → `const int&` | `T&()` → `const int&` | gcc13: `T&&()` → `const int&&` (differs); gcc14: `T&&()` → `const int&&` (differs); clang18: `T&()` → `const int&`; clang20: `T&()` → `const int&`; clang18-libcxx: `T&()` → `const int&` | `T()` → `int` (differs) |
| `string` | `T()` → `string` | `T()` → `string` | `T()` → `string` | `T()` → `string` | `T()` → `string` | ✗ ambiguous | `T&&()` → `string&&` (differs) | `T()` → `string` |
| `string_view` | `T()` → `string_view` | `T()` → `string_view` | `T()` → `string_view` | `T()` → `string_view` | `T()` → `string_view` | ✗ ambiguous | `T&&()` → `string_view&&` (differs) | `T()` → `string_view` |
| `vector<X>` | `T()` → `vector<X>` | `T()` → `vector<X>` | `T()` → `vector<X>` | `T()` → `vector<X>` | `T()` → `vector<X>` | ✗ ambiguous | `T&&()` → `vector<X>&&` (differs) | `T()` → `vector<X>` |
| `I&` | `T&()` → `I&` | `T&()` → `I&` | `T&()` → `I&` | `T&()` → `I&` | `T&()` → `I&` | `T&()` → `I&` | `T&()` → `I&` | gcc13: ✗ error: cannot allocate an object of abstract type 'forms::I'; gcc14: ✗ error: cannot allocate an object of abstract type 'forms::I'; clang18: ✗ no viable conversion; clang20: ✗ no viable conversion; clang18-libcxx: ✗ no viable conversion |
| `const I&` | `T&()` → `const I&` | `const T&()` → `const I&` | `T&()` → `const I&` | `T&()` → `const I&` | `const T&()` → `const I&` | `T&()` → `const I&` | gcc13: `T&&()` → `const I&&` (differs); gcc14: `T&&()` → `const I&&` (differs); clang18: `T&()` → `const I&`; clang20: `T&()` → `const I&`; clang18-libcxx: `T&()` → `const I&` | gcc13: ✗ error: cannot allocate an object of abstract type 'forms::I'; gcc14: ✗ error: cannot allocate an object of abstract type 'forms::I'; clang18: ✗ error: allocating an object of abstract class type 'forms::I; clang20: ✗ error: allocating an object of abstract class type 'forms::I; clang18-libcxx: ✗ error: allocating an object of abstract class type 'forms::I |
| `I*` | `T()` → `I*` | `T()` → `I*` | `T()` → `I*` | `T()` → `I*` | `T()` → `I*` | ✗ ambiguous | `T&&()` → `I*&&` (differs) | `T()` → `I*` |
| `unique_ptr<I>` | `T()` → `unique_ptr<I>` | `T()` → `unique_ptr<I>` | `T()` → `unique_ptr<I>` | `T()` → `unique_ptr<I>` | `T()` → `unique_ptr<I>` | ✗ ambiguous | `T&&()` → `unique_ptr<I>&&` (differs) | `T()` → `unique_ptr<I>` |
| `shared_ptr<I>` | `T()` → `shared_ptr<I>` | `T()` → `shared_ptr<I>` | `T()` → `shared_ptr<I>` | `T()` → `shared_ptr<I>` | `T()` → `shared_ptr<I>` | ✗ ambiguous | `T&&()` → `shared_ptr<I>&&` (differs) | `T()` → `shared_ptr<I>` |
| `reference_wrapper<X>` | gcc13: ✗ ambiguous; gcc14: `T()` → `reference_wrapper<X>`; clang18: `T()` → `reference_wrapper<X>`; clang20: `T()` → `reference_wrapper<X>`; clang18-libcxx: `T()` → `reference_wrapper<X>` | gcc13: ✗ ambiguous; gcc14: `T()` → `reference_wrapper<X>`; clang18: `T()` → `reference_wrapper<X>`; clang20: `T()` → `reference_wrapper<X>`; clang18-libcxx: `T()` → `reference_wrapper<X>` | `T()` → `reference_wrapper<X>` | gcc13: ✗ ambiguous; gcc14: `T()` → `reference_wrapper<X>`; clang18: `T()` → `reference_wrapper<X>`; clang20: `T()` → `reference_wrapper<X>`; clang18-libcxx: `T()` → `reference_wrapper<X>` | `T()` → `reference_wrapper<X>` | gcc13: `T&()` → `X&` (differs); gcc14: ✗ ambiguous; clang18: ✗ ambiguous; clang20: ✗ ambiguous; clang18-libcxx: ✗ ambiguous | `T&&()` → `reference_wrapper<X>&&` (differs) | `T()` → `reference_wrapper<X>` |
| `lazy<X&>` | `T()` → `lazy<X&>` | `T()` → `lazy<X&>` | `T()` → `lazy<X&>` | `T()` → `lazy<X&>` | `T()` → `lazy<X&>` | ✗ ambiguous | `T&&()` → `lazy<X&>&&` (differs) | `T()` → `lazy<X&>` |
| `tuple<X>` | ✗ ambiguous | ✗ ambiguous | gcc13: `T()` → `tuple<X>`; gcc14: `T()` → `tuple<X>`; clang18: ✗ ambiguous; clang20: ✗ ambiguous; clang18-libcxx: ✗ ambiguous | ✗ ambiguous | gcc13: `T()` → `tuple<X>`; gcc14: `T()` → `tuple<X>`; clang18: ✗ ambiguous; clang20: ✗ ambiguous; clang18-libcxx: ✗ ambiguous | ✗ ambiguous | gcc13: `T&&()` → `tuple<X>&&` (differs); gcc14: `T&&()` → `tuple<X>&&` (differs); clang18: ✗ ambiguous; clang20: ✗ ambiguous; clang18-libcxx: ✗ ambiguous | ✗ ambiguous |
| `any` | ✗ ambiguous | ✗ ambiguous | ✗ ambiguous | ✗ ambiguous | ✗ ambiguous | `-` → `-` (differs) | ✗ ambiguous | ✗ ambiguous |
| `variant<X,int>` | `T()` → `variant<X,int>` | `T()` → `variant<X,int>` | `T()` → `variant<X,int>` | `T()` → `variant<X,int>` | `T()` → `variant<X,int>` | ✗ ambiguous | `T&&()` → `variant<X,int>&&` (differs) | `T()` → `variant<X,int>` |

### Arity probing cost (front end only, `-fsyntax-only`; median of the runs)

| Variant | gcc13 s (MB) | gcc14 s (MB) | clang18 s (MB) | clang20 s (MB) | clang18-libcxx s (MB) |
|---|---|---|---|---|---|
| baseline | 0.286 (82) | 0.303 (81) | 0.373 (114) | 0.370 (123) | 0.301 (109) |
| explicit | 0.274 (84) | 0.295 (83) | 0.396 (116) | 0.390 (125) | 0.322 (110) |
| search-flat-16 | 0.413 (120) | 0.427 (118) | 0.643 (150) | 0.633 (160) | 0.570 (145) |
| search-rec-16 | 0.428 (124) | 0.438 (122) | 0.617 (153) | 0.687 (162) | 0.596 (148) |
| search-fold-16 | 0.465 (130) | 0.473 (128) | 0.660 (158) | 0.701 (168) | 0.644 (153) |
| search-flat-8 | 0.349 (100) | 0.366 (99) | 0.496 (128) | 0.493 (137) | 0.435 (123) |
| discover-flat-16 | 0.450 (126) | 0.452 (124) | 0.665 (154) | 0.694 (164) | 0.607 (149) |
| baseline-1000 | 0.315 (96) | 0.316 (96) | 0.372 (116) | 0.403 (125) | 0.337 (111) |
| discover-flat-16-1000 | 2.546 (534) | 2.316 (522) | 3.409 (503) | 3.700 (526) | 3.635 (505) |

Cost per class above the baseline of the same size, in milliseconds:

| Variant | gcc13 | gcc14 | clang18 | clang20 | clang18-libcxx |
|---|---|---|---|---|---|
| explicit | -0.12 | -0.08 | 0.23 | 0.20 | 0.21 |
| search-flat-16 | 1.27 | 1.24 | 2.70 | 2.63 | 2.69 |
| search-rec-16 | 1.42 | 1.35 | 2.44 | 3.17 | 2.95 |
| search-fold-16 | 1.79 | 1.70 | 2.87 | 3.31 | 3.43 |
| search-flat-8 | 0.63 | 0.63 | 1.23 | 1.23 | 1.34 |
| discover-flat-16 | 1.64 | 1.49 | 2.92 | 3.24 | 3.06 |
| discover-flat-16-1000 | 2.23 | 2.00 | 3.04 | 3.30 | 3.30 |

### Limits (limits.cpp; identical on every configuration except where noted in the source)

| Case | Status | Arity | Arity - 1 also viable |
|---|---|---|---|
| plain, 0 parameters | ok | 0 | no |
| plain, 3 parameters | ok | 3 | no |
| copy + move + default constructor | ok | 0 | no |
| only a copy constructor | not_constructible (E002) | - | no |
| explicit constructor | ok | 1 | no |
| aggregate {X, const Y&, unique_ptr<X>} | ok | 3 | yes |
| aggregate with default member initializer | ok | 2 | yes |
| aggregate {int[2], X} | ok | 3 | yes |
| aggregate with a base class | ok | 2 | yes |
| template<class U> C(U&&) | greedy (E004) | 1 | no |
| greedy template + C(X, Y) | ok | 2 | yes |
| template<integral U> C(U) + C(X) | ok | 1 | no |
| C(int) + template<class T> C(T) = delete | not_constructible (E002) | - | no |
| template<class... U> C(U&&...) | greedy (E004) | 16 | no |
| C(...) | greedy (E004) | 16 | no |
| C(X, Y) + C(Y, X) | not_constructible (E002) | - | no |
| C() + C(X, Y) + C(Y, X) | ok | 0 | no |
| C(X, int = 3) | ok | 2 | yes |
| C(X) + C(X, Y) | ok | 2 | yes |
| private C(X) | not_constructible (E002) | - | no |
| C(X) = delete | not_constructible (E002) | - | no |
| private C(X, Y) + public C(X) | ok | 1 | no |
| abstract class | abstract (E001) | - | no |
| 17 parameters (max_ctor_arity 16) | over_limit (E010) | - | no |
| int | not_class | - | no |
| C(initializer_list<int>) + C(X, Y) | ok | 2 | yes |
| C(optional<X>) | not_constructible (E002) | - | no |
| C(X&&) | ok | 1 | no |
| C() + C(const X&) | ok | 1 | yes |

### Compile-fail cases (first error contains the ID; total lines of output)

| Case | gcc13 | gcc14 | clang18 | clang20 | clang18-libcxx |
|---|---|---|---|---|---|
| e001_abstract (TRL-E001) | ok, 7 lines | ok, 9 lines | ok, 11 lines | ok, 11 lines | ok, 11 lines |
| e002_no_fallback (TRL-E002) | ok, 7 lines | ok, 9 lines | ok, 8 lines | ok, 8 lines | ok, 8 lines |
| e002_private (TRL-E002) | ok, 7 lines | ok, 9 lines | ok, 11 lines | ok, 11 lines | ok, 11 lines |
| e004_greedy (TRL-E004) | ok, 7 lines | ok, 9 lines | ok, 11 lines | ok, 11 lines | ok, 11 lines |
| e005_ref_to_unique (TRL-E005) | ok, 7 lines | ok, 9 lines | ok, 8 lines | ok, 8 lines | ok, 8 lines |
| e005_singleton_by_value (TRL-E005) | ok, 7 lines | ok, 9 lines | ok, 8 lines | ok, 8 lines | ok, 8 lines |
| e010_over_limit (TRL-E010) | ok, 7 lines | ok, 9 lines | ok, 11 lines | ok, 11 lines | ok, 11 lines |
