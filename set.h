
#define SET_COPY_INIT(mp) (set_cnew(\
    (mp)->used,\
    (mp)->keyhash,\
    (mp)->keycmp,\
    (mp)->keydup,\
    (mp)->keyfree));

#define BIGGER(a,b) ((a)->used>=(b)->used?(a):(b))
#define SMALLER(a,b) ((a)->used<(b)->used?(a):(b))

typedef struct {
    size_t hash;
    void *key;
} SetEntry;

typedef struct _setobject SetObject;
struct _setobject {
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

SetObject *
set_cnew(size_t size,
         size_t (*keyhash)(void *key),
         int (*keycmp)(void *key1, void *key2),
         void * (*keydup)(void *key),
         void (*keyfree)(void *key));
SetObject *set_new(void);
void set_clear(SetObject *so);
void set_free(SetObject *so);
SetObject *set_copy(SetObject *so);
SetObject *set_or(SetObject *so, SetObject *other);
SetObject *set_and(SetObject *so, SetObject *other);
SetObject *set_sub(SetObject *so, SetObject *other);
SetObject *set_xor(SetObject *so, SetObject *other);
int set_ior(SetObject *so, SetObject *other);
int set_iand(SetObject *so, SetObject *other);
int set_isub(SetObject *so, SetObject *other);
int set_ixor(SetObject *so, SetObject *other);
size_t set_issuperset(SetObject *so, SetObject *other);
size_t set_subset(SetObject *so, SetObject *other);
int set_add(SetObject *so, void *key);
void set_del(SetObject *so, void *key);
int set_radd(SetObject *so, void *key);
size_t set_len(SetObject *so);
size_t set_has(SetObject *mp, void *key);
void set_print(SetObject *so);
int set_addfrom(SetObject *so, void **keylist, size_t size);
