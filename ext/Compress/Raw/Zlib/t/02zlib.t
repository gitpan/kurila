BEGIN {
    if (%ENV{PERL_CORE}) {
	chdir 't' if -d 't';
	@INC = ("../lib", "lib/compress");
    }
}

use lib qw(t t/compress);
use strict;
use warnings;
use bytes;

use Test::More  ;
use CompTestUtils;


BEGIN 
{ 
    # use Test::NoWarnings, if available
    my $extra = 0 ;
    $extra = 1
        if eval { require Test::NoWarnings ;  Test::NoWarnings->import(); 1 };


    my $count = 261 ;

    plan tests => $count + $extra;

    use_ok('Compress::Raw::Zlib', 2) ;
}


my $hello = <<EOM ;
hello world
this is a test
EOM

my $len   = length $hello ;

# Check zlib_version and ZLIB_VERSION are the same.
is Compress::Raw::Zlib::zlib_version, ZLIB_VERSION, 
    "ZLIB_VERSION matches Compress::Raw::Zlib::zlib_version" ;

{
    title "Error Cases" ;

    eval { Compress::Raw::Zlib::Deflate->new(-Level) };
    like $@->{description},  mkErr("^Compress::Raw::Zlib::Deflate::new: Expected even number of parameters, got 1") ;

    eval { Compress::Raw::Zlib::Inflate->new(-Level) };
    like $@->{description}, mkErr("^Compress::Raw::Zlib::Inflate::new: Expected even number of parameters, got 1");

    eval { Compress::Raw::Zlib::Deflate->new(-Joe => 1) };
    like $@->{description}, mkErr('^Compress::Raw::Zlib::Deflate::new: unknown key value\(s\) Joe');

    eval { Compress::Raw::Zlib::Inflate->new(-Joe => 1) };
    like $@->{description}, mkErr('^Compress::Raw::Zlib::Inflate::new: unknown key value\(s\) Joe');

    eval { Compress::Raw::Zlib::Deflate->new(-Bufsize => 0) };
    like $@->{description}, mkErr("^Compress::Raw::Zlib::Deflate::new: Bufsize must be >= 1, you specified 0");

    eval { Compress::Raw::Zlib::Inflate->new(-Bufsize => 0) };
    like $@->{description}, mkErr("^Compress::Raw::Zlib::Inflate::new: Bufsize must be >= 1, you specified 0");

    eval { Compress::Raw::Zlib::Deflate->new(-Bufsize => -1) };
    like $@->{description}, mkErr("^Compress::Raw::Zlib::Deflate::new: Parameter 'Bufsize' must be an unsigned int, got '-1'");

    eval { Compress::Raw::Zlib::Inflate->new(-Bufsize => -1) };
    like $@->{description}, mkErr("^Compress::Raw::Zlib::Inflate::new: Parameter 'Bufsize' must be an unsigned int, got '-1'");

    eval { Compress::Raw::Zlib::Deflate->new(-Bufsize => "xxx") };
    like $@->{description}, mkErr("^Compress::Raw::Zlib::Deflate::new: Parameter 'Bufsize' must be an unsigned int, got 'xxx'");

    eval { Compress::Raw::Zlib::Inflate->new(-Bufsize => "xxx") };
    like $@->{description}, mkErr("^Compress::Raw::Zlib::Inflate::new: Parameter 'Bufsize' must be an unsigned int, got 'xxx'");

    eval { Compress::Raw::Zlib::Inflate->new(-Bufsize => 1, 2) };
    like $@->{description}, mkErr("^Compress::Raw::Zlib::Inflate::new: Expected even number of parameters, got 3");

    eval { Compress::Raw::Zlib::Deflate->new(-Bufsize => 1, 2) };
    like $@->{description}, mkErr("^Compress::Raw::Zlib::Deflate::new: Expected even number of parameters, got 3");

}

