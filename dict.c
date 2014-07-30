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
dict_search(DictObject *dp, void *key, size_t hash) {
    size_t i;
    size_t perturb;
    DictEntry *freeslot;
    size_t mask = dp->mask;
    DictEntry *ep0 = dp->table;
    DictEntry *ep;
    i = (size_t)hash & mask;
    ep = &ep0[i];
    if (ep->key == NULL || ep->key == key)
        return ep;
    if (ep->key == dummy)
        freeslot = ep;
    else if (ep->hash == hash
             && dp->keycmp(ep->key, key) == 0)
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
                               && dp->keycmp(ep->key, key) == 0))
            return ep;
        if (ep->key == dummy && freeslot == NULL)
            freeslot = ep;
    }
    assert(0);          /* NOT REACHED */
    return NULL;
}

/*faster method used when no dummy key exists in table*/
static DictEntry *
dict_search_nodummy(DictObject *dp, void *key, size_t hash) {
    size_t i;
    size_t perturb;
    size_t mask = dp->mask;
    DictEntry *ep0 = dp->table;
    DictEntry *ep;
    i = (size_t)hash & mask;
    ep = &ep0[i];
    if (ep->key == NULL
            || ep->key == key
            || (ep->hash == hash && dp->keycmp(ep->key, key) == 0))
        return ep;
    for (perturb = hash;; perturb >>= PERTURB_SHIFT) {
        i = (i << 2) + i + perturb + 1;
        ep = &ep0[i & mask];
        if (ep->key == NULL
                || ep->key == key
                || (ep->hash == hash && dp->keycmp(ep->key, key) == 0))
            return ep;
    }
    assert(0);          /* NOT REACHED */
    return 0;
}

/*
intern routine used by dict_add, dict_replace and dict_set,
always makes a copy of key and value and free old ones as
possible.
*/
static int
dict_insert_by_entry(DictObject *dp, void *key, size_t hash, DictEntry *ep,
                     void *value) {
    if (ep->value) {
        void *old_value = ep->value;
        if ((ep->value = dp->valuedup(value)) == NULL)
            return -1;
        dp->valuefree(old_value);
    } else {
        void *old_key = ep->key;
        if ((ep->key = dp->keydup(key)) == NULL)
            return -1;
        if ((ep->value = dp->valuedup(value)) == NULL) {
            dp->keyfree(ep->key);
            return -1;
        }
        if (old_key == NULL)
            dp->fill++;
        dp->used++;
        ep->hash = hash;
    }
    return 0;
}

static int
dict_insert(DictObject *dp, void *key, size_t hash, void *value) {
    DictEntry *ep = dict_search(dp, key, hash);
    return dict_insert_by_entry(dp, key, hash, ep, value);
}

