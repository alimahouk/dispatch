//
//  crypto.c
//  server
//
//  Created by Ali Mahouk on 1/27/18.
//  Copyright © 2018 Ali Mahouk. All rights reserved.
//

#include "crypto.h"


/*
 * Generates the double SHA-256 hash of the given bytes.
 * digest should be of size SHA256_DIGEST_LENGTH.
 */
void sha(const unsigned char *data, size_t len, unsigned char digest[])
{
	unsigned char tmp[SHA256_DIGEST_LENGTH];
	
	SHA256(data, len, tmp);
	SHA256(tmp, SHA256_DIGEST_LENGTH, digest);
}