{

    title  "deflate/inflate - small buffer";
    # ==============================

    my $hello = "I am a HAL 9000 computer" ;
    my @hello = split('', $hello) ;
    my ($err, $x, $X, $status); 
 
    ok( ($x, $err) = Compress::Raw::Zlib::Deflate->new( -Bufsize => 1), "Create deflate object" );
    ok $x, "Compress::Raw::Zlib::Deflate ok" ;
    cmp_ok $err, '==', Z_OK, "status is Z_OK" ;
 
    ok ! defined $x->msg() ;
    is $x->total_in(), 0, "total_in() == 0" ;
    is $x->total_out(), 0, "total_out() == 0" ;

    $X = "" ;
    my $Answer = '';
    foreach (@hello)
    {
        $status = $x->deflate($_, $X) ;
        last unless $status == Z_OK ;
    
        $Answer .= $X ;
    }
     
    cmp_ok $status, '==', Z_OK, "deflate returned Z_OK" ;
    
    cmp_ok  $x->flush($X), '==', Z_OK, "flush returned Z_OK" ;
    $Answer .= $X ;
     
    ok ! defined $x->msg()  ;
    is $x->total_in(), length $hello, "total_in ok" ;
    is $x->total_out(), length $Answer, "total_out ok" ;
     
    my @Answer = split('', $Answer) ;
     
    my $k;
    ok(($k, $err) = Compress::Raw::Zlib::Inflate->new( \%(-Bufsize => 1)) );
    ok $k, "Compress::Raw::Zlib::Inflate ok" ;
    cmp_ok $err, '==', Z_OK, "status is Z_OK" ;
 
    ok ! defined $k->msg(), "No error messages" ;
    is $k->total_in(), 0, "total_in() == 0" ;
    is $k->total_out(), 0, "total_out() == 0" ;
    my $GOT = '';
    my $Z;
    $Z = 1 ;#x 2000 ;
    foreach (@Answer)
    {
        $status = $k->inflate($_, $Z) ;
        $GOT .= $Z ;
        last if $status == Z_STREAM_END or $status != Z_OK ;
     
    }
     
    cmp_ok $status, '==', Z_STREAM_END, "Got Z_STREAM_END" ;
    is $GOT, $hello, "uncompressed data matches ok" ;
    ok ! defined $k->msg(), "No error messages" ;
    is $k->total_in(), length $Answer, "total_in ok" ;
    is $k->total_out(), length $hello , "total_out ok";

}


{
    # deflate/inflate - small buffer with a number
    # ==============================

    my $hello = 6529 ;
 
    ok  my ($x, $err) = Compress::Raw::Zlib::Deflate->new( -Bufsize => 1, -AppendOutput => 1) ;
    ok $x ;
    cmp_ok $err, '==', Z_OK ;
 
    my $status;
    my $Answer = '';
     
    cmp_ok $x->deflate($hello, $Answer), '==', Z_OK ;
    
    cmp_ok $x->flush($Answer), '==', Z_OK ;
     
    my @Answer = split('', $Answer) ;
     
    my $k;
    ok(($k, $err) = Compress::Raw::Zlib::Inflate->new( \%(-Bufsize => 1, -AppendOutput =>1)) );
    ok $k ;
    cmp_ok $err, '==', Z_OK ;
     
    #my $GOT = '';
    my $GOT ;
    foreach (@Answer)
    {
        $status = $k->inflate($_, $GOT) ;
        last if $status == Z_STREAM_END or $status != Z_OK ;
     
    }
     
    cmp_ok $status, '==', Z_STREAM_END ;
    is $GOT, $hello ;

}

