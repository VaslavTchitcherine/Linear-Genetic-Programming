
// define this to verify decompiled C code agrees with the machine code
//#define PARANOID

/*
 * lgp.c
 * Linear Genetic Programming, evolution of x87 machine code
 *
 * Toy problems (for more challenging examples, see data/README)
 *
	# Even parity of 5 inputs, a classification problem:
	# (test range is null, since it will be perfect on training data)
 	lgp --inputfile=data/5parity --train=0/32 --test=32/32 --verbose
		(let the above run, it will find the perfect solution)
	# Koza's quartic regression (with only 1 lag), a regression problem:
	lgp --inputfile=data/kozaquartic --train=0/21 --test=21/21 --verbose --popsize=10000
	# Extrapolating a quartic (regression):
 	lgp --inputfile=data/quartic --train=0/800 --validate=800/990 --test=990/990 --popsize=1000 --plot=progsize --verbose
	# Sunspot regression (#define NORM2 if comparing with literature results):
 	lgp --inputfile=data/sunspots --train=0/220 --validate=220/257 --test=257/295 --verbose --popsize=10000
	# Random walk up/down classification, test hit rate had better not get good:
	lgp --inputfile=data/randwalk --train=0/1000 --validate=1000/1100 --test=1100/1200  --maxgenerations=10 --verbose

	# classifying gaussians with stdev 1.0 and 2.0 (see data/discipulus/README)
 	lgp --inputfile=data/discipulus/gaussian --train=0/500 --validate=500/999 --test=999/999 --maxgenerations=100 --popsize=100000 --verbose
 *
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
#include <time.h>
#include <assert.h>

#include <gsl/gsl_rng.h>
#include <gsl/gsl_randist.h>


#include "lgp.h"
#include "disasm.h"
#include "decompile.h"
#include "opcodes.h"
#include "load.h"
#include "util.h"

// All machine code programs start with this header, to set all
// floating point registers to 0.0
// (The necessary FINIT is handled from C, prior to calling machine code)
volatile unsigned char header[] = {
	0xd9, 0xee,         // FLDZ, push 0.0 
	0xdd, 0xd9,			// copy st(0) to st(1) and pop
	0xd9, 0xee,         // FLDZ, push 0.0 
	0xdd, 0xda,			// copy st(0) to st(2) and pop
	0xd9, 0xee,         // FLDZ, push 0.0 
	0xdd, 0xdb,			// copy st(0) to st(3) and pop
	0xd9, 0xee,         // FLDZ, push 0.0 
	0xdd, 0xdc,			// copy st(0) to st(4) and pop
	0xd9, 0xee,         // FLDZ, push 0.0 
	0xdd, 0xdd,			// copy st(0) to st(5) and pop
	0xd9, 0xee,         // FLDZ, push 0.0 
	0xdd, 0xde,			// copy st(0) to st(6) and pop
	0xd9, 0xee,         // FLDZ, push 0.0 
	0xdd, 0xdf,			// copy st(0) to st(7) and pop
	0x55,				// push ebp  
	0x89,0xE5,			// mov esp,ebp to save stack pointer
	0
};

// all machine code programs end with this footer, to return the
// result in ST(0) to the calling C program (as a long double)
volatile unsigned char footer[] = {
	0x5D,					// pop ST(0) to ebp for return
	0xC3,					// ret     
	0
};

// the population of machine code programs, array of pointers to Pop structs
Pop *pop;
long Popsize=POPSIZE;

// best individual so far
Pop best;

// randomly initialized pool of constants
double *constant;
long nconstants=NCONSTANTS;

// max # of registers to use
long nregisters=-1;
long nregisters1;		// # registers in use for current trial

// maximum # of trials
long Maxtrials=MAXTRIALS;

// maximum # of generations per trial
long Maxgenerations=MAXGENERATIONS;

// restart trial if this many evaluations (not generations) without improvement
long Stagnation=STAGNATION;

// # of demes
long Ndemes=NDEMES;
// interdeme migration (fraction, at end of generational equivalent)
double Migration=DEMETICMIGRATION;

// mutation probability
double Mutation_prob=MUTATION_PROB;

// lowest error so far in current trial
long double lowest_error;

// verbose output
unsigned char verbose=0;

// early stopping?
// If enabled, stop when validation set performance decreases, after
// having been greater than this value.
// We use the best individual before the fall in validation performance.
// Maybe it makes sense to set this to the minimum error rate we need to
// be sufficiently profitable to trade, perhaps --earlystopping=0.48  ???
double Earlystopping=0.0;

// training window
long trainstart = -1,trainstop,trainsize;
// optional validation window
long validatestart = -1,validatestop,validatesize;
// test window
long teststart = -1,teststop,testsize;

// array of input variables, cols are malloced in load()
double *inputs[MAXINPUTS];
long ninputs=0;		// width of input pattern (i.e. # lags) as read from file

// inputs bootstrap resampled in training area (for bagging)
double *inputsresampled[MAXINPUTS];

// Base address of input variable array, as appearing in machine code.
// Each input pattern from inputs[] in turn is copied here, prior to code 
// execution on the pattern.
double *input;

// desired outputs
double *outputs;

// a copy of the command by which we were called is retained in this string
char cmdline[LINE_MAX];

// program runs as classifier or regression, depending on whether the
// values of the last col of input patterns are all {-1,1}, or are float
unsigned char classify;

// is run deterministic (start from fixed random seed)
unsigned char deterministic=0;

// do we require initial pop members to have better than coin-flipping
// performance?
unsigned char initialfitness=0;

// what do we plot
unsigned char plotstdeverror=0;		// error stdev across the population
unsigned char plotprogsize=0;		// avg program size

// dump ensembled indicator in range [0,teststop] to stdout
// (Be sure to add leading zeros to compensate for head removal during
// lag construction in makeinputs.pl)
unsigned char dumpindicator=0;

// dump machine code for best individual to this file
char *dumpcode=NULL;

// are we bagging?
unsigned char bagging=0;

double *stdeverror;	// stdev of error in the population at each gen equiv
double *progsize;	// avg program size at each generational equivalent

// length of machine code header and footer, bytes
long headerlen,footerlen;

// how many opcodes of each type
long ntwobyte_noarg,ntwobyte_regarg,nsixbyte;

// so we only warn once about bloat
// (and keep track of how many times bloat impaired crossover, due to
// children being too large)
long bloatmoan=0;

// Thresholded outputs of best individual from each trial, retained
// for ensembling.
// Each element of bestoutputs[] is a vector covering timesteps [0,teststop]
long double *bestoutputs[MAXTRIALS];
long ensemblesize=0;        // how many bestoutputs in the ensemble

// Indicator resulting from ensembling the bestoutputs[] vectors
// If a classification problem indicator has values in the set {-1,1},
// for comparison to outputs[].
long double *indicator;

// Compute fitness (error) for an individual.
// In its own module, so the function can be compiled without optimization.
extern long double compute_error(Pop *pop,long from,long to,long double *out);

void scanargs(long argc, char *argv[])
{
	long i;
    int c;
	char *filename=NULL;
    char *s,*slash;

    // Create copy of cmdline, to be passed to plotting routines.
	// Also used to invoke decompiled
    cmdline[0] = '\0';
    for ( i=0 ; i<argc ; i++ ) {
        strcat(cmdline,argv[i]);
        strcat(cmdline," ");
    }
    //printf("%s\n",cmdline);

	while (1) {
      static struct option long_options[] =
        {
			{"inputfile",		required_argument,	0, 'i'},
			{"train",           required_argument,  0, 't'},
			{"validate",		required_argument,  0, 'V'},
            {"test",           	required_argument,  0, 'T'},
            {"deterministic",	no_argument,  0, 'D'},
            {"bagging",			no_argument,  0, 'b'},
            {"verbose",			no_argument,  0, 'v'},
      		{"popsize",			required_argument,  0, 'P'},
      		{"nregisters",		required_argument,  0, 'r'},
      		{"maxgenerations",	required_argument,  0, 'm'},
      		{"stagnation",		required_argument,  0, 's'},
      		{"ndemes",			required_argument,  0, 'd'},
      		{"migration",		required_argument,  0, 'g'},
            {"plot",            required_argument,  0, 'p'},
            {"dump",            required_argument,  0, 'U'},
            {"dumpcode",		required_argument,  0, 'c'},
			{"initialfitness",   no_argument,  0, 'I'},
			{"earlystopping",   required_argument,  0, 'e'},
			{"mutation_prob",   required_argument,  0, '0'},
			{0, 0, 0, 0}
        };

      // getopt_long stores the option index here
      int option_index = 0;
      c = getopt_long_only(argc, argv, "i:t:V:T:DvP:r:m:M:d:p:IU:eg:c:0:b",
                       long_options, &option_index);

      /* Detect the end of the options. */
      if (c == -1)
        break;

      switch (c) {

		case 'i':		// name of file containing input patterns to read
			filename = optarg;
            break;

		case 't':		// training window start/stop
            s = optarg;
            slash = index(s,'/');
            if ( !slash ) {
                fprintf(stderr,"Error, -t must specify start/stop, e.g. -t 0/1400\n");
                exit(-1);
            }
            *slash = '\0';
            if ( s[0] == '=' )
                s++;
            trainstart = atoi(s);
            trainstop = atoi(slash+1);
			trainsize = trainstop-trainstart;
            break;

        case 'V':       // validation window start/stop
            s = optarg;
            slash = index(s,'/');
            if ( !slash ) {
                fprintf(stderr,"Error, -V must specify start/stop, e.g. -t 500/600\n");
                exit(-1);
            }
            *slash = '\0';
            if ( s[0] == '=' )
                s++;
            validatestart = atoi(s);
            validatestop = atoi(slash+1);
            validatesize = validatestop-validatestart;
            break;


		case 'T':		// test window start/stop
            s = optarg;
            slash = index(s,'/');
            if ( !slash ) {
                fprintf(stderr,"Error, --test must specify start/stop, e.g. --test=0/1400\n");
                exit(-1);
            }
            *slash = '\0';
            if ( s[0] == '=' )
                s++;
            teststart = atoi(s);
            teststop = atoi(slash+1);
            testsize = teststop-teststart;
            break;

        case 'D':       // deterministic?
            deterministic = 1;
            break;

		case 'b':		// bagging
            bagging = 1;
            break;

		case 'v':		// verbose?
			verbose = 1;
            break;

		case 'e':		// early stopping when validating performance drops?
			Earlystopping = atof(optarg);
			break;

    	case 'P':       // population size
      		Popsize = atoi(optarg);
			if ( Popsize<10 ) {
				fprintf(stderr,"Error, unreasonably small --popsize\n");
				exit(-1);
			}
      		break;

    	case 'r':       // max # of FPU registers to use
      		nregisters = atoi(optarg);
			if ( nregisters<1 || nregisters>8 ) {
				fprintf(stderr,"Error, --nregisters cannot be %d\n",nregisters);
				exit(-1);
			}
      		break;

		case 'M':		// max # trials
			Maxtrials = atoi(optarg);
            break;

    	case 'm':       // max # generations to run per trial
      		Maxgenerations = atoi(optarg);
      		break;

		case 's':       // max evals with no improvement before trial restart
            Stagnation = atoi(optarg);
            break;

    	case 'd':       // # of demes
      		Ndemes = atoi(optarg);
      		break;

        case 'p':       // plot
            if ( !strncmp(optarg,"progsize",1) ) {
                // plot of error vs. generation
                plotprogsize = 1;
				break;
            }
            if ( !strncmp(optarg,"stdeverror",1) ) {
                // plot of error stdev across population
                plotstdeverror = 1;
                break;
            }
            else {
                fprintf(stderr,"Unknown argument to --plot: %s\n",optarg);
                exit(-1);
            }
            break;

        case 'U':       // dump
            if ( !strncmp(optarg,"indicator",1) ) {
                // dump final ensembled indicator to stdout
                dumpindicator = 1;
                break;
            }
            else {
                fprintf(stderr,"Unknown argument to --dump: %s\n",optarg);
                exit(-1);
            }
            break;

		case 'I':		// initial programs must be better than coin flip
			initialfitness = 1;
			break;

        case '?': 		// (getopt_long already printed an error message)
            break;

		case 'g':		// inter-deme Migration
			Migration = atof(optarg);
            break;

		case '0':		// probablility of mutation
			Mutation_prob = atof(optarg);
            break;

        case 'c':       // file to which we dump code for best individual
            dumpcode = optarg;
            break;

        default:
			abort();
        }
    }

	// Print any remaining command line arguments (not options)
	if ( optind < argc ) {
		printf ("Unknown cmdline argument: ");
		while (optind < argc) 
			printf("%s ", argv[optind++]); 
		putchar('\n');
		exit(-1);
	}

	// assorted sanity checking tests:
	// no inputseries -i specified
	if ( !filename ) {
		fprintf(stderr,"Must specify file containing input patterns with -i\n");
		exit(-1);
	}

	if ( Earlystopping!=0.0 && validatestart<0 ) {
		fprintf(stderr,"Must also specify --validate if you specify --earlystopping\n");
		exit(-1);
	}
    if ( Earlystopping!=0.0 && Earlystopping>0.5 ) {
        fprintf(stderr,"Error, --earlystopping should smaller than 0.5\n");
        exit(-1);
    }

    if ( trainstart<0 ) {
        fprintf(stderr,"Error, training range must be specified with --train\n");
        exit(-1);
    }
    if ( teststart<0 ) {
        fprintf(stderr,"Error, test range must be specified with --test\n");
        exit(-1);
    }

	if ( Popsize/Ndemes*Ndemes != Popsize ) {
        fprintf(stderr,"Warning, --ndemes does not evenly divide --popsize\n");
	}

	// make sure at least 1 individual will migrate inter-deme
	if ( Ndemes!=1 && Popsize*Migration/Ndemes < 0.5 ) {
		fprintf(stderr,"Warning, --migration too small for population size\n");
	}

	// load input patterns into inputs[] and outputs[] arrays, also sets ninputs
	outputs = load(filename,teststop,inputs,&ninputs);

	// Check if the problem is classification or regression.
	// It is classification if all outputs are all in the set {-1,1}
	classify = 1;
	for ( i=0 ; i<teststop ; i++ ) {
		// if any output not 1 or -1, then it's a regression problem
		if ( outputs[i]!=1.0 && outputs[i]!=-1.0 ) {
			classify = 0;
			break;
		}
	}

    if ( Earlystopping!=0.0 && !classify ) {
        fprintf(stderr,"Error, for now --earlystopping only works for classificiation programs\n");
        exit(-1);
    }
}

