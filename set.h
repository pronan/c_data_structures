
#define SET_COPY_INIT(sp) (set_cnew(\
    (sp)->used,\
    (sp)->keyhash,\
    (sp)->keycmp,\
    (sp)->keydup,\
    (sp)->keyfree));

#define BIGGER(a,b) ((a)->used>=(b)->used?(a):(b))
#define SMALLER(a,b) ((a)->used<(b)->used?(a):(b))

typedef struct {
    size_t hash;
    void *key;
} SetEntry;

typedef struct _setobject SetObject;
struct _setobject {
    ObjectType type;
    size_t fill;  /* # Active + # Dummy */
    size_t used;  /* # Active */
    size_t mask;
    SetEntry *table;
    SetEntry smalltable[HASH_MINSIZE];
    size_t (*keyhash)(void *key);
    int (*keycmp)(void *key1, void *key2);
    void *(*keydup)(void *key);
    void (*keyfree)(void *key);
};

/* set level functions */
SetObject *
set_cnew(size_t size,
         size_t (*keyhash)(void *key),
         int (*keycmp)(void *key1, void *key2),
         void * (*keydup)(void *key),
         void (*keyfree)(void *key));
SetObject *set_new(void);
void set_clear(SetObject *sp);
void set_free(SetObject *sp);
SetObject *set_copy(SetObject *sp);
size_t set_len(SetObject *sp);

/* set level functions, basic operations between two sets.
Note, for set_iand, set_isub and set_ixor,the two sets should never
share any element i.e. the two keys have the same memory address */
SetObject *set_or(SetObject *sp, SetObject *other);
SetObject *set_and(SetObject *sp, SetObject *other);
SetObject *set_sub(SetObject *sp, SetObject *other);
SetObject *set_xor(SetObject *sp, SetObject *other);
SetObject *set_ror(SetObject *sp, SetObject *other);
SetObject *set_rand(SetObject *sp, SetObject *other);
SetObject *set_rsub(SetObject *sp, SetObject *other);
SetObject *set_rxor(SetObject *sp, SetObject *other);
int set_ior(SetObject *sp, SetObject *other);
int set_iand(SetObject *sp, SetObject *other);
int set_isub(SetObject *sp, SetObject *other);
int set_ixor(SetObject *sp, SetObject *other);
size_t set_issuperset(SetObject *sp, SetObject *other);
size_t set_subset(SetObject *sp, SetObject *other);
int set_update(SetObject *sp, SetObject *other); /*same as set_ior */
SetObject *set_union(SetObject *sp, SetObject *other);/*same as set_or */

/*key level functions*/
int set_add(SetObject *sp, void *key);
size_t set_has(SetObject *sp, void *key);
void set_del(SetObject *sp, void *key);
void set_discard(SetObject *sp, void *key);

/*key level functions. 'r' prefix is short for 'reference'.
Assign @key's address directly instead of its copy's.
So it will be dangerous to pass buffered data to these functions
*/
int set_radd(SetObject *sp, void *key);

/*traversal interfaces of SetObject*/
IterObject *set_iter_new(SetObject *sp);
size_t set_iter_walk(IterObject *sio, void **key_addr);
void set_iter_flush(IterObject *sio);

/*other functions for printing or testing*/
void set_print(SetObject *sp);
void set_print_int(SetObject *sp);
void set_addfrom(SetObject *sp, void **keylist, size_t size);

/*communicate between other data structures*/
SetObject *
set_fromlist(ListObject *lp,
             size_t (*keyhash)(void *key),
             int (*keycmp)(void *key1, void *key2),
             void * (*keydup)(void *key),
             void (*keyfree)(void *key));