{

# deflate/inflate options - AppendOutput
# ================================

    # AppendOutput
    # CRC

    my $hello = "I am a HAL 9000 computer" ;
    my @hello = split('', $hello) ;
     
    ok  my ($x, $err) = Compress::Raw::Zlib::Deflate->new( \%(-Bufsize => 1, -AppendOutput =>1)) ;
    ok $x ;
    cmp_ok $err, '==', Z_OK ;
     
    my $status;
    my $X;
    foreach (@hello)
    {
        $status = $x->deflate($_, $X) ;
        last unless $status == Z_OK ;
    }
     
    cmp_ok $status, '==', Z_OK ;
     
    cmp_ok $x->flush($X), '==', Z_OK ;
     
     
    my @Answer = split('', $X) ;
     
    my $k;
    ok(($k, $err) = Compress::Raw::Zlib::Inflate->new( \%(-Bufsize => 1, -AppendOutput =>1)));
    ok $k ;
    cmp_ok $err, '==', Z_OK ;
     
    my $Z;
    foreach (@Answer)
    {
        $status = $k->inflate($_, $Z) ;
        last if $status == Z_STREAM_END or $status != Z_OK ;
     
    }
     
    cmp_ok $status, '==', Z_STREAM_END ;
    is $Z, $hello ;
}

 
{

    title "deflate/inflate - larger buffer";
    # ==============================

    # generate a long random string
    my $contents = '' ;
    foreach (1 .. 50000)
      { $contents .= chr int rand 255 }
    
    
    ok my ($x, $err) = Compress::Raw::Zlib::Deflate->new() ;
    ok $x ;
    cmp_ok $err, '==', Z_OK ;
     
    my (%X, $Y, %Z, $X, $Z);
    #cmp_ok $x->deflate($contents, $X{key}), '==', Z_OK ;
    cmp_ok $x->deflate($contents, $X), '==', Z_OK ;
    
    #$Y = $X{key} ;
    $Y = $X ;
     
     
    #cmp_ok $x->flush($X{key}), '==', Z_OK ;
    #$Y .= $X{key} ;
    cmp_ok $x->flush($X), '==', Z_OK ;
    $Y .= $X ;
     
     
 
    my $keep = $Y ;

    my $k;
    ok(($k, $err) = Compress::Raw::Zlib::Inflate->new() );
    ok $k ;
    cmp_ok $err, '==', Z_OK ;
     
    #cmp_ok $k->inflate($Y, $Z{key}), '==', Z_STREAM_END ;
    #ok $contents eq $Z{key} ;
    cmp_ok $k->inflate($Y, $Z), '==', Z_STREAM_END ;
    ok $contents eq $Z ;

    # redo deflate with AppendOutput

    ok (($k, $err) = Compress::Raw::Zlib::Inflate->new(-AppendOutput => 1)) ;
    ok $k ;
    cmp_ok $err, '==', Z_OK ;
    
    my $s ; 
    my $out ;
    my @bits = split('', $keep) ;
    foreach my $bit (@bits) {
        $s = $k->inflate($bit, $out) ;
    }
    
    cmp_ok $s, '==', Z_STREAM_END ;
     
    ok $contents eq $out ;


}

{

    title "deflate/inflate - preset dictionary";
    # ===================================

    my $dictionary = "hello" ;
    ok my $x = Compress::Raw::Zlib::Deflate->new((-Level => Z_BEST_COMPRESSION,
			     -Dictionary => $dictionary)) ;
 
    my $dictID = $x->dict_adler() ;

    my ($X, $Y, $Z);
    cmp_ok $x->deflate($hello, $X), '==', Z_OK;
    cmp_ok $x->flush($Y), '==', Z_OK;
    $X .= $Y ;
 
    ok my $k = Compress::Raw::Zlib::Inflate->new(-Dictionary => $dictionary) ;
 
    cmp_ok $k->inflate($X, $Z), '==', Z_STREAM_END;
    is $k->dict_adler(), $dictID;
    is $hello, $Z ;

}

title 'inflate - check remaining buffer after Z_STREAM_END';
#           and that ConsumeInput works.
# ===================================================
 
