#define PyDict_MINSIZE 8
#define PERTURB_SHIFT 5
#define NEED_RESIZE(mp) ((mp)->fill * 3 >= ((mp)->mask + 1) * 2)
#define RESIZE_NUM(mp) (((mp)->used > 50000 ? 2 : 4) * (mp)->used)
#define INIT_NONZERO_DICT_SLOTS(mp) do {                                \
    (mp)->table = (mp)->smalltable;                               \
    (mp)->mask = PyDict_MINSIZE - 1;                                 \
    } while(0)

#define EMPTY_TO_MINSIZE(mp) do {                                       \
    memset((mp)->smalltable, 0, sizeof((mp)->smalltable));        \
    (mp)->used = (mp)->fill = 0;                                  \
    INIT_NONZERO_DICT_SLOTS(mp);                                        \
    } while(0)

typedef struct {
    size_t hash;
    PyObject *key;
    PyObject *value;
} PyDictEntry;

typedef struct _dictobject PyDictObject;
struct _dictobject {
    size_t fill;  /* # Active + # Dummy */
    size_t used;  /* # Active */
    size_t mask;
    PyDictEntry *table;
    PyDictEntry smalltable[PyDict_MINSIZE];
    size_t (*keyhash)(PyObject *key);
    int (*keycmp)(PyObject *key1, PyObject *key2);
    PyObject *(*keydup)(PyObject *key);
    PyObject *(*valuedup)(PyObject *value);
    PyObject *(*valuedefault)(void);
    void (*keyfree)(PyObject *key);
    void (*valuefree)(PyObject *value);
};

PyDictObject *
dict_cnew(size_t size,
          size_t (*keyhash)(PyObject *key),
          int (*keycmp)(PyObject *key1, PyObject *key2),
          PyObject * (*keydup)(PyObject *key),
          PyObject * (*valuedup)(PyObject *value),
          PyObject * (*valuedefault)(void),
          void (*keyfree)(PyObject *key),
          void (*valuefree)(PyObject *value));
PyDictObject *dict_new(void);
void dict_clear(PyDictObject *mp);
void dict_free(PyDictObject *mp);
PyObject *dict_get(PyDictObject *mp, PyObject *key);
PyObject *dict_fget(PyDictObject *mp, PyObject *key);
int dict_set(PyDictObject *mp, PyObject *key, PyObject *value);
void dict_del(PyDictObject *mp, PyObject *key);
int dict_add(PyDictObject *mp, PyObject *key, PyObject *value);
int dict_update(PyDictObject *mp, PyObject *key, PyObject *value);
size_t dict_len(PyDictObject *mp);
void dict_print_by_value_desc(PyDictObject *mp);
