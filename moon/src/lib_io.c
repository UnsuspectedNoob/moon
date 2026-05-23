#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lib_io.h"
#include "memory.h"
#include "object.h"
#include "value.h"
#include "vm.h"

// --- THE PRIMITIVES ---

static Value readNative(int argCount, Value *args) {
  if (argCount != 1)
    return NIL_VAL;
  if (!IS_STRING(args[0]))
    return NIL_VAL;

  ObjString *path = AS_STRING(args[0]);
  FILE *file = fopen(path->chars, "rb");

  // The Empathetic OS Intercept!
  if (file == NULL) {
    throwNativeError("Make sure the file exists, the path is spelled "
                     "correctly, and MOON has permission to read it.",
                     "I could not open the file at '%s'.", path->chars);
  }

  // 1. Find out exactly how big the file is
  fseek(file, 0L, SEEK_END);
  size_t fileSize = ftell(file);
  rewind(file);

  // 2. Allocate exactly enough memory (+1 for the null terminator)
  char *buffer = ALLOCATE(char, fileSize + 1);

  // 3. Read the entire file into memory
  size_t bytesRead = fread(buffer, sizeof(char), fileSize, file);
  if (bytesRead < fileSize) {
    fclose(file);
    // Free the buffer so we don't leak memory before crashing!
    FREE_ARRAY(char, buffer, fileSize + 1);
    throwNativeError(
        "The file might be corrupted or locked by another program.",
        "I failed to read the full contents of '%s'.", path->chars);
  }

  buffer[bytesRead] = '\0';
  fclose(file);

  // 4. Hand the raw C-string to the MOON Garbage Collector!
  return OBJ_VAL(takeString(buffer, fileSize));
}

static Value writeNative(int argCount, Value *args) {
  if (argCount != 2)
    return NIL_VAL;
  if (!IS_STRING(args[0]) || !IS_STRING(args[1]))
    return NIL_VAL;

  ObjString *content = AS_STRING(args[0]);
  ObjString *path = AS_STRING(args[1]);

  FILE *file = fopen(path->chars, "wb"); // "w" destroys existing contents!
  if (file == NULL) {
    throwNativeError(
        "Check if the folder exists and if you have write permissions.",
        "I could not create or open the file at '%s'.", path->chars);
  }

  fwrite(content->chars, sizeof(char), content->length, file);
  fclose(file);

  return NIL_VAL;
}

static Value appendNative(int argCount, Value *args) {
  if (argCount != 2)
    return NIL_VAL;
  if (!IS_STRING(args[0]) || !IS_STRING(args[1]))
    return NIL_VAL;

  ObjString *content = AS_STRING(args[0]);
  ObjString *path = AS_STRING(args[1]);

  FILE *file = fopen(path->chars, "ab"); // "a" appends to the end!
  if (file == NULL) {
    throwNativeError(
        "Check if the folder exists and if you have write permissions.",
        "I could not open the file at '%s' to append data.", path->chars);
  }

  fwrite(content->chars, sizeof(char), content->length, file);
  fclose(file);

  return NIL_VAL;
}

static Value existsNative(int argCount, Value *args) {
  if (argCount != 1)
    return NIL_VAL;
  if (!IS_STRING(args[0]))
    return NIL_VAL;

  ObjString *path = AS_STRING(args[0]);

  // Quickest way to check existence in C without OS-specific libraries
  FILE *file = fopen(path->chars, "r");
  if (file != NULL) {
    fclose(file);
    return BOOL_VAL(true);
  }

  return BOOL_VAL(false);
}

// --- THE HANDSHAKE ---

void registerIOLibrary() {
  REGISTER_PHRASE(NULL, "read", "file,$1", 1, "read_file$1", readNative, vm.stringType);
  REGISTER_PHRASE(NULL, "write", "$1,to,$1", 2, "write$1_to$1", writeNative, vm.stringType, vm.stringType);
  REGISTER_PHRASE(NULL, "append", "$1,to,file,$1", 2, "append$1_to_file$1", appendNative, vm.stringType, vm.stringType);
  REGISTER_PHRASE(NULL, "file", "$1,exists", 1, "file$1_exists", existsNative, vm.stringType);
}
