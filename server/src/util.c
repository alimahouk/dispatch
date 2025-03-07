//
//  util.c
//  server
//
//  Created by Ali Mahouk on 1/28/18.
//  Copyright © 2018 Ali Mahouk. All rights reserved.
//

#include "util.h"

#include <stdlib.h>
#include <string.h>


char *path_str(const struct path *path)
{
	char *str;
	struct path *iter;
	
	if (!path)
		return NULL;
	
	iter = (struct path *)path;
	str = NULL;
	
	if (!iter->next &&
	    iter->previous) {
		/* Last path component; rewind to the beginning. */
		while (iter->previous)
			iter = iter->previous;
	}
	
	while (iter) {
		if (str) {
			/* +2 for the '/' and \0. */
			str = (char *)realloc(str, strlen(str) + strlen(iter->component) + 2);
			strcat(str, "/");
			strcat(str, iter->component);
		} else {
			str = (char *)calloc(strlen(iter->component) + 1, sizeof(char));
			strcpy(str, iter->component);
		}
		
		iter = iter->next;
	}
	
	return str;
}

time_t timestamp(void)
{
	time_t curr_time;
	
	time(&curr_time);
	
	return curr_time;
}

/*
 * It is the caller's responsibility to free the
 * returned pointer.
 */
char *uuid_str(void)
{
	char *uuid_str;
	uuid_t id;
	
	uuid_str = (char *)malloc(UUID_STR_LEN * sizeof(unsigned char) + 1); // e.g. "1b4e28ba-2fa1-11d2-883f-0016d3cca427" + '\0'
	
	uuid_generate(id);
	uuid_unparse_lower(id, uuid_str);
	
	return uuid_str;
}
