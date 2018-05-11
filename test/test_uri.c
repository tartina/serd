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
#include <stdio.h>
#include <string.h>

static void
test_file_uri(const char* hostname,
              const char* path,
              const char* expected_uri,
              const char* expected_path)
{
	if (!expected_path) {
		expected_path = path;
	}

	SerdNode*   node         = serd_new_file_uri(path, hostname);
	const char* node_str     = serd_node_string(node);
	char*       out_hostname = NULL;
	char*       out_path     = serd_file_uri_parse(node_str, &out_hostname);
	assert(!strcmp(node_str, expected_uri));
	assert((hostname && out_hostname) || (!hostname && !out_hostname));
	assert(!strcmp(out_path, expected_path));

	serd_free(out_path);
	serd_free(out_hostname);
	serd_node_free(node);
}

static void
test_uri_parsing(void)
{
	test_file_uri(NULL, "C:/My 100%",
	              "file:///C:/My%20100%%", NULL);
	test_file_uri("ahost", "C:\\Pointless Space",
	              "file://ahost/C:/Pointless%20Space", "C:/Pointless Space");
	test_file_uri(NULL, "/foo/bar",
	              "file:///foo/bar", NULL);
	test_file_uri("bhost", "/foo/bar",
	              "file://bhost/foo/bar", NULL);
	test_file_uri(NULL, "a/relative <path>",
	              "a/relative%20%3Cpath%3E", NULL);

	// Test tolerance of parsing junk URI escapes

	char* out_path = serd_file_uri_parse("file:///foo/%0Xbar", NULL);
	assert(!strcmp(out_path, "/foo/bar"));
	serd_free(out_path);
}

static void
test_uri_from_string(void)
{
	SerdNode* base      = serd_new_uri("http://example.org/a/b/c/");
	SerdNode* not_a_uri = serd_new_string("hello");
	SerdNode* nil       = serd_new_resolved_uri("", base);
	assert(!serd_new_resolved_uri("", NULL));
	assert(!serd_new_resolved_uri("", not_a_uri));
	assert(serd_node_type(nil) == SERD_URI);
	assert(!strcmp(serd_node_string(nil), serd_node_string(base)));
	serd_node_free(nil);
	serd_node_free(not_a_uri);
	serd_node_free(base);
}

static void
check_rel_uri(const char*     uri,
              const SerdNode* base,
              const SerdNode* root,
              const char*     expected)
{
	SerdNode* rel = serd_new_relative_uri(uri, base, root);
	const int ret = strcmp(serd_node_string(rel), expected);
	serd_node_free(rel);
	assert(!ret);
}

static void
test_relative_uri(void)
{
	SerdNode* root = serd_new_uri("http://example.org/a/b/");
	SerdNode* base = serd_new_uri("http://example.org/a/b/c/");

	check_rel_uri("http://example.org/a/b/c/foo", base, NULL, "foo");
	check_rel_uri("http://example.org/a/", base, NULL, "../../");
	check_rel_uri("http://example.org/a/", base, root, "http://example.org/a/");
	check_rel_uri("http://example.org/", base, NULL, "../../../");
	check_rel_uri("http://drobilla.net/a", base, NULL, "http://drobilla.net/a");

	serd_node_free(base);
	serd_node_free(root);
}

static void
test_uri_resolution(void)
{
	SerdNode* base      = serd_new_uri("http://example.org/a/b/c/");
	SerdNode* nil       = serd_new_resolved_uri("", base);
	SerdNode* not_a_uri = serd_new_string("hello");
	SerdNode* root      = serd_new_uri("http://example.org/a/b/");

	assert(!serd_node_resolve(nil, NULL));
	assert(!serd_node_resolve(not_a_uri, base));
	assert(!serd_node_resolve(nil, not_a_uri));

	SerdNode* rel =
	    serd_new_relative_uri("http://example.org/a/b/c/foo", base, NULL);
	SerdNode* resolved = serd_node_resolve(rel, base);
	assert(!strcmp(serd_node_string(resolved), "http://example.org/a/b/c/foo"));

	serd_node_free(nil);
	serd_node_free(not_a_uri);
	serd_node_free(resolved);
	serd_node_free(rel);
	serd_node_free(base);
	serd_node_free(root);
}

int
main(void)
{
	test_uri_parsing();
	test_uri_from_string();
	test_relative_uri();
	test_uri_resolution();

	printf("Success\n");
	return 0;
}
