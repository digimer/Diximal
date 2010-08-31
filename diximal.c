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
#include <sys/types.h>		// Data types, structures, etc.
#include <sys/stat.h>		// Get file details - See 'man 2 stat'
// #include <ctype.h>		// Library for testing and character manipulation.
// #include <stdint.h>		// Library for standard integer types (guarantees the size of an int).
// #include <limits.h>		// Provides some integer and mathmatical functions line INT_MAX and UNIT_MAX


/* prototypes */
void usage (char *name);
int strip_comments(char *contents, signed long size);

// If I wanted to have garbage collection, I'd set any variables that I'd need to free as globals, set them to NULL, then on error function,
// loop through and any that aren't NULL, free/close. I'd need to add each variable to the garbage collection function as I add them here so
// that I know whether to 'free' or 'close'.


// Main function.
int main(int argc, char *argv[])
{
	// Set variables.
	char c;
	char *read_file=NULL;
	FILE *xml;
	off_t buf_size;      // 'off_t' is defined in sys/types.h as the file size type. This usually equiv. to 'unsigned long' on *nix.
	// MADI: Read up structures.
	struct stat file_info;
	// Initial file read buffer
	char *file_contents;
	
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
		// MADI: There should be a 'free' before return to free
		return -3;
	}
	
	// Read the file.
	if ((xml = fopen(read_file, "r")) == NULL)
	{
		printf("Failed to open the file: [%s] for reading, error was: %s.\n", read_file, strerror(errno));
		return -4;
	}
	
	// Get the file info.
	// MADI: pass a reference, which read_file is, and the address of file_info
	if (stat(read_file, &file_info) != 0)
	{
		printf("Failed to get the stat info on: [%s], error was: %s.\n", read_file, strerror(errno));
		return -5;
	}
	
	// Pull the file size out of the structure.
	buf_size=(file_info.st_size+1);	// Extra space for \0.
	//printf("Reading: [%s] of size: [%ld]\n", read_file, file_info.st_size);
	
	// Like malloc, but also zeros the allocated memory.
	if ((file_contents=calloc(buf_size, 1)) == NULL)
	{
		printf("Failed to allocate memory. Error was: %s.\n", strerror(errno));
		return -6;
	}
	
	// Read in the file to the buffer.
	// This is intentionally backwards so that I read 'buf_size' once, rather that 1 byte 'buf_size' time.
	if ((fread(file_contents, file_info.st_size, 1, xml)) != 1)
	{
		printf("Failed to read the file: [%s], the error was: %s.\n", read_file, strerror(errno));
		return -7;
	}
	
	// Close the XML file, it's all in memory now!
	fclose(xml);
	
	// Strip comments.
	if (strip_comments(file_contents, buf_size) != 0)
	{
		// MADI: Parse the returned error later.
		printf("Something went boom in 'strip_comments' function.");
	}
	
	// Test.
// 	printf("%s", file_contents);
	
	
	
	// Release the memory back to the OS that had been allocated to the file buffer's memory
	free(file_contents);
	
	// If I hit here, I am exiting normally.
	return 0;
}

/* functions */

// This strips the comments from the 'file_contents'.
int strip_comments(char *contents, signed long size)
{
	// Variables; Source and Destination.
	// contents could have been 'src' directly.
	char *src, *dst;
	// State variables; Processing Instrunctions, Comment and CDATA.
	int pi=0, com=0, cd=0;
	
	// Before I do anything, make sure that my contents has a final NULL.
	if (contents == NULL)
	{
		// Error, passed an empty reference.
		return -1;
	}
	if (contents[size-1] != 0)
	{
		// Contents doesn't end with a 0, contents are not to be trusted.
		return -2;
	}
	
	// Because I am moving good data to the left and the sticking a NULL terminator at the end of the shrunken cleaned data, I 
	// can use the same memory space.
	src=dst=contents;
	
	// While I am getting data from source, and while I've not gone past my buffer length...
	while((*src != 0) && (size > 0))
	{
		// If this is the less-than sign, evaluate.
		// I put the most expensive checks on the right as the first check to fail ends all following checks.
		if ((pi == 0) && (com == 0) && (cd == 0) && (*src == '<'))
		{
			if (*(src+1) == '?')
			{
				// In a processing instruction.
				pi=1;
// 				printf("\nEntering PI\n\n");
				src++;
				size--;
			}
			// 'strncmp', unlike 'strcmp', looks ahead only X chars for a match. Returns 0 on match.
			else if (strncmp(src+1, "!--", 3) == 0)
			{
				// In a comment
				com=1;
// 				printf("\nEntering COM\n\n");
				// No need to check the next three characters.
				src+=3;
				size-=3;
			}
			else if (strncmp(src+1, "![CDATA[", 8) == 0)
			{
				// In a CDATA block.
				cd = 1;
// 				printf("\nEntering CD\n\n");
				// I don't jump forward here because I am not stripping the '<![CDATA[' at this stage.
			}
		}
		
		// If I am not in a comment of processing instruction, record the character.
		if ((pi == 0) && (com == 0))
		{
			// Push the char from src to dst and move forward one position in dst's array.
			printf("%c", *src);
			*dst++ = *src;
		}
		
		// Check for leaving states.
		// The expensive check won't happen unless the '[' char is seen.
		if ((cd == 1) && (*src == ']') && (strncmp(src+1, "]>", 2) == 0))
		{
			cd=0;
			// I don't worry about looping twice more because I know what they are and they won't trigger com or pi.
		}
		// Check for leaving comments.
		if ((com == 1) && (*src == '-') && (strncmp(src+1, "->", 2) == 0))
		{
			com=0;
			// Now jump because I don't want to record the closing brace.
			src += 2;
			size -= 2;
		}
		// Check for leaving processing instrunctions.
		if ((pi == 1) && (*src == '?') && (*(src+1) == '>'))
		{
			pi=0;
			// Now jump because I don't want to record the closing brace.
			src++;
			size--;
		}
		
		// Advance forward one position in src and reduce size by 1.
		src++;
		size--;
	}
	
	return 0;
}

// Prints usage
void usage (char *name)
{
	printf("Usage: %s -f /path/to/file.xml\n", name);
}


