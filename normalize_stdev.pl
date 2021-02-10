#!/usr/bin/perl
#
# normalize_stdev.pl
# Normalize so output has standard deviation of 1.
# Only the specified training range is used to compute stats for normalization.
# Outliers are
#
# e.g.
#	(misprice --i nasdaq100_daily --train=0/300 --test=300/437 --dump=mispricing > /tmp/mispricing) >&/tmp/coeff
#	normalize_stdev.pl --outlier=3.0 --train=0/300 </tmp/mispricing >/tmp/mispricing_norm
#	isa.pl -k=1.5 --theta=0.5 </tmp/mispricing_norm >/tmp/indicator
#

use Getopt::Long;

# default threshold for removal of wild outliers
$outlier = 3.0;

GetOptions("train=s" => \$train,
		"outlier=f" => \$outlier);

die "Must specify training range with --train=<start>/<stop>\n"
    unless $train =~ m/([0-9]*)\/([0-9]*)/;
($trainstart,$trainstop) = ($1,$2);

# read in the entire series
@ind = <STDIN>;

# compute avg and variance
$sum = 0.0;
for ( $i=$trainstart ; $i<$trainstop ; $i++ ) {
	$sum += $ind[$i];
}
$avg = $sum / ($trainstop-$trainstart);
$variance = 0.0;
for ( $i=$trainstart ; $i<$trainstop ; $i++ ) {
	$variance += ($ind[$i]-$avg)*($ind[$i]-$avg);
}
$variance /= ($trainstop-$trainstart-1);
$stdev = sqrt($variance);

$outliercount = 0;
# subtract mean and scale this difference series by stdev
for ( $i=0 ; $i<=$#ind ; $i++ ) {
	$ind[$i] = ($ind[$i]-$avg) / $stdev;

	# outlier removal (only from training region)
	if ( abs($ind[$i]) > $outlier and $i>=$trainstart and $i<$trainstop ) {
		$ind[$i] = 0.0;
		$outliercount++;
	}
}

# dump scaled series to stdout
for ( $i=0 ; $i<=$#ind ; $i++ ) {
	print $ind[$i],"\n";
}

print STDERR "Number of outliers: $outliercount\n";
