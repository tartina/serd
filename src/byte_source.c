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

#include "byte_source.h"

#include "system.h"

#include "serd/serd.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

SerdByteSource*
serd_byte_source_new_string(const char* string, const SerdNode* name)
{
	SerdByteSource* source = (SerdByteSource*)calloc(1, sizeof(SerdByteSource));

	source->page_size = 1;
	source->name      = name ? serd_node_copy(name) : serd_new_string("string");
	source->read_buf  = (const uint8_t*)string;
	source->type      = FROM_STRING;

	const SerdCursor cur = {source->name, 1, 1};
	source->cur          = cur;

	return source;
}

SerdByteSource*
serd_byte_source_new_filename(const char* path, size_t block_size)
{
	if (!path || !block_size) {
		return NULL;
	}

	FILE* fd = fopen(path, "rb");
	if (!fd) {
		return NULL;
	}

	SerdByteSource* source = (SerdByteSource*)calloc(1, sizeof(SerdByteSource));

	source->read_func   = (SerdReadFunc)fread;
	source->error_func  = (SerdStreamErrorFunc)ferror;
	source->close_func  = (SerdStreamCloseFunc)fclose;
	source->stream      = fd;
	source->page_size   = block_size;
	source->buf_size    = block_size;
	source->name        = serd_new_file_uri(path, NULL);
	source->type        = FROM_FILENAME;

	const SerdCursor cur = { source->name, 1, 1 };
	source->cur = cur;

	if (block_size > 1) {
		source->file_buf = (uint8_t*)serd_allocate_buffer(block_size);
		source->read_buf = source->file_buf;
		memset(source->file_buf, '\0', block_size);
	} else {
		source->read_buf = &source->read_byte;
	}

	return source;
}

SerdByteSource*
serd_byte_source_new_function(SerdReadFunc        read_func,
                              SerdStreamErrorFunc error_func,
                              void*               stream,
                              const SerdNode*     name,
                              size_t              block_size)
{
	if (!read_func || !block_size) {
		return NULL;
	}

	SerdByteSource* source = (SerdByteSource*)calloc(1, sizeof(SerdByteSource));

	source->read_func   = read_func;
	source->error_func  = error_func;
	// source->close_func  = close_func; FIXME
	source->stream      = stream;
	source->page_size   = block_size;
	source->buf_size    = block_size;
	source->name        = name ? serd_node_copy(name) : serd_new_string("func");
	source->type        = FROM_FUNCTION;

	const SerdCursor cur = { source->name, 1, 1 };
	source->cur = cur;

	if (block_size > 1) {
		source->file_buf = (uint8_t*)serd_allocate_buffer(block_size);
		source->read_buf = source->file_buf;
		memset(source->file_buf, '\0', block_size);
	} else {
		source->read_buf = &source->read_byte;
	}

	return source;
}

void
serd_byte_source_free(SerdByteSource* source)
{
	if (source) {
		if (source->close_func) {
			source->close_func(source->stream);
		}
		if (source->page_size > 1) {
			free(source->file_buf);
		}
		serd_node_free(source->name);
		free(source);
	}
}

SerdStatus
serd_byte_source_page(SerdByteSource* source)
{
	uint8_t* const buf = (source->page_size > 1
	                      ? source->file_buf
	                      : &source->read_byte);

	const size_t n_read = source->read_func(
		buf, 1, source->page_size, source->stream);

	source->buf_size  = n_read;
	source->read_head = 0;
	source->eof       = false;

	if (n_read < source->page_size) {
		buf[n_read] = '\0';
		if (n_read == 0) {
			source->eof = true;
			return (source->error_func(source->stream)
			        ? SERD_ERR_UNKNOWN : SERD_FAILURE);
		}
	}

	return SERD_SUCCESS;
}

SerdStatus
serd_byte_source_prepare(SerdByteSource* source)
{
	source->prepared = true;
	if (source->type != FROM_STRING) {
		if (source->page_size > 1) {
			return serd_byte_source_page(source);
		} else {
			return serd_byte_source_advance(source);
		}
	}
	return SERD_SUCCESS;
}