// Make a copy of an individual.
// This is used to save the best individual, and for interdeme Migration
void copy(Pop *from,Pop *to)
{
	long i;

	for ( i=0 ; i<=from->ninstr ; i++ ) {
		to->indices[i] = from->indices[i];
	}
	to->ninstr = from->ninstr;
	to->codelen = from->codelen;
	to->error = from->error;
	for ( i=0 ; i<from->codelen ; i++ ) {
        to->code[i] = from->code[i];
	}
}

void usage()
{
    fprintf(stderr,"Command line arguments:\n");
	fprintf(stderr,"--inputfile=<filename>\n");
    fprintf(stderr,"--train=<int>/<int>\n");
    fprintf(stderr,"[--validate=<int>/<int>]\n");
    fprintf(stderr,"--test=<int>/<int>\n");
	fprintf(stderr,"[--popsize=<int>]\n");
	fprintf(stderr,"[--nregisters=<int>]\n");
    fprintf(stderr,"[--mutation_prob=<float>]\n");
    fprintf(stderr,"[--maxgenerations=<int>]\n");
    fprintf(stderr,"[--stagnation=<int>]\n");
    fprintf(stderr,"[--ndemes=<int>]\n");
    fprintf(stderr,"[--migration=<float>]\n");
    fprintf(stderr,"[--plot=[progsize|stdeverror]\n");
    fprintf(stderr,"[--dump=[indicator]\n");
    fprintf(stderr,"[--dumpcode=<filename>]\n");
    fprintf(stderr,"[--deterministic]\n");
    fprintf(stderr,"[--earlystopping=<float>]\n");
    fprintf(stderr,"[--verbose]\n");
    fprintf(stderr,"[--initialfitness]\n");
    fprintf(stderr,"[--bagging]\n");
    exit(-1);
}

            
// Generate a random instruction, returning machine code and length.
// Instruction length can be up to 6 bytes.
// 2 byte instructions that take a register argument are signified by
// returning their length negated (i.e. -2)
void random_instruction(unsigned char code[6],long *len)
{
	float rand;
	long inst;
	long arg;

	rand = fastrand();

	// we select 2 byte, 2 byte register, or 6 byte inst with equal probability
	// 2 byte instruction, no argument
	if ( rand<0.333 ) {
		// select random index of this type of instruction
		inst = (long)(ntwobyte_noarg*fastrand());
		inst <<= 1;
		// and copy it into the return buffer
		code[0] = twobyte_noarg[inst];
		code[1] = twobyte_noarg[inst+1];
		*len = 2;
	}
	// 2 byte instruction, register argument
	else if ( rand<0.666 ) {
		inst = (long)(ntwobyte_regarg*fastrand());
		inst <<= 1;
		// and copy it into the return buffer
		code[0] = twobyte_regarg[inst];
		code[1] = twobyte_regarg[inst+1];
		// set register argument, this adds between 0 and nregisters1
		code[1] += (unsigned char)(nregisters1*fastrand());
		// 2 byte instructions that take register arg are signified with
		// a negative length
		*len = -2;
	}
	// 6 byte instruction, memory location argument
	else {
		inst = (long)(nsixbyte*fastrand());
		inst <<= 1;
		// and copy it into the return buffer
		code[0] = sixbyte[inst];
		code[1] = sixbyte[inst+1];
		// set address argument
		if ( fastrand()>FREQCONST ) {
			// address of the argument
			arg = (long)&constant[(long)(nconstants*fastrand())];
		}
		else {
			arg = (long)&input[(long)(ninputs*fastrand())];
		}
		// copy the argument into the machine code
		// "little endian", least significant byte first
		code[2] = arg & 0xff;
		code[3] = (arg>>8) & 0xff;
		code[4] = (arg>>16) & 0xff;
		code[5] = (arg>>24) & 0xff;
		*len = 6;
	}
}
        
