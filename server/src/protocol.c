//
//  protocol.c
//  server
//
//  Created by Ali Mahouk on 12/13/17.
//  Copyright © 2017 Ali Mahouk. All rights reserved.
//

#include "protocol.h"

#include "disk.h"
#include "net.h"
#include <stdio.h>
#include <string.h>


/**********************
 * Private Prototypes
 **********************/
int delimiter_check(const char *, size_t);
void directory_process(const struct filelist *, int);
void directory_scan(struct path *, int);
int header_deserialise(const struct data16 *, struct dp_parcel_head *);
int header_serialise(const struct dp_parcel_head, uint64_t, struct data16 **);
int parcel_deserialise(const struct data64 *, struct dp_parcel *);
void parcel_filename_set(struct dp_parcel *, const char *);
void parcel_recipient_addr_set(struct dp_parcel *, const char *);
int parcel_serialise(const struct dp_parcel *, struct data64 **);
/**********************/


/*
 * It is the caller's responsibility to free the
 * returned pointer.
 */
int arg_name_get(const char *token, char **arg_name)
{
	char buffer[DP_PROTO_SERV_ARGMAX_NAME + 1] = { 0 };
	size_t len_arg;
	size_t len_token;
	
	if (!token ||
	    !arg_name)
		return 1;
	
	*arg_name = NULL;
	len_arg = 0;
	len_token = strlen(token);
	
	if (len_token == 0)
		return -1;
	
	if (token[0] != '-')
		return -1;
	
	for (int i = 0; i < DP_PROTO_SERV_ARGMAX_NAME + 1 && i < len_token; i++) {
		char c;
		
		c = token[i];
		
		/*
		 * A space marks the end of an argument name, hence the
		 * +1 in the loop condition (DP_PROTO_CLARGMAX_NAME
		 * excludes the space character).
		 */
		if (c == ' ') {
			*arg_name = (char *)calloc(len_arg + 1, sizeof(**arg_name));
			strcpy(*arg_name, buffer);
			
			break;
		} else if (c != '-') {
			/* Ignore dash prefix. */
			buffer[len_arg] = c;
			len_arg++;
		}
	}
	
	return 0;
}

/*
 * It is the caller's responsibility to free the
 * returned pointer.
 */
int arg_val_get(const char *token, char **arg_val)
{
	size_t len_arg;
	size_t len_token;
	int val_start;
	
	if (!token ||
	    !arg_val)
		return 1;
	
	*arg_val = NULL;
	len_arg = 0;
	val_start = -1;
	len_token = strlen(token);
	
	if (len_token == 0)
		return -1;
	
	/*
	 * First find where the argument name ends.
	 * A space marks the end of an argument name, hence the
	 * +1 in the loop condition (DP_PROTO_CLARGMAX_NAME
	 * excludes the space character).
	 */
	for (int i = 0; i < DP_PROTO_SERV_ARGMAX_NAME + 1 && i < len_token; i++) {
		if (token[i] == ' ') {
			val_start = i;
			break;
		}
	}
	
	if (val_start > 0) {
		/* Extract the value. */
		*arg_val = (char *)calloc(len_arg + 1, sizeof(**arg_val));
		strncpy(*arg_val, token + val_start + 1, len_token - val_start - 1);
	}
	
	return 0;
}

/*
 * Note: this function will free the passed request token list.
 */
struct dp_reqstatus client_request_parse(struct token *request)
{
	struct data16 *head_data;
	struct data64 *parcel_data;
	struct dp_parcel *parcel;
	struct token *iter_req;
	
	if (!request)
		return DP_REQERR_INT_BADARG;
	
	iter_req = request;
	parcel = parcel_make();
	
	/* Loop over all tokens. */
	while (iter_req) {
		if (iter_req->name) {
			if (strcmp(iter_req->name, DP_PROTO_SERV_ARG_FILE) == 0)
				parcel_filename_set(parcel, iter_req->val);
			else if (strcmp(iter_req->name, DP_PROTO_SERV_ARG_RECIP) == 0)
				parcel_recipient_addr_set(parcel, iter_req->val);
		}
		
		iter_req = iter_req->next;
	}
	
	file_get(path_make(parcel->raw_filename), &(parcel->payload));
	service_get(parcel->raw_filename, &(parcel->service));
	parcel->head.type = DP_PROTO_HOST_MSG_PARCEL;
	parcel->sender_addr->host->identifier = "bar.com";
	parcel->sender_addr->user->identifier = "foo";
	
