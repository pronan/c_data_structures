#include "xlib.h"

/* Object used as dummy key to fill deleted entries */
static DummyStruct _dummy_struct;
#define dummy (&_dummy_struct)

static size_t
default_keyhash(void *_key) {
    char *key = (char *)_key;
    size_t hash = 5381;
    for (; *key; key++)
        hash = ((hash << 5) + hash) + *key; /* hash * 33 + c */
    return hash;
}

static int
default_keycmp(void *key1, void *key2) {
    return strcmp((char *)key1, (char *)key2);
}

static void *
default_keydup(void *key) {
    return (void *)strdup((char *)key);
}

static SetEntry *
set_search(SetObject *so, void *key, size_t hash) {
    size_t i;
    size_t perturb;
    SetEntry *freeslot;
    size_t mask = so->mask;
    SetEntry *ep0 = so->table;
    SetEntry *ep;
    i = (size_t)hash & mask;
    ep = &ep0[i];
    if (ep->key == NULL || ep->key == key)
        return ep;
    if (ep->key == dummy)
        freeslot = ep;
    else if (ep->hash == hash
             && so->keycmp(ep->key, key) == 0)
        return ep;
    else
        freeslot = NULL;
    for (perturb = hash;; perturb >>= PERTURB_SHIFT) {
        i = (i << 2) + i + perturb + 1;
        ep = &ep0[i & mask];
        if (ep->key == NULL)
            return freeslot == NULL ? ep : freeslot;
        if (ep->key == key || (ep->hash == hash
                               && ep->key != dummy
                               && so->keycmp(ep->key, key) == 0))
            return ep;
        if (ep->key == dummy && freeslot == NULL)
            freeslot = ep;
    }
    assert(0);          /* NOT REACHED */
    return NULL;
}

static SetEntry *
set_search_nodummy(SetObject *so, void *key, size_t hash) {
    size_t i;
    size_t perturb;
    size_t mask = so->mask;
    SetEntry *ep0 = so->table;
    SetEntry *ep;
    i = (size_t)hash & mask;
    ep = &ep0[i];
    if (ep->key == NULL
            || ep->key == key
            || (ep->hash == hash && so->keycmp(ep->key, key) == 0))
        return ep;
    for (perturb = hash;; perturb >>= PERTURB_SHIFT) {
        i = (i << 2) + i + perturb + 1;
        ep = &ep0[i & mask];
        if (ep->key == NULL
                || ep->key == key
                || (ep->hash == hash && so->keycmp(ep->key, key) == 0))
            return ep;
    }
    assert(0);          /* NOT REACHED */
    return EXIT_SUCCESS;
}

/* try to insert key's copy to so */
static int
set_insert(SetObject *so, void *key, size_t hash) {
    SetEntry *ep = set_search(so, key, hash);
    if (ep->key == NULL) {
        if ((ep->key = so->keydup(key)) == NULL)
            return EXIT_FAILURE;
        so->fill++;
        so->used++;
        ep->hash = hash;
    } else if (ep->key == dummy) {
        if ((ep->key = so->keydup(key)) == NULL)
            return EXIT_FAILURE;
        so->used++;
        ep->hash = hash;
    }/* else already key exists, do nothing */
    return EXIT_SUCCESS;
}

/*
intern fast function to insert key's reference to so
when no dummy or equal key exists.
*/
static void
set_insert_clean(SetObject *so, void *key, size_t hash) {
    size_t i;
    size_t perturb;
    size_t mask = so->mask;
    SetEntry *ep0 = so->table;
    SetEntry *ep;
    i = (size_t)hash & mask;
    ep = &ep0[i];
    for (perturb = hash; ep->key != NULL; perturb >>= PERTURB_SHIFT) {
        i = (i << 2) + i + perturb + 1;
        ep = &ep0[i & mask];
    }
    so->fill++;
    so->used++;
    ep->key = key;
    ep->hash = hash;
}

