/* m3_core.c - Module 3 core data structures in PURE C. See m3_core.h for the API.
 *
 * Why each structure exists (what problem it solves, and what was rejected):
 *
 *  Hash table + separate chaining
 *      "Find artifact #4471" must not scan every record. Average O(1) per lookup;
 *      worst case O(n) if every key lands in one bucket. Chaining never "fills up"
 *      (open addressing clusters and must stay well below full).
 *  djb2 hash  h = h*33 + c
 *      Spreads near-identical IDs (P001, P002) over different buckets. Summing the
 *      characters would collide on anagrams such as AB1 / BA1.
 *  Rehash to 2n+1 buckets when load factor > 0.75
 *      Keeps chains short as data grows: O(n) per rehash, amortised O(1) per insert.
 *  Merge sort
 *      Always O(n log n) and STABLE (ties keep their original order), at the cost of an
 *      O(n) buffer. Quicksort is not stable and is O(n^2) in the worst case.
 *  Graph as adjacency list
 *      O(V + E) memory; an adjacency matrix would be O(V^2) and mostly empty.
 *  BFS finds the fewest transfers (every hop costs the same). DFS lists everything
 *      reachable and, repeated from unseen sites, counts clusters.
 */
#include "m3_core.h"

#include <stdlib.h>
#include <string.h>

/* ===================== 1. Hash table ===================== */
#define HI_INITIAL_BUCKETS 17
#define HI_MAX_LOAD        0.75

static size_t djb2(const char* s)
{
    size_t h = 5381;
    const unsigned char* p = (const unsigned char*)s;
    while (*p) h = h * 33 + *p++;       /* unsigned overflow wraps, which is fine for hashing */
    return h;
}

int hi_init(HashIndex* h)
{
    h->buckets = (HashNode**)calloc(HI_INITIAL_BUCKETS, sizeof(HashNode*));
    h->bucketCount = h->buckets ? HI_INITIAL_BUCKETS : 0;
    h->count = 0;
    h->rehashes = 0;
    return h->buckets ? 0 : -1;
}

void hi_free(HashIndex* h)
{
    size_t i;
    if (h->buckets == NULL) return;
    for (i = 0; i < h->bucketCount; i++) {
        HashNode* n = h->buckets[i];
        while (n != NULL) {
            HashNode* next = n->next;
            free(n->key);
            free(n);
            n = next;
        }
    }
    free(h->buckets);
    h->buckets = NULL;
    h->bucketCount = 0;
    h->count = 0;
}

double hi_loadFactor(const HashIndex* h)
{
    return h->bucketCount ? (double)h->count / (double)h->bucketCount : 0.0;
}

size_t hi_longestChain(const HashIndex* h)
{
    size_t i, longest = 0;
    for (i = 0; i < h->bucketCount; i++) {
        size_t len = 0;
        const HashNode* n;
        for (n = h->buckets[i]; n != NULL; n = n->next) len++;
        if (len > longest) longest = len;
    }
    return longest;
}

/* Grow to 2n+1 buckets and re-link every node: a node's bucket depends on the bucket
 * count, so all of them must be recomputed. Odd sizes make the modulo use the whole
 * hash value, not only its lowest bits. If memory runs out we simply keep the old
 * (longer-chained but still correct) table. */
static void rehash(HashIndex* h)
{
    size_t newCount = h->bucketCount * 2 + 1, i;
    HashNode** bigger = (HashNode**)calloc(newCount, sizeof(HashNode*));
    if (bigger == NULL) return;

    for (i = 0; i < h->bucketCount; i++) {
        HashNode* n = h->buckets[i];
        while (n != NULL) {
            HashNode* next = n->next;
            size_t b = djb2(n->key) % newCount;
            n->next = bigger[b];
            bigger[b] = n;
            n = next;
        }
    }
    free(h->buckets);
    h->buckets = bigger;
    h->bucketCount = newCount;
    h->rehashes++;
}

int hi_insert(HashIndex* h, const char* key, const void* item)
{
    size_t b = djb2(key) % h->bucketCount;
    HashNode* n;
    size_t len;

    for (n = h->buckets[b]; n != NULL; n = n->next)
        if (strcmp(n->key, key) == 0) return HI_DUPLICATE;

    n = (HashNode*)malloc(sizeof(HashNode));
    if (n == NULL) return HI_NOMEM;
    len = strlen(key) + 1;
    n->key = (char*)malloc(len);
    if (n->key == NULL) { free(n); return HI_NOMEM; }
    memcpy(n->key, key, len);
    n->item = item;
    n->next = h->buckets[b];            /* push at the front of the chain: O(1) */
    h->buckets[b] = n;
    h->count++;

    if (hi_loadFactor(h) > HI_MAX_LOAD) rehash(h);
    return HI_OK;
}

