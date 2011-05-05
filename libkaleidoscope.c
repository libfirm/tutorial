/* The functions in this file can be accessed from Kaleidoscope programs. */

#include <stdio.h>

double putchard(double c)
{
	putchar((char) c);
	return 0;
}

double getchard()
{
	return (double) getchar();
}
