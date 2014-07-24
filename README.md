c_data_structures
=================
Frequently-used dynamic data structures written in C.<br/>
Test data file download link:<br/>http://pan.baidu.com/s/18fpnC<br/>
Test: Such as in Windows, Compile xlib.exe, and type "xlib < words > result.txt"<br/>
To do list: list.c, set.c and communication between list, set and dict.<br/><br/>
1. dict.c<br/>
    Currently, the memory pointed by pointer me_value or me_key must be free directly, or you have to provide custom free functions to them. They are passed at the last two arguments in dict_new_custom.<br/>
    Just pass key or value's buffer to functions (such as dict_add, dict_update, dict_force_search), which handle memory management.
