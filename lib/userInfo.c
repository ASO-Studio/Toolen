/*
 * userInfo.c - Lightweight user and group information library implementation
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "userInfo.h"

// Internal structures
typedef struct passwd_entry {
	uid_t uid;
	gid_t gid;
	char *name;
	char *home_dir;
	char *shell;
	struct passwd_entry *next;
} passwd_entry_t;

typedef struct group_entry {
	gid_t gid;
	char *name;
	struct group_entry *next;
} group_entry_t;

// Linked list heads
static passwd_entry_t *passwd_list = NULL;
static group_entry_t *group_list = NULL;

// Internal helper functions
static char *trim_whitespace(char *str) {
	if (!str) return NULL;
	
	// Trim leading space
	while (*str && (*str == ' ' || *str == '\t')) str++;
	
	if (!*str) return str;
	
	// Trim trailing space
	char *end = str + strlen(str) - 1;
	while (end > str && (*end == ' ' || *end == '\t' || *end == '\n')) {
		*end-- = '\0';
	}
	
	return str;
}

static void free_passwd_list(void) {
	passwd_entry_t *current = passwd_list;
	while (current) {
		passwd_entry_t *next = current->next;
		free(current->name);
		free(current->home_dir);
		free(current->shell);
		free(current);
		current = next;
	}
	passwd_list = NULL;
}

static void free_group_list(void) {
	group_entry_t *current = group_list;
	while (current) {
		group_entry_t *next = current->next;
		free(current->name);
		free(current);
		current = next;
	}
	group_list = NULL;
}

bool parse_passwd_file(void) {
	FILE *fp = fopen("/etc/passwd", "r");
	if (!fp) {
		return false;
	}
	
	// Free existing list if any
	free_passwd_list();
	
	char line[1024];
	passwd_entry_t *tail = NULL;
	
	while (fgets(line, sizeof(line), fp)) {
		// Skip comments and empty lines
		if (line[0] == '#' || line[0] == '\n') continue;
		
		char *token = strtok(line, ":");
		if (!token) continue;
		
		passwd_entry_t *entry = malloc(sizeof(passwd_entry_t));
		if (!entry) {
			fclose(fp);
			free_passwd_list();
			return false;
		}
		
		// Initialize the entry
		entry->name = strdup(trim_whitespace(token));
		token = strtok(NULL, ":");
		
		// Skip password field
		token = strtok(NULL, ":");
		if (token) entry->uid = atoi(trim_whitespace(token));
		
		token = strtok(NULL, ":");
		if (token) entry->gid = atoi(trim_whitespace(token));
		
		token = strtok(NULL, ":");
		if (token) {
			char *gecos = trim_whitespace(token);
			// Skip GECOS field for now
		}
		
		token = strtok(NULL, ":");
		if (token) entry->home_dir = strdup(trim_whitespace(token));
		
		token = strtok(NULL, ":");
		if (token) entry->shell = strdup(trim_whitespace(token));
		
		token = strtok(NULL, ":");
		// Ignore any remaining fields
		
		entry->next = NULL;
		
		// Add to list
		if (!passwd_list) {
			passwd_list = entry;
			tail = entry;
		} else {
			tail->next = entry;
			tail = entry;
		}
	}
	
	fclose(fp);
	return true;
}

bool parse_group_file(void) {
	FILE *fp = fopen("/etc/group", "r");
	if (!fp) {
		return false;
	}
	
	// Free existing list if any
	free_group_list();
	
	char line[1024];
	group_entry_t *tail = NULL;
	
	while (fgets(line, sizeof(line), fp)) {
		// Skip comments and empty lines
		if (line[0] == '#' || line[0] == '\n') continue;
		
		char *token = strtok(line, ":");
		if (!token) continue;
		
		group_entry_t *entry = malloc(sizeof(group_entry_t));
		if (!entry) {
			fclose(fp);
			free_group_list();
			return false;
		}
		
		// Initialize the entry
		entry->name = strdup(trim_whitespace(token));
		token = strtok(NULL, ":");
		
		// Skip password field
		token = strtok(NULL, ":");
		if (token) entry->gid = atoi(trim_whitespace(token));
		
		// Skip member list
		entry->next = NULL;
		
		// Add to list
		if (!group_list) {
			group_list = entry;
			tail = entry;
		} else {
			tail->next = entry;
			tail = entry;
		}
	}
	
	fclose(fp);
	return true;
}

const char *get_username(uid_t uid) {
	static char uid_buf[16];
	
	// Parse passwd file if not already done
	if (!passwd_list && !parse_passwd_file()) {
		snprintf(uid_buf, sizeof(uid_buf), "%d", uid);
		return uid_buf;
	}
	
	// Search for the UID
	passwd_entry_t *current = passwd_list;
	while (current) {
		if (current->uid == uid) {
			return current->name;
		}
		current = current->next;
	}
	
	// Not found, return UID as string
	snprintf(uid_buf, sizeof(uid_buf), "%d", uid);
	return uid_buf;
}

const char *get_groupname(gid_t gid) {
	static char gid_buf[16];
	
	// Parse group file if not already done
	if (!group_list && !parse_group_file()) {
		snprintf(gid_buf, sizeof(gid_buf), "%d", gid);
		return gid_buf;
	}
	
	// Search for the GID
	group_entry_t *current = group_list;
	while (current) {
		if (current->gid == gid) {
			return current->name;
		}
		current = current->next;
	}
	
	// Not found, return GID as string
	snprintf(gid_buf, sizeof(gid_buf), "%d", gid);
	return gid_buf;
}

bool get_user_info(uid_t uid, user_info_t *info) {
	if (!info) return false;
	
	// Initialize with default values
	info->uid = uid;
	info->gid = (gid_t)-1;
	info->name = NULL;
	info->home_dir = NULL;
	info->shell = NULL;
	
	// Parse passwd file if not already done
	if (!passwd_list && !parse_passwd_file()) {
		return false;
	}
	
	// Search for the UID
	passwd_entry_t *current = passwd_list;
	while (current) {
		if (current->uid == uid) {
			info->gid = current->gid;
			info->name = current->name;
			info->home_dir = current->home_dir;
			info->shell = current->shell;
			return true;
		}
		current = current->next;
	}
	
	return false;
}

bool get_group_info(gid_t gid, group_info_t *info) {
	if (!info) return false;
	
	// Initialize with default values
	info->gid = gid;
	info->name = NULL;
	
	// Parse group file if not already done
	if (!group_list && !parse_group_file()) {
		return false;
	}
	
	// Search for the GID
	group_entry_t *current = group_list;
	while (current) {
		if (current->gid == gid) {
			info->name = current->name;
			return true;
		}
		current = current->next;
	}
	
	return false;
}

void free_user_info(user_info_t *info) {
	// This function exists for API symmetry but doesn't need to do anything
	// since we're not allocating memory for the returned strings
	(void)info;
}

void free_group_info(group_info_t *info) {
	// This function exists for API symmetry but doesn't need to do anything
	// since we're not allocating memory for the returned strings
	(void)info;
}

// Cleanup function (call this at program exit)
void userinfo_cleanup(void) {
	free_passwd_list();
	free_group_list();
}
