// table.c

#include <string.h>

#include "memory.h"
#include "object.h"
#include "table.h"
#include "value.h"

#define TABLE_MAX_LOAD 0.75

// Extern definition so we can access valuesEqual and hashValue
extern bool valuesEqual(Value a, Value b);
extern uint32_t hashValue(Value value);

void initTable(Table *table) {
  table->count = 0;
  table->capacity = 0;
  table->entries = NULL;
}

void freeTable(Table *table) {
  FREE_ARRAY(Entry, table->entries, table->capacity);
  initTable(table);
}

static Entry *findEntry(Entry *entries, int capacity, Value key) {
  // Use our new universal hash integer scrambler!
  uint32_t index = hashValue(key) % capacity;
  Entry *tombstone = NULL;

  for (;;) {
    Entry *entry = &entries[index];

    if (IS_EMPTY(entry->key)) {
      // Truly empty slot. If we passed a tombstone earlier, reuse it!
      return tombstone != NULL ? tombstone : entry;
    } else if (IS_TOMB(entry->key)) {
      // We found a tombstone. Save it in case we need to insert.
      if (tombstone == NULL)
        tombstone = entry;
    } else if (valuesEqual(entry->key, key)) {
      // Exact match! 1 == 1, but 1 != "1"
      return entry;
    }

    index = (index + 1) % capacity;
  }
}

static void adjustCapacity(Table *table, int capacity) {
  Entry *entries = ALLOCATE(Entry, capacity);
  for (int i = 0; i < capacity; i++) {
    entries[i].key = EMPTY_VAL; // Initialize with our secret empty tag
    entries[i].value = NIL_VAL;
  }

  table->count = 0;
  for (int i = 0; i < table->capacity; i++) {
    Entry *entry = &table->entries[i];

    // Don't copy over empty slots OR tombstones
    if (IS_EMPTY(entry->key) || IS_TOMB(entry->key))
      continue;

    Entry *dest = findEntry(entries, capacity, entry->key);
    dest->key = entry->key;
    dest->value = entry->value;
    table->count++;
  }

  FREE_ARRAY(Entry, table->entries, table->capacity);
  table->entries = entries;
  table->capacity = capacity;
}

bool tableGet(Table *table, Value key, Value *value) {
  if (table->count == 0)
    return false;

  Entry *entry = findEntry(table->entries, table->capacity, key);
  if (IS_EMPTY(entry->key) || IS_TOMB(entry->key))
    return false;

  *value = entry->value;
  return true;
}

bool tableSet(Table *table, Value key, Value value) {
  if (table->count + 1 > table->capacity * TABLE_MAX_LOAD) {
    int capacity = GROW_CAPACITY(table->capacity);
    adjustCapacity(table, capacity);
  }

  Entry *entry = findEntry(table->entries, table->capacity, key);

  // It's a new key if the slot is completely empty OR if it's a reusable
  // tombstone
  bool isNewKey = IS_EMPTY(entry->key) || IS_TOMB(entry->key);

  // We only increment the count if it goes into a truly empty slot
  if (isNewKey && IS_EMPTY(entry->key))
    table->count++;

  entry->key = key;
  entry->value = value;
  return isNewKey;
}

bool tableDelete(Table *table, Value key) {
  if (table->count == 0)
    return false;

  Entry *entry = findEntry(table->entries, table->capacity, key);
  if (IS_EMPTY(entry->key) || IS_TOMB(entry->key))
    return false;

  // Place a tombstone using our secret tag!
  entry->key = TOMBSTONE_VAL;
  entry->value = BOOL_VAL(true);
  return true;
}

void tableAddAll(Table *from, Table *to) {
  for (int i = 0; i < from->capacity; i++) {
    Entry *entry = &from->entries[i];
    // Copy valid entries only
    if (!IS_EMPTY(entry->key) && !IS_TOMB(entry->key)) {
      tableSet(to, entry->key, entry->value);
    }
  }
}

ObjString *tableFindString(Table *table, const char *chars, int length,
                           uint32_t hash) {
  if (table->count == 0)
    return NULL;

  uint32_t index = hash % table->capacity;
  for (;;) {
    Entry *entry = &table->entries[index];

    if (IS_EMPTY(entry->key)) {
      if (IS_NIL(entry->value))
        return NULL; // Stop if truly empty
    }
    // Is it a string? And do the hashes and characters match?
    else if (IS_STRING(entry->key)) {
      ObjString *stringKey = AS_STRING(entry->key);
      if (stringKey->length == length && stringKey->hash == hash &&
          memcmp(stringKey->chars, chars, length) == 0) {
        return stringKey;
      }
    }

    index = (index + 1) % table->capacity;
  }
}
