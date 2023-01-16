#include "Platform.h"
#include "Common.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

enum Token_Kind {
	Token_Kind_INTEGER,
	Token_Kind_PLUS,
	Token_Kind_MINUS,
	Token_Kind_MULTIPLY,
	Token_Kind_DIVIDE,

	Token_Kind_END,
};

static const String TokenKindNames[] = {
	"Integer", "Plus", "Minus", "Multiply", "Divide"
};

static_assert(ArrayCount(TokenKindNames) == Token_Kind_END, "");

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
	String source;
	char   error[1024];
};

void LexWhiteSpace(Lexer *l) {
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

void LexToken(Lexer *l, Token *token, Token_Kind kind, u8 *pos) {
	token->kind       = kind;
	token->range.from = l->cursor - l->first;
	token->range.to   = pos - l->first;
	l->cursor         = pos;
}

void LexError(Lexer *l, Token *token, u8 *pos, const char *fmt, ...) {
	va_list args;
	va_start(args, fmt);
	vsnprintf(l->error, sizeof(l->error), fmt, args);
	va_end(args);

	LexToken(l, token, Token_Kind_END, pos);
}

static constexpr u8 CharacterTokenValues[]    = { '+', '-', '*', '/' };
static constexpr Token_Kind CharacterTokens[] = { Token_Kind_PLUS, Token_Kind_MINUS, Token_Kind_MULTIPLY, Token_Kind_DIVIDE };

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

void LexDump(FILE *out, const Token &token) {
	String name = TokenKindNames[token.kind];
	fprintf(out, "." StrFmt " ", StrArg(name));

	if (token.kind == Token_Kind_INTEGER) {
		fprintf(out, "(%zu) ", token.value.integer);
	} else if (token.kind == Token_Kind_PLUS || token.kind == Token_Kind_MINUS ||
		token.kind == Token_Kind_MULTIPLY || token.kind == Token_Kind_MULTIPLY) {
		fprintf(out, "(%c) ", (char)token.value.symbol);
	}
}

void Error(struct Parser *parser, const char *fmt, ...) {
	Unimplemented();
}

struct Arena {

};

void *PushSize(Arena *arena, umem size, umem alignment) {
	return malloc(size);
}

void *operator new(umem size, Arena *arena) noexcept {
	return PushSize(arena, size, sizeof(umem));
}

void operator delete(void *ptr, Arena *arena) noexcept {
}

enum Expr_Kind {
	Expr_Kind_LITERAL,
	Expr_Kind_UNARY_OPERATOR,
	Expr_Kind_BINARY_OPERATOR,

	Expr_Kind_COUNT
};

static const String ExprKindNames[] = {
	"Literal", "Unary Operator", "Binary Operator"
};
static_assert(ArrayCount(ExprKindNames) == Expr_Kind_COUNT, "");

struct Expr_Type {
};

struct Expr {
	Expr_Kind   kind  = Expr_Kind_COUNT;
	Expr_Type * type  = nullptr;
	Token_Range range = { 0,0 };

	Expr(Expr_Kind _kind): kind(_kind) {}
};

struct Expr_Literal : Expr {
	Token_Value value;

	Expr_Literal(): Expr(Expr_Kind_LITERAL) {}
};

struct Expr_Unary_Operator : Expr {
	Expr *child = nullptr;
	u32 symbol  = 0;

	Expr_Unary_Operator(): Expr(Expr_Kind_UNARY_OPERATOR) {}
};

struct Expr_Binary_Operator : Expr {
	Expr *left  = nullptr;
	Expr *right = nullptr;
	u32  symbol = 0;

	Expr_Binary_Operator(): Expr(Expr_Kind_BINARY_OPERATOR) {}
};

static constexpr uint PARSER_MAX_LOOKUP = 4;

struct Parser {
	Lexer  lexer;
	Token  lookup[PARSER_MAX_LOOKUP];
	Arena *arena;
};

Token PeekToken(Parser *parser, uint index = 0) {
	Assert(index <= PARSER_MAX_LOOKUP);
	return parser->lookup[index];
}

void _AdvanceToken(Parser *parser) {
	for (uint i = 0; i < PARSER_MAX_LOOKUP - 1; ++i) {
		parser->lookup[i] = parser->lookup[i + 1];
	}

	Token *dst = &parser->lookup[PARSER_MAX_LOOKUP - 1];
	if (!LexNext(&parser->lexer, dst)) {
		Error(parser, "");
	}
}

void AdvanceToken(Parser *parser) {
#ifdef BUILD_DEBUG
	LexDump(stdout, parser->lookup[0]);
#endif
	_AdvanceToken(parser);
}

Token NextToken(Parser *parser) {
	Token result = parser->lookup[0];
	AdvanceToken(parser);
	return result;
}

Expr *ParseTerm(Parser *parser) {
	Arena *arena = parser->arena;

	Token token = NextToken(parser);

	if (token.kind == Token_Kind_INTEGER) {
		auto expr = new(arena) Expr_Literal;

		// @Todo: Set type
		expr->range = token.range;
		//expr->type = ? ;
		expr->value = token.value;

		//expr->type = ?
		return expr;
	}

	if (token.kind == Token_Kind_PLUS || token.kind == Token_Kind_MINUS) {
		auto expr = new(arena) Expr_Unary_Operator;

		expr->child  = ParseTerm(parser);
		expr->range  = token.range;
		expr->type   = expr->child->type;
		expr->symbol = token.value.symbol;

		return expr;
	}

	Error(parser, "");

	return nullptr;
}

static constexpr Token_Kind BinaryOpTokens[] = { Token_Kind_PLUS, Token_Kind_MINUS, Token_Kind_MULTIPLY, Token_Kind_DIVIDE };

static int BinaryOpPrecedence[Token_Kind_END];

Expr *ParseExpression(Parser *parser, int prev_prec = -1) {
	Arena *arena = parser->arena;

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

				auto op  = new(arena) Expr_Binary_Operator;

				op->left   = expr;
				op->right  = ParseExpression(parser, prec);
				op->range  = token.range;
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
	case Expr_Kind_LITERAL:
	{
		auto expr = (Expr_Literal *)root;
		fprintf(out, "(%zu)", expr->value.integer);
		fprintf(out, "\n");
	} break;

	case Expr_Kind_UNARY_OPERATOR:
	{
		auto expr = (Expr_Unary_Operator *)root;
		fprintf(out, "(%c)", (char)expr->symbol);
		fprintf(out, "\n");
		ExprDump(out, expr->child, indent + 1);
	} break;

	case Expr_Kind_BINARY_OPERATOR:
	{
		auto expr = (Expr_Binary_Operator *)root;
		fprintf(out, "(%c)", (char)expr->symbol);
		fprintf(out, "\n");
		ExprDump(out, expr->left, indent + 1);
		ExprDump(out, expr->right, indent + 1);
	} break;
	}
}

int main(int argc, const char *argv[]) {
	String input = " 4 + 5 * 3 -2 ";

	BinaryOpPrecedence[Token_Kind_PLUS]     = 10;
	BinaryOpPrecedence[Token_Kind_MINUS]    = 10;
	BinaryOpPrecedence[Token_Kind_MULTIPLY] = 20;
	BinaryOpPrecedence[Token_Kind_DIVIDE]   = 20;

	Parser parser;
	parser.lexer.first  = input.begin();
	parser.lexer.last   = input.end();
	parser.lexer.cursor = parser.lexer.first;
	parser.lexer.source = "-generated-";

	parser.arena = nullptr;

	for (uint i = 0; i < PARSER_MAX_LOOKUP; ++i) {
		_AdvanceToken(&parser);
	}

	Expr *expr = ParseExpression(&parser);

	fprintf(stdout, "\n");

	ExprDump(stdout, expr);

	return 0;
}
