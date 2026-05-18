// cast.c
#include "cast.h"
#include "error.h"
#include "memory.h"
#include "vm.h"
#include <stdlib.h>
#include <string.h>

// --- CASTING HELPERS ---
static bool castToString(Value val, Value *result) {
  if (IS_LIST(val)) {
    ObjList *list = AS_LIST(val);
    ObjString **strings = malloc(sizeof(ObjString *) * list->count);
    int totalLength = 0;

    for (int i = 0; i < list->count; i++) {
      strings[i] = valueToString(list->items[i]);
      push(OBJ_VAL(strings[i])); // GC Protection
      totalLength += strings[i]->length;
    }

    char *chars = ALLOCATE(char, totalLength + 1);
    char *dest = chars;
    for (int i = 0; i < list->count; i++) {
      memcpy(dest, strings[i]->chars, strings[i]->length);
      dest += strings[i]->length;
    }
    chars[totalLength] = '\0';

    ObjString *strResult = takeString(chars, totalLength);
    vm.stackTop -= list->count; // Cleanup
    free(strings);

    *result = OBJ_VAL(strResult);
    return true;
  }

  *result = OBJ_VAL(valueToString(val));
  return true;
}

static bool castToBool(Value val, Value *result) {
  if (IS_STRING(val)) {
    ObjString *s = AS_STRING(val);
    if (strcmp(s->chars, "true") == 0) {
      *result = BOOL_VAL(true);
      return true;
    } else if (strcmp(s->chars, "false") == 0) {
      *result = BOOL_VAL(false);
      return true;
    }
    runtimeErrorDetailed(ERR_TYPE, "String must be exactly 'true' or 'false'.",
                         "Invalid string to bool cast.");
    return false;
  }

  *result = BOOL_VAL(!isFalsey(val));
  return true;
}

static bool castToNumber(Value val, Value *result) {
  if (IS_STRING(val)) {
    ObjString *s = AS_STRING(val);
    char *end;
    double num = strtod(s->chars, &end);

    if (*end != '\0') {
      runtimeErrorDetailed(ERR_TYPE,
                           "Make sure the string only contains digits, a "
                           "decimal point, or a minus sign.",
                           "Cannot cast the string '%s' to a Number.",
                           s->chars);
      return false;
    }
    *result = NUMBER_VAL(num);
    return true;
  } else if (IS_BOOL(val)) {
    *result = NUMBER_VAL(AS_BOOL(val) ? 1.0 : 0.0);
    return true;
  } else if (IS_LIST(val)) {
    ObjList *list = AS_LIST(val);
    double numResult = 0.0;

    for (int i = 0; i < list->count; i++) {
      if (!IS_NUMBER(list->items[i])) {
        runtimeErrorDetailed(
            ERR_TYPE,
            "A list can only be cast to a Number if every single item inside "
            "it is a number.",
            "Found a %s inside the list. Cannot cast to Number.",
            TYPE_NAME(list->items[i]));
        return false;
      }
      // Horner's Method
      numResult = (numResult * 10.0) + AS_NUMBER(list->items[i]);
    }
    *result = NUMBER_VAL(numResult);
    return true;
  }

  runtimeErrorDetailed(ERR_TYPE,
                       "Can only cast Strings, Bools, and Lists to Numbers.",
                       "Invalid cast to Number.");
  return false;
}

