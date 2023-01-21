#include "Lexer.h"

#include <string.h>
#include <stdlib.h>

static const char *TokenKindNames[] = {
	"True", "False", "Integer", "Plus", "Minus", "Multiply", "Divide", "BracketOpen", "BracketClose", "Equals", "Identifier"
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

typedef enum Lex_Prod {
	Lex_Prod_None,
	Lex_Prod_Reset,
	Lex_Prod_Token,
	Lex_Prod_Integer,
	Lex_Prod_Symbol,
	Lex_Prod_Identifier,
} Lex_Prod;

typedef enum Lex_State {
	Lex_State_Error,
	Lex_State_Whitespace,
	Lex_State_Plus,
	Lex_State_Minus,
	Lex_State_Multiply,
	Lex_State_Divide,
	Lex_State_Bracket_Open,
	Lex_State_Bracket_Close,
	Lex_State_Equals,
	Lex_State_Integer,
	Lex_State_Identifier,
	Lex_State_Identifier_Cont1,
	Lex_State_Identifier_Cont2,
	Lex_State_Identifier_Cont3,

	Lex_State_COUNT
} Lex_State;

static_assert(Lex_State_COUNT <= 256, "");

static Lex_State  TransitionTable[Lex_State_COUNT][255];
static Lex_Prod   ProductionTable[Lex_State_COUNT][Lex_State_COUNT];
static Token_Kind TokenKindMap[Lex_State_COUNT];

static void LexUpdateTransition(const Lex_State *const entries, int count, Lex_State next, u8 ch) {
	for (int i = 0; i < count; ++i) {
		Lex_State entry = entries[i];
		TransitionTable[entry][ch] = next;
	}
}

static void LexUpdateTransitionRange(const Lex_State *const entries, int count, Lex_State next, u8 first, u8 last) {
	for (u8 i = first; i <= last; ++i) {
		LexUpdateTransition(entries, count, next, i);
	}
}

void LexInitTable(void) {
	const u8 Whitespaces[] = " \t\n\r\v\f";

	// Single Byte Tokens
	for (int i = 0; i < Lex_State_COUNT; ++i) {
		for (int j = 0; j < ArrayCount(Whitespaces); ++j) {
			TransitionTable[i][Whitespaces[j]] = Lex_State_Whitespace;
		}

		TransitionTable[i]['+'] = Lex_State_Plus;
		TransitionTable[i]['-'] = Lex_State_Minus;
		TransitionTable[i]['*'] = Lex_State_Multiply;
		TransitionTable[i]['/'] = Lex_State_Divide;
		TransitionTable[i]['('] = Lex_State_Bracket_Open;
		TransitionTable[i][')'] = Lex_State_Bracket_Close;
		TransitionTable[i]['='] = Lex_State_Equals;
	}

	const Lex_State IntegerEntries[] = {
		Lex_State_Plus, Lex_State_Minus, Lex_State_Multiply, Lex_State_Divide, 
		Lex_State_Bracket_Open, Lex_State_Bracket_Close,
		Lex_State_Whitespace, Lex_State_Integer
	};

	LexUpdateTransitionRange(IntegerEntries, ArrayCount(IntegerEntries), Lex_State_Integer, '0', '9');

	const Lex_State IdentifierEntries[] = {
		Lex_State_Plus, Lex_State_Minus, Lex_State_Multiply, Lex_State_Divide,
		Lex_State_Bracket_Open, Lex_State_Bracket_Close, Lex_State_Equals,
		Lex_State_Whitespace, Lex_State_Identifier
	};

	LexUpdateTransition(IdentifierEntries, ArrayCount(IdentifierEntries), Lex_State_Identifier, '_');
	LexUpdateTransitionRange(IdentifierEntries, ArrayCount(IdentifierEntries), Lex_State_Identifier, 'a', 'z');
	LexUpdateTransitionRange(IdentifierEntries, ArrayCount(IdentifierEntries), Lex_State_Identifier, 'A', 'Z');

	const Lex_State IdentifierMids[] = { Lex_State_Identifier };
	LexUpdateTransitionRange(IdentifierMids, ArrayCount(IdentifierMids), Lex_State_Identifier, '0', '9');

	// 2 byte unicode
	LexUpdateTransitionRange(IdentifierEntries, ArrayCount(IdentifierEntries), Lex_State_Identifier_Cont1, 192, 223);

	// 3 bytes unicode
	LexUpdateTransitionRange(IdentifierEntries, ArrayCount(IdentifierEntries), Lex_State_Identifier_Cont2, 224, 239);

	// 4 bytes unicode
	LexUpdateTransitionRange(IdentifierEntries, ArrayCount(IdentifierEntries), Lex_State_Identifier_Cont3, 240, 247);

	// continuation bytes
	const Lex_State IdentifierContEntries[] = { Lex_State_Identifier_Cont1 };
	LexUpdateTransitionRange(IdentifierContEntries, ArrayCount(IdentifierContEntries), Lex_State_Identifier, 128, 191);

	const Lex_State IdentifierCont1Entries[] = { Lex_State_Identifier_Cont2 };
	LexUpdateTransitionRange(IdentifierCont1Entries, ArrayCount(IdentifierCont1Entries), Lex_State_Identifier_Cont1, 128, 191);

	const Lex_State IdentifierCont2Entries[] = { Lex_State_Identifier_Cont3 };
	LexUpdateTransitionRange(IdentifierCont2Entries, ArrayCount(IdentifierCont2Entries), Lex_State_Identifier_Cont2, 128, 191);

	for (int i = 0; i < Lex_State_COUNT; ++i) {
		ProductionTable[i][Lex_State_Error]         = Lex_Prod_Token;
		ProductionTable[Lex_State_Whitespace][i]    = Lex_Prod_Reset;
		ProductionTable[Lex_State_Identifier][i]    = Lex_Prod_Identifier;
		ProductionTable[Lex_State_Integer][i]       = Lex_Prod_Integer;
		ProductionTable[Lex_State_Plus][i]          = Lex_Prod_Symbol;
		ProductionTable[Lex_State_Minus][i]         = Lex_Prod_Symbol;
		ProductionTable[Lex_State_Multiply][i]      = Lex_Prod_Symbol;
		ProductionTable[Lex_State_Divide][i]        = Lex_Prod_Symbol;
		ProductionTable[Lex_State_Bracket_Open][i]  = Lex_Prod_Symbol;
		ProductionTable[Lex_State_Bracket_Close][i] = Lex_Prod_Symbol;
		ProductionTable[Lex_State_Equals][i]        = Lex_Prod_Symbol;
	}

	ProductionTable[Lex_State_Identifier][Lex_State_Identifier]       = Lex_Prod_None;
	ProductionTable[Lex_State_Identifier][Lex_State_Identifier_Cont1] = Lex_Prod_None;
	ProductionTable[Lex_State_Identifier][Lex_State_Identifier_Cont2] = Lex_Prod_None;
	ProductionTable[Lex_State_Identifier][Lex_State_Identifier_Cont3] = Lex_Prod_None;
	ProductionTable[Lex_State_Integer][Lex_State_Integer]             = Lex_Prod_None;

	for (int i = 0; i < Lex_State_COUNT; ++i)
		TokenKindMap[i] = Token_Kind_END;

	TokenKindMap[Lex_State_Plus]          = Token_Kind_Plus;
	TokenKindMap[Lex_State_Minus]         = Token_Kind_Minus;
	TokenKindMap[Lex_State_Multiply]      = Token_Kind_Multiply;
	TokenKindMap[Lex_State_Divide]        = Token_Kind_Divide;
	TokenKindMap[Lex_State_Bracket_Open]  = Token_Kind_Bracket_Open;
	TokenKindMap[Lex_State_Bracket_Close] = Token_Kind_Bracket_Close;
	TokenKindMap[Lex_State_Equals]        = Token_Kind_Equals;
	TokenKindMap[Lex_State_Integer]       = Token_Kind_Integer;
	TokenKindMap[Lex_State_Identifier]    = Token_Kind_Identifier;
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
	Lex_State curr = Lex_State_Whitespace;
	Lex_Prod  prod = Lex_Prod_None;

	u8 *beg = l->cursor;
	u8 *end = beg;

	for (; end < l->last; ++end) {
		Lex_State next = TransitionTable[curr][*end];

		prod = ProductionTable[curr][next];

		if (prod > Lex_Prod_Reset) {
			break;
		}

		if (prod == Lex_Prod_Reset)
			beg = end;

		curr = next;
	}

	l->cursor    = end;

	token->kind  = TokenKindMap[curr];
	token->range = (Token_Range){ beg - l->first, end - l->first };

	memset(&token->value, 0, sizeof(token->value));

	if (curr == Lex_State_Error) {
		int advance = UTF8Advance(l->cursor, l->last);
		LexError(l, "bad character: \"%.*s\"", advance, l->cursor);
		l->cursor += advance;
		return false;
	}

	if (prod == Lex_Prod_Integer) {
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

	if (prod == Lex_Prod_Symbol) {
		Assert(token->range.to - token->range.from == 1);
		token->value.symbol = *beg;
		return true;
	}

	if (prod == Lex_Prod_Identifier) {
		umem count = end - beg;
		u8 * data  = M_PoolPush(l->pool, count + 1, 1, 0);

		memcpy(data, beg, count);
		data[count] = 0;

		token->value.string = (String){ .count=count, .data=data };
		return true;
	}

	return true;
}

void LexDump(FILE *out, const Token *token) {
	const char *name = TokenKindNames[token->kind];
	fprintf(out, ".%s ", name);

	switch (token->kind) {
	case Token_Kind_Integer:
		fprintf(out, "%zu", token->value.integer);
		break;
	case Token_Kind_Identifier:
		fprintf(out, StrFmt, StrArg(token->value.string));
		break;
	case Token_Kind_Plus:
	case Token_Kind_Minus:
	case Token_Kind_Multiply:
	case Token_Kind_Divide:
		fprintf(out, "%c", (char)token->value.symbol);
		break;
	case Token_Kind_Bracket_Open:
		fprintf(out, "(");
		break;
	case Token_Kind_Bracket_Close:
		fprintf(out, ")");
		break;
	case Token_Kind_Equals:
		fprintf(out, "=");
		break;
	}

	fprintf(out, "\n");
}
