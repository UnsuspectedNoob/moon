#ifndef MOON_TABLE_H
#define MOON_TABLE_H

#include "common.h"
#include "value.h"

// Forward declaration for tableFindString
typedef struct ObjString ObjString;

typedef struct {
  Value key; // <--- CHANGED: Now accepts ANY Moon value!
  Value value;
} Entry;

typedef struct {
  int count;
  int capacity;
  int tombstones;
  Entry *entries;
} Table;

void initTable(Table *table);
void freeTable(Table *table);

// Notice these now take a 'Value key'
bool tableGet(Table *table, Value key, Value *value);
bool tableSet(Table *table, Value key, Value value);
bool tableDelete(Table *table, Value key);

void tableAddAll(Table *from, Table *to);

// We keep this specific to strings for our intern pool!
ObjString *tableFindString(Table *table, const char *chars, int length,
                           uint32_t hash);
void tableRemoveWhite(Table *table);

#endif
