
use charnames ':full';
use Test::More;
BEGIN { plan tests => 104 };

#use Pod::Simple::Debug (5);

#sub Pod::Simple::MANY_LINES () {1}
#sub Pod::Simple::PullParser::DEBUG () {3}


use Pod::Simple::PullParser;

ok 1;

#~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
###########################################################################

do {
print $^STDOUT, "# Testing another set, at line ", __LINE__, "\n";

my $p = Pod::Simple::PullParser->new;
$p->set_source( \qq{\n=head1 NAME\n\nBzorch\n\n=pod\n\nLala\n\n\=cut\n} );

is $p->get_title(), 'Bzorch';
my $t;

ok( $t = $p->get_token);
is( $t && $t->type, 'start');
is( $t && $t->type eq 'start' && $t->tagname, 'Document' );

ok( $t = $p->get_token);
is( $t && $t->type, 'start');
is( $t && $t->type eq 'start' && $t->tagname, 'head1' );

ok( $t = $p->get_token);
is( $t && $t->type, 'text');
is( $t && $t->type eq 'text' && $t->text, 'NAME' );

};

###########################################################################

do {
print $^STDOUT, "# Testing another set, at line ", __LINE__, "\n";

my $p = Pod::Simple::PullParser->new;
$p->set_source( \qq{\n=head1 NE<65>ME\n\nBzorch\n\n=pod\n\nLala\n\n\=cut\n} );

is $p->get_title(), 'Bzorch';
my $t;

ok( $t = $p->get_token);
is( $t && $t->type, 'start');
is( $t && $t->type eq 'start' && $t->tagname, 'Document' );

ok( $t = $p->get_token);
is( $t && $t->type, 'start');
is( $t && $t->type eq 'start' && $t->tagname, 'head1' );

ok( $t = $p->get_token);
is( $t && $t->type, 'text');

};


###########################################################################

do {
print $^STDOUT, "# Testing another set, at line ", __LINE__, "\n";

do {
my $p = Pod::Simple::PullParser->new;
$p->set_source( \qq{\n=head1 NAME\n\nBzorch - I<thing> lala\n\n=pod\n\nLala\n\n\=cut\n} );
is $p->get_title(), 'Bzorch - thing lala';
};


my $p = Pod::Simple::PullParser->new;
$p->set_source( \qq{\n=head1 NAME\n\nBzorch - I<thing> lala\n\n=pod\n\nLala\n\n\=cut\n} );
is $p->get_title(), 'Bzorch - thing lala';

my $t;

ok( $t = $p->get_token);
is( $t && $t->type, 'start');
is( $t && $t->type eq 'start' && $t->tagname, 'Document' );

ok( $t = $p->get_token);
is( $t && $t->type, 'start');
is( $t && $t->type eq 'start' && $t->tagname, 'head1' );

ok( $t = $p->get_token);
is( $t && $t->type, 'text');
is( $t && $t->type eq 'text' && $t->text, 'NAME' );

};

###########################################################################

do {
print $^STDOUT, "# Testing another set, at line ", __LINE__, "\n";

my $p = Pod::Simple::PullParser->new;
$p->set_source( \qq{\n=head1 Bzorch lala\n\n=pod\n\nLala\n\n\=cut\n} );

ok $p->get_title(), 'Bzorch lala';
my $t;

ok( $t = $p->get_token);
is( $t && $t->type, 'start');
is( $t && $t->type eq 'start' && $t->tagname, 'Document' );

ok( $t = $p->get_token);
is( $t && $t->type, 'start');
is( $t && $t->type eq 'start' && $t->tagname, 'head1' );

ok( $t = $p->get_token);
is( $t && $t->type, 'text');
is( $t && $t->type eq 'text' && $t->text, 'Bzorch lala' );

};

###########################################################################

