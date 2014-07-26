#define DICT_COPY_INIT(mp) dict_cnew(\
    (mp)->used,\
    (mp)->keyhash,\
    (mp)->keycmp,\
    (mp)->keydup,\
    (mp)->valuedup,\
    (mp)->valuedefault,\
    (mp)->keyfree,\
    (mp)->valuefree);

typedef struct {
    size_t hash;
    void *key;
    void *value;
} DictEntry;

typedef struct _dictobject DictObject;
struct _dictobject {
    size_t fill;  /* # Active + # Dummy */
    size_t used;  /* # Active */
    size_t mask;
    DictEntry *table;
    DictEntry smalltable[HASH_MINSIZE];
    size_t (*keyhash)(void *key);
    int (*keycmp)(void *key1, void *key2);
    void *(*keydup)(void *key);
    void *(*valuedup)(void *value);
    void *(*valuedefault)(void);
    void (*keyfree)(void *key);
    void (*valuefree)(void *value);
};

DictObject *
dict_cnew(size_t size,
          size_t (*keyhash)(void *key),
          int (*keycmp)(void *key1, void *key2),
          void * (*keydup)(void *key),
          void * (*valuedup)(void *value),
          void * (*valuedefault)(void),
          void (*keyfree)(void *key),
          void (*valuefree)(void *value));
DictObject *dict_new(void);
void dict_clear(DictObject *mp);
void dict_free(DictObject *mp);
int dict_update(DictObject *mp, DictObject *other);
DictObject *dict_copy(DictObject *mp);
void *dict_get(DictObject *mp, void *key);
int dict_set(DictObject *mp, void *key, void *value);
void *dict_fget(DictObject *mp, void *key);
void dict_del(DictObject *mp, void *key);
int dict_add(DictObject *mp, void *key, void *value);
int dict_update_value(DictObject *mp, void *key, void *value);
int dict_rset(DictObject *mp, void *key, void *value);
int dict_radd(DictObject *mp, void *key, void *value);
int dict_rupdate_value(DictObject *mp, void *key, void *value);
size_t dict_has(DictObject *mp, void *key);
size_t dict_len(DictObject *mp);
void dict_print_by_value_desc(DictObject *mp);
