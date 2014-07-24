#include "xlib.h"

static int
default_keycmp(PyObject *key1, PyObject *key2) {
    return strcmp((char *)key1, (char *)key2);
}

static PyObject *
default_keydup(PyObject *key) {
    return (PyObject *)strdup((char *)key);
}

static PyObject *
default_valuedup(PyObject *_value) {
    size_t *value = (size_t*)malloc(sizeof(size_t));
    *value = *(size_t *)_value;
    return (PyObject *)value;
}

static PyObject *
default_valuedefault(void) {
    size_t *value = (size_t*)malloc(sizeof(size_t));
    *value = 0;
    return (PyObject *)value;
}

static rbnode *
rbnode_new(rbtree *tr, PyObject *key, PyObject *value) {
    rbnode *nd = (rbnode *) malloc(sizeof(rbnode));
    if (nd == NULL)
        return NULL;
    memset(nd, 0, sizeof(nd));
    if (key) {
        if ((nd->key = tr->keydup(key)) == NULL) {
            free(nd);
            return NULL;
        }
    }
    if (value) {
        if ((nd->value = tr->valuedup(value)) == NULL) {
            free(nd->key);
            free(nd);
            return NULL;
        }
    }
    return nd;
}

static rbnode *
rbnode_fnew(rbtree *tr, PyObject *key) {
    assert(key);
    rbnode *nd = (rbnode *) malloc(sizeof(rbnode));
    if (nd == NULL)
        return NULL;
    memset(nd, 0, sizeof(nd));
    if ((nd->key = tr->keydup(key)) == NULL) {
        free(nd);
        return NULL;
    }
    if ((nd->value = tr->valuedefault()) == NULL) {
        free(nd->key);
        free(nd);
        return NULL;
    }
    return nd;
}

static void
rbnode_clear(rbtree *tr, rbnode *nd) {
    tr->keyfree(nd->key);
    tr->valuefree(nd->value);
    free(nd);
}

static void
_rb_clear(rbtree *tr, rbnode *nd) {
    if (nd != tr->nil) {
        _rb_clear(tr, nd->left);
        _rb_clear(tr, nd->right);
        rbnode_clear(tr, nd);
    }
}

static void
left_rotate(rbtree *tr, rbnode *x) {
    rbnode *y = x->right;
    x->right = y->left;
    if (y->left != tr->nil)
        y->left->p = x;
    y->p = x->p;
    if (x == tr->root)
        tr->root = y;
    else if ( x == x->p->left)
        x->p->left = y;
    else
        x->p->right = y;
    y->left = x;
    x->p = y;
}

static void
right_rotate(rbtree *tr, rbnode *x) {
    rbnode *y = x->left;
    x->left = y->right;
    if (y->right != tr->nil)
        y->right->p = x;
    y->p = x->p;
    if (x == tr->root)
        tr->root = y;
    else if ( x == x->p->right )
        x->p->right = y;
    else
        x->p->left = y;
    y->right = x;
    x->p = y;
}

static rbnode *
rbnode_get(rbtree *tr, PyObject *key) {
    rbnode *x = tr->root;
    int r;
    while (x != tr->nil) {
        r = tr->keycmp(key, x->key);
        if (r == 0)
            return x;
        else if (r < 0)
            x = x->left;
        else
            x = x->right;
    }
    return NULL;
}

static void
rb_add_fixup(rbtree *tr, rbnode *z) {
    rbnode *y;
    while (z->p->color == RED) {
        if (z->p == z->p->p->left) {
            y = z->p->p->right;
            if (y->color == RED) {
                z->p->color = BLACK;
                y->color = BLACK;
                z->p->p->color = RED;
                z = z->p->p;
            } else {
                if (z == z->p->right) {
                    z = z->p;
                    left_rotate(tr, z);
                }
                z->p->color = BLACK;
                z->p->p->color = RED;
                right_rotate(tr, z->p->p);
            }
        } else {
            y = z->p->p->left;
            if (y->color == RED) {
                z->p->color = BLACK;
                y->color = BLACK;
                z->p->p->color = RED;
                z = z->p->p;
            } else {
                if (z == z->p->left) {
                    z = z->p;
                    right_rotate(tr, z);
                }
                z->p->color = BLACK;
                z->p->p->color = RED;
                left_rotate(tr, z->p->p);
            }
        }
    }
    tr->root->color = BLACK;
}

