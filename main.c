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
    //dict_print_by_value_desc(copy);
    //fprintf(stdout, "===above is copy===\n");
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
    //dict_print_by_value_desc(mp);
    dict_print(mp);
    void *key, *value;
    IterObject *dio = iter(mp);
    printf("\nwalk again...\n");
    while(iterw(dio, &key)) {
        value = dict_get(mp, key);
        fprintf(stdout, "%s\t%u\n", (char*)key, *(size_t*)value);
    }
    *valuebuf = 9888888;
    dict_set(mp, "emacs", valuebuf);
    iterf(dio);
    printf("\nwalk again 2...\n");
    while(iterw(dio, &key)) {
        value = dict_get(mp, key);
        fprintf(stdout, "%s\t%u\n", (char*)key, *(size_t*)value);
    }
    free(dio);
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

void test_list(void) {
    int valuebuf[] = { 1 };
    size_t i;
    ListObject *lp = list_new();
    assert(list_is_empty(lp));
    for(i = 0; i < 10; i++) {
        *valuebuf = i;
        list_add(lp, valuebuf);
    }
    list_print(list_rsslice(lp, 0, 10, 1));
    list_print(list_sslice(lp, 9, -11, -1));
    list_print(list_rsslice(lp, -1, -1, -1));
    list_print(list_rsslice(lp, -1, -110, -1));
    list_print(list_rsslice(lp, -1, -11, -2));
    list_print(list_sslice(lp, -1, -3, -1));
    list_print(list_sslice(lp, -3, -1, 1));
    list_print(list_sslice(lp, -1, 7, -1));
    printf("get: %d\n", *(int*)list_get(lp, 2));
    printf("pop: %d\n", *(int*)list_popi(lp, 0));
    *valuebuf = -2;
    list_insert(lp, 0, valuebuf);
    *valuebuf = -200;
    list_insert(lp, -1, valuebuf);
    list_print(lp);
    list_del(lp, 0);
    list_rinsert(lp, -1, valuebuf);
    list_rinsert(lp, -1, valuebuf);
    printf("get: %d\n", *(int*)list_get(lp, 0));
    list_print(lp);
    ListObject *nlp = list_copy(lp);
    printf("new copy's size is %u, allocated is %u\n", nlp->used, nlp->allocated);
    list_print(nlp);
    printf("count %d: %d\n", *valuebuf, list_count(lp, valuebuf));
    //list_clear(lp);
    //assert(list_is_empty(lp));
    //assert(lp->table==NULL);
    list_free(nlp);
    printf("MAX size is : %u, MAX int is :%d, MIN int is: %d\n", SIZE_MAX,
           SSIZE_T_MAX, SSIZE_T_MIN);
}

size_t
int_hash(void *key) {
    int n = *(int*)key;
    return (size_t)n;
}

void
test_communicate() {
    int valuebuf[] = { 1 };
    size_t i;
    ListObject *lp = list_new();
    for(i = 0; i < 10; i++) {
        *valuebuf = i;
        list_add(lp, valuebuf);
    }
    ListObject *lp2 = list_new();
    for(i = 5; i < 15; i++) {
        *valuebuf = i;
        list_add(lp2, valuebuf);
    }
    printf("list1 is:\n");
    list_print(lp);
    printf("list2 is:\n");
    list_print(lp2);
    SetObject *sp = set_fromlist(lp, int_hash, lp->keycmp, lp->keydup, 0);
    printf("set from  list1 is:\n");
    set_print_int(sp);
    SetObject *sp2 = set_fromlist(lp2, int_hash, lp2->keycmp, lp2->keydup, 0);
    printf("set from  list2 is:\n");
    set_print_int(sp2);
    printf("so, intersection of the two list is\n");
    set_print_int(set_and(sp, sp2));
    list_free(lp);
    list_free(lp2);
    set_free(sp);
    set_free(sp2);
}

/*scan words from stdin, print total amount for each word by DESC order*/
int main(void) {
    test_communicate();
    return 0;
}
