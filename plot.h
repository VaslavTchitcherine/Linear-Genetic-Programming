
/*
 * plot.h
 * plotting routines
 */

#ifndef PLOTH
#define PLOTH

// plot generic series
void plot(double *s,long start,long stop,char *title,char *cmdline,long split);

// plot multiple curves (e.g. portfolio vector)
void plot_multi(char *filename,long start,long stop,long nvars,char *title,char *cmdline);

#endif // PLOTH
