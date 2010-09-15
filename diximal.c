/* Diximal; Digimer's XML parser

This is a primitive XML parser written as an excercise in C to help me convert
my Perl knowledge into C experience. It's probably never going to be a useful
program.

Madison Kelly (Digimer)
mkelly@alteeve.com
Sep. 14, 2010

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

// Include my stack header.
#include "stack.h"

// This sets the limit of child tags I can delve in to.
#define STACK_SIZE	100

/* Enumeration */

typedef enum {
	// These are being assigned sequential integer values by the compile
	// (xml_content = 0, ... )
	xml_none,	// Initial type.
	xml_root,	// This type is set at the root of the parsed tree.
	xml_content,	// While reading CONTENT.
	xml_cdata,	// While in a CDATA block.
	xml_tag		// While parsing a tag.
} xml_obj_type_t;


/* Unions */

// This has to come first as it's a forward decleration. This means that I can
// reference it in my structures before the unions defines the connections of
// the structures.
union xml_obj;


/* structures */

// This will be used for attributes and their values found in tags. It includes
// a pointer to the next, if any, attribute for the given tag.
typedef struct xml_attr_s
{
	char * name;
	char * value;
	struct xml_attr_s * next;
} xml_attr_t;	// This name is used as a short-form for "struct xml_attr_s
		// <name>".

// The name suffix '_s' helps show that it's a structure (vs. '_t' for type).
// This is the generic structure used to link things together.
struct xml_generic_obj_s
{
	// This is a variable that stores the named type (the enumerations
	// above).
	xml_obj_type_t obj_type;
	
	// This points to whatever is to the right in the linked list, or NULL
	// if nothing.
	union xml_obj * right;
};

// This is used for storing pointers to CONTENTS.
struct xml_content_obj_s
{
	xml_obj_type_t obj_type;
	union xml_obj * right;
	
	// This will be the pointer at the start of the CONTENTs.
	char * xmldata;
};

// This is used for storing the pointers to CDATA.
struct xml_cdata_obj_s
{
	xml_obj_type_t obj_type;
	union xml_obj * right;
	
	// This is a pointer the the start of the CDATA string.
	char * xmldata;
};

// This is used to store the pointers to the beginning of tags. It can have
// child tags.
struct xml_tag_obj_s
{
	xml_obj_type_t obj_type;
	union xml_obj * right;
	
	// This points to the start of the tag name.
	char * tagname;
	
	// This is a pointer to the first (if any) linked list of attributes.
	xml_attr_t * attributes;
	
	// Flag to indicate presence of child tags. Set to '1' when there are
	// children. This is used to speed up the decision making when it comes
	// to whether CONTENTS will be printed, which will only happen on leaf
	// tags.
	int has_child_tag;
	
	// Pointer to child tags, if any.
	union xml_obj * children;
};

// This is used to store the special 'root' object; The start of the XML
// contents tree.
struct xml_root_obj_s
{
	// This, being root, has no siblings
	xml_obj_type_t obj_type;
	union xml_obj * children;
};


/* Unions */

// This union is used to help me identify what I can print when I look at an
// object's 'obj_type'.
typedef union xml_obj
{
	// The object type is accessed here.
	xml_obj_type_t obj_type;
	struct xml_generic_obj_s generic;
	struct xml_content_obj_s content;
	struct xml_cdata_obj_s   cdata;
	struct xml_root_obj_s    xmlroot;
	struct xml_tag_obj_s     xmltag;
} xml_obj_t;


/* prototypes */

// Prints the help message and exits.
void usage(char *name);

// This does an initial pass of the file and strips out comments.
int strip_comments(char *contents, signed long size);

// This is the main parser function.
xml_obj_t * parse_xml_content(char * file_contents);

// This is called when I've exited early. It reconstructs the XML to the point
// of the error.
void print_broken_tree(xml_obj_t * current_object);

// This is the parent function for printing the XML tree in the desired format.
void print_tree(xml_obj_t * current_object);

// This is the recursive portion of the tree printer.
void print_tree_recursive(xml_obj_t * current_object, int has_tags, char * path);

// This function parses an 'attribute="value"' pair in a tag.
char * parse_attribute_value_pairs(char * file_contents, xml_obj_t * current_object);

// This cleans up by running through the object tree, freeing memory as it
// goes.
void free_tree_mem(xml_obj_t * current_object);