/*
intern fast function to insert item when no dummy or
equal key exists in table, and insert references, not
a new data copy.
*/
static void
dict_insert_clean(DictObject *dp, void *key, size_t hash,
                  void *value) {
    size_t i;
    size_t perturb;
    size_t mask = dp->mask;
    DictEntry *ep0 = dp->table;
    DictEntry *ep;
    i = (size_t)hash & mask;
    ep = &ep0[i];
    for (perturb = hash; ep->key != NULL; perturb >>= PERTURB_SHIFT) {
        i = (i << 2) + i + perturb + 1;
        ep = &ep0[i & mask];
    }
    dp->fill++;
    dp->used++;
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
dict_resize(DictObject *dp, size_t minused) {
    size_t newsize;
    DictEntry *oldtable, *newtable, *ep;
    DictEntry small_copy[HASH_MINSIZE];
    /* Find the smallest table size > minused. */
    for (newsize = HASH_MINSIZE;
            newsize <= minused && newsize > 0;
            newsize <<= 1)
        ;
    /* Get space for a new table. */
    oldtable = dp->table;
    size_t is_oldtable_malloced = (oldtable != dp->smalltable);
    if (newsize == HASH_MINSIZE) {
        /* A large table is shrinking, or we can't get any smaller. */
        newtable = dp->smalltable;
        if (newtable == oldtable) {
            if (dp->fill == dp->used) {
                /* No dummies, so no point doing anything. */
                return 0;
            }
            assert(dp->fill > dp->used);
            memcpy(small_copy, oldtable, sizeof(small_copy));
            oldtable = small_copy;
        }
    } else {
        newtable = (DictEntry*)malloc(sizeof(DictEntry) * newsize);
        if (newtable == NULL)
            return -1;
    }
    /* Make the dict empty, using the new table. */
    assert(newtable != oldtable);
    memset(newtable, 0, sizeof(DictEntry)* newsize);
    dp->table = newtable;
    dp->mask = newsize - 1;
    size_t used = dp->used;
    dp->used = 0;
    dp->fill = 0;
    for (ep = oldtable; used > 0; ep++) {
        if (ep->value) {             /* active entry */
            used--;
            dict_insert_clean(dp, ep->key, ep->hash, ep->value);
        }
    }
    if (is_oldtable_malloced)
        free(oldtable);
    return 0;
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
    DictObject *dp = (DictObject *)malloc(sizeof(DictObject));
    if (dp == NULL)
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
        dp->table = newtable;
        dp->mask = newsize - 1;
        dp->fill = dp->used = 0;
    } else {
        EMPTY_TO_MINSIZE(dp);
    }
    dp->type = DICT;
    dp->keyhash = keyhash ? keyhash : default_keyhash;
    dp->keycmp = keycmp ? keycmp : default_keycmp;
    dp->keydup = keydup ? keydup : default_keydup;
    dp->valuedup = valuedup ? valuedup : default_valuedup;
    dp->valuedefault = valuedefault ? valuedefault : default_valuedefault;
    dp->keyfree = keyfree ? keyfree : free;
    dp->valuefree = valuefree ? valuefree : free;
    return dp;
}

DictObject *
dict_new(void) {
    DictObject *dp = (DictObject *)malloc(sizeof(DictObject));
    if (dp == NULL)
        return NULL;
    EMPTY_TO_MINSIZE(dp);
    dp->type = DICT;
    dp->keyhash = default_keyhash;
    dp->keycmp = default_keycmp;
    dp->keydup = default_keydup;
    dp->valuedup = default_valuedup;
    dp->valuedefault = default_valuedefault;
    dp->keyfree = free;
    dp->valuefree = free;
    return dp;
}

void
dict_clear(DictObject *dp) {
    DictEntry *ep, *table = dp->table;
    assert(table != NULL);
    DictEntry small_copy[HASH_MINSIZE];
    size_t table_is_malloced = (table != dp->smalltable);
    size_t fill = dp->fill;
    size_t used = dp->used;
    if (table_is_malloced)
        EMPTY_TO_MINSIZE(dp);
    else if (fill > 0) {
        /* It's a small table with something that needs to be cleared. */
        memcpy(small_copy, table, sizeof(small_copy));
        table = small_copy;
        EMPTY_TO_MINSIZE(dp);
    } else /* else it's a small table that's already empty */
        return;
    for (ep = table; used > 0; ep++) {
        /*only free active entry, this is different from thon 2.7*/
        if (ep->value) {
            used--;
            dp->keyfree(ep->key);
            dp->valuefree(ep->value);
        }
    }
    if (table_is_malloced)
        free(table);
}

void
dict_free(DictObject *dp) {
    dict_clear(dp);
    free(dp);
}

/*return key's correspondent value. if key does not
exist, return NULL.*/
void *
dict_get(DictObject *dp, void *key) {
    assert(key);
    size_t hash = dp->keyhash(key);
    DictEntry *ep = dict_search(dp, key, hash);
    return ep->value;
}

/*if the @key exists, replacing its correspondent value A with @value's
copy, then free A. if not, add @key and @value's copy to the dict*/
int
dict_set(DictObject *dp, void *key, void *value) {
    assert(key);
    assert(value);
    size_t hash = dp->keyhash(key);
    if (dict_insert(dp, key, hash, value) == -1)
        return -1;
    if (NEED_RESIZE(dp))
        return dict_resize(dp, RESIZE_NUM(dp));
    return 0;
}

