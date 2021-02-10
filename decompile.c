
/*
 * decompile.c
 * Crude decompiler for x87 FPU code, takes machine code and
 * emits the corresponding C code to stdout.
 */

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>

#include <limits.h>         // for LINE_MAX
#include <values.h>         // for LONG_MAX
#include <strings.h>

#include "libdis.h"         // from libdisasm_0.21-pre2

static FILE *fp;

extern long headerlen,footerlen;

void top_is_top_minus_1()
{
    fprintf(fp,"    f[7]=f[6];\n");
    fprintf(fp,"    f[6]=f[5];\n");
    fprintf(fp,"    f[5]=f[4];\n");
    fprintf(fp,"    f[4]=f[3];\n");
    fprintf(fp,"    f[3]=f[2];\n");
    fprintf(fp,"    f[2]=f[1];\n");
    fprintf(fp,"    f[1]=f[0];\n");
}

void rotstack(long dir)
{
	if ( dir == -1 ) { 
		fprintf(fp,"    tmp=f[7];\n");
		fprintf(fp,"    f[7]=f[6];\n");
		fprintf(fp,"    f[6]=f[5];\n");
		fprintf(fp,"    f[5]=f[4];\n");
		fprintf(fp,"    f[4]=f[3];\n");
		fprintf(fp,"    f[3]=f[2];\n");
		fprintf(fp,"    f[2]=f[1];\n");
		fprintf(fp,"    f[1]=f[0];\n");
		fprintf(fp,"    f[0]=tmp;\n");
	}
	else if ( dir == 1 ) {
		fprintf(fp,"    tmp=f[0];\n");
		fprintf(fp,"    f[0]=f[1];\n");
		fprintf(fp,"    f[1]=f[2];\n");
		fprintf(fp,"    f[2]=f[3];\n");
		fprintf(fp,"    f[3]=f[4];\n");
		fprintf(fp,"    f[4]=f[5];\n");
		fprintf(fp,"    f[5]=f[6];\n");
		fprintf(fp,"    f[6]=f[7];\n");
		fprintf(fp,"    f[7]=tmp;\n");
	}
	else {
		fprintf(stderr,"Error, can't rotate stack by %d\n",dir);
		exit(-1);
	}
}

// IS THIS RIGHT?
// INTEL DOCS SEEM TO SAY IT SHOUD BE rotstack(1), LEAVING f[0] EMPTY
void popstack()
{
	// according to discipulus
	fprintf(fp,"    tmp=f[0];\n");
	fprintf(fp,"    f[0]=f[1];\n");
    fprintf(fp,"    f[1]=f[2];\n");
    fprintf(fp,"    f[2]=f[3];\n");
    fprintf(fp,"    f[3]=f[4];\n");
    fprintf(fp,"    f[4]=f[5];\n");
    fprintf(fp,"    f[5]=f[6];\n");
    fprintf(fp,"    f[6]=f[7];\n");
    fprintf(fp,"    f[7]=tmp;\n");

/*
    fprintf(fp,"    f[0]=f[1];\n");
    fprintf(fp,"    f[1]=f[2];\n");
    fprintf(fp,"    f[2]=f[3];\n");
    fprintf(fp,"    f[3]=f[4];\n");
    fprintf(fp,"    f[4]=f[5];\n");
    fprintf(fp,"    f[5]=f[6];\n");
    fprintf(fp,"    f[6]=f[7];\n");
*/
/* to me, the following seems like a push... 
    fprintf(fp,"    f[7]=f[6];\n");
    fprintf(fp,"    f[6]=f[5];\n");
    fprintf(fp,"    f[5]=f[4];\n");
    fprintf(fp,"    f[4]=f[3];\n");
    fprintf(fp,"    f[3]=f[2];\n");
    fprintf(fp,"    f[2]=f[1];\n");
    fprintf(fp,"    f[1]=f[0];\n");
*/
}

