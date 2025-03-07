//
//  disk.c
//  server
//
//  Created by Ali Mahouk on 12/10/17.
//  Copyright © 2017 Ali Mahouk. All rights reserved.
//

#include "disk.h"

#include <dirent.h>
#include <errno.h>
#include <pwd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include "util.h"


/**********************
 * Private Prototypes
 **********************/
struct path *about_file_get(struct path *);
int about_file_make(const struct path *);
void about_file_verify(const struct path *);
char *about_get(struct path *);
const char *config_default(void);
struct path *config_dir_get(void);
struct path *config_file_get(void);
int config_file_make(const struct path *);
int config_file_validate(const struct path *);
void config_file_verify(const struct path *);
void config_list_deserialise(const char *, struct token **);
void config_list_serialise(const struct token *, char **);
struct path *default_dir_get(struct path *);
struct path *errlog_file_get(void);
int errlog_file_make(const struct path *);
void errlog_file_verify(const struct path *);
char *property_name_get(const char *);
char *property_val_get(const char *);
struct path *readme_file_get(struct path *);
int readme_file_make(const struct path *);
const char *value_get(const char *, const struct token *);
/**********************/


struct path *about_file_get(struct path *root)
{
	struct path *path_dir_root;
	
	path_dir_root = path_copy(root);
	path_append(&path_dir_root, DP_FILE_ABOUT);
	
	return path_dir_root;
}

int about_file_make(const struct path *path)
{
	char *about;
	
	if (!path)
		return -1;
	
	about = "# ABOUT ME\n# --\n# Enter a name to identify yourself after this line.\n";
	writet(path, about);
	
	return 0;
}

void about_file_verify(const struct path *path)
{
	if (file_exists(path) != 1)
		about_file_make(path);
}

char *about_get(struct path *path)
{
	char *about;
	
	about = NULL;
	
	return about;
}

const char *config_default(void)
{
	char *config;
	struct path *path_dir_config;
	struct path *path_dir_docroot;
	struct token *config_list;
	struct token *config_docroot;
	
	/*
	 * The default config and root directories are in the user's
	 * home directory.
	 */
	path_dir_config = home_dir_get();
	path_dir_docroot = home_dir_get();
	
	path_append(&path_dir_config, DP_DIR_CONF);
	path_append(&path_dir_docroot, DP_DIR_DOCROOT);
	
	config_list = (struct token *)malloc(sizeof(*config_list));
	config_list->name = (char *)DP_CONF_HEADER;
	config_list->val = NULL;
	
	/* Remember to chain each new token to the previous one. */
	config_docroot = (struct token *)malloc(sizeof(*config_docroot));
	config_docroot->name = (char *)DP_CKEY_ROOT;
	config_docroot->val = path_str(path_dir_docroot);
	config_docroot->next = NULL;
	config_list->next = config_docroot;
	
	config_list_serialise(config_list, &config);
	
	if (config_list)
		free(config_list);
	
	if (config_docroot)
		free(config_docroot);
	
	if (path_dir_config)
		free(path_dir_config);
	
	if (path_dir_docroot)
		free(path_dir_docroot);
	
	return config;
}

struct path *config_dir_get(void)
{
	struct path *path_dir_home;
	
	path_dir_home = home_dir_get();
	path_append(&path_dir_home, DP_DIR_CONF);
	
	return path_dir_home;
}

struct path *config_file_get(void)
{
	struct path *path_dir_config;
	
	path_dir_config = config_dir_get();
	path_append(&path_dir_config, DP_FILE_CONF);
	
	return path_dir_config;
}

/*
 * Any existing config file will be overwritten.
 */
int config_file_make(const struct path *path)
{
	const char *config;
	
	if (!path)
		return -1;
	
	config = config_default();
	writet(path, config);
	
	if (config)
		free((char *)config);
	
	return 0;
}

int config_file_validate(const struct path *path)
{
	char *config;
	int valid;
	
	if (!path) {
		return -1;
	}
	
	valid = 0;
	
	readt(path, &config);
	
	if (strncmp(config, DP_CONF_HEADER, strlen(DP_CONF_HEADER)) == 0)
		valid = 1;
	
	return valid;
}

void config_file_verify(const struct path *path)
{
	if (file_exists(path) == 1) {
		if (config_file_validate(path) != 1)
		/* Corrupt config file; make a new one. */
			config_file_make(path);
	} else {
		/* No prior config; create a fresh one. */
		config_file_make(path);
	}
}

