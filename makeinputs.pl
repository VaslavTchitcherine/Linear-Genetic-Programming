#!/usr/bin/perl
#
# makeinputs.pl
#
# Takes a univariate timeseries file, produces
# a file of lags with training values, which can be input to lgp. 
# Note that the timeseries should probably be stationary, take differences
# or log ratio of non-stationary series before using this code.
# Perhaps normalization will also be needed.
# In addition to the lags, each row of the output file also has a 
# training value obtained by peeking one timestep into the future.
# This last value on each output line is 1 when the timeseries will go up
# at the next step, else it is -1.
# (I.e. this last column is the classification of each training vector).
# If --regress is specified instead of --classify then
# the last value on each output line is the actual next value of the timeseries.
#
# Example:
#	awk '{print 0.5*($1+$2)}' <rawdata/eurusd_gainhourly >/tmp/mid
#	abstologrel.pl </tmp/mid >/tmp/logrel
#	normalize_stdev.pl --outlier=10 --train=0/30000  </tmp/logrel >/tmp/norm
#	makeinputs.pl --classify --nlags=10 </tmp/norm >/tmp/lags
#

use Getopt::Long;

GetOptions(
		"classify" => \$classify,
		"regress" => \$regress,
        "nlags=i" => \$nlags);

die "Must specify # lags with --nlags" if !$nlags;
die "Must specify one of --classify or --regress" if !$classify and !$regress;
die "Only specify one of --classify or --regress" if $classify and $regress;
die "Could not find makelags.pl executable" unless -e "makelags.pl";
die "Could not find signum.pl executable" unless -e "signum.pl";

# Note that the first $nlags-1 rows will be bogus, as the lags are not
# fully initialized. We trim these off later in this routine.
# (Note this is reading from stdin)
`./makelags.pl -nlags $nlags >/tmp/lags`;

# Add a final column, the target. This peeks one step into the future.
# This last column is set to differences (for an up/down classification
# problem) or to the value of the series at the next timestep
# (for a regression problem).
# In either case, this last column will be 1 line shorter than the input,
# as we chop off 1 element from the head (so when pasted on to the inputs,
# it is one step ahead with respect to them).
chomp($serieslen = `wc -l /tmp/lags`);
$chop1 = $serieslen-1;
if ( $classify ) {
	# take first order differences of stdin, then
	# map positive differences to 1, negative differences to 0
	# (i.e. an indication of direction of move to next timeseries value)
	`./diffs.pl | ./signum.pl | tail -$chop1 > /tmp/lastcol`;
}
else {
	# just copy input file, chopping off the 1st element to timeshift
	`tail -$chop1 $inputfile > /tmp/lastcol`;
}

# now get rid of the initial lines, which have bogus lags
$taillen = $serieslen-$nlags;
    
# Catenate on the col of classification values (/tmp/lastcol).
# Also, get rid of last line (with the head --lines=-1), as classification
# value is not known for the final value.
print `paste -d ' ' /tmp/lags /tmp/lastcol | head --lines=-1 | tail -$taillen`;

