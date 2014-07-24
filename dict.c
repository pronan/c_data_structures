#include "xlib.h"

/* Object used as dummy key to fill deleted entries */
static PyDictEntry _dummy_struct;
#define dummy (&_dummy_struct)

static size_t
keyhash(PyObject *_key) {
    char *key = (char *)_key;
    size_t hash = 5381;
    for (; *key; key++)
        hash = ((hash << 5) + hash) + *key; /* hash * 33 + c */
    return hash;
}

static int
keycmp(PyObject *key1, PyObject *key2) {
    return strcmp((char *)key1, (char *)key2);
}

static PyObject *
keydup(PyObject *key) {
    return (PyObject *)strdup((char *)key);
}

static PyObject *
valuedup(PyObject *_value) {
    size_t *value = (size_t*)malloc(sizeof(size_t));
    *value = *(size_t *)_value;
    return (PyObject *)value;
}

static PyObject *
get_default_value(void) {
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
    size_t mask = mp->ma_mask;
    PyDictEntry *ep0 = mp->ma_table;
    PyDictEntry *ep;
    i = (size_t)hash & mask;
    ep = &ep0[i];
    if (ep->me_key == NULL || ep->me_key == key)
        return ep;
    if (ep->me_key == dummy)
        freeslot = ep;
    else if (ep->me_hash == hash
             && mp->ma_keycmp(ep->me_key, key) == 0)
        return ep;
    else
        freeslot = NULL;
    for (perturb = hash;; perturb >>= PERTURB_SHIFT) {
        i = (i << 2) + i + perturb + 1;
        ep = &ep0[i & mask];
        if (ep->me_key == NULL)
            return freeslot == NULL ? ep : freeslot;
        if (ep->me_key == key
                || (ep->me_hash == hash
                    && ep->me_key != dummy
                    && mp->ma_keycmp(ep->me_key, key) == 0))
            return ep;
        if (ep->me_key == dummy && freeslot == NULL)
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
    size_t mask = mp->ma_mask;
    PyDictEntry *ep0 = mp->ma_table;
    PyDictEntry *ep;
    i = (size_t)hash & mask;
    ep = &ep0[i];
    if (ep->me_key == NULL
            || ep->me_key == key
            || (ep->me_hash == hash && mp->ma_keycmp(ep->me_key, key) == 0))
        return ep;
    for (perturb = hash;; perturb >>= PERTURB_SHIFT) {
        i = (i << 2) + i + perturb + 1;
        ep = &ep0[i & mask];
        if (ep->me_key == NULL
                || ep->me_key == key
                || (ep->me_hash == hash && mp->ma_keycmp(ep->me_key, key) == 0))
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
    size_t mask = mp->ma_mask;
    PyDictEntry *ep0 = mp->ma_table;
    PyDictEntry *ep;
    i = (size_t)hash & mask;
    ep = &ep0[i];
    for (perturb = hash; ep->me_key != NULL; perturb >>= PERTURB_SHIFT) {
        i = (i << 2) + i + perturb + 1;
        ep = &ep0[i & mask];
    }
    mp->ma_fill++;
    mp->ma_used++;
    ep->me_key = key;
    ep->me_hash = hash;
    ep->me_value = value;
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
    oldtable = mp->ma_table;
    size_t is_oldtable_malloced = (oldtable != mp->ma_smalltable);
    if (newsize == PyDict_MINSIZE) {
        /* A large table is shrinking, or we can't get any smaller. */
        newtable = mp->ma_smalltable;
        if (newtable == oldtable) {
            if (mp->ma_fill == mp->ma_used) {
                /* No dummies, so no point doing anything. */
                return EXIT_SUCCESS;
            }
            assert(mp->ma_fill > mp->ma_used);
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
    mp->ma_table = newtable;
    mp->ma_mask = newsize - 1;
    mp->ma_used = 0;
    size_t i = mp->ma_fill;
    mp->ma_fill = 0;
    for (ep = oldtable; i > 0; ep++) {
        if (ep->me_value != NULL) {             /* active entry */
            --i;
            insertdict_clean(mp, ep->me_key, ep->me_hash, ep->me_value);
        } else if (ep->me_key != NULL) {    /* dummy entry */
            --i;
            assert(ep->me_key == dummy);
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
    return *(size_t *)(*(PyDictEntry *)ep1).me_value > *(size_t *)(*
            (PyDictEntry *)ep2).me_value ? -1 : 1;
}

PyDictObject *
dict_new_custom(size_t ma_size,
                size_t (*ma_keyhash)(PyObject *key),
                int (*ma_keycmp)(PyObject *key1, PyObject *key2),
                PyObject * (*ma_keydup)(PyObject *key),
                PyObject * (*ma_valuedup)(PyObject *value),
                PyObject * (*ma_default)(void),
                void (*ma_keyfree)(PyObject *key),
                void (*ma_valuefree)(PyObject *value)) {
    PyDictObject *mp = (PyDictObject *)malloc(sizeof(PyDictObject));
    if (mp == NULL)
        return NULL;
    size_t newsize;
    for (newsize = PyDict_MINSIZE;
            newsize < ma_size && newsize > 0;
            newsize <<= 1)
        ;
    if (newsize > PyDict_MINSIZE) {
        PyDictEntry *newtable = (PyDictEntry*)malloc(sizeof(PyDictEntry) * newsize);
        if (newtable == NULL)
            return NULL;
        memset(newtable, 0, sizeof(PyDictEntry)* newsize);
        mp->ma_table = newtable;
        mp->ma_mask = newsize - 1;
        mp->ma_fill = mp->ma_used = 0;
    } else {
        EMPTY_TO_MINSIZE(mp);
    }
    mp->ma_keyhash = ma_keyhash ? ma_keyhash : keyhash;
    mp->ma_keycmp = ma_keycmp ? ma_keycmp : keycmp;
    mp->ma_keydup = ma_keydup ? ma_keydup : keydup;
    mp->ma_valuedup = ma_valuedup ? ma_valuedup : valuedup;
    mp->ma_default = ma_default ? ma_default : get_default_value;
    mp->ma_keyfree = ma_keyfree ? ma_keyfree : free;
    mp->ma_valuefree = ma_valuefree ? ma_valuefree : free;
    return mp;
}

PyDictObject *
dict_new(void) {
    PyDictObject *mp = (PyDictObject *)malloc(sizeof(PyDictObject));
    if (mp == NULL)
        return NULL;
    EMPTY_TO_MINSIZE(mp);
    mp->ma_keyhash = keyhash;
    mp->ma_keycmp = keycmp;
    mp->ma_keydup = keydup;
    mp->ma_valuedup = valuedup;
    mp->ma_default = get_default_value;
    return mp;
}

void
dict_clear(PyDictObject *mp) {
    PyDictEntry *ep, *table = mp->ma_table;
    assert(table != NULL);
    PyDictEntry small_copy[PyDict_MINSIZE];
    size_t table_is_malloced = (table != mp->ma_smalltable);
    size_t fill = mp->ma_fill;
    size_t used = mp->ma_used;
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
        if (ep->me_value) {
            used--;
            mp->ma_keyfree(ep->me_key);
            mp->ma_valuefree(ep->me_value);
        }
    }
    if (table_is_malloced)
        free(table);
}

PyObject *
dict_search(PyDictObject *mp, PyObject *key) {
    assert(key);
    size_t hash = mp->ma_keyhash(key);
    PyDictEntry *ep = lookdict(mp, key, hash);
    return ep->me_value;
}

int
dict_add(PyDictObject *mp, PyObject *key, PyObject *value) {
    assert(key);
    assert(value);
    size_t hash = mp->ma_keyhash(key);
    PyDictEntry *ep = lookdict(mp, key, hash);
    /*only for non-existing keys*/
    assert(ep->me_value == NULL);
    PyObject *old_key = ep->me_key;
    if ((ep->me_key = mp->ma_keydup(key)) == NULL)
        return EXIT_FAILURE;
    if ((ep->me_value = mp->ma_valuedup(value)) == NULL) {
        mp->ma_keyfree(ep->me_key);
        return EXIT_FAILURE;
    }
    if (old_key == NULL)
        mp->ma_fill++;
    mp->ma_used++;
    ep->me_hash = hash;
    if (NEED_RESIZE(mp))
        return dict_resize(mp, RESIZE_NUM(mp));
    return EXIT_SUCCESS;
}

int
dict_update(PyDictObject *mp, PyObject *key, PyObject *value) {
    assert(key);
    assert(value);
    size_t hash = mp->ma_keyhash(key);
    PyDictEntry *ep = lookdict(mp, key, hash);
    /*only for existing keys*/
    assert(ep->me_value != NULL);
    PyObject *old_value = ep->me_value;
    if ((ep->me_value = mp->ma_valuedup(value)) == NULL)
        return EXIT_FAILURE;
    mp->ma_valuefree(old_value);
    return EXIT_SUCCESS;
}

int
dict_del(PyDictObject *mp, PyObject *key) {
    assert(key);
    size_t hash = mp->ma_keyhash(key);
    PyDictEntry *ep = lookdict(mp, key, hash);
    /*only for existing keys*/
    assert(ep->me_value != NULL);
    mp->ma_keyfree(ep->me_key);
    mp->ma_valuefree(ep->me_value);
    ep->me_key = dummy;
    ep->me_value = NULL;
    mp->ma_used--;
    return EXIT_SUCCESS;
}

size_t
dict_contain(PyDictObject *mp, PyObject *key) {
    return dict_search(mp, key) ? 1 : 0;
}

PyObject *
dict_force_search(PyDictObject *mp, PyObject *key) {
    assert(key);
    size_t hash = mp->ma_keyhash(key);
    PyDictEntry *ep = lookdict(mp, key, hash);
    if (ep->me_value == NULL) { /* dummy or unused */
        PyObject *old_key = ep->me_key;
        if ((ep->me_key = mp->ma_keydup(key)) == NULL)
            return NULL;
        if ((ep->me_value = mp->ma_default()) == NULL) {
            mp->ma_keyfree(ep->me_key);
            return NULL;
        }
        if (old_key == NULL)
            mp->ma_fill++;
        mp->ma_used++;
        ep->me_hash = hash;
        if (NEED_RESIZE(mp)) {
            dict_resize(mp, RESIZE_NUM(mp));
            ep = lookdict_nodummy(mp, key, hash);
        }
    }
    return ep->me_value;
}

size_t
dict_len(PyDictObject *mp) {
    return mp->ma_used;
}

/*print key value pair by value DESC order*/
void
dict_print_by_value_desc(PyDictObject *mp) {
    PyDictEntry *ep;
    PyDictEntry *temp_table = (PyDictEntry *)malloc(sizeof(PyDictEntry) *
                              (mp->ma_used));
    size_t i = 0, used = mp->ma_used;
    for (ep = mp->ma_table; used > 0; ep++) {
        if (ep->me_value) {
            used--;
            temp_table[i++] = *ep;
        }
    }
    used = mp->ma_used;
    qsort(temp_table, used, sizeof(temp_table[0]), _valcmp);
    for (i = 0; i < used; i++)
        fprintf(stdout, "%s\t%d\n", (char *)temp_table[i].me_key,
                *(size_t *)temp_table[i].me_value);
    free(temp_table);
}
