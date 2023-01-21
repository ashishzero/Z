#include "Parser.h"

#define MICROSOFT_WINDOWS_WINBASE_H_DEFINE_INTERLOCKED_CPLUSPLUS_OVERLOADS 0
#include <Windows.h>
#include <consoleapi2.h>

int main(int argc, const char *argv[]) {
	SetConsoleOutputCP(CP_UTF8);

	M_Pool pool;
	M_PoolInit(&pool, KiloBytes(128));

	String input = Str(u8"Val_1_日本語 = -4 + 5 * (3 - 2)");
	Expr * expr  = Parse(input, Str("$STDIN"), &pool);

	return 0;
}
