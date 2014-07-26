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

static void *
default_valuedup(void *_value) {
    size_t *value = (size_t*)malloc(sizeof(size_t));
    *value = *(size_t *)_value;
    return (void *)value;
}

static void *
default_valuedefault(void) {
    size_t *value = (size_t*)malloc(sizeof(size_t));
    *value = 0;
    return (void *)value;
}

/*intern basic search method, used by other fucntions*/
static DictEntry *
dict_search(DictObject *mp, void *key, size_t hash) {
    size_t i;
    size_t perturb;
    DictEntry *freeslot;
    size_t mask = mp->mask;
    DictEntry *ep0 = mp->table;
    DictEntry *ep;
    i = (size_t)hash & mask;
    ep = &ep0[i];
    if (ep->key == NULL || ep->key == key)
        return ep;
    if (ep->key == dummy)
        freeslot = ep;
    else if (ep->hash == hash
             && mp->keycmp(ep->key, key) == 0)
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
                               && mp->keycmp(ep->key, key) == 0))
            return ep;
        if (ep->key == dummy && freeslot == NULL)
            freeslot = ep;
    }
    assert(0);          /* NOT REACHED */
    return NULL;
}

/*faster method used when no dummy key exists in table*/
static DictEntry *
dict_search_nodummy(DictObject *mp, void *key, size_t hash) {
    size_t i;
    size_t perturb;
    size_t mask = mp->mask;
    DictEntry *ep0 = mp->table;
    DictEntry *ep;
    i = (size_t)hash & mask;
    ep = &ep0[i];
    if (ep->key == NULL
        || ep->key == key
        || (ep->hash == hash && mp->keycmp(ep->key, key) == 0))
        return ep;
    for (perturb = hash;; perturb >>= PERTURB_SHIFT) {
        i = (i << 2) + i + perturb + 1;
        ep = &ep0[i & mask];
        if (ep->key == NULL
            || ep->key == key
            || (ep->hash == hash && mp->keycmp(ep->key, key) == 0))
            return ep;
    }
    assert(0);          /* NOT REACHED */
    return EXIT_SUCCESS;
}

/*
intern routine used by dict_update and dict_set, allocate
new data copy if necessary.
*/
static int
dict_insert_by_entry(DictObject *mp, void *key, size_t hash, DictEntry *ep,
                    void *value) {
    if (ep->value) {
        void *old_value = ep->value;
        if ((ep->value = mp->valuedup(value)) == NULL)
            return EXIT_FAILURE;
        mp->valuefree(old_value);
    } else {
        void *old_key = ep->key;
        if ((ep->key = mp->keydup(key)) == NULL)
            return EXIT_FAILURE;
        if ((ep->value = mp->valuedup(value)) == NULL) {
            mp->keyfree(ep->key);
            return EXIT_FAILURE;
        }
        if (old_key == NULL)
            mp->fill++;
        mp->used++;
        ep->hash = hash;
    }
    return EXIT_SUCCESS;
}

static int
dict_insert(DictObject *mp, void *key, size_t hash, void *value) {
    DictEntry *ep = dict_search(mp, key, hash);
    return dict_insert_by_entry(mp, key, hash, ep, value);
}

/*
intern fast function to insert item when no dummy or
equal key exists in table, and insert references, not
a new data copy.
*/
static void
dict_insert_clean(DictObject *mp, void *key, size_t hash,
                 void *value) {
    size_t i;
    size_t perturb;
    size_t mask = mp->mask;
    DictEntry *ep0 = mp->table;
    DictEntry *ep;
    i = (size_t)hash & mask;
    ep = &ep0[i];
    for (perturb = hash; ep->key != NULL; perturb >>= PERTURB_SHIFT) {
        i = (i << 2) + i + perturb + 1;
        ep = &ep0[i & mask];
    }
    mp->fill++;
    mp->used++;
    ep->key = key;
    ep->hash = hash;
    ep->value = value;
}

