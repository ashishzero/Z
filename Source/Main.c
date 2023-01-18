#include "Platform.h"
#include "Lexer.h"
#include "Pool.h"

#include <stdlib.h>

#define PARSER_MAX_LOOKUP 4

#ifdef BUILD_DEBUG
//#define PARSER_DUMP_TOKENS
#define PARSER_DUMP_EXPR
#endif

typedef enum Expr_Kind {
	Expr_Kind_Literal,
	Expr_Kind_Unary_Operator,
	Expr_Kind_Binary_Operator,

	Expr_Kind_COUNT
} Expr_Kind;

static const char *ExprKindNames[] = {
	"Literal", "Unary Operator", "Binary Operator"
};
static_assert(ArrayCount(ExprKindNames) == Expr_Kind_COUNT, "");

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

static Expr_Type_Integer ExprBuiltinUnsigned8  = { { Expr_Type_Id_INTEGER, 1 }, 0 };
static Expr_Type_Integer ExprBuiltinUnsigned16 = { { Expr_Type_Id_INTEGER, 2 }, 0 };
static Expr_Type_Integer ExprBuiltinUnsigned32 = { { Expr_Type_Id_INTEGER, 4 }, 0 };
static Expr_Type_Integer ExprBuiltinUnsigned64 = { { Expr_Type_Id_INTEGER, 8 }, 0 };
static Expr_Type_Integer ExprBuiltinSigned8    = { { Expr_Type_Id_INTEGER, 1 }, EXPR_TYPE_INTEGER_IS_SIGNED };
static Expr_Type_Integer ExprBuiltinSigned16   = { { Expr_Type_Id_INTEGER, 2 }, EXPR_TYPE_INTEGER_IS_SIGNED };
static Expr_Type_Integer ExprBuiltinSigned32   = { { Expr_Type_Id_INTEGER, 4 }, EXPR_TYPE_INTEGER_IS_SIGNED };
static Expr_Type_Integer ExprBuiltinSigned64   = { { Expr_Type_Id_INTEGER, 8 }, EXPR_TYPE_INTEGER_IS_SIGNED };

typedef struct Expr {
	Expr_Kind   kind;
	Expr_Type * type;
	Token_Range range;
} Expr;

typedef struct Expr_Literal {
	Expr        base;
	Token_Value value;
} Expr_Literal;

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

typedef struct Parser {
	Lexer  lexer;
	Token  lookup[PARSER_MAX_LOOKUP];
	M_Pool pool;
	String source;
} Parser;

Expr *ExprAllocate(Parser *parser, umem size, Expr_Kind kind, Token_Range range) {
	const u32 alignment = _Alignof(Expr);

	Expr *expr  = M_PoolPush(&parser->pool, size, alignment, M_CLEAR_MEMORY);
	expr->kind  = kind;
	expr->range = range;

	return expr;
}

typedef enum Log_Kind {
	Log_Kind_INFO,
	Log_Kind_WARNING,
	Log_Kind_ERROR,
	Log_Kind_FATAL
} Log_Kind;

void Log(Parser *parser, umem pos_0, umem pos_1, FILE *out, Log_Kind kind, const char *fmt, va_list args) {
	umem r = 1, c = 0;

	u8 *data = parser->lexer.first;
	for (umem pos = 0; pos < pos_0; ++pos) {
		if (data[pos] != '\n') {
			c += 1;
		} else {
			c = 0;
			r += 1;
		}
	}

	static const char *LogKindNames[] = { "info", "warning", "error", "error" };

	fprintf(out, StrFmt "(%zu,%zu): %s: ", StrArg(parser->source), r, c, LogKindNames[kind]);
	vfprintf(out, fmt, args);
	fprintf(out, "\n");

	if (kind == Log_Kind_FATAL) {
		exit(1);
	}
}

void Info(Parser *parser, Token_Range range, const char *fmt, ...) {
	va_list args;
	va_start(args, fmt);
	Log(parser, range.from, range.to, stdout, Log_Kind_INFO, fmt, args);
	va_end(args);
}

void Warning(Parser *parser, Token_Range range, const char *fmt, ...) {
	va_list args;
	va_start(args, fmt);
	Log(parser, range.from, range.to, stderr, Log_Kind_WARNING, fmt, args);
	va_end(args);
}

void Error(Parser *parser, Token_Range range, const char *fmt, ...) {
	va_list args;
	va_start(args, fmt);
	Log(parser, range.from, range.to, stderr, Log_Kind_ERROR, fmt, args);
	va_end(args);
}

void Fatal(Parser *parser, Token_Range range, const char *fmt, ...) {
	va_list args;
	va_start(args, fmt);
	Log(parser, range.from, range.to, stderr, Log_Kind_FATAL, fmt, args);
	va_end(args);
}

#define AllocateExpr(parser, type, range) (Expr_##type *)ExprAllocate(parser, sizeof(Expr_##type), Expr_Kind_##type, range)

Token PeekToken(Parser *parser, uint index) {
	Assert(index <= PARSER_MAX_LOOKUP);
	return parser->lookup[index];
}

void AdvanceTokenHelper(Parser *parser) {
	for (uint i = 0; i < PARSER_MAX_LOOKUP - 1; ++i) {
		parser->lookup[i] = parser->lookup[i + 1];
	}

	Token *token = &parser->lookup[PARSER_MAX_LOOKUP - 1];
	if (!LexNext(&parser->lexer, token)) {
		Fatal(parser, token->range, parser->lexer.error);
	}
}

