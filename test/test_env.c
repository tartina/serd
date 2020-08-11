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
#include <string.h>

#define NS_EG "http://example.org/"

static SerdStatus
count_prefixes(void* handle, const SerdEvent* event)
{
	*(int*)handle += event->type == SERD_PREFIX;

	return SERD_SUCCESS;
}

static void
test_null(void)
{
	SerdNode* const eg = serd_new_uri(NS_EG);

	// Accessors are tolerant to a NULL env for convenience
	assert(!serd_env_base_uri(NULL));
	assert(!serd_env_expand(NULL, NULL));
	assert(!serd_env_qualify(NULL, eg));

	// Only null is equal to null
	assert(serd_env_equals(NULL, NULL));

	serd_node_free(eg);
}

static void
test_base_uri(void)
{
	SerdEnv* const  env   = serd_env_new(NULL);
	SerdNode* const empty = serd_new_uri("");
	SerdNode* const hello = serd_new_string("hello");
	SerdNode* const eg    = serd_new_uri(NS_EG);

	// Test that invalid calls work as expected
	assert(!serd_env_base_uri(env));
	assert(!serd_env_set_base_uri(env, NULL));
	assert(!serd_env_base_uri(env));
	assert(serd_env_set_base_uri(env, empty) == SERD_ERR_BAD_ARG);
	assert(serd_env_set_base_uri(env, hello) == SERD_ERR_BAD_ARG);
	assert(!serd_env_base_uri(env));

	// Set a valid base URI
	assert(!serd_env_set_base_uri(env, eg));
	assert(serd_node_equals(serd_env_base_uri(env), eg));

	// Reset the base URI
	assert(!serd_env_set_base_uri(env, NULL));
	assert(!serd_env_base_uri(env));

	serd_node_free(eg);
	serd_node_free(hello);
	serd_node_free(empty);
	serd_env_free(env);
}

static void
test_set_prefix(void)
{
	SerdEnv* const  env   = serd_env_new(NULL);
	SerdNode* const name1 = serd_new_string("eg.1");
	SerdNode* const name2 = serd_new_string("eg.2");
	SerdNode* const eg    = serd_new_uri(NS_EG);
	SerdNode* const curie = serd_new_curie("invalid");
	SerdNode* const rel   = serd_new_uri("rel");
	SerdNode* const base  = serd_new_uri("http://example.org/");

	// Test that invalid calls work as expected
	assert(serd_env_set_prefix(env, curie, eg) == SERD_ERR_BAD_ARG);
	assert(serd_env_set_prefix(env, name1, curie) == SERD_ERR_BAD_ARG);

	// Set a valid prefix
	assert(!serd_env_set_prefix(env, name1, eg));

	// Test setting a prefix from a relative URI
	assert(serd_env_set_prefix(env, name2, rel) == SERD_ERR_BAD_ARG);
	assert(!serd_env_set_base_uri(env, base));
	assert(!serd_env_set_prefix(env, name2, rel));

	// Test setting a prefix from strings
	assert(!serd_env_set_prefix_from_strings(env,
	                                         "eg.3",
	                                         "http://example.org/three"));

	// Test that we ended up with the expected number of prefixes
	size_t    n_prefixes          = 0;
	SerdSink* count_prefixes_sink = serd_sink_new(&n_prefixes, NULL);
	serd_sink_set_event_func(count_prefixes_sink, count_prefixes);
	serd_env_write_prefixes(env, count_prefixes_sink);
	serd_sink_free(count_prefixes_sink);
	assert(n_prefixes == 3);

	serd_node_free(base);
	serd_node_free(rel);
	serd_node_free(curie);
	serd_node_free(eg);
	serd_node_free(name2);
	serd_node_free(name1);
	serd_env_free(env);
}

