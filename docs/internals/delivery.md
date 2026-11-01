# Delivery: the C++20 modules and the single header

How Trellis reaches users besides `include/trellis/`: the C++20 modules `trellis` and `trellis.testing`, and the generated single header (T1.8). Both forms are made from the headers, which stay the only source. The single header runs every unit test; the modules run their smoke tests and the unit tests that use only public names. Files: `modules/trellis.cppm`, `modules/trellis.testing.cppm` and `modules/CMakeLists.txt`, `tools/module_exports.py`, `tools/amalgamate.py`, `tests/module/` and `tests/single_header/`. Decisions: [ADR-0008](../adr/0008-header-only-delivery.md) (several delivery forms, each tested from Phase 1), plan §10 and risk R15 (a module build that breaks unnoticed).

## The module

**What.** `modules/trellis.cppm` includes the umbrella header in its global module fragment, declares `export module trellis;`, and exports each public entity with a using-declaration in the namespace that declares it:

```cpp
module;
#include <trellis/trellis.hpp>
export module trellis;

export namespace trellis::inline v1 {
using trellis::v1::bind;
using trellis::v1::make_injector;
// ... every public name of trellis::v1
}  // namespace trellis::inline v1

export namespace trellis::error {
using trellis::error::E002;
// ... the compile-error vocabulary
}  // namespace trellis::error
```

`modules/trellis.testing.cppm` is the module `trellis.testing`: its global module fragment includes `<trellis/testing/wiring.hpp>`, it re-exports `trellis` (`export import trellis;`), and it exports the testing helpers, `trellis::testing::wireable` and `trellis::testing::create_all`.

CMake builds both as the static library `trellis::module` when `TRELLIS_ENABLE_MODULE` is on; the Clang presets turn it on. A program links `trellis::module` and writes `import trellis;`; a test writes `import trellis.testing;`.

**Why this form.**

- **The global module fragment keeps the headers the library.** Declarations included there belong to no module, so they keep their names and their mangling. A program can import the module in some files and include the headers in others, and the two meet as the same entities: `tests/module/headers_test.cpp` defines functions with the headers' types that `import_test.cpp` calls with the module's, and the program links. Writing the library as module units instead would mean two sources. `export extern "C++" { #include ... }`, the other common trick, would export `trellis::detail` as well, and needs every standard header included before it.
- **Exports in the declaring namespace.** The using-declarations sit in `trellis::inline v1`, beside the entities, not in `trellis`. GCC 14 doesn't make `using trellis::bind;` in namespace `trellis` visible to importers when `bind` lives in the inline namespace `v1`; in `v1` it does, and Clang accepts both. This placement also keeps `trellis::v1::bind` valid for importers, as for the headers.
- **The error vocabulary is exported.** `trellis::error` holds the types that diagnostics name (ADR-0006). Users rarely write them, but they are documented public entities, and a test may assert on them, so the module exports them like any other public name.
- **Nothing in `detail` is exported.** Exporting makes a name visible to the importer; templates instantiated in the importer still reach `detail` through their definitions in the module, which is all they need.
- **The testing helpers are a module of their own.** The umbrella header leaves out `<trellis/testing/wiring.hpp>`, so that only tests, which include it themselves, see the testing helpers (T1.7). Exporting them from `trellis` would make `import trellis;` give more than `#include <trellis/trellis.hpp>`, so they get the module `trellis.testing` instead. It re-exports `trellis`, as the header includes the injector, so a test needs only `import trellis.testing;`. Both modules come from one target: a second target would mean a second set of build settings to keep equal.

**What an importer gets, and what it doesn't.**

