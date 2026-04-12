#include "cyralithfs.h"
#include "console.h"
#include "string.h"
#include "user.h"
#include "storage.h"

#define AFS_MAX_NODES 64
#define AFS_NAME_MAX 24
#define AFS_DATA_MAX 1024
#define AFS_PATH_MAX 128

#define AFS_PERSIST_MAGIC 0x31534641U
#define AFS_PERSIST_VERSION 1U
#define AFS_PERSIST_SECTORS 20U
#define AFS_PERSIST_LBA 1U

#define AFS_MODE_OWNER_R 0x001U
#define AFS_MODE_OWNER_W 0x002U
#define AFS_MODE_GROUP_R 0x004U
#define AFS_MODE_GROUP_W 0x008U
#define AFS_MODE_OTHER_R 0x010U
#define AFS_MODE_OTHER_W 0x020U

typedef struct {
    int used;
    afs_node_type_t type;
    int parent;
    char name[AFS_NAME_MAX];
    char owner[16];
    char group[16];
    unsigned int mode;
    char data[AFS_DATA_MAX];
    size_t size;
} afs_node_t;

typedef struct {
    unsigned int magic;
    unsigned int version;
    unsigned int node_count;
    unsigned int current_dir_index;
    unsigned int checksum;
} afs_snapshot_header_t;

static afs_node_t nodes[AFS_MAX_NODES];
static unsigned char afs_persist_buffer[AFS_PERSIST_SECTORS * STORAGE_SECTOR_SIZE];
static int current_dir = 0;

static void afs_copy_limited(char* dst, const char* src, size_t max) {
    size_t i = 0U;
    if (max == 0U) {
        return;
    }
    while (src[i] != '\0' && i + 1U < max) {
        dst[i] = src[i];
        i++;
    }
    dst[i] = '\0';
}

