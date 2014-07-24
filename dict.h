#define PyDict_MINSIZE 8
#define PERTURB_SHIFT 5
#define NEED_RESIZE(mp) ((mp)->ma_fill * 3 >= ((mp)->ma_mask + 1) * 2)
#define RESIZE_NUM(mp) (((mp)->ma_used > 50000 ? 2 : 4) * (mp)->ma_used)
#define INIT_NONZERO_DICT_SLOTS(mp) do {                                \
    (mp)->ma_table = (mp)->ma_smalltable;                               \
    (mp)->ma_mask = PyDict_MINSIZE - 1;                                 \
    } while(0)

#define EMPTY_TO_MINSIZE(mp) do {                                       \
    memset((mp)->ma_smalltable, 0, sizeof((mp)->ma_smalltable));        \
    (mp)->ma_used = (mp)->ma_fill = 0;                                  \
    INIT_NONZERO_DICT_SLOTS(mp);                                        \
    } while(0)

typedef struct {
    size_t me_hash;
    PyObject *me_key;
    PyObject *me_value;
} PyDictEntry;

typedef struct _dictobject PyDictObject;
struct _dictobject {
    size_t ma_fill;  /* # Active + # Dummy */
    size_t ma_used;  /* # Active */
    size_t ma_mask;
    PyDictEntry *ma_table;
    PyDictEntry ma_smalltable[PyDict_MINSIZE];
    size_t (*ma_keyhash)(PyObject *key);
    int (*ma_keycmp)(PyObject *key1, PyObject *key2);
    PyObject *(*ma_keydup)(PyObject *key);
    PyObject *(*ma_valuedup)(PyObject *value);
    PyObject *(*ma_default)(void);
    void (*ma_keyfree)(PyObject *key);
    void (*ma_valuefree)(PyObject *value);
};

PyDictObject *
dict_new_custom(size_t ma_size,
                size_t (*ma_keyhash)(PyObject *key),
                int (*ma_keycmp)(PyObject *key1, PyObject *key2),
                PyObject * (*ma_keydup)(PyObject *key),
                PyObject * (*ma_valuedup)(PyObject *value),
                PyObject * (*ma_default)(void),
                void (*ma_keyfree)(PyObject *key),
                void (*ma_valuefree)(PyObject *value));
PyDictObject *dict_new(void);
void dict_clear(PyDictObject *mp);
PyObject *dict_search(PyDictObject *mp, PyObject *key);
int dict_add(PyDictObject *mp, PyObject *key, PyObject *value);
int dict_update(PyDictObject *mp, PyObject *key, PyObject *value);
int dict_del(PyDictObject *mp, PyObject *key);
size_t dict_contain(PyDictObject *mp, PyObject *key);
PyObject *dict_force_search(PyDictObject *mp, PyObject *key);
size_t dict_len(PyDictObject *mp);
void dict_print_by_value_desc(PyDictObject *mp);
