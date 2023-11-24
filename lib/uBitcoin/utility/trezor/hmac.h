/**
 * Copyright (c) 2013-2014 Tomas Dzetkulic
 * Copyright (c) 2013-2014 Pavol Rusnak
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT HMAC_SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES
 * OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef UBITCOIN_HMAC_H
#define UBITCOIN_HMAC_H

#include <stdint.h>
#include "sha2.h"

typedef struct _HMAC_SHA256_CTX {
	uint8_t o_key_pad[SHA256_BLOCK_LENGTH];
	SHA256_CTX ctx;
} HMAC_SHA256_CTX;

typedef struct _HMAC_SHA512_CTX {
	uint8_t o_key_pad[SHA512_BLOCK_LENGTH];
	SHA512_CTX ctx;
} HMAC_SHA512_CTX;

#ifdef __cplusplus
extern "C"
{
#endif

void ubtc_hmac_sha256_Init(HMAC_SHA256_CTX *hctx, const uint8_t *key, const uint32_t keylen);
void ubtc_hmac_sha256_Update(HMAC_SHA256_CTX *hctx, const uint8_t *msg, const uint32_t msglen);
void ubtc_hmac_sha256_Final(HMAC_SHA256_CTX *hctx, uint8_t *hmac);
void ubtc_hmac_sha256(const uint8_t *key, const uint32_t keylen, const uint8_t *msg, const uint32_t msglen, uint8_t *hmac);
void ubtc_hmac_sha256_prepare(const uint8_t *key, const uint32_t keylen, uint32_t *opad_digest, uint32_t *ipad_digest);

#ifdef __cplusplus
} /* end of extern "C" */
#endif

#endif
