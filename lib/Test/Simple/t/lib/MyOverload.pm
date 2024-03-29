package Overloaded;

sub new {
    my $class = shift;
    bless \%( string => shift, num => shift ), $class;
}


package Overloaded::Compare;
our @ISA = qw(Overloaded);

# Sometimes objects have only comparison ops overloaded and nothing else.
# For example, DateTime objects.
use overload
        q{eq}   => sub { @_[0]->{string} eq @_[1] },
        q{==}   => sub { @_[0]->{num}    == @_[1] };



package Overloaded::Ify;
our @ISA = qw(Overloaded);

use overload
        q{""}    => sub { @_[0]->{string} },
        q{0+}    => sub { @_[0]->{num} };

1;
