
#include "main.h"
#include "common_func.h"
#include "log.h"
#include <stdio.h>
#ifdef _WIN32
#include <Windows.h>
#endif

int main()
{
	LOG3("INFO", "Hello,commo\n");

	char ch = 0;
	while ('e' != ch)
	{
		printf(">:");
		ch = get_char();
		printf("\n");
		switch(ch) {
			case '1': {
				string tt = timestamp_to_string(time(NULL));
				LOG3("INFO", " current time: %s\n", tt.c_str());
			}break;
			case '2': {
				time_t t = string_to_timestamp("2021-11-12 10:00:39");
				LOG3("INFO", "    timestamp: %lld\n", t);
			}break;
			case '3': {
				string str = random_string(64);
				LOG3("INFO", "random_string: %s\n", str.c_str());
			}break;
			case '4': {
				init_rand();
				LOG3("INFO", "init rand complete.\n");
			}break;
			default: LOG3("INFO", "Invalid command '%c'\n", ch);
		}
	}

	#ifdef _WIN32
	system("pause");
	#endif
    return 0;
}