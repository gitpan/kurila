BEGIN {
    if (env::var('PERL_CORE')) {
	push $^INCLUDE_PATH, "lib/compress";
    }
}

use lib < qw(t t/compress);

use warnings;
use bytes;

use Test::More ;
use CompTestUtils;

BEGIN 
{ 
    # use Test::NoWarnings, if available
    my $extra = 0 ;
    $extra = 1
        if try { require Test::NoWarnings ;  Test::NoWarnings->import(); 1 };

    plan tests => 288 + $extra ;

    use_ok('Compress::Raw::Zlib', 2) ; 
}


my $hello = <<EOM ;
hello world
this is a test
EOM

my $len   = length $hello ;

# Check zlib_version and ZLIB_VERSION are the same.
is Compress::Raw::Zlib::zlib_version, ZLIB_VERSION ;


for my $i (1 .. 13)
{

    print $^STDOUT, "#\n#Length $i\n#\n";

    my $hello = "I am a HAL 9000 computer" x 2001;
    my $tmp = $hello ;
    
    my @hello = @();
    push @hello, $1 
	while $tmp =~ s/^(.\{$i\})//;
    push @hello, $tmp if length $tmp ;

    my ($err, $x, $X, $status); 
 
    ok( @($x, $err) =  Compress::Raw::Zlib::Deflate->new(AppendOutput => 1));
    ok $x ;
    cmp_ok $err, '==', Z_OK, "  status is Z_OK" ;
 
    ok ! defined $x->msg(), "  no msg" ;
    is $x->total_in(), 0, "  total_in == 0" ;
    is $x->total_out(), 0, "  total_out == 0" ;

    my $out ;
    foreach (@hello)
    {
        $status = $x->deflate($_, $out) ;
        last unless $status == Z_OK ;
     
    }
    cmp_ok $status, '==', Z_OK, "  status is Z_OK" ;
    
    cmp_ok $x->flush($out), '==', Z_OK, "  flush returned Z_OK" ;
     
    ok ! defined $x->msg(), "  no msg"  ;
    is $x->total_in(), length $hello, "  length total_in" ;
    is $x->total_out(), length $out, "  length total_out" ;
     
    my @Answer = @();
    $tmp = $out;
    push @Answer, $1 while $tmp =~ s/^(.\{$i\})//;
    push @Answer, $tmp if length $tmp ;
     
    my $k;
    ok(@($k, $err) =  Compress::Raw::Zlib::Inflate->new( AppendOutput => 1));
    ok $k ;
    cmp_ok $err, '==', Z_OK, "  status is Z_OK" ;
 
    ok ! defined $k->msg(), "  no msg" ;
    is $k->total_in(), 0, "  total_in == 0" ;
    is $k->total_out(), 0, "  total_out == 0" ;
    my $GOT = '';
    my $Z;
    $Z = 1 ;#x 2000 ;
    foreach (@Answer)
    {
        $status = $k->inflate($_, $GOT) ;
        last if $status == Z_STREAM_END or $status != Z_OK ;
     
    }
     
    cmp_ok $status, '==', Z_STREAM_END, "  status is Z_STREAM_END" ;
    is $GOT, $hello, "  got expected output" ;
    ok ! defined $k->msg(), "  no msg" ;
    is $k->total_in(), length $out, "  length total_in ok" ;
    is $k->total_out(), length $hello, "  length total_out ok" ;

}