do {
print $^STDOUT, "# Testing another set, at line ", __LINE__, "\n";

my $p = Pod::Simple::PullParser->new;
$p->set_source( \qq{\n=head1 Bzorch - I<thing> lala\n\n=pod\n\nLala\n\n\=cut\n} );

ok $p->get_title(), 'Bzorch - thing lala';
my $t;

ok( $t = $p->get_token);
is( $t && $t->type, 'start');
is( $t && $t->type eq 'start' && $t->tagname, 'Document' );

ok( $t = $p->get_token);
is( $t && $t->type, 'start');
is( $t && $t->type eq 'start' && $t->tagname, 'head1' );

ok( $t = $p->get_token);
is( $t && $t->type, 'text');
is( $t && $t->type eq 'text' && $t->text, 'Bzorch - ' );

};
###########################################################################

do {
print $^STDOUT, "# Testing another set, at line ", __LINE__, "\n";

my $p = Pod::Simple::PullParser->new;
$p->set_source( \qq{\n=head1 Nombre (NAME)\n\nBzorch - I<thing> lala\n\n=pod\n\nGrunk\n\n\=cut\n} );

is $p->get_version || '', '';
is $p->get_author  || '', '';

is $p->get_title(), 'Bzorch - thing lala';

my $t;
ok( $t = $p->get_token);
is( $t && $t->type, 'start');
is( $t && $t->type eq 'start' && $t->tagname, 'Document' );

};
###########################################################################