	printf("RAW FILENAME: %s\n", parcel->raw_filename);
	printf("FILE IS %lu byte(s)\n", parcel->payload->len);
	printf("SERVICE: %s\n", parcel->service);
	printf("TO: %s AT %s\n", parcel->recipient_addr->user->identifier, parcel->recipient_addr->host->identifier);
	
	parcel_serialise(parcel, &parcel_data);
	header_serialise(parcel->head, parcel_data->len, &head_data);
	
	data64_send(parcel->recipient_addr->host->identifier, head_data, parcel_data);
	parcel_free(&parcel);
	
	return DP_REQOK;
}

/*
 * It is the caller's responsibility to free the
 * returned pointer.
 */
int client_request_tokenise(const char *reqstr, uint16_t len, struct token **request)
{
	struct token *last_line;
	size_t delim_len;
	int eol; /* "End of line" */
	int token_len;
	
	delim_len = strlen(DP_PROTO_SERV_DELIM);
	
	if (!reqstr ||
	    delim_len == 0)
		return 1;
	
	/*
	 * The below are checks for invalid request lengths.
	 * A request can never be shorter than at least one
	 * delimeter.
	 */
	if (len < delim_len ||
	    len > DP_PROTO_SERV_MAXREAD)
		return -1;
	
	eol = 0;
	last_line = NULL;
	*request = NULL;
	token_len = 0;
	
	for (int i = 0; i < len; i++) {
		int eor;
		char c;
		
		c = reqstr[i];
		eor = 0;
		
		/*
		 * Delimeter checking should not assume a
		 * specific length. We do know, however,
		 * that it is always at least a single
		 * character in length.
		 */
		if (c == DP_PROTO_SERV_DELIM[0]) {
			eol = delimiter_check(reqstr, i);
			
			/*
			 * Now check if this is a double delimeter,
			 * which would mark the end of a request.
			 */
			if (eol == 1 &&
			    delimiter_check(reqstr, i + delim_len) == 1)
				eor = 1;
		}
		
		if (eol == 1 &&
		    token_len > 0) {
			/* End of line reached; create a token. */
			char *arg_name;
			char *token;
			
			arg_name = NULL;
			token = (char *)calloc(token_len + 1, sizeof(*token));
			strncpy(token, reqstr + i - token_len, token_len);
			
			if (arg_name_get(token, &arg_name) == 0) {
				char *arg_val;
				
				arg_val = NULL;
				arg_val_get(token, &arg_val);
				
				if (!last_line) {
					*request = (struct token *)malloc(sizeof(**request));
					(*request)->name = arg_name;
					(*request)->val = arg_val;
					last_line = *request;
				} else {
					struct token *new_line;
					
					/* Append a new request line. */
					new_line = (struct token *)malloc(sizeof(*new_line));
					new_line->name = arg_name;
					new_line->val = arg_val;
					last_line->next = new_line;
					last_line = new_line;
				}
			}
			
			/* Reset EOL and clean iteration memory. */
			free(token);
			eol = 0;
			token = NULL;
			token_len = 0;
			
			/* Check if this is the end of the request. */
			if (eor == 1) {
				/* 8<-- Snip off the list. -- */
				last_line->next = NULL;
				return 0;
			}
			
			/* Advance the loop to skip over the delimeter. */
			i += delim_len - 1;
		} else {
			token_len++;
		}
	}
	
	return 1;
}

int delimiter_check(const char *str, size_t index)
{
	size_t len;
	size_t delim_len;
	int delimited;
	
	if (!str)
		return 0;
	
	delim_len = strlen(DP_PROTO_SERV_DELIM);
	delimited = 0;
	len = strlen(str);
	
	if (len < delim_len)
		return 0;
	
	if (str[index] == DP_PROTO_SERV_DELIM[0]) {
		/* Check if this is a delimeter. */
		for (int j = 1; j < delim_len && index + j < len; j++) {
			if (str[index + j] != DP_PROTO_SERV_DELIM[j]) {
				delimited = -1;
				break;
			} else {
				delimited = 1;
			}
		}
		
		/*
		 * The below condition is only true
		 * if the delimeter happens to be
		 * a single character in length.
		 */
		if (delimited == 0)
			delimited = 1;
	}
	
	return delimited;
}

void directory_process(const struct filelist *files, int depth)
{
	/* Skip the root directory. */
	if (depth == 0)
		return;
	
	if (depth == 1) {
		/* Sender's domain. */
		
	} else if (depth == 2) {
		/* Sender's username. */
		
	} else if (depth == 3) {
		/* Recipient's domain. */
		
	} else if (depth == 4) {
		/* Recipient's username. */
		
	} else {
		/* Nested directories for organisation purposes. */
		
	}
}

