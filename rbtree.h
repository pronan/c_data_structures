#define MAX_HEIGHT 50

typedef enum {
    BLACK, RED
} rbcolor;

typedef struct rbnode {
    rbcolor color;
    struct rbnode *p;
    struct rbnode *left;
    struct rbnode *right;
    void *key;
    void *value;
} rbnode;

typedef struct rbtree {
    rbnode *root;
    rbnode *nil;
    size_t size;
    int (*keycmp)(void *key1, void *key2);
    void *(*keydup)(void *key);
    void *(*valuedup)(void *value);
    void *(*dvf)(void);
    void (*keyfree)(void *key);
    void (*valuefree)(void *value);
} rbtree;

typedef struct {
    void *key;
    void *value;
} key_value;

/* tree level functions */
rbtree *
rb_cnew(int (*keycmp)(void *key1, void *key2),
        void * (*keydup)(void *key),
        void * (*valuedup)(void *value),
        void * (*dvf)(void),
        void (*keyfree)(void *key),
        void (*valuefree)(void *value));
rbtree *rb_new(void) ;
void rb_clear(rbtree *tr) ;
void rb_free(rbtree *tr) ;

/* node level functions */
void *rb_get(rbtree *tr, void *key) ;
void *rb_fget(rbtree *tr, void *key) ;
int rb_set(rbtree *tr, void *key, void *value) ;
int rb_add(rbtree *tr, void *key, void *value) ;
int rb_update(rbtree *tr, void *key, void *value) ;
void rb_del(rbtree *tr, void *key) ;
rbnode *rb_min(rbtree *tr, rbnode* x) ;
rbnode *rb_max(rbtree *tr, rbnode* x) ;

/*node level functions, passing references instead of a copy.
'r' prefix is short for 'reference'*/
int rb_rset(rbtree *tr, void *key, void *value) ;
int rb_radd(rbtree *tr, void *key, void *value) ;
int rb_rupdate(rbtree *tr, void *key, void *value) ;

/* classical non-recursive traversal functions of binary tree */
int rb_prewalk(rbtree *tr, void (*nodef)(rbnode *nd));
int rb_inwalk(rbtree *tr, void (*nodef)(rbnode *nd));
int rb_postwalk(rbtree *tr, void (*nodef)(rbnode *nd));

/*other functions for printing or testing*/
void rb_print(rbtree *tr);
