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

	Token_Kind_COUNT,
};

static const String TokenKindNames[] = {
	"Integer", "Plus", "Minus", "Multiply", "Divide"
};

static_assert(ArrayCount(TokenKindNames) == Token_Kind_COUNT, "");

struct Token_Range {
	umem from;
	umem to;
};

union Token_Value {
	u8  symbol;
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

void LexError(Lexer *l, const char *fmt, ...) {
	Assert(false);
}

static constexpr u8 CharacterTokenValues[]    = { '+', '-', '*', '/' };
static constexpr Token_Kind CharacterTokens[] = { Token_Kind_PLUS, Token_Kind_MINUS, Token_Kind_MULTIPLY, Token_Kind_DIVIDE };

static_assert(ArrayCount(CharacterTokens) == ArrayCount(CharacterTokenValues), "");

bool LexNext(Lexer *l, Token *token) {
	LexWhiteSpace(l);

	if (l->cursor >= l->last)
		return false;

	u8 match = *l->cursor;

	for (umem index = 0; index < ArrayCount(CharacterTokenValues); ++index) {
		if (match == CharacterTokenValues[index]) {
			token->kind         = CharacterTokens[index];
			token->range.from   = l->cursor - l->first;
			token->range.to     = token->range.from + 1;
			token->value.symbol = match;

			l->cursor += 1;
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
			LexError(l, "bad number");
			return false;
		}

		char buff[256];
		memcpy(buff, start, count);
		buff[count] = 0;

		char *endptr = nullptr;
		u64 value    = strtoull(buff, &endptr, 10);

		if (endptr != buff + count || errno == ERANGE) {
			LexError(l, "bad number");
			return false;
		}

		token->kind          = Token_Kind_INTEGER;
		token->range.from    = l->cursor - l->first;
		token->range.to      = pos - l->first;
		token->value.integer = value;

		l->cursor = pos;

		return true;
	}

	LexError(l, "bad character");

	return false;
}

void LexDump(FILE *out, const Token &token) {
	String name = TokenKindNames[token.kind];
	fprintf(out, "." StrFmt " ", StrArg(name));

	if (token.kind == Token_Kind_INTEGER) {
		fprintf(out, "(%zu) ", token.value.integer);
	} else if (token.kind == Token_Kind_PLUS || token.kind == Token_Kind_MINUS ||
		token.kind == Token_Kind_MULTIPLY || token.kind == Token_Kind_MULTIPLY) {
		fprintf(out, "(%c) ", token.value.symbol);
	}
}

int main(int argc, const char *argv[]) {
	String input = " 4 + 5 * 3 -2 ";

	Lexer l;
	l.cursor = input.data;
	l.first  = l.cursor;
	l.last   = l.first + input.count;
	l.source = "-generated-";

	Token token;
	while (LexNext(&l, &token)) {
		LexDump(stdout, token);
		fprintf(stdout, "\n");
	}

	return 0;
}

#if 0
bool is_number(u8 ch) {
	return ch >= '0' && ch <= '9';
}

bool is_whitespace(u8 ch) {
	return ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r';
}

String token_number(String input) {
	imem pos = 0;

	for (; pos < input.count; ++pos) {
		if (!is_number(input[pos]))
			break;
	}

	String result = String(input.data, pos);

	return result;
}

enum Token_Kind {
	TOKEN_INTEGER,
	TOKEN_PLUS,
	TOKEN_MINUS,
	TOKEN_MULTIPLY,
	TOKEN_DIVIDE,
	TOKEN_COUNT
};

const char *TokenNames[] = {
	"integer", "plus", "minus", "multiply", "divide"
};

static_assert(ArrayCount(TokenNames) == TOKEN_COUNT, "");

struct Token {
	Token_Kind kind;
	String     source;
	uint64_t   integer;
};

struct Lexer {
	Token  token;
	String input;
	imem   cursor;

