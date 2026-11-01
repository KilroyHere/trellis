# Error catalog

Every mistake Trellis reports has a stable ID: `TRL-E0xx` for errors found at compile time and `TRL-R0xx` for errors found at run time ([plan §7.8](../plan.md#78-diagnostics-engine)). The first error line of a Trellis compile error names its ID (NFR-07), and so does the message of a runtime error, so the ID leads here: the page for `TRL-E001` is `docs/errors/TRL-E001.md`. An ID never changes meaning ([ADR-0006](../adr/0006-diagnostics-contract.md)).

Each page gives the cause, a minimal example, the fix and the related errors ([plan §11](../plan.md#11-documentation-plan)). A page is written when the library first emits its ID, starting from [the template](_template.md). Until then, the ID's link below leads nowhere.

## Compile-time errors

| ID | Meaning | Status |
|---|---|---|
| [TRL-E001](TRL-E001.md) | No binding for an abstract type | Emitted by T1.5, by `create<T>()`; with the path from the root since T1.6 |
| [TRL-E002](TRL-E002.md) | Type not constructible: no viable constructor for the available bindings | Emitted by T1.2, for an explicit constructor declaration that matches no constructor, by T1.3, for a type whose constructor deduction can't find, and by T1.5, for a by-value request whose copy can't be made; all of them through `create<T>()` since T1.5, with the path from the root since T1.6. A class whose constructor exists but whose parameters can't be served is reported as its first failing parameter's own error (T1.6, plan §7.8) |
| [TRL-E003](TRL-E003.md) | Duplicate binding for a key | Emitted by T1.4; by T1.7 also for two overrides of one key |
| [TRL-E004](TRL-E004.md) | Ambiguous constructor | Emitted by T1.3; through `create<T>()` since T1.5, with the path from the root since T1.6 |
| [TRL-E005](TRL-E005.md) | Requested form incompatible with the lifetime | Emitted by T1.4; by `create<T>()` since T1.5, with the path from the root since T1.6 |
| [TRL-E006](TRL-E006.md) | Captive dependency | Not yet emitted; planned in Phase 2 |
| [TRL-E007](TRL-E007.md) | Dependency cycle. In deduced mode only with `options{.cycle_check = true}`; otherwise the compiler's own errors, which the page will document ([ADR-0014](../adr/0014-resolution-memoization-and-cycles.md)) | Not yet emitted; planned in Phase 2 |
| [TRL-E008](TRL-E008.md) | Factory signature does not match the constructor | Not yet emitted; planned in Phase 3 |
| [TRL-E009](TRL-E009.md) | Override without an existing binding | Emitted by T1.6 (entry point) and T1.7, by `make_injector` for `override<K>` |
| [TRL-E010](TRL-E010.md) | Constructor arity above `max_ctor_arity` | Emitted by T1.3; through `create<T>()` since T1.5, with the path from the root since T1.6 |
| [TRL-E011](TRL-E011.md) | Scoped binding requested outside its scope | Not yet emitted; planned in Phase 2 |
| [TRL-E012](TRL-E012.md) | Unbound concrete type requested by reference, pointer or `shared_ptr` | Emitted by T1.4; by `create<T>()` since T1.5, with the path from the root since T1.6 |
| [TRL-E013](TRL-E013.md) | Unbound concrete type requested in strict mode (provisional ID) | Emitted by T1.5, by `create<T>()` under `options{.strict = true}`; with the path from the root since T1.6 |
| [TRL-E014](TRL-E014.md) | Owning request would delete the implementation through a base class whose destructor isn't virtual (provisional ID) | Emitted by T1.5, by `create<T>()`, for `std::unique_ptr<I>`; with the path from the root since T1.6 |

## Runtime errors

They follow [ADR-0009](../adr/0009-runtime-error-model.md): `get<T>()` throws a `trellis::resolution_error`, or calls the error handler when exceptions are off, and `try_get<T>()` returns the error in a `trellis::result<T>`.

| ID | Meaning | Status |
|---|---|---|
| [TRL-R001](TRL-R001.md) | Missing registration | Not yet emitted; planned in Phase 5 |
| [TRL-R002](TRL-R002.md) | Dependency cycle | Not yet emitted; planned in Phase 5 |
| [TRL-R003](TRL-R003.md) | Captive dependency | Not yet emitted; planned in Phase 5 |
| [TRL-R004](TRL-R004.md) | Re-entrant construction: a constructor dereferences a `lazy` that leads back to the object under construction. The static engine reports it too, in every build ([ADR-0012](../adr/0012-threading-model.md)) | Not yet emitted; planned in Phase 2, with `lazy` and `provider`, and in Phase 5 |
| [TRL-R005](TRL-R005.md) | Type-key mismatch across shared libraries | Not yet emitted; planned in Phase 5 |

## Reserved

| ID | Meaning | Status |
|---|---|---|
| [TRL-E000](TRL-E000.md) | Placeholder for the tooling's own tests | Test-only |

Some tests also contain ID-like strings as inputs, not diagnostics, so they have no pages of their own:

- the fixtures of the runner self-test in `tests/tools/compile_fail_fixtures/`: `TRL-E001`, `TRL-E999` and the malformed `TRL-E0012`. They test how `tools/compile_fail.py` matches IDs;
- `tests/unit/fixed_string_test.cpp`, which builds the text `trellis [TRL-E001]` to test compile-time message assembly.

## Status values

- **Emitted by T*x.y*:** the library reports the ID since that task, and its page is complete.
- **Not yet emitted:** nothing reports the ID yet. "Planned in" names the tasks whose acceptance criteria name it, in `docs/tasks.json`, or the phase of [plan §12.2](../plan.md#122-phases) that brings it, for phases not yet broken into tasks.
- **Test-only:** reserved for the tooling's own tests. The library never emits it.

## Adding an error

1. Use the ID this catalog gives the mistake. A mistake it doesn't cover gets the next free number in its series: E for compile time, R for run time. IDs are never renumbered or given a second meaning.
2. Write the ID out in full where it is raised: in the `static_assert` message, or in the runtime error's message. List it under `@par Errors` in the doc comment of every public entity that can raise it ([doc-comment convention](../internals/README.md#doc-comments-in-public-headers)).
3. Copy [`_template.md`](_template.md) to `<ID>.md` and fill it in. The minimal example is the ID's compile-fail test, or the test of a runtime error.
4. In the table above, change the ID's status to "Emitted by T*x.y*", or add a row for a new ID.
5. Add a line under "Unreleased" in [`CHANGELOG.md`](../../CHANGELOG.md).

The check `tools/error_catalog.py` runs in every build as the CTest test `tools.error_catalog`, so `scripts/check-fast.sh` and CI run it. It fails when an ID used anywhere in `include/` has no page here, when `include/` uses `TRL-E000` or a malformed ID such as `TRL-E0012`, and when a page isn't linked from this index. It finds IDs by their text, so an ID assembled from pieces escapes it: write each one out in full.
