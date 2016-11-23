#include <stdio.h>
#include <string.h>


void _prog();
void _print_string();

int main()
{
	_prog();
}

void _print_int(int n)
{
	printf("%d", n);
}

void _print_string(const char* str)
{
	fwrite(str, 1, strlen(str), stdout);
}