	u8 scratch[1024];
};

static constexpr u8 Operators[] = { '+', '-', '*', '/'};
static constexpr Token_Kind OperatorTokens[] = { TOKEN_PLUS, TOKEN_MINUS, TOKEN_MULTIPLY, TOKEN_DIVIDE };

static_assert(ArrayCount(OperatorTokens) == ArrayCount(Operators), "");

void skip_whitespace(Lexer *lexer) {
	for (; lexer->cursor < lexer->input.count; ++lexer->cursor) {
		if (!is_whitespace(lexer->input[lexer->cursor])) {
			return;
		}
	}
}

bool lex(Lexer *lexer) {
	skip_whitespace(lexer);

	if (lexer->cursor >= lexer->input.count)
		return false;

	Token *token = &lexer->token;
	imem cursor  = lexer->cursor;
	String input = lexer->input; // substring

	assert(input.count); // todo: error check

	u8 ch = input[cursor];

	for (umem index = 0; index < ArrayCount(Operators); ++index) {
		u8 match = Operators[index];
		if (match == ch) {
			token->kind = OperatorTokens[index];
			token->source = String(input.data + cursor, 1);
			lexer->cursor += 1;
			return true;
		}
	}

	if (is_number(ch)) {
		String number = String(lexer->input.data + cursor, lexer->input.count - cursor);
		number = token_number(number);

		assert(number.count < 100);

		memcpy(lexer->scratch, number.data, number.count);
		lexer->scratch[number.count] = 0;

		char *endptr = nullptr;
		token->integer = (uint64_t)strtoull((char *)lexer->scratch, &endptr, 10);

		assert(number.count == (endptr - (char *)lexer->scratch));

		token->kind = TOKEN_INTEGER;
		token->source = number;

		lexer->cursor += number.count;

		return true;
	}

	assert(false); // unimplemented
}

enum Code_Node_Kind {
	CODE_BINARY_OPERATOR,
	CODE_INTEGER_LITERAL,
};

struct Code_Node {
	Code_Node_Kind kind;
	Code_Node(){}
	Code_Node(Code_Node_Kind _kind): kind(_kind){}
};

struct Code_Node_Binary_Operator : Code_Node {
	Code_Node *left;
	Code_Node *right;

	u8    symbol;

	Code_Node_Binary_Operator(): Code_Node(CODE_BINARY_OPERATOR), left(nullptr), right(nullptr) {}
};

struct Code_Node_Integer_Literal :Code_Node {
	uint64_t value;

	Code_Node_Integer_Literal(): Code_Node(CODE_INTEGER_LITERAL) {}
};

void expect(Lexer *lexer, Token_Kind kind) {
	if (lex(lexer)) {
		assert(lexer->token.kind == kind);
		return;
	}

	assert(false);
}

bool accept(Lexer *lexer, Token_Kind kind) {
	return lexer->token.kind == kind;
}

Code_Node *parse_subexpression(Lexer *lexer) {
	expect(lexer, TOKEN_INTEGER);

	auto node = new Code_Node_Integer_Literal();
	node->value = lexer->token.integer;

	return node;
}

int operator_prec(Token_Kind kind) {
	if (kind == TOKEN_PLUS) return 1;
	if (kind == TOKEN_MINUS) return 1;
	if (kind == TOKEN_MULTIPLY) return 2;
	if (kind == TOKEN_DIVIDE) return 2;
	return 0;
}

Code_Node *parse_expression(Lexer *lexer, int prec = -1) {
	Code_Node *left = parse_subexpression(lexer);

	while (lex(lexer)) {
		if (accept(lexer, TOKEN_PLUS) || accept(lexer, TOKEN_MINUS) ||
			accept(lexer, TOKEN_MULTIPLY) || accept(lexer, TOKEN_DIVIDE)) {

			int op_prec = operator_prec(lexer->token.kind);

			if (op_prec <= prec)
				break;

			auto node = new Code_Node_Binary_Operator;
			node->symbol = lexer->token.source[0];
			node->left = left;
			node->right = parse_expression(lexer, op_prec);

			left = node;
		} else {
			assert(false);
		}
	}

	return left;
}

void dump(Code_Node *root, int indent = 0) {
	for (int i = 0; i < indent; ++i) {
		printf("  ");
	}

	switch (root->kind) {
	case CODE_INTEGER_LITERAL:
	{
		auto node = (Code_Node_Integer_Literal *)root;
		printf("Integer: %zu\n", node->value);
		break;
	}

	case CODE_BINARY_OPERATOR:
	{
		auto node = (Code_Node_Binary_Operator *)root;
		printf("Binary Operator: %c\n", node->symbol);
		dump(node->left, indent + 1);
		dump(node->right, indent + 1);
		break;
	}
	}
}

int main(int argc, const char *argv[]) {
	static Code_Node * stack[2048];

	String input = " 4 + 5 * 3 -2 ";

	Lexer lexer;
	lexer.input = input;
	lexer.cursor = 0;

	umem top = 0;

	Code_Node *node = parse_expression(&lexer);
	dump(node);

	return 0;
}
#endif