// initialize random programs in population
void init_programs()
{
	long i,j,k,t;
	unsigned char code[6];
	long len;
	long double err;
	long ninstr;
	long nonesinoutput,nzerosinoutput;

	if ( verbose ) {
		fprintf(stderr,"Generating initial population (size %d)...",Popsize);
		fflush(stderr);
	}

	// if we require the random inital programs to meet some fitness threshold,
    // count number of occurences of 0.0 and 1.0 in output patterns.
    // We can use this to determine if program output is stuck,
	// when the problem is classification.
	if ( initialfitness ) {
    	nonesinoutput = nzerosinoutput = 0;
    	for ( t=trainstart ; t<trainstop ; t++ ) {
        	if ( outputs[t] == 0.0 ) 
            	nzerosinoutput++;
        	if ( outputs[t] == 1.0 )
            	nonesinoutput++;
    	}
	}

	i = 0;
	while ( i<Popsize ) {
		// all programs have the header
		for ( j=0 ; j<headerlen ; j++ ) {
			pop[i].code[j] = header[j];
		}
		// just the header in this program so far
		pop[i].codelen = headerlen;
		// and no instructions (other than header)
		pop[i].ninstr = 0;
		// first instruction starts at byte index headerlen
		pop[i].indices[0] = headerlen;
		// no fitness value yet
		pop[i].error = 0.0;

		// Choose a random # of instructions for this program in the
		// interval [MININSTRS,0.1*MAXINSTRS]
		// SHOULD USE 0.2 or 0.5 !!!
		ninstr = MININSTRS + (long)((0.1*MAXINSTRS-MININSTRS)*fastrand());

		for ( j=0 ; j<ninstr ; j++ ) {
			// generate a random instruction, returning machine code and len
			random_instruction(code,&len);
			// copy the instruction onto the end of the program
			for ( k=0 ; k<ABS(len) ; k++ ) {
				pop[i].code[pop[i].codelen++] = code[k];
			}
			// record index of end of this new jth instruction, computed
			// as the index of the start of the instruction, plus the
			// length of the instruction
			pop[i].indices[j+1] = ABS(len) + ABS(pop[i].indices[j]);
			// 2 byte instructions that take register arguments are
			// signified with a negative index
			if ( len<0 ) {
				pop[i].indices[j+1] = -pop[i].indices[j+1];
			}
			// count this instruction
			pop[i].ninstr++;
		}

		// append the machine code footer
		for ( j=0 ; j<footerlen ; j++ ) {
			pop[i].code[pop[i].codelen++] = footer[j];
		}

#ifdef PARANOID
assert( pop[i].codelen == ABS(pop[i].indices[pop[i].ninstr])+footerlen );
#endif // PARANOID

		// do we require initial pop members to have some competence?
		// DEPRECATED, NOT USEFUL
		if ( initialfitness ) {
			err = compute_error(&pop[i],trainstart,trainstop,NULL);
			// if classifying, error must be better than coin flipping
			//if ( classify && err>=(double)(trainsize>>1) ) {
			// if classifying, program output must not ever be NaN,
			// and must not be stuck
			if ( (classify && (err==LDBL_MAX || err==nonesinoutput || err==nzerosinoutput)) ) {
				// do not fall thru to increment i, regenerate pop[i]
				continue;
			}
		}

		// increment population index
		i++;
	}

	if ( verbose )
		fprintf(stderr,"done\n");
}


