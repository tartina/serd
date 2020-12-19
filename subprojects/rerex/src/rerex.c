/*
  Copyright 2020 David Robillard <d@drobilla.net>

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

#include "rerex/rerex.h"

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

static const char cmin = 0x20; // Inclusive minimum normal character
static const char cmax = 0x7E; // Inclusive maximum normal character

/* Status */

const char*
rerex_strerror(const RerexStatus status)
{
	switch (status) {
	case REREX_SUCCESS:
		return "Success";
	case REREX_EXPECTED_CHAR:
		return "Expected a regular character";
	case REREX_EXPECTED_ELEMENT:
		return "Expected a character in a set";
	case REREX_EXPECTED_RBRACKET:
		return "Expected ']'";
	case REREX_EXPECTED_RPAREN:
		return "Expected ')'";
	case REREX_EXPECTED_SPECIAL:
		return "Expected a special character (one of \"()*+-?[]^|\")";
	case REREX_UNEXPECTED_SPECIAL:
		return "Unexpected special character";
	case REREX_UNEXPECTED_END:
		return "Unexpected end of input";
	case REREX_UNORDERED_RANGE:
		return "Range is out of order";
	}

	return "Unknown error";
}

/* State */

#define NO_STATE 0

// The ID for a state, which is an index into the state array
typedef size_t StateIndex;

// Special type for states with only epsilon out arcs
typedef enum {
	REREX_MATCH = 254, ///< Matching state, no out arcs
	REREX_SPLIT = 255  ///< Splitting state, one or two out arcs
} StateType;

/* A state in an NFA.

   A state in Thompson's NFA can have either a single character-labeled
   transition to another state, or up to two unlabeled epsilon transitions to
   other states.  There is both a minimum and maximum label for supporting
   character ranges.  So, either `min` and `max` are ASCII characters that are
   the label of an arc to next1 (and next2 is null), or `min` is a special
   StateType and next1 and/or next2 may be set to successor states.
*/
typedef struct {
	StateIndex next1; ///< Head of first out arc (or NULL)
	StateIndex next2; ///< Head of second out arc (or NULL)
	uint8_t    min;   ///< Special type, or inclusive min label for next1
	uint8_t    max;   ///< Inclusive max label for next2
} State;

// Create a match (end) state with no successors
static State
match_state(void)
{
	const State s = {NO_STATE, NO_STATE, REREX_MATCH, 0};
	return s;
}

// Create a split state with at most two successors
static State
split_state(const StateIndex next1, const StateIndex next2)
{
	const State s = {next1, next2, REREX_SPLIT, 0};
	return s;
}

// Create a labeled state with one successor reached by a character arc
static State
range_state(const char min, const char max, const StateIndex next)
{
	const State s = {next, NO_STATE, (uint8_t)min, (uint8_t)max};
	return s;
}

/* Array of states.

   States are stored in a flat array to reduce memory fragmentation, and for
   easy memory management since the automata graph may be cyclic.  This simple
   implementation calls realloc() for every state, which isn't terribly
   efficient, but works well enough.  Note that state addresses therefore
   change during compilation, so states are generally referred to by their
   index, and not by pointer.  Conveniently, using indices is also useful
   during matching for storing auxiliary information about states.
*/
typedef struct {
	State* states;
	size_t n_states;
} StateArray;

// Append a new state to the end of the state array
static StateIndex
add_state(StateArray* const array, const State state)
{
	const size_t new_n_states = array->n_states + 1;
	const size_t new_size     = new_n_states * sizeof(State);

	array->states                  = (State*)realloc(array->states, new_size);
	array->states[array->n_states] = state;

	return array->n_states++;
}

/* Automata */

// Lightweight description of an NFA fragment (states are stored elsewhere)
typedef struct {
	StateIndex start;
	StateIndex end;
} Automata;

// Simple utility function for making an automata in an expression
static Automata
make_automata(const StateIndex start, const StateIndex end)
{
	Automata result = {start, end};
	return result;
}

// Return whether `nfa` has only two simple states (used for optimizations)
static inline bool
is_trivial(const StateArray* const states, const Automata nfa)
{
	return states->states[nfa.start].min < REREX_MATCH &&
	       states->states[nfa.start].next1 == nfa.end;
}

// Kleene's Star of an NFA
static Automata
star(StateArray* const states, const Automata nfa)
{
	const StateIndex end   = add_state(states, match_state());
	const StateIndex start = add_state(states, split_state(nfa.start, end));

	states->states[nfa.end] = split_state(nfa.start, end);

	return make_automata(start, end);
}

// Zero-or-one of an NFA
static Automata
question(StateArray* const states, const Automata nfa)
{
	const StateIndex start = add_state(states, split_state(nfa.start, nfa.end));

	return make_automata(start, nfa.end);
}

