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

#include "env.h"

#include "node.h"

#include "serd/serd.h"

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
	SerdNode* name;
	SerdNode* uri;
} SerdPrefix;

struct SerdEnvImpl {
	SerdPrefix* prefixes;
	size_t      n_prefixes;
	SerdNode*   base_uri_node;
	SerdURI     base_uri;
};

SerdEnv*
serd_env_new(const SerdNode* base_uri)
{
	SerdEnv* env = (SerdEnv*)calloc(1, sizeof(struct SerdEnvImpl));
	if (env && base_uri) {
		serd_env_set_base_uri(env, base_uri);
	}
	return env;
}

SerdEnv*
serd_env_copy(const SerdEnv* env)
{
	if (!env) {
		return NULL;
	}

	SerdEnv* copy = (SerdEnv*)calloc(1, sizeof(struct SerdEnvImpl));

	copy->n_prefixes = env->n_prefixes;
	copy->prefixes = (SerdPrefix*)malloc(copy->n_prefixes * sizeof(SerdPrefix));
	for (size_t i = 0; i < copy->n_prefixes; ++i) {
		copy->prefixes[i].name = serd_node_copy(env->prefixes[i].name);
		copy->prefixes[i].uri  = serd_node_copy(env->prefixes[i].uri);
	}

	serd_env_set_base_uri(copy, serd_env_base_uri(env));
	return copy;
}

void
serd_env_free(SerdEnv* env)
{
	if (!env) {
		return;
	}

	for (size_t i = 0; i < env->n_prefixes; ++i) {
		serd_node_free(env->prefixes[i].name);
		serd_node_free(env->prefixes[i].uri);
	}
	free(env->prefixes);
	serd_node_free(env->base_uri_node);
	free(env);
}

bool
serd_env_equals(const SerdEnv* a, const SerdEnv* b)
{
	if (!a || !b) {
		return !a == !b;
	} else if (a->n_prefixes != b->n_prefixes ||
	    !serd_node_equals(a->base_uri_node, b->base_uri_node)) {
		return false;
	}

	for (size_t i = 0; i < a->n_prefixes; ++i) {
		if (!serd_node_equals(a->prefixes[i].name, b->prefixes[i].name) ||
		    !serd_node_equals(a->prefixes[i].uri, b->prefixes[i].uri)) {
			return false;
		}
	}

	return true;
}

const SerdURI*
serd_env_get_parsed_base_uri(const SerdEnv* env)
{
	return &env->base_uri;
}

const SerdNode*
serd_env_base_uri(const SerdEnv* env)
{
	return env ? env->base_uri_node : NULL;
}

SerdStatus
serd_env_set_base_uri(SerdEnv*        env,
                      const SerdNode* uri)
{
	if (uri && uri->type != SERD_URI) {
		return SERD_ERR_BAD_ARG;
	} else if (!uri) {
		serd_node_free(env->base_uri_node);
		env->base_uri_node = NULL;
		env->base_uri      = SERD_URI_NULL;
		return SERD_SUCCESS;
	}

	// Resolve base URI and create a new node and URI for it
	SerdNode* base_uri_node =
	    serd_new_resolved_uri_i(serd_node_string(uri), &env->base_uri);

	if (base_uri_node) {
		// Replace the current base URI
		SerdURI   base_uri;
		serd_uri_parse(serd_node_string(base_uri_node), &base_uri);
		serd_node_free(env->base_uri_node);
		env->base_uri_node = base_uri_node;
		env->base_uri      = base_uri;
		return SERD_SUCCESS;
	}

	return SERD_ERR_BAD_ARG;
}

static inline SERD_PURE_FUNC SerdPrefix*
serd_env_find(const SerdEnv* env,
              const char*    name,
              size_t         name_len)
{
	for (size_t i = 0; i < env->n_prefixes; ++i) {
		const SerdNode* const prefix_name = env->prefixes[i].name;
		if (prefix_name->n_bytes == name_len) {
			if (!memcmp(serd_node_string(prefix_name), name, name_len)) {
				return &env->prefixes[i];
			}
		}
	}
	return NULL;
}

