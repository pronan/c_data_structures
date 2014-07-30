#include "xlib.h"

static int
default_keycmp(void *key1, void *key2) {
    return *(int *)key1 - *(int *)key2;
}

static void *
default_keydup(void *_key) {
    int *key = (int*)malloc(sizeof(int));
    *key = *(int *)_key;
    return (void *)key;
}

static int
list_resize(ListObject *lp, int newsize) {
    void **items;
    size_t new_allocated;
    int allocated = lp->allocated;
    /* Bypass realloc() when a previous overallocation is large enough
       to accommodate the newsize.  If the newsize falls lower than half
       the allocated size, then proceed with the realloc() to shrink the list.
    */
    if (allocated >= newsize && newsize >= (allocated >> 1)) {
        assert(lp->table != NULL || newsize == 0);
        lp->used = newsize;
        return 0;
    }
    /* This over-allocates proportional to the list size, making room
     * for additional growth.  The over-allocation is mild, but is
     * enough to give linear-time amortized behavior over a long
     * sequence of appends() in the presence of a poorly-performing
     * system realloc().
     * The growth pattern is:  0, 4, 8, 16, 25, 35, 46, 58, 72, 88, ...
     */
    new_allocated = (newsize >> 3) + (newsize < 9 ? 3 : 6);
    /* check for integer overflow */
    if (new_allocated > SIZE_MAX - newsize) {
        return -1;
    } else {
        new_allocated += newsize;
    }
    if (newsize == 0)
        new_allocated = 0;
    items = lp->table;
    if (new_allocated <= (SIZE_MAX / sizeof(void *)))
        Mem_RESIZE(items, void *, new_allocated);
    else
        items = NULL;
    if (items == NULL) {
        return -1;
    }
    lp->table = items;
    lp->used = newsize;
    lp->allocated = new_allocated;
    return 0;
}

ListObject *
list_cnew(size_t size,
          int (*keycmp)(void *key1, void *key2),
          void *(*keydup)(void *key),
          void (*keyfree)(void *key)) {
    ListObject *lp;
    size_t nbytes;
    if (size < 0) {
        return NULL;
    }
    /* Check for overflow without an actual overflow,
     *  which can cause compiler to optimise out */
    if ((size_t)size > SIZE_MAX / sizeof(void *))
        return NULL;
    nbytes = size * sizeof(void *);
    lp = (ListObject *)malloc(sizeof(ListObject));
    if (lp == NULL)
        return NULL;
    if (size == 0)
        lp->table = NULL;
    else {
        lp->table = (void **) Mem_MALLOC(nbytes);
        if (lp->table == NULL) {
            free(lp);
            return NULL;
        }
        memset(lp->table, 0, nbytes);
    }
    lp->type = LIST;
    lp->used = size;
    lp->allocated = size;
    lp->keycmp = keycmp ? keycmp : default_keycmp;
    lp->keydup = keydup ? keydup : default_keydup;
    lp->keyfree = keyfree ? keyfree : free;
    return lp;
}

ListObject *
list_new(void) {
    ListObject *lp = Mem_NEW(ListObject, 1);
    if (lp == NULL)
        return NULL;
    lp->table = NULL;
    lp->type = LIST;
    lp->used = 0;
    lp->allocated = 0;
    lp->keycmp = default_keycmp;
    lp->keydup = default_keydup;
    lp->keyfree = free;
    return lp;
}

void
list_clear(ListObject *lp) {
    int n = lp->used;
    while (--n >= 0)
        lp->keyfree(lp->table[n]);
    free(lp->table);
    lp->used = 0;
    lp->allocated = 0;
    lp->table = NULL;
}

int
list_free(ListObject *lp) {
    list_clear(lp);
    free(lp);
    return 0;
}

ListObject *
list_copy(ListObject *lp) {
    ListObject *nlp = LIST_COPY_INIT(lp);
    if (nlp == NULL)
        return NULL;
    size_t i, n = lp->used;
    void *lpk;
    for(i = 0; i < n; i++) {
        lpk = lp->keydup(lp->table[i]);
        if (lpk == NULL) {
            list_free(nlp);
            return NULL;
        }
        nlp->table[i] = lpk;
    }
    return nlp;
}

