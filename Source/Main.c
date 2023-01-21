#include "Parser.h"

int main(int argc, const char *argv[]) {
	M_Pool pool;
	M_PoolInit(&pool, KiloBytes(128));

	String input = Str("Val_1 = -4 + 5 * (3 - 2)");
	Expr * expr  = Parse(input, Str("$STDIN"), &pool);

	return 0;
}
