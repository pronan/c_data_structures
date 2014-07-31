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
set_search(SetObject *sp, void *key, size_t hash) {
    size_t i;
    size_t perturb;
    SetEntry *freeslot;
    size_t mask = sp->mask;
    SetEntry *ep0 = sp->table;
    SetEntry *ep;
    i = (size_t)hash & mask;
    ep = &ep0[i];
    if (ep->key == NULL || ep->key == key)
        return ep;
    if (ep->key == dummy)
        freeslot = ep;
    else if (ep->hash == hash
             && sp->keycmp(ep->key, key) == 0)
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
                               && sp->keycmp(ep->key, key) == 0))
            return ep;
        if (ep->key == dummy && freeslot == NULL)
            freeslot = ep;
    }
    assert(0);          /* NOT REACHED */
    return NULL;
}

static size_t
set_has_intern(SetObject *sp, void *key, size_t hash) {
    SetEntry *ep = set_search(sp, key, hash);
    return ep->key && ep->key != dummy;
}

/* try to insert key's copy to sp */
static int
set_insert(SetObject *sp, void *key, size_t hash) {
    SetEntry *ep = set_search(sp, key, hash);
    if (ep->key == NULL) {
        if ((ep->key = sp->keydup(key)) == NULL)
            return -1;
        sp->fill++;
        sp->used++;
        ep->hash = hash;
    } else if (ep->key == dummy) {
        if ((ep->key = sp->keydup(key)) == NULL)
            return -1;
        sp->used++;
        ep->hash = hash;
    }/* else already key exists, do nothing */
    return 0;
}

/*
intern fast function to insert key's reference to sp
when no dummy or equal key exists.
*/
static void
set_insert_clean(SetObject *sp, void *key, size_t hash) {
    size_t i;
    size_t perturb;
    size_t mask = sp->mask;
    SetEntry *ep0 = sp->table;
    SetEntry *ep;
    i = (size_t)hash & mask;
    ep = &ep0[i];
    for (perturb = hash; ep->key != NULL; perturb >>= PERTURB_SHIFT) {
        i = (i << 2) + i + perturb + 1;
        ep = &ep0[i & mask];
    }
    sp->fill++;
    sp->used++;
    ep->key = key;
    ep->hash = hash;
}

static int
set_resize(SetObject *sp, size_t minused) {
    size_t newsize;
    SetEntry *oldtable, *newtable, *ep;
    SetEntry small_copy[HASH_MINSIZE];
    /* Find the smallest table size > minused. */
    for (newsize = HASH_MINSIZE;
            newsize <= minused && newsize > 0;
            newsize <<= 1)
        ;
    /* Get space for a new table. */
    oldtable = sp->table;
    size_t is_oldtable_malloced = (oldtable != sp->smalltable);
    if (newsize == HASH_MINSIZE) {
        /* A large table is shrinking, or we can't get any smaller. */
        newtable = sp->smalltable;
        if (newtable == oldtable) {
            if (sp->fill == sp->used) {
                /* No dummies, sp no point doing anything. */
                return 0;
            }
            memcpy(small_copy, oldtable, sizeof(small_copy));
            oldtable = small_copy;
        }
    } else {
        newtable = (SetEntry*)malloc(sizeof(SetEntry) * newsize);
        if (newtable == NULL)
            return -1;
    }
    memset(newtable, 0, sizeof(SetEntry)* newsize);
    sp->table = newtable;
    sp->mask = newsize - 1;
    size_t used = sp->used;
    sp->used = 0;
    sp->fill = 0;
    for (ep = oldtable; used > 0; ep++) {
        if (ep->key && ep->key != dummy) {           /* active key */
            used--;
            set_insert_clean(sp, ep->key, ep->hash);
        }
    }
    if (is_oldtable_malloced)
        free(oldtable);
    return 0;
}

