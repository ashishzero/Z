#pragma once
#include "Common.h"

#include <stdio.h>

enum Token_Kind {
	Token_Kind_INTEGER,
	Token_Kind_PLUS,
	Token_Kind_MINUS,
	Token_Kind_MULTIPLY,
	Token_Kind_DIVIDE,

	Token_Kind_END,
};

struct Token_Range {
	umem from;
	umem to;
};

union Token_Value {
	u32 symbol;
	u64 integer;
	r64 floating;

	struct {
		u8 * data;
		umem length;
	} string;
};

struct Token {
	Token_Kind  kind;
	Token_Range range;
	Token_Value value;
};

struct Lexer {
	u8 *   cursor;
	u8 *   last;
	u8 *   first;
	char   error[1024];
};

void LexInit(Lexer *l, String input);
bool LexNext(Lexer *l, Token *token);
void LexDump(FILE *out, const Token &token);
