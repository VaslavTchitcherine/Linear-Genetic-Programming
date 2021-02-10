

/*
 * simple.c
 * A simple example of calling x87 machine code from C,
 * Computes 2.0+3.0 using FPU registers.
 */

#include <stdio.h> 

typedef long double (*function_ptr)();

unsigned char adder_program[] = {
	// header
    0xd9, 0xee,         // FLDZ	(f[0] = 0.0)
    0x55,               // push ebp
    0x89,0xE5,          // mov esp,ebp

	// the program
	0xdc,0x05,0x00,0x00,0x00,0x00,	// FADD m64fp (f[0] += m64fp)
	0xdc,0x05,0x00,0x00,0x00,0x00,	// FADD m64fp (f[0] += m64fp)

	// footer
    0x5D,                   // pop ST(0) to ebp for return
    0xC3                   // ret
};

double *two;
double *three;

int main(int argc, char *argv[])
{
	long double sum;
	long addr;

	// space for addends (malloced like constants and input in lgp)
	two = (double *)malloc(sizeof(double));
	three = (double *)malloc(sizeof(double));

	*two = 2.0;
	*three = 3.0;

	// copy the addresses into the machine code vector (little endian)
	addr = (long)&two[0];
	adder_program[7] = addr & 0xff;
	adder_program[8] = (addr>>8) & 0xff;
	adder_program[9] = (addr>>16) & 0xff;
	adder_program[10] = (addr>>24) & 0xff;

    addr = (long)&three[0];
    adder_program[13] = addr & 0xff;
    adder_program[14] = (addr>>8) & 0xff;
    adder_program[15] = (addr>>16) & 0xff;
    adder_program[16] = (addr>>24) & 0xff;


	// call the machine code
	sum = ((function_ptr)adder_program)();

	printf("Should be 5.0: %Lf\n",sum);

	return 0;
}
