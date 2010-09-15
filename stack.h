// Headers hold structures and prototypes made available, generally, by the NAME.c file (but not always)

// This prevents this header from being loaded twice. The first time it's called, MYSTACK_H is undefined so it enters the loop (up to the #endif).
// The first line in the loop defines it.
#ifndef MYSTACK_H
#define MYSTACK_H

/* prototypes */

int create_stack(int size);	// Create X number of items on top of the stack
void destroy_stack(void);	// Cleanup function, frees all memory used by the stack
int push_stack(void *data);	// Push a pointer on to the stack. Return 0 on success, -1 on fail.
void * pop_stack(void);		// Return a pointer to the top element on the stack or NULL is the stack is empty.
void * peek_stack(int offset);	// Return a pointer to the offset-th element from the top of the stack without removing it.

#endif