void directory_scan(struct path *path, int depth)
{
	struct filelist *files;
	struct filelist *iter;
	
	printf("Scanning %s…\n", path->component);
	filelist_get(path, &files);
	
	/*
	 * Analysis of directory at current depth, i.e.
	 * what files ought to be present and creating
	 * them if they're absent, checking the index,
	 * etc.
	 */
	directory_process(files, depth);
	
	iter = files;
	
	while (iter) {
		/* The directory function requires an absolute path. */
		path_append(&path, iter->path->component);
		
		if (is_directory(path) == 1)
			directory_scan(path, depth + 1);
		else
			
			path_pop(&path);
		iter = iter->next;
	}
	
	filelist_free(&files);
}

void *directory_tree_scan(void *root)
{
	struct path *path_dir_root;
	
	if (root) {
		path_dir_root = (struct path *)root;
		directory_scan(path_dir_root, 0);
	}
	
	return 0;
}

int header_deserialise(const struct data16 *head_data, struct dp_parcel_head *out)
{
	int pos;
	int status;
	
	pos = DP_PROTO_HOST_MAGIC_NUM_LEN + sizeof(DP_PROTO_SERV_VER);
	status = 0;
	
	/*
	 * STRUCTURE
	 * 1) Magic number
	 * 2) Protocol version (4 bytes)
	 * 3) Checksum (32 bytes)
	 * 4) Timestamp (8 bytes)
	 * 5) Type (2 bytes)
	 * 6) UUID (16 bytes)
	 * 7) Parcel size (8 bytes)
	 */
	
	/* 3) Checksum (32 bytes) */
	memcpy(out->checksum, &head_data->bytes[pos], SHA256_DIGEST_LENGTH * sizeof(unsigned char));
	pos += SHA256_DIGEST_LENGTH * sizeof(unsigned char);
	
	/* 4) Timestamp (8 bytes) */
	out->timestamp = head_data->bytes[pos + 7] |
		( (uint64_t)head_data->bytes[pos + 6] << 8 ) |
		( (uint64_t)head_data->bytes[pos + 5] << 16 ) |
		( (uint64_t)head_data->bytes[pos + 4] << 24 ) |
		( (uint64_t)head_data->bytes[pos + 3] << 32 ) |
		( (uint64_t)head_data->bytes[pos + 2] << 40 ) |
		( (uint64_t)head_data->bytes[pos + 1] << 48 ) |
		( (uint64_t)head_data->bytes[pos] << 56 );
	pos += sizeof(uint64_t);
	
	/* 5) Type (2 bytes) */
	out->type = head_data->bytes[pos + 1] |
	( (uint16_t)head_data->bytes[pos] << 8 );
	pos += sizeof(uint16_t);
	
	/* 6) UUID (16 bytes) */
	memcpy(out->uuid, &head_data->bytes[pos], UUID_LEN * sizeof(unsigned char));
	//pos += UUID_LEN * sizeof(unsigned char); /* Ucomment to continue parsing. */
	
	return status;
}