SetObject *
set_cnew(size_t size,
         size_t (*keyhash)(void *key),
         int (*keycmp)(void *key1, void *key2),
         void * (*keydup)(void *key),
         void (*keyfree)(void *key)) {
    SetObject *sp = (SetObject *)malloc(sizeof(SetObject));
    if (sp == NULL)
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
        sp->table = newtable;
        sp->mask = newsize - 1;
        sp->fill = sp->used = 0;
    } else {
        EMPTY_TO_MINSIZE(sp);
    }
    sp->type = SET;
    sp->keyhash = keyhash ? keyhash : default_keyhash;
    sp->keycmp = keycmp ? keycmp : default_keycmp;
    sp->keydup = keydup ? keydup : default_keydup;
    sp->keyfree = keyfree ? keyfree : free;
    return sp;
}

SetObject *
set_new(void) {
    SetObject *sp = (SetObject *)malloc(sizeof(SetObject));
    if (sp == NULL)
        return NULL;
    EMPTY_TO_MINSIZE(sp);
    sp->type = SET;
    sp->keyhash = default_keyhash;
    sp->keycmp = default_keycmp;
    sp->keydup = default_keydup;
    sp->keyfree = free;
    return sp;
}

void
set_clear(SetObject *sp) {
    SetEntry *ep, *table = sp->table;
    SetEntry small_copy[HASH_MINSIZE];
    size_t table_is_malloced = (table != sp->smalltable);
    size_t fill = sp->fill;
    size_t used = sp->used;
    if (table_is_malloced)
        EMPTY_TO_MINSIZE(sp);
    else if (fill > 0) {
        /* It's a small table with something that needs to be cleared. */
        memcpy(small_copy, table, sizeof(small_copy));
        table = small_copy;
        EMPTY_TO_MINSIZE(sp);
    } else /* else it's a small table that's already empty */
        return;
    for (ep = table; used > 0; ep++) {
        /*only free active key, this is different from thon 2.7*/
        if (ep->key && ep->key != dummy) {
            used--;
            sp->keyfree(ep->key);
        }
    }
    if (table_is_malloced)
        free(table);
}

void
set_free(SetObject *sp) {
    set_clear(sp);
    free(sp);
}

/* make a fresh copy of sp, filtering dummy keys by the way.*/
SetObject *
set_copy(SetObject *sp) {
    SetObject *copy = SET_COPY_INIT(sp);
    if (copy == NULL)
        return NULL;
    SetEntry *ep;
    void *key;
    size_t used = sp->used;
    for (ep = sp->table; used > 0; ep++) {
        if (ep->key && ep->key != dummy) {           /* active key */
            used--;
            if ((key = copy->keydup(ep->key)) == NULL) {
                set_free(copy);
                return NULL;
            }
            set_insert_clean(copy, key, ep->hash);
        }
    }
    return copy;
}

size_t set_len(SetObject *sp) {
    return sp->used;
}

int
set_add(SetObject *sp, void *key) {
    assert(key);
    if (set_insert(sp, key, sp->keyhash(key)) == -1)
        return -1;
    if (NEED_RESIZE(sp))
        return set_resize(sp, RESIZE_NUM(sp));
    return 0;
}

int
set_radd(SetObject *sp, void *key) {
    assert(key);
    size_t hash = sp->keyhash(key);
    SetEntry *ep = set_search(sp, key, hash);
    if (ep->key == NULL) {
        ep->key = key;
        sp->fill++;
        sp->used++;
        ep->hash = hash;
    } else if (ep->key == dummy) {
        ep->key = key;
        sp->used++;
        ep->hash = hash;
    } else if(ep->key != key)
        /*key already exists and its address is different
        from @key's, so free @key*/
        sp->keyfree(key);
    if (NEED_RESIZE(sp))
        return set_resize(sp, RESIZE_NUM(sp));
    return 0;
}

void
set_del(SetObject *sp, void *key) {
    assert(key);
    size_t hash = sp->keyhash(key);
    SetEntry *ep = set_search(sp, key, hash);
    /*only for existing keys*/
    assert(ep->key && ep->key != dummy);
    sp->keyfree(ep->key);
    ep->key = dummy;
    sp->used--;
}

