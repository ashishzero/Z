#include "Lexer.h"

#include <string.h>
#include <stdlib.h>

static const char *TokenKindNames[] = {
	"Integer", "Plus", "Minus", "Multiply", "Divide", "BracketOpen", "BracketClose"
};

static_assert(ArrayCount(TokenKindNames) == Token_Kind_END, "");

static int UTF8Advance(u8 *beg, u8 *end) {
	u32 codepoint = *beg;

	int advance;
	if ((codepoint & 0x80) == 0x00) {
		advance = 1;
	} else if ((codepoint & 0xe0) == 0xc0) {
		advance = 2;
	} else if ((codepoint & 0xf0) == 0xe0) {
		advance = 3;
	} else if ((codepoint & 0xF8) == 0xf0) {
		advance = 4;
	}

	if (beg + advance < end) {
		end = beg + advance;
	}

	// Validate and set actual advance
	advance = 1;
	u8 *pos = beg + 1;
	for (; pos < end; ++pos) {
		if (((*pos) & 0xc0) != 0x80)
			break; // invalid continuation character
		advance += 1;
	}

	return advance;
}

typedef enum Lex_State {
	Lex_State_ERROR,
	Lex_State_EMPTY,
	Lex_State_INTEGER,
	Lex_State_PLUS,
	Lex_State_MINUS,
	Lex_State_MULTIPLY,
	Lex_State_DIVIDE,
	Lex_State_BRACKET_OPEN,
	Lex_State_BRACKET_CLOSE,

	Lex_State_COUNT,
} Lex_State;

typedef enum Lex_Value {
	Lex_Value_NULL,
	Lex_Value_INTEGER,
	Lex_Value_SYMBOL,

	Lex_Value_COUNT
} Lex_Value;

static_assert(Lex_State_COUNT <= 256, "");
static_assert(Lex_Value_COUNT <= 256, "");

static u8   TransitionTable[Lex_State_COUNT][255];
static uint TransitionOutput[Lex_State_COUNT][Lex_State_COUNT];
static u8   TransitionValue[Lex_State_COUNT];

enum {
	Token_Kind_EMPTY = Token_Kind_END + 1
};

void LexInitTable() {
	const u8 WhiteSpaces[] = " \t\n\r\v\f";

	for (int i = 0; i < Lex_State_COUNT; ++i) {
		for (int j = 0; j < Lex_State_COUNT; ++j) {
			TransitionOutput[i][j] = Token_Kind_EMPTY;
		}
	}

	for (int i = 0; i < Lex_State_COUNT; ++i) {
		for (int j = 0; j < ArrayCount(WhiteSpaces); ++j) {
			TransitionTable[i][WhiteSpaces[j]] = Lex_State_EMPTY;
		}

		for (int j = '0'; j <= '9'; ++j) {
			TransitionTable[i][j] = Lex_State_INTEGER;
		}

		TransitionTable[i]['+'] = Lex_State_PLUS;
		TransitionTable[i]['-'] = Lex_State_MINUS;
		TransitionTable[i]['*'] = Lex_State_MULTIPLY;
		TransitionTable[i]['/'] = Lex_State_DIVIDE;
		TransitionTable[i]['('] = Lex_State_BRACKET_OPEN;
		TransitionTable[i][')'] = Lex_State_BRACKET_CLOSE;

		TransitionOutput[i][Lex_State_ERROR]         = Token_Kind_END;

		TransitionOutput[Lex_State_INTEGER][i]       = Token_Kind_INTEGER;

		TransitionOutput[Lex_State_PLUS][i]          = Token_Kind_PLUS;
		TransitionOutput[Lex_State_MINUS][i]         = Token_Kind_MINUS;
		TransitionOutput[Lex_State_MULTIPLY][i]      = Token_Kind_MULTIPLY;
		TransitionOutput[Lex_State_DIVIDE][i]        = Token_Kind_DIVIDE;
		TransitionOutput[Lex_State_BRACKET_OPEN][i]  = Token_Kind_BRACKET_OPEN;
		TransitionOutput[Lex_State_BRACKET_CLOSE][i] = Token_Kind_BRACKET_CLOSE;
	}

	TransitionOutput[Lex_State_EMPTY][Lex_State_EMPTY] = Token_Kind_END;

	TransitionValue[Lex_State_INTEGER]       = Lex_Value_INTEGER;
	TransitionValue[Lex_State_PLUS]          = Lex_Value_SYMBOL;
	TransitionValue[Lex_State_MINUS]         = Lex_Value_SYMBOL;
	TransitionValue[Lex_State_MULTIPLY]      = Lex_Value_SYMBOL;
	TransitionValue[Lex_State_DIVIDE]        = Lex_Value_SYMBOL;
	TransitionValue[Lex_State_BRACKET_OPEN]  = Lex_Value_SYMBOL;
	TransitionValue[Lex_State_BRACKET_CLOSE] = Lex_Value_SYMBOL;
}

