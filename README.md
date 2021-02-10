
Machine Code Linear Genetic Programming

This code implements a GP system that evolves x87 FPU machine code.
Linear strings of machine code bytes are evolved, so runtime is
extremely fast compared with traditional C-based GP systems.

It performs very well on traditional machine learning benchmarks,
included in the data subdirectory (5 parity, sunspots, etc.)
See data/README.

It was inspired by the commercial Windows software product Discipulus,
and the associated patent (doc/US5841947.pdf).

An IA32 or IA64 architecture machine (e.g. Pentium, Athlon) and gcc are required.
You must run noexec=false and noexec32=false as kernel arguments,
to permit execution of the evolving machine code.

Note that if running under gdb, you should set:
 handle SIGFPE nostop
 handle SIGFPE noprint
 handle SIGFPE pass
to ensure floating point exceptions as passed on to lgp, where they
are handled.

RUNNING LGP

In case it's not obvious, when you see output like:

    Generating initial population (size 10000)...done
    0: 0.508889 0.517949
    1: 0.483333 0.533333
    16: 0.464444 0.492308

The number before the ':' is a count of the number of breedings performed.
The program uses steady state tournament, so a generational equivalent is
popsize/breedings.

The first floating point number on each output line is the error on
training, the second number the error on validation set,
if --validate was specified on the command line.  Which it should be, for
all except trivial problems not prone to overfitting.

Error is defined as the fraction of values misclassified,
for classification problems.
(Ensembling may not work for regression problems, and support for
regression might altogether fall out of this code sometime soon).

Error on training should steadily decrease during a trial.
As generally should fitness on validation, for easy problems not 
prone to overfitting.

lgp command line arguments
==========================

In addition to the following command line arguments, other lgp
parameters can be set in lgp.h, though their default settings seem
fine for most problems.

Optional command line arguments are given in square brackets.

--inputfile=<filename>
Specifies the file containing the series of input and output patterns.
Be sure there are no leading or trailing blanks on the lines.
File format, and the makelags.pl program that can produce the
input file from a univariate time series, are discussed below.

--train=<int>/<int>
Specifies the range of lines of the input file used for training.
Training starts at the first index, ends before the second index.

[--validate=<int>/<int>]
If specified, gives range of a validation set.
Used for early stopping, if --earlystopping is specified.

--test=<int>/<int>
A test range, used for out of sample evaluation.

[--popsize=<int>]
Size of population (default: 1000)
Note that each individual in the population uses 2060 bytes.
This could be further reduced at the expense of performance,
but extremely large populations such as --popsize=500000 can be 
run in a gig of RAM.  For difficult problems, --popsize=10000
or even --popsize=100000 are recommended.

[--nregisters=<int>]
Specified number of FPU registers in the range [1,8] to be used by the
machine code programs.  If not specified, a random number of registers
is selected for each trial.  Probably best to not specify this.

[--maxtrials=<int>]
Maximum number of trials.  Termination can occur earlier if
a perfect solution is found, though this happens only for simple toy problems.
The default is 100, which is probably a reasonable size for an ensemble.