// Crossover 2 parents, producing the 2 children.
// Returns 1 if crossover is successful.
unsigned char crossover(Pop *parent1,Pop *parent2,Pop *child1,Pop *child2)
{
	long i;
	long point1,point2;
	long code1end,code2end;
	long dest;
	long ninstr1,ninstr2;
	long crossovertrials;

	if ( fastrand() > HXO_PROB ) {		
		// 2 point crossover
		crossovertrials = 0;
		do {
			point1 = 1+(long)((parent1->ninstr-1)*fastrand());
			point2 = 1+(long)((parent2->ninstr-1)*fastrand());
			// precompute # of instructions there will be in children, and
			// check that with these crossover points, the resulting children
			// will not have too many or too few instructions
			ninstr1 = point1 + parent2->ninstr - point2;
			ninstr2 = point2 + parent1->ninstr - point1;
		} while ( (ninstr1<MININSTRS || ninstr1>MAXINSTRS ||
				  ninstr2<MININSTRS || ninstr2>MAXINSTRS) &&
					crossovertrials++<MAXCROSSOVERTRIALS);
		if ( crossovertrials>=MAXCROSSOVERTRIALS ) {
			if ( !bloatmoan && verbose ) {
				fprintf(stderr,"Warning, MAXCROSSOVERTRIALS exceeded! (bloaty)\n");
			}
			bloatmoan++;
			// INTRON REMOVAL FROM PARENT1 AND PARENT2, AND RETRY?
			return 0;
		}

/*
if ( crossovertrials>0.5*MAXCROSSOVERTRIALS ) {
printf("Warning, crossovertrials %d\n",crossovertrials);
}
*/

	}
	else {
		// homogeneous crossover, no need to check child sizes
		point1 = point2 = 1+(long)(MIN2(parent1->ninstr-1,parent2->ninstr-1)*fastrand());
	}

//printf("DEBUG crossover points:  %d %d\n",point1,point2);
//printf("DEBUG parents ninstr %d %d\n",parent1->ninstr,parent2->ninstr);

	// copy head of parent1 indices into child 1 unchanged
	for ( i=0 ; i<point1 ; i++ ) {
		child1->indices[i] = parent1->indices[i];
	}
	// and copy head of parent1 code into child 1 unchanged
	code1end = ABS(parent1->indices[point1-1]);
	for ( i=headerlen ; i<code1end ; i++ ) {
		child1->code[i] = parent1->code[i];
	}
	child1->ninstr = point1-1;
	
	// copy head of parent2 indices into child 2 unchanged
    for ( i=0 ; i<point2 ; i++ ) {
        child2->indices[i] = parent2->indices[i];
    }
    // and copy head of parent2 code into child 2 unchanged
	code2end = ABS(parent2->indices[point2-1]);
    for ( i=headerlen ; i<code2end ; i++ ) {
        child2->code[i] = parent2->code[i];
    }
    child2->ninstr = point2-1;

	// copy tail of parent2 indices into child 1
	dest = point1;
	for ( i=point2 ; i<=parent2->ninstr ; i++ ) {
		child1->indices[dest] = ABS(child1->indices[dest-1])+
			(ABS(parent2->indices[i])-ABS(parent2->indices[i-1]));
		if ( parent2->indices[i]<0 )
			child1->indices[dest] = -child1->indices[dest];
		dest++;
		child1->ninstr++;
	}
	// copy tail of parent2 code into child 1
	for ( i=ABS(parent2->indices[point2-1]) ; i<parent2->codelen ; i++ ) {
		child1->code[code1end++] = parent2->code[i];
	}

    // copy tail of parent1 indices into child 2
	dest = point2;
    for ( i=point1 ; i<=parent1->ninstr ; i++ ) {
        child2->indices[dest] = ABS(child2->indices[dest-1])+
            (ABS(parent1->indices[i])-ABS(parent1->indices[i-1]));
        if ( parent1->indices[i]<0 )
            child2->indices[dest] = -child2->indices[dest];
		dest++;
		child2->ninstr++;
    }
    // copy tail of parent1 code into child2 
    for ( i=ABS(parent1->indices[point1-1]) ; i<parent1->codelen ; i++ ) {
        child2->code[code2end++] = parent1->code[i];
    }

	// WE DON'T REALLY NEED codelen AS PART OF THE STRUCT
	child1->codelen = ABS(child1->indices[child1->ninstr])+footerlen;
	child2->codelen = ABS(child2->indices[child2->ninstr])+footerlen;

/*
// DEBUG
printf("DEBUG parent1:\n");
disasm((unsigned char*)parent1->code,parent1->codelen);
printf("DEBUG parent2:\n");
disasm((unsigned char*)parent2->code,parent2->codelen);
printf("DEBUG child1:\n");
disasm((unsigned char*)child1->code,child1->codelen);
printf("DEBUG child2:\n");
disasm((unsigned char*)child2->code,child2->codelen);
exit(0);
// END DEBUG
*/

#ifdef PARANOID
	// conservation of number of instructions
	assert( parent1->ninstr+parent2->ninstr==child1->ninstr+child2->ninstr);
	// conservation of code length
	assert( parent1->codelen+parent2->codelen==child1->codelen+child2->codelen);
#endif // PARANOID

	return 1;
}
	

