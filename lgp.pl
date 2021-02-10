#!/usr/bin/perl
#
# lgp.pl
# Call lgp for many trials, plot training and validation error by gen
#
# e.g. lgp.pl --inputfile=data/qqqq --train=0/300 --validate=300/400 --test=400/508 --maxgenerations=50 --maxtrials=20
#

$| = 1;

use Getopt::Long;

# default values of optional parameters
$maxtrials = 100;

# since @ARGV gets destroyed by call to GetOptions
$args = join(' ',@ARGV);

GetOptions("inputfile=s" => \$inputfile,
		"train=s" => \$train,
		"validate=s" => \$validate,
        "test=s" => \$test,
        "maxgenerations=i" => \$maxgenerations,
        "maxtrials=i" => \$maxtrials);

die "Must specify number of trials with -maxtrials\n" if !$maxtrials;

# make sure subdir exists in which to store error by generation for each trial
mkdir('errors') if ! -d 'errors';

# clear old errors from the errors subdirectory
system("/bin/rm errors/*");

# remove the --maxtrials arg from cmdline, since we loop explicitly here, with
# --maxtrials=1, so as to save the errors from each trial for plotting
$args =~ s/--maxtrials=[0-9]+//;

# run the requested number of trials
# errors by generation (for training and validation) are dumped to files
# in the subdirectory, errors
for ( $trial=0 ; $trial<$maxtrials ; $trial++ ) {
	# each call only does 1 trial
	$cmd = "./lgp $args --maxtrials=1 --dump=error >errors/$trial";
print "DEBUG: $cmd","\n";
    `$cmd`;
}

# plot all of the error curves together
open(PLOTDATA, "> /tmp/gnuplot");
print PLOTDATA "set term X11\n";
print PLOTDATA "set style data lines\n";
print PLOTDATA "set title \"lgp errors\"\n";
print PLOTDATA "set xlabel \"generation\"\n";
print PLOTDATA "set ylabel \"hit rate\"\n";
#print PLOTDATA "set nokey\n";
print PLOTDATA "plot ";
print PLOTDATA "\"errors/0\" using 1:2 lt 1 title \"training\",\"\" using 1:3 lt 2 title \"validation\",\"\" using 1:4 lt 3 title \"test\",";
for ( $trial=1 ; $trial<$maxtrials ; $trial++ ) {
	print PLOTDATA "\"errors/$trial\" using 1:2 lt 1 notitle ,\"\" using 1:3 lt 2 notitle,\"\" using 1:4 lt 3 notitle,";
}
print PLOTDATA "0.5\n";
close PLOTDATA;
system("/usr/local/bin/gnuplot -persist /tmp/gnuplot");

