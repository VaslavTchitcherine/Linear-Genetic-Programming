
/*
 * lgp.h
 */

#ifndef LGPH
#define LGPH

// default population size
#define POPSIZE 1000

// default # of constants
#define NCONSTANTS 100

// default range of constants
#define MINCONST -1.0
#define MAXCONST 1.0

// maximum # of generations (actually, breedings/popsize)
#define MAXGENERATIONS 1000

// how many times can we unsuccessfully select crossover points
// which yield children too long or too short, before reselecting parents
#define MAXCROSSOVERTRIALS 10

// restart trial if this many evaluations (not generations) without improvement 
// (if we used generations, this would have to scale inversely with popsize)
// N.B. if STAGNATION >= maxgenerations*popsize, then it has no effect
// as a trial always runs to maxgenerations before hitting stagnation,
// even if no improvement
#define STAGNATION 100000		// 200000

// maximum # of trials
#define MAXTRIALS 100

// number of demes
#define NDEMES 10

// inter-deme migration (fraction, at end of generational equivalent)
#define DEMETICMIGRATION 0.01

// program output threshold for classification
#define THRESHOLD 0.0L

// rel freq of input vars to constants in 6 byte instructions
// (in initial pop, and for mutations to constants vs. mutations to inputs)
#define FREQCONST 0.5

// maximum width of input pattern (just for static allocation)
#define MAXINPUTS 100

// minimum permitted program length, instructions
#define MININSTRS 10

// maximum permitted program length, instructions
// warning:  if this is too large, we will overfit
// PERHAPS THIS SHOULD BE A RANDOM PARAMETER, LIKE NREGISTERS???
#define MAXINSTRS 100		// 200 ?

// tolerance for fuzzy comparison with 0.0
#define FUZZ 1.0e-8

// What order of norm is used for error in regression problems.
// Only one of the following should be uncommented
#define NORM1
//#define NORM2

// probability of homogenous crossover (otherwise 2XO, standard 2 point)
// note that we bloat more if HXO_PROB is lower
//#define HXO_PROB 0.5
#define HXO_PROB 0.95

// Probability of mutation
// Banzhaf claims a very high rate helps generalization for hard problems,
// due to a maintenance of diversity for longer (cf. banzhaf96effect.pdf)
//#define MUTATION_PROB 0.95
#define MUTATION_PROB 0.5

// longest possible header and footer lengths, bytes (for static allocation)
#define MAXHEADERLEN 34
#define MAXFOOTERLEN 2

// Machine code programs are cast as this, to be callable from C.
// We use long double to have same precision as 80 bit FPU registers, so
// SIGFPE arises in the same instances in decompiled C code as in machine code.
typedef volatile long double (*function_ptr)();

// Length of machine code vector, in bytes, given a pointer to an individual.
// Could replace the codelen field in the Pop struct with use of this macro.
#define CODELEN(p) ABS((p)->indices[(p)->instr])+(p)->footerlen

// struct for a member of the population (2060 bytes)
typedef struct pop {
	long indices[MAXINSTRS+1];		// indices of instruction boundaries
	long ninstr;					// how many instructions in program
	long codelen;					// length of machine code vector, bytes
	long double error;				// error (negative fitness)
	// the machine code vector, with header and footer
	// we statically allocate enough space for all instrs to be 6 bytes
	volatile unsigned char code[6*MAXINSTRS+MAXHEADERLEN+MAXFOOTERLEN];
} Pop;

#endif // LGPH
