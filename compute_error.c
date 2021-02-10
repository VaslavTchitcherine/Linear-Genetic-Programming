
/* 
 * compute_error.c
 * Compute error for a member of the population in given interval.
 * Less error means the individual is more fit.
 * Located in its own module, as this MUST be compiled with no optimization, as 
 * dealing with i/o and invocation for the machine code genome.
 * requires various assembler level instructions.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <fcntl.h>
#include <math.h>
#include <ctype.h>
#include <limits.h>         // for LINE_MAX
#include <values.h>         // for LDBL_MAX
#include <signal.h>
#include <setjmp.h>

#include "lgp.h"
#include "disasm.h"
#include "util.h"

extern long ninputs;
extern double *inputsresampled[MAXINPUTS];
extern double *input,*outputs;
extern unsigned char classify;
extern unsigned char bagging;

sigjmp_buf env;

void sigfpe(int sig)
{
	//fprintf(stderr,"FPE interrupt occured\n");
    siglongjmp(env, 1);
}


// Compute error for a member of the population in the interval [start,stop)
// Less error means the individual is more fit.
// If out is not NULL, return outputs in this vector for each timestep
// between start and stop.
// These are binary for classification problems, error values for regression.
long double compute_error(Pop *pop,long start,long stop,long double *out)
{
	long t;
	long i,j;
	long double thisoutput;
	long double thiserror;
	long double error;

	// area to store extended FPU register state
	// (though storing more data, FXSAVE is indeed faster than FSAVE).
	unsigned char __attribute__ ((aligned (16))) fxsave[512];

	// the following FPU control word is the same as that set by:
	// feenableexcept(FE_DIVBYZERO|FE_OVERFLOW|FE_INVALID);
	volatile short fpu_controlword=0x0372;	// 0x362 if we also want underflow

	error = 0.0;

	// signal handler for FPU exceptions
    signal(SIGFPE,sigfpe);

	// We will return here by siglongjmp from the sigfpe signal handler.
	// On initialization, sigsetjmp returns 0 and we fall through.
	if ( sigsetjmp(env,1) ) {
        // restore floating point state
        asm volatile("FXRSTOR %0":"=m" (fxsave));
		// return with max possible error, since SIGFPE occured
		return LDBL_MAX;
	}
	
	for ( t=start ; t<stop ; t++ ) {

		// copy input pattern for this timestep t into input array, 
		// since base address for variables in machine code starts at input[0]
		// CAN WE AVOID THIS COPY BY CHANGING SEG REGISTER IN MACHINE CODE?
		for ( j=0 ; j<ninputs ; j++ ) {
			input[j] = inputsresampled[j][t];
		}

		// save floating point state
		asm volatile("FXSAVE %0":"=m" (fxsave));

		// NOTE it is reputedly faster to initialize the fpu context in memory
		// and load it with FLDENV, rather than calling FINIT, so perhaps
		// we should try this
		asm volatile("FINIT");

		// load FPU control word
		asm volatile ("FLDCW %0": :"m" (fpu_controlword));

		// execute the machine code for this input pattern at t
		thisoutput = ((function_ptr)pop->code)();

		// restore floating point state
		asm volatile("FXRSTOR %0":"=m" (fxsave));

		// compare this output with desired output
		if ( classify ) {
			// classification problem, just count the misclassifications
			// (cf. Kuncheva "counting estimator" p.8)
			if ( (thisoutput>0.0L) && outputs[t]==-1.0L ||
			     (thisoutput<=0.0L) && outputs[t]==1.0L ) {
				error += 1.0L;
			}
			// save thresholded output for later ensembling
			// (if vector in which to save has been supplied by caller)
			if ( out ) {
				out[t] = (thisoutput>THRESHOLD)?1.0L:-1.0L;
			}
		}
		// regression problem
		else {
			// WHAT ERROR NORM SHOULD WE USE?  PERHAPS 1?
			// NOTE: if you uncomment SQ(), then should do the same in
			// decompiled(), or #define PARANOID will break for regression
#ifdef NORM1
            thiserror = fabsl(((long double)outputs[t]) - thisoutput);
#endif // NORM1
#ifdef NORM2
            thiserror = SQ(((long double)outputs[t]) - thisoutput);
#endif // NORM2
            error += thiserror;
			// save output, if vector in which to save supplied by caller
			if ( out ) {
				out[t] = thiserror;
			}
		}
	}

	return error;
}
