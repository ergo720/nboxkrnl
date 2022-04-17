#define _HAS_EXCEPTIONS 0

#define KERNEL_STACK_SIZE 12288

unsigned char KiIdleThreadStack[KERNEL_STACK_SIZE];

void KernelEntry()
{
	__asm {
		xor ebp, ebp
		mov esp, offset KiIdleThreadStack + KERNEL_STACK_SIZE
		cli
		hlt
	}
}
