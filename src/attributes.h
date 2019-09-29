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

#ifndef SERD_ATTRIBUTES_H
#define SERD_ATTRIBUTES_H

#ifdef __GNUC__
#    define SERD_I_PURE_FUNC __attribute__((pure))
#    define SERD_I_CONST_FUNC __attribute__((const))
#    define SERD_I_MALLOC_FUNC __attribute__((malloc))
#else
#    define SERD_I_PURE_FUNC
#    define SERD_I_CONST_FUNC
#    define SERD_I_MALLOC_FUNC
#endif

#endif // SERD_ATTRIBUTES_H