// decompile a single opcode and operand
void decompile1(char *opcode,char *operand,double *constant,double *variable)
{
//fprintf(fp,"DEBUG opcode: %s (len %d)   operand: %s\n",opcode,strlen(opcode),operand);
	char *c;
	long reg1,reg2;		// indices of registers
	long addr;			// address (of constant or variable)
	long varindex;
	long constindex;
	long tmp;
	long varoffset,constoffset;

	// emit assembler as a comment
	fprintf(fp,"// %s %s\n",opcode,operand);

	addr = -1;			// no address argument decoded yet
    reg1 = reg2 = -1;	// no register arguments decoded yet
	varindex = -1;		// index of input variable not yet known
	constindex = -1;	// index of input constant not yet known

	// start with the simple stuff:
	// opcodes for 2 byte instructions with no operand
	if ( !strcasecmp(opcode,"FABS") ) {
		fprintf(fp,"    f[0] = fabsl(f[0]);\n");
		return;
	}
	if ( !strcasecmp(opcode,"FCHS") ) {
		fprintf(fp,"    f[0] = -f[0];\n");
		return;
	}
	if ( !strcasecmp(opcode,"FSCALE") ) {
		fprintf(fp,"    f[0] *= powl(2.0L,(f[1]>0.0L?1.0L:-1.0L)*floorl(fabsl(f[1])));\n");
		return;
	}
	if ( !strcasecmp(opcode,"FSQRT") ) {
		fprintf(fp,"    f[0] = sqrtl(f[0]);\n");
		return;
	}
	if ( !strcasecmp(opcode,"FLD1") ) {
		top_is_top_minus_1();
		fprintf(fp,"    f[0] = 1.0L;\n");
		return;
	}
	if ( !strcasecmp(opcode,"FLDZ") ) {
		top_is_top_minus_1();
		fprintf(fp,"    f[0] = 0.0L;\n");
		return;
	}
	if ( !strcasecmp(opcode,"F2XM1") ) {
		fprintf(fp,"    if ( f[0]>=-1.0L && f[0]<=1.0L )\n");
		fprintf(fp,"        f[0] = powl(2.0L,f[0])-1.0L;\n");
		return;
	}
	if ( !strcasecmp(opcode,"FYL2X") ) {
		fprintf(fp,"    f[1] *= logl(f[0])/logl(2.0L);\n");
		popstack();
		return;
	}
	if ( !strcasecmp(opcode,"FYL2XP1") ) {
		fprintf(fp,"    f[1] *= logl(f[0]+1.0L)/logl(2.0L);\n");
		popstack();
		return;
	}
	if ( !strcasecmp(opcode,"FDECSTP") ) {
		rotstack(-1);
		return;
	}
	if ( !strcasecmp(opcode,"FINCSTP") ) {
		rotstack(1);
		return;
	}
	if ( !strcasecmp(opcode,"FCOS") ) {
		fprintf(fp,"    if ( fabsl(f[0]) < powl(2.0L,63.0L))\n");
		fprintf(fp,"       f[0] = cosl(f[0]);\n");
		return;
	}
	if ( !strcasecmp(opcode,"FSIN") ) {
		fprintf(fp,"    if ( fabsl(f[0]) < powl(2.0L,63.0L))\n");
		fprintf(fp,    "    f[0] = sinl(f[0]);\n");
		return;
	}

	// decode the operand(s), which will either be:
	//	1) address argument, for a constant or variable (e.g. [0x804F04C])
	//	2) one register [e.g. st(1)]
	//	3) two registers [e.g. st(0), st(1)]

	c = index(operand,'[');
	// there will be an address argument, if '[' was found
	if ( c ) {
		// point to the hex number after the '['
		operand = c+1;
		c = index(operand,']');
		// overwrite the final ']' with string terminator
		if ( c ) {
			*c = '\0';
		}
		else {
			fprintf(stderr,"Error, final ] not found\n");
			exit(-1);
		}

		// convert hex operand into decimal address
		sscanf(operand,"%x",&addr);

		varoffset = addr-(long)variable;
		if ( varoffset<0 )
			varoffset = LONG_MAX;
		constoffset = addr-(long)constant;
		if ( constoffset<0 )
			constoffset = LONG_MAX;
		// this argument is an input variable 
		if ( varoffset < constoffset ) {
			varindex = (addr-(long)variable) / sizeof(double);
		}
		// otherwise it is a constant
		else {
			constindex = (addr-(long)constant) / sizeof(double);
		}
	}

	// no address argument, look for register arguments
	else {
		c = strstr(operand,"st(");
		if ( c ) {
			*(c+4) = '\0';
			reg1 = atoi(c+3);
			c += 5;
		}
		else {
			fprintf(stderr,"Error, neither address nor register operand??\n");
			exit(-1);
		}

		// perhaps there is a second register arg
		c = strstr(c,"st(");
        if ( c ) {
            *(c+4) = '\0';
            reg2 = atoi(c+3);
        }
	}

	// OK, we've parsed the operand, decode the opcode

    if ( !strcasecmp(opcode,"FST") ) {
		fprintf(fp,"    f[%d] = f[0];\n",reg1);
		return;
	}

	// this is broken, in that in machine code the CF bit of the EFLAGS register	// gets reset by any intervening arithmetic ops which happend
	// before the FCMOVB or FCMOVNB
	if ( !strcasecmp(opcode,"FCOMI") ) {
        fprintf(fp,"    cflag = (f[%d] < f[%d] || isnan(f[%d]) || isnan(f[%d]));\n",reg1,reg2,reg1,reg2);
        return;
    }

	if ( !strcasecmp(opcode,"FCMOVB") ) {
        fprintf(fp,"    if ( cflag ) f[%d] = f[%d];\n",reg1,reg2);
        return;
    }

	if ( !strcasecmp(opcode,"FCMOVNB") ) {
        fprintf(fp,"    if ( !cflag ) f[%d] = f[%d];\n",reg1,reg2);
        return;
    }

	if ( !strcasecmp(opcode,"FLD") ) {
		if ( reg1 >= 0 ) {
        	fprintf(fp,"    tmp = f[%d];\n",reg1);
		}
		top_is_top_minus_1();
		if ( varindex >= 0 || constindex >= 0 ) {
			if ( varindex >= 0 ) {
				fprintf(fp,"    f[0] = v[%d];\n",varindex);
			}
			else {
				// is 12 decimal digits enough precision for a double
				// (
                fprintf(fp,"    f[0] = %5.12lf;\n",constant[constindex]);
			}
		}
		else {
                fprintf(fp,"    f[0] = tmp;\n");
		}
        return;
	}

	if ( !strcasecmp(opcode,"FXCH") ) {
		fprintf(fp,"    tmp = f[0];\n");
		fprintf(fp,"    f[0] = f[%d];\n",reg2);
		fprintf(fp,"    f[%d] = tmp;\n",reg2);
        return;
	}

    if ( !strcasecmp(opcode,"FADD") ) {
		if ( reg1>=0 && reg2>=0 ) {
			fprintf(fp,"    f[%d] += f[%d];\n",reg1,reg2);
        	return;
		}
		else {
			if ( varindex >= 0 ) {
				fprintf(fp,"    f[0] += v[%d];\n",varindex);
            	return;
			}
			else if ( constindex >= 0 ) {
                fprintf(fp,"    f[0] += %5.12lf;\n",constant[constindex]);
                return;
            }
			else {
				fprintf(stderr,"Error in FADD no varindex or constindex\n");
				exit(-1);
			}
		}
	}

    if ( !strcasecmp(opcode,"FADDP") ) {
        if ( reg1<0 || reg2<0 ) {
			fprintf(stderr,"Error in FADDP operand is not 2 registers\n");
			exit(-1);
		}
		fprintf(fp,"    f[%d] += f[%d];\n",reg1,reg2);
		popstack();
		return;
	}

    if ( !strcasecmp(opcode,"FSUB") ) {
		if ( reg1>=0 && reg2>=0 ) {
			fprintf(fp,"    f[%d] -= f[%d];\n",reg1,reg2);
        	return;
		}
		else {
			if ( varindex >= 0 ) {
				fprintf(fp,"    f[0] -= v[%d];\n",varindex);
            	return;
			}
			else if ( constindex >= 0 ) {
                fprintf(fp,"    f[0] -= %5.12lf;\n",constant[constindex]);
                return;
            }
			else {
				fprintf(stderr,"Error in FSUB no varindex or constindex\n");
				exit(-1);
			}
		}
	}

    if ( !strcasecmp(opcode,"FSUBP") ) {
        if ( reg1<0 || reg2<0 ) {
			fprintf(stderr,"Error in FSUBP operand is not 2 registers\n");
			exit(-1);
		}
		fprintf(fp,"    f[%d] -= f[%d];\n",reg1,reg2);
		popstack();
		return;
	}

    if ( !strcasecmp(opcode,"FMUL") ) {
		if ( reg1>=0 && reg2>=0 ) {
			fprintf(fp,"    f[%d] *= f[%d];\n",reg1,reg2);
        	return;
		}
		else {
			if ( varindex >= 0 ) {
				fprintf(fp,"    f[0] *= v[%d];\n",varindex);
            	return;
			}
			else if ( constindex >= 0 ) {
                fprintf(fp,"    f[0] *= %5.12lf;\n",constant[constindex]);
                return;
            }
			else {
				fprintf(stderr,"Error in FMUL no varindex or constindex\n");
				exit(-1);
			}
		}
	}

    if ( !strcasecmp(opcode,"FMULP") ) {
        if ( reg1<0 || reg2<0 ) {
			fprintf(stderr,"Error in FMULP operand is not 2 registers\n");
			exit(-1);
		}
		fprintf(fp,"    f[%d] *= f[%d];\n",reg1,reg2);
		popstack();
		return;
	}

    if ( !strcasecmp(opcode,"FDIV") ) {
		if ( reg1>=0 && reg2>=0 ) {
			fprintf(fp,"    f[%d] /= f[%d];\n",reg1,reg2);
        	return;
		}
		else {
			if ( varindex >= 0 ) {
				fprintf(fp,"    f[0] /= v[%d];\n",varindex);
            	return;
			}
			else if ( constindex >= 0 ) {
                fprintf(fp,"    f[0] /= %5.12lf;\n",constant[constindex]);
                return;
            }
			else {
				fprintf(stderr,"Error in FDIV no varindex or constindex\n");
				exit(-1);
			}
		}
	}

    if ( !strcasecmp(opcode,"FDIVP") ) {
        if ( reg1<0 || reg2<0 ) {
			fprintf(stderr,"Error in FDIVP operand is not 2 registers\n");
			exit(-1);
		}
		fprintf(fp,"    f[%d] /= f[%d];\n",reg1,reg2);
		popstack();
		return;
	}
	// if we've fallen through to here, no known opcode was located
	fprintf(stderr,"Error, unknown opcode: %s\n",opcode);
	exit(-1);
}

