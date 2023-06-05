//
// Created by teich on 05/06/2023.
//
#ifndef OS_HW3_LIST_H
#define OS_HW3_LIST_H

#include <stdio.h>
#include <stdlib.h>
#include "segel.h"

typedef struct node {
    int data;
    struct node* next;
    struct node* previous;
} Node;

typedef struct list {
    int size;
    Node* first;
    Node* last;
} List;

List* create_list() {
    List* new_list = (List*)malloc(sizeof(List));
    new_list->size = 0;
    new_list->first = NULL;
    new_list->last = NULL;
    return new_list;
}

void add_to_list(List* list, int data) {
    Node* new_node = (Node*)malloc(sizeof(Node));
    new_node->data = data;
    new_node->previous = list->last;
    new_node->next = NULL;

    if (list->first==NULL){
        assert(list->size==0);
        list->first=new_node;
    }

    if (list->last != NULL){
        list->last->next = new_node;
    }

    list->last = new_node;
    list->size++;
}

int remove_first(List* list) {
    assert(list->size>0);
    if (list->size == 0) {
        return -1;
    }
    Node* node_to_remove = list->first;
    int data = node_to_remove->data;
    list->first = node_to_remove->next;
    if (list->first != NULL){
        list->first->previous = NULL;
    }
    free(node_to_remove);
    list->size--;
    return data;
}

void free_list(List* list) {
    Node* current_node = list->first;
    while (current_node != NULL) {
        Node* next_node = current_node->next;
        free(current_node);
        current_node = next_node;
    }
    free(list);
}


#endif //OS_HW3_LIST_H