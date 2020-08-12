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

/// @file serd.h API for Serd, a lightweight RDF syntax library

#ifndef SERD_SERD_H
#define SERD_SERD_H

#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#if defined(SERD_SHARED) && defined(SERD_INTERNAL) && defined(_WIN32)
#    define SERD_API __declspec(dllexport)
#elif defined(SERD_SHARED) && defined(_WIN32)
#    define SERD_API __declspec(dllimport)
#elif defined(SERD_SHARED) && defined(__GNUC__)
#    define SERD_API __attribute__((visibility("default")))
#else
#    define SERD_API
#endif

#ifdef __GNUC__
#    define SERD_PURE_FUNC __attribute__((pure))
#    define SERD_CONST_FUNC __attribute__((const))
#    define SERD_MALLOC_FUNC __attribute__((malloc))
#else
#    define SERD_PURE_FUNC
#    define SERD_CONST_FUNC
#    define SERD_MALLOC_FUNC
#endif

#ifdef __clang__
#    define SERD_NONNULL _Nonnull
#    define SERD_NULLABLE _Nullable
#    define SERD_ALLOCATED _Null_unspecified
#else
#    define SERD_NONNULL
#    define SERD_NULLABLE
#    define SERD_ALLOCATED
#endif

#define SERD_PURE_API SERD_API SERD_PURE_FUNC
#define SERD_CONST_API SERD_API SERD_CONST_FUNC
#define SERD_MALLOC_API SERD_API SERD_MALLOC_FUNC

#if defined(__GNUC__)
#    define SERD_LOG_FUNC(fmt, arg1) __attribute__((format(printf, fmt, arg1)))
#else
#    define SERD_LOG_FUNC(fmt, arg1)
#endif

