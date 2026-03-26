#include "behavior_tree.h"

bt_status_t bt_selector(const bt_node_fn *nodes, size_t count)
{
    for (size_t i = 0; i < count; i++) {
        bt_status_t s = nodes[i]();
        if (s != BT_FAILURE) return s;
    }
    return BT_FAILURE;
}

bt_status_t bt_sequence(const bt_node_fn *nodes, size_t count)
{
    for (size_t i = 0; i < count; i++) {
        bt_status_t s = nodes[i]();
        if (s != BT_SUCCESS) return s;
    }
    return BT_SUCCESS;
}
