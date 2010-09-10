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
#include <ctype.h>		// Library for testing and character manipulation.
// #include <stdint.h>		// Library for standard integer types (guarantees the size of an int).
// #include <limits.h>		// Provides some integer and mathmatical functions line INT_MAX and UNIT_MAX

// Include my header
#include "stack.h"

#define STACK_SIZE	100


/* Enumeration */

typedef enum {
	// These are being assigned sequential integer values by the compile
	// (xml_content = 0, ... )
	xml_none,	// Initial type.
	xml_root,	// 
	xml_content,	// While reading CONTENT.
	xml_cdata,	// While in a CDATA block
	xml_tag		// While parsing a tag.
} xml_obj_type_t;


/* Unions */

// This has to come first as it's a forward decleration. This means that I can
// reference it in my structures before the unions define the connections of
// the structures.
union xml_obj;

/* structures */

typedef struct xml_attr_s {
	char * name;
	char * value;
	struct xml_attr_s * next;
} xml_attr_t;			// This name is use as a short-form for "struct
				// xml_attr_s <name>".

// The name suffix _s helps show that it's a structure (vs. _t for type).
// This is the generic function used to link things together.
struct xml_generic_obj_s {
	// This is a variable of the named type (see in enumeration above)
	xml_obj_type_t obj_type;
	// This points to ourself and points to whatever is to the right, or
	// NULL.
	union xml_obj * right;
};

struct xml_content_obj_s {
	xml_obj_type_t obj_type;
	union xml_obj * right;
	// Contents between the opening tag and the first non-content element.
	char * xmldata;
};

struct xml_cdata_obj_s {
	xml_obj_type_t obj_type;
	union xml_obj * right;
	// This is the string between the opening and closing CDATA stanza.
	char * xmldata;
};

struct xml_tag_obj_s {
	xml_obj_type_t obj_type;
	union xml_obj * right;
	// This is the name of the tag.
	char * tagname;
	// This is a pointer to a linked list.
	xml_attr_t * attributes;
	// Pointer to chilren.
	union xml_obj * children;
};

struct xml_root_obj_s {
	// This, being root, has no siblings
	xml_obj_type_t obj_type;
	union xml_obj * children;
};


// Define the union.
typedef union xml_obj {
	xml_obj_type_t obj_type;
	// If I didn't want to use 'struct ...', I could have put
	// 'xml_generic_obj_t' after the decleration above.
	struct xml_generic_obj_s generic;
	struct xml_content_obj_s content;
	struct xml_cdata_obj_s   cdata;
	struct xml_root_obj_s    xmlroot;
	struct xml_tag_obj_s     xmltag;
} xml_obj_t;


/* prototypes */
void usage (char *name);
int strip_comments(char *contents, signed long size);
xml_obj_t * parse_xml_content(char * file_contents);


// If I wanted to have garbage collection, I'd set any variables that I'd need
// to free as globals, set them to NULL, then on error function, loop through
// and any that aren't NULL, free/close. I'd need to add each variable to the
// garbage collection function as I add them here so that I know whether to
// 'free' or 'close'.

// Main function.
int main(int argc, char *argv[])
{
	// Set variables.
	char c;
	char *read_file=NULL;
	FILE *xml;
	// 'off_t' is defined in sys/types.h as the file size type. This
	// usually equiv. to 'unsigned long' on *nix.
	off_t buf_size;
	// MADI: Read up structures.
	struct stat file_info;
	// Initial file read buffer
	char *file_contents;
	// The root of the parsed stack
	xml_obj_t * root;
	
	// Pick up switches
	while ((c = getopt (argc, argv, "hf:")) != -1)
	{
		switch (c)
		{
			// Show usage
			case 'h':
				usage(argv[0]);
				return 0;
			break;
			
			// The file to read
			case 'f':
				read_file = optarg;
			break;
			
			// '?' is set if an unknown argument is passed. If the previous
			// option had a ':' after it without something after it, optopt
			// will have the argument so you can report what was missing.
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
			
			// This is a catch all, but shouldn't be hit as '?'
			// should catch all unknown switches.
			default:
				printf("Unexpected error!\n");
				usage(argv[0]);
				return -2;
		}
	}
	
	// Create the stack. The '100' means that I can go as far as 100 child
	// tags. This *should* be enough.
	if (create_stack(STACK_SIZE) != 0)
	{
		// Failed to allocate enough memory.
		printf("Failed to allocate enough memory for a stack depth of: [%d], the error was: %s.\n", STACK_SIZE, strerror(errno));
		return -8;
	}
	
	// Did I get a file?
	if (read_file == NULL)
	{
		printf("No file passed.\n");
		usage(argv[0]);
		// MADI: There should be a 'free' before return.
		return -3;
	}
	
	// Read the file.
	if ((xml = fopen(read_file, "r")) == NULL)
	{
		printf("Failed to open the file: [%s] for reading, error was: %s.\n", read_file, strerror(errno));
		return -4;
	}
	
	// Get the file info.
	// MADI: pass a reference, which read_file is, and the address of
	// 'file_info'.
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
	// This is intentionally backwards so that I read 'buf_size' once,
	// rather that 1 byte 'buf_size' time.
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
	
	/* Start Parsing! */
	
	// Get the pointer to the root object. Parsing error if NULL.
	if ((root = parse_xml_content(file_contents)) != NULL)
	{
		// I could use errno.
		printf("Parsing error!");
		return -9;
	}
	
	// Test.
// 	printf("%s", file_contents);
	
	// Free and destroy the stack
	destroy_stack();
	
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
		// Contents don't end with a 0, contents are not to be trusted.
		return -2;
	}
	
	// Because I am moving good data to the left and the sticking a NULL
	// terminator at the end of the shrunken cleaned data, I can use the
	// same memory space.
	src=dst=contents;
	
	// While I am getting data from source, and while I've not gone past
	// my buffer length...
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
	
	// Ensure that the destination array has a NULL at the end.
	*dst=0;
	
	return 0;
}