/*silent version of set_del*/
void
set_discard(SetObject *sp, void *key) {
    assert(key);
    size_t hash = sp->keyhash(key);
    SetEntry *ep = set_search(sp, key, hash);
    if (ep->key == NULL || ep->key == dummy)
        return;
    sp->keyfree(ep->key);
    ep->key = dummy;
    sp->used--;
}

/*from other to sp, add keys in other but not in sp */
int
set_update(SetObject *sp, SetObject *other) {
    size_t o_used = other->used;
    if (sp == other || o_used == 0)
        return 0;
    /*if no keys in sp, keys in other should always be added*/
    size_t fast_add = sp->used == 0;
    /* Do one big resize at the start, rather than
     * incrementally resizing as we insert new items.  Expect
     * that there will be no (or few) overlapping keys.
     */
    if ((sp->fill + o_used) * 3 >= (sp->mask + 1) * 2) {
        if (set_resize(sp, (sp->used + o_used) * 2) != 0)
            return -1;
    }
    SetEntry *ep;
    void *key;
    /* now walk other's keys */
    for (ep = other->table; o_used > 0; ep++) {
        key = ep->key;
        if (key && key != dummy) {           /* key in other */
            o_used--;
            if (fast_add || !set_has_intern(sp, key, ep->hash))  /* but key not in sp */
                if (set_insert(sp, key, ep->hash) == -1)
                    return -1;
        }
    }
    return 0;
}

/*return a new allocated set that is the union of sp and other*/
SetObject *
set_union(SetObject *sp, SetObject *other) {
    SetObject *big = BIGGER(sp, other);
    SetObject *sml = SMALLER(sp, other);
    SetObject *result = set_copy(big); /* choose the bigger one as the start set*/
    if (result == NULL)
        return NULL;
    size_t s_used = sml->used;
    if (big == sml || s_used == 0)
        return result;
    if ((result->fill + s_used) * 3 >= (result->mask + 1) * 2) {
        if (set_resize(result, (result->used + s_used) * 2) != 0) {
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
            if (!set_has_intern(result, ep->key, ep->hash)) {  /* but key not in result */
                if ((key = sml->keydup(ep->key)) == NULL) {
                    set_free(result);
                    return NULL;
                }
                /* there's no dummy key in result, sp use this fast way */
                set_insert_clean(result, key, ep->hash);
            }
        }
    }
    return result;
}

/*delete sp's keys that is not in other */
int
set_iand(SetObject *sp, SetObject *other) {
    if (sp == other)
        return 0;
    size_t o_used = other->used;
    if (o_used == 0) {
        set_clear(sp);
        return 0;
    }
    size_t used = sp->used;
    SetEntry *ep;
    void *key;
    size_t fast_del = 0;
    /*walk keys in sp*/
    for (ep = sp->table; used > 0; ep++) {
        key = ep->key;
        if (key && key != dummy) {           /* key in sp */
            used--;
            if (fast_del || !set_has_intern(other, key, ep->hash)) { /* but key not in other */
                sp->keyfree(ep->key);
                ep->key = dummy;
                sp->used--;
            } else if (--o_used == 0)
                /*now, the rest of sp's keys can delete directly*/
                fast_del = 1;
        }
    }
    return 0;
}

/*return the intersection copy of sp and other*/
SetObject *
set_and(SetObject *sp, SetObject *other) {
    if (sp == other)
        return set_copy(sp);
    SetObject *big = BIGGER(sp, other);
    SetObject *sml = SMALLER(sp, other);
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
            if (set_has_intern(big, ep->key, ep->hash)) {    /* key also in big, insert */
                if ((key = sml->keydup(ep->key)) == NULL) {
                    set_free(result);
                    return NULL;
                }
                /* there's no dummy key in result, sp use this fast way */
                set_insert_clean(result, key, ep->hash);
            }
        }
    }
    return result;
}

