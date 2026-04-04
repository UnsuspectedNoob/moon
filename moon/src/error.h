#ifndef MOON_ERROR_H
#define MOON_ERROR_H

#include "object.h"
#include "scanner.h"

// --- THE ANSI COLOR PALETTE ---
#define COLOR_RED "\x1b[1;31m"
#define COLOR_YELLOW "\x1b[1;33m"
#define COLOR_CYAN "\x1b[1;36m"
#define COLOR_DIM "\x1b[90m"
#define COLOR_RESET "\x1b[0m"

typedef enum { ERR_SYNTAX, ERR_TYPE, ERR_REFERENCE, ERR_RUNTIME } ErrorType;

// The Anchor: We call this once at the start of interpretation
void initErrorEngine(const char *source);

// Front-end error (Parser / Compiler)
void reportCompileError(Token *token, ErrorType type, const char *message,
                        const char *hint);

// Back-end error (VM Execution)
void reportRuntimeError(ObjString *moduleName, int line, ErrorType type,
                        const char *message, const char *hint);

// The Spellchecking Oracle
const char *findVariableSuggestion(const char *misspelled);

#endif
