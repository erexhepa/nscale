#!/usr/bin/perl
# ONLY NEEDED DURING A TRANSITION DURING V2 DEVELOPMENT
use strict;
use warnings;
use File::Copy;

my(@filenames) = </home/tcpan/PhD/path/Data/adios/*.summary.v2.walltimes.csv>;
print "filenames:";
print "$_," foreach (@filenames);
print "\n";

my($fn2);
my($ofn);
my(%walltimes);

foreach my $filename (@filenames) {
	$fn2 = $filename;
	$fn2 =~ s/\.walltimes\./\./;	
	print "input file: $fn2 \n";

	# back up
	$ofn = $fn2 . ".bak";
	print "output file: $ofn \n";
	copy("$fn2", "$ofn") or die("ERROR: cannot make backup for $fn2\n");
	
	%walltimes = ();
	
	# read the new wall times
	open(FH, $filename) or die("ERROR: cannot open walltime filename $filename\n");
	my(@newlines) = <FH>;
	close(FH);
	# parse the update lines into a hash table
	foreach my $newline (@newlines) {
		$newline =~ s/^\s+|\s+$//g;
		my($fn) = $newline =~ m/^EXPERIMENT,([^,]+),.*$/;
		$fn =~ s/^\s+|\s+$//g;
		$walltimes{$fn} = $newline;
	} 
#	print "$_, $walltimes{$_}\n" foreach (keys %walltimes);
	
	# read the input file
	open(FH2, $fn2) or die("ERROR: cannot open summary file $fn2\n");
	my(@lines) = <FH2>;
	close(FH2);
	
	# open the same input file for write
	open(FH2, ">", "$fn2") or die("ERROR: cannot update file $fn2\n");
	
	# replace the appropriate lines
	foreach my $line (@lines) {
		$line =~ s/^\s+|\s+$//g;
		if ($line =~ m/^EXPERIMENT,/) {
			my($key) = $line =~ m/^EXPERIMENT,([^,]+),.*$/;
			$key =~ s/^\s+|\s+$//g;
			print "$key => $walltimes{$key}\n";
						
			print FH2 "$walltimes{$key}\n";
		} else {
			print FH2 "$line\n";
		}
	}
	close(FH2);
	
}
