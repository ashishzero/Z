#pragma once
#include "Common.h"

#include <stdio.h>

typedef enum Token_Kind {
	Token_Kind_INTEGER,
	Token_Kind_PLUS,
	Token_Kind_MINUS,
	Token_Kind_MULTIPLY,
	Token_Kind_DIVIDE,
	Token_Kind_BRACKET_OPEN,
	Token_Kind_BRACKET_CLOSE,

	Token_Kind_END,
} Token_Kind;

typedef struct Token_Range {
	umem from;
	umem to;
} Token_Range;

typedef union Token_Value {
	u32    symbol;
	u64    integer;
	r64    floating;
	String string;
} Token_Value;

typedef struct Token {
	Token_Kind  kind;
	Token_Range range;
	Token_Value value;
} Token;

typedef struct Lexer {
	u8 *   cursor;
	u8 *   last;
	u8 *   first;
	char   error[1024];
} Lexer;

void LexInit(Lexer *l, String input);
bool LexNext(Lexer *l, Token *token);
void LexDump(FILE *out, const Token *token);
