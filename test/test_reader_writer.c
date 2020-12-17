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
#include <stdlib.h>
#include <string.h>

typedef struct {
	int             n_statements;
	const SerdNode* graph;
} ReaderTest;

static SerdStatus
test_sink(void*              handle,
          SerdStatementFlags flags,
          const SerdNode*    graph,
          const SerdNode*    subject,
          const SerdNode*    predicate,
          const SerdNode*    object)
{
	(void)flags;
	(void)subject;
	(void)predicate;
	(void)object;

	ReaderTest* rt = (ReaderTest*)handle;
	++rt->n_statements;
	rt->graph = graph;
	return SERD_SUCCESS;
}

/// Returns EOF after a statement, then succeeds again (like a socket)
static size_t
eof_test_read(void* buf, size_t size, size_t nmemb, void* stream)
{
	assert(nmemb == 1);

	static const char* const string = "_:s1 <http://example.org/p> _:o1 .\n"
	                                  "_:s2 <http://example.org/p> _:o2 .\n";

	size_t* count = (size_t*)stream;
	if (*count == 34 || *count == 35 || *count + nmemb >= strlen(string)) {
		++*count;
		return 0;
	}

	memcpy((char*)buf, string + *count, size * nmemb);
	*count += nmemb;
	return nmemb;
}

static int
eof_test_error(void* stream)
{
	(void)stream;
	return 0;
}

static void
test_read_chunks(void)
{
	SerdWorld*        world  = serd_world_new();
	ReaderTest* const rt     = (ReaderTest*)calloc(1, sizeof(ReaderTest));
	FILE* const       f      = tmpfile();
	static const char null   = 0;
	SerdSink          sink   = {rt, NULL, NULL, test_sink, NULL};
	SerdReader*       reader = serd_reader_new(world, SERD_TURTLE, &sink);

	assert(reader);
	assert(f);

	SerdStatus st = serd_reader_start_stream(reader,
	                                         (SerdReadFunc)fread,
	                                         (SerdStreamErrorFunc)ferror,
	                                         f,
	                                         NULL,
	                                         1);
	assert(st == SERD_SUCCESS);

	// Write two statement separated by null characters
	fprintf(f, "@prefix eg: <http://example.org/> .\n");
	fprintf(f, "eg:s eg:p eg:o1 .\n");
	fwrite(&null, sizeof(null), 1, f);
	fprintf(f, "eg:s eg:p eg:o2 .\n");
	fwrite(&null, sizeof(null), 1, f);
	fseek(f, 0, SEEK_SET);

	// Read prefix
	st = serd_reader_read_chunk(reader);
	assert(st == SERD_SUCCESS);
	assert(rt->n_statements == 0);

	// Read first statement
	st = serd_reader_read_chunk(reader);
	assert(st == SERD_SUCCESS);
	assert(rt->n_statements == 1);

	// Read terminator
	st = serd_reader_read_chunk(reader);
	assert(st == SERD_FAILURE);
	assert(rt->n_statements == 1);

	// Read second statement (after null terminator)
	st = serd_reader_read_chunk(reader);
	assert(st == SERD_SUCCESS);
	assert(rt->n_statements == 2);

	// Read terminator
	st = serd_reader_read_chunk(reader);
	assert(st == SERD_FAILURE);
	assert(rt->n_statements == 2);

	// EOF
	st = serd_reader_read_chunk(reader);
	assert(st == SERD_FAILURE);
	assert(rt->n_statements == 2);

	serd_reader_free(reader);
	fclose(f);
	free(rt);
	serd_world_free(world);
}

static void
test_read_string(void)
{
	SerdWorld*  world  = serd_world_new();
	ReaderTest* rt     = (ReaderTest*)calloc(1, sizeof(ReaderTest));
	SerdSink    sink   = {rt, NULL, NULL, test_sink, NULL};
	SerdReader* reader = serd_reader_new(world, SERD_TURTLE, &sink);
	assert(reader);

	// Test reading a string that ends exactly at the end of input (no newline)
	const SerdStatus st =
		serd_reader_read_string(reader,
		                        "<http://example.org/s> <http://example.org/p> "
		                        "<http://example.org/o> .");

	assert(!st);
	assert(rt->n_statements == 1);

	serd_reader_free(reader);
	free(rt);
	serd_world_free(world);
}

