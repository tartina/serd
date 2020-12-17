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

#include "sink.h"

#include "statement.h"

#include "serd/serd.h"

#include <stdlib.h>

SerdSink*
serd_sink_new(void* handle)
{
	SerdSink* sink = (SerdSink*)calloc(1, sizeof(SerdSink));

	sink->handle = handle;
	return sink;
}

void
serd_sink_free(SerdSink* sink)
{
	free(sink);
}

SerdStatus
serd_sink_set_base_func(SerdSink* sink, SerdBaseSink base_func)
{
	sink->base = base_func;
	return SERD_SUCCESS;
}

SerdStatus
serd_sink_set_prefix_func(SerdSink* sink, SerdPrefixSink prefix_func)
{
	sink->prefix = prefix_func;
	return SERD_SUCCESS;
}

SerdStatus
serd_sink_set_statement_func(SerdSink* sink, SerdStatementSink statement_func)
{
	sink->statement = statement_func;
	return SERD_SUCCESS;
}

SerdStatus
serd_sink_set_end_func(SerdSink* sink, SerdEndSink end_func)
{
	sink->end = end_func;
	return SERD_SUCCESS;
}

SerdStatus
serd_sink_write_base(const SerdSink* sink, const SerdNode* uri)
{
	return sink->base ? sink->base(sink->handle, uri) : SERD_SUCCESS;
}

SerdStatus
serd_sink_write_prefix(const SerdSink* sink,
                       const SerdNode* name,
                       const SerdNode* uri)
{
	return sink->prefix ? sink->prefix(sink->handle, name, uri) : SERD_SUCCESS;
}

SerdStatus
serd_sink_write_statement(const SerdSink*          sink,
                          const SerdStatementFlags flags,
                          const SerdStatement*     statement)
{
	return sink->statement ? sink->statement(sink->handle, flags, statement)
	                       : SERD_SUCCESS;
}

SerdStatus
serd_sink_write(const SerdSink*          sink,
                const SerdStatementFlags flags,
                const SerdNode*          subject,
                const SerdNode*          predicate,
                const SerdNode*          object,
                const SerdNode*          graph)
{
	const SerdStatement statement = { { subject, predicate, object, graph },
		                              NULL };
	return serd_sink_write_statement(sink, flags, &statement);
}

SerdStatus
serd_sink_write_end(const SerdSink* sink, const SerdNode* node)
{
	return sink->end ? sink->end(sink->handle, node) : SERD_SUCCESS;
}
