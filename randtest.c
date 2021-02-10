
#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>

#include <gsl/gsl_rng.h>
#include <gsl/gsl_randist.h>

// instead of calling gsl_rng_env_setup() to acquire seed from the
// environment, we access and set this GSL library global directly
extern unsigned long int gsl_rng_default_seed;

double weights[] = {0.01, 0.5 , 0.3};

main()
{
	long seed;
	int randomfd;
	long i;
	const gsl_rng_type *T;
  	gsl_rng *r;

	// set random seed from /dev/urandom
	if ( 0 > (randomfd = open("/dev/urandom",O_RDONLY)) ) {
		fprintf(stderr,"Couldn't open /dev/urandom\n");
		exit(-1);
	}
	if ( 4 != read(randomfd,&seed,4) ) {
		fprintf(stderr,"Couldn't read 4 bytes from /dev/urandom\n");
		exit(-1);
	}
	close(randomfd);

	gsl_rng_default_seed = seed;

	T = gsl_rng_default;
	r = gsl_rng_alloc (T);

	gsl_ran_discrete_t *gsl_ran_discrete_t;
	gsl_ran_discrete_t = gsl_ran_discrete_preproc(3,weights);

	for ( i=0 ; i<20 ; i++ ) {
		printf("%d\n",gsl_ran_discrete(r,gsl_ran_discrete_t));
	}

	gsl_ran_discrete_free(gsl_ran_discrete_t);
	gsl_rng_free(r);
}

