#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stddef.h>
#include <assert.h>

#ifndef X_DEBUG
    #define X_DEBUG
#endif

#ifndef SIZE_MAX
    #define SIZE_MAX ((size_t)-1)
#endif

#define SSIZE_T_MAX ((ssize_t)(((size_t)-1)>>1))

#define SSIZE_T_MIN (-SSIZE_T_MAX-1)

/* Mem_MALLOC(0) means malloc(1). Some systems would return NULL
   for malloc(0), which would be treated as an error. Some platforms
   would return a pointer with no memory behind it, which would break
   pymalloc. To solve these problems, allocate an extra byte. */
/* Returns NULL to indicate error if a negative size or size larger than
   ssize_t can represent is supplied.  Helps prevents security holes. */
#define Mem_MALLOC(n) ((size_t)(n)>(size_t)SSIZE_T_MAX?\
                          NULL:malloc((n)?(n):1))

#define Mem_REALLOC(p, n) ((size_t)(n)>(size_t)SSIZE_T_MAX ?\
                             NULL:realloc((p),(n)?(n):1))
#define Mem_FREE free

#define Mem_NEW(type, n) (((size_t)(n)>SSIZE_T_MAX/sizeof(type))?\
                            NULL:((type*)Mem_MALLOC((n)*sizeof(type))))

#define Mem_RESIZE(p, type, n) ((p)=((size_t)(n)>SSIZE_T_MAX/sizeof(type))?\
                                  NULL:(type*) Mem_REALLOC((p),(n)*sizeof(type)))

#define PERTURB_SHIFT 5

#define HASH_MINSIZE 8

#define NEED_RESIZE(op) ((op)->fill * 3 >= ((op)->mask + 1) * 2)

#define RESIZE_NUM(op) (((op)->used > 50000 ? 2 : 4) * (op)->used)

#define INIT_NONZERO_DICT_SLOTS(op) do {                                \
    (op)->table = (op)->smalltable;                               \
    (op)->mask = HASH_MINSIZE - 1;                                 \
    } while(0)

#define EMPTY_TO_MINSIZE(op) do {                                       \
    memset((op)->smalltable, 0, sizeof((op)->smalltable));        \
    (op)->used = (op)->fill = 0;                                  \
    INIT_NONZERO_DICT_SLOTS(op);                                        \
    } while(0)

#define iter(op) ((op)->type==LIST?list_iter_new(op):\
                  (op)->type==DICT?dict_iter_new(op):\
                  (op)->type==SET?set_iter_new(op):\
                  NULL)

#define iterw(iop,keyaddr) ((iop)->type==LIST?list_iter_walk(iop,keyaddr):\
                            (iop)->type==DICT?dict_iter_walk(iop,keyaddr):\
                            (iop)->type==SET?set_iter_walk(iop,keyaddr):\
                            0)

#define iterf(iop) ((iop)->type==LIST?list_iter_flush(iop):\
                    (iop)->type==DICT?dict_iter_flush(iop):\
                    (iop)->type==SET?set_iter_flush(iop):\
                    0)

typedef struct {} DummyStruct;

typedef enum {
    LIST, DICT, SET
} ObjectType;

typedef struct {
    ObjectType type;
    void *object;
    void *inipos;
    size_t rest;
} IterObject;

#include "dict.h"
#include "rbtree.h"
#include "list.h"
#include "set.h"