int header_serialise(const struct dp_parcel_head head, uint64_t parcel_size, struct data16 **out)
{
	int pos;
	int status;
	
	status = 0;
	
	/*
	 * STRUCTURE
	 * 1) Magic number
	 * 2) Protocol version (4 bytes)
	 * 3) Checksum (32 bytes)
	 * 4) Timestamp (8 bytes)
	 * 5) Type (2 bytes)
	 * 6) UUID (16 bytes)
	 * 7) Parcel size (8 bytes)
	 */
	*out = (struct data16 *)malloc(sizeof(**out));
	(*out)->len = DP_PROTO_HOST_HEAD_LEN;
	(*out)->bytes = (unsigned char *)calloc((*out)->len, sizeof(unsigned char));
	
	/* 1) Magic number */
	memcpy((*out)->bytes, DP_PROTO_HOST_MAGIC_NUM, DP_PROTO_HOST_MAGIC_NUM_LEN * sizeof(unsigned char));
	pos = DP_PROTO_HOST_MAGIC_NUM_LEN * sizeof(unsigned char);
	
	/* 2) Protocol version (4 bytes) */
	(*out)->bytes[pos]   = (DP_PROTO_HOST_VER >> 24) & 0xff;
	(*out)->bytes[++pos] = (DP_PROTO_HOST_VER >> 16) & 0xff;
	(*out)->bytes[++pos] = (DP_PROTO_HOST_VER >> 8) & 0xff;
	(*out)->bytes[++pos] = DP_PROTO_HOST_VER & 0xff;
	
	/* 3) Checksum (32 bytes) */
	memcpy(&(*out)->bytes[++pos], head.checksum, SHA256_DIGEST_LENGTH * sizeof(unsigned char));
	pos += SHA256_DIGEST_LENGTH * sizeof(unsigned char);
	
	/* 4) Timestamp (8 bytes) */
	(*out)->bytes[pos] = (head.timestamp >> 56) & 0xff;
	(*out)->bytes[++pos] = (head.timestamp >> 48) & 0xff;
	(*out)->bytes[++pos] = (head.timestamp >> 40) & 0xff;
	(*out)->bytes[++pos] = (head.timestamp >> 32) & 0xff;
	(*out)->bytes[++pos] = (head.timestamp >> 24) & 0xff;
	(*out)->bytes[++pos] = (head.timestamp >> 16) & 0xff;
	(*out)->bytes[++pos] = (head.timestamp >> 8) & 0xff;
	(*out)->bytes[++pos] = head.timestamp & 0xff;
	
	/* 5) Type (2 bytes) */
	(*out)->bytes[++pos] = (head.type >> 8) & 0xff;
	(*out)->bytes[++pos] = head.type & 0xff;
	
	/* 6) UUID (16 bytes) */
	memcpy(&(*out)->bytes[++pos], head.uuid, UUID_LEN * sizeof(unsigned char));
	pos += UUID_LEN * sizeof(unsigned char);
	
	/* 7) Parcel size (8 bytes) */
	(*out)->bytes[pos] = (parcel_size >> 56) & 0xff;
	(*out)->bytes[++pos] = (parcel_size >> 48) & 0xff;
	(*out)->bytes[++pos] = (parcel_size >> 40) & 0xff;
	(*out)->bytes[++pos] = (parcel_size >> 32) & 0xff;
	(*out)->bytes[++pos] = (parcel_size >> 24) & 0xff;
	(*out)->bytes[++pos] = (parcel_size >> 16) & 0xff;
	(*out)->bytes[++pos] = (parcel_size >> 8) & 0xff;
	(*out)->bytes[++pos] = parcel_size & 0xff;
	
	return status;
}

/*
 * In the case of local domain addresses, this function
 * might return a null host.
 */
int host_get(const char *addr_str, char **host)
{
	size_t addr_len;
	size_t i_host;
	int has_host;
	
	if (!addr_str ||
	    !host)
		return 1;
	
	addr_len = strlen(addr_str);
	has_host = 0;
	i_host = 0;
	
	/*
	 * Whatever comes after the first @ symbol
	 * is the host.
	 */
	for (int i = 0; i < addr_len; i++) {
		if (addr_str[i] == '@') {
			has_host = 1;
			i_host = i;
			break;
		}
	}
	
	if (has_host == 0) {
		/* No host specified; the whole address is the user. */
		*host = NULL;
	} else {
		*host = (char *)calloc(addr_len - i_host, sizeof(**host));
		strncpy(*host, addr_str + i_host + 1, addr_len - i_host - 1);
	}
	
	return 0;
}

