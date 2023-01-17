#include "Lexer.h"

#include <string.h>
#include <stdlib.h>

static const char *TokenKindNames[] = {
	"Integer", "Plus", "Minus", "Multiply", "Divide"
};

static_assert(ArrayCount(TokenKindNames) == Token_Kind_END, "");

static void LexWhiteSpace(Lexer *l) {
	u8 *pos = l->cursor;
	for (; pos < l->last; ++pos) {
		u8 ch = *pos;
		if (ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r' || ch == '\v' || ch == '\f') {
			continue;
		} else {
			break;
		}
	}
	l->cursor = pos;
}

static void LexToken(Lexer *l, Token *token, Token_Kind kind, u8 *pos) {
	token->kind       = kind;
	token->range.from = l->cursor - l->first;
	token->range.to   = pos - l->first;
	l->cursor         = pos;
}

static void LexError(Lexer *l, Token *token, u8 *pos, const char *fmt, ...) {
	va_list args;
	va_start(args, fmt);
	vsnprintf(l->error, sizeof(l->error), fmt, args);
	va_end(args);

	LexToken(l, token, Token_Kind_END, pos);
}

void LexInit(Lexer *l, String input) {
	l->first    = input.data;
	l->last     = input.data + input.count;
	l->cursor   = l->first;
	l->error[0] = 0;
}

static const u8 CharacterTokenValues[]    = { '+', '-', '*', '/' };
static const Token_Kind CharacterTokens[] = { Token_Kind_PLUS, Token_Kind_MINUS, Token_Kind_MULTIPLY, Token_Kind_DIVIDE };

static_assert(ArrayCount(CharacterTokens) == ArrayCount(CharacterTokenValues), "");

bool LexNext(Lexer *l, Token *token) {
	LexWhiteSpace(l);

	if (l->cursor >= l->last) {
		token->kind       = Token_Kind_END;
		token->range.from = l->last - l->first;
		token->range.to   = token->range.from;
		memset(&token->value, 0, sizeof(token->value));
		return true;
	}

	u8 match = *l->cursor;

	for (umem index = 0; index < ArrayCount(CharacterTokenValues); ++index) {
		if (match == CharacterTokenValues[index]) {
			Token_Kind kind = CharacterTokens[index];
			LexToken(l, token, kind, l->cursor + 1);
			token->value.symbol = match;
			return true;
		}
	}

	if (match >= '0' && match <= '9') {
		u8 * start = l->cursor;
		u8 * pos   = l->cursor;
		umem count = 0;

		for (; pos < l->last; ++pos, ++count) {
			u8 ch = *pos;

			if (ch >= '0' && ch <= '9') {
				continue;
			} else {
				break;
			}
		}

		while (count > 1 && *start == '0') {
			count -= 1;
			start += 1;
		}

		if (count > 255) {
			token->kind = Token_Kind_END;
			LexError(l, token, pos, "integer literal is too big");
			return false;
		}

		char buff[256];
		memcpy(buff, start, count);
		buff[count] = 0;

		char *endptr = nullptr;
		u64 value    = strtoull(buff, &endptr, 10);

		if (endptr != buff + count || errno == ERANGE) {
			token->kind = Token_Kind_END;
			LexError(l, token, pos, "integer literal is too big");
			return false;
		}

		LexToken(l, token, Token_Kind_INTEGER, pos);
		token->value.integer = value;

		return true;
	}

	// Advance UTF-8

	u32 codepoint = *l->cursor;

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

	if (l->cursor + advance >= l->last) {
		advance = (int)(l->last - l->cursor);
	}

	u8 *pos = l->cursor + 1;

	for (int i = 1; i < advance; ++i) {
		if ((pos[i] & 0xc0) == 0x80) {
			pos += 1;
		}
	}

	LexError(l, token, pos, "bad character: \"%.*s\"", advance, l->cursor);

	return false;
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
