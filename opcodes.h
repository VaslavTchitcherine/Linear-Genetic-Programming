
/*
 * opcodes.h
 * Defines what opcodes we use.
 * http://www.intel.com/design/pentium4/manuals/index_new.htm
 * WARNING:  decompile() may break if you uncomment any of the commented ones!
 * Furthermore, the uncommented set seems adequate.
 */

#ifndef OPCODESH
#define OPCODESH

// opcodes for 2 byte instructions (no arguments)
unsigned char twobyte_noarg[] = {
	0xd8,0xe1,		// FABS					f[0]=fabs(f[0])
	0xd9,0xe0,		// FCHS					f[0]=-f[0]
	0xd9,0xfd,		// FSCALE				f[0]=f[0]*pow(2.0,(long)f[1])
	0xd9,0xfa,		// FSQRT				f[0] = sqrt(f[0])
	//0x72,0x06,	// JB EPI+6 			if (flag) goto 6 bytes ahead
	//0x72,0x06,	// JNB EPI+6 			if (!flag) goto 6 bytes ahead
	//0xd9,0xe8,		// FLD1					f[0] = 1.0; rot
	//0xd9,0xee,		// FLDZ					f[0] = 0.0; rot
// the following opcode works OK, but leads to many introns due
// to its input range restriction
	//0xd9,0xf0,		// F2XM1				f[0]=pow(2,f[0])-1
	//0xd9,0xf1,		// FYL2X				f[1]*=log(f[0])/log(2)); rot
	//0xd9,0xf9,	// FYL2XP1				f[1]*=log(f[0]+1.0)/log(2); rot
	//0xd9,0xf6,	// FDECSTP				decrements stack pointer by 1
	//0xd9,0xf7,	// FINCSTP				increments stack pointer by 1
	0xd9,0xff,		// FCOS					f[0]=cos(f[0])
	//0xd9,0xf2,		// FPTAN				f[0]=tan(f[0]); weird stuff
	0xd9,0xfe,		// FSIN					f[0]=sin(f[0])
	//0xd9,0xfb,	// FSINCOS				f[0]=cos(f[1]); f[1]=sin(f[1]);
	0
};

// 2 byte instructions (register argument)
unsigned char twobyte_regarg[] = {
	0xd8,0xc0,		// FADD ST(0),ST(n)			f[0] += f[n]
	0xdc,0xc0,		// FADD ST(n),ST(0)			f[n] += f[0]
	//0xde,0xc0,	// FADDP ST(n),ST(0)		f[n] += f[0]; pop
	0xdd,0xd0,		// FST ST(n)				f[n] = f[0]
	//0xdb,0xf0,	// FCOMI ST(0),ST(n)		cflag = f[0]<f[n]
	//0xda,0xc0,	// FCMOVB ST(0),ST(n)		if (cflag) f[0] = f[n]
	//0xdb,0xc0,	// FCMOVNB ST(0),ST(n)		if (!cflag) f[0] = f[n]
	//0xd9,0xc0,		// FLD ST(n)				v=f[n]; rot; f[0]=v
	0xd9,0xc8,		// FXCH ST(n)				tmp=f[0]; f[0]=f[n]; f[n]=tmp
	0xd8,0xf0,		// FDIV ST(0),ST(n)         f[0] /= f[n]
	//0xde,0xf8,	// FDIVP ST(n),ST(0)		f[n] /= f[0]; pop
	0xd8,0xc8,		// FMUL ST(0),ST(n)         f[0] *= f[n]
	0xdc,0xc8,		// FMUL ST(n),ST(0)         f[n] *= f[0]
	//0xde,0xc8,	// FMULP ST(n),ST(0)		f[n] *= f[0]; pop
	0xd8,0xe0,		// FSUB ST(0),ST(n)         f[0] -= f[n]
	0xdc,0xe8,		// FSUB ST(n),ST(0)         f[n] -= f[0]
	//0xde,0xe8,	// FSUBP ST(n),ST(0)		f[n] -= f[0]; pop
	0
};

// 6 byte instructions (last 4 bytes are memory location argument)
// Note that these all load double (m64fp) arguments, need different opcodes
// if our arrays of constants and input variables are not double.
unsigned char sixbyte[] = {
	//0xdd,0x05,		// FLD m64fp				f[0] = x; rot
	0xdc,0x05,		// FADD m64fp				f[0] += x
	0xdc,0x35,		// FDIV m64fp				f[0] /= x
	0xdc,0x0d,		// FMUL m64fp				f[0] *= x
	0xdc,0x25,		// FSUB m64fp				f[0] -= x
	0
};

#endif // OPCODESH