/*almost the same as dict_set, except only passing @key or @value's
address, not a copy*/
int
dict_rset(DictObject *dp, void *key, void *value) {
    assert(key);
    assert(value);
    size_t hash = dp->keyhash(key);
    DictEntry *ep = dict_search(dp, key, hash);
    /*if key exists, same as dict_rreplace*/
    if (ep->value) {
        /* free the key if necessary*/
        if (ep->key != key)
            dp->keyfree(key);
        if (ep->value == value)
            return 0;
        void *old_value = ep->value;
        ep->value = value;
        dp->valuefree(old_value);
    } else {
        /*if key doesn't exist, same as dict_radd*/
        if (ep->key == NULL)
            dp->fill++;
        dp->used++;
        ep->key = key;
        ep->value = value;
        ep->hash = hash;
        if (NEED_RESIZE(dp))
            return dict_resize(dp, RESIZE_NUM(dp));
    }
    return 0;
}

/*add @key and @value's copy to the dict. if @key already exists,
the program will exit.*/
int
dict_add(DictObject *dp, void *key, void *value) {
    assert(key);
    assert(value);
    size_t hash = dp->keyhash(key);
    DictEntry *ep = dict_search(dp, key, hash);
    /*only for non-existing keys*/
    assert(ep->value == NULL);
    if (dict_insert_by_entry(dp, key, hash, ep, value) == -1)
        return -1;
    if (NEED_RESIZE(dp))
        return dict_resize(dp, RESIZE_NUM(dp));
    return 0;
}

/*almost the same as dict_add, except only passing @key
or @value's address, not its memory copy*/
int
dict_radd(DictObject *dp, void *key, void *value) {
    assert(key);
    assert(value);
    size_t hash = dp->keyhash(key);
    DictEntry *ep = dict_search(dp, key, hash);
    /*only for non-existing keys*/
    assert(ep->value == NULL);
    if (ep->key == NULL)
        dp->fill++;
    dp->used++;
    ep->key = key;
    ep->value = value;
    ep->hash = hash;
    if (NEED_RESIZE(dp))
        return dict_resize(dp, RESIZE_NUM(dp));
    return 0;
}

/*replacing an existing key's correspondent value with
the value's copy and free old value. But if the two
values' addresses are the same, do nothing. */
int
dict_replace(DictObject *dp, void *key, void *value) {
    assert(key);
    assert(value);
    size_t hash = dp->keyhash(key);
    DictEntry *ep = dict_search(dp, key, hash);
    /*only for existing keys*/
    assert(ep->value);
    return dict_insert_by_entry(dp, key, hash, ep, value);
}

/*almost the same as dict_replace, except only passing
@value's address, not its memory copy*/
int
dict_rreplace(DictObject *dp, void *key, void *value) {
    assert(key);
    assert(value);
    size_t hash = dp->keyhash(key);
    DictEntry *ep = dict_search(dp, key, hash);
    /*only for existing keys*/
    assert(ep->value);
    /*duplicate call, no need to go further*/
    if (ep->value == value)
        return 0;
    void *old_value = ep->value;
    ep->value = value; /*passing address*/
    dp->valuefree(old_value);
    return 0;
}

void
dict_del(DictObject *dp, void *key) {
    assert(key);
    size_t hash = dp->keyhash(key);
    DictEntry *ep = dict_search(dp, key, hash);
    /*only for existing keys*/
    assert(ep->value);
    dp->keyfree(ep->key);
    dp->valuefree(ep->value);
    ep->key = dummy;
    ep->value = NULL;
    dp->used--;
}

/*if @key exists, same as dict_get. if not, add @key and the
dict's default value's copies to dict first, then return this
entry's value.*/
void *
dict_fget(DictObject *dp, void *key) {
    assert(key);
    size_t hash = dp->keyhash(key);
    DictEntry *ep = dict_search(dp, key, hash);
    if (ep->value == NULL) { /* dummy or unused */
        void *old_key = ep->key;
        if ((ep->key = dp->keydup(key)) == NULL)
            return NULL;
        if ((ep->value = dp->valuedefault()) == NULL) {
            dp->keyfree(ep->key);
            return NULL;
        }
        if (old_key == NULL)
            dp->fill++;
        dp->used++;
        ep->hash = hash;
        if (NEED_RESIZE(dp)) {
            dict_resize(dp, RESIZE_NUM(dp));
            ep = dict_search_nodummy(dp, key, hash);
        }
    }
    return ep->value;
}

