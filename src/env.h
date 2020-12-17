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

#ifndef SERD_ENV_H
#define SERD_ENV_H

#include "serd/serd.h"

#include <stdbool.h>

/**
   Qualify `uri` into a CURIE if possible.
*/
bool
serd_env_qualify_in_place(const SerdEnv*   env,
                          const SerdNode*  uri,
                          const SerdNode** prefix,
                          SerdStringView*  suffix);

/**
   Expand `curie`.

   Errors: SERD_ERR_BAD_ARG if `curie` is not valid, or SERD_ERR_BAD_CURIE if
   prefix is not defined in `env`.
*/
SerdStatus
serd_env_expand_in_place(const SerdEnv*  env,
                         const SerdNode* curie,
                         SerdStringView* uri_prefix,
                         SerdStringView* uri_suffix);

SERD_CONST_FUNC
const SerdURI*
serd_env_get_parsed_base_uri(const SerdEnv* env);

#endif // SERD_ENV_H
