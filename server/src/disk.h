//
//  disk.h
//  server
//
//  Created by Ali Mahouk on 12/10/17.
//  Copyright © 2017 Ali Mahouk. All rights reserved.
//

#ifndef DISK_H
#define DISK_H


#include <stdio.h>
#include "types.h"


/*************
 * CONSTANTS *
 *************/
static const char *DP_CKEY_ROOT 	= "DOCROOT";
static const char  DP_CONF_COMMENT 	= '#';
static const char *DP_CONF_HEADER 	= "!DP_CONFIG";
static const char *DP_DIR_CONF 		= ".dispatch";
static const char *DP_DIR_DEFAULT 	= "localhost";	/* Default domain that maps to the local machine */
static const char *DP_DIR_DOCROOT 	= "Dispatch";	/* The top-level Dispatch directory */
static const int   DP_DIR_SCAN_INT	= 3 * 1000;	/* Directory tree scanning interval (in milliseconds). */
static const char *DP_FILE_ABOUT	= "About.txt";	/* Contains 1 line which will be used as the local machine's display name */
static const char *DP_FILE_ADDRRULES 	= "dp.rules";	/* Black/whitelisted addresses */
static const char *DP_FILE_AUTOFORWARD	= "dp.forward";	/* Auto-forwards new parcels to addresses in this file; can be made more specific by being placed in deeper directories. */
static const char *DP_FILE_AUTORESPONSE = "auto";	/* This file (with any extension) can be sent as an automatic response to new parcels */
static const char *DP_FILE_CONF 	= "dp.conf";	/* Daemon config file */
static const char *DP_FILE_ERRLOG 	= "dp.log";	/* Error log */
static const char *DP_FILE_INDEX 	= ".index";	/* Index of files in the directory */
static const char *DP_FILE_LIST 	= "dp.list";	/* Dispatch address list */
static const char *DP_FILE_LOG 		= ".log";	/* Event log */
static const char *DP_FILE_PRIVKEY 	= "id.pem";	/* Local machine's private key */
static const char *DP_FILE_PUBKEY 	= ".pubkey";	/* A public key */
static const char *DP_FILE_README 	= "Instructions.txt";


/*************
 * FUNCTIONS *
 *************/
struct path *directories_bootstrap(void);
int directory_exists(const struct path *);
int directory_make(const struct path *);
int directory_remove(const struct path *);
void empty_file_make(const struct path *, int);
int file_exists(const struct path *);
int file_get(const struct path *, struct data64 **);
FILE *file_handle(const struct path *);
FILE *file_make(const struct path *);
int file_remove(const struct path *);
void filelist_free(struct filelist **);
void filelist_get(const struct path *, struct filelist **);
struct path *home_dir_get(void);
int is_directory(const struct path *);
int is_file(const struct path *);
void path_append(struct path **, const char *);
struct path *path_copy(const struct path *);
void path_free(struct path **);
struct path *path_make(const char *);
void path_pop(struct path **);
size_t readb(const struct path *, unsigned char **);
size_t readt(const struct path *, char **);
size_t writeb(const struct path *, const unsigned char *, const size_t);
size_t writet(const struct path *, const char *);


#endif /* DISK_H */