- **Names:** every public entity, as `trellis::X` or `trellis::v1::X`. `injector.create<T>()` works through the import, with declared constructors (a specialization of `trellis::ctor_traits` written in the importer, the member alias `trellis_ctor`) and deduced ones, as do `trellis::override<K>`, deduction guides (`trellis::fixed_string{"text"}`), class-type template arguments, `trellis::type_name<T>()` and modules derived from `make_module`'s result (`tests/module/import_test.cpp`, `testing_test.cpp`).
- **No standard library.** The global module fragment includes standard headers, but the module exports none of their names, so an importer includes the standard headers it uses itself: `<string_view>` to compare the `std::string_view` that `type_name<T>()` returns (`type_name<T>() == "car"` is "invalid operands" without it), `<memory>` to name `std::shared_ptr`. Argument-dependent lookup doesn't help, because the standard library's declarations in the fragment belong to no module and aren't exported.
- **No macros.** Macros never cross a module boundary, so `import trellis;` defines none of `config.hpp`'s: not `TRELLIS_VERSION` and its parts, `TRELLIS_CPLUSPLUS`, the `TRELLIS_HAS_*` feature macros, nor the internal ones (`TRELLIS_IS_SAME`, `TRELLIS_FUNCTION_SIGNATURE` and the compiler workarounds). An importer that needs them includes `<trellis/config.hpp>`, which defines macros and declares nothing, so it is safe beside the import; include it first (`tests/module/config_test.cpp`). The values match the module's own build when the importer uses the same compiler, language mode and standard library, which reading a compiled module requires anyway.
- **Configuration happens in the module's build.** A macro that changes what Trellis does has to be defined where the module is compiled, not before `import trellis;`, where the module never sees it. Today that is `TRELLIS_HAS_ATOMIC_WAIT`, which a build may set to 0 to make singletons wait on a mutex and a condition variable instead of `std::atomic::wait` (ADR-0012): for the module, `target_compile_definitions(trellis_module PUBLIC TRELLIS_HAS_ATOMIC_WAIT=0)`. PUBLIC passes the definition on to the targets that link `trellis::module` too, as config.hpp requires: every translation unit of a program must agree on it, the module's and those that include headers. A test program that defines the macro for itself, as `trellis_wait_fallback_tests` does, therefore runs only against the headers and the single header.
- **The same language options.** An importer must be compiled with the module's language mode and extensions setting; Clang refuses a compiled module interface built otherwise. Set `CMAKE_CXX_STANDARD` and `CMAKE_CXX_EXTENSIONS` before adding Trellis. The tests set them on `trellis_module` in `tests/module/CMakeLists.txt`, with the project warning set.
- **The same diagnostics.** A wiring mistake is still one `static_assert` with its ID on the first error line; the only change is the context line before it, "In module 'trellis' imported from ..." instead of "In file included from ...".
- **One form per translation unit.** Different files of one program may use different forms (tested). Within one file, `config.hpp` before the import is fine, as it declares nothing; the whole headers and the import together rely on the compiler merging their declarations, which isn't tested here.
- **Includes before the import.** A standard header included *after* `import trellis;` has to be merged with the copy that the module's global module fragment already holds, and Clang gets that wrong in some combinations. In T1.8, Clang 20 with libstdc++ 14 rejected `<utility>` ("'lifetimebound' attribute cannot be applied to a parameter of a function that returns void"), and Clang 18 with libc++ 18 rejected `<optional>` ("redefinition of concept '__is_derived_from_optional'"). Clang 18 with libstdc++ accepted both. Every `#include` therefore goes first, then the import, as in `tests/module/`.

**Build requirements**, checked at configure time by `modules/CMakeLists.txt`, which names what is missing:

| Need | Why |
|---|---|
| CMake 3.28 or later | The first version with C++20 named modules outside experimental gates. The headers still need only 3.21. |
| Ninja 1.11 or later, or Visual Studio 2022 | The generators that build modules. |
| Clang 16 or later with its `clang-scan-deps` | CMake scans each source for imports with it; Debian and Ubuntu ship it in `clang-tools-<major>`. Tested: Clang 18 and 20, with libstdc++ and with libc++ (the `clang-18-libcxx-debug` preset), and under ASan with UBSan, TSan and in release builds. |
| Not GCC 14 | See below. GCC 15 or later is accepted untested, as is MSVC 19.34 (Visual Studio 17.4) or later; CI builds neither yet. Any compiler other than Clang 18 and 20 gets a configure-time warning that says to check it with the module's tests, whose `static_assert`s catch GCC 14's faults. |

**GCC 14 isn't sufficient.** With CMake 3.28, GCC 14.2 builds the module, but its importers can't use it (T1.8, measured here):

- `trellis::type_name<int>()` returns `"ith T = int"` in an importer. `type_name` cuts a type's name out of `__PRETTY_FUNCTION__` at offsets computed once, inside the module; GCC spells the same function `signature_of@trellis` in the importer and `signature_of` in the module, 8 characters longer, so the cut lands 8 characters early. Every name in a diagnostic or a failure message would be wrong.
- Class template argument deduction ignores deduction guides from the global module fragment: `trellis::fixed_string text{"abc"}` fails, as does `template <trellis::fixed_string Name>` with a string argument.
- Exports in namespace `trellis` of entities in `v1` are invisible (worked around, above).

No upstream report was checked, as gcc.gnu.org can't be reached from here. `TRELLIS_ENABLE_MODULE` therefore fails at configure time with GCC before 15 and says why.