const void* hi_find(const HashIndex* h, const char* key)
{
    const HashNode* n;
    if (h->bucketCount == 0) return NULL;
    for (n = h->buckets[djb2(key) % h->bucketCount]; n != NULL; n = n->next)
        if (strcmp(n->key, key) == 0) return n->item;
    return NULL;
}

/* ===================== 2. Stable merge sort ===================== */
static void sortRange(char* base, char* buf, size_t lo, size_t hi, size_t sz,
                      CompareFn cmp, void* ctx)
{
    size_t mid, i, j, k;
    if (hi - lo < 2) return;            /* 0 or 1 element: already sorted */
    mid = lo + (hi - lo) / 2;
    sortRange(base, buf, lo, mid, sz, cmp, ctx);
    sortRange(base, buf, mid, hi, sz, cmp, ctx);

    i = lo; j = mid; k = lo;
    while (i < mid && j < hi) {
        /* Take from the right half only when it is strictly smaller. On a tie the left
         * element (the one that came first) goes first: that is what makes it stable. */
        if (cmp(base + j * sz, base + i * sz, ctx) < 0) memcpy(buf + k++ * sz, base + j++ * sz, sz);
        else                                            memcpy(buf + k++ * sz, base + i++ * sz, sz);
    }
    while (i < mid) memcpy(buf + k++ * sz, base + i++ * sz, sz);
    while (j < hi)  memcpy(buf + k++ * sz, base + j++ * sz, sz);
    memcpy(base + lo * sz, buf + lo * sz, (hi - lo) * sz);
}

int merge_sort(void* base, size_t n, size_t elemSize, CompareFn cmp, void* ctx)
{
    char* buf;
    if (n < 2) return 0;
    buf = (char*)malloc(n * elemSize);
    if (buf == NULL) return -1;
    sortRange((char*)base, buf, 0, n, elemSize, cmp, ctx);
    free(buf);
    return 0;
}

/* ===================== 3. Graph ===================== */
Graph* graph_new(void)
{
    Graph* g = (Graph*)calloc(1, sizeof(Graph));
    if (g == NULL) return NULL;
    if (hi_init(&g->byName) != 0) { free(g); return NULL; }
    return g;
}

void graph_free(Graph* g)
{
    int i;
    if (g == NULL) return;
    for (i = 0; i < g->siteCount; i++) {
        free(g->sites[i]->name);
        free(g->sites[i]->adj);
        free(g->sites[i]);
    }
    free(g->sites);
    hi_free(&g->byName);
    free(g);
}

int graph_find(const Graph* g, const char* name)
{
    const Site* s = (const Site*)hi_find(&g->byName, name);
    return s ? s->index : -1;
}

int graph_addSite(Graph* g, const char* name)
{
    int existing = graph_find(g, name);
    Site* s;
    size_t len;

    if (existing >= 0) return existing;

    if (g->siteCount == g->capacity) {
        int newCap = g->capacity ? g->capacity * 2 : 8;
        Site** bigger = (Site**)realloc(g->sites, (size_t)newCap * sizeof(Site*));
        if (bigger == NULL) return -1;
        g->sites = bigger;
        g->capacity = newCap;
    }
    s = (Site*)calloc(1, sizeof(Site));
    if (s == NULL) return -1;
    len = strlen(name) + 1;
    s->name = (char*)malloc(len);
    if (s->name == NULL) { free(s); return -1; }
    memcpy(s->name, name, len);
    s->index = g->siteCount;

    if (hi_insert(&g->byName, s->name, s) != HI_OK) { free(s->name); free(s); return -1; }
    g->sites[g->siteCount++] = s;
    return s->index;
}

int graph_hasEdge(const Graph* g, int a, int b)
{
    int k;
    for (k = 0; k < g->sites[a]->degree; k++)
        if (g->sites[a]->adj[k] == b) return 1;
    return 0;
}

