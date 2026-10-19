#define GHASH_IMPLEMENTATION
#include "ghash.h"

#include <stdio.h>

static u8 g_arena_memory[16 * 1024];

int main(void)
{
    GHArena arena;
    GHMap   map;
    GHIter  it;
    int     hp;

    gh_arena_init(&arena, g_arena_memory, sizeof g_arena_memory);
    gh_init(&map, &arena, 16);

    // NOTE(Giovanni): numeric values smuggled through void*
    gh_put(&map, "hp", (void *)(uptr)100);
    gh_put(&map, "mana", (void *)(uptr)50);
    gh_put(&map, "level", (void *)(uptr)7);

    hp = (int)(uptr)gh_get(&map, "hp");
    printf("hp    = %d\n", hp);
    printf("luck? = %s\n", gh_has(&map, "luck") ? "yes" : "no");

    gh_put(&map, "hp", (void *)(uptr)80); // NOTE(Giovanni): overwrite
    printf("hp    = %d (after overwrite)\n", (int)(uptr)gh_get(&map, "hp"));

    gh_remove(&map, "mana");
    printf("mana? = %s\n", gh_has(&map, "mana") ? "yes" : "no");

    printf("count = %llu\n", (unsigned long long)gh_count(&map));
    printf("contents:\n");
    gh_iter_init(&it, &map);
    while (gh_iter_next(&it)) {
        printf("  %-8s -> %llu\n", it.key, (unsigned long long)(uptr)it.value);
    }

    printf("arena used = %llu / %llu bytes\n",
           (unsigned long long)arena.used,
           (unsigned long long)arena.cap);
    return 0;
}
