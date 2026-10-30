Median compile time (s) and peak memory (MB) of `-std=c++20 -O2 -c`, and in brackets the ratio to hand-wiring (manual) in the same run: time×, memory×. Every compiled program was linked, run and printed the equivalence oracle's line.

| compiler | fan-in | N | manual | memo | depth | path |
|---|---|---|---|---|---|---|
| clang-18 | 1-3 | 10 |  |  |  | 1.35 s, 141 MB (3 runs) |
| clang-18 | 1-3 | 15 |  |  |  | 2.77 s, 203 MB (3 runs) |
| clang-18 | 1-3 | 20 |  |  |  | 12.09 s, 783 MB |
| clang-18 | 1-3 | 25 | 2.52 s, 146 MB (3 runs) | 2.31 s, 151 MB [0.92×, 1.04×] (3 runs) | 2.51 s, 166 MB [1.00×, 1.14×] (3 runs) | 34.44 s, 1934 MB [13.67×, 13.25×] |
| clang-18 | 1-3 | 50 | 4.01 s, 180 MB (3 runs) | 4.22 s, 189 MB [1.05×, 1.05×] (3 runs) | 5.38 s, 260 MB [1.34×, 1.45×] (3 runs) | **memory cap** after 67 s, 3698 MB |
| clang-18 | 1-3 | 100 | 7.60 s, 248 MB (3 runs) | 7.05 s, 264 MB [0.93×, 1.06×] (3 runs) | 14.30 s, 561 MB [1.88×, 2.26×] | **not attempted** |
| clang-18 | 1-3 | 250 | 21.00 s, 454 MB | 18.46 s, 494 MB [0.88×, 1.09×] | 73.23 s, 2510 MB [3.49×, 5.53×] |  |
| clang-18 | 1-3 | 500 | 45.03 s, 796 MB | 39.64 s, 875 MB [0.88×, 1.10×] |  |  |
| clang-18 | 1-3 | 1000 | 124.09 s, 1482 MB | 88.88 s, 1631 MB [0.72×, 1.10×] |  |  |
| clang-18 | 10 | 10 |  |  |  | 1.98 s, 152 MB (3 runs) |
| clang-18 | 10 | 15 |  |  |  | 12.92 s, 708 MB |
| clang-18 | 10 | 20 |  |  |  | **memory cap** after 75 s, 3692 MB |
| clang-18 | 10 | 25 | 2.89 s, 153 MB (3 runs) | 2.97 s, 154 MB [1.03×, 1.01×] (3 runs) | 3.62 s, 185 MB [1.25×, 1.21×] (3 runs) | **not attempted** |
| clang-18 | 10 | 50 | 5.42 s, 196 MB (3 runs) | 5.66 s, 196 MB [1.05×, 1.00×] (3 runs) | 9.56 s, 360 MB [1.76×, 1.84×] (3 runs) | **not attempted** |
| clang-18 | 10 | 100 | 12.58 s, 279 MB | 13.09 s, 278 MB [1.04×, 1.00×] | 30.26 s, 1002 MB [2.41×, 3.59×] | **not attempted** |
| clang-18 | 10 | 250 | 28.43 s, 528 MB | 31.23 s, 524 MB [1.10×, 0.99×] | **memory cap** after 94 s, 3692 MB |  |
| gcc-13 | 1-3 | 10 |  |  |  | 1.38 s, 130 MB (3 runs) |
| gcc-13 | 1-3 | 15 |  |  |  | 2.06 s, 175 MB (3 runs) |
| gcc-13 | 1-3 | 20 |  |  |  | 5.98 s, 467 MB (3 runs) |
| gcc-13 | 1-3 | 25 | 2.60 s, 180 MB (3 runs) | 2.31 s, 173 MB [0.89×, 0.96×] (3 runs) | 2.45 s, 179 MB [0.94×, 0.99×] (3 runs) | 12.68 s, 1068 MB [4.88×, 5.92×] |
| gcc-13 | 1-3 | 50 | 4.02 s, 255 MB (3 runs) | 3.54 s, 238 MB [0.88×, 0.93×] (3 runs) | 3.67 s, 270 MB [0.91×, 1.06×] (3 runs) | **memory cap** after 51 s, 3881 MB |
| gcc-13 | 1-3 | 100 | 7.59 s, 383 MB (3 runs) | 6.46 s, 353 MB [0.85×, 0.92×] (3 runs) | 9.13 s, 466 MB [1.20×, 1.22×] (3 runs) | **memory cap** after 43 s, 3881 MB |
| gcc-13 | 1-3 | 250 | 21.21 s, 814 MB | 16.20 s, 739 MB [0.76×, 0.91×] | 34.88 s, 1601 MB [1.64×, 1.97×] |  |
| gcc-13 | 1-3 | 500 | 44.27 s, 1564 MB | 43.20 s, 1415 MB [0.98×, 0.90×] |  |  |
| gcc-13 | 1-3 | 1000 | 179.48 s, 3107 MB | 110.43 s, 2738 MB [0.62×, 0.88×] |  |  |
| gcc-13 | 10 | 10 |  |  |  | 2.02 s, 146 MB (3 runs) |
| gcc-13 | 10 | 15 |  |  |  | 6.12 s, 417 MB (3 runs) |
| gcc-13 | 10 | 20 |  |  |  | 38.83 s, 2551 MB |
| gcc-13 | 10 | 25 | 2.80 s, 191 MB (3 runs) | 2.59 s, 180 MB [0.92×, 0.94×] (3 runs) | 2.72 s, 193 MB [0.97×, 1.01×] (3 runs) | **memory cap** after 54 s, 3881 MB |
| gcc-13 | 10 | 50 | 4.85 s, 286 MB (3 runs) | 4.54 s, 269 MB [0.93×, 0.94×] (3 runs) | 5.69 s, 315 MB [1.17×, 1.10×] (3 runs) | **not attempted** |
| gcc-13 | 10 | 100 | 11.01 s, 439 MB | 10.07 s, 409 MB [0.91×, 0.93×] | 15.31 s, 632 MB [1.39×, 1.44×] | **not attempted** |
| gcc-13 | 10 | 250 | 33.46 s, 896 MB | 26.45 s, 867 MB [0.79×, 0.97×] | 68.01 s, 2426 MB [2.03×, 2.71×] |  |