// mutate an individual
void mutate(Pop *p)
{
	long i;
	long diff;
    long inst;
    long arg;
	unsigned char hinib,lonib;

	// perhaps no mutation
	if ( fastrand()>Mutation_prob )
		return;

	// select index of instruction to mutate
	i = (long)(p->ninstr*fastrand());

	// determine what type of instruction this is
	diff = ABS(p->indices[i+1]) - ABS(p->indices[i]);

	// six byte instruction
	if ( diff==6 ) {
		// change to a different 6 byte opcode
		if ( fastrand()>0.5 ) {
        	inst = (long)(nsixbyte*fastrand());
        	inst <<= 1;
        	// and copy it into the code
/*
printf("inst %d\n",inst);
printf("indices %d %d\n",ABS(p->indices[i]),ABS(p->indices[i])+1);
printf("opcodes %x %x\n",p->code[ABS(p->indices[i])],p->code[ABS(p->indices[i])+1]);
*/

        	p->code[ABS(p->indices[i])] = sixbyte[inst];
        	p->code[ABS(p->indices[i])+1] = sixbyte[inst+1];
		}
		// change to a different argument
		else {
        	// set address argument
        	if ( fastrand()>FREQCONST ) {
            	// address of the argument
            	arg = (long)&constant[(long)(nconstants*fastrand())];
        	}
        	else {
            	arg = (long)&input[(long)(ninputs*fastrand())];
        	}
        	// copy the argument into the machine code
        	// "little endian", least significant byte first
        	p->code[ABS(p->indices[i])+2] = arg & 0xff;
        	p->code[ABS(p->indices[i])+3] = (arg>>8) & 0xff;
        	p->code[ABS(p->indices[i])+4] = (arg>>16) & 0xff;
        	p->code[ABS(p->indices[i])+5] = (arg>>24) & 0xff;
		}
	}
	// two byte no argument
	else if ( diff==2 && p->indices[i+1]>0 ) {
        // select random index of this type of instruction
        inst = (long)(ntwobyte_noarg*fastrand());
        inst <<= 1;
        // and copy it into the return buffer
        p->code[ABS(p->indices[i])] = twobyte_noarg[inst];
        p->code[ABS(p->indices[i])+1] = twobyte_noarg[inst+1];
	}
	// 2 byte register argument
	else if ( diff==2 ) {
		if ( fastrand()>0.5 ) {
			// mutate opcode
        	inst = (long)(ntwobyte_regarg*fastrand());
        	inst <<= 1;
        	// and copy it into the return buffer
        	p->code[ABS(p->indices[i])] = twobyte_regarg[inst];
        	p->code[ABS(p->indices[i])+1] = twobyte_regarg[inst+1];
		}
		else {
			hinib = p->code[ABS(p->indices[i])+1] & 0xf0;
			lonib = p->code[ABS(p->indices[i])+1] & 0x0f;
			// mutate register argument
			if ( lonib >= 8 ) {
				lonib = 8+(unsigned char)(nregisters1*fastrand());
			}
			else {
				lonib = (unsigned char)(nregisters1*fastrand());
			}
			p->code[ABS(p->indices[i])+1] = hinib | lonib;
		}
	}
	else {
		fprintf(stderr,"BOOM, weird diff %d\n",diff);
		exit(-1);
	}
}

// callback for quicksort
int compareerror(const void *a,const void *b)
{
    if (((Pop*)a)->error < ((Pop*)b)->error)
        return -1;
    else if (((Pop*)a)->error > ((Pop*)b)->error)
        return 1;
    else
        return 0;
}
			
// Interdeme Migration.
// Take best individuals from each deme, copy them to next deme,
// overwriting the worst individuals in the destination deme.
// Migration pattern is a simple ring.
void migrate()
{
	long i;
	long fromdeme,todeme;
	long from,to;
	long nmigrate;

	// how many migrate from each deme
	nmigrate = (long)(0.5+Popsize*Migration/Ndemes);

	// within each deme, sort by increasing error
	for ( i=0 ; i<Ndemes ; i++ ) {
		qsort(&pop[(long)(Popsize*i/Ndemes)],	// base addr of this deme
			(long)(Popsize/Ndemes),				// how many in a deme
			sizeof(Pop),
			&compareerror);
	}

	// for each deme in turn
	for ( fromdeme=0 ; fromdeme<Ndemes ; fromdeme++ ) {
		// the next deme in the ring, to which we are migrating
		todeme = (fromdeme+1)%Ndemes;

		// first element (most fit) in deme from which we are copying
		from = fromdeme*Popsize/Ndemes;
		// last element (least fit) in deme to which we are copying
		to = (todeme+1)*Popsize/Ndemes - 1;

		// migrate
		for ( i=0 ; i<nmigrate ; i++ ) {
			copy(&pop[from++],&pop[to--]);
		}
	}
}

// Record output of individual across all sets (i.e. [0,teststop])
// for later use as part of an ensemble.
// Note that it can be removed later, if it turns out that its addition
// failed to increase the performance of the ensemble.
void addto_ensemble(Pop *p)
{
	long i,j;
	long double *out;

	out = (long double*)Malloc(teststop*sizeof(long double));

	// Obtain vector out containing program classification output {-1,1} for
	// each timestep between 0 and teststop.
	// Each value in out is in the set {-1,1}, so can be compared pointwise
	// with outputs[]
	(void)compute_error(p,0,teststop,out);
	
	// very simple diversity enforced, we
	// check that an identical output vector is not already recorded
	for ( i=0 ; i<ensemblesize ; i++ ) {
		for ( j=0 ; j<teststop ; j++ ) {
			if ( bestoutputs[i][j] != out[j] ) {
				break;
			}
		}
		// return without recording if the ith recorded output identical to out
		// at all timesteps
		if ( j==teststop ) {
			free(out);
			return;
		}
	}

	// save pointer to the vector of outputs, for use in ensemble
	bestoutputs[ensemblesize++] = out;
}