// One-or-more of an NFA
static Automata
plus(StateArray* const states, const Automata nfa)
{
	const StateIndex end = add_state(states, match_state());

	states->states[nfa.end] = split_state(nfa.start, end);

	return make_automata(nfa.start, end);
}

// Concatenation of two NFAs
static Automata
concatenate(StateArray* const states, const Automata a, const Automata b)
{
	if (is_trivial(states, a)) {
		// Optimization: link a's start directly to b's start (drop a's end)
		states->states[a.start].next1 = b.start;
	} else {
		states->states[a.end] = split_state(b.start, NO_STATE);
	}

	return make_automata(a.start, b.end);
}

// Alternation (OR) of two NFAs
static Automata
alternate(StateArray* const states, const Automata a, const Automata b)
{
	const StateIndex split = add_state(states, split_state(a.start, b.start));

	if (is_trivial(states, a)) {
		// Optimization: link a's start directly to b's end (drop a's end)
		states->states[a.start].next1 = b.end;
		return make_automata(split, b.end);
	}

	if (is_trivial(states, b)) {
		// Optimization: link b's start directly to a's end (drop b's end)
		states->states[b.start].next1 = a.end;
		return make_automata(split, a.end);
	}

	const StateIndex end = add_state(states, match_state());

	states->states[a.end] = split_state(end, NO_STATE);
	states->states[b.end] = split_state(end, NO_STATE);

	return make_automata(split, end);
}

/* Parser */

typedef struct {
	const char* const str;
	size_t            offset;
} Input;

static RerexStatus
read_expr(Input* input, StateArray* states, Automata* out);

// Return the next character in the input without consuming it
static inline char
peek(Input* const input)
{
	return input->str[input->offset];
}

// Return the next-next character in the input without consuming any
static inline char
peekahead(Input* const input)
{
	// Unfortunately we need 2-char lookahead for the ambiguity of '-' in sets
	return input->str[input->offset + 1];
}

// Consume and return the next character in the input
static inline char
eat(Input* const input)
{
	return input->str[input->offset++];
}

// DOT      ::= '.'
// OPERATOR ::= '*' | '+' | '?'
// SPECIAL  ::= DOT | OPERATOR | '(' | ')' | '[' | ']' | '^' | '{' | '|' | '}'
static inline bool
is_special(const char c)
{
	switch (c) {
	case '(':
	case ')':
	case '*':
	case '+':
	case '.':
	case '?':
	case '[':
	case ']':
	case '^':
	case '{':
	case '|':
	case '}':
		return true;
	default:
		break;
	}

	return false;
}

// DOT ::= '.'
static RerexStatus
read_dot(Input* const input, StateArray* const states, Automata* const out)
{
	assert(peek(input) == '.');
	eat(input);

	const StateIndex end   = add_state(states, match_state());
	const StateIndex start = add_state(states, range_state(cmin, cmax, end));

	*out = make_automata(start, end);

	return REREX_SUCCESS;
}

// ESCAPE ::= '\' SPECIAL
static RerexStatus
read_escape(Input* const input, char* const out)
{
	assert(peek(input) == '\\');
	eat(input);

	const char c = peek(input);
	if (is_special(c) || c == '-') {
		*out = eat(input);
		return REREX_SUCCESS;
	}

	return REREX_EXPECTED_SPECIAL;
}

// CHAR ::= ESCAPE | [#x20-#x7E] - SPECIAL
static RerexStatus
read_char(Input* const input, char* const out)
{
	const char c = peek(input);

	switch (c) {
	case '\0':
		return REREX_UNEXPECTED_END;
	case '\\':
		return read_escape(input, out);
	default:
		break;
	}

	if (is_special(c)) {
		return REREX_UNEXPECTED_SPECIAL;
	}

	if (c >= cmin && c <= cmax) {
		*out = eat(input);
		return REREX_SUCCESS;
	}

	return REREX_EXPECTED_CHAR;
}

// ELEMENT ::= ([#x20-#x7E] - ']') | ('\' ']')
static RerexStatus
read_element(Input* const input, char* const out)
{
	const char c = peek(input);

	switch (c) {
	case '\0':
		return REREX_UNEXPECTED_END;
	case ']':
		return REREX_UNEXPECTED_SPECIAL;
	case '\\':
		eat(input);
		if (peek(input) != ']') {
			return REREX_EXPECTED_RBRACKET;
		}

		*out = eat(input);
		return REREX_SUCCESS;
	default:
		break;
	}

	if (c >= cmin && c <= cmax) {
		*out = eat(input);
		return REREX_SUCCESS;
	}

	return REREX_EXPECTED_ELEMENT;
}