static void
test_writer(const char* const path)
{
	FILE* fd = fopen(path, "wb");
	SerdEnv* env = serd_env_new(NULL);
	assert(fd);

	SerdWorld* world = serd_world_new();

	SerdWriter* writer = serd_writer_new(world,
	                                     SERD_TURTLE,
	                                     (SerdStyle)0,
	                                     env,
	                                     NULL,
	                                     (SerdWriteFunc)fwrite,
	                                     fd);
	assert(writer);

	serd_writer_chop_blank_prefix(writer, "tmp");
	serd_writer_chop_blank_prefix(writer, NULL);

	SerdNode* lit = serd_new_string(SERD_LITERAL, "hello");

	const SerdSink* const iface = serd_writer_sink(writer);
	assert(iface->base(iface->handle, lit));
	assert(iface->prefix(iface->handle, lit, lit));
	assert(serd_writer_env(writer) == env);

	uint8_t buf[] = { 0xEF, 0xBF, 0xBD, 0 };
	SerdNode* s = serd_new_string(SERD_URI, "");
	SerdNode* p = serd_new_string(SERD_URI, "http://example.org/pred");
	SerdNode* o = serd_new_string(SERD_LITERAL, (char*)buf);

	// Write 3 invalid statements (should write nothing)
	const SerdNode* junk[][3] = { { s, o, o },
	                              { o, p, o },
	                              { s, o, p } };
	for (size_t i = 0; i < sizeof(junk) / (sizeof(SerdNode*) * 3); ++i) {
		assert(iface->statement(
		    iface->handle, 0, NULL, junk[i][0], junk[i][1], junk[i][2]));
	}

	SerdNode* t = serd_new_literal((char*)buf, "urn:Type", NULL);
	SerdNode* l = serd_new_literal((char*)buf, NULL, "en");
	const SerdNode* good[][5] = { { s, p, o },
	                              { s, p, o },
	                              { s, p, t },
	                              { s, p, l },
	                              { s, p, l },
	                              { s, p, t },
	                              { s, p, l },
	                              { s, p, o },
	                              { s, p, o },
	                              { s, p, o } };
	for (size_t i = 0; i < sizeof(good) / (sizeof(SerdNode*) * 5); ++i) {
		assert(!iface->statement(
		        iface->handle, 0, NULL, good[i][0], good[i][1], good[i][2]));
	}

	// Write statements with bad UTF-8 (should be replaced)
	const char bad_str[] = { (char)0xFF, (char)0x90, 'h', 'i', 0 };
	SerdNode*  bad_lit   = serd_new_string(SERD_LITERAL, bad_str);
	SerdNode*  bad_uri   = serd_new_string(SERD_URI, bad_str);
	assert(!iface->statement(iface->handle, 0, NULL, s, p, bad_lit));
	assert(!iface->statement(iface->handle, 0, NULL, s, p, bad_uri));
	serd_node_free(bad_lit);
	serd_node_free(bad_uri);

	// Write 1 valid statement
	serd_node_free(o);
	o = serd_new_string(SERD_LITERAL, "hello");
	assert(!iface->statement(iface->handle, 0, NULL, s, p, o));

	serd_writer_free(writer);
	serd_node_free(lit);
	serd_node_free(o);
	serd_node_free(t);
	serd_node_free(l);

	// Test buffer sink
	SerdBuffer buffer = { NULL, 0 };
	writer = serd_writer_new(
		world, SERD_TURTLE, (SerdStyle)0, env, NULL, serd_buffer_sink, &buffer);

	o = serd_new_string(SERD_URI, "http://example.org/base");
	assert(!serd_writer_set_base_uri(writer, o));

	serd_node_free(o);
	serd_writer_free(writer);
	char* out = serd_buffer_sink_finish(&buffer);

	assert(!strcmp(out, "@base <http://example.org/base> .\n"));
	serd_free(out);

	serd_node_free(p);
	serd_node_free(s);

	serd_env_free(env);
	serd_world_free(world);
	fclose(fd);
}

static void
test_reader(const char* path)
{
	SerdWorld*  world  = serd_world_new();
	ReaderTest* rt     = (ReaderTest*)calloc(1, sizeof(ReaderTest));
	SerdSink    sink   = { rt, NULL, NULL, test_sink, NULL };
	SerdReader* reader = serd_reader_new(world, SERD_TURTLE, &sink);
	assert(reader);

	SerdNode* g = serd_new_string(SERD_URI, "http://example.org/");
	serd_reader_set_default_graph(reader, g);
	serd_reader_add_blank_prefix(reader, "tmp");

#if defined(__GNUC__)
#	pragma GCC diagnostic push
#	pragma GCC diagnostic ignored "-Wnonnull"
#endif
	serd_reader_add_blank_prefix(reader, NULL);
#if defined(__GNUC__)
#	pragma GCC diagnostic pop
#endif

	serd_node_free(g);

	assert(serd_reader_read_file(reader, "http://notafile"));
	assert(serd_reader_read_file(reader, "file:///better/not/exist"));
	assert(serd_reader_read_file(reader, "file://"));

	const SerdStatus st = serd_reader_read_file(reader, path);
	assert(!st);
	assert(rt->n_statements == 13);
	assert(rt->graph && serd_node_string(rt->graph) &&
	       !strcmp(serd_node_string(rt->graph), "http://example.org/"));

	assert(serd_reader_read_string(reader, "This isn't Turtle at all."));

	// A read of a big page hits EOF then fails to read chunks immediately
	{
		FILE* temp = tmpfile();
		assert(temp);
		fprintf(temp, "_:s <http://example.org/p> _:o .\n");
		fflush(temp);
		fseek(temp, 0L, SEEK_SET);

		serd_reader_start_stream(reader,
		                         (SerdReadFunc)fread,
		                         (SerdStreamErrorFunc)ferror,
		                         temp,
		                         NULL,
		                         4096);

		assert(serd_reader_read_chunk(reader) == SERD_SUCCESS);
		assert(serd_reader_read_chunk(reader) == SERD_FAILURE);
		assert(serd_reader_read_chunk(reader) == SERD_FAILURE);

		serd_reader_end_stream(reader);
		fclose(temp);
	}

	// A byte-wise reader that hits EOF once then continues (like a socket)
	{
		size_t n_reads = 0;
		serd_reader_start_stream(reader,
		                         (SerdReadFunc)eof_test_read,
		                         (SerdStreamErrorFunc)eof_test_error,
		                         &n_reads,
		                         NULL,
		                         1);

		assert(serd_reader_read_chunk(reader) == SERD_SUCCESS);
		assert(serd_reader_read_chunk(reader) == SERD_FAILURE);
		assert(serd_reader_read_chunk(reader) == SERD_SUCCESS);
		assert(serd_reader_read_chunk(reader) == SERD_FAILURE);
	}

	serd_reader_free(reader);
	free(rt);
	serd_world_free(world);
}

int
main(void)
{
	test_read_chunks();
	test_read_string();

	const char* const path = "serd_test.ttl";
	test_writer(path);
	test_reader(path);

	printf("Success\n");
	return 0;
}
