BEGIN {
    if (env::var('PERL_CORE')) {
	chdir 't' if -d 't';
	$^INCLUDE_PATH = @("../lib", "lib/compress");
    }
}

use lib < qw(t t/compress);

use warnings;
use bytes;

use IO::Compress::Base::Common;

use Test::More ; 
use CompTestUtils;

BEGIN {
    # use Test::NoWarnings, if available
    my $extra = 0 ;
    $extra = 1
        if try { require Test::NoWarnings ;  Test::NoWarnings->import(); 1 };

    plan tests => 77 + $extra ;

    use_ok('Scalar::Util');
}


ok gotScalarUtilXS(), "Got XS Version of Scalar::Util"
    or diag <<EOM;
You don't have the XS version of Scalar::Util
EOM

# Compress::Zlib::Common;

sub My::testParseParameters()
{
    try { ParseParameters(1, \%(), 1) ; };
    like $^EVAL_ERROR->{?description}, mkErr(': Expected even number of parameters, got 1'), 
            "Trap odd number of params";

    try { ParseParameters(1, \%(), undef) ; };
    like $^EVAL_ERROR->{?description}, mkErr(': Expected even number of parameters, got 1'), 
            "Trap odd number of params";

    try { ParseParameters(1, \%(), \@()) ; };
    like $^EVAL_ERROR->{?description}, mkErr(': Expected even number of parameters, got 1'), 
            "Trap odd number of params";

    try { ParseParameters(1, \%('Fred' => \@(1, 1, Parse_boolean, 0)), Fred => 'joe') ; };
    like $^EVAL_ERROR->{?description}, mkErr("Parameter 'Fred' must be an int, got 'joe'"), 
            "wanted unsigned, got undef";

    try { ParseParameters(1, \%('Fred' => \@(1, 1, Parse_unsigned, 0)), Fred => undef) ; };
    like $^EVAL_ERROR->{?description}, mkErr("Parameter 'Fred' must be an unsigned int, got 'undef'"), 
            "wanted unsigned, got undef";

    try { ParseParameters(1, \%('Fred' => \@(1, 1, Parse_signed, 0)), Fred => undef) ; };
    like $^EVAL_ERROR->{?description}, mkErr("Parameter 'Fred' must be a signed int, got 'undef'"), 
            "wanted signed, got undef";

    try { ParseParameters(1, \%('Fred' => \@(1, 1, Parse_signed, 0)), Fred => 'abc') ; };
    like $^EVAL_ERROR->{?description}, mkErr("Parameter 'Fred' must be a signed int, got 'abc'"), 
            "wanted signed, got 'abc'";


  SKIP:
    do {
        use Config;

        try { ParseParameters(1, \%('Fred' => \@(1, 1, Parse_writable_scalar, 0)), Fred => 'abc') ; };
        like $^EVAL_ERROR->{?description}, mkErr("Parameter 'Fred' not writable"), 
                "wanted writable, got readonly";
    };

    my @xx;
    try { ParseParameters(1, \%('Fred' => \@(1, 1, Parse_writable_scalar, 0)), Fred => \@xx) ; };
    like $^EVAL_ERROR->{?description}, mkErr("Parameter 'Fred' not a scalar reference"), 
            "wanted scalar reference";

    local *ABC;
    try { ParseParameters(1, \%('Fred' => \@(1, 1, Parse_writable_scalar, 0)), Fred => \*ABC) ; };
    like $^EVAL_ERROR->{?description}, mkErr("Parameter 'Fred' not a scalar"), 
            "wanted scalar";

    #try { ParseParameters(1, \%('Fred' => \@(1, 1, Parse_any|Parse_multiple, 0)), Fred => 1, Fred => 2) ; };
    #like $@, mkErr("Muliple instances of 'Fred' found"),
        #"wanted scalar";

    ok 1;

    my $got = ParseParameters(1, \%('Fred' => \@(1, 1, 0x1000000, 0)), Fred => 'abc') ;
    is $got->value('Fred'), "abc", "other" ;

    $got = ParseParameters(1, \%('Fred' => \@(0, 1, Parse_any, undef)), Fred =>
undef) ;
    ok $got->parsed('Fred'), "undef" ;
    ok ! defined $got->value('Fred'), "undef" ;

    $got = ParseParameters(1, \%('Fred' => \@(0, 1, Parse_string, undef)), Fred =>
undef) ;
    ok $got->parsed('Fred'), "undef" ;
    is $got->value('Fred'), "", "empty string" ;

    my $xx;
    $got = ParseParameters(1, \%('Fred' => \@(1, 1, Parse_writable_scalar, undef)), Fred => $xx) ;

    ok $got->parsed('Fred'), "parsed" ;
    my $xx_ref = $got->value('Fred');
    $$xx_ref = 77 ;
    is $xx, 77;

    $got = ParseParameters(1, \%('Fred' => \@(1, 1, Parse_writable_scalar, undef)), Fred => \$xx) ;

    ok $got->parsed('Fred'), "parsed" ;
    $xx_ref = $got->value('Fred');
    $$xx_ref = 666 ;
    is $xx, 666;

}