static int
set_resize(SetObject *so, size_t minused) {
    size_t newsize;
    SetEntry *oldtable, *newtable, *ep;
    SetEntry small_copy[HASH_MINSIZE];
    /* Find the smallest table size > minused. */
    for (newsize = HASH_MINSIZE;
            newsize <= minused && newsize > 0;
            newsize <<= 1)
        ;
    /* Get space for a new table. */
    oldtable = so->table;
    size_t is_oldtable_malloced = (oldtable != so->smalltable);
    if (newsize == HASH_MINSIZE) {
        /* A large table is shrinking, or we can't get any smaller. */
        newtable = so->smalltable;
        if (newtable == oldtable) {
            if (so->fill == so->used) {
                /* No dummies, so no point doing anything. */
                return EXIT_SUCCESS;
            }
#ifdef X_DEBUG
            assert(so->fill > so->used);
#endif
            memcpy(small_copy, oldtable, sizeof(small_copy));
            oldtable = small_copy;
        }
    } else {
        newtable = (SetEntry*)malloc(sizeof(SetEntry) * newsize);
        if (newtable == NULL)
            return EXIT_FAILURE;
    }
#ifdef X_DEBUG
    assert(newtable != oldtable);
#endif
    memset(newtable, 0, sizeof(SetEntry)* newsize);
    so->table = newtable;
    so->mask = newsize - 1;
    size_t used = so->used;
    so->used = 0;
    so->fill = 0;
    for (ep = oldtable; used > 0; ep++) {
        if (ep->key && ep->key != dummy) {           /* active key */
            used--;
            set_insert_clean(so, ep->key, ep->hash);
        }
#ifdef X_DEBUG
        else if (ep->key) {    /* dummy key */
            assert(ep->key == dummy);
            fprintf(stderr, "set_resize -> dummy key[%u].\n",
                    ep->hash);
        }
#endif
    }
    if (is_oldtable_malloced)
        free(oldtable);
    return EXIT_SUCCESS;
}

SetObject *
set_cnew(size_t size,
         size_t (*keyhash)(void *key),
         int (*keycmp)(void *key1, void *key2),
         void * (*keydup)(void *key),
         void (*keyfree)(void *key)) {
    SetObject *so = (SetObject *)malloc(sizeof(SetObject));
    if (so == NULL)
        return NULL;
    size_t newsize;
    for (newsize = HASH_MINSIZE;
            newsize < size && newsize > 0;
            newsize <<= 1)
        ;
    if (newsize > HASH_MINSIZE) {
        SetEntry *newtable = (SetEntry*)malloc(sizeof(SetEntry) * newsize);
        if (newtable == NULL)
            return NULL;
        memset(newtable, 0, sizeof(SetEntry)* newsize);
        so->table = newtable;
        so->mask = newsize - 1;
        so->fill = so->used = 0;
    } else {
        EMPTY_TO_MINSIZE(so);
    }
    so->keyhash = keyhash ? keyhash : default_keyhash;
    so->keycmp = keycmp ? keycmp : default_keycmp;
    so->keydup = keydup ? keydup : default_keydup;
    so->keyfree = keyfree ? keyfree : free;
    return so;
}

SetObject *
set_new(void) {
    SetObject *so = (SetObject *)malloc(sizeof(SetObject));
    if (so == NULL)
        return NULL;
    EMPTY_TO_MINSIZE(so);
    so->keyhash = default_keyhash;
    so->keycmp = default_keycmp;
    so->keydup = default_keydup;
    so->keyfree = free;
    return so;
}

void
set_clear(SetObject *so) {
    SetEntry *ep, *table = so->table;
#ifdef X_DEBUG
    assert(table != NULL);
#endif
    SetEntry small_copy[HASH_MINSIZE];
    size_t table_is_malloced = (table != so->smalltable);
    size_t fill = so->fill;
    size_t used = so->used;
    if (table_is_malloced)
        EMPTY_TO_MINSIZE(so);
    else if (fill > 0) {
        /* It's a small table with something that needs to be cleared. */
        memcpy(small_copy, table, sizeof(small_copy));
        table = small_copy;
        EMPTY_TO_MINSIZE(so);
    } else /* else it's a small table that's already empty */
        return;
    for (ep = table; used > 0; ep++) {
        /*only free active key, this is different from thon 2.7*/
        if (ep->key && ep->key != dummy) {
            used--;
#ifdef X_DEBUG
            assert(ep->key != dummy);
#endif
            so->keyfree(ep->key);
        }
#ifdef X_DEBUG
        else if (ep->key) {    /* dummy key */
            assert(ep->key == dummy);
            fprintf(stderr, "set_clear -> dummy key[%u].\n", ep->hash);
        }
#endif
    }
    if (table_is_malloced)
        free(table);
}

void
set_free(SetObject *so) {
    set_clear(so);
    free(so);
}

