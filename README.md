# ghash

A string keyed hash map for C in a single header. It maps null-terminated C strings to
`void *` values using separate chaining, backed by a caller-supplied
arena allocator.

## Including it

In exactly **one** `.c` file:

```c
#define GHASH_IMPLEMENTATION
#include "ghash.h"
```

Everywhere else just `#include "ghash.h"`.

## Using it

```c
GHArena arena;
GHMap   map;
u8      memory[65536];

gh_arena_init(&arena, memory, sizeof memory);
gh_init(&map, &arena, 16);              /* 16 = expected entries */

gh_put(&map, "hp",   (void *)(uptr)100);
gh_put(&map, "mana", (void *)(uptr)50);

int hp = (int)(uptr)gh_get(&map, "hp"); /* 100 */
int has_luck = gh_has(&map, "luck");    /* 0 */

gh_put(&map, "hp", (void *)(uptr)80);   /* overwrite */
gh_remove(&map, "mana");

GHIter it;
gh_iter_init(&it, &map);
while (gh_iter_next(&it)) {
    printf("%s = %llu\n", it.key, (unsigned long long)(uptr)it.value);
}
```

## API

| Function                                  | Meaning                                                            |
| ----------------------------------------- | ------------------------------------------------------------------ |
| `gh_arena_init(a, memory, size)`          | Point an arena at memory you own.                                  |
| `gh_init(m, a, initial_capacity)`         | Prepare a map; capacity rounded up to a power of two.              |
| `gh_put(m, key, value)`                   | Insert or overwrite. Copies the key. Returns false if out of arena. |
| `gh_get(m, key)`                          | Value for key, or `NULL` if absent.                                |
| `gh_has(m, key)`                          | Whether key is present.                                            |
| `gh_remove(m, key)`                       | Unlink key. Returns false if it wasn't there.                      |
| `gh_count(m)`                             | Number of live entries.                                            |
| `gh_iter_init(it, m)` / `gh_iter_next(it)`| Iterate; on each true fill `it->key` / `it->value`. Order arbitrary. |

## Configuration

Define before including `ghash.h`:

| Macro            | Default | Meaning                                    |
| ---------------- | ------- | ------------------------------------------ |
| `GH_LOAD_NUM`    | 3       | Load factor numerator (grow above NUM/DEN).|
| `GH_LOAD_DEN`    | 4       | Load factor denominator.                   |
| `GH_MIN_BUCKETS` | 8       | Smallest table size (power of two).        |
