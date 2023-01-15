#include <stdio.h>
#include <stdint.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>

typedef size_t    umem;
typedef ptrdiff_t imem;

#define static_count(arr) (sizeof(arr)/sizeof((arr)[0]))

struct String {
	imem     count;
	uint8_t *data;

	String(): data(0), count(0) {}
	template <imem N> constexpr String(const char(&a)[N]) : data((uint8_t *)a), count(N - 1) {}
	String(const uint8_t *_Data, imem _Length): data((uint8_t *)_Data), count(_Length) {}
	String(const char *_Data, imem _Length): data((uint8_t *)_Data), count(_Length) {}
	const uint8_t &operator[](const imem index) const { assert(index < count); return data[index]; }
	uint8_t &operator[](const imem index) { assert(index < count); return data[index]; }
};

bool is_number(uint8_t ch) {
	return ch >= '0' && ch <= '9';
}

bool is_whitespace(uint8_t ch) {
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

static_assert(static_count(TokenNames) == TOKEN_COUNT, "");

struct Token {
	Token_Kind kind;
	String     source;
	uint64_t   integer;
};

struct Lexer {
	Token  token;
	String input;
	imem   cursor;

	uint8_t scratch[1024];
};

static constexpr uint8_t Operators[] = { '+', '-', '*', '/'};
static constexpr Token_Kind OperatorTokens[] = { TOKEN_PLUS, TOKEN_MINUS, TOKEN_MULTIPLY, TOKEN_DIVIDE };

static_assert(static_count(OperatorTokens) == static_count(Operators), "");

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

	uint8_t ch = input[cursor];

	for (umem index = 0; index < static_count(Operators); ++index) {
		uint8_t match = Operators[index];
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

	uint8_t    symbol;

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
