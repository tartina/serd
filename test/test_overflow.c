/*
  Copyright 2018 David Robillard <http://drobilla.net>

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
#include <stdio.h>

static SerdStatus
test(SerdWorld* world, SerdSink* sink, const char* str, size_t stack_size)
{
	SerdReader* reader = serd_reader_new(
	        world, SERD_TURTLE, SERD_READ_VARIABLES, sink, stack_size);

	serd_reader_start_string(reader, str, NULL);
	const SerdStatus st = serd_reader_read_document(reader);
	serd_reader_free(reader);
	return st;
}

int
main(void)
{
	typedef struct
	{
		const char* str;
		size_t      stack_size;
	} Test;

	const size_t sizes = 3 * sizeof(size_t);

	const Test tests[] = {{":s :p :%99 .", sizes + 280},
	                      {":s :p <http://", sizes + 276},
	                      {":s :p eg:foo", sizes + 275},
	                      {":s :p 1234", sizes + 177},
	                      {":s :p 1234", sizes + 280},
	                      {":s :p (1 2 3 4) .", 8 * sizeof(size_t) + 357},
	                      {"@prefix eg: <http://example.org> .", sizes + 224},
	                      {":s :p \"literal\"", sizes + 264},
	                      {":s :p \"verb\"", sizes + 263},
	                      {":s :p _:blank .", sizes + 276},
	                      {":s :p ?o .", sizes + 295},
	                      {":s :p true .", sizes + 295},
	                      {":s :p true .", sizes + 329},
	                      {":s :p \"\"@en .", sizes + 302},
	                      {NULL, 0}};

	SerdWorld* world = serd_world_new();
	SerdSink*  sink  = serd_sink_new(NULL, NULL);

	for (const Test* t = tests; t->str; ++t) {
		const SerdStatus st = test(world, sink, t->str, t->stack_size);
		assert(st == SERD_ERR_OVERFLOW);
	}

	serd_sink_free(sink);
	serd_world_free(world);
	return 0;
}
