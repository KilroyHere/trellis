Smallest -ftemplate-depth that compiles a chain of K hops; the slope is the instantiation levels per hop. The longest chain is at the compiler's default limits (-ftemplate-depth=900 on GCC, 1024 on Clang).

| compiler | config | 50 hops | 100 hops | 250 hops | 500 hops | levels/hop | longest chain at default limits | what stops it |
|---|---|---|---|---|---|---|---|---|
| gcc-13 | s1-stub value | 312 | 612 | 1512 | 3012 | 6 | 148 | template depth |
| gcc-13 | memo value | 154 | 304 | 754 | 1504 | 3 | 298 | template depth |
| gcc-13 | memo shared_ptr | 215 | 415 | 1015 | 2015 | 4 | 221 | template depth |
| gcc-13 | depth value | 154 | 304 | 754 | 1504 | 3 | 298 | template depth |
| gcc-13 | path value | 154 | 304 | 754 | 1504 | 3 | 298 | template depth |
| gcc-13 | memo value, check only | 110 | 210 | 510 | 1010 | 2 | 445 | template depth |
| gcc-13 | memo shared_ptr, check only | 110 | 210 | 510 | 1010 | 2 | 445 | template depth |
| gcc-14 | s1-stub value | 312 | 612 | 1512 | 3012 | 6 | 148 | template depth |
| gcc-14 | memo value | 155 | 305 | 755 | 1505 | 3 | 298 | template depth |
| gcc-14 | memo shared_ptr | 215 | 415 | 1015 | 2015 | 4 | 221 | template depth |
| gcc-14 | depth value | 155 | 305 | 755 | 1505 | 3 | 298 | template depth |
| gcc-14 | path value | 155 | 305 | 755 | 1505 | 3 | 298 | template depth |
| gcc-14 | memo value, check only | 110 | 210 | 510 | 1010 | 2 | 445 | template depth |
| gcc-14 | memo shared_ptr, check only | 110 | 210 | 510 | 1010 | 2 | 445 | template depth |
| clang-18 | s1-stub value | 418 | 818 | 2018 | 4018 | 8 | 125 | template depth |
| clang-18 | memo value | 218 | 418 | 1018 | 2018 | 4 | 251 | template depth |
| clang-18 | memo shared_ptr | 218 | 418 | 1018 | 2018 | 4 | 251 | template depth |
| clang-18 | depth value | 218 | 418 | 1018 | 2018 | 4 | 251 | template depth |
| clang-18 | path value | 218 | 418 | 1018 | - | 4 | 251 | template depth |
| clang-18 | memo value, check only | 217 | 417 | 1017 | 2017 | 4 | 251 | template depth |
| clang-18 | memo shared_ptr, check only | 217 | 417 | 1017 | 2017 | 4 | 251 | template depth |
| clang-20 | s1-stub value | 418 | 818 | 2018 | 4018 | 8 | 125 | template depth |
| clang-20 | memo value | 218 | 418 | 1018 | 2018 | 4 | 251 | template depth |
| clang-20 | memo shared_ptr | 218 | 418 | 1018 | 2018 | 4 | 251 | template depth |
| clang-20 | depth value | 218 | 418 | 1018 | 2018 | 4 | 251 | template depth |
| clang-20 | path value | 218 | 418 | 1018 | - | 4 | 251 | template depth |
| clang-20 | memo value, check only | 217 | 417 | 1017 | 2017 | 4 | 251 | template depth |
| clang-20 | memo shared_ptr, check only | 217 | 417 | 1017 | 2017 | 4 | 251 | template depth |

Not measured:
- clang-18 path value: 500 hops: other error at -ftemplate-depth=8128: spikes/S2-memoization-cycles/stub.hpp:217:79: fatal error: instantiating fold expression with 257 a
- clang-20 path value: 500 hops: other error at -ftemplate-depth=8128: spikes/S2-memoization-cycles/stub.hpp:217:79: fatal error: instantiating fold expression with 257 a