size_t
list_len(ListObject *lp) {
    return lp->used;
}

size_t
list_is_empty(ListObject *lp) {
    return lp->used == 0;
}

void *
list_get(ListObject *lp, int index) {
    int n = lp->used;
    if (index < 0)
        index += n;
    if (index < 0 || index >= n) {
        return NULL;
    }
    return lp->table[index];
}

/* insert v's reference before where */
int
list_rinsert(ListObject *lp, int where, void *v) {
    int i, n = lp->used;
    void **items;
    if (v == NULL) {
        return -1;
    }
    if (n == SSIZE_T_MAX) {
        return -1;
    }
    if (list_resize(lp, n + 1) == -1)
        return -1;
    if (where < 0) {
        where += n;
        if (where < 0)
            where = 0;
    }
    if (where > n)
        where = n;
    items = lp->table;
    for (i = n; --i >= where; )
        items[i + 1] = items[i];
    items[where] = v;
    return 0;
}

/* insert v's copy before where */
int
list_insert(ListObject *lp, int where, void *v) {
    int n = lp->used;
    void *new_v;
    if (v == NULL) {
        return -1;
    }
    if (n == SSIZE_T_MAX) {
        return -1;
    }
    new_v = (void*)lp->keydup(v);
    if (new_v == NULL) {
        return -1;
    }
    if (list_rinsert(lp, where, new_v) == -1) {
        lp->keyfree(new_v);
        return -1;
    }
    return 0;
}

/* add nv's copy to the end of lp */
int
list_add(ListObject *lp, void *v) {
    int n = lp->used;
    if (n == SSIZE_T_MAX) {
        return -1;
    }
    assert (v != NULL);
    void *new_v = (void*)lp->keydup(v);
    if (new_v == NULL) {
        return -1;
    }
    if (list_resize(lp, n + 1) == -1) {
        lp->keyfree(new_v);
        return -1;
    }
    lp->table[n] = new_v;
    return 0;
}

/* add nv's reference to the end of lp */
int
list_radd(ListObject *lp, void *v) {
    int n = lp->used;
    if (n == SSIZE_T_MAX) {
        return -1;
    }
    assert (v != NULL);
    if (list_resize(lp, n + 1) == -1) {
        return -1;
    }
    lp->table[n] = v;
    return 0;
}

/* pop the last key out of lp */
void *
list_pop(ListObject *lp) {
    size_t n = lp->used;
    if (n == 0)
        return NULL;
    void *v = lp->table[n - 1];
    if (list_resize(lp, n - 1) == -1) {
        return NULL;
    }
    return v;
}

/* pop index th key out of lp */
void *
list_popi(ListObject *lp, int index) {
    int i, n = lp->used;
    void *v;
    if (index < 0)
        index += n;
    if (index < 0 || index >= n) {
        return NULL;
    }
    v = lp->table[index];
    for (i = index + 1; i < n; i++) {
        lp->table[i - 1] = lp->table[i];
    }
    if (list_resize(lp, n - 1) == -1) {
        return NULL;
    }
    return v;
}

int
list_del(ListObject *lp, int index) {
    void *v = list_popi(lp, index);
    if (v == NULL)
        return -1;
    lp->keyfree(v);
    return 0;
}

size_t
list_index(ListObject *lp, void *key) {
    size_t i, n = lp->used;
    void *k;
    for(i = 0; i < n; i++) {
        k = lp->table[i];
        if(k == key || lp->keycmp(key, k) == 0)
            return i;
    }
    return -1;
}

size_t
list_count(ListObject *lp, void *key) {
    size_t i, result = 0, n = lp->used;
    void *k;
    for(i = 0; i < n; i++) {
        k = lp->table[i];
        if(k == key || lp->keycmp(key, k) == 0)
            result++;
    }
    return result;
}

