#ifndef KOKORO_H
#define KOKORO_H

#include <setjmp.h>

typedef void(*kokoro_entry_t)(void* args);
typedef struct kokoro_stack_s kokoro_stack_t;

typedef enum kokoro_status_e
{
	KOKORO_RUNNING,
	KOKORO_SUSPENDED,
	KOKORO_STOPPED
} kokoro_status_t;

typedef struct kokoro_s
{
	jmp_buf yield_buf;
	jmp_buf resume_buf;
	void* val;

	void* stack_start;
	kokoro_stack_t* stack;
} kokoro_t;

void* kkr_spawn(kokoro_t* koro, kokoro_entry_t entry, void* args);
void kkr_cancel(kokoro_t* koro);
void* kkr_resume(kokoro_t* koro, void* val);
void* kkr_yield(void* val);
kokoro_status_t kkr_status(kokoro_t* koro);

#ifdef KOKORO_IMPLEMENTATION

#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <inttypes.h>

#define KKR_MIN(A, B) ((A) < (B) ? (A) : (B))
#define KKR_MAX(A, B) ((A) < (B) ? (B) : (A))

struct kokoro_stack_s
{
	char* min_addr;
	size_t size;
	char data[];
};

static kokoro_t* current_koro = NULL;
static kokoro_stack_t* current_stack = NULL;

static void* kkr_save_context(
	void* val, void* stack_start, void* stack_end, kokoro_stack_t** stackp
)
{
	char* stack_min = KKR_MIN(stack_start, stack_end);
	char* stack_max = KKR_MAX(stack_start, stack_end);
	size_t stack_size = stack_max - stack_min;

	if(stack_size)
	{
		*stackp = realloc(*stackp, sizeof(kokoro_stack_t) + stack_size);
		(*stackp)->size = stack_size;
		(*stackp)->min_addr = stack_min;
		memcpy((*stackp)->data, stack_min, stack_size);

		return stack_min <= (char*)val && (char*)val <= stack_max
			? (char*)((*stackp)->data) + ((char*)val - stack_min)
			: val;
	}
	else
	{
		return val;
	}
}

void kkr_restore_context(kokoro_stack_t* stack)
{
	memcpy(stack->min_addr, stack->data, stack->size);
}

void* kkr_spawn(kokoro_t* koro, kokoro_entry_t entry, void* args)
{
	assert(current_koro == NULL);

	void* stack_mark = NULL;
	koro->stack_start = (char*)&stack_mark;
	koro->stack = NULL;
	if(setjmp(koro->yield_buf) == 0)
	{
		current_koro = koro;
		entry(args);
		kkr_cancel(current_koro);
		current_koro = NULL;
		return NULL;
	}
	else
	{
		if(current_stack) { kkr_restore_context(current_stack); }

		void* ret_val = current_koro->val;
		current_koro = NULL;
		return ret_val;
	}
}

void kkr_cancel(kokoro_t* koro)
{
	free(koro->stack);
}

void* kkr_resume(kokoro_t* koro, void* val)
{
	assert(current_koro == NULL);

	void* stack_mark = 0;
	if(setjmp(koro->yield_buf) == 0)
	{
		current_koro = koro;
		koro->val = kkr_save_context(
			val, koro->stack_start, &stack_mark, &current_stack
		);
		// stupid trick to stop compiler from optimizing away the caller's
		// assignment to val
		longjmp(koro->resume_buf, (int)(uintptr_t)val);
	}
	else
	{
		if(current_stack) { kkr_restore_context(current_stack); }

		void* ret_val = current_koro->val;
		current_koro = NULL;
		return ret_val;
	}
}

void* kkr_yield(void* val)
{
	assert(current_koro != NULL);

	void* stack_mark = 0;
	if(setjmp(current_koro->resume_buf) == 0)
	{
		current_koro->val = kkr_save_context(
			val, current_koro->stack_start, &stack_mark, &current_koro->stack
		);
		// stupid trick to stop compiler from optimizing away the caller's
		// assignment to val
		longjmp(current_koro->yield_buf, (int)(uintptr_t)val);
	}
	else
	{
		if(current_koro->stack) { kkr_restore_context(current_koro->stack); }
		return current_koro->val;
	}
}

#endif

#endif
