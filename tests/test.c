#define GHASH_IMPLEMENTATION
#include "ghash.h"

#include <stdio.h>

static int g_failures = 0;

#define CHECK(cond)                                                            \
    do {                                                                       \
        if (!(cond)) {                                                         \
            printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);             \
            g_failures++;                                                      \
        }                                                                      \
    } while (0)

static u8 g_arena_memory[1 * 1024 * 1024];

static void test_basic(void)
{
    GHArena arena;
    GHMap   map;

    gh_arena_init(&arena, g_arena_memory, sizeof g_arena_memory);
    CHECK(gh_init(&map, &arena, 16));

    CHECK(gh_count(&map) == 0);
    CHECK(gh_get(&map, "missing") == (void *)0);
    CHECK(!gh_has(&map, "missing"));

    CHECK(gh_put(&map, "hp", (void *)(uptr)100));
    CHECK(gh_put(&map, "mana", (void *)(uptr)50));
    CHECK(gh_count(&map) == 2);

    CHECK(gh_has(&map, "hp"));
    CHECK((uptr)gh_get(&map, "hp") == 100);
    CHECK((uptr)gh_get(&map, "mana") == 50);
}

static void test_overwrite(void)
{
    GHArena arena;
    GHMap   map;
    usize   before;

    gh_arena_init(&arena, g_arena_memory, sizeof g_arena_memory);
    gh_init(&map, &arena, 16);

    gh_put(&map, "x", (void *)(uptr)1);
    before = gh_count(&map);

    gh_put(&map, "x", (void *)(uptr)2);
    CHECK(gh_count(&map) == before);
    CHECK((uptr)gh_get(&map, "x") == 2);
}

static void test_collisions_and_growth(void)
{
    GHArena arena;
    GHMap   map;
    int     i;
    usize   seen;
    GHIter  it;

    gh_arena_init(&arena, g_arena_memory, sizeof g_arena_memory);
    // NOTE(Giovanni): tiny table on purpose, forces collisions and repeated growth
    gh_init(&map, &arena, 4);

    for (i = 0; i < 1000; i++) {
        char key[32];
        snprintf(key, sizeof key, "key%d", i);
        CHECK(gh_put(&map, key, (void *)(uptr)(i * 3 + 1)));
    }
    CHECK(gh_count(&map) == 1000);

    for (i = 0; i < 1000; i++) {
        char key[32];
        snprintf(key, sizeof key, "key%d", i);
        CHECK(gh_has(&map, key));
        CHECK((uptr)gh_get(&map, key) == (uptr)(i * 3 + 1));
    }

    seen = 0;
    gh_iter_init(&it, &map);
    while (gh_iter_next(&it)) {
        seen++;
        CHECK(gh_has(&map, it.key));
        CHECK(gh_get(&map, it.key) == it.value);
    }
    CHECK(seen == gh_count(&map));
}

static void test_remove(void)
{
    GHArena arena;
    GHMap   map;
    int     i;
    GHIter  it;
    usize   seen;

    gh_arena_init(&arena, g_arena_memory, sizeof g_arena_memory);
    gh_init(&map, &arena, 8);

    for (i = 0; i < 100; i++) {
        char key[32];
        snprintf(key, sizeof key, "k%d", i);
        gh_put(&map, key, (void *)(uptr)(i + 1));
    }
    CHECK(gh_count(&map) == 100);

    // NOTE(Giovanni): remove every even key
    for (i = 0; i < 100; i += 2) {
        char key[32];
        snprintf(key, sizeof key, "k%d", i);
        CHECK(gh_remove(&map, key));
    }
    CHECK(gh_count(&map) == 50);

    for (i = 0; i < 100; i++) {
        char key[32];
        snprintf(key, sizeof key, "k%d", i);
        if (i % 2 == 0) {
            CHECK(!gh_has(&map, key));
        } else {
            CHECK(gh_has(&map, key));
            CHECK((uptr)gh_get(&map, key) == (uptr)(i + 1));
        }
    }

    CHECK(!gh_remove(&map, "k0"));   // NOTE(Giovanni): already gone
    CHECK(!gh_remove(&map, "nope")); // NOTE(Giovanni): never existed

    // NOTE(Giovanni): re-put a removed key
    CHECK(gh_put(&map, "k0", (void *)(uptr)777));
    CHECK((uptr)gh_get(&map, "k0") == 777);

    seen = 0;
    gh_iter_init(&it, &map);
    while (gh_iter_next(&it)) seen++;
    CHECK(seen == gh_count(&map));
}

static void test_null_value(void)
{
    GHArena arena;
    GHMap   map;

    gh_arena_init(&arena, g_arena_memory, sizeof g_arena_memory);
    gh_init(&map, &arena, 8);

    CHECK(gh_put(&map, "nil", (void *)0));
    CHECK(gh_has(&map, "nil"));              // NOTE(Giovanni): present...
    CHECK(gh_get(&map, "nil") == (void *)0); // ...but value is NULL
}

static void test_long_keys(void)
{
    GHArena arena;
    GHMap   map;
    char    key[512];
    int     i;

    gh_arena_init(&arena, g_arena_memory, sizeof g_arena_memory);
    gh_init(&map, &arena, 8);

    for (i = 0; i < (int)sizeof key - 1; i++) key[i] = (char)('a' + (i % 26));
    key[sizeof key - 1] = '\0';

    CHECK(gh_put(&map, key, (void *)(uptr)42));
    CHECK((uptr)gh_get(&map, key) == 42);
    CHECK(gh_has(&map, key));

    // NOTE(Giovanni): a key that differs only in the final byte must not match
    key[sizeof key - 2] = (key[sizeof key - 2] == 'z') ? 'a' : key[sizeof key - 2] + 1;
    CHECK(!gh_has(&map, key));
}

static void test_exhaustion(void)
{
    GHArena arena;
    GHMap   map;
    u8      tiny[16];
    u8      small[256];
    int     i;
    b32     all_ok = true;

    // NOTE(Giovanni): 16 bytes cannot hold even the smallest table
    gh_arena_init(&arena, tiny, sizeof tiny);
    CHECK(!gh_init(&map, &arena, 8));

    // NOTE(Giovanni): init fits, but puts eventually fail without crashing and
    // the table stays queryable
    gh_arena_init(&arena, small, sizeof small);
    CHECK(gh_init(&map, &arena, 8));
    for (i = 0; i < 100; i++) {
        char key[32];
        snprintf(key, sizeof key, "e%d", i);
        if (!gh_put(&map, key, (void *)(uptr)(i + 1))) all_ok = false;
    }
    CHECK(!all_ok); // NOTE(Giovanni): it must have run out of arena at some point
    CHECK(gh_count(&map) > 0);

    // NOTE(Giovanni): everything that was accepted is still findable
    for (i = 0; i < 100; i++) {
        char key[32];
        snprintf(key, sizeof key, "e%d", i);
        if (gh_has(&map, key)) {
            CHECK((uptr)gh_get(&map, key) == (uptr)(i + 1));
        }
    }
}

int main(void)
{
    test_basic();
    test_overwrite();
    test_collisions_and_growth();
    test_remove();
    test_null_value();
    test_long_keys();
    test_exhaustion();

    if (g_failures == 0) {
        printf("all tests passed\n");
        return 0;
    }
    printf("%d check(s) failed\n", g_failures);
    return 1;
}
