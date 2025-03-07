//
//  protocol.h
//  server
//
//  Created by Ali Mahouk on 12/10/17.
//  Copyright © 2017 Ali Mahouk. All rights reserved.
//

#ifndef PROTOCOL_H
#define PROTOCOL_H


#include "crypto.h"
#include <stdint.h>
#include <stdlib.h>
#include "types.h"
#include "util.h"


#define DP_PROTO_HOST_MAGIC_NUM_LEN  	9

/*************
 * CONSTANTS *
 *************/
static const char *DP_PROTO_SERV_HEAD 					= "!DP";
static const char *DP_PROTO_SERV_DELIM 					= "\r\n";
static const char *DP_PROTO_SERV_ARG_FILE 				= "f"; 	/* Specifies a file path */
static const char *DP_PROTO_SERV_ARG_RECIP 				= "r"; 	/* Specifies a recipient */
static const uint32_t DP_PROTO_SERV_VER 				= 1;
static const char DP_PROTO_HOST_MAGIC_NUM[DP_PROTO_HOST_MAGIC_NUM_LEN] 	= { 0x89, 0x50, 0x44, 0x48, 0x5a, 0x0d, 0x0a, 0x1a, 0x0a };
static const uint32_t DP_PROTO_HOST_VER 				= 1;
static const int DP_PROTO_SERV_ARGMAX_NAME 				= 4; 	/* The maximum length of an argument name. */
static const int DP_PROTO_SERV_ARGMAX_VAL 				= 256;	/* The maximum length of an argument value. */
static const int DP_PROTO_SERV_MAXREAD 					= 8192; /* 8 KB */
static const uint16_t DP_PROTO_HOST_MSG_UNDEF 				= 0;
static const uint16_t DP_PROTO_HOST_MSG_PARCEL 				= 1;
static const uint16_t DP_PROTO_HOST_HEAD_LEN 				= DP_PROTO_HOST_MAGIC_NUM_LEN + 	/* Magic number */
										sizeof(DP_PROTO_HOST_VER) + 	/* Protocol version (4 bytes) */
										SHA256_DIGEST_LENGTH + 		/* Checksum (32 bytes) */
										sizeof(uint64_t) + 		/* Timestamp (8 bytes) */
										sizeof(uint16_t) + 		/* Type (2 bytes) */
										UUID_LEN +			/* UUID (16 bytes) */
										sizeof(uint64_t); 		/* Parcel size (8 bytes) */


/**************
 * STRUCTURES *
 **************/
/*
 * STRUCTURE PACKING
 * The struct members are ordered in such a way
 * that slop is minimized. This is crucial as it
 * helps reduce the memory footprint when scaling
 * up.
 */

/*
 * A representation of user@server.
 */
struct dp_addr {
	struct dp_node *host;
	struct dp_node *user;
};

/*
 * Represents any entity that can communicate with
 * the rest of the network, i.e. servers and users.
 */
struct dp_node {
	char *identifier;
	void *pkey; /* !TODO! Change to EVP_PKEY once crypto is in place! */
};

struct dp_parcel_head {
	unsigned char checksum[SHA256_DIGEST_LENGTH * sizeof(unsigned char)];	/* Checksum includes the header along with the parcel */
	uuid_t uuid;								/* Message's unique identifier */
	time_t timestamp;							/* Message timestamp */
	uint16_t type;
};

struct dp_parcel {
	char *raw_filename;
	char *sender_name;
	char *service;  		/* The service as denoted by the file extension */
	struct data64 *payload;  	/* File bytes */
	struct dp_addr *recipient_addr;  /* user@host, user, or @host */
	struct dp_addr *sender_addr;
	struct dp_parcel_head head;
};

struct dp_reqstatus {
	char *name;
	uint16_t code;
};


/**********
 * ERRORS *
 **********/
/* External Errors */
static const struct dp_reqstatus DP_REQOK 		= { .name = "OK", .code = 200 };
static const struct dp_reqstatus DP_REQERR_BADREQ 	= { .name = "Bad Request", .code = 400 };

/* Internal Program Errors */
static const struct dp_reqstatus DP_REQERR_INT_BADARG = { .name = "Bad request passed to function", .code = 600 };


/*************
 * FUNCTIONS *
 *************/
int arg_name_get(const char *, char **);
int arg_val_get(const char *, char **);
struct dp_reqstatus client_request_parse(struct token *);
int client_request_tokenise(const char *, uint16_t, struct token **);
void *directory_tree_scan(void *);
int host_get(const char *, char **);
void parcel_free(struct dp_parcel **);
struct dp_parcel *parcel_make(void);
void parcel_parse(const struct data16 *, const struct data64 *);
uint64_t parcel_size_get(const struct data16 *);
void request_free(struct token **);
int service_get(const char *, char **);
int user_get(const char *, char **);
int valid_check(const char *);


#endif /* PROTOCOL_H */
