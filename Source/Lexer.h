#pragma once
#include "Platform.h"
#include "Pool.h"

#include <stdio.h>

typedef enum Token_Kind {
	Token_Kind_True,
	Token_Kind_False,
	Token_Kind_Integer,
	Token_Kind_Plus,
	Token_Kind_Minus,
	Token_Kind_Multiply,
	Token_Kind_Divide,
	Token_Kind_Bracket_Open,
	Token_Kind_Bracket_Close,
	Token_Kind_Equals,
	Token_Kind_Identifier,

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
	u8 *    cursor;
	u8 *    last;
	u8 *    first;
	M_Pool *pool;
	char    error[1024];
} Lexer;

void LexInitTable(void);
void LexInit(Lexer *l, String input, M_Pool *pool);
bool LexNext(Lexer *l, Token *token);
void LexDump(FILE *out, const Token *token);
