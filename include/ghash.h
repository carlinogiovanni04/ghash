//
//   COPYRIGHT (C) - Giovanni Carlino - 2026 - ALL RIGHTS RESERVED
//
//   ghash - single header string keyed hash map
//
//   Maps null-terminated C strings to void* values using a separate chaining
//   hash table that lives entirely in a caller supplied arena. Nothing here
//   calls malloc.
//
// Usage:
//   #define GHASH_IMPLEMENTATION   // in exactly ONE .c file
//   #include "ghash.h"
//
//   GHArena arena;
//   GHMap   map;
//   u8      buffer[65536];
//
//   gh_arena_init(&arena, buffer, sizeof buffer);
//   gh_init(&map, &arena, 16);            // 16 = expected number of entries
//
//   gh_put(&map, "hp",   (void *)(uptr)100);
//   gh_put(&map, "mana", (void *)(uptr)50);
//
//   int hp = (int)(uptr)gh_get(&map, "hp");
//   gh_has(&map, "luck");                 // false
//   gh_remove(&map, "mana");
//
//   GHIter it;
//   gh_iter_init(&it, &map);
//   while (gh_iter_next(&it)) {
//       printf("%s = %p\n", it.key, it.value);
//   }
//
// Notes:
//   - Keys are COPIED into the arena, so the string you pass can be freed or
//     reused as soon as gh_put returns.
//   - Values are stored as-is (the pointer is not copied or owned). NULL is a
//     valid value, so use gh_has() when NULL is meaningful to you.
//   - The table GROWS by doubling: the new bucket array is allocated from the
//     arena and the old one is abandoned. Budget a little slack for the retired arrays.
//   - There is no removal of nodes back to the arena; gh_remove just unlinks.
//

#ifndef GHASH_H
#define GHASH_H

// NOTE(Giovanni): Base types, skipped if base.h already provided them.
#if !defined(BASE_H)

#if defined(_MSC_VER)
typedef signed char        i8;
typedef short              i16;
typedef int                i32;
typedef long long          i64;
typedef unsigned char      u8;
typedef unsigned short     u16;
typedef unsigned int       u32;
typedef unsigned long long u64;
typedef unsigned long long usize;
typedef unsigned long long uptr;
#else
typedef __INT8_TYPE__   i8;
typedef __INT16_TYPE__  i16;
typedef __INT32_TYPE__  i32;
typedef __INT64_TYPE__  i64;
typedef __UINT8_TYPE__  u8;
typedef __UINT16_TYPE__ u16;
typedef __UINT32_TYPE__ u32;
typedef __UINT64_TYPE__ u64;
typedef __SIZE_TYPE__   usize;
typedef __SIZE_TYPE__   uptr;
#endif

typedef float  f32;
typedef double f64;
typedef i32    b32;

#endif // !BASE_H

#ifndef true
#define true  1
#endif
#ifndef false
#define false 0
#endif

// NOTE(Giovanni): Optional configuration, define before including ghash.h.

// NOTE(Giovanni): Load factor as a fraction, grow when count/buckets exceeds
// NUM/DEN. Default is 3/4 = 0.75.
#ifndef GH_LOAD_NUM
#define GH_LOAD_NUM 3
#endif
#ifndef GH_LOAD_DEN
#define GH_LOAD_DEN 4
#endif

// NOTE(Giovanni): Smallest table size, also used when you ask for 0. Power of two.
#ifndef GH_MIN_BUCKETS
#define GH_MIN_BUCKETS 8
#endif

// NOTE(Giovanni): A bump allocator over memory YOU provide. ghash never frees.
typedef struct GHArena {
    u8   *base;
    usize cap;
    usize used;
} GHArena;

// NOTE(Giovanni): One key/value pair plus its chain link. The key bytes live
// inline right after the header (flexible array member), so a node and its key
// are a single arena allocation and share a cache line.
typedef struct GHNode {
    struct GHNode *next;
    u64            hash;
    void          *value;
    char           key[];
} GHNode;