/*same as bulk call dict_set to dp with keys and values from other*/
int
dict_update(DictObject *dp, DictObject *other) {
    if (dp == other || other->used == 0)
        return 0;
    /* Do one big resize at the start, rather than
     * incrementally resizing as we insert new items.  Expect
     * that there will be no (or few) overlapping keys.
     */
    if ((dp->fill + other->used) * 3 >= (dp->mask + 1) * 2) {
        if (dict_resize(dp, (dp->used + other->used) * 2) != 0)
            return -1;
    }
    DictEntry *ep;
    size_t used = other->used;
    for (ep = other->table; used > 0; ep++) {
        if (ep->value) {             /* active entry */
            used--;
            if (dict_insert_by_entry(dp, ep->key, ep->hash, ep, ep->value) == -1)
                return -1;
        }
    }
    return 0;
}

/*make a copy of dp, deleting dummy entries by the way*/
DictObject *
dict_copy(DictObject *dp) {
    DictObject *copy = DICT_COPY_INIT(dp);
    if (copy == NULL)
        return NULL;
    DictEntry *ep;
    void *key, *value;
    size_t used = dp->used;
    for (ep = dp->table; used > 0; ep++) {
        if (ep->value) {             /* active entry */
            used--;
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
    }
    return copy;
}

size_t
dict_len(DictObject *dp) {
    return dp->used;
}

size_t 
dict_has(DictObject *dp, void *key) {
    assert(key);
    return dict_get(dp, key) ? 1 : 0;
}

IterObject *
dict_iter_new(DictObject *dp) {
    IterObject *dio;
    dio = (IterObject*)malloc(sizeof(IterObject));
    if (dio == NULL)
        return NULL;
    dio->object = (void*)dp;
    dio->inipos = (void*)dp->table;
    dio->rest = dp->used;
    dio->type = DICT;
    return dio;
}

size_t
dict_iter_walk(IterObject *dio, void **key_addr) {
    DictEntry *ep;
    void *key;
    size_t rest = dio->rest;
    for(ep = (DictEntry *)dio->inipos; rest > 0; ep++) {
        key = ep->key;
        if ( key && key != dummy) {
            dio->rest--;
            dio->inipos = (void*)(ep + 1);
            *key_addr = key;
            return 1;
        }
    }
    return 0;
}

void
dict_iter_flush(IterObject *dio) {
    DictObject *dp = (DictObject *)dio->object;
    dio->inipos = (void*)dp->table;
    dio->rest = dp->used;
    dio->type = DICT;
}

size_t
dict_iter_items(IterObject *dio, void **key_addr, void **value_addr) {
    DictEntry *ep;
    void *key;
    size_t rest = dio->rest;
    for(ep = (DictEntry *)dio->inipos; rest > 0; ep++) {
        key = ep->key;
        if ( key && key != dummy) {
            dio->rest--;
            dio->inipos = (void*)(ep + 1);
            *key_addr = key;
            *value_addr = ep->value;
            return 1;
        }
    }
    return 0;
}

void
dict_print2(DictObject *dp) {
    void *key, *value;
    IterObject *dio = dict_iter_new(dp);
    while (dict_iter_items(dio, &key, &value)) {
        fprintf(stdout, "%s\t%u\n", (char*)key, *(size_t*)value);
    }
    free(dio);
}

void
dict_print(DictObject *dp) {
    void *key, *value;
    IterObject *dio = iter(dp);
    while (iterw(dio, &key)) {
        value = dict_get(dp, key);
        fprintf(stdout, "%s\t%u\n", (char*)key, *(size_t*)value);
    }
    free(dio);
}

/*print key value pair by value DESC order*/
void
dict_print_by_value_desc(DictObject *dp) {
    DictEntry *ep;
    DictEntry temp_table[dp->used];
    size_t i = 0, used = dp->used;
    for (ep = dp->table; used > 0; ep++) {
        if (ep->value) {
            used--;
            temp_table[i++] = *ep;
        }
    }
    used = dp->used;
    qsort(temp_table, used, sizeof(temp_table[0]), _valcmp);
    for (i = 0; i < used; i++)
        fprintf(stdout, "%s\t%u\n", (char *)temp_table[i].key,
                *(size_t *)temp_table[i].value);
}
