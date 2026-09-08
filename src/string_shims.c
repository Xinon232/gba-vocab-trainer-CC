#include <stddef.h>

// GBAWriter's freestanding string routines needed by its input/layout modules.
int strcmp(const char* first, const char* second) {
  while (*first && *first == *second) { ++first; ++second; }
  return (unsigned char)*first - (unsigned char)*second;
}

char* strcpy(char* destination, const char* source) {
  char* result = destination;
  while ((*destination++ = *source++)) {}
  return result;
}


size_t strlen(const char* s) {
  const char* p = s;
  while (*p) ++p;
  return (size_t)(p - s);
}

char* strchr(const char* s, int c) {
  char ch = (char)c;
  while (*s) {
    if (*s == ch) return (char*)s;
    ++s;
  }
  return ch == 0 ? (char*)s : 0;
}