typedef struct GHMap {
    GHNode **buckets;      // array of bucket_count chain heads
    usize    bucket_count; // power of two, so idx = hash & (count - 1)
    usize    count;        // number of live entries
    GHArena *arena;        // where buckets, nodes and key copies live
} GHMap;

typedef struct GHIter {
    const GHMap *map;
    usize        bucket;
    GHNode      *node;
    const char  *key;
    void        *value;
} GHIter;

// NOTE(Giovanni): Point an arena at a block of memory you own. size is in bytes.
void gh_arena_init(GHArena *arena, void *memory, usize size);

// NOTE(Giovanni): Prepare map on arena. initial_capacity is rounded up to a power
// of two and clamped to at least GH_MIN_BUCKETS.
b32 gh_init(GHMap *map, GHArena *arena, usize initial_capacity);

// NOTE(Giovanni): Insert or overwrite. Copies the key into the arena. Grows the
// table when the load factor is exceeded.
b32 gh_put(GHMap *map, const char *key, void *value);

// NOTE(Giovanni): Value for key, or NULL if absent. NULL values are
// indistinguishable from a miss here, use gh_has() instead if you need to 
// disambiguate.
void *gh_get(const GHMap *map, const char *key);

// NOTE(Giovanni): Whether key is present.
b32 gh_has(const GHMap *map, const char *key);

// NOTE(Giovanni): Unlink key. Arena memory is not reused.
b32 gh_remove(GHMap *map, const char *key);

// NOTE(Giovanni): Number of live entries.
usize gh_count(const GHMap *map);

// NOTE(Giovanni): Start an iteration, then call gh_iter_next() until it returns
// false. On each true it fills it->key and it->value. Order is arbitrary.
void gh_iter_init(GHIter *it, const GHMap *map);
b32  gh_iter_next(GHIter *it);

#endif // GHASH_H

#if defined(GHASH_IMPLEMENTATION)

// NOTE(Giovanni): Tiny libc-free helpers.

static usize gh__str_len(const char *s)
{
    usize n = 0;
    while (s[n]) n++;
    return n;
}

static b32 gh__str_eq(const char *a, const char *b)
{
    while (*a && *a == *b) {
        a++;
        b++;
    }
    return *a == *b;
}

static void gh__mem_copy(void *dst, const void *src, usize n)
{
    u8       *d = (u8 *)dst;
    const u8 *s = (const u8 *)src;
    usize     i;
    for (i = 0; i < n; i++) d[i] = s[i];
}

static void gh__mem_zero(void *dst, usize n)
{
    u8   *d = (u8 *)dst;
    usize i;
    for (i = 0; i < n; i++) d[i] = 0;
}

// NOTE(Giovanni): Bump allocate size bytes, aligned to 16, or NULL if the arena is full.
static void *gh__arena_alloc(GHArena *arena, usize size)
{
    usize aligned = (arena->used + 15u) & ~(usize)15u;
    void *p;
    if (size == 0) return (void *)0;
    if (aligned > arena->cap || size > arena->cap - aligned) return (void *)0;
    p = arena->base + aligned;
    arena->used = aligned + size;
    return p;
}

// NOTE(Giovanni): FNV-1a, 64-bit. Same key always hashes the same.
static u64 gh__hash(const char *s)
{
    u64 h = 14695981039346656037ull;
    while (*s) {
        h ^= (u8)*s++;
        h *= 1099511628211ull;
    }
    return h;
}

// NOTE(Giovanni): Walk a bucket chain looking for an exact key.
static GHNode *gh__find(const GHMap *map, const char *key, u64 hash)
{
    usize   idx = (usize)hash & (map->bucket_count - 1);
    GHNode *node = map->buckets[idx];
    while (node) {
        if (node->hash == hash && gh__str_eq(node->key, key)) return node;
        node = node->next;
    }
    return (GHNode *)0;
}

