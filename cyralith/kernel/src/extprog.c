#include "extprog.h"
#include "cyralithfs.h"
#include "string.h"
#include "user.h"

static void append_text(char* dst, size_t max, const char* src) {
    size_t pos = kstrlen(dst);
    size_t i = 0U;
    if (pos >= max) {
        return;
    }
    while (src[i] != '\0' && pos + 1U < max) {
        dst[pos++] = src[i++];
    }
    dst[pos] = '\0';
}

static void trim_line_end(char* text) {
    size_t len = kstrlen(text);
    while (len > 0U && (text[len - 1U] == '\n' || text[len - 1U] == '\r' || text[len - 1U] == ' ' || text[len - 1U] == '\t')) {
        text[len - 1U] = '\0';
        len--;
    }
}

static int command_name_valid(const char* name) {
    size_t i;
    if (name == (const char*)0 || name[0] == '\0') {
        return 0;
    }
    for (i = 0U; name[i] != '\0'; ++i) {
        char c = name[i];
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' || c == '_')) {
            return 0;
        }
    }
    return 1;
}

int extprog_manifest_path(const char* name, char* out, size_t max) {
    if (command_name_valid(name) == 0 || out == (char*)0 || max == 0U) {
        return -1;
    }
    out[0] = '\0';
    append_text(out, max, "/apps/programs/");
    append_text(out, max, name);
    append_text(out, max, ".app");
    return 0;
}

static int parse_manifest_line(const char* line, const char* key, char* out, size_t max) {
    size_t key_len = kstrlen(key);
    size_t pos = 0U;
    if (kstrncmp(line, key, key_len) != 0 || line[key_len] != '=') {
        return -1;
    }
    line += key_len + 1U;
    while (*line != '\0' && *line != '\n' && *line != '\r' && pos + 1U < max) {
        out[pos++] = *line++;
    }
    out[pos] = '\0';
    trim_line_end(out);
    return 0;
}

unsigned int extprog_caps_from_text(const char* text) {
    unsigned int caps = 0U;
    if (text == (const char*)0) {
        return 0U;
    }
    if (kcontains_ci(text, "fs-read") != 0 || kcontains_ci(text, "read") != 0) {
        caps |= EXTPROG_CAP_FS_READ;
    }
    if (kcontains_ci(text, "fs-write") != 0 || kcontains_ci(text, "write") != 0) {
        caps |= EXTPROG_CAP_FS_WRITE;
    }
    if (kcontains_ci(text, "network") != 0 || kcontains_ci(text, "net") != 0) {
        caps |= EXTPROG_CAP_NETWORK;
    }
    if (kcontains_ci(text, "apps") != 0 || kcontains_ci(text, "app") != 0) {
        caps |= EXTPROG_CAP_APPS;
    }
    if (kcontains_ci(text, "system") != 0 || kcontains_ci(text, "admin") != 0) {
        caps |= EXTPROG_CAP_SYSTEM;
    }
    return caps;
}

void extprog_caps_to_text(unsigned int caps, char* out, size_t max) {
    int first = 1;
    if (out == (char*)0 || max == 0U) {
        return;
    }
    out[0] = '\0';
    if (caps == 0U) {
        append_text(out, max, "none");
        return;
    }
    if ((caps & EXTPROG_CAP_FS_READ) != 0U) {
        append_text(out, max, first ? "fs-read" : ",fs-read");
        first = 0;
    }
    if ((caps & EXTPROG_CAP_FS_WRITE) != 0U) {
        append_text(out, max, first ? "fs-write" : ",fs-write");
        first = 0;
    }
    if ((caps & EXTPROG_CAP_NETWORK) != 0U) {
        append_text(out, max, first ? "network" : ",network");
        first = 0;
    }
    if ((caps & EXTPROG_CAP_APPS) != 0U) {
        append_text(out, max, first ? "apps" : ",apps");
        first = 0;
    }
    if ((caps & EXTPROG_CAP_SYSTEM) != 0U) {
        append_text(out, max, first ? "system" : ",system");
    }
}

static int write_manifest(const extprog_manifest_t* manifest) {
    char path[96];
    char text[256];
    char caps[96];
    if (manifest == (const extprog_manifest_t*)0) {
        return -1;
    }
    if (extprog_manifest_path(manifest->name, path, sizeof(path)) != 0) {
        return -1;
    }
    extprog_caps_to_text(manifest->caps, caps, sizeof(caps));
    text[0] = '\0';
    append_text(text, sizeof(text), "entry=");
    append_text(text, sizeof(text), manifest->entry);
    append_text(text, sizeof(text), "\ntrust=");
    append_text(text, sizeof(text), manifest->trust);
    append_text(text, sizeof(text), "\ncaps=");
    append_text(text, sizeof(text), caps);
    append_text(text, sizeof(text), "\napproved=");
    append_text(text, sizeof(text), manifest->approved != 0U ? "yes" : "no");
    append_text(text, sizeof(text), "\nowner=");
    append_text(text, sizeof(text), manifest->owner);
    append_text(text, sizeof(text), "\n");
    return afs_write_file(path, text);
}

