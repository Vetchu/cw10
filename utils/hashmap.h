//
// Created by vetch on 02.06.19.
//

#ifndef CW10_HASHMAP_H
#define CW10_HASHMAP_H
struct word {
    char *key;
    int val;
    struct word *next;
};
struct table {
    int size;
    struct word **list;
};
struct table *createTable(int size);

int hashCode(struct table *t, const char *key_string);

int insert(struct table *t, char *key);

int lookup(struct table *t, char *key);

char *parseText(char *text);
#endif //CW10_HASHMAP_H
