//
//  crypto.h
//  server
//
//  Created by Ali Mahouk on 1/27/18.
//  Copyright © 2018 Ali Mahouk. All rights reserved.
//

#ifndef CRYPTO_H
#define CRYPTO_H


#include <openssl/sha.h>


/*************
 * FUNCTIONS *
 *************/
void sha(const unsigned char *, size_t, unsigned char[]);


#endif /* CRYPTO_H */
