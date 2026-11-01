# Internals

Notes for maintainers on how Trellis works inside, and why, written for the second maintainer ([plan §11](../plan.md#11-documentation-plan)). The decisions behind the design are in [`docs/adr/`](../adr/README.md), and the errors users can see are in [`docs/errors/`](../errors/README.md). This file indexes the notes and sets the doc-comment convention for public headers.

## Notes

| Note | What it covers | Tests |
|---|---|---|
| [meta-toolkit.md](meta-toolkit.md) | `include/trellis/meta/`: flat pack expansions instead of folds, the per-list indexed set behind `contains_v`, `index_of_v` and `at_t`, the inheritance-based type map and its deduction-free lookups, `type_name` from function signatures, `fixed_string`, and the `result<T>` fallback | `tests/unit/{type_list,type_map,type_name,fixed_string,result,error_namespace,no_exceptions}_test.cpp`, `tests/compile_fail/meta/` |
| [constructor-discovery.md](constructor-discovery.md) | The explicit strategy: `ctor<Args...>`, `ctor_traits<T>` and the member alias, why a specialization wins over the alias, the binding's hook, the check that a declaration can't lie (TRL-E002), and the "nothing declared" state that deduction takes over | `tests/unit/ctor_traits_test.cpp`, `tests/compile_fail/discovery/` |
| [deduction.md](deduction.md) | Deduced discovery: spike S1's placeholders and why their operator set deduces each form, discovery before resolution, the flat arity search with a bound, the limits and the aggregate checks (TRL-E002, TRL-E004, TRL-E010), `discovered_ctor_t`, the entry point the tests use (the engine asks `discoverable_v`), the two `config.hpp` workarounds, and S1's form table with what Trellis deduces | `tests/unit/deduced_test.cpp`, `tests/compile_fail/discovery/deduced_*.cpp` |
| [bindings-and-modules.md](bindings-and-modules.md) | One binding type per chain step; a constructor declared on the binding, carried in the implementation's place (T1.15); flattening modules once, linearly, through indexed bases; the binding map and duplicate keys; request forms, the plan §6.3 table and its checks (T1.4) | `tests/unit/{bind,module,injector,request_forms,with_ctor,with_ctor_engine}_test.cpp`, `tests/compile_fail/{dsl,static}/` |
| [resolution-and-storage.md](resolution-and-storage.md) | `create<T>()` (T1.5): the memoized check, a plan and a node per request, and its depth per hop; the creation code's three function bodies per hop and the build function a slot calls; the chain test and the deepest chains (T1.11); shared storage in one allocation; spike S4's slots; the teardown stack and the slot store; eager mode | `tests/unit/{create,threading,scenario,no_exceptions_create}_test.cpp`, `tests/compile_fail/create/`, `tests/chain_depth/` |
| [explain-pass.md](explain-pass.md) | The explain pass (T1.6): one function that holds every TRL `static_assert` and why a report is unique to the request, not the path; the steps through declared and deduced classes, the first failing parameter of a deduced class, one note per deduced class from one line that the compilers fold, the lookahead, the path in `explain_cause`'s note, the stop at a dependency cycle, eager mode's check in `make_injector`; output sizes, and the chains that still reach the backtrace limit | `tests/unit/explain_test.cpp`, `tests/compile_fail/explain/` and its goldens, `tests/compile_fail/create/` |
| [overrides-and-testing.md](overrides-and-testing.md) | `trellis::override<K>` (T1.7): the wrapper that reuses `bind<K>`'s chain; how `make_injector` applies overrides, flattening once and replacing each overridden binding in place with one map lookup; TRL-E003 and TRL-E009 for overrides that don't apply, and why the injector then resolves nothing more; what an override costs beside a large module; `testing::wireable` as `make_injector`'s and `create<T>()`'s own checks; `testing::create_all` through `create<Key&>()` | `tests/unit/{override,testing}_test.cpp`, `tests/compile_fail/{override,testing}/` |
| [delivery.md](delivery.md) | The C++20 modules `trellis` and `trellis.testing`: the global module fragment, exports in the declaring namespace, what importers lose (macros, the standard library), configuration macros, build requirements, why GCC 14 and ccache are out; the export list check from Clang's syntax tree, per module; the single header: generation, its refusals, `--check`, and the unit tests run against it and through the modules (T1.8) | `tests/module/`, `tests/single_header/`, `tests/tools/{module_exports,amalgamate}_selftest.py` |

Plan §11's note on the explain pass with paths is [explain-pass.md](explain-pass.md); [resolution-and-storage.md](resolution-and-storage.md) covers memoization ([ADR-0014](../adr/0014-resolution-memoization-and-cycles.md)) and the storage layout.

### Writing a note

Every metaprogramming technique the code relies on gets a note (CLAUDE.md, and the simplicity budget of [ADR-0013](../adr/0013-governance-and-releases.md)). Add a note and a row to the table above, or extend the note on the same part of the library, and update it in the same commit as any change to the technique. For each technique, a note says:

- **What it does:** the problem and the mechanism, with a short excerpt of the code.
- **Why:** what it costs and what the alternatives would cost, with links to the ADR, spike or benchmark behind the choice.
- **Where:** the headers and entities that use it.
- **Tests:** the test files and test names that cover it, so that whoever changes the technique knows what to run.

Name a note after its subject, in lower case with hyphens, such as `meta-toolkit.md`.

## Doc comments in public headers

Every public entity in `include/` has a doc comment in the format below. That means every class, function, alias, variable, concept and enumeration a user may name, templates included, and the public members of public classes: everything outside `trellis::detail`, including the compile-error vocabulary in `trellis::error` ([ADR-0006](../adr/0006-diagnostics-contract.md)). Macros aren't entities: `config.hpp`, the only header that defines them, explains each one in a plain comment.

The reference documentation will be generated from these comments and checked for completeness in CI (plan §11), so the format uses `///` lines and Doxygen commands, which Doxygen, MrDocs and clangd all read.

### Format

```cpp
/// @brief <Purpose: what the entity is for, in one sentence.>
///
/// <Optional: behaviour, guarantees and cost, in a few plain sentences.>
///
/// @tparam <Name> <What the template parameter is.>
/// @param <name> <What the function parameter is.>
/// @return <What the caller gets.>
///
/// @par Constraints
/// - <A requirement on the arguments or the program, and what happens when it isn't met.>
///
/// @par Errors
/// - `TRL-E0xx`: <when this entity raises it.>
///
/// @par Example
/// @code
/// <A few lines that use the entity the way a user would.>
/// @endcode
```

### Rules

- **Placement:** directly above the declaration, and above its `template <...>` line, with nothing in between. Each overload gets its own comment.
- **Required parts:** `@brief`, `@par Constraints`, `@par Errors`, and `@par Example` with its `@code` block, in this order. An entity with no constraints, or one that raises no error, says `None.` on the line after the heading, so that a missing part is always a mistake.
- **Optional parts:** the paragraph after `@brief`, and `@tparam`, `@param` and `@return` where a name doesn't explain itself. `@see` points to related entities or catalog pages.
- **Constraints** cover the template arguments, the function arguments and the state of the injector or scope. Each says what happens when it isn't met: a compile error, with the ID that the entity also lists under Errors; a runtime error, with its ID; or undefined behaviour, said plainly.
- **Errors** list every `TRL-` ID a user can get from the entity, each written out in full with its condition. Every ID listed needs a page in [`docs/errors/`](../errors/README.md). The catalog check reads comments too, so it fails on an ID that has no page.
- **Example:** about ten lines at most, using public names only, and correct as written, given the declarations it implies. Plan §9.3 asks for documentation snippets that compile in CI; once that check exists, it compiles these.
- **Style:** wrap at 100 columns, as in the code. Backticks and `-` lists are fine; text inside `@code` stays verbatim.
- **Internal entities,** those in `trellis::detail`, get a plain `//` comment instead. It says what the entity does and why, and for a metaprogramming trick, which test covers it (CLAUDE.md). A note in this directory explains the technique. Keeping `///` for public entities keeps `detail` out of the reference.

### Example

This illustration follows the API sketch in plan §6.1 and §6.9; the real comment, from T1.4, is in `include/trellis/static/injector.hpp`.

```cpp
/// @brief Builds a static injector from bindings and modules.
///
/// The arguments are flattened once into one binding map (ADR-0010). The injector owns every
/// object it creates with an owning lifetime, and destroys them in reverse order of construction.
///
/// @tparam Opts The injector's options (plan §6.13), such as `options{.eager = true}`.
/// @param parts Bindings, such as `bind<IClock>.to<SystemClock>()`, and modules made with
///              `make_module`, in any order.
/// @return The injector, by value.
///
/// @par Constraints
/// - Each key, a type with its qualifier, is bound at most once across `parts`; otherwise
///   compilation fails with TRL-E003.
///
/// @par Errors
/// - `TRL-E003`: two of `parts` bind the same key.
///
/// @par Example
/// @code
/// auto injector = trellis::make_injector(storage_module(config),
///                                        trellis::bind<IClock>.to<SystemClock>());
/// App app = injector.create<App>();
/// @endcode
template <options Opts = options{}, class... Parts>
auto make_injector(Parts&&... parts);
```

An entity without constraints or errors says so:

```cpp
/// @par Constraints
/// None.
///
/// @par Errors
/// None.
```