void extprog_init(void) {
    (void)afs_mkdir("/apps/programs");
    (void)afs_mkdir("/apps/commands");
}

int extprog_register(const char* name, const char* entry, unsigned int caps, const char* trust, int approved) {
    extprog_manifest_t manifest;
    if (command_name_valid(name) == 0 || entry == (const char*)0 || entry[0] == '\0') {
        return -1;
    }
    manifest.name[0] = '\0';
    manifest.entry[0] = '\0';
    manifest.trust[0] = '\0';
    manifest.owner[0] = '\0';
    kstrcpy(manifest.name, name);
    kstrcpy(manifest.entry, entry);
    kstrcpy(manifest.trust, trust != (const char*)0 && trust[0] != '\0' ? trust : "local");
    kstrcpy(manifest.owner, user_current()->username);
    manifest.caps = caps;
    manifest.approved = approved != 0 ? 1U : 0U;
    return write_manifest(&manifest);
}

int extprog_load(const char* name, extprog_manifest_t* out) {
    char path[96];
    char text[256];
    size_t i;
    size_t start = 0U;
    int rc;
    if (out == (extprog_manifest_t*)0) {
        return -1;
    }
    if (extprog_manifest_path(name, path, sizeof(path)) != 0) {
        return -1;
    }
    rc = afs_read_file(path, text, sizeof(text));
    if (rc < 0) {
        return rc;
    }
    out->name[0] = '\0';
    out->entry[0] = '\0';
    kstrcpy(out->trust, "local");
    out->owner[0] = '\0';
    out->caps = 0U;
    out->approved = 0U;
    kstrcpy(out->name, name);

    for (i = 0U;; ++i) {
        if (text[i] == '\r') {
            text[i] = '\n';
        }
        if (text[i] == '\n' || text[i] == '\0') {
            char saved = text[i];
            char line[128];
            size_t pos = 0U;
            size_t j = start;
            while (text[j] != '\0' && text[j] != '\n' && pos + 1U < sizeof(line)) {
                line[pos++] = text[j++];
            }
            line[pos] = '\0';
            if (parse_manifest_line(line, "entry", out->entry, sizeof(out->entry)) == 0) {
            } else if (parse_manifest_line(line, "trust", out->trust, sizeof(out->trust)) == 0) {
            } else if (parse_manifest_line(line, "caps", line, sizeof(line)) == 0) {
                out->caps = extprog_caps_from_text(line);
            } else if (parse_manifest_line(line, "approved", line, sizeof(line)) == 0) {
                out->approved = (kstrcmp(line, "yes") == 0 || kstrcmp(line, "1") == 0) ? 1U : 0U;
            } else if (parse_manifest_line(line, "owner", out->owner, sizeof(out->owner)) == 0) {
            }
            if (saved == '\0') {
                break;
            }
            start = i + 1U;
        }
    }
    if (out->entry[0] == '\0') {
        return -1;
    }
    if (out->owner[0] == '\0') {
        kstrcpy(out->owner, "guest");
    }
    return 0;
}

int extprog_set_caps(const char* name, unsigned int caps) {
    extprog_manifest_t manifest;
    int rc = extprog_load(name, &manifest);
    if (rc != 0) {
        return rc;
    }
    manifest.caps = caps;
    return write_manifest(&manifest);
}

int extprog_set_trust(const char* name, const char* trust) {
    extprog_manifest_t manifest;
    int rc = extprog_load(name, &manifest);
    if (rc != 0) {
        return rc;
    }
    if (trust == (const char*)0 || trust[0] == '\0') {
        return -1;
    }
    kstrcpy(manifest.trust, trust);
    return write_manifest(&manifest);
}

int extprog_set_approved(const char* name, int approved) {
    extprog_manifest_t manifest;
    int rc = extprog_load(name, &manifest);
    if (rc != 0) {
        return rc;
    }
    manifest.approved = approved != 0 ? 1U : 0U;
    return write_manifest(&manifest);
}

int extprog_remove(const char* name) {
    char path[96];
    if (extprog_manifest_path(name, path, sizeof(path)) != 0) {
        return -1;
    }
    return afs_rm(path);
}