// If I wanted to have garbage collection, I'd set any variables that I'd need
// to free as globals, set them to NULL, and then on error, loop through any
// that aren't NULL and call free/close against them. I'd need to add each
// variable to the garbage collection function as I add them here so that I
// know whether to use 'free' or 'close'.

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
	
	// This will hold the information returned by 'stat' on the file I am
	// reading. The most important being the size.
	struct stat file_info;
	
	// Initial file read buffer. This will be where I will do all my work.
	char *file_contents;
	
	// The root of the parsed stack.
	xml_obj_t * root;
	
	// Pick up command line switches.
	while ((c = getopt (argc, argv, "hf:")) != -1)
	{
		// 'c' is the switch I am looking at just now.
		switch (c)
		{
			// Show usage
			case 'h':
				usage(argv[0]);
				return 0;
			break;
			
			// The file to read. This requires a value and will
			// return '?' on the next pass if none was found.
			case 'f':
				read_file = optarg;
			break;
			
			// '?' is set if an unknown argument is passed. If the
			// previous option had a ':' after it and there was no
			// following value, 'optopt' will have the argument so
			// I can report what was missing.
			case '?':
				if (optopt == 'f')
				{
					// -f was passed without a filename
					// after it.
					printf("Missing filename.\n");
				}
				else
				{
					// An undefined switch was used.
					printf("Invalid switch '%c'\n", optopt);
				}
				usage(argv[0]);
				return -1;
			break;
			
			// This is a catch all, but shouldn't be hit as '?'
			// should catch all unknown switches.
			default:
				printf("Unexpected error while reading command line switches!\n");
				usage(argv[0]);
				return -2;
		}
	}
	
	// Create the stack. The 'STACK_SIZE' is an integer that sets the
	// maximum number of child tags I can create.
	if (create_stack(STACK_SIZE) != 0)
	{
		// Failed to allocate enough memory.
		printf("Failed to allocate enough memory for a stack depth of: [%d]\nThe error was: %s.\n", STACK_SIZE, strerror(errno));
		return -3;
	}
	
	// This might be hit if no '-f' argument was passed.
	if (read_file == NULL)
	{
		printf("No file specified, nothing to parse.\n");
		usage(argv[0]);
		return -4;
	}
	
	// Try to read the file.
	if ((xml = fopen(read_file, "r")) == NULL)
	{
		printf("Failed to open the file: [%s] for reading.\nThe error was: %s.\n", read_file, strerror(errno));
		return -5;
	}
	
	// Pass a reference to 'file_info', which 'stat' will store the
	// information on the 'read_file' in.
	if (stat(read_file, &file_info) != 0)
	{
		printf("Failed to get the 'stat' info for the file: [%s]\n The error was: %s.\n", read_file, strerror(errno));
		return -6;
	}
	
	// Get the size of the buffer needed to read in the contents of the XML
	// file out of the 'file_info' structure. The extra space is for the
	// final \0.
	buf_size=(file_info.st_size+1);
	
	// Allocate the memory for the XML file contents. I realize that this
	// method would fail if the XML file size was greater than the
	// available free memory. Part of the 'primitive' parser.
	// 'calloc' is like 'malloc', but it zeros out the allocated memory.
	if ((file_contents=calloc(buf_size, 1)) == NULL)
	{
		printf("Failed to allocate memory for the XML file: %s]\nThe error was: %s.\n", read_file, strerror(errno));
		return -7;
	}
	
	// Read the XML file in to the 'file_contents' buffer. This is
	// intentionally backwards so that I read 'buf_size' once rather that
	// 1 byte 'buf_size' times.
	if ((fread(file_contents, file_info.st_size, 1, xml)) != 1)
	{
		printf("Failed to read into memory the file: [%s].\nThe error was: %s.\n", read_file, strerror(errno));
		return -8;
	}
	
	// Close the XML file, it's all in memory now!
	fclose(xml);
	
	// Strip comments.
	if (strip_comments(file_contents, buf_size) != 0)
	{
		// MADI: Parse the returned error later.
		printf("There was an error in the 'strip_comments()' function.\n");
		return -9;
	}
	
	// Get the pointer to the root object. Null is return if there was a
	// parsing error.
	if ((root = parse_xml_content(file_contents)) == NULL)
	{
		// I could use errno.
		printf("\n\nERROR\nThe above XML represents what I successfully parsed.\nPlease see the top of the XML output for a more detailed error.\n\n");
		return -10;
	}
	
	// Print the XML tree!
	print_tree(root);
	
	// Free and destroy the stack
	destroy_stack();
	
	// Free the memory that had been allocated to the XML file contents.
	// This doesn't actually clear the contents though. If security was a
	// real concern, I'd want to flush it.
	free(file_contents);
	
	// If I hit here, I am exiting normally.
	return 0;
}


