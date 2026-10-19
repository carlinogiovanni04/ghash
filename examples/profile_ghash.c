//
// Profiles a ghash workload with microprof and writes a trace gview can open.
//
//   profile_ghash [output.json]
//

#define GHASH_IMPLEMENTATION
#include "ghash.h"

#define PROF_PROCESS_NAME "ghash example"
#define MICROPROF_IMPLEMENTATION
#include "microprof.h"

#include <stdio.h>

#define GV_ARENA_BYTES (8u * 1024u * 1024u)
#define GV_KEYS        500
#define GV_ROUNDS      5

static unsigned char g_arena_memory[GV_ARENA_BYTES];

static void prof_put(GHMap *map, const char *key, uptr value)
{
    PROF_SCOPE("gh_put") {
        gh_put(map, key, (void *)value);
    }
}

static void prof_get(const GHMap *map, const char *key)
{
    PROF_SCOPE("gh_get") {
        (void)gh_get(map, key);
    }
}

static void prof_has(const GHMap *map, const char *key)
{
    PROF_SCOPE("gh_has") {
        (void)gh_has(map, key);
    }
}

static void prof_remove(GHMap *map, const char *key)
{
    PROF_SCOPE("gh_remove") {
        (void)gh_remove(map, key);
    }
}

static void prof_iterate(const GHMap *map)
{
    PROF_SCOPE("gh_iterate") {
        GHIter it;
        uptr   sum = 0;
        gh_iter_init(&it, map);
        while (gh_iter_next(&it)) {
            sum += (uptr)it.value;
        }
        (void)sum;
    }
}

int main(int argc, char **argv)
{
    const char *out = argc > 1 ? argv[1] : "tests/fixtures/ghash_trace.json";
    GHArena     arena;
    GHMap       map;
    int         round;

    gh_arena_init(&arena, g_arena_memory, sizeof g_arena_memory);
    if (!gh_init(&map, &arena, 64)) {
        printf("profile_ghash: could not initialize map\n");
        return 1;
    }

    PROF_SCOPE("ghash.session") {
        for (round = 0; round < GV_ROUNDS; round++) {
            PROF_SCOPE("round") {
                int i;

                PROF_SCOPE("insert") {
                    for (i = 0; i < GV_KEYS; i++) {
                        char key[32];
                        snprintf(key, sizeof key, "key%d", i);
                        prof_put(&map, key, (uptr)(i + 1));
                    }
                }

                PROF_SCOPE("lookup_hit") {
                    for (i = 0; i < GV_KEYS; i++) {
                        char key[32];
                        snprintf(key, sizeof key, "key%d", i);
                        prof_get(&map, key);
                    }
                }

                PROF_SCOPE("lookup_miss") {
                    for (i = 0; i < GV_KEYS; i++) {
                        char key[32];
                        snprintf(key, sizeof key, "miss%d", i);
                        prof_get(&map, key);
                    }
                }

                PROF_SCOPE("probe") {
                    for (i = 0; i < GV_KEYS; i++) {
                        char key[32];
                        snprintf(key, sizeof key, "key%d", i);
                        prof_has(&map, key);
                    }
                }

                PROF_SCOPE("iterate") {
                    prof_iterate(&map);
                }

                PROF_SCOPE("remove_even") {
                    for (i = 0; i < GV_KEYS; i += 2) {
                        char key[32];
                        snprintf(key, sizeof key, "key%d", i);
                        prof_remove(&map, key);
                    }
                }

                PROF_SCOPE("reinsert_even") {
                    for (i = 0; i < GV_KEYS; i += 2) {
                        char key[32];
                        snprintf(key, sizeof key, "key%d", i);
                        prof_put(&map, key, (uptr)(i + 1));
                    }
                }
            }

            prof_frame_end();
        }

        PROF_SCOPE("report") {
            prof_report();
        }
    }

    if (!prof_write_trace(out)) {
        printf("profile_ghash: could not write %s\n", out);
        return 1;
    }

    printf("wrote %s (%llu events)\n", out,
           (unsigned long long)prof_event_count());
    return 0;
}
