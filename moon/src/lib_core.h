#ifndef moon_lib_core_h
#define moon_lib_core_h

// 1. Expose the registration function
void registerCoreLibrary();

// 2. Expose the string as an ARRAY, not a pointer!
extern const char coreLibrary[];

#endif