/* make a fresh copy of so, filtering dummy keys by the way.*/
SetObject *
set_copy(SetObject *so) {
    SetObject *copy = SET_COPY_INIT(so);
    if (copy == NULL)
        return NULL;
    SetEntry *ep;
    void *key;
    size_t used = so->used;
    for (ep = so->table; used > 0; ep++) {
        if (ep->key && ep->key != dummy) {           /* active key */
            used--;
            if ((key = copy->keydup(ep->key)) == NULL) {
                set_free(copy);
                return NULL;
            }
            set_insert_clean(copy, key, ep->hash);
        }
#ifdef X_DEBUG
        else if (ep->key) {    /* dummy key */
            assert(ep->key == dummy);
            fprintf(stderr, "set_copy -> dummy key[%u].\n", ep->hash);
        }
#endif
    }
    return copy;
}

int
set_add(SetObject *so, void *key) {
    assert(key);
    size_t hash = so->keyhash(key);
    if (set_insert(so, key, hash) == EXIT_FAILURE)
        return EXIT_FAILURE;
    if (NEED_RESIZE(so))
        return set_resize(so, RESIZE_NUM(so));
    return EXIT_SUCCESS;
}

int
set_radd(SetObject *so, void *key) {
    assert(key);
    size_t hash = so->keyhash(key);
    SetEntry *ep = set_search(so, key, hash);
    if (ep->key == NULL) {
        ep->key = key;
        so->fill++;
        so->used++;
        ep->hash = hash;
    } else if (ep->key == dummy) {
        ep->key = key;
        so->used++;
        ep->hash = hash;
    }
    if (NEED_RESIZE(so))
        return set_resize(so, RESIZE_NUM(so));
    return EXIT_SUCCESS;
}

void
set_del(SetObject *so, void *key) {
    assert(key);
    size_t hash = so->keyhash(key);
    SetEntry *ep = set_search(so, key, hash);
    /*only for existing keys*/
    assert(ep->key && ep->key != dummy);
    so->keyfree(ep->key);
    ep->key = dummy;
    so->used--;
}

/*silent version of set_del*/
void
set_discard(SetObject *so, void *key) {
    assert(key);
    size_t hash = so->keyhash(key);
    SetEntry *ep = set_search(so, key, hash);
    if (ep->key == NULL || ep->key == dummy)
        return;
    so->keyfree(ep->key);
    ep->key = dummy;
    so->used--;
}

/*from other to so, add keys in other but not in so */
int
set_update(SetObject *so, SetObject *other) {
    size_t o_used = other->used;
    if (so == other || o_used == 0)
        return EXIT_SUCCESS;
    /*if no keys in so, keys in other should always be added*/
    size_t fast_add = so->used == 0;
    /* Do one big resize at the start, rather than
     * incrementally resizing as we insert new items.  Expect
     * that there will be no (or few) overlapping keys.
     */
    if ((so->fill + o_used) * 3 >= (so->mask + 1) * 2) {
        if (set_resize(so, (so->used + o_used) * 2) != EXIT_SUCCESS)
            return EXIT_FAILURE;
    }
    SetEntry *ep;
    void *key;
    /* now walk other's keys */
    for (ep = other->table; o_used > 0; ep++) {
        key = ep->key;
        if (key && key != dummy) {           /* key in other */
            o_used--;
            if (fast_add || !set_has(so, key))  /* but key not in so */
                if (set_insert(so, key, ep->hash) == EXIT_FAILURE)
                    return EXIT_FAILURE;
        }
#ifdef X_DEBUG
        else if (key) {    /* dummy key */
            assert(key == dummy);
            fprintf(stderr, "set_update -> dummy key[%u].\n", ep->hash);
        }
#endif
    }
    return EXIT_SUCCESS;
}

/*return a new allocated set that is the union of so and other*/
SetObject *
set_union(SetObject *so, SetObject *other) {
    SetObject *big = BIGGER(so, other);
    SetObject *sml = SMALLER(so, other);
    SetObject *result = set_copy(big); /* choose the bigger one as the start set*/
    if (result == NULL)
        return NULL;
    size_t s_used = sml->used;
    if (big == sml || s_used == 0)
        return result;
    if ((result->fill + s_used) * 3 >= (result->mask + 1) * 2) {
        if (set_resize(result, (result->used + s_used) * 2) != EXIT_SUCCESS) {
            set_free(result);
            return NULL;
        }
    }
    SetEntry *ep;
    void *key;
    /* now walk the smaller one's keys */
    for (ep = sml->table; s_used > 0; ep++) {
        if (ep->key && ep->key != dummy) {           /* key in sml */
            s_used--;
            if (!set_has(result, ep->key)) {  /* but key not in result */
                if ((key = sml->keydup(ep->key)) == NULL) {
                    set_free(result);
                    return NULL;
                }
                /* there's no dummy key in result, so use this fast way */
                set_insert_clean(result, key, ep->hash);
            }
        }
#ifdef X_DEBUG
        else if (ep->key) {    /* dummy key */
            assert(ep->key == dummy);
            fprintf(stderr, "set_union -> dummy key[%u].\n", ep->hash);
        }
#endif
    }
    return result;
}

