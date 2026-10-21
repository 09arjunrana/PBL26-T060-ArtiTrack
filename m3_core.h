/* m3_core.h - Module 3 core data structures, written in PURE C (compile with gcc)
 *
 *   1. HashIndex : hash table, separate chaining, djb2 hash, rehash at load factor > 0.75
 *   2. merge_sort: generic, stable merge sort driven by a comparator function pointer
 *   3. Graph     : undirected graph as an adjacency list, with BFS, DFS and cluster count
 *
 * Nothing here knows about the C++ Artifact class. The C++ layer (m3.cpp) wraps these
 * and turns the C return codes into the exceptions the OOP syllabus asks for.
 */
#ifndef M3_CORE_H
#define M3_CORE_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ===================== 1. Hash table ===================== */
typedef struct HashNode {
    char*            key;     /* private copy of the key string */
    const void*      item;    /* NOT owned: whoever inserted it keeps it alive */
    struct HashNode* next;    /* next node in the same bucket (chaining) */
} HashNode;

typedef struct {
    HashNode** buckets;
    size_t     bucketCount;
    size_t     count;
    int        rehashes;
} HashIndex;

enum { HI_OK = 0, HI_DUPLICATE = 1, HI_NOMEM = 2 };

int         hi_init(HashIndex* h);                 /* 0 on success, -1 if out of memory */
void        hi_free(HashIndex* h);                 /* frees every node; h must be hi_init'ed again to reuse */
int         hi_insert(HashIndex* h, const char* key, const void* item);   /* HI_OK / HI_DUPLICATE / HI_NOMEM */
const void* hi_find(const HashIndex* h, const char* key);                  /* NULL if the key is absent */
double      hi_loadFactor(const HashIndex* h);
size_t      hi_longestChain(const HashIndex* h);

/* ===================== 2. Stable merge sort ===================== */
/* cmp returns <0, 0 or >0 like strcmp. ctx is passed through untouched. */
typedef int (*CompareFn)(const void* a, const void* b, void* ctx);

/* Sorts n elements of elemSize bytes (elements must be safe to memcpy).
 * Stable: elements that compare equal keep their original order.
 * Returns 0 on success, -1 if the temporary buffer could not be allocated. */
int merge_sort(void* base, size_t n, size_t elemSize, CompareFn cmp, void* ctx);

/* ===================== 3. Graph ===================== */
typedef struct {
    char* name;
    int   index;
    int*  adj;                /* indices of neighbouring sites, in insertion order */
    int   degree;
    int   capacity;
} Site;

typedef struct {
    Site**    sites;          /* sites[i]->index == i */
    int       siteCount;
    int       capacity;
    HashIndex byName;         /* site name -> Site*  (the hash table above is reused) */
} Graph;

Graph*      graph_new(void);                                   /* NULL if out of memory */
void        graph_free(Graph* g);
int         graph_addSite(Graph* g, const char* name);         /* index of the site; creates it if new; -1 if out of memory */
int         graph_find(const Graph* g, const char* name);      /* index or -1 */
int         graph_hasEdge(const Graph* g, int a, int b);
/* Undirected edge. Creates missing sites. Returns 1 added, 0 rejected (self-loop or
 * already connected), -1 out of memory. */
int         graph_connect(Graph* g, const char* a, const char* b);
int         graph_siteCount(const Graph* g);
const char* graph_siteName(const Graph* g, int i);
int         graph_degree(const Graph* g, int i);
int         graph_neighbor(const Graph* g, int i, int k);

/* BFS: fewest-transfers route. Fills outPath (room for siteCount ints) with site indices
 * from..to and returns how many; returns 0 if there is no route; -1 on out of memory. */
int         graph_shortestPath(const Graph* g, int from, int to, int* outPath);
/* DFS (recursive): every site reachable from start, in visiting order. Returns the count. */
int         graph_dfsOrder(const Graph* g, int start, int* out);
/* Number of separate clusters (connected components); -1 on out of memory. */
int         graph_clusterCount(const Graph* g);

#ifdef __cplusplus
}
#endif

#endif /* M3_CORE_H */