static void
test_expand(void)
{
	SerdNode* const name    = serd_new_string("eg.1");
	SerdNode* const eg      = serd_new_uri(NS_EG);
	SerdNode* const blank   = serd_new_blank("b1");
	SerdNode* const rel     = serd_new_uri("rel");
	SerdNode* const base    = serd_new_uri("http://example.org/b/");
	SerdNode* const c1      = serd_new_curie("eg.1:foo");
	SerdNode* const c1_full = serd_new_uri("http://example.org/foo");
	SerdNode* const c2      = serd_new_curie("hm:what");
	SerdNode* const type    = serd_new_uri("Type");
	SerdNode* const typed   = serd_new_typed_literal("data", type);
	SerdEnv* const  env     = serd_env_new(base);

	assert(!serd_env_set_prefix(env, name, eg));

	assert(!serd_env_expand(env, name));
	assert(!serd_env_expand(env, blank));

	// Expand CURIE
	SerdNode* const c1_out = serd_env_expand(env, c1);
	assert(serd_node_equals(c1_out, c1_full));
	serd_node_free(c1_out);

	// Expand relative URI
	SerdNode* const rel_out = serd_env_expand(env, rel);
	assert(!strcmp(serd_node_string(rel_out), "http://example.org/b/rel"));
	serd_node_free(rel_out);

	// Expand literal with URI datatype
	SerdNode* const typed_out = serd_env_expand(env, typed);
	assert(typed_out);
	assert(!strcmp(serd_node_string(typed_out), "data"));
	assert(serd_node_datatype(typed_out));
	assert(!strcmp(serd_node_string(serd_node_datatype(typed_out)),
	               "http://example.org/b/Type"));
	serd_node_free(typed_out);

	assert(!serd_env_expand(env, c2));

	serd_env_free(env);
	serd_node_free(typed);
	serd_node_free(type);
	serd_node_free(c2);
	serd_node_free(c1_full);
	serd_node_free(c1);
	serd_node_free(base);
	serd_node_free(rel);
	serd_node_free(blank);
	serd_node_free(eg);
	serd_node_free(name);
}

static void
test_qualify(void)
{
	SerdNode* const name = serd_new_string("eg");
	SerdNode* const eg   = serd_new_uri(NS_EG);
	SerdNode* const u1   = serd_new_uri("http://example.org/foo");
	SerdNode* const c1   = serd_new_curie("eg:foo");
	SerdNode* const u2   = serd_new_uri("http://drobilla.net/bar");
	SerdEnv* const  env  = serd_env_new(NULL);

	assert(!serd_env_set_prefix(env, name, eg));

	assert(!serd_env_expand(env, name));

	SerdNode* const u1_out = serd_env_qualify(env, u1);
	assert(serd_node_equals(u1_out, c1));
	serd_node_free(u1_out);

	assert(!serd_env_qualify(env, u2));

	serd_env_free(env);
	serd_node_free(u2);
	serd_node_free(c1);
	serd_node_free(u1);
	serd_node_free(eg);
	serd_node_free(name);
}

static void
test_equals(void)
{
	SerdNode* const base1 = serd_new_uri(NS_EG "b1/");
	SerdNode* const base2 = serd_new_uri(NS_EG "b2/");
	SerdEnv* const  env1  = serd_env_new(base1);
	SerdEnv* const  env2  = serd_env_new(base2);

	assert(!serd_env_equals(env1, NULL));
	assert(!serd_env_equals(NULL, env1));
	assert(serd_env_equals(NULL, NULL));
	assert(!serd_env_equals(env1, env2));

	serd_env_set_base_uri(env2, base1);
	assert(serd_env_equals(env1, env2));

	assert(!serd_env_set_prefix_from_strings(env1, "n1", NS_EG "n1"));
	assert(!serd_env_equals(env1, env2));
	assert(!serd_env_set_prefix_from_strings(env2, "n1", NS_EG "othern1"));
	assert(!serd_env_equals(env1, env2));
	assert(!serd_env_set_prefix_from_strings(env2, "n1", NS_EG "n1"));
	assert(serd_env_equals(env1, env2));

	serd_env_set_base_uri(env2, base2);
	assert(!serd_env_equals(env1, env2));

	SerdEnv* const env3 = serd_env_copy(env2);
	assert(serd_env_equals(env3, env2));
	serd_env_free(env3);

	serd_node_free(base1);
	serd_node_free(base2);
	serd_env_free(env1);
	serd_env_free(env2);
}

int
main(void)
{
	test_null();
	test_base_uri();
	test_set_prefix();
	test_expand();
	test_qualify();
	test_equals();
	return 0;
}
