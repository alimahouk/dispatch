//
//  types.h
//  server
//
//  Created by Ali Mahouk on 12/8/17.
//  Copyright © 2017 Ali Mahouk. All rights reserved.
//

#ifndef TYPES_H
#define TYPES_H


#include <stdint.h>


struct data64 {
	unsigned char *bytes;
	uint64_t len;
};

struct data32 {
	unsigned char *bytes;
	uint32_t len;
};

struct data16 {
	unsigned char *bytes;
	uint16_t len;
};

struct path {
	char *component;
	struct path *next;
	struct path *previous;
};

struct filelist {
	struct path *path;
	struct filelist *next;
};

struct token {
	char *name;
	char *val;
	struct token *next;
};


#endif /* TYPES_H */