/* functions */

// Prints the help message and exits.
void usage (char *name)
{
	printf("Usage: %s -f /path/to/file.xml\n", name);
}

// This does an initial pass of the file and strips out comments.
int strip_comments(char *contents, signed long size)
{
	// The source and destination variables will be set initially to the
	// same pointer as in 'file_contents'.
	char *src, *dst;
	
	// State variables; Processing Instrunctions, Comment and CDATA.
	int pi=0, com=0, cd=0;
	
	// Before I do anything, make sure that my contents have something and
	// terminate in a final \0.
	if (contents == NULL)
	{
		// Error, passed an empty reference.
		return -1;
	}
	if (contents[size-1] != 0)
	{
		// Contents don't end with a 0, contents are not to be trusted.
		return -1;
	}
	
	// Because I am moving good data to the left and then sticking a NULL
	// terminator at the end of the shrunken cleaned data, I can use the
	// same memory space. Start by setting 'src' and 'dst' to 'contents'.
	src = dst = contents;
	
	// While I am getting data from source, and while I've not gone past
	// my buffer length...
	while((*src != 0) && (size > 0))
	{
		// If this is the less-than sign, evaluate.
		// I put the most expensive checks on the right as the first
		// check to fail aborts all following checks.
		if ((pi == 0) && (com == 0) && (cd == 0) && (*src == '<'))
		{
			// If the character after '<' is '?', I am looking at
			// a processing instruction.
			if (*(src+1) == '?')
			{
				pi=1;
				src++;
				size--;
			}
			// 'strncmp', unlike 'strcmp', looks ahead only X chars
			// for a match. Returns 0 on match. If the following
			// three characters are '!---', I am looking at the
			// start of a comment.
			else if (strncmp(src+1, "!--", 3) == 0)
			{
				com=1;
				src+=3;
				size-=3;
			}
			// If the following eight characters are '![CDATA['
			// then I am looking at the start of a CDATA block.
			else if (strncmp(src+1, "![CDATA[", 8) == 0)
			{
				cd = 1;
				// I don't want to strip the '<![CDATA[' as I
				// will need it later.
			}
		}
		
		// If I am not in a comment of processing instruction, record
		// the character.
		if ((pi == 0) && (com == 0))
		{
			// Copy the character from 'src' to 'dst' and move
			// forward one position in dst's array.
			*dst++ = *src;
		}
		
		/* Check for leaving states. */
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

// This is the main parser function.
xml_obj_t * parse_xml_content(char * file_contents)
{
	// Root object, initialized to NULL to make sure I'm not looking at
	// garbage.
	xml_obj_t * root_object = NULL;
	
	// Current and parent objects.
	xml_obj_t * current_object;
	xml_obj_t * parent_object;
	
	// This stores whether the next object is to be stored as a child (1)
	// or sibling (0).
	int is_child = 0;
	
	// This is a temporary pointer used for comparing strings in closing
	// tags against the string popped off the stack.
	char * close_name = NULL;
	
	// Allocate and wipe the memory for root_object.
	if ((root_object = calloc(1, sizeof(xml_obj_t))) == NULL)
	{
		// Failed to allocate the memory.
		printf("Failed to allocate memory for the root object in 'parse_xml_content()'.\nThe error was: %s.\n", strerror(errno));
		return NULL;
	}
	
	// Set the type of the root object.
	root_object->obj_type = xml_root;
	
	// Now, being the beginning, set 'current_object' to 'root_object'.
	current_object = root_object;
	
	// Loop through until I hit \0.
	while (*file_contents != 0)
	{
		// Do we have the start of some tag?
		if (*file_contents == '<')
		{
			// Yar. '<' is ALWAYS a delimeter, so \0 it.
			*file_contents++ = 0;
			
			// Am I going in to a CDATA?
			if (strncmp(file_contents, "![CDATA[", 8) == 0)
			{
				// Yes, jump past '<![CDATA['.
				file_contents += 8;
				
				// Is this a child or a sibling?
				if (is_child == 0)
				{
					// Sibling. Allocate memory for the
					// next object and store it in the
					// 'generic.right' pointer. 
					current_object->generic.right = calloc(1, sizeof(xml_obj_t));
					if (current_object->generic.right == NULL)
					{
						// Failed to allocate memory.
						printf("Failed to allocate memory for sibling object in 'parse_xml_content()'.\nThe error was: %s.\n", strerror(errno));
						free_tree_mem(root_object);
						return NULL;
					}
					
					// Make the 'current_object' now point to the
					// memory we just calloc'ed.
					current_object = current_object->generic.right;
				}
				else
				{
					// Child. Make sure the code is sane.
					// I can't create a child unless I am a tag.
					if (current_object->obj_type != xml_tag)
					{
						// Invalid, parser error. Something in the parser broke.
						printf("Tried to parse attributes when not in an 'xml_tag' in 'parse_xml_content()'.\n");
						print_broken_tree(root_object);
						free_tree_mem(root_object);
						return NULL;
					}
					
					// We want to clear the child state and
					// let it be reset later if
					// appropriate.
					is_child = 0;
					
					// Allocate memory for the child and
					// store the pointer in
					// 'xmltag.children'.
					current_object->xmltag.children = calloc(1, sizeof(xml_obj_t));
					if (current_object->xmltag.children == NULL)
					{
						// Failed to allocate memory.
						printf("Failed to allocate memory for child object in 'parse_xml_content()'.\nThe error was: %s.\n", strerror(errno));
						free_tree_mem(root_object);
						return NULL;
					}
					
					// Push the current object on to the stack.
					if ((push_stack(current_object)) != 0)
					{
						// push_stack returns non-zero on error.
						printf("Failed to push the current child tag on to the stack in 'parse_xml_content()'.\n");
						print_broken_tree(root_object);
						free_tree_mem(root_object);
						return NULL;
					}
					
					// Make the 'current_object' point to
					// the memory we just calloc'ed.
					current_object = current_object->xmltag.children;
				}
				
				// This lets me know later what I am looking at
				// ('xml_cdata', in this case.)
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
				// Get the pointer at the top of the stack.
				parent_object = peek_stack(0);
				
				// If it's not NULL, note it so that I'll know
				// what to print later.
				if (parent_object != NULL)
				{
					parent_object->xmltag.has_child_tag = 1;
				}
				
				// I want my child or sibling to point to this
				// new XML object.
				if (is_child == 0)
				{
					current_object->generic.right = calloc(1, sizeof(xml_obj_t));
					if (current_object->generic.right == NULL)
					{
						// Failed to allocate memory.
						printf("Failed to allocate memory for sibling tag in 'parse_xml_content()'.\nThe error was: %s.\n", strerror(errno));
						free_tree_mem(root_object);
						return NULL;
					}
					
					// Make the 'current_object' now point
					// to the memory we just calloc'ed.
					current_object = current_object->generic.right;
				}
				else
				{
					// Make sure the code is sane. I can't
					// create a child unless I am a tag.
					if (current_object->obj_type != xml_tag)
					{
						// Invalid, parser error. 
						// Something in the parser
						// broke.
						printf("Tried to parse attributes when not in an 'xml_tag' in 'parse_xml_content()'.\n");
						print_broken_tree(root_object);
						free_tree_mem(root_object);
						return NULL;
					}
					
					// Clear the child state as I don't
					// know what the next object will be.
					is_child = 0;
					
					// Allocate memory.
					current_object->xmltag.children = calloc(1, sizeof(xml_obj_t));
					if (current_object->xmltag.children == NULL)
					{
						// Failed to allocate memory.
						printf("Failed to allocate memory for child tag in 'parse_xml_content()'.\nThe error was: %s.\n", strerror(errno));
						free_tree_mem(root_object);
						return NULL;
					}
					
					// Push the current object on to the stack.
					if ((push_stack(current_object)) != 0)
					{
						// push_stack returns non-zero on error.
						printf("Failed to push the new child tag on to the stack in 'parse_xml_content()'.\n");
						print_broken_tree(root_object);
						free_tree_mem(root_object);
						return NULL;
					}
					
					// Make the 'current_object' now point
					// to the memory we just calloc'ed.
					current_object = current_object->xmltag.children;
				}
				
				// Record the object type.
				current_object->cdata.obj_type = xml_tag;
				
				// This will mark the start of the tag name.
				current_object->xmltag.tagname = file_contents++;
				
				// Loop through until I see either a space, a
				// closing '>' or a self-terminating '/>'. When
				// I see a space, I will loop until I see a
				// non-space character and determine if it's an
				// attribute or a (self)closing brace.
				while ((*file_contents != 0) && ( ! isspace(*file_contents))
					&& (*file_contents != '/') && (*file_contents != '>'))
				{
					file_contents++;
				}
				
				// If the loop exited because I hit \0, there
				// was a parsing error.
				if (*file_contents == 0)
				{
					printf("Premature exit of XML data while parsing tag in 'parse_xml_content()'.\n");
					print_broken_tree(root_object);
					free_tree_mem(root_object);
					return NULL;
				}
				
				// At this point, I've hit the end of the tag
				// name one way or the other. So now I need to
				// mark this as the end of the tag name and
				// then sort out what I am looking at. To
				// start, loop past all white spaces until I
				// see something.
				if (isspace(*file_contents))
				{
					// I am looking at a white space. Set
					// it to '\0' and loop until I find
					// something.
					*file_contents++ = 0;
					
					// Now loop until I see something.
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
						// Parse out the 
						// variable="value" pair.
						if ((file_contents = parse_attribute_value_pairs(file_contents, current_object)) == NULL)
						{
							// Something went wrong while parsing attributes.
							printf("Error parsing attribute-value pairs in 'parse_xml_content()'.\n");
							print_broken_tree(root_object);
							free_tree_mem(root_object);
							return NULL;
						}
						
						// Eat up any trailing spaces
						// so that I don't re-enter if
						// there are spaces before the
						// '/>'.
						while ((*file_contents != 0) && (isspace(*file_contents)))
						{
							file_contents++;
						}
						
						// If the loop exited because I
						// hit \0, there was a parsing
						// error.
						if (*file_contents == 0)
						{
							printf("Premature exit of XML data while parsing attributes in 'parse_xml_content()'.\n");
							print_broken_tree(root_object);
							free_tree_mem(root_object);
							return NULL;
						}
					}
				}
				
				// I know that I am passed any attributes, so I
				// can look for the close.
				if ((*file_contents == '/') && (*file_contents+=1 == '>'))
				{
					// This is a self-closing tag.
					// Set to \0 because, if there was no
					// spaces after the name, we wouldn't
					// have a terminator of the tag name.
					*file_contents = 0;
					
					// Skip past the next character as I
					// know it's '>'.
					file_contents += 2;
				}
				else if (*file_contents == '>')
				{
					// Set to \0 because, if there were no
					// spaces after the name, we wouldn't
					// have a terminator of the tag name.
					*file_contents++ = 0;
					
					// The next object must be a child of ours.
					is_child = 1;
				}
				else
				{
					// Malformed XML.
					printf("Parse error. Failed to find a closing brace for tag in 'parse_xml_content()'.\n");
					print_broken_tree(root_object);
					free_tree_mem(root_object);
					return NULL;
				}
			}
			else if (*file_contents == '/')
			{
				// I'm looking at a closing tag. 
				// If 'is_child' is set, then I might be
				// looking at the closing tag with no CONTENT.
				// Thus, I don't want to pop off the stack.
				if (is_child == 0)
				{
					// I need to get the last value off the
					// stack.
					current_object = pop_stack();
					if (current_object == NULL)
					{
						// Didn't get anything off the
						// stack, bad XML.
						printf("Parse error. I saw a closing tag without a prior opening tag in 'parse_xml_content()'.\n");
						print_broken_tree(root_object);
						free_tree_mem(root_object);
						return NULL;
					}
				}
				
				// Clear the child state.
				is_child = 0;
				
				// Advance one as I am no longer interested in
				// the '/'.
				file_contents++;
				
				// Make 'close_name' match to the
				// 'file_contents' pointer.
				close_name = file_contents;
				
				// Loop until I see the closing tag name end
				// condition.
				while ((*file_contents != 0) && (*file_contents != '>') && (!isspace(*file_contents)))
				{
					file_contents++;
				}
				
				// If the loop exited because I hit \0, there
				// was a parsing error.
				if (*file_contents == 0)
				{
					printf("Premature exit of XML data while parsing closing tag in 'parse_xml_content()'.\n");
					print_broken_tree(root_object);
					free_tree_mem(root_object);
					return NULL;
				}
				
				// If I am looking at a space, loop forward
				// until I see '>' or die if anything else.
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
						printf("Parsing error while searching for '>' in closing tag.\n");
						print_broken_tree(root_object);
						free_tree_mem(root_object);
						return NULL;
					}
				}
				
				// End of the close tag found. Set it to \0 and
				// move forward one character.
				*file_contents++ = 0;
				
				// Make sure that the current object is an XML
				// tag. 
				if (current_object->obj_type != xml_tag)
				{
					// Whut?
					printf("Incorrent object type while parsing closing tag in 'parse_xml_content()'.\n");
					print_broken_tree(root_object);
					free_tree_mem(root_object);
					return NULL;
				}
				
				// Do the actual compare.
				if (strcmp(close_name, current_object->xmltag.tagname) != 0)
				{
					// Tags don't match.
					printf("Parse error. Closing tag does not match last opening tag.\n");
					print_broken_tree(root_object);
					free_tree_mem(root_object);
					return NULL;
				}
			}
			else
			{
				// Bad formatting, can't parse.
				printf("Parse error. Invalid character after '<'.\n");
				print_broken_tree(root_object);
				free_tree_mem(root_object);
				return NULL;
			}
		}
		else
		{
			// I'm looking at content.
			if (is_child == 0)
			{
				current_object->generic.right = calloc(1, sizeof(xml_obj_t));
				if (current_object->generic.right == NULL)
				{
					// Failed to allocate memory.
					printf("Failed to allocate memory for sibling content in 'parse_xml_content()'.\nThe error was: %s.\n", strerror(errno));
					free_tree_mem(root_object);
					return NULL;
				}
				
				// Make the 'current_object' now point to the
				// memory we just calloc'ed.
				current_object = current_object->generic.right;
			}
			else
			{
				// Make sure the code is sane. I can't create
				// a child unless I am a tag.
				if (current_object->obj_type != xml_tag)
				{
					// Invalid, parser error. Something in
					// the parser broke.
					printf("Tried to parse child content when not in an 'xml_tag' object in 'parse_xml_content()'.\n");
					print_broken_tree(root_object);
					free_tree_mem(root_object);
					return NULL;
				}
				
				// Clear the child state.
				is_child = 0;
				
				// Allocate memory for the child object.
				current_object->xmltag.children = calloc(1, sizeof(xml_obj_t));
				if (current_object->xmltag.children == NULL)
				{
					// Failed to allocate memory.
					printf("Failed to allocate memory in for child content in 'parse_xml_content()'.\nThe error was: %s.\n", strerror(errno));
					free_tree_mem(root_object);
					return NULL;
				}
				
				// Push the current object on to the stack.
				if ((push_stack(current_object)) != 0)
				{
					// 'push_stack' returns non-zero on
					// error.
					printf("Error pushing the child content on to the stack.\n");
					print_broken_tree(root_object);
					free_tree_mem(root_object);
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
			
			// I might legitimately hit \0 here, so I don't check
			// for it.
		}
	}
	
	return root_object;
}

// This cleans up by running through the object tree, freeing memory as it
// goes.
void free_tree_mem(xml_obj_t * current_object)
{
	// Create two attribute pointers and an object pointer.
	xml_attr_t *current_attr, *next_attr;
	xml_obj_t *next_obj;
	
	// Do I actually have something?
	if (current_object == NULL)
	{
		// Nothing to free.
		return;
	}
	
	// If this is the root, I need to get the pointer to the first child.
	if (current_object->obj_type == xml_root)
	{
		// Got the pointer.
		next_obj = current_object->xmlroot.children;
		
		// Free the memory.
		free(current_object);
		
		// Set 'current_object' to the stored next object.
		current_object = next_obj;
	}
	
	// This travels the tree.
	while (current_object != NULL)
	{
		// If I am looking at a tag, check for attributes.
		if (current_object->obj_type == xml_tag)
		{
			// Tag, see if there is an attribute object.
			current_attr = current_object->xmltag.attributes;
			
			// Loop through the first and all linked attribute 
			// objects, freeing the memory as I go.
			while (current_attr != NULL)
			{
				// Store the next position, free the memory,
				// then reset 'current_attr' to the next one
				// in the list.
				next_attr = current_attr->next;
				free(current_attr);
				current_attr = next_attr;
			}
			
			// Done looking for attributes. Free the tag's memory.
			free_tree_mem(current_object->xmltag.children);
		}
		
		// Record the sibling's pointer, free this object and then step
		// forward.
		next_obj = current_object->generic.right;
		free(current_object);
		current_object = next_obj;
	}
}

// This is called when I've exited early. It reconstructs the XML to the point
// of the error. It's not perfect...
void print_broken_tree(xml_obj_t * current_object)
{
	xml_attr_t *current_attr;
	
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
	
	// Walk through the tree.
	while (current_object != NULL)
	{
		// Print CDATA.
		if (current_object->obj_type == xml_cdata)
		{
			printf("<![CDATA[%s]]>", current_object->cdata.xmldata);
		}
		
		// Print content.
		if (current_object->obj_type == xml_content)
		{
			printf("%s", current_object->content.xmldata);
		}
		
		// Print XML tags. This is a bit trickier as there may be zero
		// or more attribute="value" pairs.
		if (current_object->obj_type == xml_tag)
		{
			// Print the start of the tag.
			printf("<%s", current_object->xmltag.tagname);
			
			// Get the pointer to the first attribute, if any.
			current_attr = current_object->xmltag.attributes;
			
			// Walk through the attribute linked list until 'next'
			// is NULL.
			while (current_attr != NULL)
			{
				// Format the attibute="value" pair.
				printf(" %s=\"%s\"", current_attr->name, current_attr->value);
				
				// And then step right.
				current_attr = current_attr->next;
			}
			
			// There should be no more children, so close up.
			if (current_object->xmltag.children == NULL)
			{
				printf(" />");
			}
			else 
			{
				// Here is where things went wrong.
				printf(">");
				print_broken_tree(current_object->xmltag.children);
				// I don't print the closing tag as that might
				// have been the problem.
			}
		}
		// Step right.
		current_object = current_object->generic.right;
	}
}

// This is the parent function for printing the XML tree in the desired format.
void print_tree(xml_obj_t * current_object)
{
	// I print newlines at the start of strings so that I don't show
	// useless blank lines in the final output. For this resson, I don't
	// have a '\n' after the 'START'.
	printf("START");
	
	// Jump in to the recursive printer.
	print_tree_recursive(current_object, 1, NULL);
	
	// Close up.
	printf("\nEND\n");
}

// This is the recursive portion of the tree printer.
void print_tree_recursive(xml_obj_t * current_object, int has_tags, char * path)
{
	// Set a couple variables to build the double-colon separated chain of
	// tags (the 'path').
	xml_attr_t *current_attr;
	int pathlen;
	char * newpath;
	
	// Do I actually have something?
	if (current_object == NULL)
	{
		// Because this is recursive, it is expected to sometimes be
		// NULL so this isn't an error.
		return;
	}
	
	// If this is the root, I need to get the pointer to the first child.
	if (current_object->obj_type == xml_root)
	{
		current_object = current_object->xmlroot.children;
	}
	
	// Walk through the tree.
	while (current_object != NULL)
	{
		// If the 'has_tags' state variable is '0' then I know I am at
		// a leaf tag and thus want to print the CONTENT (which may
		// include CDATA bits).
		if (has_tags == 0)
		{
			// Print CDATA chunks
			if (current_object->obj_type == xml_cdata)
			{
				printf("%s", current_object->cdata.xmldata);
			}
			
			// Print the rest of the content.
			if (current_object->obj_type == xml_content)
			{
				printf("%s", current_object->content.xmldata);
			}
		}
		
		// If I am looking at a tag, append the 'variable=value' to the
		// chain of tags.
		if (current_object->obj_type == xml_tag)
		{
			// Get the length of the path.
			pathlen = strlen(current_object->xmltag.tagname);
			
			// Allocate or extend the memory for path.
			if (path == NULL)
			{
				// Start of the path.
				if ((newpath = calloc(1, pathlen+1)) == NULL)
				{
					printf("Failed to allocate memory for 'newpath' in 'print_tree_recursive()'.\nThe error was: %s\n", strerror(errno));
				}
				// Copy the tag name on to the path.
				strcpy(newpath, current_object->xmltag.tagname);
			}
			else
			{
				// Appending to the path.
				pathlen += strlen(path) + 2;
				if ((newpath = calloc(1, pathlen+1)) == NULL)
				{
					printf("Failed to allocate memory when appending 'newpath' in 'print_tree_recursive()'.\nThe error was: %s\n", strerror(errno));
				}
				// Print the new extended path.
				sprintf(newpath, "%s::%s", path,  current_object->xmltag.tagname);
			}
			
			// Print attributes for this tag, if any.
			current_attr = current_object->xmltag.attributes;
			
			// Print the path. Attribute(s) will follow.
			printf("\n%s", newpath);
			while (current_attr != NULL)
			{
				// Print the path and append the attribute
				// 'name=value' suffix.
				printf("\n%s::", newpath);
				printf("%s=%s", current_attr->name, current_attr->value);
				
				// Step right in the attribute linked list.
				current_attr = current_attr->next;
			}
			
			// If I have a child object, I need to recurse into it.
			if (current_object->xmltag.children != NULL)
			{
				// Before I do though, check if I have any
				// children. If not, this is a leaf tag and I
				// will want to print the '::CONTENT=' suffix.
				// The actual content will come in the next
				// loop.
				if (current_object->xmltag.has_child_tag == 0)
				{
					printf("\n%s::CONTENT=", newpath);
				}
				print_tree_recursive(current_object->xmltag.children, current_object->xmltag.has_child_tag, newpath);
			}
			// Now that I am done, free the 'newpath'. This will be
			// regenerated with the appropriate path on the next
			// loop.
			free(newpath);
		}
		// Step to the right.
		current_object = current_object->generic.right;
	}
}

// This function parses an 'attribute="value"' pair in a tag.
char* parse_attribute_value_pairs(char* file_contents, xml_obj_t* current_object)
{
	// At this point, I should be looking at the first character in the
	// attribute name. I will need to set it's pointer.
	xml_attr_t *current_attr = NULL;
	
	// Make sure the code is sane. This should not happen. I can't be
	// parsing attributes unless I am in a tag.
	if (current_object->obj_type != xml_tag)
	{
		// Invalid, parser error. Something in the parser broke.
		printf("Tried to parse attributes when not in an 'xml_tag' in 'parse_attribute_value_pairs()'.\n");
		return NULL;
	}
	
	// If this is the first attribute for the tag, create this attributes
	// pointer on the tag's 'attribute' pointer. Otherwise, create this
	// pointer on the last attribute's 'next' pointer.
	if (current_object->xmltag.attributes == NULL)
	{
		// First attribute for the tag.
		if ((current_attr = calloc(1, sizeof(xml_attr_t))) == NULL)
		{
			// unable to allocate memory
			printf("Failed to allocate memory for the tag's first attribute in 'parse_attribute_value_pairs()'.\nThe error was: %s\n", strerror(errno));
			return NULL;
		}
		// Set the 'current_object' to the newly allocated memory.
		current_object->xmltag.attributes = current_attr;
	}
	else
	{
		// Not the first attribute. Allocate the memory on the last
		// attribute's 'next' pointer.
		current_attr = current_object->xmltag.attributes;
		while(current_attr->next != NULL)
		{
			current_attr =  current_attr->next;
		}
		
		if ((current_attr->next = calloc(1, sizeof(xml_attr_t))) == NULL)
		{
			// unable to allocate memory
			printf("Failed to allocate memory for the tag's next attribute in 'parse_attribute_value_pairs()'.\nThe error was: %s\n", strerror(errno));
			return NULL;
		}
		// and switch to it.
		current_attr = current_attr->next;	
	}
	
	// 'file_contents' points to the start of the attribute name. Copy it's
	// pointer to 'name',
	current_attr->name = file_contents;
	
	// Now loop while I see valid attribute name characters.
	while ((*file_contents != 0) && (*file_contents != '=') && 
		((*file_contents == '.') || (*file_contents == '_') || (*file_contents == '-') || (isalnum(*file_contents))))
	{
		file_contents++;
	}
	
	// Burn up spaces, if any.
	if (isspace(*file_contents))
	{
		// Looking at a space.
		*file_contents++ = 0;
		while ((*file_contents != 0 ) && (isspace(*file_contents)))
		{
			file_contents++;
		}
	}
	
	// Make sure I am looking at an equals sign now.
	if (*file_contents == '=')
	{
		// If this is an equal, set it as \0.
		*file_contents++ = 0;
	}
	else
	{
		// Bad formatting.
		printf("I didn't see an equal sign while parsing attribute/value pairs in 'parse_attribute_value_pairs()'.\n");
		return NULL;
	}
	
	// Burn up any more spaces, if any. I don't have an 'if' now as I don't
	// need to set \0 here.
	while ((*file_contents != 0 ) && (isspace(*file_contents)))
	{
		file_contents++;
	}
	
	// Make sure I am now looking at the opening double-quote.
	if (*file_contents != '"')
	{
		// Bad formatting.
		printf("I didn't see the leading double-quote while parsing attribute's value in 'parse_attribute_value_pairs()'.\n");
		return NULL;
	}
	
	// I know I am looking at the first double-quote now. Advance, then
	// mark that point as the value.
	file_contents++;
	current_attr->value = file_contents;
	
	// Loop now until I see the second double-quote.
	while ((*file_contents != 0 ) && (*file_contents != '"'))
	{
		file_contents++;
	}
	
	// I'm out of the look. Make sure it's because I saw a double-quote.
	if (*file_contents == '"')
	{
		// Awesome.
		*file_contents++ = 0;
	}
	else
	{
		// Bad formatting.
		printf("I didn't see closing double-quote in 'parse_attribute_value_pairs()'.\n");
		return NULL;
	}
	
	// All done!
	return file_contents;
}
