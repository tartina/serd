/*
  Copyright 2019-2020 David Robillard <http://drobilla.net>

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

#include "serd/serd.h"

#include <stdlib.h>

typedef struct
{
	const SerdSink* target;
	SerdNode*       subject;
	SerdNode*       predicate;
	SerdNode*       object;
	SerdNode*       graph;
} SerdFilterData;

static void
free_data(void* handle)
{
	if (handle) {
		SerdFilterData* data = (SerdFilterData*)handle;

		serd_node_free(data->subject);
		serd_node_free(data->predicate);
		serd_node_free(data->object);
		serd_node_free(data->graph);
		free(data);
	}
}

static SerdStatus
serd_filter_on_event(void* handle, const SerdEvent* event)
{
	const SerdFilterData* const data = (SerdFilterData*)handle;

	if (event->type != SERD_STATEMENT) {
		return serd_sink_write_event(data->target, event);
	} else if (serd_statement_matches(event->statement.statement,
	                                  data->subject,
	                                  data->predicate,
	                                  data->object,
	                                  data->graph)) {
		return serd_sink_write_event(data->target, event);
	}

	return SERD_SUCCESS;
}

SerdSink*
serd_filter_new(const SerdSink* target,
                const SerdNode* subject,
                const SerdNode* predicate,
                const SerdNode* object,
                const SerdNode* graph)
{
	SerdFilterData* data = (SerdFilterData*)calloc(1, sizeof(SerdFilterData));

	data->target = target;

	if (subject && serd_node_type(subject) != SERD_VARIABLE) {
		data->subject = serd_node_copy(subject);
	}

	if (predicate && serd_node_type(predicate) != SERD_VARIABLE) {
		data->predicate = serd_node_copy(predicate);
	}

	if (object && serd_node_type(object) != SERD_VARIABLE) {
		data->object = serd_node_copy(object);
	}

	if (graph && serd_node_type(graph) != SERD_VARIABLE) {
		data->graph = serd_node_copy(graph);
	}

	SerdSink* sink = serd_sink_new(data, free_data);

	serd_sink_set_event_func(sink, serd_filter_on_event);

	return sink;
}