[--mutation_prob=<float>]
Probability of mutation after breeding. (default: 0.5)
For difficult problems, a much higher rate (0.95) may be useful
to promote diversity in the population.
(See http://citeseer.csail.mit.edu/banzhaf96effect.html)

[--maxgenerations=<int>]
Maximum number of generations to run in a trial.
Note that we use steady state tournament of size 4, so a generational
equivalent is defined as number of breedings divided by population size.
(default: 1000)

[--stagnation=<int>]
If specified, the current trial is terminated if this
many breedings have occured, without improvement on the training set.
We use breedings rather than generations, otherwise this value would have to
scale inversely with popsize.  Note that if the value for --stagnation
>= maxgenerations*popsize, then it has no effect.
(default: 100000)

[--ndemes=<int>]
Number of demes in the population.  Breeding always occurs intra-deme,
with inter-deme migration occuring at the end of a generational
equivalent, as specified by --migration.
(default: 10)

[--migration=<float>]
Fraction of the population from each deme migrating at the end of
a generational equivalent.  Migration happens inter-deme with a
ring topology.  The best individuals from each deme d overwrite the
worst individuals in deme (d+1)%ndemes.

[--plot=[progsize|stdeverror]
Produces plots of average program size, or standard deviation of program
error, at the end of each trial.

[--dump=[error]
Each time a individual with better performance arises,
the breeding number, performance on training, and performance on
validation are written to stdout.   For classification problems,
performance is classification error fraction in the range [0.0,1.0].
For regression problems, performance is sum of squared errors across the
training set. In both cases, error rate decreases as as fitness improves.

[--dumpcode=<filename>]
At the end of execution, the best individual is dumped to a file.
The Pop struct and array of random constants are both written to the
file in binary form.  Most useful with --maxtrials=1 when lgp
invoked with trials in parallel on the cluster, and ensembling
is done by the calling program.

[--deterministic]
Fixed random seeds are used, so the run is repeatable.

[--bagging]
Each trial we train on a different bootstrap resampling of the training input.

[--earlystopping=<float>]
Stop a trial when validation set error increases, after
having been less than this value.
We use the best individual before the fall in validation performance.
Maybe it makes sense to set this to the minimum error rate we need to
be sufficiently profitable to trade, perhaps --earlystopping=0.475
(i.e. a hit rate of 52.5%).
AS IMPLEMENTED, THIS IS PROBABLY TOO BRUTAL.

[--verbose]
Emits diagnostics to stderr, including breeding number and performance,
each time performance increases on training set.

[--initialfitness]
Forces initial pop members to have better than coin-flipping performance,
generating random individuals until --popsize competent individuals
have been produced.  [DEPRECATED]


Input file format
=================

The input file contains one input pattern per line.  Additionally,
the last field on each line is the training output for that pattern.

The lgp program is designed to handle both binary classification
and regression programs.  If the training outputs (i.e. last field
on each line) are all in the set {-1,1}, then the problem is assumed to be a
classification problem, and lgp verbose outputs are given as misclassification
error fraction, in the range [0.0,1.0].  A perfect program would
have an error rate of 0.0.
Otherwise, if the training outputs are floating point values, then the
problem is assumed to be a regression problem, and lgp verbose outputs
are given as the regression error, the sum of squares of the differences
between lgp output and desired output.  (Or the sum of absolute
values of differences, #ifdef NORM1 in lgp.h)

The program makeinputs.pl can be used to transform a univariate time
series into a file appropriate for input to lgp.

For example, if the file /tmp/mispricing contains a mispricing
timeseries (as produced by statarb.pl), then an lgp input file
with 10 lags could be produced with:

   makeinputs.pl --classify --nlags=10 --inputfile=/tmp/mispricing > data/mispricing0

Note that due to the time needed to initialize the lags,
the output file will be shorter than the input file by the specified
number of lags.


Command line arguments
======================

--inputfile=<filename>
Specifies the file containing the univariate input series, one value per line.

--nlags=<int>
The given number of lags are constructed for each input pattern.

--classify
If specified, the problem is a classification problem.  The last field
of each output line t, following the lags, will be 1 if the univariate
series value at the next timestep t+1 is greater than the series value 
at the current timestep t.  Otherwise the last field will be -1,
indicating the series value at the next timestep t+1 is less then
the series value at the current timestep t.
Note that either --classify or --regress must be specified.

--regress
If specified, the problem is a regression problem.  The last field
of each output line t, following the lags, will be the univariate
series value at the next timestep t+1.
I.e. this last field is the correct output for the current pattern of lags.
Note that either --classify or --regress must be specified.

--takediffs
This option causes the program to output lags of first order differences
of the input series.

--normalize
This option causes the program to normalize the input series into the
range [-1,1] using the standard deviation of the data in the training range.
        
--outlier=<float>
Specifies the number of standard deviations at which an input
point is considered an outlier.  (default: 3.0)
        
--train=<int>/<int>
Gives the range over which stats are computed for the normalization.
We don't want to peek ahead at validation or training data when
determining how to normalize.
Must be supplied if --normalize is specified.

DEPENDENCIES

libdisasm.so from http://bastard.sourceforge.net/libdisasm.html
Supplied with libdisasm_0.21-pre2 from bastard.sf.net
This should be compiled with -m32 for IA32.
A copy of the correctly compiled libdisasm.a and the libdis.h headers
are included here.


