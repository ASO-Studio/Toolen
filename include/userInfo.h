/*
 * userInfo.h - Lightweight user and group information library
 */

#ifndef USERINFO_H
#define USERINFO_H

#include <sys/types.h>
#include <stdbool.h>

// User information structure
typedef struct {
    uid_t uid;
    gid_t gid;
    const char *name;
    const char *home_dir;
    const char *shell;
} user_info_t;

// Group information structure
typedef struct {
    gid_t gid;
    const char *name;
} group_info_t;

// Function prototypes
const char *get_username(uid_t uid);
const char *get_groupname(gid_t gid);
bool get_user_info(uid_t uid, user_info_t *info);
bool get_group_info(gid_t gid, group_info_t *info);
void free_user_info(user_info_t *info);
void free_group_info(group_info_t *info);

// File parsing functions (for internal use but exposed if needed)
bool parse_passwd_file(void);
bool parse_group_file(void);

#endif /* USERINFO_H */
