/* Diximal; Digimer's XML parser

This is a primitive XML parser written as an excercise in C to help me convert
my Perl knowledge into C experience. It's probably never going to be a useful
program.

Madison Kelly (Digimer)
mkelly@alteeve.com
Aug. 28, 2010

See README for usage and expected behaviour.
*/

// Inluded libraries.
#include <stdio.h>		// Standard I/O
#include <stdlib.h>		// Library for things like 'sizeof()', 'itoa()', rand(), etc.
#include <unistd.h>		// Unix standard stuff.
#include <errno.h>		// Error stuff
#include <string.h>		// Verbose errors, etc.
// #include <ctype.h>		// Library for testing and character manipulation.
// #include <stdint.h>		// Library for standard integer types (guarantees the size of an int).
// #include <limits.h>		// Provides some integer and mathmatical functions line INT_MAX and UNIT_MAX


/* prototypes */
void usage (char *name);


// Main function.
int main(int argc, char *argv[])
{
	// Set variables.
	char c;
	char *read_file=NULL;
	FILE *xml;
	
	// Pick up switches
	while ((c = getopt (argc, argv, "hf:")) != -1)
	{
		switch (c)
		{
			case 'h':
				usage(argv[0]);
				return 0;
			break;
			
			case 'f':
				read_file = optarg;
			break;
			
			case '?':
				if (optopt == 'f')
				{
					printf("Missing filename.\n");
				}
				else
				{
					printf("Invalid switch '%c'\n", optopt);
				}
				usage(argv[0]);
				return -1;
			break;
			
			default:
				printf("Unexpected error!\n");
				usage(argv[0]);
				return -2;
		}
	}
	
	// Did I get a file?
	if (read_file == NULL)
	{
		printf("No file passed.\n");
		usage(argv[0]);
		return -3;
	}
	
	// Read the file.
	if ((xml = fopen(read_file, "r")) == NULL)
	{
		printf("Failed to read: [%s], error was: %s.\n", read_file, strerror(errno));
		return -4;
	}
	
	
	
	
	
	// Close the XML file.
	fclose(read_file);
	
	// If I hit here, I am exiting normally.
	return 0;
}

void usage (char *name)
{
	printf("Usage: %s -f /path/to/file.xml\n", name);
}