// Range ::= ELEMENT | ELEMENT '-' ELEMENT
static RerexStatus
read_range(Input* const      input,
           StateArray* const states,
           const bool        negated,
           Automata* const   out)
{
	RerexStatus st  = REREX_SUCCESS;
	char        min = 0;
	char        max = 0;

	// Read the first (or only) character
	if ((st = read_element(input, &min))) {
		return st;
	}

	// Handle '-' which is a bit hairy because it may or may not be special
	if (peek(input) == '-') {
		switch (peekahead(input)) {
		case ']':
			// Weird case like [a-] where '-' is a character
			max = min;
			break;
		default:
			// Normal range like [a-z] (note that '[' isn't special here)
			eat(input);
			if ((st = read_element(input, &max))) {
				return st;
			}
			break;
		}
	} else {
		// Single character element
		max = min;
	}

	if (max < min) {
		return REREX_UNORDERED_RANGE;
	}

	const StateIndex end = add_state(states, match_state());
	if (negated) {
		const char       emin = (char)(min - 1);
		const char       emax = (char)(max + 1);
		const StateIndex low  = add_state(states, range_state(cmin, emin, end));
		const StateIndex high = add_state(states, range_state(emax, cmax, end));
		const StateIndex fork = add_state(states, split_state(low, high));

		*out = make_automata(fork, end);
	} else {
		const StateIndex start = add_state(states, range_state(min, max, end));

		*out = make_automata(start, end);
	}

	return st;
}

// Set ::= Range | Range Set
static RerexStatus
read_set(Input* const input, StateArray* const states, Automata* const out)
{
	RerexStatus st      = REREX_SUCCESS;
	bool        negated = false;

	if (peek(input) == '^') {
		eat(input);
		negated = true;
	}

	Automata nfa = {NO_STATE, NO_STATE};
	if ((st = read_range(input, states, negated, &nfa))) {
		return st;
	}

	while (peek(input) != ']') {
		Automata range_nfa = {NO_STATE, NO_STATE};
		if ((st = read_range(input, states, negated, &range_nfa))) {
			return st;
		}

		nfa = alternate(states, nfa, range_nfa);
	}

	*out = nfa;
	return st;
}

// Atom ::= CHAR | DOT | '(' Expr ')' | '[' Set ']'
static RerexStatus
read_atom(Input* const input, StateArray* const states, Automata* const out)
{
	RerexStatus st = REREX_SUCCESS;

	switch (peek(input)) {
	case '(':
		eat(input);
		if ((st = read_expr(input, states, out))) {
			return st;
		}

		if (peek(input) != ')') {
			return REREX_EXPECTED_RPAREN;
		}

		eat(input);
		return st;

	case '.':
		return read_dot(input, states, out);

	case '[':
		eat(input);
		if ((st = read_set(input, states, out))) {
			return st;
		}

		eat(input);
		return st;

	default:
		break;
	}

	char c = 0;
	if ((st = read_char(input, &c))) {
		return st;
	}

	const StateIndex end   = add_state(states, match_state());
	const StateIndex start = add_state(states, range_state(c, c, end));

	*out = make_automata(start, end);

	return st;
}

// OPERATOR ::= '*' | '+' | '?'
// Factor   ::= Atom | Atom OPERATOR
static RerexStatus
read_factor(Input* const input, StateArray* const states, Automata* const out)
{
	RerexStatus st       = REREX_SUCCESS;
	Automata    atom_nfa = {NO_STATE, NO_STATE};

	if (!(st = read_atom(input, states, &atom_nfa))) {
		const char c = peek(input);
		switch (c) {
		case '*':
			eat(input);
			*out = star(states, atom_nfa);
			break;
		case '+':
			eat(input);
			*out = plus(states, atom_nfa);
			break;
		case '?':
			eat(input);
			*out = question(states, atom_nfa);
			break;
		default:
			*out = atom_nfa;
			break;
		}
	}

	return st;
}

// Term ::= Factor | Factor Term
static RerexStatus
read_term(Input* const input, StateArray* const states, Automata* const out)
{
	RerexStatus st         = REREX_SUCCESS;
	Automata    factor_nfa = {NO_STATE, NO_STATE};
	Automata    term_nfa   = {NO_STATE, NO_STATE};

	if (!(st = read_factor(input, states, &factor_nfa))) {
		switch (peek(input)) {
		case '\0':
		case ')':
		case '|':
			*out = factor_nfa;
			break;
		default:
			if (!(st = read_term(input, states, &term_nfa))) {
				*out = concatenate(states, factor_nfa, term_nfa);
			}
			break;
		}
	}

	return st;
}