static void
serd_env_add(SerdEnv* env, const SerdNode* name, SerdNode* uri)
{
	const char*       name_str = serd_node_string(name);
	SerdPrefix* const prefix   = serd_env_find(env, name_str, name->n_bytes);
	if (prefix) {
		if (!serd_node_equals(prefix->uri, uri)) {
			serd_node_free(prefix->uri);
			prefix->uri = uri;
		} else {
			serd_node_free(uri);
		}
	} else {
		env->prefixes = (SerdPrefix*)realloc(
			env->prefixes, (++env->n_prefixes) * sizeof(SerdPrefix));
		env->prefixes[env->n_prefixes - 1].name = serd_node_copy(name);
		env->prefixes[env->n_prefixes - 1].uri  = uri;
	}
}

SerdStatus
serd_env_set_prefix(SerdEnv*        env,
                    const SerdNode* name,
                    const SerdNode* uri)
{
	if (name->type != SERD_LITERAL || uri->type != SERD_URI) {
		return SERD_ERR_BAD_ARG;
	} else if (serd_uri_string_has_scheme(serd_node_string(uri))) {
		// Set prefix to absolute URI
		serd_env_add(env, name, serd_node_copy(uri));
	} else if (!env->base_uri_node) {
		return SERD_ERR_BAD_ARG;
	} else {
		// Resolve relative URI and create a new node and URI for it
		SerdNode* abs_uri =
		    serd_new_resolved_uri_i(serd_node_string(uri), &env->base_uri);

		// Set prefix to resolved (absolute) URI
		serd_env_add(env, name, abs_uri);
	}
	return SERD_SUCCESS;
}

bool
serd_env_qualify_in_place(const SerdEnv*   env,
                          const SerdNode*  uri,
                          const SerdNode** prefix,
                          SerdStringView*  suffix)
{
	if (!env) {
		return false;
	}

	for (size_t i = 0; i < env->n_prefixes; ++i) {
		const SerdNode* const prefix_uri = env->prefixes[i].uri;
		if (uri->n_bytes >= prefix_uri->n_bytes) {
			const char* prefix_str = serd_node_string(prefix_uri);
			const char* uri_str    = serd_node_string(uri);

			if (!strncmp(uri_str, prefix_str, prefix_uri->n_bytes)) {
				*prefix = env->prefixes[i].name;
				suffix->buf = uri_str + prefix_uri->n_bytes;
				suffix->len = uri->n_bytes - prefix_uri->n_bytes;
				return true;
			}
		}
	}
	return false;
}

SerdStatus
serd_env_set_prefix_from_strings(SerdEnv*    env,
                                 const char* name,
                                 const char* uri)
{
	SerdNode* name_node = serd_new_string(name);
	SerdNode* uri_node  = serd_new_uri(uri);

	const SerdStatus st = serd_env_set_prefix(env, name_node, uri_node);

	serd_node_free(name_node);
	serd_node_free(uri_node);
	return st;
}

SerdNode*
serd_env_qualify(const SerdEnv* env, const SerdNode* uri)
{
	const SerdNode* prefix = NULL;
	SerdStringView  suffix = {NULL, 0};
	if (serd_env_qualify_in_place(env, uri, &prefix, &suffix)) {
		const size_t prefix_len = serd_node_length(prefix);
		const size_t n_bytes    = prefix_len + 1 + suffix.len;
		SerdNode*    node       = serd_node_malloc(n_bytes, 0, SERD_CURIE);
		memcpy(serd_node_buffer(node),
		       serd_node_string(prefix),
		       prefix_len);
		serd_node_buffer(node)[prefix_len] = ':';
		memcpy(serd_node_buffer(node) + 1 + prefix_len, suffix.buf, suffix.len);
		node->n_bytes = n_bytes;
		return node;
	}

	return NULL;
}