// Prints usage
void usage (char *name)
{
	printf("Usage: %s -f /path/to/file.xml\n", name);
}

// This does the actual parsing. Returns a pointer to the root
xml_obj_t * parse_xml_content(char * file_contents)
{
	// Root object, initialized to NULL to make sure I'm not looking at garbage.
	xml_obj_t * root_object = NULL;
	xml_obj_type_t state_type = xml_none; // This stores the type of object we're in.
	
	// Current object
	xml_obj_t * current_object;
	
	// Allocate and wipe the memory for root_object.
	if ((root_object = calloc(1, sizeof(xml_obj_t))) == NULL)
	{
		// Failed to allocate the memory.
		return NULL;
	}
	
	// Set the type of the root object.
	root_object->obj_type = xml_root;
	// the above is equiv. to "*root_object.type"
	current_object = root_object;
	
	// Loop through until I hit 0.
	while (*file_contents != 0)
	{
		// If I am in a CDATA type, only look for the closing ]]>.
		// Do we have the start of some tag?
		if (*file_contents == '<')
		{
			// Yar. < is ALWAYS a delimeter, so \0 it.
			*file_contents = 0;
			file_contents++;
			// This is equal; *file_contents++ = 0; because it sets and /then/ increments.
			
			// Am I going in to a CDATA?
			if (strncmp(file_contents, "![CDATA[", 8) == 0)
			{
				// Set the < to \0 so that I terminate the previous block.
				file_contents += 8;	// Jump past '<![CDATA['
				
				// I want my sibling to point to this new XML object.
				current_object->generic.right = calloc(1, sizeof(xml_obj_t));
				if (current_object->generic.right == NULL)
				{
					// Failed to allocate memory.
					return NULL;
				}
				
				// Make the 'current_object' now point to the memory we just calloc'ed.
				current_object = current_object->generic.right;
				current_object->cdata.obj_type = xml_cdata;	// This lets me know later what I am looking at (xml_cdata)
				current_object->cdata.xmldata = file_contents;	// This will mark the start of the CDATA.
				
				while ((*file_contents != ']') && (strncmp(file_contents+1, "]>", 2) != 0))
				{
					file_contents++;
				}
				
				*file_contents = 0;	// This changes ']]>' to '\0]>'.
				file_contents += 2;	// Jump past the ']>'
				state_type = xml_none;	// Reset to xml_none as I don't know what I will be in the next loop around.
			}
			
			
			
/*			else if ((isalpha(*(file_contents+1)) != 0) || (*(file_contents+1) == '_'))
			{
				// Tag name.
				state_type = xml_tag;
			}
			else if (*(file_contents+1) == '/')
			{
				// Closing a tag
			}
			else if (strncmp(file_contents+1, "![CDATA[", 8) == 0)
			{
				// CDATA block
				state_type = xml_cdata;
			}
			else
			{
				// Bad formatting, can't parse.
				// I could set 'errno = X' to make the caller print a useful error.
				return NULL;
			}*/
		}
// 		else if (strncmp(file_contents, "/>", 2) == 1)
			
/*		}
		// strncmp returns 0 on match, non-zero otherwise.
		else if (strncmp(file_contents, "]]>", 3) == 0)
		{
			// Closing CDATA.
		}*/
		
		// Data, continue.
		file_contents++;
	}
	
	return 0;
}
