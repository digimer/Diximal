#include <stdio.h>	// Standard I/O
#include <stdlib.h>	// Library for things like 'sizeof()', 'itoa()', rand(), etc.

#include "stack.h"	// By convention, load your own header. 

/* globals */

// 'static' is like a 'local' in perl - it makes the variable available in this file only. Note that 'static' is different inside functions.
static int stack_size = 0;	// This gets set by the caller of 'create_stack' so that I know how much memory to allocate. I could use malloc/realloc to make this dynamic, but that's over kill at this stage..
static int stack_index = 0;	// This is incremented as pointers are added to the stack and decremented as they are popped off. At no time are they allowed to go past 'stack_size' to prevent a buffer overrun.
static void **stack = NULL;	// The stack itself! The '**' means pointer to an array of pointers.
// If I had wanted to make a variable available to other programs, use 'extern'. Note though that if something else loads with an 'extern'
// variable of the same name, they will be linked! So be careful/unique with your names.

/* functions */

// Create X number of items on top of the stack
int create_stack(int size)
{
	// Use calloc to allocate size * (size of a pointer on this hardware) and return a pointer to 'stack'.
	if ((stack = calloc(size, sizeof(void *))) == NULL)
	{
		// Failed to allocate the memory.
		return -1;
	}
	
	// Set the 'stack_size' to size so that we know what the limit is.
	stack_size = size;
	
	return 0;
}

// Cleanup function, frees all memory used by the stack
void destroy_stack(void)
{
	// Destroy the entire stack.
	free(stack);	// Frees the memory
	stack = NULL;	// Destroy the pointer
	stack_size = 0;	// Set the stack_size back to 0 as there is no stack any more.
}

// Push a pointer on to the stack. Return 0 on success, -1 on fail.
int push_stack(void *data)
{
	if (stack_index >= stack_size)
	{
		// No space left on the stack (only reason this would fail).
		return -1;
	}
	
	// Stuff the passed pointer onto the stack and then increment stack_index by one.
	stack[stack_index++]=data;
	
	// If I wanted to, I could return the number of elements left if the stack. By using negative integers for errors, this is possible.
	// For now, just return success.
	return 0;
}

// Return a pointer to the top element on the stack or NULL is the stack is empty.
void * pop_stack(void)
{
	// Do I even have anything on the stack?
	if (stack_index == 0)
	{
		// Nope.
		return NULL;
	}
	
	// Decrement the stack index by one (the top is empty always) and return the pointer.
	return stack[--stack_index];
}

// Return a pointer to the offset-th element from the top of the stack without removing it.
void * peek_stack(int offset)
{
	// Make sure I am not being asked to access an offset beyond stack_index or below 0.
	if ((offset >= stack_index) || (offset < 0 ))
	{
		// Out of range.
		return NULL;
	}
	
	// Return the stack index minus the offset, minus one because the stack_index points to the next free spot.
	return stack[(stack_index-1)-offset];
}
