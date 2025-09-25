#ifndef EXTRAINCLUDE_H
#define EXTRAINCLUDE_H

#include <stddef.h> // For size_t

// --- Standard C Library String and Memory Functions ---
// Implementations are located in kernel.c

// String Manipulation
char* strcpy(char *dest, const char *src);
char* strcat(char *dest, const char *src);
char* strncpy(char *dest, const char *src, size_t n);
size_t strlen(const char* str);

// String Comparison
int strcmp(const char *a, const char *b);
int strncmp(const char *a, const char *b, size_t n);

// Memory Manipulation
void* memset(void *s, int c, size_t n);


#endif // EXTRAINCLUDE_H