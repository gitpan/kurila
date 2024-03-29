BEGIN {
   use File::Basename;
   my $THISDIR = dirname $^PROGRAM_NAME;
   unshift $^INCLUDE_PATH, $THISDIR;
   require "testp2pt.pl";
   TestPodIncPlainText->import();
}

my %options = %( < @+: map { @: $_ => 1 }, @ARGV );  ## convert cmdline to options-hash
my $passed  = testpodplaintext \%options, $^PROGRAM_NAME;
exit( ($passed == 1) ?? 0 !! -1 )  unless env::var('HARNESS_ACTIVE');


__END__


=pod

This file tries to demonstrate a simple =include directive
for pods. It is used as follows:

   =include filename

where "filename" is expected to be an absolute pathname, or else
reside be relative to the directory in which the current processed
podfile resides, or be relative to the current directory.

Lets try it out with the file "included.t" shall we.

***THIS TEXT IS IMMEDIATELY BEFORE THE INCLUDE***

=include included.t

***THIS TEXT IS IMMEDIATELY AFTER THE INCLUDE***

So how did we do???