/*delete so's keys that is not in other */
int
set_iand(SetObject *so, SetObject *other) {
    if (so == other)
        return EXIT_SUCCESS;
    size_t o_used = other->used;
    if (o_used == 0) {
        set_clear(so);
        return EXIT_SUCCESS;
    }
    size_t used = so->used;
    SetEntry *ep;
    void *key;
    size_t fast_del = 0;
    /*walk keys in so*/
    for (ep = so->table; used > 0; ep++) {
        key = ep->key;
        if (key && key != dummy) {           /* key in so */
            used--;
            if (fast_del || !set_has(other, key))  /* but key not in other */
                set_del(so, key);
            else if (--o_used == 0)
                /*now, the rest of so's keys can delete directly*/
                fast_del = 1;
        }
#ifdef X_DEBUG
        else if (key) {    /* dummy key */
            assert(key == dummy);
            fprintf(stderr, "set_iand -> dummy key[%u].\n", ep->hash);
        }
#endif
    }
    return EXIT_SUCCESS;
}

/*return the intersection copy of so and other*/
SetObject *
set_and(SetObject *so, SetObject *other) {
    if (so == other)
        return set_copy(so);
    SetObject *big = BIGGER(so, other);
    SetObject *sml = SMALLER(so, other);
    SetObject *result = SET_COPY_INIT(sml);
    if (result == NULL)
        return NULL;
    size_t s_used = sml->used;
    SetEntry *ep;
    void *key;
    /*walk smaller one is better*/
    for (ep = sml->table; s_used > 0; ep++) {
        if (ep->key && ep->key != dummy) {           /* key in sml */
            s_used--;
            if (set_has(big, ep->key)) {    /* key also in big, insert */
                if ((key = sml->keydup(ep->key)) == NULL) {
                    set_free(result);
                    return NULL;
                }
                /* there's no dummy key in result, so use this fast way */
                set_insert_clean(result, key, ep->hash);
            }
        }
#ifdef X_DEBUG
        else if (ep->key) {    /* dummy key */
            assert(ep->key == dummy);
            fprintf(stderr, "set_and -> dummy key[%u].\n", ep->hash);
        }
#endif
    }
    return result;
}

/*delete so's keys that is in other*/
int
set_isub(SetObject *so, SetObject *other) {
    if (so == other) {
        set_clear(so);
        return EXIT_SUCCESS;
    }
    size_t o_used = other->used;
    if (o_used == 0)
        return EXIT_SUCCESS;
    size_t used = so->used;
    SetEntry *ep;
    void *key;
    for (ep = so->table; used > 0; ep++) {
        key = ep->key;
        if (key && key != dummy) {           /* key in so */
            used--;
            if (set_has(other, key)) { /* key also in other */
                set_del(so, key);
                if(--o_used == 0)
                    /*all other's keys are checked, no need to go further*/
                    break;
            }
        }
#ifdef X_DEBUG
        else if (key) {    /* dummy key */
            assert(key == dummy);
            fprintf(stderr, "set_isub -> dummy key[%u].\n", ep->hash);
        }
#endif
    }
    return EXIT_SUCCESS;
}

/*return the difference set of so - other*/
SetObject *
set_sub(SetObject *so, SetObject *other) {
    if (so == other)
        return set_cnew(HASH_MINSIZE, so->keyhash, so->keycmp, so->keydup,
                        so->keyfree);
    size_t o_used = other->used;
    if (o_used == 0)
        return set_copy(so);
    SetObject *result = SET_COPY_INIT(so);
    if (result == NULL)
        return NULL;
    size_t used = so->used;
    SetEntry *ep;
    void *key;
    size_t fast_add = 0;
    for (ep = so->table; used > 0; ep++) {
        if (ep->key && ep->key != dummy) {           /* key in so */
            used--;
            if (fast_add || !set_has(other, ep->key)) {    /* but key not in other */
                if ((key = so->keydup(ep->key)) == NULL) {
                    set_free(result);
                    return NULL;
                }
                /* there's no dummy key in result, so use this faster way */
                set_insert_clean(result, key, ep->hash);
            } else if (--o_used == 0)
                /*now, the rest of so's keys can add directly*/
                fast_add = 1;
        }
#ifdef X_DEBUG
        else if (ep->key) {    /* dummy key */
            assert(ep->key == dummy);
            fprintf(stderr, "set_sub -> dummy key[%u].\n", ep->hash);
        }
#endif
    }
    return result;
}

