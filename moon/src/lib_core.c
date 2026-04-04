#include <stdio.h>
#include <string.h>
#include <time.h>

#include "lib_core.h"
#include "object.h"
#include "value.h"
#include "vm.h"

static Value clockNative(int argCount, Value *args) {
  (void)args;
  if (argCount != 0)
    return NIL_VAL; // Arity Shield
  return NUMBER_VAL((double)clock() / CLOCKS_PER_SEC);
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
    return NIL_VAL; // Arity Shield

  ObjString *prompt = valueToString(args[0]);
  printf("%s", prompt->chars);
  fflush(stdout);

  char buffer[1024];
  if (fgets(buffer, sizeof(buffer), stdin) != NULL) {
    size_t length = strlen(buffer);
    if (length > 0 && buffer[length - 1] == '\n') {
      buffer[length - 1] = '\0';
      length--;
    }
    return OBJ_VAL(copyString(buffer, (int)length));
  }
  return OBJ_VAL(copyString("", 0));
}

// The Handshake
void registerCoreLibrary() {
  // We use the exact names your core.h MOON wrapper expects
  defineNative("__show", showNative);
  defineNative("__ask", askNative);
  defineNative("__clock", clockNative);
}

// Define the actual array here! (Notice the [])
const char coreLibrary[] = "let show (stuff):\n"
                           "    give __show(stuff)\n"
                           "end\n"
                           "let ask (prompt: String):\n"
                           "    give __ask(prompt)\n"
                           "end\n"
                           "let clock:\n"
                           "    give __clock()\n"
                           "end\n";