/*
Restructure the table by allocating a new table and reinserting all
items again.  When entries have been deleted, the new table may
actually be smaller than the old one.
*/
static int
dict_resize(DictObject *mp, size_t minused) {
    size_t newsize;
    DictEntry *oldtable, *newtable, *ep;
    DictEntry small_copy[HASH_MINSIZE];
    /* Find the smallest table size > minused. */
    for (newsize = HASH_MINSIZE;
            newsize <= minused && newsize > 0;
            newsize <<= 1)
        ;
    /* Get space for a new table. */
    oldtable = mp->table;
    size_t is_oldtable_malloced = (oldtable != mp->smalltable);
    if (newsize == HASH_MINSIZE) {
        /* A large table is shrinking, or we can't get any smaller. */
        newtable = mp->smalltable;
        if (newtable == oldtable) {
            if (mp->fill == mp->used) {
                /* No dummies, so no point doing anything. */
                return EXIT_SUCCESS;
            }
            assert(mp->fill > mp->used);
            memcpy(small_copy, oldtable, sizeof(small_copy));
            oldtable = small_copy;
        }
    } else {
        newtable = (DictEntry*)malloc(sizeof(DictEntry) * newsize);
        if (newtable == NULL)
            return EXIT_FAILURE;
    }
    /* Make the dict empty, using the new table. */
    assert(newtable != oldtable);
    memset(newtable, 0, sizeof(DictEntry)* newsize);
    mp->table = newtable;
    mp->mask = newsize - 1;
    size_t used = mp->used;
    mp->used = 0;
    mp->fill = 0;
    for (ep = oldtable; used > 0; ep++) {
        if (ep->value) {             /* active entry */
            used--;
#ifdef X_DEBUG
            assert(ep->key != dummy);
#endif
            dict_insert_clean(mp, ep->key, ep->hash, ep->value);
        }
#ifdef X_DEBUG
        else if (ep->key) {    /* dummy entry */
            assert(ep->key == dummy);
            fprintf(stderr, "dict_resize -> found a dummy key, its hash is %u.\n",
                    ep->hash);
        }
#endif
    }
    if (is_oldtable_malloced)
        free(oldtable);
    return EXIT_SUCCESS;
}

/*helper function for sorting a DictEntry by its value*/
static int
_valcmp(const void *ep1, const void *ep2) {
    return *(size_t *)(*(DictEntry *)ep1).value > *(size_t *)(*
            (DictEntry *)ep2).value ? -1 : 1;
}

DictObject *
dict_cnew(size_t size,
          size_t (*keyhash)(void *key),
          int (*keycmp)(void *key1, void *key2),
          void * (*keydup)(void *key),
          void * (*valuedup)(void *value),
          void * (*valuedefault)(void),
          void (*keyfree)(void *key),
          void (*valuefree)(void *value)) {
    DictObject *mp = (DictObject *)malloc(sizeof(DictObject));
    if (mp == NULL)
        return NULL;
    size_t newsize;
    for (newsize = HASH_MINSIZE;
            newsize < size && newsize > 0;
            newsize <<= 1)
        ;
    if (newsize > HASH_MINSIZE) {
        DictEntry *newtable = (DictEntry*)malloc(sizeof(DictEntry) * newsize);
        if (newtable == NULL)
            return NULL;
        memset(newtable, 0, sizeof(DictEntry)* newsize);
        mp->table = newtable;
        mp->mask = newsize - 1;
        mp->fill = mp->used = 0;
    } else {
        EMPTY_TO_MINSIZE(mp);
    }
    mp->keyhash = keyhash ? keyhash : default_keyhash;
    mp->keycmp = keycmp ? keycmp : default_keycmp;
    mp->keydup = keydup ? keydup : default_keydup;
    mp->valuedup = valuedup ? valuedup : default_valuedup;
    mp->valuedefault = valuedefault ? valuedefault : default_valuedefault;
    mp->keyfree = keyfree ? keyfree : free;
    mp->valuefree = valuefree ? valuefree : free;
    return mp;
}

DictObject *
dict_new(void) {
    DictObject *mp = (DictObject *)malloc(sizeof(DictObject));
    if (mp == NULL)
        return NULL;
    EMPTY_TO_MINSIZE(mp);
    mp->keyhash = default_keyhash;
    mp->keycmp = default_keycmp;
    mp->keydup = default_keydup;
    mp->valuedup = default_valuedup;
    mp->valuedefault = default_valuedefault;
    mp->keyfree = free;
    mp->valuefree = free;
    return mp;
}

