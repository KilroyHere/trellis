| compiler | headers | preprocessed lines | -fsyntax-only, median of 5 (s) |
|---|---|---|---|
| g++-14 | baseline (C: atomic wait) | 37718 | 0.294 |
| g++-14 | + <mutex> (A: call_once) | 40863 | 0.325 |
| g++-14 | + <mutex>, <condition_variable> (B) | 42415 | 0.362 |
| clang++-18 | baseline (C: atomic wait) | 37696 | 0.350 |
| clang++-18 | + <mutex> (A: call_once) | 40837 | 0.410 |
| clang++-18 | + <mutex>, <condition_variable> (B) | 42389 | 0.438 |
