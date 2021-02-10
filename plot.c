
/*
 * plot.c
 * Invoke gnuplot to produce a variety of plots
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <values.h>         // for DBL_MAX

#define MAXPLOTSIZE 10000

// plot generic series
void plot(double *p,long start, long stop,char *title,char *cmdline,long split)
{
	FILE *fp;
	long i;
	double min,max;
	long decimate;

	min = DBL_MAX;
	max = -DBL_MAX;

	// decimation factor (since gnuplot dies on huge plots)
	decimate = (stop-start)/MAXPLOTSIZE;
	if ( decimate < 1 )
		decimate = 1;

	// dump data to the file named plot
	fp = fopen("plot","w");
	if ( !fp ) {
		fprintf(stderr,"Couldn't open file plot: for writing\n");
		exit(-1);
	}
	for ( i=start ; i<stop ; i+=decimate ) {
		fprintf(fp,"%d %lf\n",i,p[i]);
		if ( p[i]>max )
			max = p[i];
		if ( p[i]<min )
			min = p[i];
	}
	fclose(fp);

	// gnuplot commands
	fp = fopen("plotcmds","w");
	if (!fp ) {
		fprintf(stderr,"Couldn't open file: plotcmds\n");
		exit(-1);
	}
	fprintf(fp,"set style data lines\n");
	fprintf(fp,"set title \"%s\\n%s\"\n",title,cmdline);
	fprintf(fp,"unset label\n");
	fprintf(fp,"set xlabel \"time\"\n");
	fprintf(fp,"unset key\n");
	fprintf(fp,"set xrange[%d:%d]\n",start,stop);
	fprintf(fp,"set x2range[%d:%d]\n",start,stop);
	fprintf(fp,"set yrange[%lf:%lf]\n",min-(max-min)/20.0,max+(max-min)/20.0);
	fprintf(fp,"set y2range[%lf:%lf]\n",min-(max-min)/20.0,max+(max-min)/20.0);
	fprintf(fp,"set x2tics\n");
	fprintf(fp,"set y2tics\n");
	fprintf(fp,"set grid\n");

    fprintf(fp,"set parametric\n");
    fprintf(fp,"const=%ld\n",split-start);
    fprintf(fp,"set trange[%lf:%lf]\n",min,max);
    fprintf(fp,"plot \"plot\" using 1:2,const,t\n");

	//fprintf(fp,"plot \"plot\" using 1:2,0\n");
	fclose(fp);
	system("gnuplot -persist plotcmds");

	// erase the temporary files
	//unlink("plotcmds");
	//unlink("plot");
}


// plot multiple curves (e.g. portfolio vector)
void plot_multi(char *filename,long start,long stop,long nvars,char *title,char *cmdline)
{
	long i;
    FILE *fp;

    // gnuplot commands
    fp = fopen("plotcmds","w");
    if (!fp ) {
        fprintf(stderr,"Couldn't open file: plotcmds\n");
        exit(-1);
    }
    fprintf(fp,"set style data lines\n");
    fprintf(fp,"set title \"%s\\n%s\"\n",title,cmdline);
    fprintf(fp,"unset label\n");
    fprintf(fp,"unset key\n");
	fprintf(fp,"set xlabel \"time\"\n");
    fprintf(fp,"plot \"%s\" using 1",filename);
	for ( i=2 ; i<=nvars ; i++ )
    	fprintf(fp,",\"\" using %ld",i);
    fprintf(fp,"\n");
    fclose(fp);
    system("gnuplot -persist plotcmds");

    // erase the temporary files
    //unlink("plotcmds");

}