static bool castToList(Value val, Value *result) {
  ObjList *l = newList();
  push(OBJ_VAL(l)); // GC Protection

  if (IS_RANGE(val)) {
    ObjRange *r = AS_RANGE(val);
    if (r->start <= r->end) {
      for (double i = r->start; i <= r->end; i += r->step)
        appendList(l, NUMBER_VAL(i));
    } else {
      for (double i = r->start; i >= r->end; i -= r->step)
        appendList(l, NUMBER_VAL(i));
    }
  } else if (IS_STRING(val)) {
    ObjString *s = AS_STRING(val);
    for (int i = 0; i < s->length; i++) {
      appendList(l, OBJ_VAL(vm.charStrings[(uint8_t)s->chars[i]]));
    }
  } else if (IS_NUMBER(val)) {
    ObjString *s = valueToString(val);
    push(OBJ_VAL(s));
    for (int i = 0; i < s->length; i++) {
      char c = s->chars[i];
      if (c >= '0' && c <= '9')
        appendList(l, NUMBER_VAL(c - '0'));
      else
        appendList(l, OBJ_VAL(vm.charStrings[(uint8_t)c]));
    }
    pop();
  } else if (IS_DICT(val)) {
    ObjDict *d = AS_DICT(val);
    for (int i = 0; i < d->fields.capacity; i++) {
      Entry *e = &d->fields.entries[i];
      if (!IS_EMPTY(e->key) && !IS_TOMB(e->key)) {
        ObjList *pair = newList();
        push(OBJ_VAL(pair));
        appendList(pair, e->key);
        appendList(pair, e->value);
        pop();
        appendList(l, OBJ_VAL(pair));
      }
    }
  } else {
    pop(); // Remove the unused list
    runtimeErrorDetailed(ERR_TYPE, "Cannot cast this type to a List.",
                         "Invalid cast to List.");
    return false;
  }

  pop(); // Remove protection
  *result = OBJ_VAL(l);
  return true;
}

static bool castToDict(Value val, Value *result) {
  ObjDict *d = newDict();
  push(OBJ_VAL(d)); // GC Protection

  if (IS_LIST(val)) {
    ObjList *l = AS_LIST(val);
    for (int i = 0; i < l->count; i++) {
      if (!IS_LIST(l->items[i])) {
        pop();
        runtimeErrorDetailed(
            ERR_TYPE,
            "List items must be key-value pairs (lists of 2) to cast to Dict.",
            "Invalid list to dict cast.");
        return false;
      }
      ObjList *pair = AS_LIST(l->items[i]);
      if (pair->count != 2) {
        pop();
        runtimeErrorDetailed(ERR_TYPE, "Pairs must have exactly 2 items.",
                             "Invalid list to dict cast.");
        return false;
      }
      tableSet(&d->fields, pair->items[0], pair->items[1]);
    }
  } else if (IS_INSTANCE(val)) {
    ObjInstance *inst = AS_INSTANCE(val);
    tableAddAll(&inst->fields, &d->fields);
  } else {
    pop();
    runtimeErrorDetailed(ERR_TYPE, "Cannot cast this type to a Dict.",
                         "Invalid cast to Dict.");
    return false;
  }

  pop();
  *result = OBJ_VAL(d);
  return true;
}

static bool hydrateInstance(Value val, ObjType *targetType, Value *result) {
  if (!IS_DICT(val)) {
    runtimeErrorDetailed(ERR_TYPE,
                         "Can only hydrate a Blueprint using a Dictionary.",
                         "Invalid hydration cast.");
    return false;
  }

  ObjDict *d = AS_DICT(val);
  ObjInstance *inst = newInstance(targetType);
  push(OBJ_VAL(inst));

  tableAddAll(&targetType->properties, &inst->fields); // Load defaults

  for (int i = 0; i < d->fields.capacity; i++) {
    Entry *e = &d->fields.entries[i];
    if (!IS_EMPTY(e->key) && !IS_TOMB(e->key)) {
      Value dummy;
      if (!tableGet(&targetType->properties, e->key, &dummy)) {
        pop();
        runtimeErrorDetailed(
            ERR_TYPE, "Dictionary contains a key not present on the Blueprint.",
            "Strict hydration failure.");
        return false;
      }
      tableSet(&inst->fields, e->key, e->value);
    }
  }

  pop();
  *result = OBJ_VAL(inst);
  return true;
}

// The exposed Universal Router
bool executeCast(Value val, ObjType *targetType, Value *result) {
  // Fast path: Exact same type
  if (targetType == getObjType(val)) {
    *result = val;
    return true;
  }

  if (targetType == vm.typeType) {
    *result = OBJ_VAL(getObjType(val));
    return true;
  }
  if (targetType == vm.stringType)
    return castToString(val, result);
  if (targetType == vm.boolType)
    return castToBool(val, result);
  if (targetType == vm.numberType)
    return castToNumber(val, result);
  if (targetType == vm.listType)
    return castToList(val, result);
  if (targetType == vm.dictType)
    return castToDict(val, result);

  if (!targetType->isNative)
    return hydrateInstance(val, targetType, result);

  runtimeErrorDetailed(ERR_TYPE, "Cannot cast to this native type directly.",
                       "Invalid cast target.");
  return false;
}
