### All compilers: median ns per access (ratio to C wait in the same run)
| benchmark | design | spin8 | spin32 | spin128 |
|---|---|---|---|---|
| cold8_same/inline | A once |  |  | 46.2 (4.04x) |
| cold8_same/inline | B cv | 3.38 (0.42x) | 2.5 (0.32x) | 2.57 (0.23x) |
| cold8_same/inline | C wait | 8.12 | 7.69 | 11.4 |
| cold8_same/inline | C+spin | 13 (1.60x) | 4.95 (0.64x) | 9.56 (0.84x) |
| cold8_same_spin1us/inline | A once |  |  | 253 (1.33x) |
| cold8_same_spin1us/inline | B cv | 170 (0.90x) | 150 (0.80x) | 153 (0.80x) |
| cold8_same_spin1us/inline | C wait | 189 | 187 | 190 |
| cold8_same_spin1us/inline | C+spin | 195 (1.04x) | 188 (1.01x) | 48.3 (0.25x) |