int parcel_deserialise(const struct data64 *parcel_data, struct dp_parcel *out)
{
	uint32_t size_raw_filename;
	uint32_t size_recipient_host;
	uint32_t size_recipient_user;
	uint32_t size_sender_host;
	uint32_t size_sender_user;
	int pos;
	int status;
	
	pos = 0;
	status = 0;
	
	/*
	 * STRUCTURE
	 * 1) Raw filename size (4 bytes)
	 * 2) Raw filename
	 * 3) Recipient host size (4 bytes)
	 * 4) Recipient host
	 * 5) Recipient user size (4 bytes)
	 * 6) Recipient user
	 * 7) Sender host size (4 bytes)
	 * 8) Sender host
	 * 9) Sender user size (4 bytes)
	 * 10) Sender user
	 * 11) Payload size (8 bytes)
	 * 12) Payload
	 *
	 * NOTE: null terminators are not copied to save space.
	 * They should be accounted for upon deserialisation.
	 */
	
	/* 1) Raw filename size (4 bytes) */
	size_raw_filename = parcel_data->bytes[pos + 3] |
		( (uint32_t)parcel_data->bytes[pos + 2] << 8 ) |
		( (uint32_t)parcel_data->bytes[pos + 1] << 16 ) |
		( (uint32_t)parcel_data->bytes[pos] << 24 );
	pos += sizeof(uint32_t);
	
	/* 2) Raw filename */
	out->raw_filename = (char *)calloc(size_raw_filename + 1, sizeof(char));
	memcpy(out->raw_filename, &parcel_data->bytes[pos], size_raw_filename * sizeof(char));
	pos += size_raw_filename * sizeof(char);
	
	/* 3) Recipient host size (4 bytes) */
	size_recipient_host = parcel_data->bytes[pos + 3] |
		( (uint32_t)parcel_data->bytes[pos + 2] << 8 ) |
		( (uint32_t)parcel_data->bytes[pos + 1] << 16 ) |
		( (uint32_t)parcel_data->bytes[pos] << 24 );
	pos += sizeof(uint32_t);
	
	/* 4) Recipient host */
	out->recipient_addr->host->identifier = (char *)calloc(size_recipient_host + 1, sizeof(char));
	memcpy(out->recipient_addr->host->identifier, &parcel_data->bytes[pos], size_recipient_host * sizeof(char));
	pos += size_recipient_host * sizeof(char);
	
	/* 5) Recipient user size (4 bytes) */
	size_recipient_user = parcel_data->bytes[pos + 3] |
		( (uint32_t)parcel_data->bytes[pos + 2] << 8 ) |
		( (uint32_t)parcel_data->bytes[pos + 1] << 16 ) |
		( (uint32_t)parcel_data->bytes[pos] << 24 );
	pos += sizeof(uint32_t);
	
	/* 6) Recipient user */
	out->recipient_addr->user->identifier = (char *)calloc(size_recipient_user + 1, sizeof(char));
	memcpy(out->recipient_addr->user->identifier, &parcel_data->bytes[pos], size_recipient_user * sizeof(char));
	pos += size_recipient_user * sizeof(char);
	
	/* 7) Sender host size (4 bytes) */
	size_sender_host = parcel_data->bytes[pos + 3] |
		( (uint32_t)parcel_data->bytes[pos + 2] << 8 ) |
		( (uint32_t)parcel_data->bytes[pos + 1] << 16 ) |
		( (uint32_t)parcel_data->bytes[pos] << 24 );
	pos += sizeof(uint32_t);
	
	/* 8) Sender host */
	out->sender_addr->host->identifier = (char *)calloc(size_sender_host + 1, sizeof(char));
	memcpy(out->sender_addr->host->identifier, &parcel_data->bytes[pos], size_sender_host * sizeof(char));
	pos += size_sender_host * sizeof(char);
	
	/* 9) Sender user size (4 bytes) */
	size_sender_user = parcel_data->bytes[pos + 3] |
		( (uint32_t)parcel_data->bytes[pos + 2] << 8 ) |
		( (uint32_t)parcel_data->bytes[pos + 1] << 16 ) |
		( (uint32_t)parcel_data->bytes[pos] << 24 );
	pos += sizeof(uint32_t);
	
	/* 10) Sender user */
	out->sender_addr->user->identifier = (char *)calloc(size_sender_user + 1, sizeof(char));
	memcpy(out->sender_addr->user->identifier, &parcel_data->bytes[pos], size_sender_user * sizeof(char));
	pos += size_sender_user * sizeof(char);
	
	out->payload = (struct data64 *)malloc(sizeof(struct data64));
	
	/* 11) Payload size (8 bytes) */
	out->payload->len = parcel_data->bytes[pos + 7] |
		( (uint64_t)parcel_data->bytes[pos + 6] << 8 ) |
		( (uint64_t)parcel_data->bytes[pos + 5] << 16 ) |
		( (uint64_t)parcel_data->bytes[pos + 4] << 24 ) |
		( (uint64_t)parcel_data->bytes[pos + 3] << 32 ) |
		( (uint64_t)parcel_data->bytes[pos + 2] << 40 ) |
		( (uint64_t)parcel_data->bytes[pos + 1] << 48 ) |
		( (uint64_t)parcel_data->bytes[pos] << 56 );
	pos += sizeof(uint64_t);
	
	/* 12) Payload */
	out->payload->bytes = (unsigned char *)calloc(out->payload->len, sizeof(unsigned char));
	memcpy(out->payload->bytes, &parcel_data->bytes[pos], out->payload->len * sizeof(unsigned char));
	//pos += size_sender_user * sizeof(unsigned char); /* Ucomment to continue parsing. */
	
	return status;
}

