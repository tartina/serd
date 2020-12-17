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

#include "../src/decimal.h"

#include "serd/serd.h"

#include <assert.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

static void
test_count_digits(void)
{
	assert(1 == serd_count_digits(0));
	assert(1 == serd_count_digits(1));
	assert(1 == serd_count_digits(9));
	assert(2 == serd_count_digits(10));
	assert(2 == serd_count_digits(99ull));
	assert(3 == serd_count_digits(999ull));
	assert(4 == serd_count_digits(9999ull));
	assert(5 == serd_count_digits(99999ull));
	assert(6 == serd_count_digits(999999ull));
	assert(7 == serd_count_digits(9999999ull));
	assert(8 == serd_count_digits(99999999ull));
	assert(9 == serd_count_digits(999999999ull));
	assert(10 == serd_count_digits(9999999999ull));
	assert(11 == serd_count_digits(99999999999ull));
	assert(12 == serd_count_digits(999999999999ull));
	assert(13 == serd_count_digits(9999999999999ull));
	assert(14 == serd_count_digits(99999999999999ull));
	assert(15 == serd_count_digits(999999999999999ull));
	assert(16 == serd_count_digits(9999999999999999ull));
	assert(17 == serd_count_digits(99999999999999999ull));
	assert(18 == serd_count_digits(999999999999999999ull));
	assert(19 == serd_count_digits(9999999999999999999ull));
	assert(20 == serd_count_digits(18446744073709551615ull));
}

static void
check_precision(const double   d,
                const unsigned precision,
                const unsigned frac_digits,
                const char*    expected)
{
	SerdNode* const node = serd_new_decimal(d, precision, frac_digits, NULL);
	const char*     str  = serd_node_string(node);

	if (strcmp(str, expected)) {
		fprintf(stderr, "error: string is \"%s\"\n", str);
		fprintf(stderr, "note:  expected  \"%s\"\n", expected);
		assert(false);
	}

	serd_node_free(node);
}

static void
test_precision(void)
{
	assert(serd_new_decimal((double)INFINITY, 17, 0, NULL) == NULL);
	assert(serd_new_decimal((double)-INFINITY, 17, 0, NULL) == NULL);
	assert(serd_new_decimal((double)NAN, 17, 0, NULL) == NULL);

	check_precision(1.0000000001, 17, 8, "1.0");
	check_precision(0.0000000001, 17, 10, "0.0000000001");
	check_precision(0.0000000001, 17, 8, "0.0");

	check_precision(12345.678900, 9, 5, "12345.6789");
	check_precision(12345.678900, 8, 5, "12345.678");
	check_precision(12345.678900, 5, 5, "12345.0");
	check_precision(12345.678900, 3, 5, "12300.0");

	check_precision(12345.678900, 9, 0, "12345.6789");
	check_precision(12345.678900, 9, 5, "12345.6789");
	check_precision(12345.678900, 9, 3, "12345.678");
	check_precision(12345.678900, 9, 1, "12345.6");
}

int
main(void)
{
	test_count_digits();
	test_precision();
	return 0;
}
