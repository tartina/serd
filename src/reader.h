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

#ifndef SERD_READER_H
#define SERD_READER_H

#include "byte_source.h"
#include "node.h"
#include "stack.h"

#include "serd/serd.h"

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#if defined(__GNUC__)
#    define SERD_LOG_FUNC(fmt, arg1) __attribute__((format(printf, fmt, arg1)))
#else
#    define SERD_LOG_FUNC(fmt, arg1)
#endif

#ifdef SERD_STACK_CHECK
#    define SERD_STACK_ASSERT_TOP(reader, ref) \
            assert(ref == reader->allocs[reader->n_allocs - 1]);
#else
#    define SERD_STACK_ASSERT_TOP(reader, ref)
#endif

typedef struct {
	SerdNode*           graph;
	SerdNode*           subject;
	SerdNode*           predicate;
	SerdNode*           object;
	SerdStatementFlags* flags;
} ReadContext;

struct SerdReaderImpl {
	SerdWorld*        world;
	const SerdSink*   sink;
	SerdErrorSink     error_sink;
	void*             error_handle;
	SerdNode*         rdf_first;
	SerdNode*         rdf_rest;
	SerdNode*         rdf_nil;
	SerdNode*         default_graph;
	SerdByteSource    source;
	SerdStack         stack;
	SerdSyntax        syntax;
	unsigned          next_id;
	uint8_t*          buf;
	char*             bprefix;
	size_t            bprefix_len;
	bool              strict;     ///< True iff strict parsing
	bool              seen_genid;
#ifdef SERD_STACK_CHECK
	SerdNode**        allocs; ///< Stack of push offsets
	size_t            n_allocs; ///< Number of stack pushes
#endif
};

SERD_LOG_FUNC(3, 4)
SerdStatus
r_err(SerdReader* reader, SerdStatus st, const char* fmt, ...);

SerdNode* push_node_padded(SerdReader* reader,
                     size_t      maxlen,
                     SerdType    type,
                     const char* str,
                     size_t      n_bytes);

SerdNode* push_node(SerdReader* reader,
                    SerdType    type,
                    const char* str,
                    size_t      n_bytes);

SERD_PURE_FUNC size_t genid_size(SerdReader* reader);
SerdNode*             blank_id(SerdReader* reader);
void                  set_blank_id(SerdReader* reader, SerdNode* node, size_t buf_size);

SerdStatus
emit_statement(SerdReader* reader, ReadContext ctx, SerdNode* o);

SerdStatus read_n3_statement(SerdReader* reader);
SerdStatus read_nquadsDoc(SerdReader* reader);
SerdStatus read_turtleTrigDoc(SerdReader* reader);

static inline int
peek_byte(SerdReader* reader)
{
	SerdByteSource* source = &reader->source;

	return source->eof ? EOF : (int)source->read_buf[source->read_head];
}

static inline int
eat_byte_safe(SerdReader* reader, const int byte)
{
	(void)byte;

	const int c = peek_byte(reader);
	assert(c == byte);

	serd_byte_source_advance(&reader->source);
	return c;
}

static inline int
eat_byte_check(SerdReader* reader, const int byte)
{
	const int c = peek_byte(reader);
	if (c != byte) {
		r_err(reader, SERD_ERR_BAD_SYNTAX,
		      "expected `%c', not `%c'\n", byte, c);
		return 0;
	}
	return eat_byte_safe(reader, byte);
}

static inline SerdStatus
eat_string(SerdReader* reader, const char* str, unsigned n)
{
	for (unsigned i = 0; i < n; ++i) {
		if (!eat_byte_check(reader, ((const uint8_t*)str)[i])) {
			return SERD_ERR_BAD_SYNTAX;
		}
	}
	return SERD_SUCCESS;
}

static inline SerdStatus
push_byte(SerdReader* reader, SerdNode* node, const int c)
{
	assert(c != EOF);
	SERD_STACK_ASSERT_TOP(reader, node);

	char* const s = (char*)serd_stack_push(&reader->stack, 1);
	if (!s) {
		return SERD_ERR_OVERFLOW;
	}

	*(uint8_t*)(s - 1) = (uint8_t)c;
	*s                 = '\0';

	++node->n_bytes;
	return SERD_SUCCESS;
}

static inline SerdStatus
push_bytes(SerdReader* reader, SerdNode* ref, const uint8_t* bytes, unsigned len)
{
	if (reader->stack.buf_size < reader->stack.size + len) {
		return SERD_ERR_OVERFLOW;
	}

	for (unsigned i = 0; i < len; ++i) {
		push_byte(reader, ref, bytes[i]);
	}
	return SERD_SUCCESS;
}

#endif // SERD_READER_H