void
dict_clear(DictObject *mp) {
    DictEntry *ep, *table = mp->table;
    assert(table != NULL);
    DictEntry small_copy[HASH_MINSIZE];
    size_t table_is_malloced = (table != mp->smalltable);
    size_t fill = mp->fill;
    size_t used = mp->used;
    if (table_is_malloced)
        EMPTY_TO_MINSIZE(mp);
    else if (fill > 0) {
        /* It's a small table with something that needs to be cleared. */
        memcpy(small_copy, table, sizeof(small_copy));
        table = small_copy;
        EMPTY_TO_MINSIZE(mp);
    } else /* else it's a small table that's already empty */
        return;
    for (ep = table; used > 0; ep++) {
        /*only free active entry, this is different from thon 2.7*/
        if (ep->value) {
            used--;
#ifdef X_DEBUG
            assert(ep->key != dummy);
#endif
            mp->keyfree(ep->key);
            mp->valuefree(ep->value);
        }
#ifdef X_DEBUG
        else if (ep->key) {    /* dummy entry */
            assert(ep->key == dummy);
            fprintf(stderr, "dict_clear -> found a dummy key, its hash is %u.\n", ep->hash);
        }
#endif
    }
    if (table_is_malloced)
        free(table);
}

void
dict_free(DictObject *mp) {
    dict_clear(mp);
    free(mp);
}

void *
dict_get(DictObject *mp, void *key) {
    assert(key);
    size_t hash = mp->keyhash(key);
    DictEntry *ep = dict_search(mp, key, hash);
    return ep->value;
}

int
dict_set(DictObject *mp, void *key, void *value) {
    assert(key);
    assert(value);
    size_t hash = mp->keyhash(key);
    if (dict_insert(mp, key, hash, value) == EXIT_FAILURE)
        return EXIT_FAILURE;
    if (NEED_RESIZE(mp))
        return dict_resize(mp, RESIZE_NUM(mp));
    return EXIT_SUCCESS;
}

int
dict_rset(DictObject *mp, void *key, void *value) {
    assert(key);
    assert(value);
    size_t hash = mp->keyhash(key);
    DictEntry *ep = dict_search(mp, key, hash);
    /*if key exists, same as dict_rupdate_value*/
    if (ep->value) {
        if (ep->value == value)
            return EXIT_SUCCESS;
        void *old_value = ep->value;
        ep->value = value;
        mp->valuefree(old_value);
    } else {
        /*if key doesn't exist, same as dict_radd*/
        if (ep->key == NULL)
            mp->fill++;
        mp->used++;
        ep->key = key;
        ep->value = value;
        ep->hash = hash;
        if (NEED_RESIZE(mp))
            return dict_resize(mp, RESIZE_NUM(mp));
    }
    return EXIT_SUCCESS;
}

int
dict_add(DictObject *mp, void *key, void *value) {
    assert(key);
    assert(value);
    size_t hash = mp->keyhash(key);
    DictEntry *ep = dict_search(mp, key, hash);
    /*only for non-existing keys*/
    assert(ep->value == NULL);
    if (dict_insert_by_entry(mp, key, hash, ep, value) == EXIT_FAILURE)
        return EXIT_FAILURE;
    if (NEED_RESIZE(mp))
        return dict_resize(mp, RESIZE_NUM(mp));
    return EXIT_SUCCESS;
}

int
dict_radd(DictObject *mp, void *key, void *value) {
    assert(key);
    assert(value);
    size_t hash = mp->keyhash(key);
    DictEntry *ep = dict_search(mp, key, hash);
    /*only for non-existing keys*/
    assert(ep->value == NULL);
    if (ep->key == NULL)
        mp->fill++;
    mp->used++;
    ep->key = key;
    ep->value = value;
    ep->hash = hash;
    if (NEED_RESIZE(mp))
        return dict_resize(mp, RESIZE_NUM(mp));
    return EXIT_SUCCESS;
}

int
dict_update_value(DictObject *mp, void *key, void *value) {
    assert(key);
    assert(value);
    size_t hash = mp->keyhash(key);
    DictEntry *ep = dict_search(mp, key, hash);
    /*only for existing keys*/
    assert(ep->value);
    return dict_insert_by_entry(mp, key, hash, ep, value);
}

int
dict_rupdate_value(DictObject *mp, void *key, void *value) {
    assert(key);
    assert(value);
    size_t hash = mp->keyhash(key);
    DictEntry *ep = dict_search(mp, key, hash);
    /*only for existing keys*/
    assert(ep->value);
    /*duplicate update, no need to go further*/
    if (ep->value == value)
        return EXIT_SUCCESS;
    void *old_value = ep->value;
    ep->value = value;
    mp->valuefree(old_value);
    return EXIT_SUCCESS;
}

void
dict_del(DictObject *mp, void *key) {
    assert(key);
    size_t hash = mp->keyhash(key);
    DictEntry *ep = dict_search(mp, key, hash);
    /*only for existing keys*/
    assert(ep->value);
    mp->keyfree(ep->key);
    mp->valuefree(ep->value);
    ep->key = dummy;
    ep->value = NULL;
    mp->used--;
}