// Average pointwise the bestoutputs[] output vectors, storing the result
// in the global indicator.
// Whenever the ensemble changes, this must be called prior to calling
// ensemble_error().
void update_ensembled_indicator()
{
    long double sum,out;
    long i,t;

    for ( t=0 ; t<teststop ; t++ ) {
        // compute the average for this timestep t across ensemble members
        sum = 0.0;
        for ( i=0 ; i<ensemblesize ; i++ ) {
            sum += bestoutputs[i][t];
        }
        out = sum/ensemblesize;

		if ( classify ) {
			// threshold (i.e. vote) to get {-1,1} indicator value that
			// can be compared to outputs[t] for classification problem
			if ( out>0.0L )
				indicator[t] = 1.0L;
			else
				indicator[t] = -1.0L;
		}
		// for regression problems, indicator is the average
		else {
			indicator[t] = out;
		}
	}
}

// Count misclassification errors of an ensemble between given start
// and stop points.
// This assumes update_ensembled_indicator() has been called to update the
// global indicator vector after any changes to the ensemble.
long double ensemble_error(long start,long stop)
{
	long double error;
	long t;

	error = 0.0L;
	for ( t=start ; t<stop ; t++ ) {
		if ( classify ) {
			if ( indicator[t] != outputs[t] ) 
				error += 1.0;
		}
		// regression problem
		else {
            error += indicator[t];
		}
	}

	return error;
}

