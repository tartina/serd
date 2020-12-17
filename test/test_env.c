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

static SerdStatus
count_prefixes(void* handle, const SerdNode* name, const SerdNode* uri)
{
	(void)name;
	(void)uri;

	++*(int*)handle;
	return SERD_SUCCESS;
}

static void
test_env(void)
{
	SerdWorld* world = serd_world_new();

	SerdNode* hello = serd_new_string("hello\"");
	SerdNode* eg    = serd_new_uri("http://example.org/");
	SerdNode* foo_u = serd_new_uri("http://example.org/foo");
	SerdNode* foo_c = serd_new_curie("eg.2:foo");
	SerdNode* b     = serd_new_curie("invalid");
	SerdNode* pre   = serd_new_curie("eg.2");
	SerdEnv*  env   = serd_env_new(NULL);
	serd_env_set_prefix(env, pre, eg);

	assert(!serd_env_base_uri(env));
	assert(!serd_env_set_base_uri(env, NULL));
	assert(!serd_env_base_uri(env));
	assert(serd_env_set_base_uri(env, hello));
	assert(!serd_env_base_uri(env));

	SerdNode* xnode = serd_env_expand(env, hello);
	assert(!xnode);

	assert(!serd_env_expand(env, b));
	assert(!serd_env_expand(env, hello));

	assert(!serd_env_set_base_uri(env, NULL));

	serd_node_free(hello);

	SerdNode* xu = serd_env_expand(env, foo_c);
	assert(!strcmp(serd_node_string(xu), "http://example.org/foo"));
	serd_node_free(xu);

	SerdNode* badpre = serd_new_curie("hm:what");
	SerdNode* xbadpre = serd_env_expand(env, badpre);
	assert(!xbadpre);
	serd_node_free(badpre);

	SerdNode* xc = serd_env_expand(env, foo_c);
	assert(serd_node_equals(xc, foo_u));
	serd_node_free(xc);

	SerdNode* lit = serd_new_string("hello");
	assert(serd_env_set_prefix(env, b, lit));

	SerdNode* blank = serd_new_blank("b1");
	assert(!serd_env_expand(env, blank));
	serd_node_free(blank);

	int n_prefixes = 0;
	serd_env_set_prefix(env, pre, eg);
	serd_env_foreach(env, count_prefixes, &n_prefixes);
	assert(n_prefixes == 1);

	SerdNode* qualified = serd_env_qualify(env, foo_u);
	assert(serd_node_equals(qualified, foo_c));

	serd_node_free(qualified);
	serd_node_free(foo_c);
	serd_node_free(foo_u);
	serd_node_free(lit);
	serd_node_free(b);
	serd_node_free(pre);
	serd_node_free(eg);

	serd_env_free(env);
	serd_world_free(world);
}

int
main(void)
{
	test_env();
	return 0;
}
