
/*
 * decompiled.c
 * Standalone program to call decompiled C code produced by decompile()
 * This code is written to the function f.c by lgp.c
 * Given the inputfile and training range, run the decompiled code and
 * write the error to stderr.
 * This had better agree with the output of compute_error().
 *
 * Example:   decompiled --inputfile=data/qqqqdiffs --train=0/476 --dump=indicator > /tmp/ind
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <fcntl.h>
#include <math.h>
#include <signal.h>
#include <values.h>         // for LDBL_MAX
#include <fenv.h>

#include "lgp.h"
#include "load.h"
#include "util.h"

// the decompiled function in f.c
extern long double lgp(double *v);

// training window
// (not really training, we just run the decompiled program on this region)
long trainstart = -1,trainstop,trainsize;

// array of input variables, cols are malloced in load()
double *inputs[MAXINPUTS];
long ninputs=0;     // width of input pattern (i.e. # lags)

// dump indicator to stdout, for use with trade.c
unsigned char dumpindicator=0;

// desired outputs
double *outputs;

// program runs as classifier or regression, depending on whether the
// values of the last col of input patterns are binary or float
unsigned char classify;

// if we get a SIGFPE, print maximum possible error and exit
void sigfpe(int sig)
{
    printf("%lf\n",LDBL_MAX);
	exit(0);
}

void scanargs(long argc, char *argv[])
{
	long i;
    int c;
	char *filename=NULL;
    char *s,*slash;

	while (1) {
      static struct option long_options[] =
        {
			{"inputfile",		required_argument,	0, 'i'},
            {"train",          	required_argument,  0, 't'},
            {"deterministic",  	required_argument,  0, 'D'},
            {"dump",			required_argument,  0, 'd'},
			{0, 0, 0, 0}
        };

      // getopt_long stores the option index here
      int option_index = 0;
      c = getopt_long_only(argc, argv, "i:t:D:d:",
                       long_options, &option_index);

      /* Detect the end of the options. */
      if (c == -1)
        break;

      switch (c) {

		case 'i':		// name of file containing input patterns to read
			filename = optarg;
            break;

		case 't':		// "training" window start/stop
            s = optarg;
            slash = index(s,'/');
            if ( !slash ) {
                fprintf(stderr,"Error, --train must specify start/stop, e.g. --train=0/1400\n");
                exit(-1);
            }
            *slash = '\0';
            if ( s[0] == '=' )
                s++;
            trainstart = atoi(s);
            trainstop = atoi(slash+1);
            trainsize = trainstop-trainstart;
            break;

		case 'D':		// this is just ignored here
            break;

		case 'd':		// dump
            if ( !strncmp(optarg,"indicator",1) ) {
                // dump indicator series to stdout
                dumpindicator = 1;
            }
			// ignore this lgp cmdline argument
            else if ( !strncmp(optarg,"error",1) ) {
			}
            else {
                fprintf(stderr,"Unknown argument to --dump: %s\n",optarg);
                exit(-1);
            }
            break;

        case '?': 		// (getopt_long already printed an error message)
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

	// no inputseries -i specified
	if ( !filename ) {
		fprintf(stderr,"Must specify file containing input patterns with -i\n");
		exit(-1);
	}

    if ( trainstart<0 ) {
        fprintf(stderr,"Error, train range must be specified with --train\n");
        exit(-1);
    }

	// load input patterns into inputs[] and outputs[] arrays, also sets ninputs
	outputs = load(filename,trainstop,inputs,&ninputs);

	// Check if the problem is classification or regression.
	// It is classification if all outputs are binary.
	classify = 1;
	for ( i=0 ; i<trainstop ; i++ ) {
		// if any output not binary, then it's a regression problem
		if ( outputs[i]!=1.0 && outputs[i]!=0.0 ) {
			classify = 0;
			break;
		}
	}
}

int main(int argc, char *argv[])
{
	long j;
	double *input;
	long double output;
	long double error;
	long t;

    // enable all floating point exceptions
	// (all except FE_INEXACT and FE_UNDERFLOW)
    feenableexcept(FE_DIVBYZERO|FE_OVERFLOW|FE_INVALID);
    signal(SIGFPE,sigfpe);

    scanargs(argc,argv);

    // Pool of input variables.
    // To eval the machine code on a given specific input pattern from inputs[]
    // that pattern is copied into this input array.
    input = (double*)Malloc(ninputs*sizeof(double));

	error = 0.0;

	// compute error on "training" section
	for ( t=trainstart ; t<trainstop ; t++ ) {

        // copy input pattern for this timestep t into input array,
        for ( j=0 ; j<ninputs ; j++ ) {
            input[j] = inputs[j][t];
        }

		// execute the decompiled machine code (in f.c)
		output = lgp(input);

		// output indicator for use with trade()
		if ( dumpindicator ) {
			if ( output>THRESHOLD ) 
				printf("1.0\n");
			else
				printf("-1.0\n");
		}

//fprintf(stderr,"Output (decompiled) %d: %Lf\n",t,output);
//printf("%f %f\n",input[0],outputs[t]);

        // compare output with desired output
        if ( classify ) {
            // classification problem, just count the misclassifications
            // (cf. Kuncheva "counting estimator" p.8)
            if ( (output>THRESHOLD) != outputs[t] ) {
                // just count the number of misclassified patterns
                error += 1.0;
            }
        }
        // regression problem
        else {
#ifdef NORM1
            error += fabsl(((long double)outputs[t]) - output);
#endif // NORM1
#ifdef NORM2
            error += SQ(((long double)outputs[t]) - output);
#endif // NORM2
        }

	}

	// print the training error to stdout
	// (for comparison with machine code output when PARANOID is #defined)
	printf("%Lf\n",error);

	free(input);
}