void parcel_filename_set(struct dp_parcel *parcel, const char *name)
{
	if (parcel) {
		if (parcel->raw_filename)
			free(parcel->raw_filename);
		
		if (name) {
			parcel->raw_filename = (char *)calloc(strlen(name) + 1, sizeof(*name));
			strcpy(parcel->raw_filename, name);
		} else {
			parcel->raw_filename = NULL;
		}
	}
	
}

/*
 * !STUB!
 */
void parcel_free(struct dp_parcel **parcel)
{
	
}

/*
 * Initialises an empty parcel struct.
 * It is the caller's responsibility to free the
 * returned pointer.
 */
struct dp_parcel *parcel_make(void)
{
	struct dp_parcel *parcel;
	
	parcel = (struct dp_parcel *)malloc(sizeof(*parcel));
	parcel->head.timestamp = timestamp();
	parcel->head.type = DP_PROTO_HOST_MSG_UNDEF;
	parcel->raw_filename = NULL;
	parcel->recipient_addr = (struct dp_addr *)malloc(sizeof(*(parcel->recipient_addr)));
	parcel->recipient_addr->host = (struct dp_node *)malloc(sizeof(*(parcel->recipient_addr->host)));
	parcel->recipient_addr->host->identifier = NULL;
	parcel->recipient_addr->user = (struct dp_node *)malloc(sizeof(*(parcel->recipient_addr->user)));
	parcel->recipient_addr->user->identifier = NULL;
	parcel->sender_addr = (struct dp_addr *)malloc(sizeof(*(parcel->sender_addr)));
	parcel->sender_addr->host = (struct dp_node *)malloc(sizeof(*(parcel->sender_addr->host)));
	parcel->sender_addr->host->identifier = NULL;
	parcel->sender_addr->user = (struct dp_node *)malloc(sizeof(*(parcel->sender_addr->user)));
	parcel->sender_addr->user->identifier = NULL;
	parcel->service = NULL;
	
	uuid_generate(parcel->head.uuid);
	
	return parcel;
}

void parcel_parse(const struct data16 *head_data, const struct data64 *parcel_data)
{
	struct dp_parcel *parcel;
	
	parcel = parcel_make();
	header_deserialise(head_data, &(parcel->head));
	parcel_deserialise(parcel_data, parcel);
	service_get(parcel->raw_filename, &(parcel->service));
	
	printf("RAW FILENAME: %s\n", parcel->raw_filename);
	printf("FILE IS %lu byte(s)\n", parcel->payload->len);
	printf("SERVICE: %s\n", parcel->service);
	printf("TO: %s AT %s\n", parcel->recipient_addr->user->identifier, parcel->recipient_addr->host->identifier);
	printf("PAYLOAD: %s\n", parcel->payload->bytes);
}

void parcel_recipient_addr_set(struct dp_parcel *parcel, const char *addr_str)
{
	if (parcel) {
		host_get(addr_str, &(parcel->recipient_addr->host->identifier));
		user_get(addr_str, &(parcel->recipient_addr->user->identifier));
	}
}

