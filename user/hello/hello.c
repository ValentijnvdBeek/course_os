#include <stdio.h>
#include <stdint.h>
#include <process.h>

uint32_t listener(uint32_t parent_tid) {

	return 0;
}

int main(int argc, char ** argv) {

	__asm ("mov r0, #0x15":::);
	__asm ("msr CPSR, r0":::);
	__asm ("msr SPSR, r0":::);

	printf(argv[0], 3);
	printf("\n");
	//printf(argv[0], 4);
	//printf(argv[1], 8);

////
////	for (int i = 0; i < argc; i++) {
////		printf(argv[i], &argv[i]);
////		//	printf("\n", 99);
////	}
//
//	printf("Process ready\n", 99);
//
//	process_emit(12, 1, "Hey, how are you?", 18);
//
//	printf("Process started\n", 99);
//
//	process_emit(12, 1, "I am doing great!", 18);
//
//	printf("Process continued\n", 99);
//
//	process_emit(12, 1, "Are you sure?", 18);
//
//	process_yield();
//
//	process_emit(12, 1, "Yeah!!", 18);

	while (1)
		;
}
