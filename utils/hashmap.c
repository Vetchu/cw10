//
// Created by vetch on 02.06.19.
//

#include "hashmap.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>

struct table *createTable(int size) {
    struct table *t = (struct table *) malloc(sizeof(struct table));
    t->size = size;
    t->list = (struct word **) malloc(sizeof(struct word *) * size);
    int i;
    for (i = 0; i < size; i++)
        t->list[i] = NULL;
    return t;
}

int hashCode(struct table *t, const char *key_string) {
    int key = 0;
    for (int i = 0; key_string[i]; i++) {
        key += key_string[i];
    }
    if (key < 0) {
        return -(key % t->size);
    }
    return key % t->size;
}

int insert(struct table *t, char *key) {
    int pos = hashCode(t, key);
    struct word *list = t->list[pos];
    struct word *temp = list;
    while (temp) {
        if (strcmp(temp->key, key) == 0) {
            temp->val++;
            return 1;
        }
        temp = temp->next;
    }

    struct word *newNode = (struct word *) malloc(sizeof(struct word));
    newNode->key = key;
    newNode->val = 1;
    newNode->next = list;
    t->list[pos] = newNode;
    return 0;
}

int lookup(struct table *t, char *key) {
    int pos = hashCode(t, key);
    struct word *list = t->list[pos];
    struct word *temp = list;
    while (temp) {
        if (strcmp(temp->key, key) == 0) {
            return temp->val;
        }
        temp = temp->next;
    }
    return -1;
}

char *parseText(char *text) {
//    printf("%s\n",text);
    char *word = strtok(text, " ,-:/?!()[]\n");
    int sum = 1;

    struct table *t = createTable(sizeof(text));

    struct word *word_list = calloc(1, sizeof(struct word));
    word_list->key = 0;
    word_list->val = 0;
    word_list->next = NULL;

    do {
        for (int i = 0; word[i]; i++) {
            word[i] = tolower(word[i]);
        }
        if (!insert(t, word)) {
            struct word *new_word = calloc(1, sizeof(struct word));
            new_word->key = word;
            new_word->next = word_list->next;
            word_list->next = new_word;
        }
        sum++;
        usleep(100);
    } while ((word = strtok(NULL, " ,-:/?!()[]\n")) != NULL);

    char* buf=calloc(60000, sizeof(char));
    struct word *tmp = word_list->next;

    while (tmp) {
        char keyval[400];
        sprintf(keyval, "%s:\t%d\n", tmp->key, lookup(t, tmp->key));
        strcat(buf, keyval);
        tmp = tmp->next;
    }
//    printf("%s", buf);
    while (word_list) {
        tmp = word_list;
        word_list = word_list->next;
        free(tmp);
    }
    for (int i = 0; i < sizeof(text); i++) {
        tmp = t->list[i];
        while (tmp) {
            struct word *temp = tmp;
            tmp = tmp->next;
            free(temp);
        }
    }
    free(t->list);
    free(t);
//    return buf;
    char* buf1=calloc(10, sizeof(char));
    sprintf(buf1,"%d",sum);
    return  buf1;
}