static void
rb_transplant(rbtree *tr, rbnode *u, rbnode *v) {
    if (u->p == tr->nil)
        tr->root = v;
    else if (u == u->p->left)
        u->p->left = v;
    else
        u->p->right = v;
    v->p = u->p;
}

static void
rb_del_fixup(rbtree *tr, rbnode *x) {
    rbnode *w;
    while (x != tr->root && x->color == BLACK) {
        if (x == x->p->left) {
            w = x->p->right;
            if (w->color == RED) {
                w->color = BLACK;
                x->p->color = RED;
                left_rotate(tr, x->p);
                w = x->p->right;
            }
            if (w->left->color == BLACK && w->right->color == BLACK) {
                w->color = RED;
                x = x->p;
            } else {
                if (w->right->color == BLACK) {
                    w->left->color = BLACK;
                    w->color = RED;
                    right_rotate(tr, w);
                    w = x->p->right;
                }
                w->color = x->p->color;
                w->p->color = BLACK;
                w->right->color = BLACK;
                left_rotate(tr, x->p);
                x = tr->root;
            }
        } else {
            w = x->p->left;
            if (w->color == RED) {
                w->color = BLACK;
                x->p->color = RED;
                right_rotate(tr, x->p);
                w = x->p->left;
            }
            if (w->right->color == BLACK && w->left->color == BLACK) {
                w->color = RED;
                x = x->p;
            } else {
                if (w->left->color == BLACK) {
                    w->right->color = BLACK;
                    w->color = RED;
                    left_rotate(tr, w);
                    w = x->p->left;
                }
                w->color = x->p->color;
                w->p->color = BLACK;
                w->left->color = BLACK;
                right_rotate(tr, x->p);
                x = tr->root;
            }
        }
    }
    x->color = BLACK;
}

static void
print_by_key_asce(rbtree *tr, rbnode *nd) {
    if (nd != tr->nil) {
        print_by_key_asce(tr, nd->left);
        printf("%s\t%u\n", (char*)nd->key, *(size_t*)nd->value);
        print_by_key_asce(tr, nd->right);
    }
}

rbtree *
rb_cnew(int (*keycmp)(PyObject *key1, PyObject *key2),
        PyObject * (*keydup)(PyObject *key),
        PyObject * (*valuedup)(PyObject *value),
        PyObject * (*valuedefault)(void),
        void (*keyfree)(PyObject *key),
        void (*valuefree)(PyObject *value)) {
    rbtree *tr = (rbtree *) malloc(sizeof(rbtree));
    if (tr == NULL)
        return NULL;
    rbnode *nil = rbnode_new(tr, 0, 0);
    if (nil == NULL) {
        free(tr);
        return NULL;
    }
    tr->nil = nil;
    tr->root = nil;
    tr->size = 0;
    tr->keycmp = keycmp ? keycmp : default_keycmp;
    tr->keydup = keydup ? keydup : default_keydup;
    tr->valuedup = valuedup ? valuedup : default_valuedup;
    tr->valuedefault = valuedefault ? valuedefault : default_valuedefault;
    tr->keyfree = keyfree ? keyfree : free;
    tr->valuefree = valuefree ? valuefree : free;
    return tr;
}

rbtree *
rb_new(void) {
    rbtree *tr = (rbtree *) malloc(sizeof(rbtree));
    if (tr == NULL)
        return NULL;
    rbnode *nil = rbnode_new(tr, 0, 0);
    if (nil == NULL) {
        free(tr);
        return NULL;
    }
    tr->nil = nil;
    tr->root = nil;
    tr->size = 0;
    tr->keycmp = default_keycmp;
    tr->keydup = default_keydup;
    tr->valuedup = default_valuedup;
    tr->valuedefault = default_valuedefault;
    tr->keyfree = free;
    tr->valuefree = free;
    return tr;
}

void
rb_clear(rbtree *tr) {
    _rb_clear(tr, tr->root);
    tr->root = tr->nil;
    tr->size = 0;
}

void
rb_free(rbtree *tr) {
    _rb_clear(tr, tr->root);
    rbnode_clear(tr, tr->nil);
    free(tr);
}

PyObject *
rb_get(rbtree *tr, PyObject *key) {
    rbnode *nd = rbnode_get(tr, key);
    return nd ? nd->value : NULL;
}

