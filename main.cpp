#include "Platform.h"
#include "Common.h"
#include "Lexer.h"
#include "Memory.h"

#include <stdlib.h>

constexpr int PARSER_DUMP_TOKEN = 0x1;
constexpr int PARSER_DUMP_EXPR  = 0x2;

#ifdef BUILD_DEBUG
static constexpr u32 ParserDumpFlags = 0xff;
#else
static constexpr u32 ParserDumpFlags = 0;
#endif

enum Expr_Kind {
	Expr_Kind_Literal,
	Expr_Kind_Unary_Operator,
	Expr_Kind_Binary_Operator,

	Expr_Kind_COUNT
};

static const String ExprKindNames[] = {
	"Literal", "Unary Operator", "Binary Operator"
};
static_assert(ArrayCount(ExprKindNames) == Expr_Kind_COUNT, "");

struct Expr_Type {
};

struct Expr {
	Expr_Kind   kind;
	Expr_Type * type;
	Token_Range range;
};

struct Expr_Literal : Expr {
	Token_Value value;
};

struct Expr_Unary_Operator : Expr {
	Expr *child;
	u32   symbol;
};

struct Expr_Binary_Operator : Expr {
	Expr *left;
	Expr *right;
	u32   symbol;
};

static constexpr uint PARSER_MAX_LOOKUP = 4;

struct Parser {
	Lexer    lexer;
	Token    lookup[PARSER_MAX_LOOKUP];
	M_Arena *arena;
	String   source;
};

void *OutOfMemory() {
	TriggerBreakpoint();
	exit(1);
	return nullptr;
}

void *Allocate(Parser *parser, umem size, u32 alignment) {
	constexpr umem PARSER_ARENA_SIZE = MegaBytes(16);

	void *ptr = M_PushSizeAligned(parser->arena, size, alignment, M_CLEAR_MEMORY);
	if (ptr) return ptr;

	M_Arena *arena = M_ArenaAllocate(PARSER_ARENA_SIZE);
	arena->next    = parser->arena;
	parser->arena  = arena;

	ptr = M_PushSizeAligned(parser->arena, size, alignment, M_CLEAR_MEMORY);
	if (ptr) return ptr;

	return OutOfMemory();
}

Expr *ExprInit(Expr *expr, Expr_Kind kind, Token_Range range) {
	expr->kind  = kind;
	expr->range = range;
	return expr;
}

enum Log_Kind {
	Log_Kind_INFO,
	Log_Kind_WARNING,
	Log_Kind_ERROR,
	Log_Kind_FATAL
};

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

	static const char *LogKindNames[] = { "info", "warning","error", "error" };

	fprintf(out, StrFmt "(%zu,%zu): %s: ", StrArg(parser->source), r, c, LogKindNames[kind]);
	vfprintf(out, fmt, args);
	fprintf(out, "\n");

	if (kind == Log_Kind_FATAL) {
		exit(1);
	}
}

void Info(Parser *parser, Token *token, const char *fmt, ...) {
	va_list args;
	va_start(args, fmt);
	Log(parser, token->range.from, token->range.to, stdout, Log_Kind_INFO, fmt, args);
	va_end(args);
}

void Info(Parser *parser, Expr *expr, const char *fmt, ...) {
	va_list args;
	va_start(args, fmt);
	Log(parser, expr->range.from, expr->range.to, stdout, Log_Kind_INFO, fmt, args);
	va_end(args);
}

void Warning(Parser *parser, Token *token, const char *fmt, ...) {
	va_list args;
	va_start(args, fmt);
	Log(parser, token->range.from, token->range.to, stderr, Log_Kind_WARNING, fmt, args);
	va_end(args);
}

void Warning(Parser *parser, Expr *expr, const char *fmt, ...) {
	va_list args;
	va_start(args, fmt);
	Log(parser, expr->range.from, expr->range.to, stderr, Log_Kind_WARNING, fmt, args);
	va_end(args);
}

void Error(Parser *parser, Token *token, const char *fmt, ...) {
	va_list args;
	va_start(args, fmt);
	Log(parser, token->range.from, token->range.to, stderr, Log_Kind_ERROR, fmt, args);
	va_end(args);
}

void Error(Parser *parser, Expr *expr, const char *fmt, ...) {
	va_list args;
	va_start(args, fmt);
	Log(parser, expr->range.from, expr->range.to, stderr, Log_Kind_ERROR, fmt, args);
	va_end(args);
}

void Fatal(Parser *parser, Token *token, const char *fmt, ...) {
	va_list args;
	va_start(args, fmt);
	Log(parser, token->range.from, token->range.to, stderr, Log_Kind_FATAL, fmt, args);
	va_end(args);
}

void Fatal(Parser *parser, Expr *expr, const char *fmt, ...) {
	va_list args;
	va_start(args, fmt);
	Log(parser, expr->range.from, expr->range.to, stderr, Log_Kind_FATAL, fmt, args);
	va_end(args);
}

