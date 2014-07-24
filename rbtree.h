typedef enum {
    BLACK, RED
} rb_color;

typedef struct rbnode {
    rb_color color;
    struct rbnode *p;
    struct rbnode *left;
    struct rbnode *right;
    PyObject *key;
    PyObject *value;
} rbnode;

typedef struct rbtree {
    rbnode *root;
    rbnode *nil;
    size_t size;
    int (*keycmp)(PyObject *key1, PyObject *key2);
    PyObject *(*keydup)(PyObject *key);
    PyObject *(*valuedup)(PyObject *value);
    PyObject *(*valuedefault)(void);
    void (*keyfree)(PyObject *key);
    void (*valuefree)(PyObject *value);
} rbtree;

typedef struct {
    PyObject *key;
    PyObject *value;
} key_value;

rbtree *
rb_cnew(int (*keycmp)(PyObject *key1, PyObject *key2),
        PyObject * (*keydup)(PyObject *key),
        PyObject * (*valuedup)(PyObject *value),
        PyObject * (*valuedefault)(void),
        void (*keyfree)(PyObject *key),
        void (*valuefree)(PyObject *value));
rbtree *rb_new(void) ;
void rb_clear(rbtree *tr) ;
void rb_free(rbtree *tr) ;
PyObject *rb_get(rbtree *tr, PyObject *key) ;
PyObject *rb_fget(rbtree *tr, PyObject *key) ;
int rb_set(rbtree *tr, PyObject *key, PyObject *value) ;
void rb_del(rbtree *tr, PyObject *key) ;
int rb_add(rbtree *tr, PyObject *key, PyObject *value) ;
int rb_update(rbtree *tr, PyObject *key, PyObject *value) ;
rbnode *rb_min(rbtree *tr, rbnode* x) ;
rbnode *rb_max(rbtree *tr, rbnode* x) ;
void rb_print(rbtree *tr);