void *
dict_fget(DictObject *mp, void *key) {
    assert(key);
    size_t hash = mp->keyhash(key);
    DictEntry *ep = dict_search(mp, key, hash);
    if (ep->value == NULL) { /* dummy or unused */
        void *old_key = ep->key;
        if ((ep->key = mp->keydup(key)) == NULL)
            return NULL;
        if ((ep->value = mp->valuedefault()) == NULL) {
            mp->keyfree(ep->key);
            return NULL;
        }
        if (old_key == NULL)
            mp->fill++;
        mp->used++;
        ep->hash = hash;
        if (NEED_RESIZE(mp)) {
            dict_resize(mp, RESIZE_NUM(mp));
            ep = dict_search_nodummy(mp, key, hash);
        }
    }
    return ep->value;
}

int
dict_update(DictObject *mp, DictObject *other) {
    if (mp == other || other->used == 0)
        return EXIT_SUCCESS;
    /* Do one big resize at the start, rather than
     * incrementally resizing as we insert new items.  Expect
     * that there will be no (or few) overlapping keys.
     */
    if ((mp->fill + other->used) * 3 >= (mp->mask + 1) * 2) {
        if (dict_resize(mp, (mp->used + other->used) * 2) != EXIT_SUCCESS)
            return EXIT_FAILURE;
    }
    DictEntry *ep;
    size_t used = other->used;
    for (ep = other->table; used > 0; ep++) {
        if (ep->value) {             /* active entry */
            used--;
#ifdef X_DEBUG
            assert(ep->key != dummy);
#endif
            if (dict_insert(mp, ep->key, ep->hash, ep->value) == EXIT_FAILURE)
                return EXIT_FAILURE;
        }
#ifdef X_DEBUG
        else if (ep->key) {    /* dummy entry */
            assert(ep->key == dummy);
            fprintf(stderr, "dict_copy -> found a dummy key, its hash is %u.\n",
                    ep->hash);
        }
#endif
    }
    return EXIT_SUCCESS;
}

DictObject *dict_copy(DictObject *mp) {
    DictObject *copy = DICT_COPY_INIT(mp);
    if (copy == NULL)
        return NULL;
    DictEntry *ep;
    void *key, *value;
    size_t used = mp->used;
    for (ep = mp->table; used > 0; ep++) {
        if (ep->value) {             /* active entry */
            used--;
#ifdef X_DEBUG
            assert(ep->key != dummy);
#endif
            if ((key = copy->keydup(ep->key)) == NULL) {
                dict_free(copy);
                return NULL;
            }
            if ((value = copy->valuedup(ep->value)) == NULL) {
                copy->keyfree(key);
                dict_free(copy);
                return NULL;
            }
            dict_insert_clean(copy, key, ep->hash, value);
        }
#ifdef X_DEBUG
        else if (ep->key) {    /* dummy entry */
            assert(ep->key == dummy);
            fprintf(stderr, "dict_copy -> found a dummy key, its hash is %u.\n",
                    ep->hash);
        }
#endif
    }
    return copy;
}

size_t
dict_len(DictObject *mp) {
    return mp->used;
}

size_t dict_has(DictObject *mp, void *key){
    assert(key);
    return dict_get(mp,key)?1:0;
}

/*print key value pair by value DESC order*/
void
dict_print_by_value_desc(DictObject *mp) {
    DictEntry *ep;
    DictEntry temp_table[mp->used];
    size_t i = 0, used = mp->used;
#ifdef X_DEBUG
    fprintf(stderr, "hash table, size:%u, del:%u, used:%u\n", mp->mask + 1,
            mp->fill - mp->used, mp->used);
#endif
    for (ep = mp->table; used > 0; ep++) {
        if (ep->value) {
            used--;
#ifdef X_DEBUG
            assert(ep->key != dummy);
#endif
            temp_table[i++] = *ep;
        }
#ifdef X_DEBUG
        else if (ep->key) {
            assert(ep->key == dummy);
            fprintf(stderr, "print -> found a dummy key, its hash is %u.\n", ep->hash);
        }
#endif
    }
    used = mp->used;
    qsort(temp_table, used, sizeof(temp_table[0]), _valcmp);
    for (i = 0; i < used; i++)
        fprintf(stdout, "%s\t%u\n", (char *)temp_table[i].key,
                *(size_t *)temp_table[i].value);
}
