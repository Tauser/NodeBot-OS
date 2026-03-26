#pragma once

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * behavior_tree — utilitários genéricos de Behavior Tree (E33).
 *
 * Tipos de nó:
 *   Selector : executa filhos em ordem; retorna SUCCESS no primeiro que
 *              não falhe.  Retorna FAILURE se todos falharem.
 *   Sequence : executa filhos em ordem; retorna FAILURE no primeiro que
 *              falhe.  Retorna SUCCESS se todos sucederem.
 *   Leaf     : função simples que implementa condição ou ação.
 *
 * Uso:
 *   static const bt_node_fn root[] = { node_safe_mode, node_talking,
 *                                      node_engaged,   node_idle };
 *   bt_selector(root, 4);
 */

typedef enum {
    BT_SUCCESS = 0,   /* nó completou com êxito   */
    BT_FAILURE,       /* nó não aplicável / falhou */
    BT_RUNNING,       /* nó em execução longa      */
} bt_status_t;

/* Ponteiro para uma função folha */
typedef bt_status_t (*bt_node_fn)(void);

/*
 * bt_selector — executa os nós em ordem até o primeiro SUCCESS ou RUNNING.
 * Retorna BT_FAILURE se todos os nós retornarem BT_FAILURE.
 */
bt_status_t bt_selector(const bt_node_fn *nodes, size_t count);

/*
 * bt_sequence — executa os nós em ordem até o primeiro FAILURE ou RUNNING.
 * Retorna BT_SUCCESS se todos os nós retornarem BT_SUCCESS.
 */
bt_status_t bt_sequence(const bt_node_fn *nodes, size_t count);

#ifdef __cplusplus
}
#endif