int parcel_serialise(const struct dp_parcel *parcel, struct data64 **out)
{
	uint32_t size_raw_filename;
	uint32_t size_recipient_host;
	uint32_t size_recipient_user;
	uint32_t size_sender_host;
	uint32_t size_sender_user;
	int pos;
	int status;
	
	pos = 0;
	size_raw_filename = (uint32_t)strlen(parcel->raw_filename);
	size_recipient_host = (uint32_t)strlen(parcel->recipient_addr->host->identifier);
	size_recipient_user = (uint32_t)strlen(parcel->recipient_addr->user->identifier);
	size_sender_host = (uint32_t)strlen(parcel->sender_addr->host->identifier);
	size_sender_user = (uint32_t)strlen(parcel->sender_addr->user->identifier);
	status = 0;
	
	/*
	 * STRUCTURE
	 * 1) Raw filename size (4 bytes)
	 * 2) Raw filename
	 * 3) Recipient host size (4 bytes)
	 * 4) Recipient host
	 * 5) Recipient user size (4 bytes)
	 * 6) Recipient user
	 * 7) Sender host size (4 bytes)
	 * 8) Sender host
	 * 9) Sender user size (4 bytes)
	 * 10) Sender user
	 * 11) Payload size (8 bytes)
	 * 12) Payload
	 *
	 * NOTE: null terminators are not copied to save space.
	 * They should be accounted for upon deserialisation.
	 */
	*out = (struct data64 *)malloc(sizeof(**out));
	(*out)->len = sizeof(uint32_t) + size_raw_filename +
	sizeof(uint32_t) + size_recipient_host +
	sizeof(uint32_t) + size_recipient_user +
	sizeof(uint32_t) + size_sender_host +
	sizeof(uint32_t) + size_sender_user +
	sizeof(uint64_t) + parcel->payload->len;
	(*out)->bytes = (unsigned char *)calloc((*out)->len, sizeof(unsigned char));
	
	/* 1) Raw filename size (4 bytes) */
	(*out)->bytes[pos]   = (size_raw_filename >> 24) & 0xff;
	(*out)->bytes[++pos] = (size_raw_filename >> 16) & 0xff;
	(*out)->bytes[++pos] = (size_raw_filename >> 8) & 0xff;
	(*out)->bytes[++pos] = size_raw_filename & 0xff;
	
	/* 2) Raw filename */
	memcpy(&(*out)->bytes[++pos], parcel->raw_filename, size_raw_filename * sizeof(char));
	pos += size_raw_filename * sizeof(char);
	
	/* 3) Recipient host size (4 bytes) */
	(*out)->bytes[pos]   = (size_recipient_host >> 24) & 0xff;
	(*out)->bytes[++pos] = (size_recipient_host >> 16) & 0xff;
	(*out)->bytes[++pos] = (size_recipient_host >> 8) & 0xff;
	(*out)->bytes[++pos] = size_recipient_host & 0xff;
	
	/* 4) Recipient host */
	memcpy(&(*out)->bytes[++pos], parcel->recipient_addr->host->identifier, size_recipient_host * sizeof(char));
	pos += size_recipient_host * sizeof(char);
	
	/* 5) Recipient user size (4 bytes) */
	(*out)->bytes[pos]   = (size_recipient_user >> 24) & 0xff;
	(*out)->bytes[++pos] = (size_recipient_user >> 16) & 0xff;
	(*out)->bytes[++pos] = (size_recipient_user >> 8) & 0xff;
	(*out)->bytes[++pos] = size_recipient_user & 0xff;
	
	/* 6) Recipient user */
	memcpy(&(*out)->bytes[++pos], parcel->recipient_addr->user->identifier, size_recipient_user * sizeof(char));
	pos += size_recipient_user * sizeof(char);
	
	/* 7) Sender host size (4 bytes) */
	(*out)->bytes[pos]   = (size_sender_host >> 24) & 0xff;
	(*out)->bytes[++pos] = (size_sender_host >> 16) & 0xff;
	(*out)->bytes[++pos] = (size_sender_host >> 8) & 0xff;
	(*out)->bytes[++pos] = size_sender_host & 0xff;
	
	/* 8) Sender host */
	memcpy(&(*out)->bytes[++pos], parcel->sender_addr->host->identifier, size_sender_host * sizeof(char));
	pos += size_sender_host * sizeof(char);
	
	/* 9) Sender user size (4 bytes) */
	(*out)->bytes[pos]   = (size_sender_user >> 24) & 0xff;
	(*out)->bytes[++pos] = (size_sender_user >> 16) & 0xff;
	(*out)->bytes[++pos] = (size_sender_user >> 8) & 0xff;
	(*out)->bytes[++pos] = size_sender_user & 0xff;
	
	/* 10) Sender user */
	memcpy(&(*out)->bytes[++pos], parcel->sender_addr->user->identifier, size_sender_user * sizeof(char));
	pos += size_sender_user * sizeof(char);
	
	/* 7) Payload size (8 bytes) */
	(*out)->bytes[pos] = (parcel->payload->len >> 56) & 0xff;
	(*out)->bytes[++pos] = (parcel->payload->len >> 48) & 0xff;
	(*out)->bytes[++pos] = (parcel->payload->len >> 40) & 0xff;
	(*out)->bytes[++pos] = (parcel->payload->len >> 32) & 0xff;
	(*out)->bytes[++pos] = (parcel->payload->len >> 24) & 0xff;
	(*out)->bytes[++pos] = (parcel->payload->len >> 16) & 0xff;
	(*out)->bytes[++pos] = (parcel->payload->len >> 8) & 0xff;
	(*out)->bytes[++pos] = parcel->payload->len & 0xff;
	
	/* 7) Payload */
	memcpy(&(*out)->bytes[++pos], parcel->payload->bytes, parcel->payload->len * sizeof(unsigned char));
	
	return status;
}