static int
slice_interpret(int n, int *start, int *stop, int step) {
    if (step > 0) {
        if (*start >= n) /*the result must be empty list, return directly */
            return -1;
        if (*start < 0)
            *start += n;
        if (*start < 0)
            *start = 0;
        if (*stop < 0)
            *stop += n;
        if (*stop <= 0) /*the result must be empty list, return directly */
            return -1;
        if (*stop > n)
            *stop = n;
    } else {
        if (*start >= -n && *start <= -1)
            *start += n;
        if (*start > n - 1)
            *start = n - 1;
        if (*start < -n) /*the result must be empty list, return directly */
            return -1;
        if (*stop >= -n && *stop <= -1)
            *stop += n;
        if (*stop < -n)
            *stop = -1;
        if (*stop >= n - 1 ) /*the result must be empty list, return directly */
            return -1;
    }
    return 0;
}

ListObject *
list_rsslice(ListObject *lp, int start, int stop, int step) {
    assert(step != 0);
    ListObject *nlp = list_cnew(0, lp->keycmp, lp->keydup, lp->keyfree);
    if (nlp == NULL)
        return NULL;
    size_t n = lp->used;
    if (slice_interpret(n, &start, &stop, step) == -1)
        return nlp;
    void *k;
    int factor = step > 0 ? 1 : -1;
    for(; start * factor < stop * factor; start += step) {
        k = lp->table[start];
        if (list_radd(nlp, k) == -1) {
            free(nlp); /*free nlp directly, or will infect the lp.*/
            return NULL;
        }
    }
    return nlp;
}

ListObject *
list_sslice(ListObject *lp, int start, int stop, int step) {
    assert(step != 0);
    ListObject *nlp = list_cnew(0, lp->keycmp, lp->keydup, lp->keyfree);
    if (nlp == NULL)
        return NULL;
    size_t n = lp->used;
    if (slice_interpret(n, &start, &stop, step) == -1)
        return nlp;
    void *k;
    int factor = step > 0 ? 1 : -1;
    for(; start * factor < stop * factor; start += step) {
        k = lp->table[start];
        if (list_add(nlp, k) == -1) {
            list_free(nlp);
            return NULL;
        }
    }
    return nlp;
}

ListObject *
list_rslice(ListObject *lp, int start, int stop) {
    return list_rsslice(lp, start, stop, 1);
}

ListObject *
list_slice(ListObject *lp, int start, int stop) {
    return list_sslice(lp, start, stop, 1);
}

IterObject *
list_iter_new(ListObject * lp) {
    IterObject *lio;
    lio = (IterObject*)malloc(sizeof(IterObject));
    if (lio == NULL)
        return NULL;
    lio->object = (void*)lp;
    lio->inipos = *lp->table;
    lio->rest = lp->used;
    lio->type = LIST;
    return lio;
}

size_t
list_iter_walk(IterObject * lio, void **key_addr) {
    if (lio->rest == 0)
        return 0;
    ListObject* lp = (ListObject*)lio->object;
    *key_addr = lio->inipos;
    lio->rest--;
    lio->inipos = lp->table[lp->used - lio->rest];
    return 1;
}

void
list_iter_flush(IterObject * lio) {
    ListObject *lp = (ListObject *)lio->object;
    lio->inipos = *lp->table;
    lio->rest = lp->used;
    lio->type = LIST;
}

void
list_print2(ListObject * lp) {
    size_t i, n = lp->used;
    void *k;
    for(i = 0; i < n; i++) {
        k = lp->table[i];
        printf("index %u, value is %d\n", i, *(int*)k);
    }
}

void
list_print(ListObject *lp) {
    assert(lp);
    if (lp->used == 0) {
        printf("[]\n\n");
        return;
    }
    void *key;
    IterObject *sio = iter(lp);
    printf("[");
    while(iterw(sio, &key)) {
        printf("%d, ", *(int*)key);
    }
    free(sio);
    printf("]\n\n");
}
