#ifndef EXTPROG_H
#define EXTPROG_H

#include <stddef.h>

enum {
    EXTPROG_CAP_FS_READ  = 1U << 0,
    EXTPROG_CAP_FS_WRITE = 1U << 1,
    EXTPROG_CAP_NETWORK  = 1U << 2,
    EXTPROG_CAP_APPS     = 1U << 3,
    EXTPROG_CAP_SYSTEM   = 1U << 4
};

typedef struct {
    char name[32];
    char entry[96];
    char trust[16];
    char owner[24];
    unsigned int caps;
    unsigned int approved;
} extprog_manifest_t;

void extprog_init(void);
int extprog_register(const char* name, const char* entry, unsigned int caps, const char* trust, int approved);
int extprog_remove(const char* name);
int extprog_load(const char* name, extprog_manifest_t* out);
int extprog_set_caps(const char* name, unsigned int caps);
int extprog_set_trust(const char* name, const char* trust);
int extprog_set_approved(const char* name, int approved);
unsigned int extprog_caps_from_text(const char* text);
void extprog_caps_to_text(unsigned int caps, char* out, size_t max);
int extprog_manifest_path(const char* name, char* out, size_t max);

#endif