void config_list_deserialise(const char *list, struct token **out)
{
	struct token *config_list;
	size_t len_list;
	int eol; /* "End of line" */
	int comment;
	int len_line;
	
	if (!list ||
	    !out)
		return;
	
	config_list = NULL;
	eol = 1;
	comment = 0;
	len_line = 0;
	len_list = strlen(list);
	*out = NULL;
	
	/* Config list properties are delimted by newlines. */
	for (int i = 0; i < len_list; i++) {
		char c;
		
		c = list[i];
		
		/* Ignore comment lines. */
		if (eol) {
			if (c == DP_CONF_COMMENT)
				comment = 1;
			
			eol = 0;
		}
		
		if (c == '\n') {
			char *line;
			struct token *property;
			
			line = (char *)calloc(len_line + 1, sizeof(*line));
			strncpy(line, list + i - len_line, len_line);
			
			property = (struct token *)malloc(sizeof(*property));
			property->name = property_name_get(line);
			property->val = property_val_get(line);
			
			if (!config_list) {
				*out = (struct token *)malloc(sizeof(**out));
				(*out)->name = property_name_get(line);
				(*out)->val = property_val_get(line);
				config_list = *out;
			} else {
				struct token *property;
				
				/* Append a new property. */
				property = (struct token *)malloc(sizeof(*property));
				property->name = property_name_get(line);
				property->val = property_val_get(line);
				config_list->next = property;
				config_list = property;
			}
			
			comment = 0;
			eol = 1;
			len_line = 0;
			
			if (line)
				free(line);
		} else if (comment == 0) {
			len_line++;
		}
	}
	
	/* 8<-- Snip off the list. -- */
	if (config_list)
		config_list->next = NULL;
}

void config_list_serialise(const struct token *list, char **out)
{
	struct token *iter;
	size_t len;
	
	if (!list ||
	    !out)
		return;
	
	iter = (struct token *)list;
	len = 0;
	*out = NULL;
	
	while (iter) {
		char *line;
		size_t line_len;
		
		line_len = strlen(iter->name) + 1; /* +1 for \n */
		
		if (iter->val)
			line_len += strlen(iter->val) + 1; /* +1 for the space separator. */
		
		line = (char *)calloc(line_len, sizeof(*line));
		strcpy(line, iter->name);
		
		if (iter->val) {
			strcat(line, " ");
			strcat(line, iter->val);
		}
		
		strcat(line, "\n");
		
		if (*out) {
			*out = (char *)realloc(*out, (len + line_len) * sizeof(**out));
		} else {
			*out = (char *)calloc(len + line_len, sizeof(**out));
		}
		
		memcpy(&(*out)[len], line, line_len * sizeof(**out));
		
		len += line_len;
		iter = iter->next;
	}
	
	/* Expand the final-sized buffer to account for a null terminator. */
	if (*out) {
		*out = (char *)realloc(*out, (len + 1) * sizeof(**out));
		(*out)[len] = '\0';
	}
}

struct path *default_dir_get(struct path *root)
{
	struct path *path_dir_root;
	
	path_dir_root = path_copy(root);
	path_append(&path_dir_root, DP_DIR_DEFAULT);
	
	return path_dir_root;
}

/*
 * Returns the path to the root directory.
 */
struct path *directories_bootstrap(void)
{
	char *config;
	struct path *path_dir_config;
	struct path *path_dir_default;
	struct path *path_dir_root;
	struct path *path_file_about;
	struct path *path_file_config;
	struct path *path_file_errlog;
	struct path *path_file_readme;
	struct token *config_list;
	
	/*
	 * CHECKLIST
	 * 1) Config file
	 * 2) Error log file
	 * 3) Black/whitelist file
	 */
	path_dir_config = config_dir_get();
	path_dir_root = NULL;
	path_file_config = config_file_get();
	path_file_errlog = errlog_file_get();
	
	directory_make(path_dir_config);
	config_file_verify(path_file_config);
	readt(path_file_config, &config);
	config_list_deserialise(config, &config_list);
	
	errlog_file_verify(path_file_errlog);
	
