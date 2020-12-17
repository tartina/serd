/*
  Copyright 2019 David Robillard <http://drobilla.net>

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

#include "../src/attributes.h"
#include "../src/ieee_float.h"
#include "../src/soft_float.h"

#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <string.h>

/// Linear Congruential Generator for making random floats
static inline uint32_t
lcg32(const uint32_t i)
{
	static const uint32_t a = 134775813u;
	static const uint32_t c = 1u;

	return (a * i) + c;
}

/// Linear Congruential Generator for making random doubles
static inline uint64_t
lcg64(const uint64_t i)
{
	static const uint64_t a = 6364136223846793005ull;
	static const uint64_t c = 1ull;

	return (a * i) + c;
}

/// Return the float with representation `rep`
static inline float
float_from_rep(const uint32_t rep)
{
	float f = 0.0f;
	memcpy(&f, &rep, sizeof(f));
	return f;
}

/// Return the double with representation `rep`
static inline double
double_from_rep(const uint64_t rep)
{
	double d = 0.0;
	memcpy(&d, &rep, sizeof(d));
	return d;
}

/// Return the distance between two doubles in ULPs
static SERD_I_PURE_FUNC uint64_t
ulp_distance(const double a, const double b)
{
	assert(a >= 0.0);
	assert(b >= 0.0);

	if (a == b) {
		return 0;
	}

	if (isnan(a) || isnan(b) || isinf(a) || isinf(b)) {
		return UINT64_MAX;
	}

	const uint64_t ia = double_to_rep(a);
	const uint64_t ib = double_to_rep(b);

	return ia > ib ? ia - ib : ib - ia;
}
