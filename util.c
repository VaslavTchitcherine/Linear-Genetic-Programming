
/*
 * util.c
 * assorted utilities
 */

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <math.h>

long seedx,seedy,seedz,seedw;	// 4 seeds for rng with (2^128)-1 period

void fastrand_dumpseeds()
{
    fprintf(stderr,"seedx = %ld;\n",seedx);
    fprintf(stderr,"seedy = %ld;\n",seedy);
    fprintf(stderr,"seedz = %ld;\n",seedz);
    fprintf(stderr,"seedw = %ld;\n",seedw);
}

long fastrand_seedx()
{
	return seedx;
}

// *very* fast xor random number generator
// (from Marsaglia, G. "Xorshift RNGs", Florida State University,
// http://www.jstatsoft.org/v08/i14/xorshift.pdf)
// this is about 25 times as fast as drand48()
void fastrandseed(unsigned char deterministic)
{
    int randomfd;
	long seed;
        
	if (deterministic) {
seedx = 81723779;
seedy = 1524285617;
seedz = 1826656935;
seedw = 242948714;
	}
	else {
		// use /dev/urandom since it doesn't block if insufficient entropy in pool
		if ( 0 > (randomfd = open("/dev/urandom",O_RDONLY)) ) {
			fprintf(stderr,"Couldn't open /dev/urandom\n");
			exit(-1);
		}
		if ( 4 != read(randomfd,&seed,4) ) {
			fprintf(stderr,"Couldn't read 4 bytes from /dev/urandom\n");
			exit(-1);
		}
		close(randomfd);
		srand48(seed);
		
		seedx = lrand48();
		seedy = lrand48();
		seedz = lrand48();
		seedw = lrand48();
		//fastrand_dumpseeds();
	}
}

// return a random number in [0,1] with uniform distribution
float fastrand()
{
	long tmp;
	float r;

	tmp = seedx ^ (seedx<<15);
	seedx = seedy;
	seedy = seedz;
	seedz = seedw;
    seedw = (seedw ^ (seedw>>21)) ^ (tmp ^ (tmp>>4));

	// seedw is [0,2^31-1], scale into [0,1]
	// note that if denominator is any smaller, then when seedw=2147483647
	// r==1.0 in single precision floating point
	r = ((float)seedw) / 2147483712.0;

	return r;
}

void fastrand_setseeds(long sx, long sy, long sz, long sw)
{
	seedx = sx;
	seedy = sy;
	seedz = sz;
	seedw = sw;
}

// malloc with error checking
void *Malloc(long size)
{
	void *m;

	m = malloc(size);
	if ( !m ) {
		fprintf(stderr,"Error, malloc failed\n");
		exit(-1);
	}
	return m;
}
