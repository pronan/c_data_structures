#include "xlib.h"

/* Object used as dummy key to fill deleted entries */
static PyDictEntry _dummy_struct;
#define dummy (&_dummy_struct)

static size_t
default_keyhash(PyObject *_key) {
    char *key = (char *)_key;
    size_t hash = 5381;
    for (; *key; key++)
        hash = ((hash << 5) + hash) + *key; /* hash * 33 + c */
    return hash;
}

static int
default_keycmp(PyObject *key1, PyObject *key2) {
    return strcmp((char *)key1, (char *)key2);
}

static PyObject *
default_keydup(PyObject *key) {
    return (PyObject *)strdup((char *)key);
}

static PyObject *
default_valuedup(PyObject *_value) {
    size_t *value = (size_t*)malloc(sizeof(size_t));
    *value = *(size_t *)_value;
    return (PyObject *)value;
}

static PyObject *
default_valuedefault(void) {
    size_t *value = (size_t*)malloc(sizeof(size_t));
    *value = 0;
    return (PyObject *)value;
}

/*intern basic search method, used by other fucntions*/
static PyDictEntry *
lookdict(PyDictObject *mp, PyObject *key, size_t hash) {
    size_t i;
    size_t perturb;
    PyDictEntry *freeslot;
    size_t mask = mp->mask;
    PyDictEntry *ep0 = mp->table;
    PyDictEntry *ep;
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
        if (ep->key == key
                || (ep->hash == hash
                    && ep->key != dummy
                    && mp->keycmp(ep->key, key) == 0))
            return ep;
        if (ep->key == dummy && freeslot == NULL)
            freeslot = ep;
    }
    assert(0);          /* NOT REACHED */
    return EXIT_SUCCESS;
}

