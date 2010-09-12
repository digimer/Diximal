/* Diximal; Digimer's XML parser

This is a primitive XML parser written as an excercise in C to help me convert
my Perl knowledge into C experience. It's probably never going to be a useful
program.

Madison Kelly (Digimer)
mkelly@alteeve.com
Sep. 11, 2010

See README for usage and expected behaviour.
*/


/* Inluded libraries. */

// Standard I/O
#include <stdio.h>
// Library for things like 'sizeof()', 'itoa()', rand(), etc.
#include <stdlib.h>
// Unix standard stuff.
#include <unistd.h>
// Error stuff
#include <errno.h>
// Verbose errors, etc.
#include <string.h>
// Data types, structures, etc.
#include <sys/types.h>
// Get file details - See 'man 2 stat'
#include <sys/stat.h>
// Library for testing and character manipulation.
#include <ctype.h>
// I may not need these.
// Library for standard integer types (guarantees the size of an int).
// #include <stdint.h>
// Provides some integer and mathmatical functions line INT_MAX and UNIT_MAX
// #include <limits.h>

// Include my header
#include "stack.h"

// This sets the limit of child tags I can delve in to.
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

// This will be used for attributes and their values found in tags.
typedef struct xml_attr_s {
	char * name;
	char * value;
	struct xml_attr_s * next;
} xml_attr_t;	// This name is use as a short-form for "struct xml_attr_s
		// <name>".

// The name suffix _s helps show that it's a structure (vs. _t for type).
// This is the generic function used to link things together.
struct xml_generic_obj_s {
	// This is a variable of the named type (see in enumeration above)
	xml_obj_type_t obj_type;
	// This points to ourself and points to whatever is to the right, or
	// NULL.
	union xml_obj * right;
};

// This is used for storing pointers to CONTENTS.
struct xml_content_obj_s {
	xml_obj_type_t obj_type;
	union xml_obj * right;
	// Contents between the opening tag and the first non-content element.
	char * xmldata;
};

// This is used for storing the pointers to CDATA contents.
struct xml_cdata_obj_s {
	xml_obj_type_t obj_type;
	union xml_obj * right;
	// This is the string between the opening and closing CDATA stanza.
	char * xmldata;
};

// This is used to store the pointers to the beginning of tags.
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

// This is used to store the special 'root' object; The start of the contents.
struct xml_root_obj_s {
	// This, being root, has no siblings
	xml_obj_type_t obj_type;
	union xml_obj * children;
};


/* Unions */

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
void print_tree(xml_obj_t * current_object);


// If I wanted to have garbage collection, I'd set any variables that I'd need
// to free as globals, set them to NULL, then on error function, loop through
// and any that aren't NULL, free/close. I'd need to add each variable to the
// garbage collection function as I add them here so that I know whether to
// 'free' or 'close'.

// Main function.
int main(int argc, char *argv[])
{
	// Set variables.
	char c;			// The character being read from command line.
	char *read_file=NULL;	// Pointer for the read file, set to NULL.
	FILE *xml;		// The file pointer.
	
	// 'off_t' is defined in sys/types.h as the file size type. This
	// usually equiv. to 'unsigned long' on *nix.
	off_t buf_size;
	
	// This will hold the info on the file I am reading.
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
					// -f was passed without a filename after it.
					printf("Missing filename.\n");
				}
				else
				{
					// A switch was used that wasn't recognized.
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
		printf("Something went boom in 'strip_comments' function.\n");
	}
	
	/* Start Parsing! */
	
// 	printf("%s\n", file_contents);
	
	// Get the pointer to the root object. Parsing error if NULL.
	if ((root = parse_xml_content(file_contents)) == NULL)
	{
		// I could use errno.
		printf("Parsing error!\n");
		return -9;
	}
	
	// Test. This prints whatever is in 'file_contents'. Remove when done
