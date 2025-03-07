//
//  util.h
//  server
//
//  Created by Ali Mahouk on 1/28/18.
//  Copyright © 2018 Ali Mahouk. All rights reserved.
//

#ifndef UTIL_H
#define UTIL_H


#include <time.h>
#include "types.h"
#include <uuid/uuid.h>


/*************
 * CONSTANTS *
 *************/
static const int UUID_LEN = 16;
static const int UUID_STR_LEN = 36;

/*************
 * FUNCTIONS *
 *************/
char *path_str(const struct path *);
time_t timestamp(void);
char *uuid_str(void);


#endif /* UTIL_H */
