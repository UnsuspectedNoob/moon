#ifndef MOON_ERROR_H
#define MOON_ERROR_H

#include "object.h"
#include "scanner.h"

typedef enum { ERR_SYNTAX, ERR_TYPE, ERR_REFERENCE, ERR_RUNTIME } ErrorType;

// The Anchor: We call this once at the start of interpretation
void initErrorEngine(const char *source);

// Front-end error (Parser / Compiler)
void reportCompileError(Token *token, ErrorType type, const char *message,
                        const char *hint);

// Back-end error (VM Execution)
void reportRuntimeError(ObjString *moduleName, int line, ErrorType type,
                        const char *message, const char *hint);

#endif