PyObject *
rb_fget(rbtree *tr, PyObject *key) {
    rbnode *y = tr->nil;
    rbnode *x = tr->root;
    int r;
    while (x != tr->nil) {
        r = tr->keycmp(key, x->key);
        if (r == 0)
            return x->value;
        y = x;
        if (r < 0)
            x = x->left;
        else
            x = x->right;
    }
    rbnode *z = rbnode_fnew(tr, key);
    if (z == NULL)
        return NULL;
    rbnode *original_z = z;
    z->p = y;
    if (y == tr->nil)
        tr->root = z;
    else if (tr->keycmp(key, y->key) < 0)
        y->left = z;
    else
        y->right = z;
    z->left = tr->nil;
    z->right = tr->nil;
    z->color = RED;
    rb_add_fixup(tr, z);
    tr->size++;
    return original_z->value;
}

int
rb_set(rbtree *tr, PyObject *key, PyObject *value) {
    assert(key);
    assert(value);
    PyObject *oldvalue;
    rbnode *y = tr->nil;
    rbnode *x = tr->root;
    int r;
    while (x != tr->nil) {
        r = tr->keycmp(key, x->key);
        if (r == 0) {
            oldvalue = x->value;
            if ((x->value = tr->valuedup(value)) == NULL)
                return EXIT_FAILURE;
            tr->valuefree(oldvalue);
            return EXIT_SUCCESS;
        }
        y = x;
        if (r < 0)
            x = x->left;
        else
            x = x->right;
    }
    rbnode *z = rbnode_new(tr, key, value);
    if (z == NULL)
        return EXIT_FAILURE;
    z->p = y;
    if (y == tr->nil)
        tr->root = z;
    else if (tr->keycmp(key, y->key) < 0)
        y->left = z;
    else
        y->right = z;
    z->left = tr->nil;
    z->right = tr->nil;
    z->color = RED;
    rb_add_fixup(tr, z);
    tr->size++;
    return EXIT_SUCCESS;
}

void
rb_del(rbtree *tr, PyObject *key) {
    assert(key);
    rbnode *z = rbnode_get(tr, key);
    assert(z);
    rbnode *y = z;
    rb_color y_original_color = y->color;
    rbnode *x;
    if (z->left == tr->nil) {
        x = z->right;
        rb_transplant(tr, z, z->right);
    } else if (z->right == tr->nil) {
        x = z->left;
        rb_transplant(tr, z, z->left);
    } else {
        y = rb_min(tr, z->right);
        y_original_color = y->color;
        x = y->right;
        if (y->p == z)
            x->p = y;
        else {
            rb_transplant(tr, y, y->right);
            y->right = z->right;
            y->right->p = y;
        }
        rb_transplant(tr, z, y);
        y->left = z->left;
        y->left->p = y;
        y->color = z->color;
    }
    if (y_original_color == BLACK)
        rb_del_fixup(tr, x);
    rbnode_clear(tr, z);
    tr->size--;
}

int
rb_add(rbtree *tr, PyObject *key, PyObject *value) {
    assert(key);
    assert(value);
    rbnode *z = rbnode_new(tr, key, value);
    if (z == NULL)
        return EXIT_FAILURE;
    rbnode *y = tr->nil;
    rbnode *x = tr->root;
    while (x != tr->nil) {
        y = x;
        if (tr->keycmp(z->key, x->key) < 0)
            x = x->left;
        else
            x = x->right;
    }
    z->p = y;
    if (y == tr->nil)
        tr->root = z;
    else if (tr->keycmp(z->key, y->key) < 0)
        y->left = z;
    else
        y->right = z;
    z->left = tr->nil;
    z->right = tr->nil;
    z->color = RED;
    rb_add_fixup(tr, z);
    tr->size++;
    return EXIT_SUCCESS;
}

int
rb_update(rbtree *tr, PyObject *key, PyObject *value) {
    rbnode *nd = rbnode_get(tr, key);
    if (nd == NULL)
        return EXIT_FAILURE;
    PyObject *oldvalue = nd->value;
    if ((nd->value = tr->valuedup(value)) == NULL)
        return EXIT_FAILURE;
    tr->valuefree(oldvalue);
    return EXIT_SUCCESS;
}

rbnode *
rb_min(rbtree *tr, rbnode* x) {
    if (x == NULL)
        x = tr->root;
    while (x->left != tr->nil)
        x = x->left;
    return x;
}

rbnode *
rb_max(rbtree *tr, rbnode* x) {
    if (x == NULL)
        x = tr->root;
    while (x->right != tr->nil)
        x = x->right;
    return x;
}

void
rb_print(rbtree *tr) {
    print_by_key_asce(tr, tr->root);
}