do {
print $^STDOUT, "# Testing another set, at line ", __LINE__, "\n";

my $p = Pod::Simple::PullParser->new;
$p->set_source( \qq{\N{BYTE ORDER MARK}\n=head1 ëÏÇÄÂÁ ÞÉÔÁÌÁ (NAME)\n\nëÏÇÄÂÁ ÞÉÔÁÌÁ ÔÂÙ ÍÕÞÉÔÅÌØÎÙÂÅ ÓÔÒÏËÉ -- Fet's I<"When you were> reading\n\n=pod\n\nGrunk\n\n\=cut\n} );

ok $p->get_title(), q{ëÏÇÄÂÁ ÞÉÔÁÌÁ ÔÂÙ ÍÕÞÉÔÅÌØÎÙÂÅ ÓÔÒÏËÉ -- Fet's "When you were reading};
my $t;

ok( $t = $p->get_token);
is( $t && $t->type, 'start');
is( $t && $t->type eq 'start' && $t->tagname, 'Document' );

};

###########################################################################

do {
print $^STDOUT, "# Testing another set, at line ", __LINE__, "\n";

my $p = Pod::Simple::PullParser->new;
$p->set_source( \qq{\N{BYTE ORDER MARK}\n=head1 (NAME) ëÏÇÄÂÁ ÞÉÔÁÌÁ\n\nëÏÇÄÂÁ ÞÉÔÁÌÁ ÔÂÙ ÍÕÞÉÔÅÌØÎÙÂÅ ÓÔÒÏËÉ -- Fet's I<"When you were> reading\n\n=pod\n\nGrunk\n\n\=cut\n} );

ok $p->get_title(), q{ëÏÇÄÂÁ ÞÉÔÁÌÁ ÔÂÙ ÍÕÞÉÔÅÌØÎÙÂÅ ÓÔÒÏËÉ -- Fet's "When you were reading};
my $t;

ok( $t = $p->get_token);
is( $t && $t->type, 'start');
is( $t && $t->type eq 'start' && $t->tagname, 'Document' );

};

###########################################################################

do {
print $^STDOUT, "# Testing another set, at line ", __LINE__, "\n";

my $p = Pod::Simple::PullParser->new;
$p->set_source( \qq{\N{BYTE ORDER MARK}\n=head1 (DESCRIPTION) ëÏÇÄÂÁ ÞÉÔÁÌÁ\n\nëÏÇÄÂÁ ÞÉÔÁÌÁ ÔÂÙ ÍÕÞÉÔÅÌØÎÙÂÅ ÓÔÒÏËÉ -- Fet's I<"When you were> reading\n\n=pod\n\nGrunk\n\n\=cut\n} );

is $p->get_title() || '', '';
is $p->get_description(), q{ëÏÇÄÂÁ ÞÉÔÁÌÁ ÔÂÙ ÍÕÞÉÔÅÌØÎÙÂÅ ÓÔÒÏËÉ -- Fet's "When you were reading};
my $t;

ok( $t = $p->get_token);
is( $t && $t->type, 'start');
is( $t && $t->type eq 'start' && $t->tagname, 'Document' );

};
###########################################################################

do {
print $^STDOUT, "# Testing another set, at line ", __LINE__, "\n";

my $p = Pod::Simple::PullParser->new;
$p->set_source( \qq{\N{BYTE ORDER MARK}\n=head1 (DESCRIPTION) ëÏÇÄÂÁ ÞÉÔÁÌÁ\n\nëÏÇÄÂÁ ÞÉÔÁÌÁ ÔÂÙ ÍÕÞÉÔÅÌØÎÙÂÅ ÓÔÒÏËÉ -- Fet's I<"When you were> reading\n\n=pod\n\nGrunk\n\n\=cut\n} );

ok $p->get_description(), q{ëÏÇÄÂÁ ÞÉÔÁÌÁ ÔÂÙ ÍÕÞÉÔÅÌØÎÙÂÅ ÓÔÒÏËÉ -- Fet's "When you were reading};
is $p->get_title() || '', '';
my $t;

ok( $t = $p->get_token);
is( $t && $t->type, 'start');
is( $t && $t->type eq 'start' && $t->tagname, 'Document' );

};

###########################################################################

do {
print $^STDOUT, "# Testing another set, at line ", __LINE__, "\n";

my $p = Pod::Simple::PullParser->new;
$p->set_source( \qq{\N{BYTE ORDER MARK}\n=head1 NAME\n\nThingy\n\n=head1 (DESCRIPTION) ëÏÇÄÂÁ ÞÉÔÁÌÁ\n\nëÏÇÄÂÁ ÞÉÔÁÌÁ ÔÂÙ ÍÕÞÉÔÅÌØÎÙÂÅ ÓÔÒÏËÉ -- Fet's I<"When you were> reading\n\n=pod\n\nGrunk\n\n\=cut\n} );

ok $p->get_description(), q{ëÏÇÄÂÁ ÞÉÔÁÌÁ ÔÂÙ ÍÕÞÉÔÅÌØÎÙÂÅ ÓÔÒÏËÉ -- Fet's "When you were reading};
ok $p->get_title(), "Thingy";
my $t;

ok( $t = $p->get_token);
is( $t && $t->type, 'start');
is( $t && $t->type eq 'start' && $t->tagname, 'Document' );

};

###########################################################################

do {
print $^STDOUT, "# Testing another set, at line ", __LINE__, "\n";

my $p = Pod::Simple::PullParser->new;
$p->set_source( \qq{\N{BYTE ORDER MARK}\n=head1 NAME\n\nThingy\n\n=head1 (DESCRIPTION) ëÏÇÄÂÁ ÞÉÔÁÌÁ\n\nëÏÇÄÂÁ ÞÉÔÁÌÁ ÔÂÙ ÍÕÞÉÔÅÌØÎÙÂÅ ÓÔÒÏËÉ -- Fet's I<"When you were> reading\n\n=pod\n\nGrunk\n\n\=cut\n} );

ok $p->get_title(), "Thingy";
ok $p->get_description(), q{ëÏÇÄÂÁ ÞÉÔÁÌÁ ÔÂÙ ÍÕÞÉÔÅÌØÎÙÂÅ ÓÔÒÏËÉ -- Fet's "When you were reading};
my $t;

ok( $t = $p->get_token);
is( $t && $t->type, 'start');
is( $t && $t->type eq 'start' && $t->tagname, 'Document' );

};

###########################################################################

do {
print $^STDOUT, "# Testing another set, at line ", __LINE__, "\n";

my $p = Pod::Simple::PullParser->new;
$p->set_source( \qq{\N{BYTE ORDER MARK}\n=head1 (NAME) ÷ÄÁÌÂÉ ÐÅÒÅÂÄ\n\nThingy\n\n=head1 (DESCRIPTION) ëÏÇÄÂÁ ÞÉÔÁÌÁ\n\nëÏÇÄÂÁ ÞÉÔÁÌÁ ÔÂÙ ÍÕÞÉÔÅÌØÎÙÂÅ ÓÔÒÏËÉ -- Fet's I<"When you were> reading\n\n=pod\n\nGrunk\n\n\=cut\n} );

ok $p->get_title(), "Thingy";
ok $p->get_description(), q{ëÏÇÄÂÁ ÞÉÔÁÌÁ ÔÂÙ ÍÕÞÉÔÅÌØÎÙÂÅ ÓÔÒÏËÉ -- Fet's "When you were reading};
my $t;

ok( $t = $p->get_token);
is( $t && $t->type, 'start');
is( $t && $t->type eq 'start' && $t->tagname, 'Document' );

};

###########################################################################

do {
print $^STDOUT, "# Testing another set, at line ", __LINE__, "\n";

my $p = Pod::Simple::PullParser->new;
$p->set_source( \qq{\N{BYTE ORDER MARK}

=head1 (NAME) ÷ÄÁÌÂÉ ÐÅÒÅÂÄ

Thingy

=head1 (DESCRIPTION) ëÏÇÄÂÁ ÞÉÔÁÌÁ

ëÏÇÄÂÁ ÞÉÔÁÌÁ ÔÂÙ ÍÕÞÉÔÅÌØÎÙÂÅ ÓÔÒÏËÉ -- Fet's I<"When you were> reading

=pod

Grunk

=cut
} );

ok $p->get_title(), "Thingy";
is $p->get_version() || '', '';
is $p->get_description(), q{ëÏÇÄÂÁ ÞÉÔÁÌÁ ÔÂÙ ÍÕÞÉÔÅÌØÎÙÂÅ ÓÔÒÏËÉ -- Fet's "When you were reading};
my $t;

ok( $t = $p->get_token);
is( $t && $t->type, 'start');
is( $t && $t->type eq 'start' && $t->tagname, 'Document' );

};

###########################################################################

do {
print $^STDOUT, "# Testing another set, at line ", __LINE__, "\n";

my $p = Pod::Simple::PullParser->new;
$p->set_source( \qq{\N{BYTE ORDER MARK}

=head1 (NAME) ÷ÄÁÌÂÉ ÐÅÒÅÂÄ

Thingy

=head1 (DESCRIPTION) ëÏÇÄÂÁ ÞÉÔÁÌÁ

ëÏÇÄÂÁ ÞÉÔÁÌÁ ÔÂÙ ÍÕÞÉÔÅÌØÎÙÂÅ ÓÔÒÏËÉ -- Fet's I<"When you were> reading

=head1 VERSION

  Stuff: Thing
  Whatever: Um.

=head1 AUTHOR

Jojoj E<65>arzarz

=pod

Grunk

=cut
} );

ok $p->get_title(), "Thingy";
my $v = $p->get_version || '';
$v =~ s/^ +//m;
$v =~ s/^\s+//s;
$v =~ s/\s+$//s;
ok $v, "Stuff: Thing\nWhatever: Um.";
ok $p->get_description(), q{ëÏÇÄÂÁ ÞÉÔÁÌÁ ÔÂÙ ÍÕÞÉÔÅÌØÎÙÂÅ ÓÔÒÏËÉ -- Fet's "When you were reading};
ok $p->get_author() || '', 'Jojoj Aarzarz';


my $t;
ok( $t = $p->get_token);
is( $t && $t->type, 'start');
is( $t && $t->type eq 'start' && $t->tagname, 'Document' );

};

###########################################################################
###########################################################################


print $^STDOUT, "# Wrapping up... one for the road...\n";
ok 1;
print $^STDOUT, "# --- Done with ", __FILE__, " --- \n";

__END__

