package O;

our $VERSION = '1.00';

use B qw(minus_c save_BEGINs);
use Carp;

sub import {
    my ($class, @options) = @_;
    my ($quiet, $veryquiet) = (0, 0);
    if ($options[0] eq '-q' || $options[0] eq '-qq') {
	$quiet = 1;
	open (SAVEOUT, ">&STDOUT");
	close STDOUT;
	open (STDOUT, ">", \$O::BEGIN_output);
	if ($options[0] eq '-qq') {
	    $veryquiet = 1;
	}
	shift @options;
    }
    my $backend = shift (@options);
    eval q[
	BEGIN {
	    minus_c;
	    save_BEGINs;
	}

	CHECK {
	    if ($quiet) {
		close STDOUT;
		open (STDOUT, ">&SAVEOUT");
		close SAVEOUT;
	    }

	    # Note: if you change the code after this 'use', please
	    # change the fudge factors in B::Concise (grep for
	    # "fragile kludge") so that its output still looks
	    # nice. Thanks. --smcc
	    use B::].$backend.q[ ();
	    if ($@) {
		croak "use of backend $backend failed: $@";
	    }

	    my $compilesub = &{*{Symbol::fetch_glob("B::${backend}::compile")}}(@options);
	    if (ref($compilesub) ne "CODE") {
		die $compilesub;
	    }

            our $savebackslash;
	    local $savebackslash = $\;
	    local ($\,$",$,) = (undef,' ','');
	    &$compilesub();

	    close STDERR if $veryquiet;
	}
    ];
    die $@ if $@;
}

1;

__END__

=head1 NAME

O - Generic interface to Perl Compiler backends

=head1 SYNOPSIS

	perl -MO=[-q,]Backend[,OPTIONS] foo.pl

=head1 DESCRIPTION

This is the module that is used as a frontend to the Perl Compiler.

If you pass the C<-q> option to the module, then the STDOUT
filehandle will be redirected into the variable C<$O::BEGIN_output>
during compilation.  This has the effect that any output printed
to STDOUT by BEGIN blocks or use'd modules will be stored in this
variable rather than printed. It's useful with those backends which
produce output themselves (C<Deparse>, C<Concise> etc), so that
their output is not confused with that generated by the code
being compiled.

The C<-qq> option behaves like C<-q>, except that it also closes
STDERR after deparsing has finished. This suppresses the "Syntax OK"
message normally produced by perl.

=head1 CONVENTIONS

Most compiler backends use the following conventions: OPTIONS
consists of a comma-separated list of words (no white-space).
The C<-v> option usually puts the backend into verbose mode.
The C<-ofile> option generates output to B<file> instead of
stdout. The C<-D> option followed by various letters turns on
various internal debugging flags. See the documentation for the
desired backend (named C<B::Backend> for the example above) to
find out about that backend.

=head1 IMPLEMENTATION

This section is only necessary for those who want to write a
compiler backend module that can be used via this module.

The command-line mentioned in the SYNOPSIS section corresponds to
the Perl code

    use O ("Backend", OPTIONS);

The C<import> function which that calls loads in the appropriate
C<B::Backend> module and calls the C<compile> function in that
package, passing it OPTIONS. That function is expected to return
a sub reference which we'll call CALLBACK. Next, the "compile-only"
flag is switched on (equivalent to the command-line option C<-c>)
and a CHECK block is registered which calls CALLBACK. Thus the main
Perl program mentioned on the command-line is read in, parsed and
compiled into internal syntax tree form. Since the C<-c> flag is
set, the program does not start running (excepting BEGIN blocks of
course) but the CALLBACK function registered by the compiler
backend is called.

In summary, a compiler backend module should be called "B::Foo"
for some foo and live in the appropriate directory for that name.
It should define a function called C<compile>. When the user types

    perl -MO=Foo,OPTIONS foo.pl

that function is called and is passed those OPTIONS (split on
commas). It should return a sub ref to the main compilation function.
After the user's program is loaded and parsed, that returned sub ref
is invoked which can then go ahead and do the compilation, usually by
making use of the C<B> module's functionality.

=head1 BUGS

The C<-q> and C<-qq> options don't work correctly if perl isn't
compiled with PerlIO support : STDOUT will be closed instead of being
redirected to C<$O::BEGIN_output>.

=head1 AUTHOR

Malcolm Beattie, C<mbeattie@sable.ox.ac.uk>

=cut
