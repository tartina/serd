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

#include "sink.h"
#include "statement.h"

#include "serd/serd.h"

#include <stdbool.h>
#include <stdlib.h>

#include <stdio.h>

typedef struct
{
	const SerdEnv*  env;
	const SerdSink* target;
} SerdExpanderData;

static SerdStatus
serd_expander_on_event(void* handle, const SerdEvent* event)
{
	const SerdExpanderData* const data = (SerdExpanderData*)handle;

	if (event->type != SERD_STATEMENT) {
		return serd_sink_write_event(data->target, event);
	}

	const SerdEnv* const       env       = data->env;
	const SerdStatement* const statement = event->statement.statement;

	SerdNode* xs = serd_env_expand(env, serd_statement_subject(statement));
	SerdNode* xp = serd_env_expand(env, serd_statement_predicate(statement));
	SerdNode* xo = serd_env_expand(env, serd_statement_object(statement));
	SerdNode* xg = serd_env_expand(env, serd_statement_graph(statement));

	const SerdNode* const s = xs ? xs : serd_statement_subject(statement);
	const SerdNode* const p = xp ? xp : serd_statement_predicate(statement);
	const SerdNode* const o = xo ? xo : serd_statement_object(statement);
	const SerdNode* const g = xg ? xg : serd_statement_graph(statement);
	/* if (!xs || !p || !o || !g) { */
	/* 	fprintf(stderr, "invalid\n"); */
	/* 	return SERD_ERR_INVALID; */
	/* } */

	const SerdStatement expanded       = {{s, p, o, g}, statement->cursor};
	SerdEvent           expanded_event = {SERD_STATEMENT};

	expanded_event.statement.flags     = event->statement.flags;
	expanded_event.statement.statement = &expanded;

	SerdStatus st = serd_sink_write_event(data->target, &expanded_event);

	serd_node_free(xg);
	serd_node_free(xo);
	serd_node_free(xp);
	serd_node_free(xs);

	return st;
}

SerdSink*
serd_expander_new(const SerdSink* const target, const SerdEnv* env)
{
	SerdExpanderData* data =
	    (SerdExpanderData*)calloc(1, sizeof(SerdExpanderData));

	data->env    = env;
	data->target = target;

	SerdSink* const sink = serd_sink_new(data, free);

	serd_sink_set_event_func(sink, serd_expander_on_event);

	return sink;
}
