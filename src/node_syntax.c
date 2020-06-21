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

#include "writer.h"

#include "serd/serd.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static SerdStatus
on_node_string_event(void* handle, const SerdEvent* event)
{
	if (event->type == SERD_STATEMENT) {
		*(SerdNode**)handle = serd_node_copy(
		    serd_statement_object(event->statement.statement));
	}

	return SERD_SUCCESS;
}

SerdNode*
serd_node_from_syntax(const char* str, const SerdSyntax syntax)
{
	static const char* const prelude =
	    "_:s <http://www.w3.org/2000/01/rdf-schema#object>";

	const size_t str_len = strlen(str);
	const size_t doc_len = strlen(prelude) + str_len + 3;
	char* const  doc     = (char*)calloc(doc_len + 1, 1);

	snprintf(doc, doc_len + 1, "%s %s .", prelude, str);

	SerdNode*   object = NULL;
	SerdWorld*  world  = serd_world_new();
	SerdSink*   sink   = serd_sink_new(&object, NULL);

	SerdByteSource* byte_source = serd_byte_source_new_string(doc, NULL);

	SerdReader* reader = serd_reader_new(
	    world, syntax, SERD_READ_EXACT_BLANKS, sink, 1024 + doc_len);

	serd_world_set_log_func(world, serd_quiet_error_func, NULL);
	serd_sink_set_event_func(sink, on_node_string_event);
	serd_reader_start(reader, byte_source);
	serd_reader_read_document(reader);
	serd_reader_finish(reader);
	serd_byte_source_free(byte_source);
	serd_reader_free(reader);
	serd_sink_free(sink);
	serd_world_free(world);
	free(doc);

	return object;
}

char*
serd_node_to_syntax(const SerdNode* node, const SerdSyntax syntax)
{
	SerdWorld*    world  = serd_world_new();
	SerdEnv*      env    = serd_env_new(NULL);
	SerdBuffer    buffer = {NULL, 0};
	SerdByteSink* bsink  = serd_byte_sink_new_buffer(&buffer);
	SerdWriter*   writer = serd_writer_new(world, syntax, 0, env, bsink);

	serd_world_set_log_func(world, serd_quiet_error_func, NULL);

	SerdStatus st = serd_writer_write_node(writer, node);
	if (!st) {
		st = serd_writer_finish(writer);
	}

	char* const result = serd_buffer_sink_finish(&buffer);
	serd_writer_free(writer);
	serd_byte_sink_free(bsink);
	serd_env_free(env);
	serd_world_free(world);

	if (st) {
		serd_free(result);
		return NULL;
	}

	return result;
}
