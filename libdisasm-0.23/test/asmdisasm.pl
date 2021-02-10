#!/usr/bin/perl
# test_x86dis.pl
# ... actually this is a general test utility for libdisasm; however, it
# is also useful for comparing the AT&T syntax output of x86dis (which is
# generated by libdisasm) against what GNU as(1) expects.
# NOTE: this script expects to be run from $LIBDISASM/libdisasm/test
# and uses the instructions in ia32_test_insn.S to test disassembly.
use warnings;
use strict;

# assembler settings
my $asm_file = $ARGV[0] || 'ia32_test_insn.S';
my $obj_file = $ARGV[1] || 'ia32.o';
my $disasm_format = $ARGV[2] || "att";

# relative path, ld_library_path for running x86dis from compile dir
my $x86dis = '../x86dis/x86dis';
# path is not needed now that libtool is being used
#my $ldpath = 'LD_LIBRARY_PATH="../libdisasm"';
my $ldpath = '';

# uninitialized stuff
my ($line, $output);

system "as --32 -o $obj_file $asm_file";
exit(1) if ($? != 0);

$output = (grep(/\.text/,`objdump -h $obj_file`))[0];
$output =~ s/^\s+//g;
my ($idx,$name,$size,$vma,$lma,$offset,$align)=split(/\s+/,$output);
$size = hex($size);
$offset = hex($offset);

$output = `$ldpath $x86dis -f $obj_file -s $disasm_format -r $offset $size`;
exit(1) if ($? != 0);

foreach ( split /\n/, $output ) {
	chomp;

	# skip comment lines
	next if /^#/;

	$line = $_;
	$line =~ s/\s+/ /g;
	$line =~ s/\s+$//;
	$line =~ s/#.*//;
	$line = lc $line;
	if ($line =~ s/^[0-9][0-9a-f]+\s+(([0-9a-f]{2} )+)//)
	{
		my $byte_string = $1;
		$byte_string =~ s/ +$//g;

		printf("%s # %s\n",$line,$byte_string);
	}
	else {
		print $line,"\n";
	}
}

exit(0);