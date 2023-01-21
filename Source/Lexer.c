#include "Lexer.h"

#include <string.h>
#include <stdlib.h>

static const char *TokenKindNames[] = {
	"?", "", "True", "False", "Integer", "Plus", "Minus", "Multiply", "Divide", "BracketOpen", "BracketClose", "Equals", "Identifier"
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

typedef enum Lex_Value {
	Lex_Value_NULL,
	Lex_Value_INTEGER,
	Lex_Value_SYMBOL,
	Lex_Value_STRING,
} Lex_Value;

static_assert(Token_Kind_END <= 256, "");

static u8 TransitionTable[Token_Kind_END][255];
static u8 TransitionValue[Token_Kind_END];

void LexInitTable(void) {
	const u8 WhiteSpaces[] = " \t\n\r\v\f";

	for (int i = 0; i < Token_Kind_END; ++i) {
		for (int j = 0; j < ArrayCount(WhiteSpaces); ++j) {
			TransitionTable[i][WhiteSpaces[j]] = Token_Kind_EMPTY;
		}

		TransitionTable[i]['+'] = Token_Kind_PLUS;
		TransitionTable[i]['-'] = Token_Kind_MINUS;
		TransitionTable[i]['*'] = Token_Kind_MULTIPLY;
		TransitionTable[i]['/'] = Token_Kind_DIVIDE;
		TransitionTable[i]['('] = Token_Kind_BRACKET_OPEN;
		TransitionTable[i][')'] = Token_Kind_BRACKET_CLOSE;
		TransitionTable[i]['='] = Token_Kind_EQUALS;
	}

	for (int i = '0'; i <= '9'; ++i) {
		TransitionTable[Token_Kind_EMPTY][i]         = Token_Kind_INTEGER;
		TransitionTable[Token_Kind_INTEGER][i]       = Token_Kind_INTEGER;
		TransitionTable[Token_Kind_PLUS][i]          = Token_Kind_INTEGER;
		TransitionTable[Token_Kind_MULTIPLY][i]      = Token_Kind_INTEGER;
		TransitionTable[Token_Kind_DIVIDE][i]        = Token_Kind_INTEGER;
		TransitionTable[Token_Kind_BRACKET_OPEN][i]  = Token_Kind_INTEGER;
		TransitionTable[Token_Kind_BRACKET_CLOSE][i] = Token_Kind_INTEGER;
		TransitionTable[Token_Kind_IDENTIFIER][i]    = Token_Kind_IDENTIFIER;
	}

	TransitionTable[Token_Kind_IDENTIFIER]['_'] = Token_Kind_IDENTIFIER;

	for (int i = 'a'; i <= 'z'; ++i) {
		TransitionTable[Token_Kind_IDENTIFIER][i] = Token_Kind_IDENTIFIER;
		TransitionTable[Token_Kind_EMPTY][i]      = Token_Kind_IDENTIFIER;
	}

	for (int i = 'A'; i <= 'Z'; ++i) {
		TransitionTable[Token_Kind_IDENTIFIER][i] = Token_Kind_IDENTIFIER;
		TransitionTable[Token_Kind_EMPTY][i]      = Token_Kind_IDENTIFIER;
	}

	TransitionValue[Token_Kind_INTEGER]         = Lex_Value_INTEGER;
	TransitionValue[Token_Kind_PLUS]            = Lex_Value_SYMBOL;
	TransitionValue[Token_Kind_MINUS]           = Lex_Value_SYMBOL;
	TransitionValue[Token_Kind_MULTIPLY]        = Lex_Value_SYMBOL;
	TransitionValue[Token_Kind_DIVIDE]          = Lex_Value_SYMBOL;
	TransitionValue[Token_Kind_BRACKET_OPEN]    = Lex_Value_SYMBOL;
	TransitionValue[Token_Kind_BRACKET_CLOSE]   = Lex_Value_SYMBOL;
	TransitionValue[Token_Kind_EQUALS]          = Lex_Value_SYMBOL;
	TransitionValue[Token_Kind_IDENTIFIER]      = Lex_Value_STRING;
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
	Token_Kind curr = Token_Kind_EMPTY;
	Token_Kind next = Token_Kind_EMPTY;

	u8 *beg = l->cursor;

	// Trim whitespaces (optimization)
	for (; beg < l->last; ++beg) {
		u8 ch = *beg;
		curr  = TransitionTable[curr][ch];

		if (curr != Token_Kind_EMPTY)
			break;
	}

	u8 *end = beg;

	if (curr != Token_Kind_ERROR) {
		// Find token
		end += 1;
		for (; end < l->last; ++end) {
			u8 ch = *end;
			next  = TransitionTable[curr][ch];

			if (curr != next) {
				if (curr != Token_Kind_EMPTY)
					break;
				curr = next;
				beg  = end;
			}
		}
	}

	l->cursor    = end;

	token->kind  = curr;
	token->range = (Token_Range){ beg - l->first, end - l->first };

	memset(&token->value, 0, sizeof(token->value));

	if (curr == Token_Kind_ERROR) {
		int advance = UTF8Advance(l->cursor, l->last);
		LexError(l, "bad character: \"%.*s\"", advance, l->cursor);
		l->cursor += advance;
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

	if (out_value == Lex_Value_STRING) {
		umem count = end - beg;
		u8 * data  = M_PoolPush(l->pool, count, 1, 0);

		memcpy(data, beg, count);
		token->value.string = (String){ .count=count, .data=data };
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
