//
// Created by student on 6/15/23.
//

#ifndef OS_HW3_QUEUE_H
#define OS_HW3_QUEUE_H

#include "segel.h"

typedef struct {
    int fd;
    struct timeval arrival;
    struct timeval dispatch;
} request;

typedef struct Node {
    request data;
    struct Node* prev;
    struct Node* next;
} node;

typedef struct {
    int size;
    node* first;
    node* last;
} Queue;

Queue* create_Queue() {
    Queue* res = (Queue*)malloc(sizeof(Queue));
    if (res == NULL) {
        printf("Queue: allocation error!\n");
    }
    res->size = 0;
    res->first = NULL;
    res->last = NULL;
    return res;
};

void delete_Queue(Queue* q) {
    while (q->first != NULL){
        node* to_delete = q->first;
        q->first = to_delete->next;
        free (to_delete);
    }
    free (q);
}

void add_to_Queue (Queue* q, request req) {
    node* new_node = (node*)malloc(sizeof(node));
    new_node->data = req;
    new_node->prev = q->last;
    new_node->next = NULL;

    if (q->first==NULL){
#ifdef DEBUG_PRINT
        assert(q->size==0);
#endif
        q->first=new_node;
    }

    if (q->last != NULL){
        q->last->next = new_node;
    }

    q->last = new_node;
    q->size++;
}

request remove_first (Queue* q) {
#ifdef DEBUG_PRINT
    assert(q->size>0);
    if (q->size==0){
        printf("\nsize 0!!!\n\n");
    }
#endif
//    if (q->size == 0) {
//        return -1;
//    }
    node* node_to_remove = q->first;
    request data = node_to_remove->data;
    q->first = node_to_remove->next;
    if (q->first != NULL){
        q->first->prev = NULL;
    }
    free(node_to_remove);
    q->size--;
    return data;
}


/*
int main() {
    Queue* q = create_queue();
    free(q);
}
*/
#endif //OS_HW3_QUEUE_H