	if (config_list) {
		path_dir_root = path_make(value_get(DP_CKEY_ROOT, config_list));
		path_dir_default = default_dir_get(path_dir_root);
		path_file_about = about_file_get(path_dir_root);
		path_file_readme = readme_file_get(path_dir_root);
		
		/*
		 * CHECKLIST
		 * 1) Readme file (in case the root directory is
		 *    being created for the first time)
		 * 2) About file
		 * 3) Local private key
		 * 4) Default (localhost) directory
		 */
		if (directory_make(path_dir_root) == 0)
			readme_file_make(path_file_readme);
		
		about_file_verify(path_file_about);
		directory_make(path_dir_default);
		
		if (path_dir_default)
			path_free(&path_dir_default);
		
		if (path_file_about)
			path_free(&path_file_about);
		
		if (path_file_readme)
			path_free(&path_file_readme);
	}
	
	if (config)
		free(config);
	
	if (path_file_config)
		path_free(&path_file_config);
	
	if (path_file_errlog)
		path_free(&path_file_errlog);
	
	return path_dir_root;
}

int directory_exists(const struct path *path)
{
	DIR *dir;
	
	if (!path)
		return -1;
	
	dir = opendir(path_str(path));
	
	if (dir) { /* Directory exists. */
		closedir(dir);
		return 1;
	} else if (errno == ENOENT) { /* Directory does not exist. */
		return 0;
	}
	
	/* opendir() failed for some other reason. */
	return -1;
}

/*
 * Returns 1 if the directory already exists,
 * otherwise it returns 0 if it got created or
 * -1 if the path is invalid.
 */
int directory_make(const struct path *path)
{
	if (!path)
		return -1;
	
	if ( directory_exists(path) != 1) {
		/*
		 * Read/write/search permissions for owner and group; read/search
		 * permissions for others.
		 */
		if (mkdir(path_str(path), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH) != 0) {
			perror ("make_directory(1), mkdir(2)");
			return -1;
		}
		
		return 0;
	}
	
	return 1;
}

/*
 * This function only works on empty directories.
 */
int directory_remove(const struct path *path)
{
	if (!path)
		return 1;
	
	return rmdir(path_str(path));
}

void empty_file_make(const struct path *path, int overwrite)
{
	char *mode;
	FILE *fptr;
	
	fptr = NULL;
	
	if (!path)
		return;
	
	if (overwrite == 1)
		mode = "w+";
	else
		mode = "a";
	
	fptr = fopen(path_str(path), mode);
	fclose(fptr);
}

struct path *errlog_file_get(void)
{
	struct path *errlog_file_path;
	
	errlog_file_path = config_dir_get();
	path_append(&errlog_file_path, DP_FILE_ERRLOG);
	
	return errlog_file_path;
}

int errlog_file_make(const struct path *path)
{
	char *errlog;
	
	if (!path)
		return -1;
	
	errlog = "# Dispatch Daemon Log\n";
	writet(path, errlog);
	
	return 0;
}

void errlog_file_verify(const struct path *path)
{
	if (file_exists(path) != 1)
		errlog_file_make(path);
}

int file_exists(const struct path *path)
{
	if (!path)
		return -1;
	
	if (access(path_str(path), F_OK) != -1)
		return 1;
	
	return 0;
}

/*
 * It is the caller's responsibility to close the
 * file.
 */
FILE *file_handle(const struct path *path)
{
	FILE *fptr;
	
	fptr = NULL;
	
	if (!path)
		return fptr;
	
	fptr = fopen(path_str(path), "r");
	
	return fptr;
}

/*
 * Reads the file in binary mode and places
 * the bytes in the data structure. It is the
 * caller's responsibility to free the returned
 * pointer.
 */
int file_get(const struct path *path, struct data64 **out)
{
	unsigned char *buffer;
	size_t byte_len;
	
	if (!path ||
	    !out)
		return 1;
	
	byte_len = readb(path, &buffer);
	*out = NULL;
	
	if (buffer) {
		if (byte_len > 0) {
			*out = (struct data64 *)malloc(sizeof(**out));
			(*out)->bytes = buffer;
			(*out)->len = byte_len;
			
			return 0;
		} else {
			free(buffer);
			buffer = NULL;
		}
	}
	
	return -1;
}

/*
 * It is the caller's responsibility to close the
 * file.
 */
FILE *file_make(const struct path *path)
{
	FILE *fptr;
	
	fptr = NULL;
	
	if (!path)
		return fptr;
	
	fptr = fopen(path_str(path), "w+");
	
	return fptr;
}

int file_remove(const struct path *path)
{
	if (!path)
		return 1;
	
	return remove(path_str(path));
}

void filelist_free(struct filelist **list)
{
	if (list) {
		while (*list) {
			struct filelist *next;
			
			next = (*list)->next;
			
			if ((*list)->path)
				path_free(&(*list)->path);
			
			free(*list);
			
			*list = next;
		}
		
		*list = NULL;
	}
}