uint64_t parcel_size_get(const struct data16 *head_data)
{
	uint64_t size;
	int pos = head_data->len - 8;
	
	size = head_data->bytes[pos + 7] |
		( (uint64_t)head_data->bytes[pos + 6] << 8 ) |
		( (uint64_t)head_data->bytes[pos + 5] << 16 ) |
		( (uint64_t)head_data->bytes[pos + 4] << 24 ) |
		( (uint64_t)head_data->bytes[pos + 3] << 32 ) |
		( (uint64_t)head_data->bytes[pos + 2] << 40 ) |
		( (uint64_t)head_data->bytes[pos + 1] << 48 ) |
		( (uint64_t)head_data->bytes[pos] << 56 );
	
	return size;
}

/*
 * !INCOMPLETE!
 */
void request_free(struct token **request)
{
	while (request) {
		struct token *tmp;
		
		tmp = *request;
		*request = (*request)->next;
		free(tmp);
	}
	
	request = NULL;
}

/*
 * The service is indicated by the file extension.
 * It is the caller's responsibility to free the
 * returned pointer.
 */
int service_get(const char *filename, char **service)
{
	size_t filename_len;
	int valid;
	
	if (!filename ||
	    !service)
		return 1;
	
	filename_len = strlen(filename);
	valid = 0;
	
	/*
	 * Smallest valid filename: a.x
	 */
	if (filename_len < 3)
		return -1;
	
	/*
	 * We want to extract whatever comes after
	 * the final dot in the filename. We start
	 * from the second position because there
	 * needs to be at least 1 character after
	 * the dot.
	 */
	for (size_t i = filename_len - 2; i > 0; i--) {
		if (filename[i] == '.') {
			*service = (char *)calloc(filename_len - i, sizeof(**service));
			strncpy(*service, filename + i + 1, filename_len - i - 1);
			
			valid = 1;
			break;
		}
	}
	
	if (valid != 1) {
		free(*service);
		return -1;
	}
	
	return 0;
}

/*
 * In the case of DDM, this function might return
 * a null user.
 */
int user_get(const char *addr_str, char **user)
{
	size_t addr_len;
	size_t i_host;
	int has_host;
	
	if (!addr_str ||
	    !user)
		return 1;
	
	addr_len = strlen(addr_str);
	has_host = 0;
	i_host = 0;
	
	/*
	 * Whatever comes after the first @ symbol
	 * is the host.
	 */
	for (int i = 0; i < addr_len; i++) {
		if (addr_str[i] == '@') {
			has_host = 1;
			i_host = i;
			break;
		}
	}
	
	if (has_host == 0) {
		/* No host specified; the whole address is the user. */
		*user = (char *)calloc(addr_len + 1, sizeof(**user));
		strcpy(*user, addr_str);
	} else if (i_host > 0) {
		*user = (char *)calloc(i_host + 1, sizeof(**user));
		strncpy(*user, addr_str, i_host);
	} else {
		/* No user specified. */
		*user = NULL;
	}
	
	return 0;
}

int valid_check(const char *reqstr)
{
	char *ver_str;
	size_t head_len;
	size_t i_eol;
	size_t reqstr_len;
	uint32_t ver;
	int eol;
	int valid;
	
	if (!reqstr)
		return -1;
	
	eol = 0;
	head_len = strlen(DP_PROTO_SERV_HEAD);
	reqstr_len = strlen(reqstr);
	valid = 1;
	
	if (strlen(reqstr) < head_len ||
	    strncmp(reqstr, DP_PROTO_SERV_HEAD, head_len) != 0)
		valid = 0;
	
	/*
	 * Extract the protocol version. It should be whatever
	 * remains after the header until the first delimeter.
	 */
	for (i_eol = head_len; i_eol < reqstr_len; i_eol++) {
		char c;
		
		c = reqstr[i_eol];
		
		/*
		 * Delimeter checking should not assume a
		 * specific length. We do know, however,
		 * that it is always at least a single
		 * character in length.
		 */
		if (c == DP_PROTO_SERV_DELIM[0])
			eol = delimiter_check(reqstr, i_eol);
		
		if (eol == 1) {
			break;
		}
	}
	
	ver_str = (char *)calloc(i_eol - head_len + 1, sizeof(ver_str));
	strncpy(ver_str, reqstr + head_len, i_eol - head_len);
	ver = atoi(ver_str);
	printf("Protocol Version %d\n", ver);
	free(ver_str);
	
	return valid;
}