#ifdef __cplusplus
extern "C" {
#    if defined(__GNUC__)
#        pragma GCC diagnostic push
#        pragma GCC diagnostic ignored "-Wzero-as-null-pointer-constant"
#    endif
#endif

/**
   @defgroup serd Serd
   A lightweight RDF syntax library.
   @{
*/

/// Global library state
typedef struct SerdWorldImpl SerdWorld;

/// Hashing node container for interning and simplified memory management
typedef struct SerdNodesImpl SerdNodes;

/// A subject, predicate, and object, with optional graph context
typedef struct SerdStatementImpl SerdStatement;

/// The origin of a statement in a document
typedef struct SerdCursorImpl SerdCursor;

/// Lexical environment for relative URIs or CURIEs (base URI and namespaces)
typedef struct SerdEnvImpl SerdEnv;

/// An indexed set of statements
typedef struct SerdModelImpl SerdModel;

/// An iterator that points to a statement in a model
typedef struct SerdIterImpl SerdIter;

/// A range of statements in a model
typedef struct SerdRangeImpl SerdRange;

/// Streaming parser that reads a text stream and writes to a sink
typedef struct SerdReaderImpl SerdReader;

/// Streaming serialiser that writes a text stream as statements are pushed
typedef struct SerdWriterImpl SerdWriter;

/// An interface that receives a stream of RDF data
typedef struct SerdSinkImpl SerdSink;

/// A sink for bytes that receives string output
typedef struct SerdByteSinkImpl SerdByteSink;

/// Return status code
typedef enum {
	SERD_SUCCESS,        ///< No error
	SERD_FAILURE,        ///< Non-fatal failure
	SERD_ERR_UNKNOWN,    ///< Unknown error
	SERD_ERR_BAD_SYNTAX, ///< Invalid syntax
	SERD_ERR_BAD_ARG,    ///< Invalid argument
	SERD_ERR_BAD_ITER,   ///< Use of invalidated iterator
	SERD_ERR_NOT_FOUND,  ///< Not found
	SERD_ERR_ID_CLASH,   ///< Encountered clashing blank node IDs
	SERD_ERR_BAD_CURIE,  ///< Invalid CURIE (e.g. prefix does not exist)
	SERD_ERR_INTERNAL,   ///< Unexpected internal error (should not happen)
	SERD_ERR_OVERFLOW,   ///< Stack overflow
	SERD_ERR_INVALID,    ///< Invalid data
	SERD_ERR_NO_DATA,    ///< Unexpected end of input
	SERD_ERR_BAD_WRITE   ///< Error writing to file/stream
} SerdStatus;

/// RDF syntax type
typedef enum {
	SERD_SYNTAX_EMPTY = 0, ///< Empty syntax (suppress input or output)
	SERD_TURTLE       = 1, ///< Terse triples http://www.w3.org/TR/turtle
	SERD_NTRIPLES     = 2, ///< Flat triples http://www.w3.org/TR/n-triples/
	SERD_NQUADS       = 3, ///< Flat quads http://www.w3.org/TR/n-quads/
	SERD_TRIG         = 4  ///< Terse quads http://www.w3.org/TR/trig/
} SerdSyntax;

/// Flags indicating inline abbreviation information for a statement
typedef enum {
	SERD_EMPTY_S = 1u << 0u, ///< Empty blank node subject
	SERD_ANON_S  = 1u << 1u, ///< Start of anonymous subject
	SERD_ANON_O  = 1u << 2u, ///< Start of anonymous object
	SERD_LIST_S  = 1u << 3u, ///< Start of list subject
	SERD_LIST_O  = 1u << 4u, ///< Start of list object
	SERD_TERSE_S = 1u << 5u, ///< Terse serialisation of new subject
	SERD_TERSE_O = 1u << 6u  ///< Terse serialisation of new object
} SerdStatementFlag;

/// Bitwise OR of SerdStatementFlag values
typedef uint32_t SerdStatementFlags;

/// Flags that control the style of a model serialisation
typedef enum {
	SERD_NO_INLINE_OBJECTS = 1u << 0u ///< Disable object inlining
} SerdSerialisationFlag;

/// Bitwise OR of SerdStatementFlag values
typedef uint32_t SerdSerialisationFlags;

/**
   Type of a node.

   An RDF node, in the abstract sense, can be either a resource, literal, or a
   blank.  This type is more precise, because syntactically there are two ways
   to refer to a resource (by URI or CURIE).

   There are also two ways to refer to a blank node in syntax (by ID or
   anonymously), but this is handled by statement flags rather than distinct
   node types.
*/
typedef enum {
	/**
	   Literal value.

	   A literal optionally has either a language, or a datatype (not both).
	*/
	SERD_LITERAL = 1,

	/**
	   URI (absolute or relative).

	   Value is an unquoted URI string, which is either a relative reference
	   with respect to the current base URI (e.g. "foo/bar"), or an absolute
	   URI (e.g. "http://example.org/foo").
	   @see [RFC3986](http://tools.ietf.org/html/rfc3986)
	*/
	SERD_URI = 2,

	/**
	   CURIE, a shortened URI.

	   Value is an unquoted CURIE string relative to the current environment,
	   e.g. "rdf:type".  @see [CURIE Syntax 1.0](http://www.w3.org/TR/curie)
	*/
	SERD_CURIE = 3,

	/**
	   A blank node.

	   Value is a blank node ID without any syntactic prefix, like "id3", which
	   is meaningful only within this serialisation.  @see [RDF 1.1
	   Turtle](http://www.w3.org/TR/turtle/#grammar-production-BLANK_NODE_LABEL)
	*/
	SERD_BLANK = 4
} SerdNodeType;

/// Flags indicating certain string properties relevant to serialisation
typedef enum {
	SERD_HAS_NEWLINE  = 1u << 0u, ///< Contains line breaks ('\\n' or '\\r')
	SERD_HAS_QUOTE    = 1u << 1u, ///< Contains quotes ('"')
	SERD_HAS_DATATYPE = 1u << 2u, ///< Literal node has datatype
	SERD_HAS_LANGUAGE = 1u << 3u  ///< Literal node has language
} SerdNodeFlag;

/// Bitwise OR of SerdNodeFlag values
typedef uint32_t SerdNodeFlags;

/// Index of a node in a statement
typedef enum {
	SERD_SUBJECT   = 0, ///< Subject
	SERD_PREDICATE = 1, ///< Predicate ("key")
	SERD_OBJECT    = 2, ///< Object    ("value")
	SERD_GRAPH     = 3  ///< Graph     ("context")
} SerdField;

/// Flags that control model storage and indexing
typedef enum {
	SERD_INDEX_SPO     = 1u << 0u, ///< Subject,   Predicate, Object
	SERD_INDEX_SOP     = 1u << 1u, ///< Subject,   Object,    Predicate
	SERD_INDEX_OPS     = 1u << 2u, ///< Object,    Predicate, Subject
	SERD_INDEX_OSP     = 1u << 3u, ///< Object,    Subject,   Predicate
	SERD_INDEX_PSO     = 1u << 4u, ///< Predicate, Subject,   Object
	SERD_INDEX_POS     = 1u << 5u, ///< Predicate, Object,    Subject
	SERD_INDEX_GRAPHS  = 1u << 6u, ///< Support multiple graphs in model
	SERD_STORE_CURSORS = 1u << 7u  ///< Store original cursor of statements
} SerdModelFlag;

/// Bitwise OR of SerdModelFlag values
typedef uint32_t SerdModelFlags;

/// An RDF node
typedef struct SerdNodeImpl SerdNode;

/// An unterminated immutable slice of a string
typedef struct {
	const char* SERD_NULLABLE buf; ///< Start of chunk
	size_t                    len; ///< Length of chunk in bytes
} SerdStringView;

/// A mutable buffer in memory
typedef struct {
	void* SERD_NULLABLE buf; ///< Buffer
	size_t              len; ///< Size of buffer in bytes
} SerdBuffer;

/**
   A parsed URI

   This struct directly refers to slices in other strings, it does not own any
   memory itself.  Thus, URIs can be parsed and/or resolved against a base URI
   in-place without allocating memory.
*/
typedef struct {
	SerdStringView scheme;    ///< Scheme
	SerdStringView authority; ///< Authority
	SerdStringView path_base; ///< Path prefix if relative
	SerdStringView path;      ///< Path suffix
	SerdStringView query;     ///< Query
	SerdStringView fragment;  ///< Fragment
} SerdURI;

/// Reader options
typedef enum {
	SERD_READ_LAX = 1u << 0u  ///< Tolerate invalid input where possible
} SerdReaderFlag;

/// Bitwise OR of SerdReaderFlag values
typedef uint32_t SerdReaderFlags;

/**
   Writer style options.

   These flags allow more precise control of writer output style.  Note that
   some options are only supported for some syntaxes, for example, NTriples
   does not support abbreviation and is always ASCII.
*/
typedef enum {
	SERD_WRITE_ASCII       = 1u << 0u, ///< Escape all non-ASCII characters
	SERD_WRITE_UNQUALIFIED = 1u << 1u, ///< Do not shorten URIs into CURIEs
	SERD_WRITE_UNRESOLVED  = 1u << 2u, ///< Do not make URIs relative
	SERD_WRITE_TERSE       = 1u << 3u, ///< Write terser output without newlines
	SERD_WRITE_LAX         = 1u << 4u  ///< Tolerate lossy output
} SerdWriterFlag;

/// Bitwise OR of SerdWriterFlag values
typedef uint32_t SerdWriterFlags;

/**
   Free memory allocated by Serd.

   This function exists because some systems require memory allocated by a
   library to be freed by code in the same library.  It is otherwise equivalent
   to the standard C free() function.
*/
SERD_API
void
serd_free(void* SERD_NULLABLE ptr);

/**
   @name String Utilities
   @{
*/

/// Return a string describing a status code
SERD_CONST_API
const char* SERD_NONNULL
serd_strerror(SerdStatus status);

/**
   Measure a UTF-8 string.

   @return Length of `str` in bytes.
   @param str A null-terminated UTF-8 string.
   @param flags (Output) Set to the applicable flags.
*/
SERD_API
size_t
serd_strlen(const char* SERD_NONNULL     str,
            SerdNodeFlags* SERD_NULLABLE flags);

/**
   Parse a string to a double.

   The API of this function is similar to the standard C strtod function,
   except this function is locale-independent and always matches the lexical
   format used in the Turtle grammar (the decimal point is always ".").  The
   end parameter is an offset from the start of `str` to avoid the
   const-correctness issues of the strtod API.
*/
SERD_API
double
serd_strtod(const char* SERD_NONNULL str, size_t* SERD_NULLABLE end);

/**
   @}
   @name Base64
   @{
*/

/**
   Return the number of bytes required to encode `size` bytes in base64.

   @param size The number of input (binary) bytes to encode.
   @param wrap_lines Wrap lines at 76 characters to conform to RFC 2045.
   @return The length of the base64 encoding, excluding null terminator.
*/
SERD_CONST_API
size_t
serd_base64_encoded_length(size_t size, bool wrap_lines);

/**
   Return the maximum number of bytes required to decode `size` bytes of base64.

   @param len The number of input (text) bytes to decode.
   @return The required buffer size to decode `size` bytes of base64.
*/
SERD_CONST_API
size_t
serd_base64_decoded_size(size_t len);

/**
   Encode `size` bytes of `buf` into `str`, which must be large enough.

   @param str Output buffer of at least serd_base64_encoded_length(size) bytes.
   @param buf Input binary data.
   @param size Number of bytes to encode from `buf`.
   @param wrap_lines Wrap lines at 76 characters to conform to RFC 2045.
   @return True iff `str` contains newlines.
*/
SERD_API
bool
serd_base64_encode(char* SERD_NONNULL       str,
                   const void* SERD_NONNULL buf,
                   size_t                   size,
                   bool                     wrap_lines);

/**
   Decode a base64 string.

   This function can be used to deserialise a blob node created with
   serd_new_blob().

   @param buf Output buffer of at least serd_base64_decoded_size(size) bytes.
   @param size Set to the size of the decoded data in bytes.
   @param str Base64 string to decode.
   @param len The length of `str`.
*/
SERD_API
SerdStatus
serd_base64_decode(void* SERD_NONNULL       buf,
                   size_t* SERD_NONNULL     size,
                   const char* SERD_NONNULL str,
                   size_t                   len);

/**
   @}
   @name Byte Streams
   @{
*/

/**
   Function to detect I/O stream errors.

   Identical semantics to `ferror`.

   @return Non-zero if `stream` has encountered an error.
*/
typedef int (*SerdStreamErrorFunc)(void* SERD_NONNULL stream);

/**
   Source function for raw string input.

   Identical semantics to `fread`, but may set errno for more informative error
   reporting than supported by SerdStreamErrorFunc.

   @param buf Output buffer.
   @param size Size of a single element of data in bytes (always 1).
   @param nmemb Number of elements to read.
   @param stream Stream to read from (FILE* for fread).
   @return Number of elements (bytes) read, which is short on error.
*/
typedef size_t (*SerdReadFunc)(void* SERD_NONNULL buf,
                               size_t             size,
                               size_t             nmemb,
                               void* SERD_NONNULL stream);

/**
   Sink function for raw string output.

   Identical semantics to `fwrite`, but may set errno for more informative
   error reporting than supported by SerdStreamErrorFunc.

   @param buf Input buffer.
   @param size Size of a single element of data in bytes (always 1).
   @param nmemb Number of elements to read.
   @param stream Stream to write to (FILE* for fread).
   @return Number of elements (bytes) written, which is short on error.
*/
typedef size_t (*SerdWriteFunc)(const void* SERD_NONNULL buf,
                                size_t                   size,
                                size_t                   nmemb,
                                void* SERD_NONNULL       stream);

/**
   Create a new byte sink.

   @param write_func Function called with bytes to consume.
   @param stream Context parameter passed to `sink`.
   @param block_size Number of bytes to write per call.
*/
SERD_API
SerdByteSink* SERD_ALLOCATED
serd_byte_sink_new(SerdWriteFunc SERD_NONNULL write_func,
                   void* SERD_NULLABLE        stream,
                   size_t                     block_size);

/**
   Write to `sink`.

   Compatible with SerdWriteFunc.
*/
SERD_API
size_t
serd_byte_sink_write(const void* SERD_NONNULL   buf,
                     size_t                     size,
                     size_t                     nmemb,
                     SerdByteSink* SERD_NONNULL sink);

/// Flush any pending output in `sink` to the underlying write function
SERD_API
void
serd_byte_sink_flush(SerdByteSink* SERD_NONNULL sink);

/// Free `sink`
SERD_API
void
serd_byte_sink_free(SerdByteSink* SERD_NULLABLE sink);

/**
   @}
   @name Syntax Utilities
   @{
*/

/**
   Get a syntax by name.

   Case-insensitive, supports "Turtle", "NTriples", "NQuads", and "TriG".
   `SERD_SYNTAX_EMPTY` is returned if the name is not recognized.
*/
SERD_PURE_API
SerdSyntax
serd_syntax_by_name(const char* SERD_NONNULL name);

/**
   Guess a syntax from a filename.

   This uses the file extension to guess the syntax of a file.
   `SERD_SYNTAX_EMPTY` is returned if the extension is not recognized.
*/
SERD_PURE_API
SerdSyntax
serd_guess_syntax(const char* SERD_NONNULL filename);

/**
   Return whether a syntax can represent multiple graphs.

   @return True for SERD_NQUADS and SERD_TRIG, false otherwise.
*/
SERD_CONST_API
bool
serd_syntax_has_graphs(SerdSyntax syntax);

/**
   @}
   @name URI
   @{
*/

static const SerdURI SERD_URI_NULL = {
	{NULL, 0}, {NULL, 0}, {NULL, 0}, {NULL, 0}, {NULL, 0}, {NULL, 0}
};

/**
   Get the unescaped path and hostname from a file URI.

   The returned path and `*hostname` must be freed with serd_free().

   @param uri A file URI.
   @param hostname If non-NULL, set to the hostname, if present.
   @return The path component of the URI.
*/
SERD_API
char* SERD_NULLABLE
serd_file_uri_parse(const char* SERD_NONNULL uri,
                    char* SERD_NONNULL* SERD_NULLABLE hostname);

/// Return true iff `utf8` starts with a valid URI scheme
SERD_PURE_API
bool
serd_uri_string_has_scheme(const char* SERD_NULLABLE utf8);

/// Parse `utf8`, writing result to `out`
SERD_API
SerdStatus
serd_uri_parse(const char* SERD_NONNULL utf8, SerdURI* SERD_NONNULL out);

/**
   Set target `t` to reference `r` resolved against `base`.

   @see [RFC3986 5.2.2](http://tools.ietf.org/html/rfc3986#section-5.2.2)
*/
SERD_API
void
serd_uri_resolve(const SerdURI* SERD_NONNULL r,
                 const SerdURI* SERD_NONNULL base,
                 SerdURI* SERD_NONNULL       t);

/// Serialise `uri` with a series of calls to `sink`
SERD_API
size_t
serd_uri_serialise(const SerdURI* SERD_NONNULL uri,
                   SerdWriteFunc SERD_NONNULL  sink,
                   void* SERD_NONNULL          stream);

/**
   Serialise `uri` relative to `base` with a series of calls to `sink`

   The `uri` is written as a relative URI iff if it a child of `base` and
   `root`.  The optional `root` parameter must be a prefix of `base` and can be
   used keep up-references ("../") within a certain namespace.
*/
SERD_API
size_t
serd_uri_serialise_relative(const SerdURI* SERD_NONNULL  uri,
                            const SerdURI* SERD_NULLABLE base,
                            const SerdURI* SERD_NULLABLE root,
                            SerdWriteFunc SERD_NONNULL   sink,
                            void* SERD_NONNULL           stream);

/**
   @}
   @name Node
   @{
*/

/**
   Create a new "simple" node that is just a string.

   This can be used to create blank, CURIE, or URI nodes from an already
   measured string or slice of a buffer, which avoids a strlen compared to the
   friendly constructors.  This may not be used for literals since those must
   be measured to set the SERD_HAS_NEWLINE and SERD_HAS_QUOTE flags.
*/
SERD_API
SerdNode* SERD_ALLOCATED
serd_new_simple_node(SerdNodeType             type,
                     const char* SERD_NONNULL str,
                     size_t                   len);

/// Create a new plain literal string node from `str`
SERD_API
SerdNode* SERD_ALLOCATED
serd_new_string(const char* SERD_NONNULL str);

/// Create a new plain literal string node from a prefix of `str`
SERD_API
SerdNode* SERD_ALLOCATED
serd_new_substring(const char* SERD_NONNULL str, size_t len);

/**
   Create a new literal node from substrings.

   This is a low-level constructor which can be used for constructing a literal
   from slices of a buffer (for example, directly from a Turtle literal)
   without copying.  In most cases, applications should use the simpler
   serd_new_plain_literal() or serd_new_typed_literal().

   Either `datatype_uri` or `lang` can be given, but not both, unless
   `datatype_uri` is rdf:langString in which case it is ignored.

   @param str Literal body string.
   @param str_len Length of `str` in bytes.
   @param datatype_uri Full datatype URI, or NULL.
   @param datatype_uri_len Length of `datatype_uri` in bytes.
   @param lang Language string.
   @param lang_len Length of `lang` in bytes.
*/
SERD_API
SerdNode* SERD_ALLOCATED
serd_new_literal(const char* SERD_NONNULL  str,
                 size_t                    str_len,
                 const char* SERD_NULLABLE datatype_uri,
                 size_t                    datatype_uri_len,
                 const char* SERD_NULLABLE lang,
                 size_t                    lang_len);

/**
   Create a new literal node from `str`.

   A plain literal has no datatype, but may have a language tag.  The `lang`
   may be NULL, in which case this is equivalent to `serd_new_string()`.
*/
SERD_API
SerdNode* SERD_ALLOCATED
serd_new_plain_literal(const char* SERD_NONNULL  str,
                       const char* SERD_NULLABLE lang);

/**
   Create a new typed literal node from `str`.

   A typed literal has no language tag, but may have a datatype.  The
   `datatype` may be NULL, in which case this is equivalent to
   `serd_new_string()`.
*/
SERD_API
SerdNode* SERD_ALLOCATED
serd_new_typed_literal(const char* SERD_NONNULL      str,
                       const SerdNode* SERD_NULLABLE datatype);

/// Create a new blank node
SERD_API
SerdNode* SERD_ALLOCATED
serd_new_blank(const char* SERD_NONNULL str);

/// Create a new CURIE node
SERD_API
SerdNode* SERD_ALLOCATED
serd_new_curie(const char* SERD_NONNULL str);

/// Create a new URI from a string
SERD_API
SerdNode* SERD_ALLOCATED
serd_new_uri(const char* SERD_NONNULL str);

/// Create a new URI from a string, resolved against a base URI
SERD_API
SerdNode* SERD_ALLOCATED
serd_new_resolved_uri(const char* SERD_NONNULL      str,
                      const SerdNode* SERD_NULLABLE base);

/**
   Resolve `node` against `base`.

   If `node` is not a relative URI, an equivalent new node is returned.
*/
SERD_API
SerdNode* SERD_ALLOCATED
serd_node_resolve(const SerdNode* SERD_NONNULL  node,
                  const SerdNode* SERD_NULLABLE base);

/**
   Create a new file URI node from a file system path and optional hostname.

   Backslashes in Windows paths will be converted, and other characters will be
   percent encoded as necessary.

   If `path` is relative, `hostname` is ignored.
*/
SERD_API
SerdNode* SERD_ALLOCATED
serd_new_file_uri(const char* SERD_NONNULL  path,
                  const char* SERD_NULLABLE hostname);

/**
   Create a new URI from a string, relative to a base URI.

   The URI is made relative iff if it a child of `base` and `root`.  The
   optional `root` parameter must be a prefix of `base` and can be used keep
   up-references ("../") within a certain namespace.

   @param str URI string.
   @param base Base URI to make `str` relative to, if possible.
   @param root Optional root URI for resolution.
*/
SERD_API
SerdNode* SERD_ALLOCATED
serd_new_relative_uri(const char* SERD_NONNULL      str,
                      const SerdNode* SERD_NULLABLE base,
                      const SerdNode* SERD_NULLABLE root);

/**
   Create a new node by serialising `d` into an xsd:decimal string

   The resulting node will always contain a `.', start with a digit, and end
   with a digit (i.e. will have a leading and/or trailing `0' if necessary).
   It will never be in scientific notation.  A maximum of `frac_digits` digits
   will be written after the decimal point, but trailing zeros will
   automatically be omitted (except one if `d` is a round integer).

   Note that 17 and 9 significant digits are required to precisely represent
   any double and float, respectively.  This function may be lossy if
   `precision` is too low, or if `frac_digits` is not 0 (unlimited).

   This function may produce very long strings, serd_new_float() or
   serd_new_double() may be a better choice for very large or small numbers.

   @param d The value for the new node.
   @param max_precision Maximum number of significant decimal digits.
   @param max_frac_digits Maximum number of digits after decimal point, or 0.
   @param datatype Datatype of node, or NULL for xsd:decimal.
*/
SERD_API
SerdNode* SERD_ALLOCATED
serd_new_decimal(double                        d,
                 unsigned                      max_precision,
                 unsigned                      max_frac_digits,
                 const SerdNode* SERD_NULLABLE datatype);

/**
   Create a new node by serialising `d` into a normalised xsd:double string.

   The returned node will always be in normalised scientific notation, like
   "1.23E4", except for NaN and negative/positive infinity, which are "NaN",
   "-INF", and "INF", respectively.

   Uses the shortest possible representation that precisely describes `d`,
   which has at most 17 significant digits (under 24 characters total).

   @param d Double value to serialise.
   @return A literal node with datatype xsd:double.
*/
SERD_API
SerdNode* SERD_ALLOCATED
serd_new_double(double d);

/**
   Create a new node by serialising `f` into a normalised xsd:float string.

   Uses identical formatting to serd_new_double(), except with at most 9
   significant digits (under 14 characters total).

   @param f Float value to serialise.
   @return A literal node with datatype xsd:float.
*/
SERD_API
SerdNode* SERD_ALLOCATED
serd_new_float(float f);

/**
   Create a new node by serialising `i` into an xsd:integer string

   @param i Integer value to serialise.
   @param datatype Datatype of node, or NULL for xsd:integer.
*/
SERD_API
SerdNode* SERD_ALLOCATED
serd_new_integer(int64_t i, const SerdNode* SERD_NULLABLE datatype);

/// Create a new node by serialising `b` into an xsd:boolean string
SERD_API
SerdNode* SERD_ALLOCATED
serd_new_boolean(bool b);

/**
   Create a node by serialising `buf` into an xsd:base64Binary string

   This function can be used to make a serialisable node out of arbitrary
   binary data, which can be decoded using serd_base64_decode().

   @param buf Raw binary input data.
   @param size Size of `buf`.
   @param wrap_lines Wrap lines at 76 characters to conform to RFC 2045.
   @param datatype Datatype of node, or NULL for xsd:base64Binary.
*/
SERD_API
SerdNode* SERD_ALLOCATED
serd_new_blob(const void* SERD_NONNULL      buf,
              size_t                        size,
              bool                          wrap_lines,
              const SerdNode* SERD_NULLABLE datatype);

/// Return a deep copy of `node`
SERD_API
SerdNode* SERD_ALLOCATED
serd_node_copy(const SerdNode* SERD_NULLABLE node);

/// Free any data owned by `node`
SERD_API
void
serd_node_free(SerdNode* SERD_NULLABLE node);

/// Return the type of a node (SERD_URI, SERD_BLANK, or SERD_LITERAL)
SERD_PURE_API
SerdNodeType
serd_node_type(const SerdNode* SERD_NONNULL node);

/// Return the string value of a node
SERD_CONST_API
const char* SERD_NONNULL
serd_node_string(const SerdNode* SERD_NONNULL node);

/// Return the length of the string value of a node in bytes
SERD_PURE_API
size_t
serd_node_length(const SerdNode* SERD_NULLABLE node);

/// Return the flags (string properties) of a node
SERD_PURE_API
SerdNodeFlags
serd_node_flags(const SerdNode* SERD_NONNULL node);

/// Return the datatype of a literal node, or NULL
SERD_PURE_API
const SerdNode* SERD_NULLABLE
serd_node_datatype(const SerdNode* SERD_NONNULL node);

/// Return the language tag of a literal node, or NULL
SERD_PURE_API
const SerdNode* SERD_NULLABLE
serd_node_language(const SerdNode* SERD_NONNULL node);

/// Return true iff `a` is equal to `b`
SERD_PURE_API
bool
serd_node_equals(const SerdNode* SERD_NULLABLE a,
                 const SerdNode* SERD_NULLABLE b);

/**
   Compare two nodes.

   Returns less than, equal to, or greater than zero if `a` is less than, equal
   to, or greater than `b`, respectively.  NULL is treated as less than any
   other node.
*/
SERD_PURE_API
int
serd_node_compare(const SerdNode* SERD_NULLABLE a,
                  const SerdNode* SERD_NULLABLE b);

/**
   @}
   @name Event Handlers
   @{
*/

/// Type of a SerdEvent
typedef enum {
	SERD_BASE      = 1, ///< Base URI changed
	SERD_PREFIX    = 2, ///< New URI prefix
	SERD_STATEMENT = 3, ///< Statement
	SERD_END       = 4  ///< End of anonymous node
} SerdEventType;

/**
   Event for base URI changes

   Emitted whenever the base URI of the serialisation changes.
*/
typedef struct {
	SerdEventType                type; ///< #SERD_BASE
	const SerdNode* SERD_NONNULL uri;  ///< Base URI
} SerdBaseEvent;

/**
   Event for namespace definitions

   Emitted whenever a prefix is defined in the serialisation.
*/
typedef struct {
	SerdEventType                type; ///< #SERD_PREFIX
	const SerdNode* SERD_NONNULL name; ///< Prefix name
	const SerdNode* SERD_NONNULL uri;  ///< Namespace URI
} SerdPrefixEvent;

/**
   Event for statements

   Emitted for every RDF statement in the serialisation.
*/
typedef struct {
	SerdEventType                     type;      ///< #SERD_STATEMENT
	SerdStatementFlags                flags;     ///< Flags for pretty-printing
	const SerdStatement* SERD_NONNULL statement; ///< Statement
} SerdStatementEvent;

/**
   Event for the end of anonymous node descriptions

   This is emitted to indicate that the anonymous node with the given
   `value` will no longer be referred to by any future statements
   (i.e. the anonymous serialisation of the node is finished).
*/
typedef struct {
	SerdEventType                type; ///< #SERD_END
	const SerdNode* SERD_NONNULL node; ///< Anonymous node that is finished
} SerdEndEvent;

/**
   An event in a data stream

   Streams of data are represented as a series of events.  Events represent
   everything that can occur in an RDF document, and are used to plumb together
   different components.  For example, when parsing a document, a reader emits
   a stream of events which can be sent to a writer to serialise a document, or
   to an inserter to build a model in memory.
*/
typedef union {
	SerdEventType      type;      ///< Event type (always set)
	SerdBaseEvent      base;      ///< Base URI changed
	SerdPrefixEvent    prefix;    ///< New namespace prefix
	SerdStatementEvent statement; ///< Statement
	SerdEndEvent       end;       ///< End of anonymous node
} SerdEvent;

/// Function for handling events
typedef SerdStatus (*SerdEventFunc)(void* SERD_NULLABLE           handle,
                                    const SerdEvent* SERD_NONNULL event);

/**
   @}
   @name World
   @{
*/

/**
   Create a new Serd World.

   It is safe to use multiple worlds in one process, though no objects can be
   shared between worlds.
*/
SERD_MALLOC_API
SerdWorld* SERD_ALLOCATED
serd_world_new(void);

/// Free `world`
SERD_API
void
serd_world_free(SerdWorld* SERD_NULLABLE world);

/**
   Return the nodes cache in `world`.

   The returned cache is owned by the world and contains various nodes used
   frequently by the implementation.  For convenience, it may be used to store
   additional nodes which will be freed when the world is freed.
*/
SERD_PURE_API
SerdNodes* SERD_NONNULL
serd_world_nodes(SerdWorld* SERD_NONNULL world);

/**
   Return a unique blank node.

   The returned node is valid only until the next time serd_world_get_blank()
   is called or the world is destroyed.
*/
SERD_API
const SerdNode* SERD_NONNULL
serd_world_get_blank(SerdWorld* SERD_NONNULL world);

/**
   @}
   @name Logging
   @{
*/

/// Log message level, compatible with syslog
typedef enum {
	SERD_LOG_LEVEL_EMERG,    ///< Emergency, system is unusable
	SERD_LOG_LEVEL_ALERT,    ///< Action must be taken immediately
	SERD_LOG_LEVEL_CRIT,     ///< Critical condition
	SERD_LOG_LEVEL_ERR,      ///< Error
	SERD_LOG_LEVEL_WARNING,  ///< Warning
	SERD_LOG_LEVEL_NOTICE,   ///< Normal but significant condition
	SERD_LOG_LEVEL_INFO,     ///< Informational message
	SERD_LOG_LEVEL_DEBUG     ///< Debug message
} SerdLogLevel;

/**
   A structured log field.

   This can be used to pass additional information along with log messages.
   Syslog-compatible keys should be used where possible, otherwise, keys should
   be namespaced to prevent clashes.

   Serd itself uses the following keys:
   - ERRNO
   - SERD_COL
   - SERD_FILE
   - SERD_LINE
   - SERD_STATUS
*/
typedef struct {
	const char* SERD_NONNULL key;    ///< Field name
	const char* SERD_NONNULL value;  ///< Field value
} SerdLogField;

/**
   A log entry (message).

   This is the description of a log entry which is passed to log functions.
   It is only valid in the stack frame it appears in, and may not be copied.
*/
typedef struct {
	const char* SERD_NONNULL          domain;   ///< Library/program/module name
	const SerdLogField* SERD_NULLABLE fields;   ///< Extra log fields
	const char* SERD_NONNULL          fmt;      ///< Printf-style format string
	va_list* SERD_NONNULL             args;     ///< Arguments for `fmt`
	SerdLogLevel                      level;    ///< Log level
	size_t                            n_fields; ///< Number of `fields`
} SerdLogEntry;

/**
   Sink function for log messages.

   @param handle Handle for user data.
   @param entry Pointer to log entry description.
*/
typedef SerdStatus (*SerdLogFunc)(void* SERD_NULLABLE              handle,
                                  const SerdLogEntry* SERD_NONNULL entry);

/// A SerdLogFunc that does nothing, for suppressing log output
SERD_API
SerdStatus
serd_quiet_error_func(void* SERD_NULLABLE              handle,
                      const SerdLogEntry* SERD_NONNULL entry);

/// Return the value of the log field named `key`, or NULL if none exists
SERD_PURE_API
const char* SERD_NULLABLE
serd_log_entry_get_field(const SerdLogEntry* SERD_NONNULL entry,
                         const char* SERD_NONNULL         key);

/**
   Set a function to be called with log messages (typically errors).

   The `log_func` will be called with `handle` as its first argument.  If
   no function is set, messages are printed to stderr.
*/
SERD_API
void
serd_world_set_log_func(SerdWorld* SERD_NONNULL   world,
                        SerdLogFunc SERD_NULLABLE log_func,
                        void* SERD_NULLABLE       handle);

/// Write a message to the log
SERD_API
SERD_LOG_FUNC(6, 0)
SerdStatus
serd_world_vlogf(const SerdWorld* SERD_NONNULL     world,
                 const char* SERD_NONNULL          domain,
                 SerdLogLevel                      level,
                 size_t                            n_fields,
                 const SerdLogField* SERD_NULLABLE fields,
                 const char* SERD_NONNULL          fmt,
                 va_list                           args);

/// Write a message to the log
SERD_API
SERD_LOG_FUNC(6, 7)
SerdStatus
serd_world_logf(const SerdWorld* SERD_NONNULL     world,
                const char* SERD_NONNULL          domain,
                SerdLogLevel                      level,
                size_t                            n_fields,
                const SerdLogField* SERD_NULLABLE fields,
                const char* SERD_NONNULL          fmt,
                ...);

/**
   @}
   @name Environment
   @{
*/

/// Create a new environment
SERD_API
SerdEnv* SERD_ALLOCATED
serd_env_new(const SerdNode* SERD_NULLABLE base_uri);

/// Copy an environment
SERD_API
SerdEnv* SERD_ALLOCATED
serd_env_copy(const SerdEnv* SERD_NULLABLE env);

/// Return true iff `a` is equal to `b`
SERD_PURE_API
bool
serd_env_equals(const SerdEnv* SERD_NULLABLE a, const SerdEnv* SERD_NULLABLE b);

/// Free `env`
SERD_API
void
serd_env_free(SerdEnv* SERD_NULLABLE env);

/// Get the current base URI
SERD_PURE_API
const SerdNode* SERD_NULLABLE
serd_env_base_uri(const SerdEnv* SERD_NULLABLE env);

/// Set the current base URI
SERD_API
SerdStatus
serd_env_set_base_uri(SerdEnv* SERD_NONNULL         env,
                      const SerdNode* SERD_NULLABLE uri);

/**
   Set a namespace prefix

   A namespace prefix is used to expand CURIE nodes, for example, with the
   prefix "xsd" set to "http://www.w3.org/2001/XMLSchema#", "xsd:decimal" will
   expand to "http://www.w3.org/2001/XMLSchema#decimal".
*/
SERD_API
SerdStatus
serd_env_set_prefix(SerdEnv* SERD_NONNULL        env,
                    const SerdNode* SERD_NONNULL name,
                    const SerdNode* SERD_NONNULL uri);

/// Set a namespace prefix
SERD_API
SerdStatus
serd_env_set_prefix_from_strings(SerdEnv* SERD_NONNULL    env,
                                 const char* SERD_NONNULL name,
                                 const char* SERD_NONNULL uri);

/**
   Qualify `uri` into a CURIE if possible.

   Returns null if `uri` can not be qualified (usually because no corresponding
   prefix is defined).
*/
SERD_API
SerdNode* SERD_ALLOCATED
serd_env_qualify(const SerdEnv* SERD_NULLABLE env,
                 const SerdNode* SERD_NONNULL uri);

/**
   Expand `node`, transforming CURIEs and URI references into absolute URIs.

   If `node` is a relative URI reference, it is expanded to a full URI if
   possible.  If `node` is a literal, its datatype is expanded if necessary.
   If `node` is a CURIE, it is expanded to a full URI if possible.

   For simple nodes that do not require expansion, a copy is returned.  Null is
   returned if `node` is/contains a CURIE or relative URI that can not be
   expanded.
*/
SERD_API
SerdNode* SERD_ALLOCATED
serd_env_expand(const SerdEnv* SERD_NULLABLE  env,
                const SerdNode* SERD_NULLABLE node);

/// Write all prefixes in `env` to `sink`
SERD_API
void
serd_env_write_prefixes(const SerdEnv* SERD_NONNULL  env,
                        const SerdSink* SERD_NONNULL sink);

/**
   @}
   @name Sink
   @{
*/

/// Function to free an opaque handle
typedef void (*SerdFreeFunc)(void* SERD_NULLABLE ptr);

/**
   Create a new sink.

   Initially, the sink has no set functions and will do nothing.  Use the
   serd_sink_set_*_func functions to set handlers for various events.

   @param handle Opaque handle that will be passed to sink functions.
   @param free_handle Free function to call on handle in serd_sink_free().
*/
SERD_API
SerdSink* SERD_ALLOCATED
serd_sink_new(void* SERD_NULLABLE        handle,
              SerdFreeFunc SERD_NULLABLE free_handle);

/// Free `sink`
SERD_API
void
serd_sink_free(SerdSink* SERD_NULLABLE sink);

/// Set a function to be called for every event
SERD_API
SerdStatus
serd_sink_set_event_func(SerdSink* SERD_NONNULL     sink,
                         SerdEventFunc SERD_NONNULL event_func);

/// Send an event to the sink
SERD_API
SerdStatus
serd_sink_write_event(const SerdSink* SERD_NONNULL  sink,
                      const SerdEvent* SERD_NONNULL event);

/// Set the base URI
SERD_API
SerdStatus
serd_sink_write_base(const SerdSink* SERD_NONNULL sink,
                     const SerdNode* SERD_NONNULL uri);

/// Set a namespace prefix
SERD_API
SerdStatus
serd_sink_write_prefix(const SerdSink* SERD_NONNULL sink,
                       const SerdNode* SERD_NONNULL name,
                       const SerdNode* SERD_NONNULL uri);

/// Write a statement
SERD_API
SerdStatus
serd_sink_write_statement(const SerdSink* SERD_NONNULL      sink,
                          SerdStatementFlags                flags,
                          const SerdStatement* SERD_NONNULL statement);

/// Write a statement from individual nodes
SERD_API
SerdStatus
serd_sink_write(const SerdSink* SERD_NONNULL  sink,
                SerdStatementFlags            flags,
                const SerdNode* SERD_NONNULL  subject,
                const SerdNode* SERD_NONNULL  predicate,
                const SerdNode* SERD_NONNULL  object,
                const SerdNode* SERD_NULLABLE graph);

/// Mark the end of an anonymous node
SERD_API
SerdStatus
serd_sink_write_end(const SerdSink* SERD_NONNULL sink,
                    const SerdNode* SERD_NONNULL node);

/**
   @}
   @name Reader
   @{
*/

/// Create a new RDF reader
SERD_API
SerdReader* SERD_ALLOCATED
serd_reader_new(SerdWorld* SERD_NONNULL      world,
                SerdSyntax                   syntax,
                SerdReaderFlags              flags,
                const SerdSink* SERD_NONNULL sink,
                size_t                       stack_size);

/**
   Set a prefix to be added to all blank node identifiers

   This is useful when multiple files are to be parsed into the same output (a
   model or a file).  Since Serd preserves blank node IDs, this could cause
   conflicts where two non-equivalent blank nodes are merged, resulting in
   corrupt data.  By setting a unique blank node prefix for each parsed file,
   this can be avoided, while preserving blank node names.
*/
SERD_API
void
serd_reader_add_blank_prefix(SerdReader* SERD_NONNULL  reader,
                             const char* SERD_NULLABLE prefix);

/// Prepare to read from the file at a local file `uri`
SERD_API
SerdStatus
serd_reader_start_file(SerdReader* SERD_NONNULL reader,
                       const char* SERD_NONNULL uri,
                       bool                     bulk);

/**
   Prepare to read from a stream.

   The `read_func` is guaranteed to only be called for `page_size` elements
   with size 1 (i.e. `page_size` bytes).
*/
SERD_API
SerdStatus
serd_reader_start_stream(SerdReader* SERD_NONNULL         reader,
                         SerdReadFunc SERD_NONNULL        read_func,
                         SerdStreamErrorFunc SERD_NONNULL error_func,
                         void* SERD_NONNULL               stream,
                         const SerdNode* SERD_NULLABLE    name,
                         size_t                           page_size);

/// Prepare to read from a string
SERD_API
SerdStatus
serd_reader_start_string(SerdReader* SERD_NONNULL      reader,
                         const char* SERD_NONNULL      utf8,
                         const SerdNode* SERD_NULLABLE name);

/**
   Read a single "chunk" of data during an incremental read

   This function will read a single top level description, and return.  This
   may be a directive, statement, or several statements; essentially it reads
   until a '.' is encountered.  This is particularly useful for reading
   directly from a pipe or socket.
*/
SERD_API
SerdStatus
serd_reader_read_chunk(SerdReader* SERD_NONNULL reader);

/**
   Read a complete document from the source.

   This function will continue pulling from the source until a complete
   document has been read.  Note that this may block when used with streams,
   for incremental reading use serd_reader_read_chunk().
*/
SERD_API
SerdStatus
serd_reader_read_document(SerdReader* SERD_NONNULL reader);

/**
   Finish reading from the source.

   This should be called before starting to read from another source.
*/
SERD_API
SerdStatus
serd_reader_finish(SerdReader* SERD_NONNULL reader);

/**
   Free `reader`.

   The reader will be finished via `serd_reader_finish()` if necessary.
*/
SERD_API
void
serd_reader_free(SerdReader* SERD_NULLABLE reader);

/**
   @}
   @name Writer
   @{
*/

/// Create a new RDF writer
SERD_API
SerdWriter* SERD_ALLOCATED
serd_writer_new(SerdWorld* SERD_NONNULL    world,
                SerdSyntax                 syntax,
                SerdWriterFlags            flags,
                SerdEnv* SERD_NONNULL      env,
                SerdWriteFunc SERD_NONNULL write_func,
                void* SERD_NULLABLE        stream);

/// Free `writer`
SERD_API
void
serd_writer_free(SerdWriter* SERD_NULLABLE writer);

/// Return a sink interface that emits statements via `writer`
SERD_CONST_API
const SerdSink* SERD_NONNULL
serd_writer_sink(SerdWriter* SERD_NONNULL writer);

/// Return the env used by `writer`
SERD_PURE_API
SerdEnv* SERD_NONNULL
serd_writer_env(SerdWriter* SERD_NONNULL writer);

/**
   A convenience sink function for writing to a string.

   This function can be used as a SerdSink to write to a SerdBuffer which is
   resized as necessary with realloc().  The `stream` parameter must point to
   an initialized SerdBuffer.  When the write is finished, the string should be
   retrieved with serd_buffer_sink_finish().
*/
SERD_API
size_t
serd_buffer_sink(const void* SERD_NONNULL buf,
                 size_t                   size,
                 size_t                   nmemb,
                 void* SERD_NONNULL       stream);

/**
   Finish a serialisation to a buffer with serd_buffer_sink().

   The returned string is the result of the serialisation, which is null
   terminated (by this function) and owned by the caller.
*/
SERD_API
char* SERD_NULLABLE
serd_buffer_sink_finish(SerdBuffer* SERD_NONNULL stream);

/**
   Set a prefix to be removed from matching blank node identifiers

   This is the counterpart to serd_reader_add_blank_prefix() which can be used
   to "undo" added prefixes.
*/
SERD_API
void
serd_writer_chop_blank_prefix(SerdWriter* SERD_NONNULL  writer,
                              const char* SERD_NULLABLE prefix);

/**
   Set the current output base URI, and emit a directive if applicable.

   Note this function can be safely casted to SerdBaseSink.
*/
SERD_API
SerdStatus
serd_writer_set_base_uri(SerdWriter* SERD_NONNULL      writer,
                         const SerdNode* SERD_NULLABLE uri);

/**
   Set the current root URI.

   The root URI should be a prefix of the base URI.  The path of the root URI
   is the highest path any relative up-reference can refer to.  For example,
   with root <file:///foo/root> and base <file:///foo/root/base>,
   <file:///foo/root> will be written as <../>, but <file:///foo> will be
   written non-relatively as <file:///foo>.  If the root is not explicitly set,
   it defaults to the base URI, so no up-references will be created at all.
*/
SERD_API
SerdStatus
serd_writer_set_root_uri(SerdWriter* SERD_NONNULL      writer,
                         const SerdNode* SERD_NULLABLE uri);

/**
   Finish a write

   This flushes any pending output, for example terminating punctuation, so
   that the output is a complete document.
*/
SERD_API
SerdStatus
serd_writer_finish(SerdWriter* SERD_NONNULL writer);

/**
   @}
   @name Nodes
   @{
*/

/// Create a new node set
SERD_API
SerdNodes* SERD_ALLOCATED
serd_nodes_new(void);

/**
   Free `nodes` and all nodes that are stored in it.

   Note that this invalidates any pointers previously returned from
   `serd_nodes_intern()` or `serd_nodes_manage()` calls on `nodes`.
*/
SERD_API
void
serd_nodes_free(SerdNodes* SERD_NULLABLE nodes);

/**
   Intern `node`.

   Multiple calls with equivalent nodes will return the same pointer.

   @return A node that is different than, but equivalent to, `node`.
*/
SERD_API
const SerdNode* SERD_ALLOCATED
serd_nodes_intern(SerdNodes* SERD_NONNULL       nodes,
                  const SerdNode* SERD_NULLABLE node);

/**
   Manage `node`.

   Like `serd_nodes_intern`, but takes ownership of `node`, freeing it and
   returning a previously interned/managed equivalent node if necessary.

   @return A node that is equivalent to `node`.
*/
SERD_API
const SerdNode* SERD_ALLOCATED
serd_nodes_manage(SerdNodes* SERD_NONNULL nodes, SerdNode* SERD_NULLABLE node);

/**
   Dereference `node`.

   Decrements the reference count of `node`, and frees the internally stored
   equivalent node if this was the last reference.  Does nothing if no node
   equivalent to `node` is stored in `nodes`.
*/
SERD_API
void
serd_nodes_deref(SerdNodes* SERD_NONNULL      nodes,
                 const SerdNode* SERD_NONNULL node);

/**
   @}
   @name Model
   @{
*/

/**
   Create a new model

   @param world The world in which to make this model.

   @param flags Model options, including enabled indices, for example `SERD_SPO
   | SERD_OPS`.  Be sure to enable an index where the most significant node(s)
   are not variables in your queries.  For example, to make (? P O) queries,
   enable either SERD_OPS or SERD_POS.
*/
SERD_API
SerdModel* SERD_ALLOCATED
serd_model_new(SerdWorld* SERD_NONNULL world, SerdModelFlags flags);

/// Return a deep copy of `model`
SERD_API
SerdModel* SERD_ALLOCATED
serd_model_copy(const SerdModel* SERD_NONNULL model);

/// Return true iff `a` is equal to `b`, ignoring statement cursor metadata
SERD_API
bool
serd_model_equals(const SerdModel* SERD_NULLABLE a,
                  const SerdModel* SERD_NULLABLE b);

/// Close and free `model`
SERD_API
void
serd_model_free(SerdModel* SERD_NULLABLE model);

/// Get the world associated with `model`
SERD_PURE_API
SerdWorld* SERD_NONNULL
serd_model_world(SerdModel* SERD_NONNULL model);

/// Get the flags enabled on `model`
SERD_PURE_API
SerdModelFlags
serd_model_flags(const SerdModel* SERD_NONNULL model);

/// Return the number of statements stored in `model`
SERD_PURE_API
size_t
serd_model_size(const SerdModel* SERD_NONNULL model);

/// Return true iff there are no statements stored in `model`
SERD_PURE_API
bool
serd_model_empty(const SerdModel* SERD_NONNULL model);

/// Return an iterator to the start of `model`
SERD_API
SerdIter* SERD_ALLOCATED
serd_model_begin(const SerdModel* SERD_NONNULL model);

/// Return an iterator to the end of `model`
SERD_PURE_API
const SerdIter* SERD_NONNULL
serd_model_end(const SerdModel* SERD_NONNULL model);

/// Return a range of all statements in `model`
SERD_API
SerdRange* SERD_ALLOCATED
serd_model_all(const SerdModel* SERD_NONNULL model);

/**
   Search for statements by a quad pattern

   @return An iterator to the first match, or NULL if no matches found.
*/
SERD_API
SerdIter* SERD_ALLOCATED
serd_model_find(const SerdModel* SERD_NONNULL model,
                const SerdNode* SERD_NULLABLE s,
                const SerdNode* SERD_NULLABLE p,
                const SerdNode* SERD_NULLABLE o,
                const SerdNode* SERD_NULLABLE g);

/**
   Search for statements by a quad pattern

   @return A range containing all matching statements.
*/
SERD_API
SerdRange* SERD_ALLOCATED
serd_model_range(const SerdModel* SERD_NONNULL model,
                 const SerdNode* SERD_NULLABLE s,
                 const SerdNode* SERD_NULLABLE p,
                 const SerdNode* SERD_NULLABLE o,
                 const SerdNode* SERD_NULLABLE g);

/**
   Search for a single node that matches a pattern

   Exactly one of `s`, `p`, `o` must be NULL.
   This function is mainly useful for predicates that only have one value.
   @return The first matching node, or NULL if no matches are found.
*/
SERD_API
const SerdNode* SERD_NULLABLE
serd_model_get(const SerdModel* SERD_NONNULL model,
               const SerdNode* SERD_NULLABLE s,
               const SerdNode* SERD_NULLABLE p,
               const SerdNode* SERD_NULLABLE o,
               const SerdNode* SERD_NULLABLE g);

/**
   Search for a single statement that matches a pattern

   This function is mainly useful for predicates that only have one value.

   @return The first matching statement, or NULL if none are found.
*/
SERD_API
const SerdStatement* SERD_NULLABLE
serd_model_get_statement(const SerdModel* SERD_NONNULL model,
                         const SerdNode* SERD_NULLABLE s,
                         const SerdNode* SERD_NULLABLE p,
                         const SerdNode* SERD_NULLABLE o,
                         const SerdNode* SERD_NULLABLE g);

/// Return true iff a statement exists
SERD_API
bool
serd_model_ask(const SerdModel* SERD_NONNULL model,
               const SerdNode* SERD_NULLABLE s,
               const SerdNode* SERD_NULLABLE p,
               const SerdNode* SERD_NULLABLE o,
               const SerdNode* SERD_NULLABLE g);

/// Return the number of matching statements
SERD_API
size_t
serd_model_count(const SerdModel* SERD_NONNULL model,
                 const SerdNode* SERD_NULLABLE s,
                 const SerdNode* SERD_NULLABLE p,
                 const SerdNode* SERD_NULLABLE o,
                 const SerdNode* SERD_NULLABLE g);

/**
   Add a statement to a model from nodes

   This function fails if there are any active iterators on `model`.
*/
SERD_API
SerdStatus
serd_model_add(SerdModel* SERD_NONNULL       model,
               const SerdNode* SERD_NULLABLE s,
               const SerdNode* SERD_NULLABLE p,
               const SerdNode* SERD_NULLABLE o,
               const SerdNode* SERD_NULLABLE g);

/**
   Add a statement to a model

   This function fails if there are any active iterators on `model`.
   If statement is null, then SERD_FAILURE is returned.
*/
SERD_API
SerdStatus
serd_model_insert(SerdModel* SERD_NONNULL            model,
                  const SerdStatement* SERD_NULLABLE statement);

/**
   Add a range of statements to a model

   This function fails if there are any active iterators on `model`.
*/
SERD_API
SerdStatus
serd_model_add_range(SerdModel* SERD_NONNULL model,
                     SerdRange* SERD_NONNULL range);

/**
   Remove a quad from a model via an iterator

   Calling this function invalidates all iterators on `model` except `iter`.

   @param model The model which `iter` points to.
   @param iter Iterator to the element to erase, which is incremented to the
   next value on return.
*/
SERD_API
SerdStatus
serd_model_erase(SerdModel* SERD_NONNULL model, SerdIter* SERD_NONNULL iter);

/**
   Remove a range from a model

   Calling this function invalidates all iterators on `model` except `iter`.

   @param model The model which `range` points to.
   @param range Range to erase, which will be empty on return;
*/
SERD_API
SerdStatus
serd_model_erase_range(SerdModel* SERD_NONNULL model,
                       SerdRange* SERD_NONNULL range);

/**
   Validate model.

   This performs validation based on the RDFS and OWL vocabularies.  All
   necessary data, including those vocabularies and any property/class
   definitions that use them, are assumed to be in `model`.

   Validation errors are reported to the world's error sink.

   @return 0 on success.
*/
SERD_API
SerdStatus
serd_validate(const SerdModel* SERD_NONNULL model);

/**
   @}
   @name Inserter
   @{
*/

/// Create an inserter for writing statements to a model
SERD_API
SerdSink* SERD_ALLOCATED
serd_inserter_new(SerdModel* SERD_NONNULL       model,
                  SerdEnv* SERD_NONNULL         env,
                  const SerdNode* SERD_NULLABLE default_graph);

/**
   @}
   @name Statement
   @{
*/

/**
   Create a new statement

   Note that, to minimise model overhead, statements do not own their nodes, so
   they must have a longer lifetime than the statement for it to be valid.  For
   statements in models, this is the lifetime of the model.  For user-created
   statements, the simplest way to handle this is to use `SerdNodes`.

   @param s The subject
   @param p The predicate ("key")
   @param o The object ("value")
   @param g The graph ("context")
   @param cursor Optional cursor at the origin of this statement
   @return A new statement that must be freed with serd_statement_free()
*/
SERD_API
SerdStatement* SERD_ALLOCATED
serd_statement_new(const SerdNode* SERD_NONNULL    s,
                   const SerdNode* SERD_NONNULL    p,
                   const SerdNode* SERD_NONNULL    o,
                   const SerdNode* SERD_NULLABLE   g,
                   const SerdCursor* SERD_NULLABLE cursor);

/// Return a copy of `statement`
SERD_API
SerdStatement* SERD_ALLOCATED
serd_statement_copy(const SerdStatement* SERD_NULLABLE statement);

/// Free `statement`
SERD_API
void
serd_statement_free(SerdStatement* SERD_NULLABLE statement);

/// Return the given node in `statement`
SERD_PURE_API
const SerdNode* SERD_NULLABLE
serd_statement_node(const SerdStatement* SERD_NONNULL statement,
                    SerdField                         field);

/// Return the subject in `statement`
SERD_PURE_API
const SerdNode* SERD_NONNULL
serd_statement_subject(const SerdStatement* SERD_NONNULL statement);

/// Return the predicate in `statement`
SERD_PURE_API
const SerdNode* SERD_NONNULL
serd_statement_predicate(const SerdStatement* SERD_NONNULL statement);

/// Return the object in `statement`
SERD_PURE_API
const SerdNode* SERD_NONNULL
serd_statement_object(const SerdStatement* SERD_NONNULL statement);

/// Return the graph in `statement`
SERD_PURE_API
const SerdNode* SERD_NULLABLE
serd_statement_graph(const SerdStatement* SERD_NONNULL statement);

/// Return the source location where `statement` originated, or NULL
SERD_PURE_API
const SerdCursor* SERD_NULLABLE
serd_statement_cursor(const SerdStatement* SERD_NONNULL statement);

/**
   Return true iff `a` is equal to `b`, ignoring statement cursor metadata

   Only returns true if nodes are equivalent, does not perform wildcard matching.
*/
SERD_PURE_API
bool
serd_statement_equals(const SerdStatement* SERD_NULLABLE a,
                      const SerdStatement* SERD_NULLABLE b);

/**
   Return true iff `statement` matches the given pattern

   Nodes match if they are equivalent, or if one of them is NULL.  The
   statement matches if every node matches.
*/
SERD_PURE_API
bool
serd_statement_matches(const SerdStatement* SERD_NONNULL statement,
                       const SerdNode* SERD_NULLABLE     subject,
                       const SerdNode* SERD_NULLABLE     predicate,
                       const SerdNode* SERD_NULLABLE     object,
                       const SerdNode* SERD_NULLABLE     graph);

/**
   @}
   @name Iterator
   @{
*/

/// Return a new copy of `iter`
SERD_API
SerdIter* SERD_ALLOCATED
serd_iter_copy(const SerdIter* SERD_NULLABLE iter);

/// Return the statement pointed to by `iter`
SERD_API
const SerdStatement* SERD_NULLABLE
serd_iter_get(const SerdIter* SERD_NONNULL iter);

/**
   Increment `iter` to point to the next statement

   @return True iff `iter` has reached the end.
*/
SERD_API
bool
serd_iter_next(SerdIter* SERD_NONNULL iter);

/// Return true iff `lhs` is equal to `rhs`
SERD_PURE_API
bool
serd_iter_equals(const SerdIter* SERD_NULLABLE lhs,
                 const SerdIter* SERD_NULLABLE rhs);

/// Free `iter`
SERD_API
void
serd_iter_free(SerdIter* SERD_NULLABLE iter);

/**
   @}
   @name Range
   @{
*/

/// Return a new copy of `range`
SERD_API
SerdRange* SERD_ALLOCATED
serd_range_copy(const SerdRange* SERD_NULLABLE range);

/// Free `range`
SERD_API
void
serd_range_free(SerdRange* SERD_NULLABLE range);

/// Return the first statement in `range`, or NULL if `range` is empty
SERD_API
const SerdStatement* SERD_NULLABLE
serd_range_front(const SerdRange* SERD_NONNULL range);

/// Return true iff `lhs` is equal to `rhs`
SERD_PURE_API
bool
serd_range_equals(const SerdRange* SERD_NULLABLE lhs,
                  const SerdRange* SERD_NULLABLE rhs);

/// Increment the start of `range` to point to the next statement
SERD_API
bool
serd_range_next(SerdRange* SERD_NONNULL range);

/// Return true iff there are no statements in `range`
SERD_PURE_API
bool
serd_range_empty(const SerdRange* SERD_NULLABLE range);

/// Return an iterator to the start of `range`
SERD_PURE_API
const SerdIter* SERD_NULLABLE
serd_range_cbegin(const SerdRange* SERD_NONNULL range);

/// Return an iterator to the end of `range`
SERD_PURE_API
const SerdIter* SERD_NULLABLE
serd_range_cend(const SerdRange* SERD_NONNULL range);

/// Return an iterator to the start of `range`
SERD_PURE_API
SerdIter* SERD_NULLABLE
serd_range_begin(SerdRange* SERD_NONNULL range);

/// Return an iterator to the end of `range`
SERD_PURE_API
SerdIter* SERD_NULLABLE
serd_range_end(SerdRange* SERD_NONNULL range);

/**
   Write `range` to `sink`

   The serialisation style can be controlled with `flags`.  The default is to
   write statements in an order suited for pretty-printing with Turtle or TriG
   with as many objects written inline as possible.  If
   `SERD_NO_INLINE_OBJECTS` is given, a simple sorted stream is written
   instead, which is significantly faster since no searching is required, but
   can result in ugly output for Turtle or Trig.
*/
SERD_API
SerdStatus
serd_range_serialise(const SerdRange* SERD_NONNULL range,
                     const SerdSink* SERD_NONNULL  sink,
                     SerdSerialisationFlags        flags);

/**
   @}
   @}
*/

/**
   @}
   @name Cursor
   @{
*/

/**
   Create a new cursor

   Note that, to minimise model overhead, the cursor does not own the name
   node, so `name` must have a longer lifetime than the cursor for it to be
   valid.  That is, serd_cursor_name() will return exactly the pointer
   `name`, not a copy.  For cursors from models, this is the lifetime of the
   model.  For user-created cursors, the simplest way to handle this is to use
   `SerdNodes`.

   @param name The name of the document or stream (usually a file URI)
   @param line The line number in the document (1-based)
   @param col The column number in the document (1-based)
   @return A new cursor that must be freed with serd_cursor_free()
*/
SERD_API
SerdCursor* SERD_ALLOCATED
serd_cursor_new(const SerdNode* SERD_NONNULL name, unsigned line, unsigned col);

/// Return a copy of `cursor`
SERD_API
SerdCursor* SERD_ALLOCATED
serd_cursor_copy(const SerdCursor* SERD_NULLABLE cursor);

/// Free `cursor`
SERD_API
void
serd_cursor_free(SerdCursor* SERD_NULLABLE cursor);

/// Return true iff `lhs` is equal to `rhs`
SERD_PURE_API
bool
serd_cursor_equals(const SerdCursor* SERD_NULLABLE lhs,
                   const SerdCursor* SERD_NULLABLE rhs);

/**
   Return the document name.

   This is typically a file URI, but may be a descriptive string node for
   statements that originate from streams.
*/
SERD_PURE_API
const SerdNode* SERD_NONNULL
serd_cursor_name(const SerdCursor* SERD_NONNULL cursor);

/// Return the one-relative line number in the document
SERD_PURE_API
unsigned
serd_cursor_line(const SerdCursor* SERD_NONNULL cursor);

/// Return the zero-relative column number in the line
SERD_PURE_API
unsigned
serd_cursor_column(const SerdCursor* SERD_NONNULL cursor);

#ifdef __cplusplus
#    if defined(__GNUC__)
#        pragma GCC diagnostic pop
#    endif
}  /* extern "C" */
#endif

#endif  /* SERD_SERD_H */
