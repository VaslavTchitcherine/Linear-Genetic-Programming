
/*
 * decompile.h
 */

#ifndef DECOMPILEH
#define DECOMPILEH

// decompile the machine code in the buf, writing to stdout
void decompile(char *filename,      // leave decompiled C code in this file
                unsigned char *buf, // machine code to decompile
                long len,           // length of machine code, bytes
                double *constant,    // array of constants used by code
                double *variable);    // array of input variables used by code

#endif // DECOMPILEH
