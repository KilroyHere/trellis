# Internals

Notes for maintainers on how Trellis works inside, and why, written for the second maintainer ([plan §11](../plan.md#11-documentation-plan)). The decisions behind the design are in [`docs/adr/`](../adr/README.md), and the errors users can see are in [`docs/errors/`](../errors/README.md). This file indexes the notes and sets the doc-comment convention for public headers.

## Notes

| Note | What it covers | Tests |
|---|---|---|
| [meta-toolkit.md](meta-toolkit.md) | `include/trellis/meta/`: flat pack expansions instead of folds, the per-list indexed set behind `contains_v`, `index_of_v` and `at_t`, the inheritance-based type map and its deduction-free lookups, `type_name` from function signatures, `fixed_string`, and the `result<T>` fallback | `tests/unit/{type_list,type_map,type_name,fixed_string,result,error_namespace,no_exceptions}_test.cpp`, `tests/compile_fail/meta/` |

Plan §11 also asks for notes on the deduction tricks of spike S1 (T1.3), on memoization ([ADR-0014](../adr/0014-resolution-memoization-and-cycles.md)), on the storage layout (T1.5) and on the explain pass (T1.6).

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

This illustration follows the API sketch in plan §6.1 and §6.9; the real comment comes with `make_injector` in T1.4.

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