for my $consume ( 0 .. 1)
{
    ok my $x = Compress::Raw::Zlib::Deflate->new(-Level => Z_BEST_COMPRESSION) ;
 
    my ($X, $Y, $Z);
    cmp_ok $x->deflate($hello, $X), '==', Z_OK;
    cmp_ok $x->flush($Y), '==', Z_OK;
    $X .= $Y ;
 
    ok my $k = Compress::Raw::Zlib::Inflate->new( -ConsumeInput => $consume) ;
 
    my $first = substr($X, 0, 2) ;
    my $remember_first = $first ;
    my $last  = substr($X, 2) ;
    cmp_ok $k->inflate($first, $Z), '==', Z_OK;
    if ($consume) {
        ok $first eq "" ;
    }
    else {
        ok $first eq $remember_first ;
    }

    my $T ;
    $last .= "appendage" ;
    my $remember_last = $last ;
    cmp_ok $k->inflate($last, $T),  '==', Z_STREAM_END;
    is $hello, $Z . $T  ;
    if ($consume) {
        is $last, "appendage" ;
    }
    else {
        is $last, $remember_last ;
    }

}



{

    title 'Check - MAX_WBITS';
    # =================
    
    my $hello = "Test test test test test";
    my @hello = split('', $hello) ;
     
    ok  my ($x, $err) = 
       Compress::Raw::Zlib::Deflate->new( -Bufsize => 1, 
                                     -WindowBits => -MAX_WBITS(),
                                     -AppendOutput => 1) ;
    ok $x ;
    cmp_ok $err, '==', Z_OK ;

    my $Answer = '';
    my $status;
    foreach (@hello)
    {
        $status = $x->deflate($_, $Answer) ;
        last unless $status == Z_OK ;
    }
     
    cmp_ok $status, '==', Z_OK ;
    
    cmp_ok $x->flush($Answer), '==', Z_OK ;
     
    my @Answer = split('', $Answer) ;
    # Undocumented corner -- extra byte needed to get inflate to return 
    # Z_STREAM_END when done.  
    push @Answer, " " ; 
     
    my $k;
    ok(($k, $err) = Compress::Raw::Zlib::Inflate->new( 
			\%(-Bufsize => 1, 
			-AppendOutput =>1,
			-WindowBits => -MAX_WBITS()))) ;
    ok $k ;
    cmp_ok $err, '==', Z_OK ;
     
    my $GOT = '';
    foreach (@Answer)
    {
        $status = $k->inflate($_, $GOT) ;
        last if $status == Z_STREAM_END or $status != Z_OK ;
     
    }
     
    cmp_ok $status, '==', Z_STREAM_END ;
    is $GOT, $hello ;
    
}

{
    title 'inflateSync';

    # create a deflate stream with flush points

    my $hello = "I am a HAL 9000 computer" x 2001 ;
    my $goodbye = "Will I dream?" x 2010;
    my ($x, $err, $answer, $X, $Z, $status);
    my $Answer ;
     
    #use Devel::Peek ;
    ok(($x, $err) = Compress::Raw::Zlib::Deflate->new(AppendOutput => 1)) ;
    ok $x ;
    cmp_ok $err, '==', Z_OK ;
     
    cmp_ok $x->deflate($hello, $Answer), '==', Z_OK;
    
    # create a flush point
    cmp_ok $x->flush($Answer, Z_FULL_FLUSH), '==', Z_OK ;
     
    cmp_ok $x->deflate($goodbye, $Answer), '==', Z_OK;
    
    cmp_ok $x->flush($Answer), '==', Z_OK ;
     
    my ($first, @Answer) = split('', $Answer) ;
     
    my $k;
    ok(($k, $err) = Compress::Raw::Zlib::Inflate->new()) ;
    ok $k ;
    cmp_ok $err, '==', Z_OK ;
     
    cmp_ok  $k->inflate($first, $Z), '==', Z_OK;

    # skip to the first flush point.
    while (@Answer)
    {
        my $byte = shift @Answer;
        $status = $k->inflateSync($byte) ;
        last unless $status == Z_DATA_ERROR;
    }

    cmp_ok $status, '==', Z_OK;
     
    my $GOT = '';
    foreach (@Answer)
    {
        my $Z = '';
        $status = $k->inflate($_, $Z) ;
        $GOT .= $Z if defined $Z ;
        # print "x $status\n";
        last if $status == Z_STREAM_END or $status != Z_OK ;
     
    }
     
    cmp_ok $status, '==', Z_DATA_ERROR ;
    is $GOT, $goodbye ;


    # Check inflateSync leaves good data in buffer
    my $rest = $Answer ;
    $rest =~ s/^(.)//;
    my $initial = $1 ;

    
    ok(($k, $err) = Compress::Raw::Zlib::Inflate->new(-ConsumeInput => 0)) ;
    ok $k ;
    cmp_ok $err, '==', Z_OK ;
     
    cmp_ok $k->inflate($initial, $Z), '==', Z_OK;

    # Skip to the flush point
    $status = $k->inflateSync($rest);
    cmp_ok $status, '==', Z_OK
     or diag "status '$status'\nlength rest is " . length($rest) . "\n" ;
     
    cmp_ok $k->inflate($rest, $GOT), '==', Z_DATA_ERROR;
    is $Z . $GOT, $goodbye ;
}

