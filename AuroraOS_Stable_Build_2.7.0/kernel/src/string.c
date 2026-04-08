#include "string.h"

static char klower(char c) {
    if (c >= 'A' && c <= 'Z') {
        return (char)(c + ('a' - 'A'));
    }
    return c;
}

size_t kstrlen(const char* s) {
    size_t len = 0;
    while (s[len] != '\0') {
        len++;
    }
    return len;
}

int kstrcmp(const char* a, const char* b) {
    while (*a != '\0' && *b != '\0' && *a == *b) {
        a++;
        b++;
    }
    return (unsigned char)(*a) - (unsigned char)(*b);
}

int kstrncmp(const char* a, const char* b, size_t n) {
    for (size_t i = 0; i < n; ++i) {
        if (a[i] == '\0' || b[i] == '\0' || a[i] != b[i]) {
            return (unsigned char)a[i] - (unsigned char)b[i];
        }
    }
    return 0;
}

void kstrcpy(char* dst, const char* src) {
    while (*src != '\0') {
        *dst++ = *src++;
    }
    *dst = '\0';
}

int kstarts_with(const char* s, const char* prefix) {
    while (*prefix != '\0') {
        if (*s == '\0' || *s != *prefix) {
            return 0;
        }
        s++;
        prefix++;
    }
    return 1;
}

int kcontains(const char* haystack, const char* needle) {
    if (*needle == '\0') {
        return 1;
    }

    for (size_t i = 0; haystack[i] != '\0'; ++i) {
        size_t j = 0;
        while (needle[j] != '\0' && haystack[i + j] != '\0' && haystack[i + j] == needle[j]) {
            j++;
        }
        if (needle[j] == '\0') {
            return 1;
        }
    }

    return 0;
}

int kcontains_ci(const char* haystack, const char* needle) {
    if (*needle == '\0') {
        return 1;
    }

    for (size_t i = 0; haystack[i] != '\0'; ++i) {
        size_t j = 0;
        while (needle[j] != '\0' && haystack[i + j] != '\0' && klower(haystack[i + j]) == klower(needle[j])) {
            j++;
        }
        if (needle[j] == '\0') {
            return 1;
        }
    }

    return 0;
}

int katoi(const char* s) {
    int value = 0;
    int sign = 1;

    if (*s == '-') {
        sign = -1;
        s++;
    }

    while (*s >= '0' && *s <= '9') {
        value = (value * 10) + (*s - '0');
        s++;
    }

    return value * sign;
}
