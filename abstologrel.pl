#!/usr/bin/perl
#
# abstologrel.pl
# relativize prices:  turn table of absolute prices into log relative prices,
# e.g. abstologrel.pl --delay=1 </home/egullich/forex/data/yahoo/nasdaq100/AAPL_close >/tmp/rel
#

use Getopt::Long;

# default is one timestep between current and prev time for relative price
$delay = 1;

GetOptions(
        "delay=i" => \$delay);

# read entire sequence from stdin
@in = <>;

for ( $i=0 ; $i<=$#in ; $i++ ) {
	# prehistoric values are not known, just print log(1)
	if ( $i-$delay < 0 ) {
		printf "0.0\n";
	}
	else {
		print log($in[$i] / $in[$i-$delay]),"\n";
	}
}