int main(int argc, char *argv[])
{
	long i,t;
	double res;
	long s1,s2,s3,s4;			// 4 distinct random indices into the pop
	long parent1i,parent2i;		// indices of parents who breed
	long loser1i,loser2i;		// indices of losers who get replaced
	Pop *child1,*child2;
	Pop *betterchild;			// the child with the lower error
	long gen;					// counts # breedings
	long trial;					// number of runs
	clock_t starttime;
	float cputime;
	long genimproved;			// gen at which we last improved
	long double validateerror,lastvalidateerror;
	long deme;					// current deme # 
	char buf[LINE_MAX];

	if ( argc<2 ) {
        usage();
    }

    scanargs(argc,argv);

    // initialize the fast random number generator
    fastrandseed(deterministic);
	
	// print the seeds used
	if ( verbose )
		fastrand_dumpseeds();  

	// to keep track of error stdev across pop at each generational equivalent
	stdeverror = (double*)Malloc(Maxgenerations*sizeof(double));
	// to keep track of avg program size at each generational equivalent
	progsize = (double*)Malloc(Maxgenerations*sizeof(double));

	// global ensembled indicator vector (classification only)
	indicator = (long double *)Malloc(teststop*sizeof(long double));

    // compute length of machine code header and footer
	headerlen = 0;
	while ( header[headerlen] )
		headerlen++;
	footerlen = 0;
	while ( footer[footerlen] ) 
		footerlen++;

	// check that lengths don't exceed static array allocation
	if ( headerlen > MAXHEADERLEN ) {
		fprintf(stderr,"Error, increase MAXHEADERLEN\n");
		exit(-1);
	}
	if ( footerlen > MAXFOOTERLEN ) {
		fprintf(stderr,"Error, increase MAXFOOTERLEN\n");
		exit(-1);
	}

	// count opcodes of each type (count bytes and divide by 2)
	ntwobyte_noarg = 0;
	while ( twobyte_noarg[ntwobyte_noarg++] ) ;
	ntwobyte_noarg >>= 1;	// divide by 2, as 2 bytes per instruction
	ntwobyte_regarg = 0;
	while ( twobyte_regarg[ntwobyte_regarg++] ) ;
	ntwobyte_regarg >>= 1;	// divide by 2, as 2 bytes per instruction
	nsixbyte = 0;
	while ( sixbyte[nsixbyte++] ) ;
	nsixbyte >>= 1;	// divide by 2, as 2 bytes per instruction (no operand yet)
	
	// array of constants (reinitialized each trial)
	constant = (double*)Malloc(nconstants*sizeof(double));

	// Pool of input variables.
	// Addresses of elements of this array appear in the machine code.
	// To eval the machine code on a given specific input pattern from inputs[]
	// that pattern is copied into this input array.
	input = (double*)Malloc(ninputs*sizeof(double));

	// allocate space for resampled inputs
	for ( i=0 ;  i<ninputs ; i++ ) {
		inputsresampled[i] = (double*)Malloc(teststop*sizeof(double));
	}

	// population of programs
    pop = (Pop*)Malloc(Popsize*sizeof(Pop));

    // start timer
    starttime = clock();

	// Make a copy of the input array, as training region gets
	// overwritten if bootstrapping.
	// The validation and test regions never bootstrap resampled.
	for ( t=0 ; t<teststop ; t++ ) {
		for ( i=0 ; i<ninputs ; i++ ) {
			inputsresampled[i][t] = inputs[i][t];
		}
	}

    for ( trial=0 ; trial<Maxtrials ; trial++ ) {

	if ( bagging ) {
		// each trial we use a different bootstrap of the training input
		for ( t=trainstart ; t<trainstop ; t++ ) {
			long resample = (long)(trainstart+trainsize*fastrand());
			for ( i=0 ; i<ninputs ; i++ ) {
    			inputsresampled[i][t] = inputs[i][resample];
			}
		}
	}

	// unless --nregisters was specified on cmdline
	// use a random # of FPU registers for each trial
	if ( nregisters>0 ) 
		nregisters1 = nregisters;
	else
		nregisters1 = (long)(1.0+8.0*fastrand());
	// SHOULD WE ALSO RANDOMIZE MAXINSTRS

    // reinitialize pool of constants (uniformly randomly generated)
    for ( i=0 ; i<nconstants ; i++ ) {
        constant[i] = MINCONST + (MAXCONST-MINCONST)*fastrand();
    }

	if ( verbose )
		fprintf(stderr,"Trial %d, using nregisters %d\n",trial,nregisters1);

    // generation at which we last found an improvement
    genimproved = 0;

	// haven't yet warned about bloat
	bloatmoan = 0;

	// lowest error so far in current trial
	lastvalidateerror = best.error = lowest_error = LDBL_MAX;

	// (re)initialize random programs in population
	init_programs();

	// compute fitness for each initial random program in population
	for ( i=0 ; i<Popsize ; i++ ) {
		pop[i].error = compute_error(&pop[i],trainstart,trainstop,NULL);
	}

	deme = 0;

	// since we use steady state, gen counts breedings, not generations
	for ( gen=0 ; gen<Maxgenerations*Popsize ; gen++ ) {

		// increment deme index, we work within each deme in turn
		deme = (deme+1)%Ndemes;

		do {

		// Selection for crossover, steady state tournament of size 4:
		// Four distinct individuals are chosen at random from within the
		// current deme.
		// Fitness of the first two are compared, giving a winner and a loser.
		s1 = (long)(Popsize*deme/Ndemes + Popsize/Ndemes*fastrand());
		s2 = s1;
		while ( s2==s1 )
			s2 = (long)(Popsize*deme/Ndemes + Popsize/Ndemes*fastrand());
		// parsimony pressure: if equally fit, choose the shorter
		if ( pop[s1].error == pop[s2].error ) {
			if ( pop[s1].codelen < pop[s2].error ) {
				parent1i = s1;
				loser1i = s2;
			}
			else {
				parent1i = s2;
				loser1i = s1;
			}
		}
		else if ( pop[s1].error < pop[s2].error ) {
			parent1i = s1;
			loser1i = s2;
		}
		else {
			parent1i = s2;
			loser1i = s1;
		}
		// The second pair are compared in the same way to give a second winner
		// and loser.
		s3 = s1;
    	while ( s3==s1 || s3==s2 )
			s3 = (long)(Popsize*deme/Ndemes + Popsize/Ndemes*fastrand());
    	s4 = s3;
    	while ( s4==s1 || s4==s2 || s4==s3 )
			s4 = (long)(Popsize*deme/Ndemes + Popsize/Ndemes*fastrand());
        // parsimony pressure: if equally fit, choose the shorter
        if ( pop[s3].error == pop[s4].error ) {
            if ( pop[s3].codelen < pop[s4].error ) {
                parent2i = s3;
                loser2i = s4;
            }   
            else {
                parent2i = s4;
                loser2i = s3;
            }
        }
    	else if ( pop[s3].error < pop[s4].error ) {
        	parent2i = s3;
        	loser2i = s4;
    	}
    	else {
        	parent2i = s4;
        	loser2i = s3;
    	}

#ifdef PARANOID
assert( pop[parent1i].ninstr >= MININSTRS );
assert( pop[parent2i].ninstr >= MININSTRS );
assert( pop[parent1i].ninstr <= MAXINSTRS );
assert( pop[parent2i].ninstr <= MAXINSTRS );
#endif // PARANOID

		// We will crossover the 2 parents, producing 2 children
		// (NOTE: the losers are always overwritten by the children)
		child1 = &pop[loser1i];
		child2 = &pop[loser2i];

		// Call crossover() to perform the breeding.
		// This might fail if after MAXCROSSOVERTRIALS no crossover
		// points have been found which result in children of admissible size
		// (between MININSTRS and MAXINSTRS instructions).
		// As the run continues, this will happen more, due to bloat.
		} while ( !crossover(&pop[parent1i],&pop[parent2i],child1,child2) ) ;

#ifdef PARANOID
assert( child1->ninstr>=MININSTRS );
assert( child1->ninstr<=MAXINSTRS );
assert( child2->ninstr>=MININSTRS );
assert( child2->ninstr<=MAXINSTRS );
assert( child1->codelen == ABS(child1->indices[child1->ninstr])+footerlen );
assert( child2->codelen == ABS(child2->indices[child2->ninstr])+footerlen );
#endif // PARANOID


//fprintf(stderr,"gen %d\n",gen);
		// mutate the childen
//fprintf(stderr,"DEBUG child 1 before mutate:\n");
//disasm((unsigned char*)child1->code,child1->codelen);
		mutate(child1);
//fprintf(stderr,"DEBUG child 1 after mutate:\n");
//disasm((unsigned char*)child1->code,child1->codelen);
//fprintf(stderr,"DEBUG child 2 before mutate:\n");
//disasm((unsigned char*)child2->code,child2->codelen);
		mutate(child2);
//fprintf(stderr,"DEBUG child 2 after mutate:\n");
//disasm((unsigned char*)child2->code,child2->codelen);

		// compute fitness of the 2 children
		child1->error = compute_error(child1,trainstart,trainstop,NULL);
		child2->error = compute_error(child2,trainstart,trainstop,NULL);

		// If error is zero we found a program that is perfect on training set.
		// (This will happen only for very simple problems, like parity.)
		if ( fabsl(child1->error)<FUZZ || fabsl(child1->error)<FUZZ ) {
			fprintf(stderr,"Perfect solution found at gen %d\n",gen);
			if ( fabsl(child1->error)<FUZZ ) {
				//disasm((unsigned char*)child1->code,child1->codelen);
				decompile("f.c",(unsigned char*)child1->code,child1->codelen,constant,input);
				system("cat f.c");
			}
			else {
				//disasm((unsigned char*)child2->code,child2->codelen);
				decompile("f.c",(unsigned char*)child2->code,child2->codelen,constant,input);
				system("cat f.c");
			}
			//break;
    		cputime = ((float)clock()-(float)starttime)/((float)CLOCKS_PER_SEC);
    		fprintf(stderr,"Cpu time: %f\n",cputime);
			exit(0);
		}

		// which of the children is better
		if ( child1->error < child2->error ) {
			betterchild = child1;
		}
		else {
			betterchild = child2;
		}

		// is the better child the best individual so far in this trial?
		if ( betterchild->error < lowest_error ) {

			// compute validation error for this new best individual,
			// if a validation set was specified
			if ( validatestart>=0 ) {
				validateerror = compute_error(betterchild,validatestart,validatestop,NULL);
			}

            // early termination:
            // Break out of generation loop to start a new trial
			// if --earlystopping was specified, and
            // if validation error was < --earlystopping, but has now
			// increased, implying overfitting has occured.
			if ( Earlystopping!=0.0 &&
					lastvalidateerror <= Earlystopping*validatesize &&
					validateerror > lastvalidateerror ) {
				if ( verbose ) {
					fprintf(stderr,"Early stopping at %d, validation error has increased to %Lf\n",gen,validateerror/validatesize);
				}
				break;			// out of generation loop
			}

			lastvalidateerror = validateerror;
			
			// retain a copy of this new best individual
			copy(betterchild,&best);

			// record this new lowest error
			lowest_error = best.error;

			// record current generation number, at which we just improved
			// (for termination due to Stagnation if no subsequent improvement)
			genimproved = gen;

			if ( verbose ) {
				// print generation number and error
				// (normalized to size of training set)
				fprintf(stderr,"%d: %Lf",gen,lowest_error/trainsize);
				// validation error, if validation set provided
				if ( validatestart>=0 ) 
                	fprintf(stderr," %Lf",validateerror/validatesize);
				fprintf(stderr,"\n");
            }

#ifdef PARANOID
// verify that decompiled code works exactly the same as the machine code
if ( lowest_error!=LDBL_MAX )
{
char buf[LINE_MAX];
FILE *fp;
long double decompilederror;
// DEBUG:  verify decompiled C code for this new best individual
// decompiled machine code to C, writing to f.c
//disasm((unsigned char*)best.code,best.codelen); // DEBUG
decompile("f.c",(unsigned char*)best.code,best.codelen,constant,input);
// recompile the decompiled code
system("gcc -m32  -g   -c -o f.o f.c");
system("gcc -m32  -g -o decompiled decompiled.o load.o util.o f.o ./libdisasm.a -lm");
// and run the decompiled code
sprintf(buf,"./decompiled %s >/tmp/error",index(cmdline,'-'));
system(buf);
// read decompiled code program error from /tmp/error
fp = fopen("/tmp/error","r");
fscanf(fp,"%Lf",&decompilederror);
// rerun best machine code to determine its error
best.error = compute_error(&best,trainstart,trainstop,NULL);
printf("Machine code: %Lf   Decompiled: %Lf\n",best.error,decompilederror);
if ( fabsl(best.error - decompilederror)>1.0e-5 ) {
	printf("%Lf\n",fabsl(best.error - decompilederror));
	printf("BOOM!  Decompiled does not agree with machine code\n");
	exit(-1);
}
}
#endif // PARANOID

		}

        // break out of generation loop to start a new trial,
        // if we have gone too many evaluations without improvement
        if ( gen-genimproved >= Stagnation ) {
            printf("Stagnated at gen %d, restarting\n",gen);
            break;			// out of generation loop
        }

		// once every generational equivalent:
		//		interdeme Migration
		//		record some stats
		if ( gen && !(gen%Popsize) ) { 
			long double avgerror;
			long n;

			// interdeme Migration
			if ( Ndemes>1 && Migration>0.0 ) 
				migrate();

			// compute some stats, we might be plotting them
			avgerror = 0.0L;
			n = 0;
			// average prog size across the population
			progsize[(long)(gen/Popsize)] = 0.0;
			for ( i=0 ; i<Popsize ; i++ ) {
				progsize[(long)(gen/Popsize)] += pop[i].codelen;
				if ( pop[i].error<1.0e10 ) {
					avgerror += pop[i].error;
					n++;
				}
			}
			avgerror /= n;
			progsize[(long)(gen/Popsize)] /= Popsize;

			// error stdev across the population
			stdeverror[(long)(gen/Popsize)] = 0.0;
			for ( i=0 ; i<Popsize ; i++ ) {
				if ( pop[i].error<1.0e10 ) {
					stdeverror[(long)(gen/Popsize)] += SQ(pop[i].error - avgerror);
				}
			}
			stdeverror[(long)(gen/Popsize)] /= (n-1);
			stdeverror[(long)(gen/Popsize)] = sqrt(stdeverror[(long)(gen/Popsize)]);
		}

	}	// generation loop

    // moan about how many times was MAXCROSSOVERTRIALS exceeded excessively
    if ( verbose )
        fprintf(stderr,"bloatmoan %d\n",bloatmoan);

    // End of this trial, due to Stagnation, early stopping,
    // or max generations exceeded.
    // Compute error of best individual on test data
    if ( verbose ) {
        fprintf(stderr,"Test error this trial: %Lf\n",
                compute_error(&best,teststart,teststop,NULL)/testsize);
    }

	validateerror = compute_error(&best,validatestart,validatestop,NULL);

	// majority vote ensemble
	// record output (on all 3 sets) of best individual, for ensembling if:
	// we are running more than 1 trial, and the
	// performance on validation is better than coin flipping
	// (or we are regression, in which case we always average in the new best)
	if ( Maxtrials>1 && 
			  (validateerror <= 0.5*validatesize || !classify) ) {
		long double train_error_before,validate_error_before;
		// ensemble error on training before new member added
		train_error_before = ensemble_error(trainstart,trainstop);
		// and on validation
		validate_error_before = ensemble_error(validatestart,validatestop);
		// save outputs from best individual from this trial
		addto_ensemble(&best);
		update_ensembled_indicator();
		// We insist that addition of members to ensemble improve
		// performance on training (and validation?)
		// If ensemble error on training or validation has decreased after
		// addition of this new trial, remove the newly added ensemble member.
		// CREE SUGGESTS ONLY TRAINING SHOULD BE USED
		if ( ensemblesize>1 &&
			(ensemble_error(trainstart,trainstop) > train_error_before ||
			 ensemble_error(validatestart,validatestop) > validate_error_before )) {
			free(bestoutputs[--ensemblesize]);
			update_ensembled_indicator();
		}
	}

	// report how well the current ensemble is doing in the different sets
	if ( verbose ) {
		if ( trial == Maxtrials-1 ) 
			fprintf(stderr,"FINAL ");
		fprintf(stderr,"ensemble (size %d) errors (train validate test): %Lf %Lf %Lf\n",
			ensemblesize,
			ensemble_error(trainstart,trainstop)/(double)trainsize,
			ensemble_error(validatestart,validatestop)/(double)validatesize,
			ensemble_error(teststart,teststop)/(double)testsize);
	}

//decompile("f.c",(unsigned char*)best.code,best.codelen,constant,input);
//system("cat f.c");
//exit(0);

    if ( plotprogsize ) {
        plot(progsize,0,gen/Popsize,"Average program size by generation",cmdline,0);
    }
	if ( plotstdeverror ) {
		// start plotting at non-infinite value
		long start=0;
		while ( stdeverror[start]>1000.0L ) start++;
        plot(stdeverror,start,gen/Popsize,"Error stdev by generation",cmdline,0);
	}

	}	// trial loop

	// Dump ensembled indicator to stdout.
	// For classification problems this has {-1,1} values
	// suitable for feeding into the trade routine.
	// (Note that you must add leading 0s, to compensate for the
	// shortening of the series in makelags.pl, before calling trade)
	if ( dumpindicator ) {
		for ( i=0 ; i<teststop ; i++ ) {
			printf("%Lf\n",indicator[i]);
		}
	}

	// dump best individual to file
	if ( dumpcode ) {
		FILE *fp;
		fp = fopen(dumpcode,"w");
		if ( !fp ) {
			fprintf(stderr,"Error, couldn't open %s\n",dumpcode);
			exit(-1);
		}
		fwrite(&best,sizeof(Pop),1,fp);
		// also dump array of random constants, needed to rerun the program
		fwrite(&nconstants,sizeof(long),1,fp);
		fwrite(&constant[0],sizeof(double),nconstants,fp);
		fclose(fp);
	}

    cputime = ((float)clock() - (float)starttime)/((float)CLOCKS_PER_SEC);
	if ( verbose )
		fprintf(stderr,"Cpu time: %f\n",cputime);

	// tidy up
	free(indicator);
	free(stdeverror);
	free(progsize);
	free(constant);
	for ( i=0 ; i<ninputs ; i++ ) {
		free(inputs[i]);
		free(inputsresampled[i]);
	}
	free(input);
	free(outputs);
	free(pop);
}
