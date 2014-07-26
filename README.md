c_data_structures
=================
Frequently-used dynamic data structures written in C.<br/>
Test data file download link: http://pan.baidu.com/s/18fpnC<br/>
Test: Such as in Windows, Compile xlib.exe, and type "xlib < words > result.txt"<br/>
To do list: list.c, set.c and communication between list, set and dict.<br/><br/>
1. dict.c<br/>
    By default, the memory pointed by pointer value or key of PyDictEntry must be free directly, or you have to provide custom free functions. They are passed at the last two arguments in dict_cnew.<br/>
    Just pass key or value's buffer to functions (such as dict_set, dict_get, dict_fget), they know how to handle memory management.<br/><br/>
2. rbtree.c<br/>
    Red black tree data structure. Functions or memory management are almost the same as dict, but rbtree store keys in order, so it is prefered when keys' order matters.<br/><br/>
3.set.c<br/>
    The hash algorithms is the same as dict.c. set offers basic operations between two sets, i.e. issubset, issuperset, intersection, difference, union and symmetric difference. Also there are key level functions:add, del and has. They are very fast.<br/><br/>
