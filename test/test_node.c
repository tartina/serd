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
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define NS_XSD "http://www.w3.org/2001/XMLSchema#"

static void
test_integer_to_node(void)
{
	const long int_test_nums[] = {0, -0, -23, 23, -12340, 1000, -1000};

	const char* int_test_strs[] = {
	    "0", "0", "-23", "23", "-12340", "1000", "-1000"};

	for (size_t i = 0; i < sizeof(int_test_nums) / sizeof(double); ++i) {
		SerdNode*   node     = serd_new_integer(int_test_nums[i], NULL);
		const char* node_str = serd_node_string(node);
		assert(!strcmp(node_str, int_test_strs[i]));
		const size_t len = strlen(node_str);
		assert(serd_node_length(node) == len);
		assert(!strcmp(serd_node_string(serd_node_datatype(node)),
		               NS_XSD "integer"));
		serd_node_free(node);
	}
}

static void
test_boolean(void)
{
	SerdNode* true_node = serd_new_boolean(true);
	assert(!strcmp(serd_node_string(true_node), "true"));
	assert(!strcmp(serd_node_string(serd_node_datatype(true_node)),
	               NS_XSD "boolean"));
	serd_node_free(true_node);

	SerdNode* false_node = serd_new_boolean(false);
	assert(!strcmp(serd_node_string(false_node), "false"));
	assert(!strcmp(serd_node_string(serd_node_datatype(false_node)),
	               NS_XSD "boolean"));
	serd_node_free(false_node);
}

static void
test_blob_to_node(void)
{
	for (size_t size = 1; size < 256; ++size) {
		uint8_t* const data = (uint8_t*)malloc(size);
		for (size_t i = 0; i < size; ++i) {
			data[i] = (uint8_t)((size + i) % 256);
		}

		size_t       out_size = 0;
		SerdNode*    blob     = serd_new_blob(data, size, size % 5, NULL);
		const char*  blob_str = serd_node_string(blob);
		const size_t len      = serd_node_length(blob);

		uint8_t* out = (uint8_t*)malloc(serd_base64_decoded_size(len));
		assert(!serd_base64_decode(out, &out_size, blob_str, len));
		assert(serd_node_length(blob) == strlen(blob_str));
		assert(out_size == size);

		for (size_t i = 0; i < size; ++i) {
			assert(out[i] == data[i]);
		}

		assert(!strcmp(serd_node_string(serd_node_datatype(blob)),
		               NS_XSD "base64Binary"));

		serd_node_free(blob);
		free(out);
		free(data);
	}
}

static void
test_node_equals(void)
{
	static const uint8_t replacement_char_str[] = { 0xEF, 0xBF, 0xBD, 0 };

	SerdNode* lhs = serd_new_string((const char*)replacement_char_str);
	SerdNode* rhs = serd_new_string("123");
	assert(!serd_node_equals(lhs, rhs));

	SerdNode* qnode = serd_new_curie("foo:bar");
	assert(!serd_node_equals(lhs, qnode));
	serd_node_free(qnode);

	assert(!serd_node_copy(NULL));

	serd_node_free(lhs);
	serd_node_free(rhs);
}

static void
test_node_from_string(void)
{
	SerdNode* hello = serd_new_string("hello\"");
	assert(serd_node_length(hello) == 6);
	assert(serd_node_flags(hello) == SERD_HAS_QUOTE);
	assert(!strncmp(serd_node_string(hello), "hello\"", 6));
	serd_node_free(hello);
}

static void
test_node_from_substring(void)
{
	SerdNode* a_b = serd_new_substring("a\"bc", 3);
	assert(serd_node_length(a_b) == 3);
	assert(serd_node_flags(a_b) == SERD_HAS_QUOTE);
	assert(strlen(serd_node_string(a_b)) == 3);
	assert(!strncmp(serd_node_string(a_b), "a\"b", 3));

	serd_node_free(a_b);
	a_b = serd_new_substring("a\"bc", 10);
	assert(serd_node_length(a_b) == 4);
	assert(serd_node_flags(a_b) == SERD_HAS_QUOTE);
	assert(strlen(serd_node_string(a_b)) == 4);
	assert(!strncmp(serd_node_string(a_b), "a\"bc", 4));
	serd_node_free(a_b);
}

static void
test_simple_node(void)
{
	assert(!serd_new_simple_node(SERD_LITERAL, "Literal", 7));
}

static void
test_literal(void)
{
	SerdNode* hello2 = serd_new_string("hello\"");
	assert(!serd_new_typed_literal("bad type", hello2));

	assert(serd_node_length(hello2) == 6 &&
	       serd_node_flags(hello2) == SERD_HAS_QUOTE &&
	       !strcmp(serd_node_string(hello2), "hello\""));

	SerdNode* hello3 = serd_new_plain_literal("hello\"", NULL);
	assert(serd_node_equals(hello2, hello3));

	SerdNode* hello4 = serd_new_typed_literal("hello\"", NULL);
	assert(serd_node_equals(hello4, hello2));

	serd_node_free(hello4);
	serd_node_free(hello3);
	serd_node_free(hello2);

	const char* lang_lit_str = "\"Hello\"@en";
	SerdNode*   sliced_lang_lit =
	    serd_new_literal(lang_lit_str + 1, 5, NULL, 0, lang_lit_str + 8, 2);
	assert(!strcmp(serd_node_string(sliced_lang_lit), "Hello"));
	assert(
	    !strcmp(serd_node_string(serd_node_language(sliced_lang_lit)), "en"));
	serd_node_free(sliced_lang_lit);

	const char* type_lit_str = "\"Hallo\"^^<http://example.org/Greeting>";
	SerdNode*   sliced_type_lit =
	    serd_new_literal(type_lit_str + 1, 5, type_lit_str + 10, 27, NULL, 0);
	assert(!strcmp(serd_node_string(sliced_type_lit), "Hallo"));
	assert(!strcmp(serd_node_string(serd_node_datatype(sliced_type_lit)),
	               "http://example.org/Greeting"));
	serd_node_free(sliced_type_lit);

	SerdNode* plain_lit = serd_new_literal("Plain", 5, NULL, 0, NULL, 0);
	assert(!strcmp(serd_node_string(plain_lit), "Plain"));
	serd_node_free(plain_lit);
}

static void
test_blank(void)
{
	SerdNode* blank = serd_new_blank("b0");
	assert(serd_node_length(blank) == 2);
	assert(serd_node_flags(blank) == 0);
	assert(!strcmp(serd_node_string(blank), "b0"));
	serd_node_free(blank);
}

int
main(void)
{
	test_integer_to_node();
	test_blob_to_node();
	test_boolean();
	test_node_equals();
	test_node_from_string();
	test_node_from_substring();
	test_simple_node();
	test_literal();
	test_blank();

	printf("Success\n");
	return 0;
}
