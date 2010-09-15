// This is the header file for the stack functions in diximal.

// Headers hold structures and prototypes made available, generally, by the
// NAME.c file (but not always).

// This prevents this header from being loaded twice. The first time it's
// called, MYSTACK_H is undefined so it enters the loop (up to the #endif). The
// first line in the loop defines it.
#ifndef MYSTACK_H
#define MYSTACK_H

/* prototypes */

// Create X number of items on top of the stack.
int create_stack(int size);

// Cleanup function, frees all memory used by the stack.
void destroy_stack(void);

// Push a pointer on to the stack. Return 0 on success, -1 on fail.
int push_stack(void *data);

// Return a pointer to the top element on the stack or NULL is the stack is
// empty.
void * pop_stack(void);

// Return a pointer to the offset-th element from the top of the stack without
// removing it.
void * peek_stack(int offset);

#endif
