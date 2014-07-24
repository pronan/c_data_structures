#include "xlib.h"

void test_dict(void){
    PyDictObject *mp = dict_cnew(10000, 0, 0, 0, 0, 0, 0, 0);
    //PyDictObject *mp = dict_new();
    char keybuf[100];
    size_t valuebuf[] = { 1 };
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
    *valuebuf=123456789;
    dict_add(mp,"the",valuebuf);
    *valuebuf=145678999;
    dict_set(mp,"xxx",valuebuf);
    dict_del(mp, "and");
//    dict_del(mp, "of");
    dict_print_by_value_desc(mp);
    dict_clear(mp);
    dict_clear(mp); //just for test
    dict_free(mp);
}

void test_rb(void){
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
    *valuebuf=123456789;
    rb_add(tr,"the",valuebuf);
    *valuebuf=1234567899;
    rb_set(tr,"aaaaaa",valuebuf);
    rb_set(tr, "and",valuebuf);
    rb_del(tr, "a");
    rb_print(tr);
    rb_clear(tr);
    rb_clear(tr); //just for test
    rb_free(tr);
}

/*scan words from stdin, print total amount for each word by DESC order*/
int main(void) {
    test_rb();
    return 0;
}