{
    title 'deflateParams';

    my $hello = "I am a HAL 9000 computer" x 2001 ;
    my $goodbye = "Will I dream?" x 2010;
    my ($x, $input, $err, $answer, $X, $status, $Answer);
     
    ok(($x, $err) = Compress::Raw::Zlib::Deflate->new(
                       -AppendOutput   => 1,
                       -Level    => Z_DEFAULT_COMPRESSION,
                       -Strategy => Z_DEFAULT_STRATEGY)) ;
    ok $x ;
    cmp_ok $err, '==', Z_OK ;

    ok $x->get_Level()    == Z_DEFAULT_COMPRESSION;
    ok $x->get_Strategy() == Z_DEFAULT_STRATEGY;
     
    $status = $x->deflate($hello, $Answer) ;
    cmp_ok $status, '==', Z_OK ;
    $input .= $hello;
    
    # error cases
    eval { $x->deflateParams() };
    like $@->{description}, mkErr('^Compress::Raw::Zlib::deflateParams needs Level and\/or Strategy');

    eval { $x->deflateParams(-Bufsize => 0) };
    like $@->{description}, mkErr('^Compress::Raw::Zlib::Inflate::deflateParams: Bufsize must be >= 1, you specified 0');

    eval { $x->deflateParams(-Joe => 3) };
    like $@->{description}, mkErr('^Compress::Raw::Zlib::deflateStream::deflateParams: unknown key value\(s\) Joe');

    is $x->get_Level(),    Z_DEFAULT_COMPRESSION;
    is $x->get_Strategy(), Z_DEFAULT_STRATEGY;
     
    # change both Level & Strategy
    $status = $x->deflateParams(-Level => Z_BEST_SPEED, -Strategy => Z_HUFFMAN_ONLY, -Bufsize => 1234) ;
    cmp_ok $status, '==', Z_OK ;
    
    is $x->get_Level(),    Z_BEST_SPEED;
    is $x->get_Strategy(), Z_HUFFMAN_ONLY;
     
    $status = $x->deflate($goodbye, $Answer) ;
    cmp_ok $status, '==', Z_OK ;
    $input .= $goodbye;
    
    # change only Level 
    $status = $x->deflateParams(-Level => Z_NO_COMPRESSION) ;
    cmp_ok $status, '==', Z_OK ;
    
    is $x->get_Level(),    Z_NO_COMPRESSION;
    is $x->get_Strategy(), Z_HUFFMAN_ONLY;
     
    $status = $x->deflate($goodbye, $Answer) ;
    cmp_ok $status, '==', Z_OK ;
    $input .= $goodbye;
    
    # change only Strategy
    $status = $x->deflateParams(-Strategy => Z_FILTERED) ;
    cmp_ok $status, '==', Z_OK ;
    
    is $x->get_Level(),    Z_NO_COMPRESSION;
    is $x->get_Strategy(), Z_FILTERED;
     
    $status = $x->deflate($goodbye, $Answer) ;
    cmp_ok $status, '==', Z_OK ;
    $input .= $goodbye;
    
    cmp_ok $x->flush($Answer), '==', Z_OK ;
     
    my $k;
    ok(($k, $err) = Compress::Raw::Zlib::Inflate->new()) ;
    ok $k ;
    cmp_ok $err, '==', Z_OK ;
     
    my $Z;
    $status = $k->inflate($Answer, $Z) ;

    cmp_ok $status, '==', Z_STREAM_END ;
    is $Z, $input ;
}


