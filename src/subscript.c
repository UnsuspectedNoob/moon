// subscript.c
#include "subscript.h"
#include "error.h"
#include "memory.h"
#include "object.h"
#include "vm.h"
#include <stdlib.h>

bool executeGetSubscript(Value seqVal, Value indexVal, Value *result) {
  if (IS_LIST(seqVal)) {
    ObjList *list = AS_LIST(seqVal);

    if (IS_NUMBER(indexVal)) {
      int userIndex = (int)AS_NUMBER(indexVal);
      if (userIndex == 0) {
        runtimeErrorDetailed(ERR_RUNTIME,
                             "You tried to access an index that doesn't exist. "
                             "Remember, MOON lists start at index 1!",
                             "Index out of bounds.");
        return false;
      }

      int index = userIndex > 0 ? userIndex - 1 : list->count + userIndex;

      if (index < 0 || index >= list->count) {
        runtimeErrorDetailed(ERR_RUNTIME,
                             "You tried to access an index that doesn't exist. "
                             "Remember, MOON lists start at index 1!",
                             "Index out of bounds.");
        return false;
      }

      *result = list->items[index];
      return true;

    } else if (IS_RANGE(indexVal)) {
      ObjRange *range = AS_RANGE(indexVal);
      int start = (int)range->start;
      int end = (int)range->end;
      int step = (int)range->step;

      if (step == 0) {
        runtimeErrorDetailed(
            ERR_RUNTIME,
            "The step size for slicing a list must be at least 1 or -1. "
            "Fractions less than 1 evaluate to 0.",
            "Slice step cannot be zero.");
        return false;
      }

      if (start < 0)
        start = list->count + start + 1;
      if (end < 0)
        end = list->count + end + 1;
      start--;
      end--;

      ObjList *resultList = newList();
      push(OBJ_VAL(resultList)); // GC Protection

      if (start <= end) {
        if (start < 0)
          start = 0;
        if (end >= list->count)
          end = list->count - 1;

        int count = ((end - start) / step) + 1;
        if (count > 0) {
          resultList->capacity = count;
          resultList->items = ALLOCATE(Value, count);
          int dest = 0;
          for (int i = start; i <= end; i += step) {
            resultList->items[dest++] = list->items[i];
          }
          resultList->count = dest;
        }
      } else {
        if (start >= list->count)
          start = list->count - 1;
        if (end < 0)
          end = 0;

        int count = ((start - end) / step) + 1;
        if (count > 0) {
          resultList->capacity = count;
          resultList->items = ALLOCATE(Value, count);
          int dest = 0;
          for (int i = start; i >= end; i -= step) {
            resultList->items[dest++] = list->items[i];
          }
          resultList->count = dest;
        }
      }

      pop(); // Unprotect
      *result = OBJ_VAL(resultList);
      return true;

    } else {
      runtimeErrorDetailed(
          ERR_TYPE,
          "Check what type of value you are passing into the brackets [].",
          "You tried to use a %s as an index for a List. Lists require a "
          "Number or a Range.",
          TYPE_NAME(indexVal));
      return false;
    }

  } else if (IS_DICT(seqVal)) {
    ObjDict *dict = AS_DICT(seqVal);
    Value dictResult;
    if (tableGet(&dict->fields, indexVal, &dictResult)) {
      *result = dictResult;
    } else {
      *result = NIL_VAL; // Safe cache miss!
    }
    return true;

  } else if (IS_STRING(seqVal)) {
    ObjString *str = AS_STRING(seqVal);

    if (IS_NUMBER(indexVal)) {
      int userIndex = (int)AS_NUMBER(indexVal);
      if (userIndex == 0) {
        runtimeErrorDetailed(ERR_RUNTIME,
                             "You tried to access an index that doesn't exist. "
                             "Remember, MOON strings start at index 1!",
                             "Index out of bounds.");
        return false;
      }

      int index = userIndex > 0 ? userIndex - 1 : str->length + userIndex;

      if (index < 0 || index >= str->length) {
        runtimeErrorDetailed(ERR_RUNTIME,
                             "You tried to access an index that doesn't exist. "
                             "Remember, MOON strings start at index 1!",
                             "Index out of bounds.");
        return false;
      }

      uint8_t charByte = (uint8_t)str->chars[index];
      *result = OBJ_VAL(vm.charStrings[charByte]);
      return true;

    } else if (IS_RANGE(indexVal)) {
      ObjRange *range = AS_RANGE(indexVal);
      int start = (int)range->start;
      int end = (int)range->end;
      int step = (int)range->step;

      if (step == 0) {
        runtimeErrorDetailed(
            ERR_RUNTIME,
            "The step size for slicing a string must be at least 1 or -1. "
            "Fractions less than 1 evaluate to 0.",
            "Slice step cannot be zero.");
        return false;
      }

      if (start < 0)
        start = str->length + start + 1;
      if (end < 0)
        end = str->length + end + 1;
      start--;
      end--;

      int maxLen = abs(start - end) + 1;
      char *chars = ALLOCATE(char, maxLen + 1);
      int dest = 0;

      if (start <= end) {
        if (start < 0)
          start = 0;
        if (end >= str->length)
          end = str->length - 1;
        for (int i = start; i <= end; i += step) {
          chars[dest++] = str->chars[i];
        }
      } else {
        if (start >= str->length)
          start = str->length - 1;
        if (end < 0)
          end = 0;
        for (int i = start; i >= end; i -= step) {
          chars[dest++] = str->chars[i];
        }
      }
      chars[dest] = '\0';

      *result = OBJ_VAL(takeString(chars, dest));
      return true;

    } else {
      runtimeErrorDetailed(
          ERR_TYPE,
          "Check what type of value you are passing into the brackets [].",
          "You tried to use a %s as an index for a String. Strings require a "
          "Number or a Range.",
          TYPE_NAME(indexVal));
      return false;
    }

  } else {
    runtimeErrorDetailed(ERR_TYPE,
                         "You can only use brackets [] to access items in "
                         "Lists, Dictionaries, or Strings.",
                         "Type is not subscriptable.");
    return false;
  }
}

