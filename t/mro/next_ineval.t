#!/usr/bin/perl

use strict;
use warnings;

require q(./test.pl); plan(tests => 1);

=pod

This tests the use of an eval{} block to wrap a next::method call.

=cut

{
    package A;
    use mro 'c3'; 

    sub foo {
      die 'A::foo died';
      return 'A::foo succeeded';
    }
}

{
    package B;
    use base 'A';
    use mro 'c3'; 
    
    sub foo {
      try {
        return 'B::foo => ' . (shift)->next::method();
      };

      if ($@) {
        return $@->{description};
      }
    }
}

like(B->foo, 
   qr/^A::foo died/, 
   'method resolved inside eval{}');


