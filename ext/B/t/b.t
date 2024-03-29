#!./perl

$^OUTPUT_AUTOFLUSH  = 1;
use warnings;

use Test::More tests => 55;

BEGIN { use_ok( 'B' ); }


package Testing::Symtable;
our ($This, @That, %wibble, $moo, %moo);
my $not_a_sym = 'moo';

sub moo { 42 }
sub car { 23 }


package Testing::Symtable::Foo;
sub yarrow { "Hock" }

package Testing::Symtable::Bar;
sub hock { "yarrow" }

package main;
our (%Subs);
local %Subs = %( () );
B::walksymtable(\%Testing::Symtable::, 'find_syms', sub { @_[0] =~ m/Foo/ },
                'Testing::Symtable::');

sub B::GV::find_syms($symbol) {

    %main::Subs{+$symbol->STASH->NAME . '::' . $symbol->NAME}++;
}

my @syms = map { 'Testing::Symtable::'.$_ }, qw(This That wibble moo car);
push @syms, "Testing::Symtable::Foo::yarrow";

# Make sure we hit all the expected symbols.
do {
    is( join('#', sort keys %Subs), join('#', sort @syms), 'all symbols found' );
};

# Make sure we only hit them each once.
ok( (!grep { $_ != 1 }, values %Subs), '...and found once' );

my $r = qr/foo/;
my $obj = B::svref_2object($r);
my $regexp =  $obj;
ok($regexp->precomp() eq 'foo', 'Get string from qr//');
like($regexp->REGEX(), qr/\d+/, "REGEX() returns numeric value");
my $iv = 1;
my $iv_ref = B::svref_2object(\$iv);
is(ref $iv_ref, "B::IV", "Test B:IV return from svref_2object");
is($iv_ref->REFCNT, 1, "Test B::IV->REFCNT");
# Flag tests are needed still
#diag $iv_ref->FLAGS();
my $iv_ret = $iv_ref->object_2svref();
is(ref $iv_ret, "SCALAR", "Test object_2svref() return is SCALAR");
is($$iv_ret, $iv, "Test object_2svref()");
is($iv_ref->int_value, $iv, "Test int_value()");
is($iv_ref->IV, $iv, "Test IV()");
is($iv_ref->IVX(), $iv, "Test IVX()");
is($iv_ref->UVX(), $iv, "Test UVX()");

my $pv = "Foo";
my $pv_ref = B::svref_2object(\$pv);
is(ref $pv_ref, "B::PV", "Test B::PV return from svref_2object");
is($pv_ref->REFCNT, 1, "Test B::PV->REFCNT");
# Flag tests are needed still
#diag $pv_ref->FLAGS();
my $pv_ret = $pv_ref->object_2svref();
is(ref $pv_ret, "SCALAR", "Test object_2svref() return is SCALAR");
is($$pv_ret, $pv, "Test object_2svref()");
is($pv_ref->PV(), $pv, "Test PV()");
try { is($pv_ref->RV(), $pv, "Test RV()"); };
ok($^EVAL_ERROR, "Test RV()");
is($pv_ref->PVX_const(), $pv, "Test PVX()");

my $nv = 1.1;
my $nv_ref = B::svref_2object(\$nv);
is(ref $nv_ref, "B::NV", "Test B::NV return from svref_2object");
is($nv_ref->REFCNT, 1, "Test B::NV->REFCNT");
# Flag tests are needed still
#diag $nv_ref->FLAGS();
my $nv_ret = $nv_ref->object_2svref();
is(ref $nv_ret, "SCALAR", "Test object_2svref() return is SCALAR");
is($$nv_ret, $nv, "Test object_2svref()");
is($nv_ref->NV, $nv, "Test NV()");
is($nv_ref->NVX(), $nv, "Test NVX()");

my $null = undef;
my $null_ref = B::svref_2object(\$null);
is(ref $null_ref, "B::NULL", "Test B::NULL return from svref_2object");
is($null_ref->REFCNT, 1, "Test B::NULL->REFCNT");
# Flag tests are needed still
#diag $null_ref->FLAGS();
my $null_ret = $nv_ref->object_2svref();
is(ref $null_ret, "SCALAR", "Test object_2svref() return is SCALAR");
is($$null_ret, $nv, "Test object_2svref()");

my $RV_class = 'B::IV';
my $cv = sub{ 1; };
my $cv_ref = B::svref_2object(\$cv);
is($cv_ref->REFCNT, 1, "Test $RV_class->REFCNT");
is(ref $cv_ref, "$RV_class",
   "Test $RV_class return from svref_2object - code");
my $cv_ret = $cv_ref->object_2svref();
is(ref $cv_ret, "REF", "Test object_2svref() return is REF");
is($$cv_ret, $cv, "Test object_2svref()");

my $av = \@();
my $av_ref = B::svref_2object(\$av);
is(ref $av_ref, "$RV_class",
   "Test $RV_class return from svref_2object - array");

my $hv = \@();
my $hv_ref = B::svref_2object(\$hv);
is(ref $hv_ref, "$RV_class",
   "Test $RV_class return from svref_2object - hash");

local *gv = $^STDOUT;
my $gv_ref = B::svref_2object(\*gv);
is(ref $gv_ref, "B::GV", "Test B::GV return from svref_2object");
ok(! $gv_ref->is_empty(), "Test is_empty()");
is($gv_ref->NAME(), "gv", "Test NAME()");
is($gv_ref->SAFENAME(), "gv", "Test SAFENAME()");

# The following return B::SPECIALs.
is(ref B::sv_yes(), "B::SPECIAL", "B::sv_yes()");
is(ref B::sv_no(), "B::SPECIAL", "B::sv_no()");
is(ref B::sv_undef(), "B::SPECIAL", "B::sv_undef()");

# More utility functions
is(B::ppname(0), "pp_null", "Testing ppname (this might break if opnames.h is changed)");
is(B::opnumber("null"), 0, "Testing opnumber with opname (null)");
is(B::opnumber("pp_null"), 0, "Testing opnumber with opname (pp_null)");
like(B::hash("wibble"), qr/0x[0-9a-f]*/, "Testing B::hash()");
is(B::cstring("wibble"), '"wibble"', "Testing B::cstring()");
is(B::perlstring("wibble"), '"wibble"', "Testing B::perlstring()");
is(B::perlstring("\n"), '"\n"', "Testing B::perlstring()");
is(B::perlstring("\""), '"\""', "Testing B::perlstring()");
is(B::class(bless \%(), "Wibble::Bibble"), "Bibble", "Testing B::class()");
is(B::cast_I32(3.14), 3, "Testing B::cast_I32()");
is(B::opnumber("chop"), 35, "Testing opnumber with opname (chop)");

do {
    no warnings 'once';
    my $sg = B::sub_generation();
    *UNIVERSAL::hand_waving = sub { };
    ok( $sg +< B::sub_generation(), "sub_generation increments" );
};
