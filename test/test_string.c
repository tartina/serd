/*
  Copyright 2011-2020 David Robillard <http://drobilla.net>

  Permission to use, copy, modify, and/or distribute this software for any
  purpose with or without fee is hereby granted, provided that the above
  copyright notice and this permission notice appear in all copies.

  THIS SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
  WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
  MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
  ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
  WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
  ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
  OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/

#undef NDEBUG

#include "serd/serd.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static void
test_strlen(void)
{
	const uint8_t str[] = { '"', '5', 0xE2, 0x82, 0xAC, '"', '\n', 0 };

	SerdNodeFlags flags   = 0;
	size_t        n_bytes = serd_strlen((const char*)str, &flags);
	assert(n_bytes == 7 && flags == (SERD_HAS_QUOTE|SERD_HAS_NEWLINE));
	assert(serd_strlen((const char*)str, NULL) == 7);
}

static void
test_strerror(void)
{
	const char* msg = serd_strerror(SERD_SUCCESS);
	assert(!strcmp(msg, "Success"));
	for (int i = SERD_FAILURE; i <= SERD_ERR_OVERFLOW; ++i) {
		msg = serd_strerror((SerdStatus)i);
		assert(strcmp(msg, "Success"));
	}

	msg = serd_strerror((SerdStatus)-1);
	assert(!strcmp(msg, "Unknown error"));
}

int
main(void)
{
	test_strlen();
	test_strerror();

	printf("Success\n");
	return 0;
}
