
/*
 * util.h
 * assorted utilities
 */

#ifndef UTILH
#define UTILH

#define SQ(x) ((x)*(x))
#define MIN2(x,y) (((x)<(y))?(x):(y))
#define MAX2(x,y) (((x)>(y))?(x):(y))
#define ABS(x) ((x)>=0?(x):(-x))

void fastrandseed(unsigned char);	// initialize the random number generator
float fastrand();					// return random number in [0.0,1.0]
void fastrand_dumpseeds();			// print current seeds (for debug)
void fastrand_setseeds(long,long,long,long);		// set seeds
long fastrand_seedx();				// return seedx

// malloc with error checking
void *Malloc(long size);

#endif // UTILH
