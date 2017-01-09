#include <stdio.h>
#define KOKORO_IMPLEMENTATION
#include "kokoro.h"

void count(void* args)
{
	int to = *(int*)args;
	int thrice = to * 3;
	printf("Koro started with: %d\n", to);
	kkr_yield(&thrice);

	for(int i = 0; i < to; ++i)
	{
		int* num = kkr_yield(&i);
		printf("Koro received: %d\n", *num);
	}
}

int main()
{
	kokoro_t koro;
	int val = 10;
	void* ret = kkr_spawn(&koro, count, &val);

	printf("Koro sent back: %d\n", *(int*)ret);

	volatile char wat = 0;
	(void)wat;

	int send = 10;
	for(;;)
	{
		void* ret_val = kkr_resume(&koro, &send);
		if(!ret_val) { break; }
		int num = *(int*)ret_val;
		printf("Koro sent: %d\n", num);
		send = num * 2;
	}

	printf("Finished \n");

	return 0;
}
