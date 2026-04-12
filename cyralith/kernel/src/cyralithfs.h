#ifndef CYRALITHFS_H
#define CYRALITHFS_H

#include <stddef.h>

typedef enum {
    AFS_NODE_DIR = 0,
    AFS_NODE_FILE = 1
} afs_node_type_t;

enum {
    AFS_OK = 0,
    AFS_ERR_NOT_FOUND = -1,
    AFS_ERR_DENIED = -2,
    AFS_ERR_INVALID = -3,
    AFS_ERR_EXISTS = -4,
    AFS_ERR_NO_SPACE = -5,
    AFS_ERR_TOO_BIG = -6,
    AFS_ERR_NOT_EMPTY = -7,
    AFS_ERR_BUSY = -8
};

void afs_init(void);
const char* afs_name(void);
size_t afs_node_count(void);
int afs_set_home(const char* username);
void afs_pwd(char* out, size_t max);
int afs_cd(const char* path);
void afs_ls(const char* path);
int afs_mkdir(const char* path);
int afs_touch(const char* path);
int afs_write_file(const char* path, const char* text);
int afs_append_file(const char* path, const char* text);
int afs_read_file(const char* path, char* out, size_t max);
int afs_rm(const char* path);
int afs_rmdir(const char* path, int recursive);
int afs_copy(const char* src, const char* dst, int recursive);
int afs_move(const char* src, const char* dst);
void afs_find(const char* start, const char* needle);
int afs_exists(const char* path);
int afs_is_dir(const char* path);
int afs_stat(const char* path, char* out, size_t max);
int afs_protect(const char* path, const char* preset_or_mode);
int afs_chown(const char* path, const char* owner, const char* group);
int afs_persistence_available(void);
int afs_save_persistent(void);
int afs_load_persistent(void);
const char* afs_persistence_name(void);


#endif