static void afs_append_limited(char* dst, size_t max, const char* src) {
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

static void afs_zero_bytes(unsigned char* dst, size_t count) {
    size_t i;
    for (i = 0U; i < count; ++i) {
        dst[i] = 0U;
    }
}

static void afs_copy_bytes(unsigned char* dst, const unsigned char* src, size_t count) {
    size_t i;
    for (i = 0U; i < count; ++i) {
        dst[i] = src[i];
    }
}

static unsigned int afs_checksum_bytes(const unsigned char* data, size_t count) {
    size_t i;
    unsigned int sum = 0U;
    for (i = 0U; i < count; ++i) {
        sum = (sum << 5U) - sum + (unsigned int)data[i];
    }
    return sum;
}

static int afs_name_eq(const char* a, const char* b) {
    return kstrcmp(a, b) == 0 ? 1 : 0;
}

static int afs_find_free_node(void) {
    int i;
    for (i = 1; i < AFS_MAX_NODES; ++i) {
        if (nodes[i].used == 0) {
            return i;
        }
    }
    return -1;
}

static int afs_find_child(int parent, const char* name) {
    int i;
    for (i = 1; i < AFS_MAX_NODES; ++i) {
        if (nodes[i].used != 0 && nodes[i].parent == parent && afs_name_eq(nodes[i].name, name) != 0) {
            return i;
        }
    }
    return -1;
}

static int afs_node_has_children(int index) {
    int i;
    for (i = 1; i < AFS_MAX_NODES; ++i) {
        if (nodes[i].used != 0 && nodes[i].parent == index) {
            return 1;
        }
    }
    return 0;
}

static void afs_clear_node(int index) {
    nodes[index].used = 0;
    nodes[index].type = AFS_NODE_DIR;
    nodes[index].parent = 0;
    nodes[index].name[0] = '\0';
    nodes[index].owner[0] = '\0';
    nodes[index].group[0] = '\0';
    nodes[index].data[0] = '\0';
    nodes[index].size = 0U;
    nodes[index].mode = 0U;
}

static int afs_is_ancestor_or_same(int maybe_parent, int index) {
    int cursor = index;
    while (cursor >= 0) {
        if (cursor == maybe_parent) {
            return 1;
        }
        if (cursor == 0) {
            break;
        }
        cursor = nodes[cursor].parent;
    }
    return 0;
}

static void afs_copy_name_from_index(int index, char* out, size_t max) {
    if (index < 0 || index >= AFS_MAX_NODES || nodes[index].used == 0) {
        if (max > 0U) {
            out[0] = '\0';
        }
        return;
    }
    afs_copy_limited(out, nodes[index].name, max);
}

static int afs_current_inside(int index) {
    int cursor = current_dir;
    while (cursor > 0) {
        if (cursor == index) {
            return 1;
        }
        cursor = nodes[cursor].parent;
    }
    return 0;
}

static void afs_remove_subtree(int index) {
    int i;
    for (i = 1; i < AFS_MAX_NODES; ++i) {
        if (nodes[i].used != 0 && nodes[i].parent == index) {
            afs_remove_subtree(i);
        }
    }
    afs_clear_node(index);
}

static int afs_is_owner(int index) {
    const user_t* current = user_current();
    if (current == (const user_t*)0) {
        return 0;
    }
    return afs_name_eq(nodes[index].owner, current->username);
}

static int afs_in_group(int index) {
    return afs_name_eq(nodes[index].group, user_current_group());
}

static int afs_mode_allows_read(unsigned int mode, int owner_match, int group_match) {
    if (owner_match != 0) {
        return (mode & AFS_MODE_OWNER_R) != 0U ? 1 : 0;
    }
    if (group_match != 0) {
        return (mode & AFS_MODE_GROUP_R) != 0U ? 1 : 0;
    }
    return (mode & AFS_MODE_OTHER_R) != 0U ? 1 : 0;
}

static int afs_mode_allows_write(unsigned int mode, int owner_match, int group_match) {
    if (owner_match != 0) {
        return (mode & AFS_MODE_OWNER_W) != 0U ? 1 : 0;
    }
    if (group_match != 0) {
        return (mode & AFS_MODE_GROUP_W) != 0U ? 1 : 0;
    }
    return (mode & AFS_MODE_OTHER_W) != 0U ? 1 : 0;
}

static int afs_can_read_node(int index) {
    if (index < 0 || index >= AFS_MAX_NODES || nodes[index].used == 0) {
        return 0;
    }
    if (user_is_master() != 0) {
        return 1;
    }
    return afs_mode_allows_read(nodes[index].mode, afs_is_owner(index), afs_in_group(index));
}

static int afs_can_write_node(int index) {
    if (index < 0 || index >= AFS_MAX_NODES || nodes[index].used == 0) {
        return 0;
    }
    if (user_is_master() != 0) {
        return 1;
    }
    return afs_mode_allows_write(nodes[index].mode, afs_is_owner(index), afs_in_group(index));
}

static int afs_can_change_perms(int index) {
    if (index < 0 || index >= AFS_MAX_NODES || nodes[index].used == 0) {
        return 0;
    }
    if (user_is_master() != 0) {
        return 1;
    }
    return afs_is_owner(index);
}

static void afs_set_meta(int index, const char* owner, const char* group, unsigned int mode) {
    afs_copy_limited(nodes[index].owner, owner, sizeof(nodes[index].owner));
    afs_copy_limited(nodes[index].group, group, sizeof(nodes[index].group));
    nodes[index].mode = mode;
}

static unsigned int afs_mode_from_triplets(unsigned int owner_bits, unsigned int group_bits, unsigned int other_bits) {
    unsigned int mode = 0U;
    if ((owner_bits & 4U) != 0U) { mode |= AFS_MODE_OWNER_R; }
    if ((owner_bits & 2U) != 0U) { mode |= AFS_MODE_OWNER_W; }
    if ((group_bits & 4U) != 0U) { mode |= AFS_MODE_GROUP_R; }
    if ((group_bits & 2U) != 0U) { mode |= AFS_MODE_GROUP_W; }
    if ((other_bits & 4U) != 0U) { mode |= AFS_MODE_OTHER_R; }
    if ((other_bits & 2U) != 0U) { mode |= AFS_MODE_OTHER_W; }
    return mode;
}

static unsigned int afs_mode_private(void) {
    return afs_mode_from_triplets(6U, 0U, 0U);
}

static unsigned int afs_mode_team(void) {
    return afs_mode_from_triplets(6U, 6U, 0U);
}

static unsigned int afs_mode_public(void) {
    return afs_mode_from_triplets(6U, 4U, 4U);
}

static unsigned int afs_mode_shared(void) {
    return afs_mode_from_triplets(6U, 6U, 6U);
}

static int afs_parse_numeric_mode(const char* text, unsigned int* mode_out) {
    unsigned int owner_bits;
    unsigned int group_bits;
    unsigned int other_bits;
    if (text == (const char*)0 || text[0] == '\0' || text[1] == '\0' || text[2] == '\0' || text[3] != '\0') {
        return -1;
    }
    if (text[0] < '0' || text[0] > '7' || text[1] < '0' || text[1] > '7' || text[2] < '0' || text[2] > '7') {
        return -1;
    }
    owner_bits = (unsigned int)(text[0] - '0');
    group_bits = (unsigned int)(text[1] - '0');
    other_bits = (unsigned int)(text[2] - '0');
    *mode_out = afs_mode_from_triplets(owner_bits, group_bits, other_bits);
    return 0;
}

static void afs_mode_to_text(unsigned int mode, char* out, size_t max) {
    if (max < 10U) {
        if (max > 0U) {
            out[0] = '\0';
        }
        return;
    }
    out[0] = (mode & AFS_MODE_OWNER_R) != 0U ? 'r' : '-';
    out[1] = (mode & AFS_MODE_OWNER_W) != 0U ? 'w' : '-';
    out[2] = '-';
    out[3] = (mode & AFS_MODE_GROUP_R) != 0U ? 'r' : '-';
    out[4] = (mode & AFS_MODE_GROUP_W) != 0U ? 'w' : '-';
    out[5] = '-';
    out[6] = (mode & AFS_MODE_OTHER_R) != 0U ? 'r' : '-';
    out[7] = (mode & AFS_MODE_OTHER_W) != 0U ? 'w' : '-';
    out[8] = '-';
    out[9] = '\0';
}

static int afs_create_node(int parent, const char* name, afs_node_type_t type, const char* owner, const char* group, unsigned int mode) {
    int index;
    if (parent < 0 || parent >= AFS_MAX_NODES) {
        return AFS_ERR_INVALID;
    }
    if (nodes[parent].used == 0 || nodes[parent].type != AFS_NODE_DIR) {
        return AFS_ERR_INVALID;
    }
    if (kstrlen(name) == 0U || kstrlen(name) >= AFS_NAME_MAX) {
        return AFS_ERR_INVALID;
    }
    if (afs_find_child(parent, name) >= 0) {
        return AFS_ERR_EXISTS;
    }
    if (afs_can_write_node(parent) == 0) {
        return AFS_ERR_DENIED;
    }

    index = afs_find_free_node();
    if (index < 0) {
        return AFS_ERR_NO_SPACE;
    }

    nodes[index].used = 1;
    nodes[index].type = type;
    nodes[index].parent = parent;
    afs_copy_limited(nodes[index].name, name, AFS_NAME_MAX);
    afs_set_meta(index, owner, group, mode);
    nodes[index].size = 0U;
    nodes[index].data[0] = '\0';
    return index;
}

static int afs_create_boot_node(int parent, const char* name, afs_node_type_t type, const char* owner, const char* group, unsigned int mode) {
    int index;
    if (parent < 0 || parent >= AFS_MAX_NODES) {
        return AFS_ERR_INVALID;
    }
    if (nodes[parent].used == 0 || nodes[parent].type != AFS_NODE_DIR) {
        return AFS_ERR_INVALID;
    }
    if (kstrlen(name) == 0U || kstrlen(name) >= AFS_NAME_MAX) {
        return AFS_ERR_INVALID;
    }
    if (afs_find_child(parent, name) >= 0) {
        return AFS_ERR_EXISTS;
    }

    index = afs_find_free_node();
    if (index < 0) {
        return AFS_ERR_NO_SPACE;
    }

    nodes[index].used = 1;
    nodes[index].type = type;
    nodes[index].parent = parent;
    afs_copy_limited(nodes[index].name, name, AFS_NAME_MAX);
    afs_set_meta(index, owner, group, mode);
    nodes[index].size = 0U;
    nodes[index].data[0] = '\0';
    return index;
}

static int afs_next_part(const char* path, size_t* offset, char* part, size_t max) {
    size_t j = 0U;
    while (path[*offset] == '/') {
        (*offset)++;
    }
    if (path[*offset] == '\0') {
        return 0;
    }
    while (path[*offset] != '\0' && path[*offset] != '/') {
        if (j + 1U >= max) {
            return -1;
        }
        part[j++] = path[*offset];
        (*offset)++;
    }
    part[j] = '\0';
    return 1;
}

static void afs_home_path(char* out, size_t max) {
    afs_copy_limited(out, user_current()->home, max);
}

static int afs_resolve_path(const char* path) {
    int current;
    size_t offset;
    char part[AFS_NAME_MAX];
    int rc;
    char expanded[AFS_PATH_MAX];

    if (path == (const char*)0 || path[0] == '\0') {
        return current_dir;
    }

    if (path[0] == '~') {
        size_t pos = 0U;
        afs_home_path(expanded, sizeof(expanded));
        pos = kstrlen(expanded);
        if (path[1] == '/' && pos + kstrlen(path + 1) < sizeof(expanded)) {
            afs_copy_limited(expanded + pos, path + 1, sizeof(expanded) - pos);
        }
        path = expanded;
    }

    current = path[0] == '/' ? 0 : current_dir;
    offset = path[0] == '/' ? 1U : 0U;

    while ((rc = afs_next_part(path, &offset, part, sizeof(part))) > 0) {
        int child;
        if (afs_name_eq(part, ".") != 0) {
            continue;
        }
        if (afs_name_eq(part, "..") != 0) {
            if (current != 0) {
                current = nodes[current].parent;
            }
            continue;
        }
        child = afs_find_child(current, part);
        if (child < 0) {
            return AFS_ERR_NOT_FOUND;
        }
        current = child;
    }

    if (rc < 0) {
        return AFS_ERR_INVALID;
    }

    return current;
}

static int afs_split_parent(const char* path, char* parent_out, size_t parent_max, char* name_out, size_t name_max) {
    size_t len = kstrlen(path);
    size_t last = 0U;
    size_t i;

    if (len == 0U || len >= AFS_PATH_MAX) {
        return AFS_ERR_INVALID;
    }

    for (i = len; i > 0U; --i) {
        if (path[i - 1U] == '/') {
            last = i - 1U;
            break;
        }
    }

    if (last == 0U && path[0] != '/') {
        afs_copy_limited(parent_out, "", parent_max);
        afs_copy_limited(name_out, path, name_max);
        return AFS_OK;
    }

    if (last == 0U && path[0] == '/') {
        afs_copy_limited(parent_out, "/", parent_max);
        afs_copy_limited(name_out, path + 1U, name_max);
        return AFS_OK;
    }

    if (last + 1U >= len || last >= parent_max) {
        return AFS_ERR_INVALID;
    }

    for (i = 0U; i < last; ++i) {
        parent_out[i] = path[i];
    }
    parent_out[last] = '\0';
    afs_copy_limited(name_out, path + last + 1U, name_max);
    return AFS_OK;
}

static void afs_build_path_from_index(int index, char* out, size_t max) {
    int stack[AFS_MAX_NODES];
    size_t depth = 0U;
    size_t pos = 0U;
    size_t s;

    if (max == 0U) {
        return;
    }

    if (index <= 0) {
        afs_copy_limited(out, "/", max);
        return;
    }

    while (index > 0 && depth < AFS_MAX_NODES) {
        stack[depth++] = index;
        index = nodes[index].parent;
    }

    out[pos++] = '/';
    for (s = depth; s > 0U; --s) {
        const char* name = nodes[stack[s - 1U]].name;
        size_t i;
        for (i = 0U; name[i] != '\0' && pos + 1U < max; ++i) {
            out[pos++] = name[i];
        }
        if (s > 1U && pos + 1U < max) {
            out[pos++] = '/';
        }
    }
    out[pos] = '\0';
}

static int afs_path_starts_with(const char* path, const char* prefix) {
    size_t prefix_len = kstrlen(prefix);
    if (kstrncmp(path, prefix, prefix_len) != 0) {
        return 0;
    }
    return path[prefix_len] == '\0' || path[prefix_len] == '/' ? 1 : 0;
}

static void afs_default_meta_for_parent(int parent, const char** owner_out, const char** group_out, unsigned int* mode_out) {
    char parent_path[AFS_PATH_MAX];
    afs_build_path_from_index(parent, parent_path, sizeof(parent_path));

    *owner_out = user_current()->username;
    *group_out = user_current_group();
    *mode_out = afs_mode_private();

    if (afs_path_starts_with(parent_path, "/temp") != 0) {
        *mode_out = afs_mode_shared();
        return;
    }

    if (afs_path_starts_with(parent_path, "/system") != 0 || afs_path_starts_with(parent_path, "/apps") != 0) {
        *owner_out = "system";
        *group_out = "system";
        *mode_out = afs_mode_public();
        return;
    }

    if (afs_path_starts_with(parent_path, "/home/") != 0) {
        *mode_out = afs_mode_private();
    }
}

static int afs_create_path(const char* path, afs_node_type_t type) {
    char parent_path[AFS_PATH_MAX];
    char name[AFS_NAME_MAX];
    int parent;
    int existing;
    const char* owner;
    const char* group;
    unsigned int mode;

    if (path == (const char*)0 || path[0] == '\0') {
        return AFS_ERR_INVALID;
    }

    if (afs_split_parent(path, parent_path, sizeof(parent_path), name, sizeof(name)) != AFS_OK) {
        return AFS_ERR_INVALID;
    }

    parent = afs_resolve_path(parent_path);
    if (parent < 0) {
        return parent;
    }

    if (nodes[parent].type != AFS_NODE_DIR) {
        return AFS_ERR_INVALID;
    }

    existing = afs_find_child(parent, name);
    if (existing >= 0) {
        if (nodes[existing].type == type) {
            return existing;
        }
        return AFS_ERR_EXISTS;
    }

    afs_default_meta_for_parent(parent, &owner, &group, &mode);
    return afs_create_node(parent, name, type, owner, group, mode);
}

static int afs_snapshot_store(void) {
    afs_snapshot_header_t* header = (afs_snapshot_header_t*)afs_persist_buffer;
    size_t payload_offset = sizeof(afs_snapshot_header_t);
    size_t payload_size = sizeof(nodes);
    unsigned int sector;

    if (storage_available() == 0) {
        return AFS_ERR_NO_SPACE;
    }
    if (payload_offset + payload_size > sizeof(afs_persist_buffer)) {
        return AFS_ERR_TOO_BIG;
    }

    afs_zero_bytes(afs_persist_buffer, sizeof(afs_persist_buffer));
    header->magic = AFS_PERSIST_MAGIC;
    header->version = AFS_PERSIST_VERSION;
    header->node_count = (unsigned int)AFS_MAX_NODES;
    header->current_dir_index = (unsigned int)current_dir;
    afs_copy_bytes(afs_persist_buffer + payload_offset, (const unsigned char*)nodes, payload_size);
    header->checksum = afs_checksum_bytes(afs_persist_buffer + payload_offset, payload_size);

    for (sector = 0U; sector < AFS_PERSIST_SECTORS; ++sector) {
        if (storage_write_sector(AFS_PERSIST_LBA + sector, afs_persist_buffer + (sector * STORAGE_SECTOR_SIZE)) != 0) {
            return AFS_ERR_NO_SPACE;
        }
    }
    return AFS_OK;
}

static int afs_snapshot_restore(void) {
    afs_snapshot_header_t* header = (afs_snapshot_header_t*)afs_persist_buffer;
    size_t payload_offset = sizeof(afs_snapshot_header_t);
    size_t payload_size = sizeof(nodes);
    unsigned int sector;
    unsigned int checksum;
    int i;

    if (storage_available() == 0) {
        return AFS_ERR_NOT_FOUND;
    }
    if (payload_offset + payload_size > sizeof(afs_persist_buffer)) {
        return AFS_ERR_TOO_BIG;
    }

    for (sector = 0U; sector < AFS_PERSIST_SECTORS; ++sector) {
        if (storage_read_sector(AFS_PERSIST_LBA + sector, afs_persist_buffer + (sector * STORAGE_SECTOR_SIZE)) != 0) {
            return AFS_ERR_NOT_FOUND;
        }
    }

    if (header->magic != AFS_PERSIST_MAGIC || header->version != AFS_PERSIST_VERSION || header->node_count != (unsigned int)AFS_MAX_NODES) {
        return AFS_ERR_NOT_FOUND;
    }

    checksum = afs_checksum_bytes(afs_persist_buffer + payload_offset, payload_size);
    if (checksum != header->checksum) {
        return AFS_ERR_INVALID;
    }

    afs_copy_bytes((unsigned char*)nodes, afs_persist_buffer + payload_offset, payload_size);
    if (header->current_dir_index < (unsigned int)AFS_MAX_NODES) {
        current_dir = (int)header->current_dir_index;
    } else {
        current_dir = 0;
    }

    if (nodes[0].used == 0 || nodes[0].type != AFS_NODE_DIR) {
        return AFS_ERR_INVALID;
    }

    for (i = 0; i < AFS_MAX_NODES; ++i) {
        if (nodes[i].used != 0) {
            nodes[i].name[AFS_NAME_MAX - 1] = '\0';
            nodes[i].owner[sizeof(nodes[i].owner) - 1U] = '\0';
            nodes[i].group[sizeof(nodes[i].group) - 1U] = '\0';
            nodes[i].data[sizeof(nodes[i].data) - 1U] = '\0';
            if (nodes[i].size >= sizeof(nodes[i].data)) {
                nodes[i].size = sizeof(nodes[i].data) - 1U;
                nodes[i].data[nodes[i].size] = '\0';
            }
            if (nodes[i].parent < 0 || nodes[i].parent >= AFS_MAX_NODES) {
                nodes[i].parent = 0;
            }
        }
    }

    return AFS_OK;
}

static void afs_auto_persist(void) {
    (void)afs_snapshot_store();
}

static void afs_create_default_tree(void) {
    int home_dir;
    int guest_dir;
    int readme_file;

    (void)afs_create_boot_node(0, "system", AFS_NODE_DIR, "system", "system", afs_mode_public());
    home_dir = afs_create_boot_node(0, "home", AFS_NODE_DIR, "system", "system", afs_mode_public());
    (void)afs_create_boot_node(0, "apps", AFS_NODE_DIR, "system", "system", afs_mode_public());
    {
        int apps_dir = afs_find_child(0, "apps");
        if (apps_dir >= 0) {
            (void)afs_create_boot_node(apps_dir, "commands", AFS_NODE_DIR, "system", "system", afs_mode_public());
            (void)afs_create_boot_node(apps_dir, "scripts", AFS_NODE_DIR, "system", "system", afs_mode_public());
        }
    }
    (void)afs_create_boot_node(0, "temp", AFS_NODE_DIR, "system", "system", afs_mode_shared());

    if (home_dir >= 0) {
        guest_dir = afs_create_boot_node(home_dir, "guest", AFS_NODE_DIR, "guest", "guest", afs_mode_private());
        if (guest_dir >= 0) {
            (void)afs_create_boot_node(guest_dir, "docs", AFS_NODE_DIR, "guest", "guest", afs_mode_private());
            (void)afs_create_boot_node(guest_dir, "bin", AFS_NODE_DIR, "guest", "guest", afs_mode_private());
        }
    }

    readme_file = afs_create_boot_node(0, "welcome.txt", AFS_NODE_FILE, "system", "system", afs_mode_public());
    if (readme_file >= 0) {
        afs_copy_limited(nodes[readme_file].data,
            "Welcome to CyralithFS\n"
            "Main folders: /system, /home, /apps, /temp\n"
            "Use 'stat <path>' to inspect owner, group and rights\n"
            "Use 'protect private/public/shared <path>' or 'chmod 644 <path>'\n"
            "Use 'owner <user> <path>' in system mode to change ownership\nUse 'cp', 'mv' and 'find' to manage files\nUse 'cmd add <name> <script>' to register custom commands\n",
            sizeof(nodes[readme_file].data));
        nodes[readme_file].size = kstrlen(nodes[readme_file].data);
    }
}

void afs_init(void) {
    int i;
    storage_init();
    for (i = 0; i < AFS_MAX_NODES; ++i) {
        nodes[i].used = 0;
        nodes[i].type = AFS_NODE_DIR;
        nodes[i].parent = 0;
        nodes[i].name[0] = '\0';
        nodes[i].owner[0] = '\0';
        nodes[i].group[0] = '\0';
        nodes[i].data[0] = '\0';
        nodes[i].size = 0U;
        nodes[i].mode = 0U;
    }

    nodes[0].used = 1;
    nodes[0].type = AFS_NODE_DIR;
    nodes[0].parent = 0;
    afs_copy_limited(nodes[0].name, "/", sizeof(nodes[0].name));
    afs_set_meta(0, "system", "system", afs_mode_public());
    current_dir = 0;

    if (afs_snapshot_restore() != AFS_OK) {
        afs_create_default_tree();
    }

    if (afs_set_home(user_current()->username) != AFS_OK) {
        current_dir = 0;
    }
}

const char* afs_name(void) {
    return "CyralithFS";
}

size_t afs_node_count(void) {
    size_t count = 0U;
    int i;
    for (i = 0; i < AFS_MAX_NODES; ++i) {
        if (nodes[i].used != 0) {
            count++;
        }
    }
    return count;
}

int afs_set_home(const char* username) {
    char path[AFS_PATH_MAX];
    afs_copy_limited(path, "/home/", sizeof(path));
    if (kstrlen(path) + kstrlen(username) + 1U >= sizeof(path)) {
        return AFS_ERR_INVALID;
    }
    afs_copy_limited(path + 6, username, sizeof(path) - 6U);
    return afs_cd(path);
}

void afs_pwd(char* out, size_t max) {
    afs_build_path_from_index(current_dir, out, max);
}

int afs_cd(const char* path) {
    int index = afs_resolve_path(path);
    if (index < 0) {
        return index;
    }
    if (nodes[index].type != AFS_NODE_DIR) {
        return AFS_ERR_INVALID;
    }
    if (afs_can_read_node(index) == 0) {
        return AFS_ERR_DENIED;
    }
    current_dir = index;
    return AFS_OK;
}

void afs_ls(const char* path) {
    int dir = afs_resolve_path(path);
    int i;
    if (dir < 0) {
        console_writeln("Folder not found.");
        return;
    }
    if (nodes[dir].type != AFS_NODE_DIR) {
        console_writeln("That is not a folder.");
        return;
    }
    if (afs_can_read_node(dir) == 0) {
        console_writeln("Access denied.");
        return;
    }

    console_writeln("Contents:");
    for (i = 1; i < AFS_MAX_NODES; ++i) {
        char mode_text[10];
        if (nodes[i].used != 0 && nodes[i].parent == dir) {
            afs_mode_to_text(nodes[i].mode, mode_text, sizeof(mode_text));
            console_write("  ");
            console_write(nodes[i].type == AFS_NODE_DIR ? "[dir]  " : "[file] ");
            console_write(nodes[i].name);
            console_write("  ");
            console_write(mode_text);
            console_write("  owner=");
            console_write(nodes[i].owner);
            console_write(":");
            console_writeln(nodes[i].group);
        }
    }
}

int afs_mkdir(const char* path) {
    int rc = afs_create_path(path, AFS_NODE_DIR);
    if (rc >= 0) {
        afs_auto_persist();
    }
    return rc;
}

int afs_touch(const char* path) {
    int rc = afs_create_path(path, AFS_NODE_FILE);
    if (rc >= 0) {
        afs_auto_persist();
    }
    return rc;
}

int afs_write_file(const char* path, const char* text) {
    int index = afs_create_path(path, AFS_NODE_FILE);
    size_t len;
    if (index < 0) {
        return index;
    }
    if (nodes[index].type != AFS_NODE_FILE) {
        return AFS_ERR_INVALID;
    }
    if (afs_can_write_node(index) == 0) {
        return AFS_ERR_DENIED;
    }
    len = kstrlen(text);
    if (len >= sizeof(nodes[index].data)) {
        return AFS_ERR_TOO_BIG;
    }
    afs_copy_limited(nodes[index].data, text, sizeof(nodes[index].data));
    nodes[index].size = len;
    afs_auto_persist();
    return AFS_OK;
}

int afs_append_file(const char* path, const char* text) {
    int index = afs_create_path(path, AFS_NODE_FILE);
    size_t add_len;
    size_t pos;
    size_t i;
    if (index < 0) {
        return index;
    }
    if (nodes[index].type != AFS_NODE_FILE) {
        return AFS_ERR_INVALID;
    }
    if (afs_can_write_node(index) == 0) {
        return AFS_ERR_DENIED;
    }
    add_len = kstrlen(text);
    pos = nodes[index].size;
    if (pos + add_len >= sizeof(nodes[index].data)) {
        return AFS_ERR_TOO_BIG;
    }
    for (i = 0U; i < add_len; ++i) {
        nodes[index].data[pos + i] = text[i];
    }
    nodes[index].size += add_len;
    nodes[index].data[nodes[index].size] = '\0';
    afs_auto_persist();
    return AFS_OK;
}

int afs_read_file(const char* path, char* out, size_t max) {
    int index = afs_resolve_path(path);
    if (index < 0 || nodes[index].type != AFS_NODE_FILE) {
        return AFS_ERR_NOT_FOUND;
    }
    if (afs_can_read_node(index) == 0) {
        return AFS_ERR_DENIED;
    }
    if (nodes[index].size + 1U > max) {
        return AFS_ERR_TOO_BIG;
    }
    afs_copy_limited(out, nodes[index].data, max);
    return (int)nodes[index].size;
}

int afs_rm(const char* path) {
    int index = afs_resolve_path(path);
    int parent;
    if (index <= 0) {
        return AFS_ERR_INVALID;
    }
    if (nodes[index].type != AFS_NODE_FILE) {
        return AFS_ERR_INVALID;
    }
    parent = nodes[index].parent;
    if (afs_can_write_node(index) == 0 || afs_can_write_node(parent) == 0) {
        return AFS_ERR_DENIED;
    }
    afs_clear_node(index);
    afs_auto_persist();
    return AFS_OK;
}

int afs_rmdir(const char* path, int recursive) {
    int index = afs_resolve_path(path);
    int parent;
    if (index <= 0) {
        return AFS_ERR_INVALID;
    }
    if (nodes[index].type != AFS_NODE_DIR) {
        return AFS_ERR_INVALID;
    }
    if (nodes[index].parent == 0) {
        return AFS_ERR_DENIED;
    }
    if (afs_current_inside(index) != 0) {
        return AFS_ERR_BUSY;
    }
    parent = nodes[index].parent;
    if (afs_can_write_node(index) == 0 || afs_can_write_node(parent) == 0) {
        return AFS_ERR_DENIED;
    }
    if (afs_node_has_children(index) != 0 && recursive == 0) {
        return AFS_ERR_NOT_EMPTY;
    }
    afs_remove_subtree(index);
    afs_auto_persist();
    return AFS_OK;
}


static int afs_copy_subtree(int src_index, int dest_parent, const char* dest_name, int recursive) {
    int created;
    int i;
    if (src_index <= 0 || dest_parent < 0 || dest_parent >= AFS_MAX_NODES) {
        return AFS_ERR_INVALID;
    }
    if (afs_can_read_node(src_index) == 0 || afs_can_write_node(dest_parent) == 0) {
        return AFS_ERR_DENIED;
    }
    if (nodes[dest_parent].type != AFS_NODE_DIR) {
        return AFS_ERR_INVALID;
    }
    created = afs_create_node(dest_parent, dest_name, nodes[src_index].type, nodes[src_index].owner, nodes[src_index].group, nodes[src_index].mode);
    if (created < 0) {
        return created;
    }
    if (nodes[src_index].type == AFS_NODE_FILE) {
        afs_copy_limited(nodes[created].data, nodes[src_index].data, sizeof(nodes[created].data));
        nodes[created].size = nodes[src_index].size;
        return created;
    }
    if (recursive == 0) {
        afs_clear_node(created);
        return AFS_ERR_NOT_EMPTY;
    }
    for (i = 1; i < AFS_MAX_NODES; ++i) {
        if (nodes[i].used != 0 && nodes[i].parent == src_index) {
            int rc = afs_copy_subtree(i, created, nodes[i].name, recursive);
            if (rc < 0) {
                afs_remove_subtree(created);
                return rc;
            }
        }
    }
    return created;
}

int afs_copy(const char* src, const char* dst, int recursive) {
    int src_index = afs_resolve_path(src);
    int dest_index;
    int dest_parent;
    char dest_name[AFS_NAME_MAX];
    char parent_path[AFS_PATH_MAX];
    if (src_index < 0) {
        return src_index;
    }
    if (src_index == 0) {
        return AFS_ERR_DENIED;
    }
    if (afs_can_read_node(src_index) == 0) {
        return AFS_ERR_DENIED;
    }
    dest_index = afs_resolve_path(dst);
    if (dest_index >= 0) {
        if (nodes[dest_index].type != AFS_NODE_DIR) {
            return AFS_ERR_EXISTS;
        }
        dest_parent = dest_index;
        afs_copy_name_from_index(src_index, dest_name, sizeof(dest_name));
    } else {
        if (afs_split_parent(dst, parent_path, sizeof(parent_path), dest_name, sizeof(dest_name)) != AFS_OK) {
            return AFS_ERR_INVALID;
        }
        dest_parent = afs_resolve_path(parent_path);
        if (dest_parent < 0) {
            return dest_parent;
        }
    }
    if (nodes[src_index].type == AFS_NODE_DIR && afs_is_ancestor_or_same(src_index, dest_parent) != 0) {
        return AFS_ERR_INVALID;
    }
    {
        int rc = afs_copy_subtree(src_index, dest_parent, dest_name, recursive);
        if (rc < 0) {
            return rc;
        }
    }
    afs_auto_persist();
    return AFS_OK;
}

int afs_move(const char* src, const char* dst) {
    int src_index = afs_resolve_path(src);
    int src_parent;
    int dest_index;
    int dest_parent;
    char dest_name[AFS_NAME_MAX];
    char parent_path[AFS_PATH_MAX];
    if (src_index <= 0) {
        return src_index < 0 ? src_index : AFS_ERR_INVALID;
    }
    src_parent = nodes[src_index].parent;
    if (afs_can_write_node(src_index) == 0 || afs_can_write_node(src_parent) == 0) {
        return AFS_ERR_DENIED;
    }
    dest_index = afs_resolve_path(dst);
    if (dest_index >= 0) {
        if (nodes[dest_index].type != AFS_NODE_DIR) {
            return AFS_ERR_EXISTS;
        }
        dest_parent = dest_index;
        afs_copy_name_from_index(src_index, dest_name, sizeof(dest_name));
    } else {
        if (afs_split_parent(dst, parent_path, sizeof(parent_path), dest_name, sizeof(dest_name)) != AFS_OK) {
            return AFS_ERR_INVALID;
        }
        dest_parent = afs_resolve_path(parent_path);
        if (dest_parent < 0) {
            return dest_parent;
        }
    }
    if (nodes[dest_parent].type != AFS_NODE_DIR) {
        return AFS_ERR_INVALID;
    }
    if (nodes[src_index].type == AFS_NODE_DIR && afs_is_ancestor_or_same(src_index, dest_parent) != 0) {
        return AFS_ERR_INVALID;
    }
    if (afs_can_write_node(dest_parent) == 0) {
        return AFS_ERR_DENIED;
    }
    if (afs_find_child(dest_parent, dest_name) >= 0) {
        return AFS_ERR_EXISTS;
    }
    if (kstrlen(dest_name) == 0U || kstrlen(dest_name) >= AFS_NAME_MAX) {
        return AFS_ERR_INVALID;
    }
    nodes[src_index].parent = dest_parent;
    afs_copy_limited(nodes[src_index].name, dest_name, sizeof(nodes[src_index].name));
    afs_auto_persist();
    return AFS_OK;
}

static void afs_find_walk(int index, const char* needle, unsigned int* count) {
    int i;
    char full_path[AFS_PATH_MAX];
    if (index < 0 || index >= AFS_MAX_NODES || nodes[index].used == 0) {
        return;
    }
    if (afs_can_read_node(index) == 0) {
        return;
    }
    afs_build_path_from_index(index, full_path, sizeof(full_path));
    if (index != 0 && (kcontains_ci(nodes[index].name, needle) != 0 || kcontains_ci(full_path, needle) != 0)) {
        console_write("  ");
        console_write(nodes[index].type == AFS_NODE_DIR ? "[dir]  " : "[file] ");
        console_writeln(full_path);
        (*count)++;
    }
    if (nodes[index].type != AFS_NODE_DIR) {
        return;
    }
    for (i = 1; i < AFS_MAX_NODES; ++i) {
        if (nodes[i].used != 0 && nodes[i].parent == index) {
            afs_find_walk(i, needle, count);
        }
    }
}

void afs_find(const char* start, const char* needle) {
    int start_index = afs_resolve_path((start == (const char*)0 || start[0] == '\0') ? "" : start);
    unsigned int count = 0U;
    if (needle == (const char*)0 || needle[0] == '\0') {
        console_writeln("Please provide a search term.");
        return;
    }
    if (start_index < 0) {
        console_writeln("Start folder not found.");
        return;
    }
    console_writeln("Matches:");
    afs_find_walk(start_index, needle, &count);
    if (count == 0U) {
        console_writeln("  (no matches)");
    }
}

int afs_exists(const char* path) {
    return afs_resolve_path(path) >= 0 ? 1 : 0;
}

int afs_is_dir(const char* path) {
    int index = afs_resolve_path(path);
    if (index < 0) {
        return 0;
    }
    return nodes[index].type == AFS_NODE_DIR ? 1 : 0;
}

static void afs_append_size(char* out, size_t max, size_t value) {
    char reversed[20];
    char size_buf[20];
    size_t pos = 0U;
    size_t out_pos = 0U;
    if (value == 0U) {
        afs_append_limited(out, max, "0");
        return;
    }
    while (value > 0U && pos + 1U < sizeof(reversed)) {
        reversed[pos++] = (char)('0' + (value % 10U));
        value /= 10U;
    }
    while (pos > 0U && out_pos + 1U < sizeof(size_buf)) {
        size_buf[out_pos++] = reversed[--pos];
    }
    size_buf[out_pos] = '\0';
    afs_append_limited(out, max, size_buf);
}

int afs_stat(const char* path, char* out, size_t max) {
    int index = afs_resolve_path(path);
    char full_path[AFS_PATH_MAX];
    char mode_text[10];
    if (index < 0) {
        return index;
    }
    if (afs_can_read_node(index) == 0) {
        return AFS_ERR_DENIED;
    }
    afs_build_path_from_index(index, full_path, sizeof(full_path));
    afs_mode_to_text(nodes[index].mode, mode_text, sizeof(mode_text));
    out[0] = '\0';
    afs_append_limited(out, max, "Path: ");
    afs_append_limited(out, max, full_path);
    afs_append_limited(out, max, "\nType: ");
    afs_append_limited(out, max, nodes[index].type == AFS_NODE_DIR ? "directory" : "file");
    afs_append_limited(out, max, "\nOwner: ");
    afs_append_limited(out, max, nodes[index].owner);
    afs_append_limited(out, max, "\nGroup: ");
    afs_append_limited(out, max, nodes[index].group);
    afs_append_limited(out, max, "\nRights: ");
    afs_append_limited(out, max, mode_text);
    afs_append_limited(out, max, "\nSize: ");
    afs_append_size(out, max, nodes[index].size);
    afs_append_limited(out, max, " bytes");
    return AFS_OK;
}

int afs_protect(const char* path, const char* preset_or_mode) {
    int index = afs_resolve_path(path);
    unsigned int new_mode;
    if (index < 0) {
        return index;
    }
    if (afs_can_change_perms(index) == 0) {
        return AFS_ERR_DENIED;
    }
    if (kstrcmp(preset_or_mode, "private") == 0) {
        new_mode = afs_mode_private();
    } else if (kstrcmp(preset_or_mode, "team") == 0) {
        new_mode = afs_mode_team();
    } else if (kstrcmp(preset_or_mode, "public") == 0) {
        new_mode = afs_mode_public();
    } else if (kstrcmp(preset_or_mode, "shared") == 0) {
        new_mode = afs_mode_shared();
    } else if (afs_parse_numeric_mode(preset_or_mode, &new_mode) == 0) {
    } else {
        return AFS_ERR_INVALID;
    }
    nodes[index].mode = new_mode;
    afs_auto_persist();
    return AFS_OK;
}

int afs_chown(const char* path, const char* owner, const char* group) {
    int index = afs_resolve_path(path);
    const char* group_to_use = group;
    if (index < 0) {
        return index;
    }
    if (user_is_master() == 0) {
        return AFS_ERR_DENIED;
    }
    if (owner == (const char*)0 || owner[0] == '\0') {
        return AFS_ERR_INVALID;
    }
    if (user_exists(owner) == 0) {
        return AFS_ERR_INVALID;
    }
    if (afs_name_eq(owner, "master") != 0 || afs_name_eq(owner, "root") != 0 || afs_name_eq(owner, "system") != 0) {
        owner = "system";
    }
    if (group_to_use == (const char*)0 || group_to_use[0] == '\0') {
        group_to_use = afs_name_eq(owner, "system") != 0 ? "system" : owner;
    }
    afs_set_meta(index, owner, group_to_use, nodes[index].mode);
    afs_auto_persist();
    return AFS_OK;
}

int afs_persistence_available(void) {
    return storage_available();
}

int afs_save_persistent(void) {
    return afs_snapshot_store();
}

int afs_load_persistent(void) {
    int rc = afs_snapshot_restore();
    if (rc == AFS_OK && afs_set_home(user_current()->username) != AFS_OK) {
        current_dir = 0;
    }
    return rc;
}

const char* afs_persistence_name(void) {
    return storage_name();
}
