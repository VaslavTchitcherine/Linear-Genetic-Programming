#!/usr/bin/perl
#
# diffs.pl
# Take first order differences of series on stdin

@s = <>;

# initial difference not known, assumed to be zero
print "0.0\n";

for ( $i=1 ; $i<=$#s ; $i++ ) {
	print $s[$i] - $s[$i-1],"\n";
}