SerdStatus
serd_env_expand_in_place(const SerdEnv*  env,
                         const SerdNode* curie,
                         SerdStringView* uri_prefix,
                         SerdStringView* uri_suffix)
{
	if (!env) {
		return SERD_ERR_BAD_CURIE;
	}

	const char* const str   = serd_node_string(curie);
	const char* const colon = (const char*)memchr(str, ':', curie->n_bytes + 1);
	if (curie->type != SERD_CURIE || !colon) {
		return SERD_ERR_BAD_ARG;
	}

	const size_t            name_len = (size_t)(colon - str);
	const SerdPrefix* const prefix   = serd_env_find(env, str, name_len);
	if (prefix) {
		uri_prefix->buf = serd_node_string(prefix->uri);
		uri_prefix->len = prefix->uri ? prefix->uri->n_bytes : 0;
		uri_suffix->buf = colon + 1;
		uri_suffix->len = curie->n_bytes - name_len - 1;
		return SERD_SUCCESS;
	}
	return SERD_ERR_BAD_CURIE;
}

static SerdNode*
expand_literal(const SerdEnv* env, const SerdNode* node)
{
	assert(serd_node_type(node) == SERD_LITERAL);

	const SerdNode* const datatype = serd_node_datatype(node);
	if (!datatype) {
		return serd_node_copy(node);
	}

	if (serd_node_type(datatype) == SERD_CURIE) {
		SerdStringView prefix;
		SerdStringView suffix;
		if (serd_env_expand_in_place(env, datatype, &prefix, &suffix)) {
			return NULL;
		}

		return serd_new_typed_literal_expanded(serd_node_string(node),
		                                       serd_node_length(node),
		                                       serd_node_flags(node),
		                                       SERD_URI,
		                                       prefix,
		                                       suffix);

	} else if (serd_node_type(datatype) == SERD_URI) {
		SerdURI datatype_uri;
		serd_uri_parse(serd_node_string(datatype), &datatype_uri);

		SerdURI abs_datatype_uri;
		serd_uri_resolve(&datatype_uri, &env->base_uri, &abs_datatype_uri);
		if (abs_datatype_uri.scheme.len == 0) {
			return NULL;
		}

		return serd_new_typed_literal_uri(serd_node_string(node),
		                                  serd_node_length(node),
		                                  serd_node_flags(node),
		                                  abs_datatype_uri);
	}

	return NULL;
}

static SerdNode*
expand_uri(const SerdEnv* env, const SerdNode* node)
{
	assert(serd_node_type(node) == SERD_URI);

	return serd_new_resolved_uri_i(serd_node_string(node), &env->base_uri);
}

static SerdNode*
expand_curie(const SerdEnv* env, const SerdNode* node)
{
	assert(serd_node_type(node) == SERD_CURIE);

	SerdStringView prefix;
	SerdStringView suffix;
	if (serd_env_expand_in_place(env, node, &prefix, &suffix)) {
		return NULL;
	}

	const size_t len = prefix.len + suffix.len;
	SerdNode*    ret = serd_node_malloc(len, 0, SERD_URI);
	char*        buf = serd_node_buffer(ret);
	snprintf(buf, len + 1, "%s%s", prefix.buf, suffix.buf);
	ret->n_bytes = len;
	return ret;
}

SerdNode*
serd_env_expand(const SerdEnv* env, const SerdNode* node)
{
	if (node) {
		switch (node->type) {
		case SERD_LITERAL:
			return expand_literal(env, node);
		case SERD_URI:
			return expand_uri(env, node);
		case SERD_CURIE:
			return expand_curie(env, node);
		case SERD_BLANK:
			return serd_node_copy(node);
		}
	}

	return NULL;
}

void
serd_env_write_prefixes(const SerdEnv* env, const SerdSink* sink)
{
	for (size_t i = 0; i < env->n_prefixes; ++i) {
		serd_sink_write_prefix(
			sink, env->prefixes[i].name, env->prefixes[i].uri);
	}
}