/*delete sp's keys that is in other*/
int
set_isub(SetObject *sp, SetObject *other) {
    if (sp == other) {
        set_clear(sp);
        return 0;
    }
    size_t o_used = other->used;
    if (o_used == 0)
        return 0;
    size_t used = sp->used;
    SetEntry *ep;
    void *key;
    for (ep = sp->table; used > 0; ep++) {
        key = ep->key;
        if (key && key != dummy) {           /* key in sp */
            used--;
            if (set_has_intern(other, key, ep->hash)) { /* key also in other */
                sp->keyfree(ep->key);
                ep->key = dummy;
                sp->used--;
                if(--o_used == 0)
                    /*all other's keys are checked, no need to go further*/
                    break;
            }
        }
    }
    return 0;
}

/*return the difference set of sp - other*/
SetObject *
set_sub(SetObject *sp, SetObject *other) {
    if (sp == other)
        return set_cnew(HASH_MINSIZE, sp->keyhash, sp->keycmp, sp->keydup,
                        sp->keyfree);
    size_t o_used = other->used;
    if (o_used == 0)
        return set_copy(sp);
    SetObject *result = SET_COPY_INIT(sp);
    if (result == NULL)
        return NULL;
    size_t used = sp->used;
    SetEntry *ep;
    void *key;
    size_t fast_add = 0;
    for (ep = sp->table; used > 0; ep++) {
        if (ep->key && ep->key != dummy) {           /* key in sp */
            used--;
            if (fast_add || !set_has_intern(other, ep->key, ep->hash)) {    /* but key not in other */
                if ((key = sp->keydup(ep->key)) == NULL) {
                    set_free(result);
                    return NULL;
                }
                /* there's no dummy key in result, sp use this faster way */
                set_insert_clean(result, key, ep->hash);
            } else if (--o_used == 0)
                /*now, the rest of sp's keys can add directly*/
                fast_add = 1;
        }
    }
    return result;
}

/*delete sp's keys that is in other, add other's keys that is not in sp to sp*/
int
set_ixor(SetObject *sp, SetObject *other) {
    if (sp == other ) {
        set_clear(sp);
        return 0;
    }
    size_t o_used = other->used;
    if ( o_used == 0)
        return 0;
    size_t used = sp->used;
    if ((sp->fill + o_used) * 3 >= (sp->mask + 1) * 2) {
        if (set_resize(sp, (used + o_used) * 2) != 0)
            return -1;
    }
    SetEntry *ep, *ep2;
    void *key, *key2;
    size_t fast_add = used == 0;
    /*must walk other's keys*/
    for (ep = other->table; o_used > 0; ep++) {
        key = ep->key;
        if (key && key != dummy) {           /* key in other */
            o_used--;
            if (fast_add) { /* but key not in sp */
                if (set_insert(sp, key, ep->hash) == -1)
                    return -1;
            } else {
                ep2 = set_search(sp, key, ep->hash);
                key2 = ep2->key;
                if (key2 == NULL || key2 == dummy) {
                    if ((ep2->key = sp->keydup(key)) == NULL)
                        return -1;
                    sp->used++;
                    ep->hash = ep->hash;
                    if (key2 == NULL)
                        sp->fill++;
                } else { /*key is also in sp*/
                    sp->keyfree(key2);
                    ep2->key = dummy;
                    sp->used--;
                    if (--used == 0)
                        fast_add = 1;
                }
            }
        }
    }
    return 0;
}

/*return a new allocated set that is the symmetric difference of
sp and other. It seems a little complicated to write a similar version
like above functions (set_and, set_sub, set_union), that is:
uses a blank set as initial, and result contains no dummy keys. So here
I reuses set_ixor.*/
SetObject *
set_xor(SetObject *sp, SetObject *other) {
    if (sp == other)
        return set_cnew(HASH_MINSIZE, sp->keyhash, sp->keycmp, sp->keydup,
                        sp->keyfree);
    SetObject *big = BIGGER(sp, other);
    SetObject *sml = SMALLER(sp, other);
    SetObject *result = set_copy(big);
    if (result == NULL)
        return NULL;
    if (set_ixor(result, sml) == -1)
        return NULL;
    return result;
}