My::testParseParameters();


do {
    title "isaFilename" ;
    ok   isaFilename("abc"), "'abc' isaFilename";

    ok ! isaFilename(undef), "undef ! isaFilename";
    ok ! isaFilename(\@()),    "[] ! isaFilename";
    $main::X = 1; $main::X = $main::X ;
    ok ! isaFilename(*X),    "glob ! isaFilename";
};

do {
    title "whatIsInput" ;

    my $lex = LexFile->new( my $out_file) ;
    open my $fh, ">", "$out_file" ;
    is whatIsInput(*$fh), 'handle', "Match filehandle" ;
    close $fh ;

    my $stdin = '-';
    is whatIsInput($stdin),       'handle',   "Match '-' as stdin";
    #is $stdin,                    \*STDIN,    "'-' changed to *STDIN";
    #isa_ok $stdin,                'IO::File',    "'-' changed to IO::File";
    is whatIsInput("abc"),        'filename', "Match filename";
    is whatIsInput(\"abc"),       'buffer',   "Match buffer";
    is whatIsInput(sub { 1 }, 1), 'code',     "Match code";
    is whatIsInput(sub { 1 }),    ''   ,      "Don't match code";

};

do {
    title "whatIsOutput" ;

    my $lex = LexFile->new( my $out_file) ;
    open my $fh, ">", "$out_file" ;
    is whatIsOutput(*$fh), 'handle', "Match filehandle" ;
    close $fh ;

    my $stdout = '-';
    is whatIsOutput($stdout),     'handle',   "Match '-' as stdout";
    #is $stdout,                   \*STDOUT,   "'-' changed to *STDOUT";
    #isa_ok $stdout,               'IO::File',    "'-' changed to IO::File";
    is whatIsOutput("abc"),        'filename', "Match filename";
    is whatIsOutput(\"abc"),       'buffer',   "Match buffer";
    is whatIsOutput(sub { 1 }, 1), 'code',     "Match code";
    is whatIsOutput(sub { 1 }),    ''   ,      "Don't match code";

};

# U64

do {
    title "U64" ;

    my $x = U64->new();
    is $x->getHigh, 0, "  getHigh is 0";
    is $x->getLow, 0, "  getLow is 0";

    $x = U64->new(1,2);
    $x = U64->new(1,2);
    is $x->getHigh, 1, "  getHigh is 1";
    is $x->getLow, 2, "  getLow is 2";

    $x = U64->new(0xFFFFFFFF,2);
    is $x->getHigh, 0xFFFFFFFF, "  getHigh is 0xFFFFFFFF";
    is $x->getLow, 2, "  getLow is 2";

    $x = U64->new(7, 0xFFFFFFFF);
    is $x->getHigh, 7, "  getHigh is 7";
    is $x->getLow, 0xFFFFFFFF, "  getLow is 0xFFFFFFFF";

    $x = U64->new(666);
    is $x->getHigh, 0, "  getHigh is 0";
    is $x->getLow, 666, "  getLow is 666";

    title "U64 - add" ;

    $x = U64->new(0, 1);
    is $x->getHigh, 0, "  getHigh is 0";
    is $x->getLow, 1, "  getLow is 1";

    $x->add(1);
    is $x->getHigh, 0, "  getHigh is 0";
    is $x->getLow, 2, "  getLow is 2";

    $x = U64->new(0, 0xFFFFFFFE);
    is $x->getHigh, 0, "  getHigh is 0";
    is $x->getLow, 0xFFFFFFFE, "  getLow is 0xFFFFFFFE";

    $x->add(1);
    is $x->getHigh, 0, "  getHigh is 0";
    is $x->getLow, 0xFFFFFFFF, "  getLow is 0xFFFFFFFF";

    $x->add(1);
    is $x->getHigh, 1, "  getHigh is 1";
    is $x->getLow, 0, "  getLow is 0";

    $x->add(1);
    is $x->getHigh, 1, "  getHigh is 1";
    is $x->getLow, 1, "  getLow is 1";

    $x = U64->new(1, 0xFFFFFFFE);
    my $y = U64->new(2, 3);

    $x->add($y);
    is $x->getHigh, 4, "  getHigh is 4";
    is $x->getLow, 1, "  getLow is 1";

    title "U64 - equal" ;

    $x = U64->new(0, 1);
    is $x->getHigh, 0, "  getHigh is 0";
    is $x->getLow, 1, "  getLow is 1";

    $y = U64->new(0, 1);
    is $x->getHigh, 0, "  getHigh is 0";
    is $x->getLow, 1, "  getLow is 1";

    my $z = U64->new(0, 2);
    is $x->getHigh, 0, "  getHigh is 0";
    is $x->getLow, 1, "  getLow is 1";

    ok $x->equal($y), "  equal";
    ok !$x->equal($z), "  ! equal";

    title "U64 - pack_V" ;
};