bool executeSetSubscript(Value collectionVal, Value indexVal, Value value,
                         Value *result) {
  if (IS_DICT(collectionVal)) {
    ObjDict *dict = AS_DICT(collectionVal);
    tableSet(&dict->fields, indexVal, value);
    *result = value;
    return true;

  } else if (IS_LIST(collectionVal)) {
    ObjList *list = AS_LIST(collectionVal);
    if (!IS_NUMBER(indexVal)) {
      runtimeErrorDetailed(
          ERR_TYPE,
          "List indices must be numbers. Check if you accidentally passed a "
          "string or another type inside the brackets.",
          "List index must be a number.");
      return false;
    }

    int userIndex = (int)AS_NUMBER(indexVal);
    if (userIndex == 0) {
      runtimeErrorDetailed(ERR_RUNTIME,
                           "You tried to access an index that doesn't exist. "
                           "Remember, MOON lists start at index 1!",
                           "Index out of bounds.");
      return false;
    }

    int index = userIndex > 0 ? userIndex - 1 : list->count + userIndex;

    if (index < 0 || index >= list->count) {
      runtimeErrorDetailed(ERR_RUNTIME,
                           "You tried to access an index that doesn't exist. "
                           "Remember, MOON lists start at index 1!",
                           "Index out of bounds.");
      return false;
    }

    list->items[index] = value;
    *result = value;
    return true;

  } else {
    runtimeErrorDetailed(
        ERR_TYPE,
        "You can only use bracket assignment (like set target[key] to value) "
        "to modify Lists and Dictionaries.",
        "Can only assign to lists and dictionaries.");
    return false;
  }
}