/*
 * The returned file list entries must be freed
 * by the caller by calling filelist_free(1).
 */
void filelist_get(const struct path *path, struct filelist **out)
{
	DIR *dir;
	struct dirent *ent;
	struct filelist *list;
	
	if (!path ||
	    !out)
		return;
	
	list = NULL;
	*out = NULL;
	
	if ((dir = opendir(path_str(path))) != NULL) {
		while ((ent = readdir(dir)) != NULL) {
			/* Ignore working and upper directory files. */
			if (strcmp(ent->d_name, ".") != 0 &&
			    strcmp(ent->d_name, "..") != 0) {
				if (!list) {
					*out = (struct filelist *)malloc(sizeof(**out));
					(*out)->path = path_make(ent->d_name);
					list = *out;
				} else {
					struct filelist *entry;
					
					/* Append a new entry. */
					entry = (struct filelist *)malloc(sizeof(*entry));
					entry->path = path_make(ent->d_name);
					list->next = entry;
					list = entry;
				}
			}
		}
		
		/* 8<-- Snip off the list. -- */
		if (list)
			list->next = NULL;
		
		closedir (dir);
	} else { /* Could not open directory. */
		perror ("get_files(1), opendir(1)");
	}
}

struct path *home_dir_get(void)
{
	char *homedir;
	
	if ((homedir = getenv("HOME")) == NULL)
		homedir = getpwuid(getuid())->pw_dir;
	
	return path_make(homedir);
}

int is_directory(const struct path *path)
{
	struct stat path_stat;
	
	stat(path_str(path), &path_stat);
	
	return S_ISDIR(path_stat.st_mode);
}

int is_file(const struct path *path)
{
	struct stat path_stat;
	
	stat(path_str(path), &path_stat);
	
	return S_ISREG(path_stat.st_mode);
}

/*
 * Appends the given component to the path and
 * returns a pointer to the new component.
 */
void path_append(struct path **path, const char *component)
{
	struct path *new_comp;
	
	new_comp = NULL;
	
	if (path &&
	    component) {
		new_comp = path_make(component);
		new_comp->previous = *path;
		(*path)->next = new_comp;
		*path = new_comp;
	}
}

struct path *path_copy(const struct path *path)
{
	struct path *copy;
	struct path *iter;
	
	copy = NULL;
	iter = (struct path *)path;
	
	if (path) {
		if (!iter->next &&
		    iter->previous) {
			/* Last path component; rewind to the beginning. */
			while (iter->previous)
				iter = iter->previous;
		}
		
		while (iter) {
			if (copy) {
				struct path *component;
				
				component = path_make(iter->component);
				component->previous = copy;
				copy->next = component;
				copy = component;
			} else {
				copy = path_make(iter->component);
			}
			
			iter = iter->next;
		}
	}
	
	return copy;
}

void path_free(struct path **path)
{
	if (path) {
		if ((*path)->component) {
			free((*path)->component);
			(*path)->component = NULL;
		}
		
		/* Free subsequent path components.*/
		if ((*path)->next)
			path_free(&(*path)->next);
		
		if ((*path)->previous)
			(*path)->previous->next = NULL;
			
		free(*path);
		*path = NULL;
	}
}

struct path *path_make(const char *str)
{
	struct path *path;
	
	path = NULL;
	
	if (str) {
		path = (struct path *)malloc(sizeof(*path));
		path->component = (char *)calloc(strlen(str) + 1, sizeof(*str));
		path->next = NULL;
		path->previous = NULL;
		strcpy(path->component, str);
	}
	
	return path;
}

/*
 * Seeks to the end of a path and removes
 * the last path component. The path becomes
 * null if it contains only one component.
 */
void path_pop(struct path **path)
{
	struct path *prev;
	
	/* Seek to the end. */
	while ((*path)->next)
		*path = (*path)->next;
	
	prev = (*path)->previous;
	
	path_free(path);
	*path = prev;
}

char *property_name_get(const char *property)
{
	char *name;
	size_t len_name;
	size_t len_property;
	
	name = NULL;
	
	if (!property)
		return name;
	
	len_name = 0;
	len_property = strlen(property);
	
	for (int i = 0; i < len_property; i++) {
		if (property[i] == ' ') {
			name = (char *)calloc(len_name + 1, sizeof(*name));
			strncpy(name, property, len_name);
			
			break;
		} else if (i == len_property - 1) {
			/*
			 * Some properties take no value and as such
			 * no space separator will be encountered.
			 */
			name = (char *)calloc(len_property + 1, sizeof(*name));
			strncpy(name, property, len_property);
			
			break;
		} else {
			len_name++;
		}
	}
	
	return name;
}