/*faster method used when no dummy key exists in table*/
static PyDictEntry *
lookdict_nodummy(PyDictObject *mp, PyObject *key, size_t hash) {
    size_t i;
    size_t perturb;
    size_t mask = mp->mask;
    PyDictEntry *ep0 = mp->table;
    PyDictEntry *ep;
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

/*intern fast function to insert item when no dummy key exists in table*/
static void
insertdict_clean(PyDictObject *mp, PyObject *key, size_t hash,
                 PyObject *value) {
    size_t i;
    size_t perturb;
    size_t mask = mp->mask;
    PyDictEntry *ep0 = mp->table;
    PyDictEntry *ep;
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
dict_resize(PyDictObject *mp, size_t minused) {
    size_t newsize;
    PyDictEntry *oldtable, *newtable, *ep;
    PyDictEntry small_copy[PyDict_MINSIZE];
    /* Find the smallest table size > minused. */
    for (newsize = PyDict_MINSIZE;
            newsize <= minused && newsize > 0;
            newsize <<= 1)
        ;
    /* Get space for a new table. */
    oldtable = mp->table;
    size_t is_oldtable_malloced = (oldtable != mp->smalltable);
    if (newsize == PyDict_MINSIZE) {
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
        newtable = (PyDictEntry*)malloc(sizeof(PyDictEntry) * newsize);
        if (newtable == NULL)
            return EXIT_FAILURE;
    }
    /* Make the dict empty, using the new table. */
    assert(newtable != oldtable);
    memset(newtable, 0, sizeof(PyDictEntry)* newsize);
    mp->table = newtable;
    mp->mask = newsize - 1;
    mp->used = 0;
    size_t i = mp->fill;
    mp->fill = 0;
    for (ep = oldtable; i > 0; ep++) {
        if (ep->value != NULL) {             /* active entry */
            --i;
            insertdict_clean(mp, ep->key, ep->hash, ep->value);
        } else if (ep->key != NULL) {    /* dummy entry */
            --i;
            assert(ep->key == dummy);
        }
        /* else key == value == NULL:  nothing to do */
    }
    if (is_oldtable_malloced)
        free(oldtable);
    return EXIT_SUCCESS;
}

/*helper function for sorting a PyDictEntry by its value*/
static int
_valcmp(const void *ep1, const void *ep2) {
    return *(size_t *)(*(PyDictEntry *)ep1).value > *(size_t *)(*
            (PyDictEntry *)ep2).value ? -1 : 1;
}

PyDictObject *
dict_cnew(size_t size,
          size_t (*keyhash)(PyObject *key),
          int (*keycmp)(PyObject *key1, PyObject *key2),
          PyObject * (*keydup)(PyObject *key),
          PyObject * (*valuedup)(PyObject *value),
          PyObject * (*valuedefault)(void),
          void (*keyfree)(PyObject *key),
          void (*valuefree)(PyObject *value)) {
    PyDictObject *mp = (PyDictObject *)malloc(sizeof(PyDictObject));
    if (mp == NULL)
        return NULL;
    size_t newsize;
    for (newsize = PyDict_MINSIZE;
            newsize < size && newsize > 0;
            newsize <<= 1)
        ;
    if (newsize > PyDict_MINSIZE) {
        PyDictEntry *newtable = (PyDictEntry*)malloc(sizeof(PyDictEntry) * newsize);
        if (newtable == NULL)
            return NULL;
        memset(newtable, 0, sizeof(PyDictEntry)* newsize);
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

PyDictObject *
dict_new(void) {
    PyDictObject *mp = (PyDictObject *)malloc(sizeof(PyDictObject));
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
dict_clear(PyDictObject *mp) {
    PyDictEntry *ep, *table = mp->table;
    assert(table != NULL);
    PyDictEntry small_copy[PyDict_MINSIZE];
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
        /*only free active entry, this is different from Python 2.7*/
        if (ep->value) {
            used--;
            mp->keyfree(ep->key);
            mp->valuefree(ep->value);
        }
    }
    if (table_is_malloced)
        free(table);
}

void
dict_free(PyDictObject *mp) {
    dict_clear(mp);
    free(mp);
}

PyObject *
dict_get(PyDictObject *mp, PyObject *key) {
    assert(key);
    size_t hash = mp->keyhash(key);
    PyDictEntry *ep = lookdict(mp, key, hash);
    return ep->value;
}

int
dict_set(PyDictObject *mp, PyObject *key, PyObject *value) {
    assert(key);
    assert(value);
    size_t hash = mp->keyhash(key);
    PyDictEntry *ep = lookdict(mp, key, hash);
    /*if key exists, same as dict_update*/
    if (ep->value) {
        PyObject *old_value = ep->value;
        if ((ep->value = mp->valuedup(value)) == NULL)
            return EXIT_FAILURE;
        mp->valuefree(old_value);
        return EXIT_SUCCESS;
    } else {
        /*if key doesn't exist, same as dict_add*/
        PyObject *old_key = ep->key;
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
        if (NEED_RESIZE(mp))
            return dict_resize(mp, RESIZE_NUM(mp));
        return EXIT_SUCCESS;
    }
}

int
dict_add(PyDictObject *mp, PyObject *key, PyObject *value) {
    assert(key);
    assert(value);
    size_t hash = mp->keyhash(key);
    PyDictEntry *ep = lookdict(mp, key, hash);
    /*only for non-existing keys*/
    assert(ep->value == NULL);
    PyObject *old_key = ep->key;
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
    if (NEED_RESIZE(mp))
        return dict_resize(mp, RESIZE_NUM(mp));
    return EXIT_SUCCESS;
}

int
dict_update(PyDictObject *mp, PyObject *key, PyObject *value) {
    assert(key);
    assert(value);
    size_t hash = mp->keyhash(key);
    PyDictEntry *ep = lookdict(mp, key, hash);
    /*only for existing keys*/
    assert(ep->value != NULL);
    PyObject *old_value = ep->value;
    if ((ep->value = mp->valuedup(value)) == NULL)
        return EXIT_FAILURE;
    mp->valuefree(old_value);
    return EXIT_SUCCESS;
}

void
dict_del(PyDictObject *mp, PyObject *key) {
    assert(key);
    size_t hash = mp->keyhash(key);
    PyDictEntry *ep = lookdict(mp, key, hash);
    /*only for existing keys*/
    assert(ep->value != NULL);
    mp->keyfree(ep->key);
    mp->valuefree(ep->value);
    ep->key = dummy;
    ep->value = NULL;
    mp->used--;
}

PyObject *
dict_fget(PyDictObject *mp, PyObject *key) {
    assert(key);
    size_t hash = mp->keyhash(key);
    PyDictEntry *ep = lookdict(mp, key, hash);
    if (ep->value == NULL) { /* dummy or unused */
        PyObject *old_key = ep->key;
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
            ep = lookdict_nodummy(mp, key, hash);
        }
    }
    return ep->value;
}

size_t
dict_len(PyDictObject *mp) {
    return mp->used;
}

/*print key value pair by value DESC order*/
void
dict_print_by_value_desc(PyDictObject *mp) {
    PyDictEntry *ep;
    PyDictEntry *temp_table = (PyDictEntry *)malloc(sizeof(PyDictEntry) *
                              (mp->used));
    size_t i = 0, used = mp->used;
    for (ep = mp->table; used > 0; ep++) {
        if (ep->value) {
            used--;
            temp_table[i++] = *ep;
        }
    }
    used = mp->used;
    qsort(temp_table, used, sizeof(temp_table[0]), _valcmp);
    for (i = 0; i < used; i++)
        fprintf(stdout, "%s\t%d\n", (char *)temp_table[i].key,
                *(size_t *)temp_table[i].value);
    free(temp_table);
}
