standard library: libstdc++ 20240908
| what | type | A call_once | B atomic+cv | C atomic+wait | none |
|---|---|---|---|---|---|
| sync per slot | | 4 (once_flag) | 1 (atomic<u8>) | 1 (atomic<u8>) | 1 (u8) |
| sync per injector | | 0 | 88 (mutex 40 + condition_variable 48) | 0 | 0 |
| slot | shared<1B> (1 B) | 24 | 24 | 24 | 24 |
| slot | shared<64B> (64 B) | 24 | 24 | 24 | 24 |
| slot | inline<1B> (1 B) | 8 | 2 | 2 | 2 |
| slot | inline<8B> (8 B) | 16 | 16 | 16 | 16 |
| slot | inline<64B> (64 B) | 68 | 65 | 65 | 65 |
other: shared_ptr 16, teardown_entry 16, teardown_stack<100> 1608, thread::id 8
