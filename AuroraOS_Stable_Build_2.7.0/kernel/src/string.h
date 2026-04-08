#ifndef STRING_H
#define STRING_H

#include <stddef.h>

size_t kstrlen(const char* s);
int kstrcmp(const char* a, const char* b);
int kstrncmp(const char* a, const char* b, size_t n);
void kstrcpy(char* dst, const char* src);
int kstarts_with(const char* s, const char* prefix);
int kcontains(const char* haystack, const char* needle);
int kcontains_ci(const char* haystack, const char* needle);
int katoi(const char* s);

#endif
