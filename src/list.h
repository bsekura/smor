#ifndef KERNEL_LIST_H
#define KERNEL_LIST_H

#define list_push_back(head, tail, elem, next, prev) \
    if (!head) { \
        head = elem; \
        tail = elem; \
        elem->next = NULL; \
        elem->prev = NULL; \
    } else { \
        tail->next = elem; \
        elem->prev = tail; \
        elem->next = NULL; \
        tail = elem; \
    }

#define list_push_front(head, tail, elem, next, prev) \
    if (!head) { \
        head = elem; \
        tail = elem; \
        elem->next = NULL; \
        elem->prev = NULL; \
    } else { \
        head->prev = elem; \
        elem->prev = NULL; \
        elem->next = head; \
        head = elem; \
    }

#define list_pop(head, tail, elem, next, prev) \
    if (!elem->prev) \
        head = elem->next; \
    else \
        elem->prev->next = elem->next; \
    if (!elem->next) \
        tail = elem->prev; \
    else \
        elem->next->prev = elem->prev;

#define queue_push_back(head, elem, next, prev) \
    if (!head) { \
        head = elem; \
        elem->next = elem->prev = head; \
    } else { \
        elem->next = head; \
        elem->prev = head->prev; \
        elem->prev->next = elem; \
        head->prev = elem; \
    }

#define queue_insert_before(head, elem, before, next, prev) \
    elem->next = before; \
    elem->prev = before->prev; \
    before->prev->next = elem; \
    before->prev = elem; \
    if (before == head) \
        head = elem;

#define queue_pop(head, elem, next, prev) \
    if (elem->next == elem) { \
        head = NULL; \
    } else { \
        typeof(elem) __next = elem->next; \
        typeof(elem) __prev = elem->prev; \
        __next->prev = __prev; \
        __prev->next = __next; \
        if (head == elem) \
            head = __next; \
    }

#endif // KERNEL_LIST_H