{
    title "ConsumeInput and a read-only buffer trapped" ;

    ok my $k = Compress::Raw::Zlib::Inflate->new(-ConsumeInput => 1) ;
     
    my $Z; 
    eval { $k->inflate("abc", $Z) ; };
    like $@->{description}, mkErr("Modification of a read-only value attempted");

}

foreach (1 .. 2)
{
    title 'test inflate/deflate with a substr';

    my $contents = '' ;
    foreach (1 .. 5000)
      { $contents .= chr int rand 255 }
    ok  my $x = Compress::Raw::Zlib::Deflate->new(-AppendOutput => 1) ;
     
    my $X ;
    my $status = $x->deflate(substr($contents,0), $X);
    cmp_ok $status, '==', Z_OK ;
    
    cmp_ok $x->flush($X), '==', Z_OK  ;
     
    my $append = "Appended" ;
    $X .= $append ;
     
    ok my $k = Compress::Raw::Zlib::Inflate->new(-AppendOutput => 1) ;
     
    my $Z; 
    my $keep = $X ;
    $status = $k->inflate($X, $Z) ;
     
    cmp_ok $status, '==', Z_STREAM_END ;
    #print "status $status X [$X]\n" ;
    is $contents, $Z ;
    ok $X eq $append;
    #is length($X), length($append);
    #ok $X eq $keep;
    #is length($X), length($keep);
}

title 'Looping Append test - checks that deRef_l resets the output buffer';
foreach (1 .. 2)
{

    my $hello = "I am a HAL 9000 computer" ;
    my @hello = split('', $hello) ;
    my ($err, $x, $X, $status); 
 
    ok( ($x, $err) = Compress::Raw::Zlib::Deflate->new( -Bufsize => 1) );
    ok $x ;
    cmp_ok $err, '==', Z_OK ;
 
    $X = "" ;
    my $Answer = '';
    foreach (@hello)
    {
        $status = $x->deflate($_, $X) ;
        last unless $status == Z_OK ;
    
        $Answer .= $X ;
    }
     
    cmp_ok $status, '==', Z_OK ;
    
    cmp_ok  $x->flush($X), '==', Z_OK ;
    $Answer .= $X ;
     
    my @Answer = split('', $Answer) ;
     
    my $k;
    ok(($k, $err) = Compress::Raw::Zlib::Inflate->new(-AppendOutput => 1) );
    ok $k ;
    cmp_ok $err, '==', Z_OK ;
 
    my $GOT ;
    my $Z;
    $Z = 1 ;#x 2000 ;
    foreach (@Answer)
    {
        $status = $k->inflate($_, $GOT) ;
        last if $status == Z_STREAM_END or $status != Z_OK ;
    }
     
    cmp_ok $status, '==', Z_STREAM_END ;
    is $GOT, $hello ;

}