size_t
set_has(SetObject *sp, void *key) {
    assert(key);
    return set_has_intern(sp, key, sp->keyhash(key));
}

/* is sp a subset of other? */
size_t
set_issubset(SetObject *sp, SetObject *other) {
    if (sp == other)
        return 1;
    SetEntry *ep;
    size_t used = sp->used;
    for (ep = sp->table; used > 0; ep++) {
        if (ep->key  && ep->key != dummy) {
            used--;
            if (!set_has_intern(other, ep->key, ep->hash))
                return 0;
        }
    }
    return 1;
}

/* is sp a superset of other? */
size_t
set_issuperset(SetObject *sp, SetObject *other) {
    if (sp == other)
        return 1;
    SetEntry *ep;
    size_t used = other->used;
    for (ep = other->table; used > 0; ep++) {
        if (ep->key  && ep->key != dummy) {
            used--;
            if (!set_has_intern(sp, ep->key, ep->hash))
                return 0;
        }
    }
    return 1;
}

int
set_ior(SetObject *sp, SetObject *other) {
    return set_update(sp, other);
}

SetObject *
set_or(SetObject *sp, SetObject *other) {
    return set_union(sp, other);
}

IterObject *
set_iter_new(SetObject *sp) {
    IterObject *sio;
    sio = (IterObject*)malloc(sizeof(IterObject));
    if (sio == NULL)
        return NULL;
    sio->object = (void*)sp;
    sio->inipos = (void*)sp->table;
    sio->rest = sp->used;
    sio->type = SET;
    return sio;
}

size_t
set_iter_walk(IterObject *sio, void **key_addr) {
    SetEntry *ep;
    void *key;
    size_t rest = sio->rest;
    for(ep = (SetEntry*)sio->inipos; rest > 0; ep++) {
        key = ep->key;
        if ( key && key != dummy) {
            sio->rest--;
            sio->inipos = (void*)(ep + 1);
            *key_addr = key;
            return 1;
        }
    }
    return 0;
}

void
set_iter_flush(IterObject *sio) {
    SetObject *sp = (SetObject *)sio->object;
    sio->inipos = (void*)sp->table;
    sio->rest = sp->used;
    sio->type = SET;
}

void
set_addfrom(SetObject *sp, void **keylist, size_t size) {
    size_t i = 0;
    for (; i < size; i++) {
        set_add(sp, (char*)keylist[i]);
    }
}

static void
set_print_intern(SetObject *sp) {
    SetEntry *ep;
    void *key;
    size_t used = sp->used;
    printf("base: member:%u, used:%u, del:%u\n{", sp->mask + 1, sp->used,
           sp->fill - sp->used);
    for(ep = sp->table; used > 0; ep++) {
        key = ep->key;
        if (key && key != dummy) {
            used--;
            printf("'%s', ", (char*)key);
        }
    }
    printf("}\n\n");
}

void
set_print(SetObject *sp) {
    void *key;
    IterObject *sio = iter(sp);
    printf("{");
    while(iterw(sio, &key)) {
        printf("'%s', ", (char*)key);
    }
    free(sio);
    printf("}\n\n");
}

void
set_print_int(SetObject *sp) {
    void *key;
    IterObject *sio = iter(sp);
    printf("{");
    while(iterw(sio, &key)) {
        printf("'%d', ", *(int*)key);
    }
    free(sio);
    printf("}\n\n");
}

SetObject *
set_fromlist(ListObject *lp,
             size_t (*keyhash)(void *key),
             int (*keycmp)(void *key1, void *key2),
             void * (*keydup)(void *key),
             void (*keyfree)(void *key)) {
    SetObject *sp = set_cnew(lp->used, keyhash, keycmp, keydup, keyfree);
    if (sp == NULL)
        return NULL;
    void *key;
    IterObject *lio = iter(lp);
    while(iterw(lio, &key)) {
        set_add(sp, key);
    }
    free(lio);
    return sp;
}
