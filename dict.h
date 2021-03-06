#define DICT_COPY_INIT(dp) dict_cnew(\
    (dp)->used,\
    (dp)->keyhash,\
    (dp)->keycmp,\
    (dp)->keydup,\
    (dp)->valuedup,\
    (dp)->dvf,\
    (dp)->keyfree,\
    (dp)->valuefree);

typedef struct {
    size_t hash;
    void *key;
    void *value;
} DictEntry;

typedef struct _dictobject DictObject;
struct _dictobject {
    ObjectType type;
    size_t fill;  /* # Active + # Dummy */
    size_t used;  /* # Active */
    size_t mask;
    DictEntry *table;
    DictEntry smalltable[HASH_MINSIZE];
    size_t (*keyhash)(void *key);
    int (*keycmp)(void *key1, void *key2);
    void *(*keydup)(void *key);
    void *(*valuedup)(void *value);
    void *(*dvf)(void);
    void (*keyfree)(void *key);
    void (*valuefree)(void *value);
};

typedef struct {
    ObjectType type;
    DictEntry *inipos;
    size_t rest;
}DictIterObject;

/*dict level functions*/
DictObject *
dict_cnew(size_t size,
          size_t (*keyhash)(void *key),
          int (*keycmp)(void *key1, void *key2),
          void * (*keydup)(void *key),
          void * (*valuedup)(void *value),
          void * (*dvf)(void),
          void (*keyfree)(void *key),
          void (*valuefree)(void *value));
DictObject *dict_new(void);
void dict_clear(DictObject *dp);
void dict_free(DictObject *dp);
int dict_update(DictObject *dp, DictObject *other);
DictObject *dict_copy(DictObject *dp);
size_t dict_len(DictObject *dp);

/*key value level functions. It is suggested that pass
buffered data of @key or @value since most of these functions will
try to make a copy of them. */
void *dict_get(DictObject *dp, void *key);
int dict_set(DictObject *dp, void *key, void *value);
void *dict_fget(DictObject *dp, void *key);
void dict_del(DictObject *dp, void *key);
int dict_add(DictObject *dp, void *key, void *value);
int dict_replace(DictObject *dp, void *key, void *value);
size_t dict_has(DictObject *dp, void *key);

/*key value level functions. 'r' prefix is short for 'reference'.
Assign @key or @value's address directly instead of its copy's.
So it will be dangerous to pass buffered data to these functions
(except the @key of dict_rreplace, which you should pass a buffered data).
*/
int dict_rset(DictObject *dp, void *key, void *value);
int dict_radd(DictObject *dp, void *key, void *value);
int dict_rreplace(DictObject *dp, void *key, void *value);

/*traversal interfaces of DictObject*/
IterObject *dict_iter_new(DictObject *dp);
size_t dict_iter_walk(IterObject *dio, void **key_addr);
void dict_iter_flush(IterObject *dio);

/*special traversal function for dict. Faster to get key and value at the same time*/
size_t dict_iterkv(IterObject *dio, void **key_addr, void **value_addr);

/*other functions for printing or testing*/
void dict_print_by_value_desc(DictObject *dp);
void dict_print(DictObject *dp);
