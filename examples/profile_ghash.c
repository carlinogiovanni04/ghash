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

static unsigned char   g_arena_memory[GV_ARENA_BYTES];
static char            g_keys[GV_KEYS][32];
static char            g_miss_keys[GV_KEYS][32];
static volatile uptr   g_sink; // NOTE(Giovanni): keeps lookups from being optimized away

static void build_keys(void)
{
    int i;
    for (i = 0; i < GV_KEYS; i++) {
        snprintf(g_keys[i], sizeof g_keys[i], "key%d", i);
        snprintf(g_miss_keys[i], sizeof g_miss_keys[i], "miss%d", i);
    }
}

int main(int argc, char **argv)
{
    const char *out = argc > 1 ? argv[1] : "hash_trace.json";
    GHArena     arena;
    GHMap       map;
    int         round;

    build_keys();

    gh_arena_init(&arena, g_arena_memory, sizeof g_arena_memory);
    if (!gh_init(&map, &arena, 64)) {
        printf("profile_ghash: could not initialize map\n");
        return 1;
    }

    PROF_SCOPE("ghash.session") {
        for (round = 0; round < GV_ROUNDS; round++) {
            PROF_SCOPE("round") {
                int  i;
                uptr acc = 0;

                PROF_SCOPE("insert") {
                    for (i = 0; i < GV_KEYS; i++)
                        gh_put(&map, g_keys[i], (void *)(uptr)(i + 1));
                }

                PROF_SCOPE("lookup_hit") {
                    for (i = 0; i < GV_KEYS; i++)
                        acc += (uptr)gh_get(&map, g_keys[i]);
                }

                PROF_SCOPE("lookup_miss") {
                    for (i = 0; i < GV_KEYS; i++)
                        acc += (uptr)gh_get(&map, g_miss_keys[i]);
                }

                PROF_SCOPE("probe") {
                    for (i = 0; i < GV_KEYS; i++)
                        acc += (uptr)gh_has(&map, g_keys[i]);
                }

                PROF_SCOPE("iterate") {
                    GHIter it;
                    gh_iter_init(&it, &map);
                    while (gh_iter_next(&it)) acc += (uptr)it.value;
                }

                PROF_SCOPE("remove_even") {
                    for (i = 0; i < GV_KEYS; i += 2)
                        acc += (uptr)gh_remove(&map, g_keys[i]);
                }

                PROF_SCOPE("reinsert_even") {
                    for (i = 0; i < GV_KEYS; i += 2)
                        gh_put(&map, g_keys[i], (void *)(uptr)(i + 1));
                }

                g_sink += acc;
            }

            prof_frame_end();
        }
    }

    // NOTE(Giovanni): report AFTER the session zone closes, otherwise it runs
    // while the only top-level scope is still open and total_ns is still 0.
    prof_report();

    if (!prof_write_trace(out)) {
        printf("profile_ghash: could not write %s\n", out);
        return 1;
    }

    printf("wrote %s (%llu events)\n", out,
           (unsigned long long)prof_event_count());
    return 0;
}