/*delete so's keys that is in other,
add other's keys that is not in so to so*/
int
set_ixor(SetObject *so, SetObject *other) {
    if (so == other ) {
        set_clear(so);
        return EXIT_SUCCESS;
    }
    size_t o_used = other->used;
    if ( o_used == 0)
        return EXIT_SUCCESS;
    size_t used = so->used;
    if ((so->fill + o_used) * 3 >= (so->mask + 1) * 2) {
        if (set_resize(so, (used + o_used) * 2) != EXIT_SUCCESS)
            return EXIT_FAILURE;
    }
    SetEntry *ep;
    void *key;
    size_t fast_add = used == 0;
    /*must walk other's keys*/
    for (ep = other->table; o_used > 0; ep++) {
        key = ep->key;
        if (key && key != dummy) {           /* key in other */
            o_used--;
            if (fast_add || !set_has(so, key)) { /* but key not in so */
                if (set_insert(so, key, ep->hash) == EXIT_FAILURE)
                    return EXIT_FAILURE;
            } else {
                set_del(so, key);
                if (--used==0)
                    fast_add=1;
            }
        }
#ifdef X_DEBUG
        else if (key) {    /* dummy key */
            assert(key == dummy);
            fprintf(stderr, "set_ixor -> dummy key[%u].\n",ep->hash);
        }
#endif
    }
    return EXIT_SUCCESS;
}

/*return a new allocated set that is the symmetric difference of
so and other. It seems a little complicated to write a similar version
like above function (set_and, set_sub, set_union), that is:
uses a blank set as initial, and result contains no dummy keys. So here
I reuses set_ixor.*/
SetObject *
set_xor(SetObject *so, SetObject *other) {
    if (so == other)
        return set_cnew(HASH_MINSIZE, so->keyhash, so->keycmp, so->keydup,
                        so->keyfree);
    SetObject *big = BIGGER(so, other);
    SetObject *sml = SMALLER(so, other);
    SetObject *result = set_copy(big);
    if (result == NULL)
        return NULL;
    if (set_ixor(result, sml) == EXIT_FAILURE)
        return NULL;
    return result;
}

size_t
set_len(SetObject *so) {
    return so->used;
}

size_t
set_has(SetObject *so, void *key) {
    assert(key);
    SetEntry *ep = set_search(so, key, so->keyhash(key));
    return ep->key && ep->key != dummy;
}

/* is so a subset of other? */
size_t
set_issubset(SetObject *so, SetObject *other){
    if (so==other)
        return 1;
    SetEntry *ep;
    size_t used = so->used;
    for (ep = so->table; used > 0; ep++) {
        if (ep->key  && ep->key != dummy){
            used--;
            if (!set_has(other,ep->key))
                return 0;
        }
    }
    return 1;
}

/* is so a superset of other? */
size_t
set_issuperset(SetObject *so, SetObject *other){
    if (so==other)
        return 1;
    SetEntry *ep;
    size_t used = other->used;
    for (ep = other->table; used > 0; ep++) {
        if (ep->key  && ep->key != dummy){
            used--;
            if (!set_has(so,ep->key))
                return 0;
        }
    }
    return 1;
}

int
set_ior(SetObject *so, SetObject *other) {
    return set_update(so, other);
}

SetObject *
set_or(SetObject *so, SetObject *other) {
    return set_union(so, other);
}

int
set_addfrom(SetObject *so, void **keylist, size_t size) {
    size_t i = 0;
    for (; i < size; i++) {
        set_add(so, (char*)keylist[i]);
    }
}

void set_print(SetObject *so) {
    SetEntry *ep;
    void *key;
    size_t used = so->used;
    printf("base: member:%u, used:%u, del:%u\n{ ", so->mask + 1, so->used,
           so->fill - so->used);
    for(ep = so->table; used > 0; ep++) {
        key = ep->key;
        if (key && key != dummy) {
            used--;
            printf("'%s', ", (char*)key);
        }
    }
    printf("}\n\n");
}