// NOTE(Giovanni): Double the table. Allocate a fresh bucket array from the arena,
// relink every existing node into it, let the old array become dead space.
static b32 gh__grow(GHMap *map)
{
    usize    new_count = map->bucket_count << 1;
    GHNode **new_buckets;
    usize    i;

    new_buckets = (GHNode **)gh__arena_alloc(map->arena, new_count * sizeof(GHNode *));
    if (!new_buckets) return false;
    gh__mem_zero(new_buckets, new_count * sizeof(GHNode *));

    for (i = 0; i < map->bucket_count; i++) {
        GHNode *node = map->buckets[i];
        while (node) {
            GHNode *next = node->next;
            usize   idx  = (usize)node->hash & (new_count - 1);
            node->next       = new_buckets[idx];
            new_buckets[idx] = node;
            node             = next;
        }
    }

    map->buckets      = new_buckets;
    map->bucket_count = new_count;
    return true;
}

// NOTE(Giovanni): Public functions.

void gh_arena_init(GHArena *arena, void *memory, usize size)
{
    arena->base = (u8 *)memory;
    arena->cap  = memory ? size : 0;
    arena->used = 0;
}

b32 gh_init(GHMap *map, GHArena *arena, usize initial_capacity)
{
    usize n = GH_MIN_BUCKETS;

    if (!map || !arena) return false;

    while (n < initial_capacity) n <<= 1;

    map->arena = arena;
    map->count = 0;

    map->buckets = (GHNode **)gh__arena_alloc(arena, n * sizeof(GHNode *));
    if (!map->buckets) {
        map->bucket_count = 0;
        return false;
    }
    gh__mem_zero(map->buckets, n * sizeof(GHNode *));
    map->bucket_count = n;
    return true;
}

b32 gh_put(GHMap *map, const char *key, void *value)
{
    u64     hash;
    GHNode *node;
    usize   len;
    usize   idx;

    if (!map || !map->buckets || !map->arena || !key) return false;

    hash = gh__hash(key);
    node = gh__find(map, key, hash);
    if (node) {
        node->value = value;
        return true;
    }

    if ((map->count + 1) * GH_LOAD_DEN > map->bucket_count * GH_LOAD_NUM) {
        if (!gh__grow(map)) return false;
    }

    len  = gh__str_len(key);
    node = (GHNode *)gh__arena_alloc(map->arena, sizeof(GHNode) + len + 1);
    if (!node) return false;

    node->value = value;
    node->hash  = hash;
    gh__mem_copy(node->key, key, len + 1);

    idx        = (usize)hash & (map->bucket_count - 1);
    node->next = map->buckets[idx];
    map->buckets[idx] = node;
    map->count++;
    return true;
}

void *gh_get(const GHMap *map, const char *key)
{
    GHNode *node;
    if (!map || !map->buckets || !key) return (void *)0;
    node = gh__find(map, key, gh__hash(key));
    return node ? node->value : (void *)0;
}

b32 gh_has(const GHMap *map, const char *key)
{
    if (!map || !map->buckets || !key) return false;
    return gh__find(map, key, gh__hash(key)) != (GHNode *)0;
}

b32 gh_remove(GHMap *map, const char *key)
{
    u64      hash;
    usize    idx;
    GHNode **link;

    if (!map || !map->buckets || !key) return false;

    hash = gh__hash(key);
    idx  = (usize)hash & (map->bucket_count - 1);
    link = &map->buckets[idx];

    while (*link) {
        GHNode *node = *link;
        if (node->hash == hash && gh__str_eq(node->key, key)) {
            *link = node->next;
            map->count--;
            return true;
        }
        link = &node->next;
    }
    return false;
}

usize gh_count(const GHMap *map)
{
    return map ? map->count : 0;
}

void gh_iter_init(GHIter *it, const GHMap *map)
{
    it->map    = map;
    it->bucket = 0;
    it->node   = (GHNode *)0;
    it->key    = (const char *)0;
    it->value  = (void *)0;
}

b32 gh_iter_next(GHIter *it)
{
    const GHMap *map = it->map;

    if (!map || !map->buckets) return false;

    if (it->node) it->node = it->node->next;

    while (!it->node) {
        if (it->bucket >= map->bucket_count) {
            it->key   = (const char *)0;
            it->value = (void *)0;
            return false;
        }
        it->node = map->buckets[it->bucket++];
    }

    it->key   = it->node->key;
    it->value = it->node->value;
    return true;
}

#endif // GHASH_IMPLEMENTATION