**No ccache for modules.** `trellis_module` and its importers have no compiler launcher. ccache 4.9 doesn't hash the compiled module interface that an importer reads, so after a header changes it returns the importer's old object file: measured with a module that exports an inline function, whose importer kept printing the old value until the launcher was removed. It never caches the interface unit itself ("unsupported source language"), so nothing is lost.

**Scanning.** CMake scans sources for imports by default only when policy CMP0155 is new, that is, when the project requires CMake 3.28. Trellis requires 3.21, so `trellis_module_importer(<target>)`, defined in `modules/CMakeLists.txt`, links `trellis::module`, turns `CXX_SCAN_FOR_MODULES` on and removes the launcher for a target of the project. Consumers that require CMake 3.28 get the scan by default.

**Not installed yet.** The module is for builds that add Trellis as a subdirectory or with `FetchContent`; `find_package(trellis)` provides the headers only. Installing it means installing the interface unit with CMake's module export support, a follow-up.

**Tests:** `ctest -L module`:

- `tests/module/import_test.cpp` (the public API through the import alone, `create<T>()` and an override included, and the absence of macros), `config_test.cpp` (`config.hpp` beside the import), `headers_test.cpp` (the headers and the module in one program) and `testing_test.cpp` (`import trellis.testing;`);
- `module.<program>.*`: the unit tests that use only public names, compiled again as importers. For each unit test program of `tests/unit/` that the compiled modules can serve, every source that never names `detail` is built with a stub for each header, `<trellis/X.hpp>`, that includes `config.hpp` for the macros and imports `trellis`, or `trellis.testing` for the testing headers; the stubs come first on the include path (`BEFORE`), so only `config.hpp` is read from `include/`. A unit test includes the Trellis headers before its standard and GoogleTest headers, which here would put them after the import; a generated prelude, force-included before each source, includes those headers first, the order recommended above. At T1.8 that runs `config`, `error_namespace`, `fixed_string`, `smoke`, `testing` and `threading` from `trellis_unit_tests`, and `scenario` from `trellis_scenario_tests`; tests of the public API that later tasks add join automatically. It doesn't run the tests that name `trellis::detail` (`create`, `explain`, `override` and the meta toolkit's), which no module exports, nor the programs built in another language mode or with other options (C++23, C++26, without exceptions), nor `trellis_wait_fallback_tests`, whose `TRELLIS_HAS_ATOMIC_WAIT=0` would have to be set in the module's own build. The selection is read from the sources, so CMake reconfigures when one of them changes;
- `module.exports`, below.

Like the single-header twins, the importers built from unit test sources stay out of `compile_commands.json`.

## The export list check

**What.** `tools/module_exports.py` fails when a module under `modules/` misses a public name of the headers it includes, exports something that isn't one, or exports a name outside the namespace that declares it, and when a public name is in a header that no module includes. A module must export exactly the public names of the headers in its global module fragment, less those of a module it re-exports with `export import`: `trellis` exports the umbrella header's 40 names, and `trellis.testing` the 2 that `<trellis/testing/wiring.hpp>` adds. CTest runs the check as `module.exports` in every Clang build with the module, so a task that adds a public entity and forgets the module fails `ctest -L module`, with the line to add and the file:

```text
FAIL module exports: trellis::make_injector is public but not exported: add
`using trellis::v1::make_injector;` to `export namespace trellis::inline v1 { ... }` in
modules/trellis.cppm
```

The public names come from Clang: the check compiles a translation unit that includes the headers of each module, and one with every header under `include/trellis/`, with `-Xclang -ast-dump=json -Xclang -ast-dump-filter=trellis` and walks namespace `trellis`. A public name is a named declaration in `trellis`, in an inline namespace (`v1`), or in a nested namespace other than `detail`, at any depth (`trellis::error`). Specializations, deduction guides and implicit declarations come with their templates; the enumerators of an unscoped enumeration are names of their own, and a using-declaration counts under the name it declares (Clang names `using detail::helper;` "detail::helper"). An operator named by a keyword keeps Clang's spelling, `operator co_await`. Output that isn't Clang's JSON is a failure line, not a traceback. The exports come from the module interfaces, which may hold, after their module declarations, only `export import M;` and `export namespace N { using N::name; ... }` blocks; anything else fails the check, so that the comparison stays exact.

**Why Clang's syntax tree.** Reading the headers as text would have to recognize every form of declaration: variable templates (`bind`), constrained templates, operators, aliases, declarations a macro produces. The compiler already does; the dump of namespace `trellis` takes about a second. It needs Clang, so GCC builds don't run the check, and they don't build the module either. Headers that only some language modes or platforms compile (a future C++26 reflection backend) are seen as the C++20 build sees them, which is how the module is built too.