// decompile the machine code in the buf, writing to stdout
void decompile(char *filename,		// leave decompiled C code in this file
				unsigned char *buf,	// machine code to decompile
				long len,			// length of machine code, bytes
				double *constant,	// array of constants used by code
				double *variable)	// array of input variables used by code
{
    char line[LINE_MAX];
    int pos;            // current position in buffer
    int size;           // size of instruction
    x86_insn_t insn;    // instruction
	char *opcode,*operand;
	char *c;

	fp = fopen(filename,"w");
	if ( !fp ) {
		fprintf(stderr,"Error, couldn't open decompilation file %s\n",filename);
		exit(-1);
	}

    //x86_init(opt_none, NULL, NULL);
    x86_init(opt_none, NULL);

	// emit the C form of the header
	fprintf(fp,"#include <math.h>\n\n");
	fprintf(fp,"long double lgp(double *v)\n");
    fprintf(fp,"{\n");
	fprintf(fp,"    long double f[8];     // 80 bit x87 FPU registers\n");
	fprintf(fp,"    long double tmp;\n");
	fprintf(fp,"    unsigned char cflag=0;\n");
	fprintf(fp,"    f[0] = f[1] = f[2] = f[3] = f[4] = f[5] = f[6] = f[7] = 0.0L;\n");

    // start at headerlen rather than 0, to skip over header
    pos = headerlen;
    while ( pos < len-footerlen ) {
        // disassemble
        size = x86_disasm(buf, len, 0, pos, &insn);

		if ( !size ) {
			fprintf(stderr,"Invalid instruction---------------------------\n");
            exit(-1);
		}

		pos += size;

		// convert disassembled instruction to ascii form in buffer, line
		x86_format_insn(&insn, line, LINE_MAX, intel_syntax);

		// scan over tab or whitespace (probably isn't any)
		c = line;
		while ( *c==' ' || *c=='\t' )	
			c++;
		opcode = c;
		
		// scan to tab or whitespace (i.e. to end of opcode)
		while ( *c!=' ' && *c!='\t' ) 
			c++;
		*c++ = '\0';
		
		// scan over tab or whitespace (to operand)
		while ( *c==' ' || *c=='\t' )
			c++;
		operand = c;

		// decompile this opcode and operand
		decompile1(opcode,operand,constant,variable);
	}

    x86_cleanup();

    fprintf(fp,"	return f[0];\n}\n");

	fclose(fp);
}
