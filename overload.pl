#!/usr/bin/perl -w

#
# Generate overload.h
# This allows the order of overloading constants to be changed.
# 

BEGIN {
    # Get function prototypes
    require 'regen_lib.pl';
}

use strict;

my (@enums, @names);
while ( ~< *DATA) {
  next if m/^#/;
  next if m/^$/;
  my ($enum, $name) = m/^(\S+)\s+(\S+)/ or die "Can't parse $_";
  push @enums, $enum;
  push @names, $name;
}

safer_unlink ('overload.h', 'overload.c');
my $c = safer_open("overload.c");
my $h = safer_open("overload.h");

sub print_header {
  my $file = shift;
  print <<"EOF";
/* -*- buffer-read-only: t -*-
 *
 *    $file
 *
 *    Copyright (C) 1997, 1998, 2000, 2001, 2005, 2006, 2007 by Larry Wall
 *    and others
 *
 *    You may distribute under the terms of either the GNU General Public
 *    License or the Artistic License, as specified in the README file.
 *
 *  !!!!!!!   DO NOT EDIT THIS FILE   !!!!!!!
 *  This file is built by overload.pl
 */
EOF
}

select $c;
print_header('overload.c');

select $h;
print_header('overload.h');
print <<'EOF';

enum {
EOF

print "    ${_}_amg,\n", foreach @enums;

print <<'EOF';
    max_amg_code
    /* Do not leave a trailing comma here.  C9X allows it, C89 doesn't. */
};

#define NofAMmeth max_amg_code

EOF

print $c <<'EOF';

#define AMG_id2name(id) (PL_AMG_names[id]+1)
#define AMG_id2namelen(id) (PL_AMG_namelens[id]-1)

char * const PL_AMG_names[NofAMmeth] = {
  /* Names kept in the symbol table.  fallback => "()", the rest has
     "(" prepended.  The only other place in perl which knows about
     this convention is AMG_id2name (used for debugging output and
     'nomethod' only), the only other place which has it hardwired is
     overload.pm.  */
EOF

my $last = pop @names;
print $c "    \"$_\",\n" foreach map { s/(["\\"])/\\$1/g; $_ } @names;

print $c <<"EOT";
    "$last"
\};
EOT

safer_close($h);
safer_close($c);

__DATA__
# Fallback should be the first
fallback	()

# These 5 are the most common in the fallback switch statement in amagic_call
to_sv		(${}
to_av		(@{}
to_hv		(%{}
to_gv		(*{}
to_cv		(&{}

# These have non-default cases in that switch statement
inc		(++
dec		(--
bool_		(bool
numer		(0+
string		(""
not		(!
copy		(=
abs		(abs
neg		(neg
iter		(<>
int		(int

# These 12 feature in the next switch statement
lt		(+<
le		(+<=
gt		(+>
ge		(+>=
eq		(==
ne		(!=
seq		(eq
sne		(ne

ref_eq          (\==

nomethod	(nomethod
add		(+
add_ass		(+=
subtr		(-
subtr_ass	(-=
mult		(*
mult_ass	(*=
div		(/
div_ass		(/=
modulo		(%
modulo_ass	(%=
pow		(**
pow_ass		(**=
lshift		(<<
lshift_ass	(<<=
rshift		(>>
rshift_ass	(>>=
band		(^&^
band_ass	(^&^=
bor		(^|^
bor_ass		(^|^=
bxor		(^^^
bxor_ass	(^^^=
ncmp		(<+>
scmp		(cmp
compl		(^~^
atan2		(atan2
cos		(cos
sin		(sin
exp		(exp
log		(log
sqrt		(sqrt
repeat		(x
repeat_ass	(x=
concat		(.
concat_ass	(.=
smart		(~~
# Note: Perl_Gv_AMupdate() assumes that DESTROY is the last entry
DESTROY		DESTROY
