#ifndef moon_lib_string_h
#define moon_lib_string_h

// Hooks the string C-primitives into the MOON VM
void registerStringLibrary();

// Expose the MOON wrapper as an array!
extern const char stringBootstrap[];

#endif
