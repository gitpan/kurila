use TestInit;
use Config;

use warnings;

use Test::More tests => 3;

BEGIN { use_ok('XS::APItest') };

# I can't see a good way to easily get back perl-space diagnostics for these
# I hope that this isn't a problem.
  ok(sv_setsv_cow_hashkey_core,
     "With PERL_CORE sv_setsv does COW for shared hash key scalars");

ok(!sv_setsv_cow_hashkey_notcore,
   "Without PERL_CORE sv_setsv doesn't COW for shared hash key scalars");
