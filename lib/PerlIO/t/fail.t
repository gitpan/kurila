#!./perl

BEGIN {
    require "../t/test.pl";
    skip_all("No perlio") unless (PerlIO::Layer->find( 'perlio'));
    plan (15);
}

use warnings 'layer';
my $warn;
my $file = "fail$$";
$^WARN_HOOK = sub { $warn = shift->{description} };

END { 1 while unlink($file) }

ok(open(FH,">",$file),"Create works");
close(FH);
ok(open(FH,"<",$file),"Normal open works");

$warn = ''; $! = 0;
ok(!binmode(FH,":-)"),"All punctuation fails binmode");
print "# $!\n";
isnt($!,0,"Got errno");
like($warn,qr/in PerlIO layer/,"Got warning");

$warn = ''; $! = 0;
ok(!binmode(FH,":nonesuch"),"Bad package fails binmode");
print "# $!\n";
isnt($!,0,"Got errno");
like($warn,qr/nonesuch/,"Got warning");
close(FH);

$warn = ''; $! = 0;
ok(!open(FH,"<:-)",$file),"All punctuation fails open");
print "# $!\n";
isnt($!,"","Got errno");
like($warn,qr/in PerlIO layer/,"Got warning");

$warn = ''; $! = 0;
ok(!open(FH,"<:nonesuch",$file),"Bad package fails open");
print "# $!\n";
isnt($!,0,"Got errno");
like($warn,qr/nonesuch/,"Got warning");

ok(open(FH,"<",$file),"Normal open (still) works");
close(FH);