// 	printf("%s", file_contents);
	
	// Print my tree.
	print_tree(root);
	
	// Free and destroy the stack
	destroy_stack();
	
	// Release the memory back to the OS that had been allocated to the
	// file buffer's memory
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
		// I put the most expensive checks on the right as the first
		// check to fail ends all following checks.
		if ((pi == 0) && (com == 0) && (cd == 0) && (*src == '<'))
		{
			if (*(src+1) == '?')
			{
				// In a processing instruction.
				pi=1;
				src++;
				size--;
			}
			// 'strncmp', unlike 'strcmp', looks ahead only X chars
			// for a match. Returns 0 on match.
			else if (strncmp(src+1, "!--", 3) == 0)
			{
				// In a comment
				com=1;
				src+=3;
				size-=3;
			}
			else if (strncmp(src+1, "![CDATA[", 8) == 0)
			{
				// In a CDATA block.
				cd = 1;
			}
		}
		
		// If I am not in a comment of processing instruction, record
		// the character.
		if ((pi == 0) && (com == 0))
		{
			// Push the char from src to dst and move forward one
			// position in dst's array.
// 			printf("%c", *src);
			*dst++ = *src;
		}
		
		// Check for leaving states.
		// The expensive check won't happen unless the '[' char is seen.
		if ((cd == 1) && (*src == ']') && (strncmp(src+1, "]>", 2) == 0))
		{
			cd=0;
			// I don't worry about looping twice more because I
			// know what they are and they won't trigger com or pi.
		}
		// Check for leaving comments.
		if ((com == 1) && (*src == '-') && (strncmp(src+1, "->", 2) == 0))
		{
			com=0;
			// Now jump because I don't want to record the closing
			// brace.
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

// Prints diximal usage.
void usage (char *name)
{
	printf("Usage: %s -f /path/to/file.xml\n", name);
}

// This does the actual parsing. Returns a pointer to the root
xml_obj_t * parse_xml_content(char * file_contents)
{
	// Root object, initialized to NULL to make sure I'm not looking at
	// garbage.
	xml_obj_t * root_object = NULL;
	
	// Current object
	xml_obj_t * current_object;
	
	// Just stores whether the next object is to be stored as a child or sibling.
	int is_child = 0;
	
	// This is a temporary pointer used for comparing strings in closing tags against the string popped off the stack.
	char * close_name = NULL;
	
	// Allocate and wipe the memory for root_object.
	if ((root_object = calloc(1, sizeof(xml_obj_t))) == NULL)
	{
		// Failed to allocate the memory.
		printf("%d\n", __LINE__);
		return NULL;
	}
	
	// Set the type of the root object.
	root_object->obj_type = xml_root;
	// the above is equiv. to "*root_object.type"
	
	// Now, being the beginning, set 'current_object' to 'root_object'.
	current_object = root_object;
	
	// Loop through until I hit 0.
	while (*file_contents != 0)
	{
		// If I am in a CDATA type, only look for the closing ]]>.
		// Do we have the start of some tag?
		if (*file_contents == '<')
		{
			// Yar. < is ALWAYS a delimeter, so \0 it.
			*file_contents++ = 0;
			// This is equal to;
			// *file_contents = 0; file_contents++;
			// because it sets and /then/ increments.
			
			// Am I going in to a CDATA?
			if (strncmp(file_contents, "![CDATA[", 8) == 0)
			{
// printf("In CDATA\n");
				// Jump past '<![CDATA['
				file_contents += 8;
				
				// I want my sibling to point to this new XML
				// object.
				if (is_child == 0)
				{
					current_object->generic.right = calloc(1, sizeof(xml_obj_t));
					if (current_object->generic.right == NULL)
					{
						// Failed to allocate memory.
						printf("%d\n", __LINE__);
						return NULL;
					}
					
					// Make the 'current_object' now point to the
					// memory we just calloc'ed.
					current_object = current_object->generic.right;
				}
				else
				{
					// Make sure the code is sane. This should not happen.
					// I can't create a child unless I am a tag.
					if (current_object->obj_type != xml_tag)
					{
						// Invalid, parser error. Something in the parser broke.
						printf("%d\n", __LINE__);
						return NULL;
					}
					
					// We're the child, make sure the next isn't unless reset.
					is_child = 0;
					
					// CDATA can very much be a child of a tag.
					current_object->xmltag.children = calloc(1, sizeof(xml_obj_t));
					if (current_object->xmltag.children == NULL)
					{
						// Failed to allocate memory.
						printf("%d\n", __LINE__);
						return NULL;
					}
					
					// Push the current object on to the stack.
					if ((push_stack(current_object)) != 0)
					{
						// push_stack returns non-zero on error.
						printf("%d\n", __LINE__);
						return NULL;
					}
					
					// Make the 'current_object' now point to the
					// memory we just calloc'ed.
					current_object = current_object->xmltag.children;
				}
				
				// This lets me know later what I am looking at
				// (xml_cdata)
				current_object->cdata.obj_type = xml_cdata;
				
				// This will mark the start of the CDATA.
				current_object->cdata.xmldata = file_contents;
				
				// Loop through until I see the closing braces.
				// I could do this with just the 'strncmp' but
				// that is somewhat expensive. By checking for
				// a ']', I won't run the 'strncmp' until I
				// have a good reason to do so.
				while ((*file_contents != 0) && (*file_contents != ']') && (strncmp(file_contents+1, "]>", 2) != 0))
				{
					file_contents++;
				}
				
				// This changes ']]>' to '\0]>'.
				*file_contents = 0;
				// Jump past the ']>'
				file_contents += 3;
			}
			else if ((isalpha(*file_contents)) || (*file_contents == '_'))
			{
// printf("In Tag\n");
				// I want my sibling to point to this new XML
				// object.
				if (is_child == 0)
				{
					current_object->generic.right = calloc(1, sizeof(xml_obj_t));
					if (current_object->generic.right == NULL)
					{
						// Failed to allocate memory.
						printf("%d\n", __LINE__);
						return NULL;
					}
					
					// Make the 'current_object' now point to the
					// memory we just calloc'ed.
					current_object = current_object->generic.right;
				}
				else
				{
					// Make sure the code is sane. This should not happen.
					// I can't create a child unless I am a tag.
					if (current_object->obj_type != xml_tag)
					{
						// Invalid, parser error. Something in the parser broke.
						printf("%d\n", __LINE__);
						return NULL;
					}
					
					// We're the child, make sure the next isn't unless reset.
					is_child = 0;
					
					// CDATA can very much be a child of a tag.
					current_object->xmltag.children = calloc(1, sizeof(xml_obj_t));
					if (current_object->xmltag.children == NULL)
					{
						// Failed to allocate memory.
						printf("%d\n", __LINE__);
						return NULL;
					}
					
					// Push the current object on to the stack.
					if ((push_stack(current_object)) != 0)
					{
						// push_stack returns non-zero on error.
						printf("%d\n", __LINE__);
						return NULL;
					}
					
					// Make the 'current_object' now point to the
					// memory we just calloc'ed.
					current_object = current_object->xmltag.children;
				}
				
				// This lets me know later what I am looking at
				// (xml_tag).
				current_object->cdata.obj_type = xml_tag;
				
				// This will mark the start of the tag name.
				current_object->xmltag.tagname = file_contents++;
				
				// Loop through until I see either a space, a
				// closing '>' or a self-terminating '/>'. When
				// I see a space, I will loop until I see a
				// non-space character and determine if it's an
				// attribute or a (self)closing brace.
				while ((*file_contents != 0) && ( ! isspace(*file_contents)) && (*file_contents != '/') && (*file_contents != '>'))
				{
// 					printf("%c", *file_contents);
					file_contents++;
				}
				
				if (*file_contents == 0)
				{
					printf("%d\n", __LINE__);
					return NULL;
				}
				
				// At this point, I've hit the end of the tag
				// name one way or the other. So now I need to
				// mark this as the end of the tag name and
				// then sort out what I am looking at.
				// To start, loop past all white spaces until I
				// see something.
				if (isspace(*file_contents))
				{
					// I am looking at a white space. Set
					// it to '\0' and loop until I find
					// something.
					*file_contents++ = 0;
					
					// Now loop until I see something.
					// MADI: Should this be a function? I
					//       can see this being used again.
					while ((*file_contents != 0) && (isspace(*file_contents)))
					{
						file_contents++;
					}
				}
				
				// At this point, I know I am looking at a
				// non-white-space character.
				if ((isalpha(*file_contents)) || (*file_contents == '_'))
				{
					// I'm looking at the start of an
					// attrib="value" pair. This must allow
					// for white-spaces around the '='
					// sign.
					
					// Loop until I see an end character
					while ((*file_contents != 0) && (*file_contents != '/') && (*file_contents != '>'))
					{
						// parse attributes and values. At this point, I must see the format 'attrib = "value"' with any number of 
						// white spaces.
/*						while ((*file_contents != 0) && (*file_contents != '=') && (isalpha(*file_contents)))
						{
							
						}*/
						
						//cheat
						file_contents++; 
					}
				}
				
				// Now I know I am passed any attributes, let's close up.
				if ((*file_contents == '/') && (*file_contents+=1 == '>'))
				{
					// Self-closing tag.
					
					// Set to \0 because, if there were no spaces after the name, we wouldn't have a terminator of the tag name.
					*file_contents = 0;
					
					// Skip past the next character as I know it's '>'.
					file_contents += 2;
				}
				else if (*file_contents == '>')
				{
					// Tag is closed. Now I'll need to push
					// this tag on to the stack so that
					// when I see a closing tag I can
					// ensure that it matches.
					
					// Set to \0 because, if there were no spaces after the name, we wouldn't have a terminator of the tag name.
					*file_contents++ = 0;
					
					// The next object must be a child of ours.
					is_child = 1;
				}
				else
				{
					// Mal-formed XML.
					printf("%d\n", __LINE__);
					return NULL;
				}
			}
			else if (*file_contents == '/')
			{
				// I'm looking at a closing tag. I'll need to
				// make sure it matches the last opening tag.
				
				// If 'is_child' is set, then I might be looking at the closing tag with no CONTENT. Thus, I don't want to pop off the stack.
				if (is_child == 0)
				{
					// I need to get the last value off the stack.
					current_object = pop_stack();
					if (current_object == NULL)
					{
						// Didn't get anything off the stack, bad XML.
						printf("%d\n", __LINE__);
						return NULL;
					}
				}
				
				// Now clear it out.
				is_child = 0;
				
				file_contents++;
				// Make 'close_name' point to the file contents.
				close_name = file_contents;
				
				// Loop until I see the closing tag name end condition.
				while ((*file_contents != 0) && (*file_contents != '>') && (!isspace(*file_contents)))
				{
					file_contents++;
				}
				
				// if I am looking at a space, for forward until I see '>' or die if anything else.
				if (isspace(*file_contents))
				{
					*file_contents=0;
					while ((*file_contents != 0) && (isspace(*file_contents)))
					{
						file_contents++;
					}
					if (*file_contents != '>')
					{
						// Saw the wrong character, bad XML.
						printf("%d\n", __LINE__);
						return NULL;
					}
				}
				
				// End of the close tag found.
				*file_contents++ = 0;
				
				// Make sure that the current object is an XML tag. This is guaranteed when popping off the stack, but we didn't
				// necessarily pop off the stack.
				if (current_object->obj_type != xml_tag)
				{
					// Whut?
					printf("%d\n", __LINE__);
					return NULL;
				}
				
				// Do the actual compare.
				if (strcmp(close_name, current_object->xmltag.tagname) != 0)
				{
					// Tags don't match.
					printf("%d\n", __LINE__);
					return NULL;
				}
			}
			else
			{
				// Bad formatting, can't parse.
				// I could set 'errno = X' to make the caller print a useful error.
				printf("%d\n", __LINE__);
				return NULL;
			}
		}
		else
		{
			// I'm looking at CONTENT then.
			if (is_child == 0)
			{
				current_object->generic.right = calloc(1, sizeof(xml_obj_t));
				if (current_object->generic.right == NULL)
				{
					// Failed to allocate memory.
					printf("%d\n", __LINE__);
					return NULL;
				}
				
				// Make the 'current_object' now point to the
				// memory we just calloc'ed.
				current_object = current_object->generic.right;
			}
			else
			{
				// Make sure the code is sane. This should not happen.
				// I can't create a child unless I am a tag.
				if (current_object->obj_type != xml_tag)
				{
					// Invalid, parser error. Something in the parser broke.
					printf("%d\n", __LINE__);
					return NULL;
				}
				
				// We're the child, make sure the next isn't unless reset.
				is_child = 0;
				
				// CDATA can very much be a child of a tag.
				current_object->xmltag.children = calloc(1, sizeof(xml_obj_t));
				if (current_object->xmltag.children == NULL)
				{
					// Failed to allocate memory.
					printf("%d\n", __LINE__);
					return NULL;
				}
				
				// Push the current object on to the stack.
				if ((push_stack(current_object)) != 0)
				{
					// push_stack returns non-zero on error.
					printf("%d\n", __LINE__);
					return NULL;
				}
				
				// Make the 'current_object' now point to the
				// memory we just calloc'ed.
				current_object = current_object->xmltag.children;
			}
			
			// This lets me know later what I am looking at
			// (xml_tag).
			current_object->cdata.obj_type = xml_content;
			
			// This will mark the start of the tag name.
			current_object->content.xmldata = file_contents;
			
			// Loop through until we see a '<' or \0.
			while ((*file_contents != 0) && (*file_contents != '<'))
			{
				file_contents++;
			}
		}
	}
	
	return root_object;
}

// Print the tree.
void print_tree(xml_obj_t * current_object)
{
	// Do I actually have something?
	if (current_object == NULL)
	{
		// Because this is recursive, it is expected to sometimes be NULL so this isn't an error.
		return;
	}
	
	// If this is the root, I need to get the pointer to the first child.
	if (current_object->obj_type == xml_root)
	{
		current_object = current_object->xmlroot.children;
	}
	
	// For now, this reconstructs.
	while (current_object != NULL)
	{
		if (current_object->obj_type == xml_cdata)
		{
			printf("<![CDATA[%s]]>", current_object->cdata.xmldata);
		}
		if (current_object->obj_type == xml_content)
		{
			printf("%s", current_object->content.xmldata);
		}
		if (current_object->obj_type == xml_tag)
		{
			
			printf("<%s", current_object->xmltag.tagname);
			
			
			if(current_object->xmltag.children==NULL)
			{
				printf(" />");
			}
			else 
			{
				printf(">");
				print_tree(current_object->xmltag.children);
				printf("</%s>", current_object->xmltag.tagname);
			}
		}
		current_object = current_object->generic.right;
	}
}
