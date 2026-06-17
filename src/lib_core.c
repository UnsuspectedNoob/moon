#include <stdio.h>
#include <string.h>
#include <time.h>

#include "lib_core.h"
#include "object.h"
#include "value.h"
#include "vm.h"
#include "memory.h"
#include "table.h"

#include <sys/time.h>

static Value clockNative(int argCount, Value *args) {
  (void)args;
  if (argCount != 0)
    return NIL_VAL; // Arity Shield

#ifdef _WIN32
  return NUMBER_VAL((double)clock() / CLOCKS_PER_SEC);
#else
  struct timeval tv;
  gettimeofday(&tv, NULL);
  return NUMBER_VAL((double)tv.tv_sec + (double)tv.tv_usec / 1000000.0);
#endif
}

static Value showNative(int argCount, Value *args) {
  if (argCount != 1)
    return NIL_VAL; // Arity Shield

  ObjString *stringified = valueToString(args[0]);
  printf("%s\n", stringified->chars);
  return NIL_VAL;
}

static Value askNative(int argCount, Value *args) {
  if (argCount != 1)
    return NIL_VAL;
  ObjString *prompt = valueToString(args[0]);
  printf("%s", prompt->chars);
  fflush(stdout);

  char buffer[4096];
  if (fgets(buffer, sizeof(buffer), stdin) != NULL) {
    size_t length = strlen(buffer);
    if (length > 0 && buffer[length - 1] == '\n') {
      buffer[length - 1] = '\0';
      length--;
    } else {
      int ch;
      while ((ch = getchar()) != '\n' && ch != EOF)
        ;
    }
    return OBJ_VAL(copyString(buffer, (int)length));
  }
  return OBJ_VAL(copyString("", 0));
}

static void setDictNumber(ObjDict *dict, const char *key, double value) {
  ObjString *keyString = copyString(key, (int)strlen(key));
  push(OBJ_VAL(keyString));
  tableSet(&dict->fields, OBJ_VAL(keyString), NUMBER_VAL(value));
  pop();
}

static Value memoryNative(int argCount, Value *args) {
  (void)args;
  if (argCount != 0) return NIL_VAL;

  ObjDict *dict = newDict();
  push(OBJ_VAL(dict)); 

  setDictNumber(dict, "used", (double)vm.bytesAllocated);
  setDictNumber(dict, "limit", (double)vm.nextGC);

  int total = 0, strings = 0, lists = 0, functions = 0, instances = 0;
  int dicts = 0, ranges = 0, types = 0, unions = 0, multi = 0, modules = 0;

  Obj *object = vm.objects;
  while (object != NULL) {
    total++;
    switch (object->type) {
      case OBJ_STRING: strings++; break;
      case OBJ_LIST: lists++; break;
      case OBJ_FUNCTION: functions++; break;
      case OBJ_INSTANCE: instances++; break;
      case OBJ_DICT: dicts++; break;
      case OBJ_RANGE: ranges++; break;
      case OBJ_TYPE_BLUEPRINT: types++; break;
      case OBJ_UNION: unions++; break;
      case OBJ_MULTI_FUNCTION: multi++; break;
      case OBJ_MODULE: modules++; break;
      default: break;
    }
    object = object->next;
  }

  setDictNumber(dict, "total_objects", (double)total);
  setDictNumber(dict, "strings", (double)strings);
  setDictNumber(dict, "lists", (double)lists);
  setDictNumber(dict, "functions", (double)functions);
  setDictNumber(dict, "instances", (double)instances);
  setDictNumber(dict, "dicts", (double)dicts);
  setDictNumber(dict, "ranges", (double)ranges);
  setDictNumber(dict, "blueprints", (double)types);
  setDictNumber(dict, "unions", (double)unions);
  setDictNumber(dict, "multi_functions", (double)multi);
  setDictNumber(dict, "modules", (double)modules);

  pop();
  return OBJ_VAL(dict);
}

static Value gcNative(int argCount, Value *args) {
  (void)args;
  if (argCount != 0) return NIL_VAL;

  size_t before = vm.bytesAllocated;
  collectGarbage();
  size_t after = vm.bytesAllocated;

  return NUMBER_VAL((double)(before - after));
}

// The Handshake
void registerCoreLibrary() {
  REGISTER_PHRASE(NULL, "show", "$1", 1, "show$1", showNative, vm.anyType);
  REGISTER_PHRASE(NULL, "ask", "$1", 1, "ask$1", askNative, vm.stringType);
  REGISTER_PHRASE_0(NULL, "clock", NULL, "clock$0", clockNative);
  REGISTER_PHRASE_0(NULL, "memory", NULL, "memory$0", memoryNative);
  REGISTER_PHRASE_0(NULL, "gc", NULL, "gc$0", gcNative);
}
