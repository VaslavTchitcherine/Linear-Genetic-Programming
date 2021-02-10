
/*
 * disasm.h
 */

#ifndef DISASMH
#define DISASMH

// buf is a pointer to the buffer of bytes being disassembled,
// len is how many bytes to disassemble.
// The disassembled code is printed to stdout.
void disasm(char *buf,long len);

#endif // DISASMH