static int addNeighbor(Site* s, int other)
{
    if (s->degree == s->capacity) {
        int newCap = s->capacity ? s->capacity * 2 : 4;
        int* bigger = (int*)realloc(s->adj, (size_t)newCap * sizeof(int));
        if (bigger == NULL) return 0;
        s->adj = bigger;
        s->capacity = newCap;
    }
    s->adj[s->degree++] = other;
    return 1;
}

int graph_connect(Graph* g, const char* a, const char* b)
{
    int ia, ib;
    if (strcmp(a, b) == 0) return 0;                 /* self-loop */
    ia = graph_addSite(g, a);
    ib = graph_addSite(g, b);
    if (ia < 0 || ib < 0) return -1;
    if (graph_hasEdge(g, ia, ib)) return 0;          /* already connected */
    if (!addNeighbor(g->sites[ia], ib)) return -1;
    if (!addNeighbor(g->sites[ib], ia)) { g->sites[ia]->degree--; return -1; }
    return 1;
}

int         graph_siteCount(const Graph* g)          { return g->siteCount; }
const char* graph_siteName(const Graph* g, int i)    { return g->sites[i]->name; }
int         graph_degree(const Graph* g, int i)      { return g->sites[i]->degree; }
int         graph_neighbor(const Graph* g, int i, int k) { return g->sites[i]->adj[k]; }

/* BFS explores sites in rings of 1, 2, 3... transfers, so the first time it reaches `to`
 * is along a fewest-transfers route. parent[] records how each site was reached; the
 * queue is a plain array with a head and a tail index (each site enters it at most once). */
int graph_shortestPath(const Graph* g, int from, int to, int* outPath)
{
    int n = g->siteCount, head = 0, tail = 0, len = 0, cur, k, i;
    int* parent;
    int* queue;

    if (from < 0 || to < 0 || from >= n || to >= n) return 0;
    parent = (int*)malloc((size_t)n * sizeof(int));
    queue  = (int*)malloc((size_t)n * sizeof(int));
    if (parent == NULL || queue == NULL) { free(parent); free(queue); return -1; }
    for (i = 0; i < n; i++) parent[i] = -1;

    parent[from] = from;
    queue[tail++] = from;
    while (head < tail) {
        cur = queue[head++];
        if (cur == to) break;
        for (k = 0; k < g->sites[cur]->degree; k++) {
            int next = g->sites[cur]->adj[k];
            if (parent[next] != -1) continue;       /* already reached by a route no longer */
            parent[next] = cur;
            queue[tail++] = next;
        }
    }
    if (parent[to] == -1) { free(parent); free(queue); return 0; }   /* different cluster */

    /* walk back from `to` to `from`, then reverse into outPath */
    for (cur = to; cur != from; cur = parent[cur]) outPath[len++] = cur;
    outPath[len++] = from;
    for (i = 0; i < len / 2; i++) {
        int t = outPath[i];
        outPath[i] = outPath[len - 1 - i];
        outPath[len - 1 - i] = t;
    }
    free(parent);
    free(queue);
    return len;
}

/* Recursive DFS: visit a site, then each unvisited neighbour, going as deep as possible */
static void dfsVisit(const Graph* g, int site, char* seen, int* out, int* count)
{
    int k;
    seen[site] = 1;
    out[(*count)++] = site;
    for (k = 0; k < g->sites[site]->degree; k++) {
        int next = g->sites[site]->adj[k];
        if (!seen[next]) dfsVisit(g, next, seen, out, count);
    }
}

int graph_dfsOrder(const Graph* g, int start, int* out)
{
    int count = 0;
    char* seen;
    if (start < 0 || start >= g->siteCount) return 0;
    seen = (char*)calloc((size_t)g->siteCount, 1);
    if (seen == NULL) return 0;
    dfsVisit(g, start, seen, out, &count);
    free(seen);
    return count;
}

int graph_clusterCount(const Graph* g)
{
    int n = g->siteCount, i, count = 0, clusters = 0;
    char* seen = (char*)calloc((size_t)(n ? n : 1), 1);
    int* scratch = (int*)malloc((size_t)(n ? n : 1) * sizeof(int));
    if (seen == NULL || scratch == NULL) { free(seen); free(scratch); return -1; }
    for (i = 0; i < n; i++) {
        if (seen[i]) continue;
        dfsVisit(g, i, seen, scratch, &count);     /* marks the whole cluster */
        clusters++;
    }
    free(seen);
    free(scratch);
    return clusters;
}
