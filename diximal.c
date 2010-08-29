/* Diximal; Digimer's XML parser

This is a primitive XML parser written as an excercise in C to help me convert
my Perl knowledge into C experience. It's probably never going to be a useful
program.

Madison Kelly (Digimer)
mkelly@alteeve.com
Aug. 28, 2010

See README for usage and expected behaviour.
*/

#include <stdio.h>	// Standard I/O
// #include <ctype.h>	// Library for testing and character manipulation.
// #include <stdint.h>	// Library for standard integer types (guarantees the size of an int).
// #include <stdlib.h>	// Library for things like 'sizeof()', 'itoa()', rand(), etc.
// #include <limits.h>	// Provides some integer and mathmatical functions line INT_MAX and UNIT_MAX

int main(void)
{
	int c;
	int is_newline;
	is_newline=1;
	
	while ((c=getchar()) != EOF)
	{
		// Cut out superfluous newlines.
		if ('\n' == c)
		{
			if (0 == is_newline)
			{
				is_newline=1;
				putchar('\n');
			}
			continue;
		}
		
		// Cut off leading white spaces.
		if ((1 == is_newline) && ((' ' != c) && ('\t' != c)))
		{
			is_newline=0;
		}
		if (1 == is_newline)
		{
			continue;
		}
		
		// Now start parsing.
		putchar(c);
	}
	
	return 0;
}