#define AllocateExpr(parser, type, range) (Expr_##type *)ExprInit((Expr *)Allocate(parser, sizeof(Expr_##type), alignof(Expr_##type)), Expr_Kind_##type, range)

Token PeekToken(Parser *parser, uint index = 0) {
	Assert(index <= PARSER_MAX_LOOKUP);
	return parser->lookup[index];
}

void _AdvanceToken(Parser *parser) {
	for (uint i = 0; i < PARSER_MAX_LOOKUP - 1; ++i) {
		parser->lookup[i] = parser->lookup[i + 1];
	}

	Token *token = &parser->lookup[PARSER_MAX_LOOKUP - 1];
	if (!LexNext(&parser->lexer, token)) {
		Fatal(parser, token, parser->lexer.error);
	}
}

void AdvanceToken(Parser *parser) {
	if constexpr (ParserDumpFlags & PARSER_DUMP_TOKEN) {
		fprintf(stdout, "T");
		LexDump(stdout, parser->lookup[0]);
	}
	_AdvanceToken(parser);
}

Token NextToken(Parser *parser) {
	Token result = parser->lookup[0];
	AdvanceToken(parser);
	return result;
}

Expr *ParseTerm(Parser *parser) {
	Token token = NextToken(parser);

	if (token.kind == Token_Kind_INTEGER) {
		auto expr   = AllocateExpr(parser, Literal, token.range);
		expr->value = token.value;
		return expr;
	}

	if (token.kind == Token_Kind_PLUS || token.kind == Token_Kind_MINUS) {
		auto expr    = AllocateExpr(parser, Unary_Operator, token.range);
		expr->child  = ParseTerm(parser);
		expr->type   = expr->child->type;
		expr->symbol = token.value.symbol;
		return expr;
	}

	Fatal(parser, &token, "expected expression");

	return nullptr;
}

static constexpr Token_Kind BinaryOpTokens[] = { Token_Kind_PLUS, Token_Kind_MINUS, Token_Kind_MULTIPLY, Token_Kind_DIVIDE };

static int BinaryOpPrecedence[Token_Kind_END];

Expr *ParseExpression(Parser *parser, int prev_prec = -1) {
	Expr *expr   = ParseTerm(parser);

	for (Token token = PeekToken(parser); 
		token.kind != Token_Kind_END;
		token = PeekToken(parser)) {

		int prec = BinaryOpPrecedence[token.kind];

		if (prec <= prev_prec)
			break;

		for (Token_Kind match : BinaryOpTokens) {
			if (match == token.kind) {
				AdvanceToken(parser);

				auto op    = AllocateExpr(parser, Binary_Operator, token.range);
				op->left   = expr;
				op->right  = ParseExpression(parser, prec);
				op->symbol = token.value.symbol;

				expr = op;
				break;
			}
		}
	}

	return expr;
}

void ExprDump(FILE *out, Expr *root, uint indent = 0) {
	for (uint i = 0; i < indent; ++i)
		fprintf(out, "    ");

	const String name = ExprKindNames[root->kind];
	fprintf(out, "." StrFmt " ", StrArg(name));

	switch (root->kind) {
	case Expr_Kind_Literal:
	{
		auto expr = (Expr_Literal *)root;
		fprintf(out, "(%zu)", expr->value.integer);
		fprintf(out, "\n");
	} break;

	case Expr_Kind_Unary_Operator:
	{
		auto expr = (Expr_Unary_Operator *)root;
		fprintf(out, "(%c)", (char)expr->symbol);
		fprintf(out, "\n");
		ExprDump(out, expr->child, indent + 1);
	} break;

	case Expr_Kind_Binary_Operator:
	{
		auto expr = (Expr_Binary_Operator *)root;
		fprintf(out, "(%c)", (char)expr->symbol);
		fprintf(out, "\n");
		ExprDump(out, expr->left, indent + 1);
		ExprDump(out, expr->right, indent + 1);
	} break;
	}
}

Expr *ParseStatement(Parser *parser) {
	Expr *expr = ParseExpression(parser);

	if constexpr (ParserDumpFlags & PARSER_DUMP_EXPR) {
		fprintf(stdout, "\n");
		ExprDump(stdout, expr);
	}

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

Expr *Parse(M_Arena *arena, String stream, String source = "$NULL") {
	InitParser();

	Parser parser;
	parser.source = source;
	parser.arena  = arena;

	LexInit(&parser.lexer, stream);

	for (uint i = 0; i < PARSER_MAX_LOOKUP; ++i) {
		_AdvanceToken(&parser);
	}

	Expr *expr = ParseStatement(&parser);
	return expr;
}

int main(int argc, const char *argv[]) {
	String input = "4 + 5 * 3 -2 ";

	M_Arena *arena = M_ArenaAllocate(0);
	Expr *expr     = Parse(arena, input, "$STDIN");

	return 0;
}
