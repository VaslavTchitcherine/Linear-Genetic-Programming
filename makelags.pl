#!/usr/bin/perl
#
# makelags.pl
#
# Filter takes a univariate series, and produces
# a file with the specified number of lags, one lag per column.
# The original series is retained as the first column.
# (The initial nlags-1 rows have bogus lags, not fully initialized,
# and should probably be trimmed by the caller.
#
# Example:
#	seq 10 | makelags.pl --nlags=4
#

use Getopt::Long;

GetOptions("nlags=i" => \$nlags);
die "Must specify number of lags with -nlags\n" if !$nlags;

@lags = ();
chop($firstval = <>);
for ( $i=0 ; $i<$nlags-1 ; $i++ ) {
	$lags[$i] = $firstval;
}
print $firstval,' ',join(' ',@lags),"\n";

while ( <> ) {
	chop;
	print $_,' ',join(' ',@lags),"\n";
	#print reverse(join(' ',@lags)),' ',$_,"\n";
	unshift @lags,$_;
	pop @lags;
}

