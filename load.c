
/*
 * load.c
 * load input patterns from file
 *
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <ctype.h>
#include <limits.h>         // for LINE_MAX

#include "lgp.h"

// # lines in input file before realloc
#define CHUNKSIZE 10000

// load input patterns from file,
// sets inputs, ninputs, and outputs
double *load(char *file,long teststop,double *inputs[MAXINPUTS],long *ninputs)
{
	long i;
	unsigned char firstline=1;	// reading the first line?
    char *bp;
	char buf[LINE_MAX];
	FILE *fp;
    long maxlines;		// max lines we can read before realloc
	long nlines=0;		// total number input lines read
	double *outputs;

	fp = fopen(file,"r");
	if ( !fp ) {
		fprintf(stderr,"Error, couldn't open input file: %s\n",file);
		exit(-1);
	}

    // we do not know how many lines are in the input file, so guess
    // CHUNKSIZE and realloc when necessary
    maxlines = CHUNKSIZE;

    while ( fgets(buf,LINE_MAX,fp) ) {
		// no need to read past the end of the test range, if
		// teststop was specified
		if ( teststop && nlines >= teststop ) 
			break;

        // skip comments
        if ( '#' == buf[0] )
            continue;

        // sanity check, data in buffer must be numeric
		if ( !isdigit(buf[0]) && '.' != buf[0] && '-' != buf[0] && ' ' != buf[0]) {
            fprintf(stderr,"Error, garbage in input file: %s\n",buf);
            exit(-1);
        }

        // is this the first line?
        if ( firstline ) {
			bp = buf;
			*ninputs = 1;
			// count number of fields in 1st line of input file
        	while ( *bp != '\n' ) {
				if ( *bp == ' ' || *bp == '\t' ) {
					(*ninputs)++;
				}
				// skip over additional blanks or tabs
				while ( *bp == ' ' || *bp == '\t' ) bp++;
				// check we do not have too many inputs
				if ( (*ninputs) >= MAXINPUTS ) {
					fprintf(stderr,"Error, increase MAXINPUTS\n");
					exit(-1);
				}
				bp++;
			}

			// subtract one, since last field is the pattern output
			(*ninputs) -= 1;

            // malloc arrays for storage of the inputs and outputs
			for ( i=0 ; i<*ninputs ; i++ ) {
            	inputs[i] = (double*)Malloc(maxlines*sizeof(double));
			}
			outputs = (double*)Malloc(maxlines*sizeof(double));
            firstline = 0;
        }

        bp = buf;
        for ( i=0 ; i<*ninputs ; i++ ) {
            if ( 1 != sscanf(bp,"%lf",&inputs[i][nlines]) ) {
				fprintf(stderr,"Error, read failed line %d\n",nlines);
				exit(-1);
			}
            // scan to and past next blank
            while ( *bp != ' ' && *bp != '\t' ) bp++;
        	while ( *bp == ' ' || *bp == '\t' ) bp++;
        }
		// last col is the output
		if ( 1 != sscanf(bp,"%lf",&outputs[nlines]) ) {
			fprintf(stderr,"Error, read failed line %d\n",nlines);
			exit(-1);
		}

        // do we need to realloc?
        if ( nlines++ == maxlines ) {
            maxlines += CHUNKSIZE;
            for ( i=0 ; i<*ninputs ; i++ ) {
                inputs[i] = (double*)realloc(inputs[i],maxlines*sizeof(double));
				if ( !inputs[i] ) {
                	fprintf(stderr,"Couldn't realloc (%ld)\n",maxlines);
                	exit(-1);
				}
            }
            outputs = (double*)realloc(outputs,maxlines*sizeof(double));
			if ( !outputs ) {
                fprintf(stderr,"Couldn't realloc (%ld)\n",maxlines);
                exit(-1);
			}
        }
	}

	// sanity: 
	if ( nlines < teststop ) {
		fprintf(stderr,"Error, only %d lines in input file, but teststop %d\n",
			nlines,teststop);
		exit(-1);
	}

	fclose(fp);

	return outputs;
}
