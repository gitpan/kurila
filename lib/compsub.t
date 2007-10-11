#!./perl

BEGIN { require './test.pl'; }
BEGIN { plan( tests => 14 ); }

use strict;
use warnings;

use compsub;

# create a user defined functions.
# basic test that is called the correct number of times.

our $foo_called;
# BEGIN {
#     # define 'function' foo, which only executes it first argument.
#     # i.e. the opcode tree generated by foo $arg1, $arg2, ...  is something like as $arg1, $arg2, ...
#     $^H{'compsub'} = {
#                         foo => sub { $foo_called++; return $_[0] },
#                        };
# }
BEGIN { compsub::define( foo => sub { $foo_called++; $_[0] or die "no arguments"; return $_[0] } ); }

BEGIN { is $foo_called, undef; }
sub bar { foo(); }
BEGIN { is $foo_called, 1; }

is( (join '*', foo 1, 2), "1*2");

BEGIN { is $foo_called, 2; }

use B ();
use B::OP ();

# fst $arg1, $arg2, ... evaluates only its first argument
# this leaks the OP_LIST, and the remaining items in the listop
BEGIN { compsub::define( fst => sub { my $first = $_[0]->first->sibling;
                                        # remove $first from the list of children of $_[0]
                                        $_[0]->first->set_sibling($first->sibling);
                                        $_[0]->free; undef $_[0];
                                        # free the $_[0] opcode.
                                        return $first;
                                    }
                         );
    }

$b = "oldb";
fst $a="newa", $b="notset";
is("$a-$b", "newa-oldb");

{
    BEGIN { compsub::define( nothing => sub { $_[0] and $_[0]->free; return B::OP->new('null', 0) } ); }
    nothing;

    eval "nothing";
    ok ! $@, "compsub in run-time eval";
}
eval "nothing";
like $@, qr/Bareword "nothing" not allowed/, "compsub lexical scoped.";


## calling a function
{
    our $x;
    sub func1 { $x++; return "func1 called. args: @_" };

    BEGIN { compsub::define( compfunc1 => sub { my $op = shift;
                                                my $cvop = B::SVOP->new('const', 0, *func1);
                                                $op = B::LISTOP->new('list', 0, ($op ? ($op, $cvop) : ($cvop, undef)));
                                                return B::UNOP->new('entersub', B::OPf_STACKED|B::OPf_SPECIAL, $op);
                                            } ); }

    is( (compfunc1), "func1 called. args: ");
    is( (compfunc1 1, 2, 3), "func1 called. args: 1 2 3");
    is( (compfunc1 1, 2, 3), "func1 called. args: 1 2 3");
    is( (compfunc1(1, 2, 3)), "func1 called. args: 1 2 3");
    is( (compfunc1(1, 2), 3), "func1 called. args: 1 2 3");
}

## parsing params, and declaring lexical variables.
{
    # assumes argument like: 'foo' => \$foo, 'bar' => \$bar, { @_ }
    sub parseparams {
        my $values = pop @_;
        while (my $name = shift @_) {
            $_[0] = $values->{$name};
            shift @_;
        }
    }

    # assumes argument like C<'foo', 'bar', { @_ }>
    # this will be converted like C<parseparams('foo', \(my $foo), 'bar', \(my $bar), { @_ })>
    sub compparams {
        my $op = shift;
        $op or return B::UNOP->new('null', 0);
        my $kid = $op->first;
        while (ref $kid ne "B::NULL") {
            if ($kid->name eq "const") {
                # allocate a 'my' variable
                my $targ = B::PAD::allocmy( '$' . ${ $kid->sv->object_2svref } );
                # introduce the 'my' variable, and insert it into the list of argument.
                my $padsv = B::OP->new('padsv', B::OPf_MOD);
                $padsv->set_private(B::OPpLVAL_INTRO);
                $padsv->set_targ($targ);
                $padsv->set_sibling($kid->sibling);
                $kid->set_sibling($padsv);

                $kid = $padsv;
            } elsif ($kid->name eq "list" or $kid->name eq "pushmark") {
                # ignore
            } elsif ($kid->name eq "anonhash") {
                # ignore, assume it is the last item in the list.
            } else {
                die "Expected constant opcode but got " . $kid->name;
            }
            $kid = $kid->sibling;
        }
        my $cvop = B::SVOP->new('const', 0, *parseparams);
        $op = B::LISTOP->new('list', 0, ($op ? ($op, $cvop) : ($cvop, undef)));
        my $entersubop = B::UNOP->new('entersub', B::OPf_STACKED|B::OPf_SPECIAL, $op);
        return $entersubop;
    }

    BEGIN {
        compsub::define( params => \&compparams )
    }

    {
        sub foobar {
            params 'foo', 'bar', { @_ };
            is $foo, 'foo-value', '$foo declared and initialized';
            is $bar, 'bar-value';
        }

        foobar( foo => "foo-value", bar => "bar-value" );
    }
}
