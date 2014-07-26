#include "xlib.h"

static void
test_dict(void) {
    DictObject *mp = dict_cnew(10000, 0, 0, 0, 0, 0, 0, 0);
    //DictObject *mp = dict_new();
    char keybuf[100];
    size_t valuebuf[] = { 1 };
    dict_add(mp, "a", valuebuf);
    dict_del(mp, "a");
    dict_add(mp, "b", valuebuf);
    dict_del(mp, "b");
    dict_add(mp, "c", valuebuf);
    dict_del(mp, "c");
    dict_add(mp, "d", valuebuf);
    dict_del(mp, "d");
    dict_add(mp, "e", valuebuf);
    dict_del(mp, "e");
    dict_add(mp, "xffff", valuebuf);
    *valuebuf = 123456789;
    dict_set(mp, "test", valuebuf);
    DictObject *copy = dict_copy(mp);
    dict_print_by_value_desc(copy);
    fprintf(stdout, "===above is copy===\n");
    dict_free(copy);
    *valuebuf = 1;
    size_t *vp;
    while (fscanf(stdin, "%s", keybuf) == 1) {
        vp = dict_get(mp, keybuf);
        if (vp)
            *vp += 1;
        else
            dict_add(mp, keybuf, valuebuf);
    }
//    this is another faster version
//    while (fscanf(stdin, "%s", keybuf) == 1) {
//        vp = dict_fget(mp, keybuf);
//        *vp += 1;
//    }
    dict_del(mp, "the");
    *valuebuf = 123456789;
    dict_add(mp, "the", valuebuf);
    *valuebuf = 145678999;
    dict_set(mp, "xxx", valuebuf);
    dict_del(mp, "xxx");
    dict_del(mp, "of");
    dict_del(mp, "it");
    DictObject *mp2 = dict_new();
    *valuebuf = 99999999;
    dict_set(mp2, "xiangnan", valuebuf);
    *valuebuf = 89999999;
    dict_add(mp2, "laoma", valuebuf);
    dict_update(mp, mp2);
    dict_free(mp2);
    dict_print_by_value_desc(mp);
    dict_clear(mp);
    dict_clear(mp); //just for test
    dict_free(mp);
}

static void
test_rb(void) {
    //rbtree *tr = rb_cnew( 0, 0, 0, 0, 0, 0);
    rbtree *tr = rb_new();
    char keybuf[100];
    size_t valuebuf[] = { 1 };
    size_t *vp;
    while (fscanf(stdin, "%s", keybuf) == 1) {
        vp = rb_get(tr, keybuf);
        if (vp)
            *vp += 1;
        else
            rb_add(tr, keybuf, valuebuf);
    }
//    this is another faster version
//    while (fscanf(stdin, "%s", keybuf) == 1) {
//        vp = rb_fget(tr, keybuf);
//        *vp += 1;
//    }
    rb_del(tr, "the");
    *valuebuf = 123456789;
    rb_add(tr, "the", valuebuf);
    *valuebuf = 1234567899;
    rb_set(tr, "aaaaaa", valuebuf);
    rb_set(tr, "and", valuebuf);
    *valuebuf = 987654321;
    //rb_update(tr,"and",valuebuf);
    rb_del(tr, "aaaaaa");
    rb_print(tr);
    rb_clear(tr);
    rb_clear(tr); //just for test
    rb_free(tr);
}

static void
test_set(void) {
    char *keys1[] = {"a", "b", "r", "a", "c", "a", "d", "a", "b", "r", "a"};
    char *keys2[] = {"a", "l", "a", "c", "a", "z", "a", "m"};
    SetObject *a = set_new();
    SetObject *b = set_new();
    set_addfrom(a, (void**)keys1, 11);
    set_addfrom(b, (void**)keys2, 8);
    set_print(a); // { 'c', 'd', 'r', 'a', 'b', }
    set_print(b); // { 'c', 'l', 'm', 'a', 'z', }
    set_print(set_sub(a, b)); //{ 'd', 'b', 'r', }
    set_print(set_or(a, b)); //{ 'a', 'b', 'c', 'd', 'l', 'm', 'r', 'z', }
    set_print(set_and(a, b)); //{ 'c', 'a', }
    set_print(set_xor(a, b)); //{ 'b', 'd', 'l', 'm', 'r', 'z', }
    set_ior(a, b);
    set_print(a);
    set_isub(a, b);
    set_print(a);
    set_ixor(a, b);
    set_print(a);
    set_iand(a, b);
    set_print(a);
    set_free(a);
    set_free(b);
}

/*scan words from stdin, print total amount for each word by DESC order*/
int main(void) {
    test_set();
    return 0;
}
