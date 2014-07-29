
#define LIST_COPY_INIT(lp) (list_cnew(\
    (lp)->used,\
    (lp)->keycmp,\
    (lp)->keydup,\
    (lp)->keyfree));

typedef struct _listobject ListObject;
struct _listobject {
    ObjectType type;
    size_t allocated;
    size_t used;  /* # Active */
    void **table;
    int (*keycmp)(void *key1, void *key2);
    void *(*keydup)(void *key);
    void (*keyfree)(void *key);
};

/* list level functions */
ListObject *
list_cnew(size_t size,
          int (*keycmp)(void *key1, void *key2),
          void * (*keydup)(void *key),
          void (*keyfree)(void *key));
ListObject *list_new(void);
int list_clear(ListObject *lp);
int list_free(ListObject *lp);
ListObject *list_copy(ListObject *lp);
int list_extend(ListObject *lp, ListObject *other);
size_t list_len(ListObject *lp);

/* list level function, return a slice copy or reference */
ListObject *list_slice(ListObject *lp, int start, int stop);
ListObject *list_sslice(ListObject *lp, int start, int stop, int step);
ListObject *list_rslice(ListObject *lp, int start, int stop); /*reference version */
ListObject *list_rsslice(ListObject *lp, int start, int stop, int step);

/* key level functions */
void *list_get(ListObject *lp, int index);
int list_add(ListObject *lp, void *key);
void *list_pop(ListObject *lp);
void *list_popi(ListObject *lp, int index);
int list_insert(ListObject *lp, int index, void *key);
size_t list_index(ListObject *lp, void *key);
size_t list_count(ListObject *lp, void *key);
int list_del(ListObject *lp, int index);
int list_remove(ListObject *lp, void *key);

/*key level functions, passing references instead of a copy.
'r' prefix is short for 'reference'*/
int list_radd(ListObject *lp, void *key);
int list_rinsert(ListObject *lp, int index, void *key);

/*traversal interfaces of ListObject*/
IterObject *list_iter_new(ListObject *lp);
size_t list_iter_walk(IterObject *lio, void **key_addr);
void list_iter_flush(IterObject *lio);

/*other functions for printing or testing*/
void list_print(ListObject *lp);
