# C++ dependency injection research

Research into whether C++ has good dependency injection frameworks, current as of October 2026.

- [`reports/Cpp dependency injection frameworks.md`](reports/Cpp%20dependency%20injection%20frameworks.md): the report, with the verdict, comparison tables and recommendations.
- [`docs/plan.md`](../docs/plan.md): a plan for building a C++ DI framework from scratch, based on the research.
- [`research_notes/Cpp dependency injection frameworks/`](research_notes/Cpp%20dependency%20injection%20frameworks/): the sourced notes behind the report, one file per topic: Boost.DI, Google Fruit, runtime containers, long-tail libraries and C++26 reflection, patterns and practice, and the hands-on evaluation.
- [`experiments/`](experiments/): the same small program wired by hand and with Boost.DI, Fruit, Kangaru, dingo, Hypodermic and qlibs/di. Scripts measure compile time, compiler memory, binary size, missing-binding diagnostics and resolution cost with GCC and Clang. [`experiments/README.md`](experiments/README.md) explains how to rerun them.

No third-party code is stored in this repo. `experiments/scripts/fetch.sh <deps-dir>` clones each framework at a pinned version into a directory you choose.