char *property_val_get(const char *property)
{
	char *val;
	size_t len_property;
	
	val = NULL;
	
	if (!property)
		return val;
	
	len_property = strlen(property);
	
	for (int i = 0; i < len_property; i++) {
		if (property[i] == ' ') {
			/* No need for +1 for \0 because i is at the space separator. */
			val = (char *)calloc(len_property - i, sizeof(*val));
			/* +1 to skip the space separator. */
			strncpy(val, property + i + 1, len_property - i - 1);
			
			break;
		}
	}
	
	return val;
}

const char *value_get(const char *key, const struct token *list)
{
	struct token *iter;
	
	if (!key ||
	    !list)
		return NULL;
	
	iter = (struct token *)list;
	
	while (iter) {
		if (strcmp(iter->name, key) == 0)
			return iter->val;
		
		iter = iter->next;
	}
	
	return NULL;
}

/*
 * It is the caller's responsibility to free the
 * returned pointer.
 */
size_t readb(const struct path *path, unsigned char **out)
{
	FILE *fptr;
	
	*out = NULL;
	fptr = NULL;
	
	if (!path)
		return 0;
	
	fptr = fopen(path_str(path), "rb");
	
	if (fptr) {
		size_t bytes_read;
		size_t len;
		
		/* Obtain the file's size. */
		fseek(fptr, 0, SEEK_END);
		
		bytes_read = 0;
		len = ftell(fptr);
		*out = (unsigned char *)malloc(len * sizeof(**out));
		rewind(fptr);
		bytes_read = fread(*out, sizeof(**out), len, fptr);
		
		if (bytes_read != len) {
			/*
			 * Something went wrong; throw away the memory and set
			 * the buffer to NULL.
			 */
			perror("file_readb(2)");
			free(*out);
			*out = NULL;
		}
		
		fclose(fptr);
		
		return bytes_read;
	}
	
	return -1;
}

struct path *readme_file_get(struct path *root_dir_path)
{
	struct path *readme_file_path;
	
	readme_file_path = path_copy(root_dir_path);
	path_append(&readme_file_path, DP_FILE_README);
	
	return readme_file_path;
}

int readme_file_make(const struct path *path)
{
	char *readme;
	
	if (!path)
		return -1;
	
	readme = "INSTRUCTIONS\n";
	writet(path, readme);
	
	return 0;
}

/*
 * It is the caller's responsibility to free the
 * returned pointer.
 */
size_t readt(const struct path *path, char **out)
{
	FILE *fptr;
	
	*out = NULL;
	
	if (!path)
		return 0;
	
	fptr = fopen(path_str(path), "r");
	
	if (fptr) {
		size_t text_read;
		size_t len;
		
		fseek(fptr, 0, SEEK_END);
		
		len = ftell(fptr);
		*out = (char *)calloc((len + 1), sizeof(char));
		rewind(fptr);
		text_read = fread(*out, sizeof(**out), len, fptr);
		
		if (text_read != len) {
			/*
			 * Something went wrong; throw away the memory and set
			 * the buffer to NULL.
			 */
			perror("file_readb(2)");
			free(*out);
			*out = NULL;
		}
		
		fclose(fptr);
		
		return text_read;
	}
	
	return -1;
}

size_t writeb(const struct path *path, const unsigned char *buffer, const size_t size)
{
	FILE *fptr;
	size_t bytes_written;
	
	bytes_written = 0;
	fptr = NULL;
	
	if (path &&
	    buffer) {
		fptr = fopen(path_str(path), "wb+");
		
		if (fptr) {
			bytes_written = fwrite(buffer, 1, size, fptr);
			fclose(fptr);
		}
	}
	
	return bytes_written;
}

size_t writet(const struct path *path, const char *buffer)
{
	FILE *fptr;
	size_t text_written;
	
	text_written = 0;
	fptr = NULL;
	
	if (path &&
	    buffer) {
		fptr = fopen(path_str(path), "w+");
		
		if (fptr) {
			text_written = fwrite(buffer, 1, strlen(buffer), fptr);
			fclose(fptr);
		}
	}
	
	return text_written;
}
