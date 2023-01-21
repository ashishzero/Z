#pragma once
#include "Lexer.h"

#ifdef BUILD_DEBUG
//#define PARSER_DUMP_TOKENS
#define PARSER_DUMP_EXPR
#endif

typedef enum Expr_Type_Id {
	Expr_Type_Id_INTEGER,

	Expr_Type_Id_COUNT
} Expr_Type_Id;

typedef struct Expr_Type {
	Expr_Type_Id id;
	u32          runtime_size;
} Expr_Type;

enum Expr_Type_Integer_Flags {
	EXPR_TYPE_INTEGER_IS_SIGNED = 0x1
};

typedef struct Expr_Type_Integer {
	Expr_Type base;
	u32       flags;
} Expr_Type_Integer;

//
//
//

typedef enum Expr_Kind {
	Expr_Kind_Literal,
	Expr_Kind_Identifier,
	Expr_Kind_Unary_Operator,
	Expr_Kind_Binary_Operator,
	Expr_Kind_Assignment,

	Expr_Kind_COUNT
} Expr_Kind;

typedef struct Expr {
	Expr_Kind   kind;
	Expr_Type * type;
	Token_Range range;
} Expr;

typedef struct Expr_Literal {
	Expr        base;
	Token_Value value;
} Expr_Literal;

typedef struct Expr_Identifier {
	Expr   base;
	String name;
} Expr_Identifier;

typedef struct Expr_Unary_Operator {
	Expr  base;
	Expr *child;
	u32   symbol;
} Expr_Unary_Operator;

typedef struct Expr_Binary_Operator {
	Expr  base;
	Expr *left;
	Expr *right;
	u32   symbol;
} Expr_Binary_Operator;

typedef struct Expr_Assignment {
	Expr  base;
	Expr *left;
	Expr *right;
} Expr_Assignment;

//
//
//

typedef struct Parser {
	Lexer   lexer;
	Token   lookup[4];
	M_Pool *pool;
	String  source;
} Parser;

void  Info(Parser *parser, Token_Range range, const char *fmt, ...);
void  Warning(Parser *parser, Token_Range range, const char *fmt, ...);
void  Error(Parser *parser, Token_Range range, const char *fmt, ...);
void  Fatal(Parser *parser, Token_Range range, const char *fmt, ...);

Expr *Parse(String stream, String source, M_Pool *pool);