// Expr ::= Term | Term '|' Expr
static RerexStatus
read_expr(Input* const input, StateArray* const states, Automata* const out)
{
	RerexStatus st       = REREX_SUCCESS;
	Automata    term_nfa = {NO_STATE, NO_STATE};
	Automata    expr_nfa = {NO_STATE, NO_STATE};

	if ((st = read_term(input, states, &term_nfa))) {
		return st;
	}

	if (peek(input) == '|') {
		eat(input);
		if (!(st = read_expr(input, states, &expr_nfa))) {
			*out = alternate(states, term_nfa, expr_nfa);
		}
	} else {
		*out = term_nfa;
	}

	return st;
}

/* Pattern */

struct RerexPatternImpl {
	StateArray states;
	StateIndex start;
};

// Allocate a new regular expression, which takes ownership of all states
static RerexPattern*
new_regexp(StateArray* const states, const Automata nfa)
{
	RerexPattern* const regexp = (RerexPattern*)malloc(sizeof(RerexPattern));

	regexp->states = *states;
	regexp->start  = nfa.start;
	return regexp;
}

void
rerex_free_pattern(RerexPattern* const regexp)
{
	free(regexp->states.states);
	free(regexp);
}

RerexStatus
rerex_compile(const char* const    pattern,
              size_t* const        end,
              RerexPattern** const out)
{
	Input      input  = {pattern, 0};
	Automata   nfa    = {NO_STATE, NO_STATE};
	StateArray states = {NULL, 0};

	// Add null state so that no actual state has NO_STATE as an ID
	add_state(&states, split_state(NO_STATE, NO_STATE));

	const RerexStatus st = read_expr(&input, &states, &nfa);
	if (st) {
		free(states.states);
	} else {
		*out = new_regexp(&states, nfa);
	}

	*end = input.offset;
	return st;
}

/* Matcher */

typedef struct {
	StateIndex* indices;
	size_t      n_indices;
} IndexList;

struct RerexMatcherImpl {
	const RerexPattern* regexp;
	IndexList           lists[2];
	size_t*             last_lists;
};

RerexMatcher*
rerex_new_matcher(const RerexPattern* const regexp)
{
	const size_t n_states = regexp->states.n_states;

	IndexList list0 = {(StateIndex*)calloc(n_states, sizeof(StateIndex)), 0};
	IndexList list1 = {(StateIndex*)calloc(n_states, sizeof(StateIndex)), 0};
	size_t*   lists = (size_t*)calloc(n_states, sizeof(size_t));

	const RerexMatcher matcher = {regexp, {list0, list1}, lists};

	RerexMatcher* const m = (RerexMatcher*)calloc(1, sizeof(RerexMatcher));

	*m = matcher;
	return m;
}

void
rerex_free_matcher(RerexMatcher* matcher)
{
	free(matcher->last_lists);
	free(matcher->lists[1].indices);
	free(matcher->lists[0].indices);
	free(matcher);
}

// Add `s` and any epsilon successors to the list
static void
enter_state(RerexMatcher* const matcher,
            const size_t        step,
            IndexList* const    list,
            const StateIndex    s)
{
	const StateArray* const states = &matcher->regexp->states;

	if (s && matcher->last_lists[s] != step) {
		matcher->last_lists[s] = step;

		const State* const state = &states->states[s];
		if (state->min == REREX_SPLIT) {
			enter_state(matcher, step, list, state->next1);
			enter_state(matcher, step, list, state->next2);
		} else {
			list->indices[list->n_indices++] = s;
		}
	}
}

// Run `matcher` and return true if `string` matches the expression
bool
rerex_match(RerexMatcher* const matcher, const char* const string)
{
	const StateArray* const states = &matcher->regexp->states;

	// Reset matcher to a consistent initial state
	matcher->lists[0].n_indices = 0;
	matcher->lists[1].n_indices = 0;
	for (size_t i = 0; i < states->n_states; ++i) {
		matcher->last_lists[i] = UINT32_MAX;
	}

	// Enter start state
	enter_state(matcher, 0, &matcher->lists[0], matcher->regexp->start);

	// Tick the matcher for every input character
	bool phase = 0;
	for (size_t i = 0; string[i]; ++i) {
		const char       c         = string[i];
		IndexList* const list      = &matcher->lists[phase];
		IndexList* const next_list = &matcher->lists[!phase];

		// Add successor states to the next iteration's list
		next_list->n_indices = 0;
		for (size_t j = 0; j < list->n_indices; ++j) {
			const State* const state = &states->states[list->indices[j]];
			if (state->min <= c && c <= state->max) {
				enter_state(matcher, i + 1, next_list, state->next1);
			}
		}

		// Flip phase to swap lists
		phase = !phase;
	}

	// Check if match state is entered in the end
	IndexList* const list = &matcher->lists[phase];
	for (size_t i = 0; i < list->n_indices; ++i) {
		if (states->states[list->indices[i]].min == REREX_MATCH) {
			return true;
		}
	}

	return false;
}