void AdvanceToken(Parser *parser) {
#ifdef PARSER_DUMP_TOKENS
	fprintf(stdout, "T");
	LexDump(stdout, &parser->lookup[0]);
#endif

	AdvanceTokenHelper(parser);
}

Token NextToken(Parser *parser) {
	Token result = parser->lookup[0];
	AdvanceToken(parser);
	return result;
}

Expr *ParseExpression(Parser *parser, int prev_prec);

Expr *ParseTerm(Parser *parser) {
	Token token = NextToken(parser);

	if (token.kind == Token_Kind_INTEGER) {
		Expr_Literal *expr = AllocateExpr(parser, Literal, token.range);
		expr->value        = token.value;
		expr->base.type    = &ExprBuiltinUnsigned64.base;
		return &expr->base;
	}

	if (token.kind == Token_Kind_PLUS || token.kind == Token_Kind_MINUS) {
		Expr_Unary_Operator *expr = AllocateExpr(parser, Unary_Operator, token.range);
		expr->child               = ParseTerm(parser);
		expr->symbol              = token.value.symbol;
		return &expr->base;
	}

	if (token.kind == Token_Kind_BRACKET_OPEN) {
		Expr *expr = ParseExpression(parser, 0);

		token = NextToken(parser);
		if (token.kind != Token_Kind_BRACKET_CLOSE) {
			Fatal(parser, token.range, "expected \")\"");
		}

		return expr;
	}

	Fatal(parser, token.range, "invalid expression");

	return nullptr;
}

static const Token_Kind BinaryOpTokens[] = { Token_Kind_PLUS, Token_Kind_MINUS, Token_Kind_MULTIPLY, Token_Kind_DIVIDE };

static int BinaryOpPrecedence[Token_Kind_END];

Expr *ParseExpression(Parser *parser, int prev_prec) {
	Expr *expr   = ParseTerm(parser);

	for (Token token = PeekToken(parser, 0); 
		token.kind != Token_Kind_END;
		token = PeekToken(parser, 0)) {

		int prec = BinaryOpPrecedence[token.kind];

		if (prec <= prev_prec)
			break;

		for (int index = 0; index < ArrayCount(BinaryOpTokens); ++index) {
			Token_Kind match = BinaryOpTokens[index];
			if (match == token.kind) {
				AdvanceToken(parser);

				Expr_Binary_Operator *op = AllocateExpr(parser, Binary_Operator, token.range);
				op->left                 = expr;
				op->right                = ParseExpression(parser, prec);
				op->symbol               = token.value.symbol;

				expr = &op->base;
				break;
			}
		}
	}

	return expr;
}

void ExprTypeDump(FILE *out, Expr_Type *root) {
	if (!root) return;

	switch (root->id) {
	case Expr_Type_Id_INTEGER:
	{
		Expr_Type_Integer *type = (Expr_Type_Integer *)root;
		fprintf(out, "%c%d", (type->flags & EXPR_TYPE_INTEGER_IS_SIGNED) ? 's' : 'u', type->base.runtime_size << 3);
	} break;
	}
}

void ExprDump(FILE *out, Expr *root, uint indent) {
	for (uint i = 0; i < indent; ++i)
		fprintf(out, "    ");

	const char *name = ExprKindNames[root->kind];
	fprintf(out, ".%s", name);

	switch (root->kind) {
	case Expr_Kind_Literal:
	{
		Expr_Literal *expr = (Expr_Literal *)root;
		fprintf(out, "(%zu) ", expr->value.integer);
		ExprTypeDump(out, root->type);
		fprintf(out, "\n");
	} break;

	case Expr_Kind_Unary_Operator:
	{
		Expr_Unary_Operator *expr = (Expr_Unary_Operator *)root;
		fprintf(out, "(%c) ", (char)expr->symbol);
		ExprTypeDump(out, root->type);
		fprintf(out, "\n");
		ExprDump(out, expr->child, indent + 1);
	} break;

	case Expr_Kind_Binary_Operator:
	{
		Expr_Binary_Operator *expr = (Expr_Binary_Operator *)root;
		fprintf(out, "(%c) ", (char)expr->symbol);
		ExprTypeDump(out, root->type);
		fprintf(out, "\n");
		ExprDump(out, expr->left, indent + 1);
		ExprDump(out, expr->right, indent + 1);
	} break;
	}
}

Expr *ParseStatement(Parser *parser) {
	Expr *expr = ParseExpression(parser, 0);

#ifdef PARSER_DUMP_EXPR
	fprintf(stdout, "\n");
	ExprDump(stdout, expr, 0);
#endif

	return expr;
}

void InitParser() {
	static bool Initialized = false;

	if (Initialized) return;
	Initialized = true;

	BinaryOpPrecedence[Token_Kind_PLUS]     = 10;
	BinaryOpPrecedence[Token_Kind_MINUS]    = 10;
	BinaryOpPrecedence[Token_Kind_MULTIPLY] = 20;
	BinaryOpPrecedence[Token_Kind_DIVIDE]   = 20;
}

Expr *Parse(String stream, String source) {
	InitParser();

	Parser parser;
	parser.source = source;

	M_PoolInit(&parser.pool, MegaBytes(16));
	LexInit(&parser.lexer, stream, &parser.pool);

	for (uint i = 0; i < PARSER_MAX_LOOKUP; ++i) {
		AdvanceTokenHelper(&parser);
	}

	Expr *expr = ParseStatement(&parser);
	return expr;
}

int main(int argc, const char *argv[]) {
	String input = Str("-4 + 5 * (3 - 2)");
	Expr *expr   = Parse(input, Str("$STDIN"));

	return 0;
}
