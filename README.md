c_data_structures
=================

Frequently-used dynamic data structures written in C.

To do list: list.c, set.c and communication between list, set and dict.<br/><br/>
1. dict.c<br/>
    Currently, the memory pointed by pointer me_value or me_key must be free directly.<br/>
    Just pass key or value's buffer to functions (such as dict_add, dict_update, dict_force_search), which handle memory management.
