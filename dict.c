/* Pure C simple version of python 2.7.8 hash table implementation */
/* Sample usage: see main() */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#define PyDict_MINSIZE 8
#define PERTURB_SHIFT 5
#define NEED_RESIZE(mp) ((mp)->ma_fill * 3 >= ((mp)->ma_mask + 1) * 2)
#define RESIZE_NUM(mp) (((mp)->ma_used > 50000 ? 2 : 4) * (mp)->ma_used)
#define INIT_NONZERO_DICT_SLOTS(mp) do {                                \
    (mp)->ma_table = (mp)->ma_smalltable;                               \
    (mp)->ma_mask = PyDict_MINSIZE - 1;                                 \
    } while(0)

#define EMPTY_TO_MINSIZE(mp) do {                                       \
    memset((mp)->ma_smalltable, 0, sizeof((mp)->ma_smalltable));        \
    (mp)->ma_used = (mp)->ma_fill = 0;                                  \
    INIT_NONZERO_DICT_SLOTS(mp);                                        \
    } while(0)

typedef void PyObject;

typedef struct {
    size_t me_hash;
    PyObject *me_key;
    PyObject *me_value;
} PyDictEntry;

typedef struct _dictobject PyDictObject;
struct _dictobject {
    size_t ma_fill;  /* # Active + # Dummy */
    size_t ma_used;  /* # Active */
    size_t ma_mask;
    PyDictEntry *ma_table;
    size_t (*ma_keyhash)(PyObject *key);
    int (*ma_keycmp)(PyObject *key1, PyObject *key2);
    PyObject *(*ma_keydup)(PyObject *key);
    PyObject *(*ma_valuedup)(PyObject *value);
    PyObject *(*ma_default)(void);
    PyDictEntry ma_smalltable[PyDict_MINSIZE];
};

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

PyDictObject *
dict_new_custom(size_t ma_size,
                size_t (*ma_keyhash)(PyObject *key),
                int (*ma_keycmp)(PyObject *key1, PyObject *key2),
                PyObject * (*ma_keydup)(PyObject *key),
                PyObject * (*ma_valuedup)(PyObject *value),
                PyObject * (*ma_default)(void)) {
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
    return 0;
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
    return 0;
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
                return 0;
            }
            assert(mp->ma_fill > mp->ma_used);
            memcpy(small_copy, oldtable, sizeof(small_copy));
            oldtable = small_copy;
        }
    } else {
        newtable = (PyDictEntry*)malloc(sizeof(PyDictEntry) * newsize);
        if (newtable == NULL)
            return -1;
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
    return 0;
}

PyObject *
dict_search(PyDictObject *mp, PyObject *key) {
    assert(key);
    size_t hash = mp->ma_keyhash(key);
    PyDictEntry *ep = lookdict(mp, key, hash);
    return ep->me_value;
}

size_t
dict_contain(PyDictObject *mp, PyObject *key) {
    return dict_search(mp, key) ? 1 : 0;
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
        return -1;
    if ((ep->me_value = mp->ma_valuedup(value)) == NULL) {
        free(ep->me_key);
        return -1;
    }
    if (old_key == NULL)
        mp->ma_fill++;
    mp->ma_used++;
    ep->me_hash = hash;
    if (NEED_RESIZE(mp))
        return dict_resize(mp, RESIZE_NUM(mp));
    return 0;
}

size_t
dict_update(PyDictObject *mp, PyObject *key, PyObject *value) {
    assert(key);
    assert(value);
    size_t hash = mp->ma_keyhash(key);
    PyDictEntry *ep = lookdict(mp, key, hash);
    /*only for existing keys*/
    assert(ep->me_value != NULL);
    PyObject *old_value = ep->me_value;
    if ((ep->me_value = mp->ma_valuedup(value)) == NULL)
        return -1;
    free(old_value);
    return 0;
}

size_t
dict_del(PyDictObject *mp, PyObject *key) {
    assert(key);
    size_t hash = mp->ma_keyhash(key);
    PyDictEntry *ep = lookdict(mp, key, hash);
    /*only for existing keys*/
    assert(ep->me_value != NULL);
    free(ep->me_key);
    free(ep->me_value);
    ep->me_key = dummy;
    ep->me_value = NULL;
    mp->ma_used--;
    return 0;
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
            free(ep->me_key);
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
    else if (fill >
             0) { /* It's a small table with something that needs to be cleared. */
        memcpy(small_copy, table, sizeof(small_copy));
        table = small_copy;
        EMPTY_TO_MINSIZE(mp);
    } else /* else it's a small table that's already empty */
        return;
    for (ep = table; used > 0; ep++) {
        /*only free active entry, this is different from Python 2.7*/
        if (ep->me_value) {
            used--;
            free(ep->me_key);
            free(ep->me_value);
        }
    }
    if (table_is_malloced)
        free(table);
}

size_t
dict_len(PyDictObject *mp) {
    return mp->ma_used;
}

/*helper function for sorting a PyDictEntry by its value*/
static int
_valcmp(const void *ep1, const void *ep2) {
    return *(size_t *)(*(PyDictEntry *)ep1).me_value > *(size_t *)(*
            (PyDictEntry *)ep2).me_value ? -1 : 1;
}

/*print key value pair by value DESC order*/
static void
print_all_by_value_desc(PyDictObject *mp) {
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

/*scan words from stdin, print total amount for each word by DESC order*/
int main(void) {
    PyDictObject *mp = dict_new_custom(10000, 0, 0, 0, 0, 0);
    //PyDictObject *mp = dict_new();
    char keybuf[100];
    size_t valuebuf[] = { 1 };
    size_t *vp;
    while (fscanf(stdin, "%s", keybuf) == 1) {
        if (dict_contain(mp, keybuf)) {
            vp = dict_search(mp, keybuf);
            *vp += 1;
        } else
            dict_add(mp, keybuf, valuebuf);
    }
//    this is another faster version
//    while (fscanf(stdin, "%s", keybuf) == 1) {
//        vp = dict_force_search(mp, keybuf);
//        *vp += 1;
//    }
//    dict_del(mp, "the");
//    dict_del(mp, "and");
//    dict_del(mp, "of");
    print_all_by_value_desc(mp);
    dict_clear(mp);
    dict_clear(mp); //just for test
    free(mp);
    return 0;
}