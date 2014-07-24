#include "xlib.h"

/*scan words from stdin, print total amount for each word by DESC order*/
int main(void) {
    PyDictObject *mp = dict_new_custom(10000, 0, 0, 0, 0, 0, 0, 0);
//    PyDictObject *mp = dict_new();
    char keybuf[100];
    size_t valuebuf[] = { 1 };
    size_t *vp;
    while (fscanf(stdin, "%s", keybuf) == 1) {
        if (dict_contain(mp, keybuf)) {
            vp = dict_search(mp, keybuf);
            *vp += 1;
        } else
            dict_add(mp, keybuf, valuebuf);
    }
//    this is another faster version
//    while (fscanf(stdin, "%s", keybuf) == 1) {
//        vp = dict_force_search(mp, keybuf);
//        *vp += 1;
//    }

//    other test functions 
//    dict_del(mp, "the");
//    *valuebuf=123456789;
//    dict_add(mp,"the",valuebuf);
//    *valuebuf=1456789;
//    dict_update(mp,"we",valuebuf);
//    dict_del(mp, "and");
//    dict_del(mp, "of");
    dict_print_by_value_desc(mp);
    dict_clear(mp);
    dict_clear(mp); //duplicate clear just for test
    free(mp);
    return 0;
}
