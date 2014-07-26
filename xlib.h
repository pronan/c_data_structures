#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stddef.h>
#include <assert.h>
#include "dict.h"
#include "rbtree.h"
#include "list.h"
#include "set.h"

#ifndef X_DEBUG
    #define X_DEBUG
#endif

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

typedef struct {} DummyStruct;
