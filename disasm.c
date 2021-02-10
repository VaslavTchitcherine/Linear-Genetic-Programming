
/*
 * disasm.c
 * Simple disassembler using libdisasm from the bastard project.
 * Requires libdisasm_0.21-pre2 from http://bastard.sf.net
 * This is superior to:  objdump -S a.out > /tmp/dis ; vi /tmp/dis
 * for a number of reasons, including that the bastard library
 * will disassemble fragments of machine code.
 */
   
#include <stdio.h>
#include <limits.h>         // for LINE_MAX

#include "libdis.h"			// from libdisasm_0.21-pre2

extern long headerlen;

// buf is a pointer to the buffer of bytes being disassembled,
// len is how many bytes to disassemble.
// The disassembled code is printed to stderr.
void disasm(unsigned char *buf,long len)
{
	char line[LINE_MAX];
	int pos;			// current position in buffer
	int size;			// size of instruction
	x86_insn_t insn;	// instruction

	//x86_init(opt_none, NULL, NULL);
	x86_init(opt_none, NULL);

	// start at headerlen rather than 0, to skip over header
	pos = headerlen;
	while ( pos < len ) {
		// disassemble
		size = x86_disasm(buf, len, 0, pos, &insn);
		if ( size ) {
			// print disassembled instruction
			x86_format_insn(&insn, line, LINE_MAX, intel_syntax);
			fprintf(stderr,"%s\n", line);
			pos += size;
		}
		else {
			fprintf(stderr,"Invalid instruction---------------------------\n");
			exit(-1);
			pos++;
		}
	}

	x86_cleanup();
}