{
    title 'test inflate input parameter via substr';

    my $hello = "I am a HAL 9000 computer" ;
    my $data = $hello ;

    my($X, $Z);

    ok my $x = Compress::Raw::Zlib::Deflate->new( -AppendOutput => 1);

    cmp_ok $x->deflate($data, $X), '==',  Z_OK ;

    cmp_ok $x->flush($X), '==', Z_OK ;
     
    my $append = "Appended" ;
    $X .= $append ;
    my $keep = $X ;
     
    ok my $k = Compress::Raw::Zlib::Inflate->new( -AppendOutput => 1,
                                             -ConsumeInput => 1) ;
     
    cmp_ok $k->inflate($X, $Z), '==', Z_STREAM_END ; ;
     
    ok $hello eq $Z ;
    is $X, $append;
    
    $X = $keep ;
    $Z = '';
    ok $k = Compress::Raw::Zlib::Inflate->new( -AppendOutput => 1,
                                          -ConsumeInput => 0) ;
     
    cmp_ok $k->inflate($X, $Z), '==', Z_STREAM_END ; ;
    #cmp_ok $k->inflate(substr($X, 0), $Z), '==', Z_STREAM_END ; ;
     
    ok $hello eq $Z ;
    is $X, $keep;
    
}

{
    # regression - check that resetLastBlockByte can cope with a NULL
    # pointer.
    Compress::Raw::Zlib::InflateScan->new->resetLastBlockByte(undef);
    ok 1, "resetLastBlockByte(undef) is ok" ;
}

title 'Looping Append test with substr output - substr the end of the string';
foreach (1 .. 2)
{

    my $hello = "I am a HAL 9000 computer" ;
    my @hello = split('', $hello) ;
    my ($err, $x, $X, $status); 
 
    ok( ($x, $err) = Compress::Raw::Zlib::Deflate->new( -Bufsize => 1,
                                            -AppendOutput => 1) );
    ok $x ;
    cmp_ok $err, '==', Z_OK ;
 
    $X = "" ;
    my $Answer = '';
    foreach (@hello)
    {
        $status = $x->deflate($_, $Answer);
        last unless $status == Z_OK ;
    
    }
     
    cmp_ok $status, '==', Z_OK ;
    
    cmp_ok  $x->flush($Answer), '==', Z_OK ;
     
    cmp_ok length $Answer, "+>", 0 ;

    my @Answer = split('', $Answer) ;
    
     
    my $k;
    ok(($k, $err) = Compress::Raw::Zlib::Inflate->new(-AppendOutput => 1) );
    ok $k ;
    cmp_ok $err, '==', Z_OK ;
 
    my $GOT = '';
    my $Z;
    $Z = 1 ;#x 2000 ;
    foreach (@Answer)
    {
        my $buf;
        $status = $k->inflate($_, $buf);
        $GOT .= $buf;
        last if $status == Z_STREAM_END or $status != Z_OK ;
    }
     
    cmp_ok $status, '==', Z_STREAM_END ;
    is $GOT, $hello ;

}

title 'Looping Append test with substr output - substr the complete string';
foreach (1 .. 2)
{

    my $hello = "I am a HAL 9000 computer" ;
    my @hello = split('', $hello) ;
    my ($err, $x, $X, $status); 
 
    ok( ($x, $err) = Compress::Raw::Zlib::Deflate->new( -Bufsize => 1,
                                            -AppendOutput => 1) );
    ok $x ;
    cmp_ok $err, '==', Z_OK ;
 
    $X = "" ;
    my $Answer = '';
    foreach (@hello)
    {
        $status = $x->deflate($_, $Answer) ;
        last unless $status == Z_OK ;
    
    }
     
    cmp_ok $status, '==', Z_OK ;
    
    cmp_ok  $x->flush($Answer), '==', Z_OK ;
     
    my @Answer = split('', $Answer) ;
     
    my $k;
    ok(($k, $err) = Compress::Raw::Zlib::Inflate->new(-AppendOutput => 1) );
    ok $k ;
    cmp_ok $err, '==', Z_OK ;
 
    my $GOT = '';
    my $Z;
    $Z = 1 ;#x 2000 ;
    foreach (@Answer)
    {
        $status = $k->inflate($_, $GOT) ;
        last if $status == Z_STREAM_END or $status != Z_OK ;
    }
     
    cmp_ok $status, '==', Z_STREAM_END ;
    is $GOT, $hello ;
}

