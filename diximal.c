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
// #include <errno.h>		// Error stuff
// #include <ctype.h>		// Library for testing and character manipulation.
// #include <stdint.h>		// Library for standard integer types (guarantees the size of an int).
#include <stdlib.h>		// Library for things like 'sizeof()', 'itoa()', rand(), etc.
// #include <limits.h>		// Provides some integer and mathmatical functions line INT_MAX and UNIT_MAX
#include <string.h>

// File to read. Make this able to be picked up by a command line switch later.
char xml_file[1024]="./source.xml";

// Main function.
int main(void)
{
	/* Variables */
	// Maximum line length/buffer size.
	char line[8096];
	// Position of the newline in a line of the file. AKA, the length of a
	// line.
	int nl;
	// Standard iterator.
	int i;
	// This gets set to 1 while inside a comment.
	int in_comment;
	// This gets set to 1 while inside a processing instruction.
	int in_process_inst;
	// This is set to 1 when I see the first non-space character is a string.
	int seen_first_char;
	// This is a counter to see how many characters I actually printed in a
	// line.
	int char_printed;
	
	// Open the XML file read-only.
	FILE*xml;
	xml=fopen(xml_file, "r");
	
	/* Initial values */
	in_comment=0;
	in_process_inst=0;
	seen_first_char=0;
	char_printed=0;
	
	// Exit with error code 1 if I couldn't read the file.
	if (0 == xml)
	{
		printf("Failed to open %s\n", xml_file);
		return 1;
	}
	
	while (fgets(line, sizeof(line), xml))
	{
		// Reset the first char flag.
		// MADI: Don't use this if I am in the middle of reading a
		// MADI: multi-line CONTENT.
		seen_first_char=0;
		
		// Reset the characters printed counter.
		char_printed=0;
		
		// MADI: Make this work properly if a line exceeds the buffer
		// MADI: (that is, 's' is NULL, concat the next line, etc).
 		nl = strcspn (line, "\n");
		
		// Nix newlines.
		line[nl]='\0';
		
		// Only proceed if the line isn't empty.
		if (nl > 0)
		{
			for (i=0; i<nl; i++)
			{
				// Get rid of comments.
				if (('<' == line[i]) && ('!' == line[i+1]) && ('-' == line[i+2]) && ('-' == line[i+3]))
				{
					in_comment=1;
				}
				else if (('-' == line[i]) && ('-' == line[i+1]) && ('>' == line[i+2]))
				{
					in_comment=0;
					i+=3;
				}
				
				// Get rid of processing instructions.
				if (('<' == line[i]) && ('?' == line[i+1]))
				{
					in_process_inst=1;
				}
				else if (('?' == line[i]) && ('>' == line[i+1]))
				{
					in_process_inst=0;
					i+=2;
				}
				
				if ((0 == in_comment) && (0 == in_process_inst))
				{
					// Skip leading white spaces.
					if ((0 == seen_first_char) && (' ' != line[i]) && '\t' != line[i])
					{
						seen_first_char=1;
					}
					if ((1 == seen_first_char) && (0 != line[i]))
					{
						printf("%c", line[i]);
						char_printed++;
					}
				}
			}
			
			// Don't print a newline if nothing was actually
			// printed to the screen.
			if (char_printed>0)
			{
				printf("\n");
			}
// 			printf("%s", line);
// 			printf("line: %d-[%s]\n", s, line);
		}
	}
	
	// If I hit here, I am exiting normally.
	return 0;
}