**Tests:** `tests/tools/module_exports_selftest.py` (label `module;tools`) plants missing, extra, misplaced and duplicate exports, stray declarations and an unexportable namespace alias in small trees, and, with a second module, a missing name, a name of the module it re-exports, a header that no module includes, a re-export of a module that doesn't exist and two files that declare one module.

## The single header

**What.** `tools/amalgamate.py` writes `single_include/trellis.hpp`, which `.gitignore` excludes: a build artifact, which releases will attach (plan §10). It holds every header under `include/trellis/`, the umbrella, the testing header and the internal headers alike, each once, in the order the preprocessor reads them when the umbrella header and then every other header, by path, is included. Each `#include` of a Trellis header is replaced by that header where it stands, unless it has been started, as its include guard would skip it, so `static/injector.hpp`, which includes `static/resolve.hpp` after the injector class so that either can be included first, is followed by it:

- each header loses its include guard, its includes of Trellis headers, and its copyright and SPDX lines; one notice at the top covers the file, and one guard, `TRELLIS_SINGLE_INCLUDE_TRELLIS_HPP`, surrounds it;
- a standard header included unconditionally is included once, at the top, sorted; an `#include` inside `#if` stays in place (`<expected>` or `<variant>` in `meta/result.hpp`);
- the leading comment of each header stays, under a rule naming the file (`// ==== include/trellis/meta/result.hpp ====`).

`--check` writes nothing. It fails when the single header can't be generated, when the result doesn't compile on its own (C++20, `-Wall -Wextra -Wundef -Werror`, included twice, in a directory with nothing else), or when `single_include/trellis.hpp` exists and differs from a fresh generation. An absent artifact passes: in a fresh checkout nothing is generated, so nothing is stale.

**Why.**

- **Strict, not clever.** Generation is text processing, and a mistake would change what the code means without a compile error, so the generator refuses whatever it can't reproduce exactly: a header without a guard that encloses it whole, code before the guard or a directive after it, `#pragma once`, `#include "..."`, a Trellis include inside `#if` or of a missing header, a missing or different notice, and a header that names another header's guard (gone in the single header). It tracks comments, string literals, raw strings, line continuations and numbers, so a `#include` in a comment or a continued `#define` survives unchanged, and the digit separator of `1'000` isn't taken for a quote that hides the comment after it (the class of bug behind R15).
- **Guards go.** Keeping the headers' guards would let a translation unit include both forms. With one guard for the whole file, doing so fails with redefinitions, instead of silently mixing two releases when the copies differ.
- **All headers, not the umbrella's reach.** Internal headers that no public header includes yet, such as `static/validate.hpp`, are in the single header too, so the unit tests, which include them, can run against it.
- **No system header.** The single header carries no `system_header` pragma: `TRELLIS_MODULE_SYSTEM_HEADER` (config.hpp, T1.15) is empty unless `TRELLIS_BUILDING_MODULE` is defined, which only the module's own build does, so the single header compiles as the main file too, as a precompiled header or a header unit compiles it, warning-free under the project warnings and `-Werror` on GCC and Clang (`single_header.main_file`). In the module built by Clang before 20, `static/declared_aggregate.hpp` alone is a system header, because Clang before 20 doesn't apply a diagnostic pragma to an instantiation in an importer; the warnings located there, `-Wdangling-field` included, then don't reach importers.
- **`-Wundef` in `--check`** catches a header placed before `config.hpp`, whose `#if TRELLIS_HAS_*` would otherwise read 0 silently.

**Tests:** `tests/single_header/` builds a twin of every unit test program of `tests/unit/` that links `trellis::trellis`: `trellis_single_header_unit_tests` with every unit test, the C++23 and C++26 ones, `scenario_tests`, `wait_fallback_tests` with its `TRELLIS_HAS_ATOMIC_WAIT=0`, and the two programs without exceptions. The build runs `amalgamate.py --output` into its own directory, and generates a stub for each header, `<trellis/X.hpp>`, that includes the single header, so the test sources compile unchanged and see only the single header; the twins copy each program's sources, definitions, options, include directories and libraries, so they follow `tests/unit/CMakeLists.txt` as it grows. A program's include directories come before the stubs, so configuration fails if one of them, or a `-I` option, names `include/`, which would hand its twin the real headers. `ctest -L single_header` runs them; `TRELLIS_SINGLE_HEADER_TESTS=OFF` leaves them out. The twins stay out of `compile_commands.json`, where clang-tidy's include-cleaner would see every include of a unit test go through a stub and call it unused. `tests/tools/amalgamate_selftest.py` (label `tools`) checks generation and each refusal on small trees, and `--check` with a fresh, a stale and a non-compiling result.
