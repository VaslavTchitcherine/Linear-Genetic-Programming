#!/usr/bin/perl
#
# signum.pl
# filter that returns 1 for each positive input, -1 for negative or zero
# 

while ( <> ) {
	if ( $_>0.0 ) {
		print "1\n";
	}
	else {
		print "-1\n";
	}
}
