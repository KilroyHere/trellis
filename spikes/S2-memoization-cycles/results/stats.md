Nodes each strategy instantiates, and recursion depths in hops. The memo check's stack is the depth of its depth-first search, with the parameters in the generated order (k-1 last) and reversed (k-1 first); the depth and path strategies recurse along every path, so they reach the longest path.

| fan-in | N | edges | memo: nodes | depth: (node, depth) pairs | path: (node, path) pairs | longest path | memo stack | memo stack, reversed parameters |
|---|---|---|---|---|---|---|---|---|
| 1-3 | 25 | 69 | 25 | 253 | 3.51e+04 | 25 | 7 | 25 |
| 1-3 | 50 | 144 | 50 | 1,080 | 3.27e+07 | 50 | 11 | 50 |
| 1-3 | 100 | 294 | 100 | 4,278 | 2.88e+10 | 100 | 14 | 100 |
| 1-3 | 250 | 743 | 250 | 28,514 | 3.15e+16 | 250 | 35 | 250 |
| 1-3 | 500 | 1493 | 500 | 118,984 | 6.64e+24 | 500 | 48 | 500 |
| 1-3 | 1000 | 2993 | 1000 | 480,163 | 2.16e+35 | 1000 | 66 | 1000 |
| 10 | 25 | 195 | 25 | 282 | 1.3e+06 | 24 | 4 | 24 |
| 10 | 50 | 436 | 50 | 1,162 | 7.29e+09 | 46 | 6 | 46 |
| 10 | 100 | 901 | 100 | 4,736 | 7.97e+14 | 91 | 8 | 91 |
| 10 | 250 | 2251 | 250 | 29,953 | 1.18e+24 | 226 | 16 | 226 |
| 10 | 500 | 4501 | 500 | 121,621 | 9e+35 | 451 | 21 | 451 |
| 10 | 1000 | 9001 | 1000 | 488,108 | 3.61e+50 | 901 | 35 | 901 |
