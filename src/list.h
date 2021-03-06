#ifndef OTRV4_LIST_H
#define OTRV4_LIST_H

#include <stdlib.h>

#include "shared.h"

typedef struct _list_element {
  void *data;
  struct _list_element *next;
} list_element_t;

INTERNAL void otrv4_list_foreach(list_element_t *head,
                                 void (*fn)(list_element_t *node,
                                            void *context),
                                 void *context);

// Free list and invoke fn to free the nodes' data
INTERNAL void otrv4_list_free(list_element_t *head, void (*fn)(void *data));

// Free list and invoke "free()" to free the nodes' data
INTERNAL void otrv4_list_free_full(list_element_t *head);

// Free list but does not free the nodes' data
INTERNAL void otrv4_list_free_nodes(list_element_t *head);

INTERNAL list_element_t *otrv4_list_add(void *data, list_element_t *head);

INTERNAL list_element_t *otrv4_list_get_last(list_element_t *head);

INTERNAL list_element_t *
otrv4_list_get(const void *wanted, list_element_t *head,
               int (*fn)(const void *current, const void *wanted));

INTERNAL list_element_t *otrv4_list_get_by_value(const void *wanted,
                                                 list_element_t *head);

INTERNAL list_element_t *otrv4_list_remove_element(const list_element_t *wanted,
                                                   list_element_t *head);

INTERNAL size_t otrv4_list_len(list_element_t *head);

#ifdef OTRV4_LIST_PRIVATE

tstatic list_element_t *list_new(void);

#endif

#endif
