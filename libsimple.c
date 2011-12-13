/* The functions in this file can be accessed from Simple programs. */

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

double read(void)
{
	double d = 0;
	if (scanf("%lf", &d) == 0)
		return 0;
	return d;
}

void print(double d)
{
	printf("%f\n", d);
}

/** the runtime library provides a "C" main function. Which calls the
 * __simple_main function of a program */
int main(void)
{
	extern double __simple_main(void);
	__simple_main();
	return 0;
}