void LexInit(Lexer *l, String input, M_Pool *pool) {
	l->first    = input.data;
	l->last     = input.data + input.count;
	l->cursor   = l->first;
	l->pool     = pool;
	l->error[0] = 0;
}

static void LexError(Lexer *l, const char *fmt, ...) {
	va_list args;
	va_start(args, fmt);
	vsnprintf(l->error, sizeof(l->error), fmt, args);
	va_end(args);
}

bool LexNext(Lexer *l, Token *token) {
	Lex_State  curr = Lex_State_EMPTY;
	Lex_State  next = Lex_State_EMPTY;
	Token_Kind out  = Token_Kind_END;

	u8 *beg = l->cursor;

	// Trim whitespaces (optimization)
	for (; beg < l->last; ++beg) {
		u8 ch = *beg;
		curr  = TransitionTable[curr][ch];

		if (curr != Lex_State_EMPTY)
			break;
	}

	u8 *end = beg + 1;

	// Find token
	for (; end < l->last; ++end) {
		u8 ch = *end;
		next  = TransitionTable[curr][ch];

		if (curr != next) {
			if (TransitionOutput[curr][next] != Token_Kind_EMPTY)
				break;
			curr = next;
			beg  = end;
		}
	}

	l->cursor    = end;

	token->kind  = TransitionOutput[curr][next];
	token->range = (Token_Range){ beg - l->first, end - l->first };

	memset(&token->value, 0, sizeof(token->value));

	if (next == Lex_State_ERROR) {
		int advance = UTF8Advance(l->cursor, l->last);
		l->cursor += advance;
		LexError(l, "bad character: \"%.*s\"", advance, l->cursor);
		return false;
	}

	Lex_Value out_value = TransitionValue[curr];

	if (out_value == Lex_Value_INTEGER) {
		u8 *start = beg;

		while (start < end && *start == '0') {
			start += 1;
		}

		int count = (int)(end - start);

		if (count > 255) {
			token->kind = Token_Kind_END;
			LexError(l, "integer literal is too big");
			return false;
		}

		char buff[256];
		memcpy(buff, start, count);
		buff[count] = 0;

		char *endptr = nullptr;
		u64 value    = strtoull(buff, &endptr, 10);

		if (endptr != buff + count || errno == ERANGE) {
			token->kind = Token_Kind_END;
			LexError(l, "integer literal is too big");
			return false;
		}

		token->value.integer = value;

		return true;
	}

	if (out_value == Lex_Value_SYMBOL) {
		Assert(token->range.to - token->range.from == 1);
		token->value.symbol = *beg;
		return true;
	}

	return true;
}

void LexDump(FILE *out, const Token *token) {
	const char *name = TokenKindNames[token->kind];
	fprintf(out, ".%s", name);

	if (token->kind == Token_Kind_INTEGER) {
		fprintf(out, "(%zu) ", token->value.integer);
	} else if (token->kind == Token_Kind_PLUS || token->kind == Token_Kind_MINUS ||
		token->kind == Token_Kind_MULTIPLY || token->kind == Token_Kind_MULTIPLY) {
		fprintf(out, "(%c) ", (char)token->value.symbol);
	}
}
