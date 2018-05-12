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

#include "model.h"
#include "statement.h"
#include "world.h"

#include "serd/serd.h"

#include <stdlib.h>

typedef struct {
	SerdEnv*   env;
	SerdModel* model;
	SerdNode*  default_graph;
} SerdInserterData;

static const SerdNode*
manage_or_intern(SerdNodes* nodes, SerdNode* manage, const SerdNode* intern)
{
	return manage ? serd_nodes_manage(nodes, manage)
	              : serd_nodes_intern(nodes, intern);
}

static SerdStatus
serd_inserter_on_base(SerdInserterData* data, const SerdNode* uri)
{
	return serd_env_set_base_uri(data->env, uri);
}

static SerdStatus
serd_inserter_on_prefix(SerdInserterData* data,
                        const SerdNode*   name,
                        const SerdNode*   uri)
{
	return serd_env_set_prefix(data->env, name, uri);
}

static SerdStatus
serd_inserter_on_statement(SerdInserterData*        data,
                           const SerdStatementFlags flags,
                           const SerdStatement*     statement)
{
	(void)flags;

	const SerdNode* const subject   = serd_statement_subject(statement);
	const SerdNode* const predicate = serd_statement_predicate(statement);
	const SerdNode* const object    = serd_statement_object(statement);
	const SerdNode* const graph     = serd_statement_graph(statement);

	// Attempt to expand all nodes to eliminate CURIEs
	SerdNode* const s = serd_env_expand(data->env, subject);
	SerdNode* const p = serd_env_expand(data->env, predicate);
	SerdNode* const o = serd_env_expand(data->env, object);
	SerdNode* const g = serd_env_expand(data->env, graph);

	SerdNodes* const nodes       = data->model->world->nodes;
	const SerdNode*  model_graph = manage_or_intern(nodes, g, graph);
	if (!model_graph) {
		model_graph = serd_nodes_intern(nodes, data->default_graph);
	}

	const SerdStatus st = serd_model_add_internal(
		data->model,
		(data->model->flags & SERD_STORE_CURSORS) ? statement->cursor
		: NULL,
		manage_or_intern(nodes, s, subject),
		manage_or_intern(nodes, p, predicate),
		manage_or_intern(nodes, o, object),
		model_graph);

	return st > SERD_FAILURE ? st : SERD_SUCCESS;
}

static void
free_data(void* handle)
{
	if (handle) {
		SerdInserterData* data = (SerdInserterData*)handle;

		serd_node_free(data->default_graph);
		free(data);
	}
}

SerdSink*
serd_inserter_new(SerdModel* model, SerdEnv* env, const SerdNode* default_graph)
{
	SerdInserterData* const data =
	    (SerdInserterData*)calloc(1, sizeof(SerdInserterData));

	data->env             = env;
	data->model           = model;
	data->default_graph   = serd_node_copy(default_graph);

	SerdSink* const sink = serd_sink_new(data, free_data);

	serd_sink_set_base_func(sink, (SerdBaseFunc)serd_inserter_on_base);
	serd_sink_set_prefix_func(sink, (SerdPrefixFunc)serd_inserter_on_prefix);
	serd_sink_set_statement_func(sink,
	                             (SerdStatementFunc)serd_inserter_on_statement);

	return sink;
}
