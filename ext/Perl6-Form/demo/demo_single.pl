use Perl6::Form;

print form {single=>'^'},{single=>'='},{single=>'_'},
		   '~~~~~~~~~',
		   '^ _ = _ ^',
		   qw(Like round and orient perls),
		   '~~~~~~~~~';

print "\n--------------------------\n\n";

print form {single=>'='},
           '   ^',
           ' = | {""""""""""""""""""""""""""""""""""""}',
			 "Height",
				 [ ~< *DATA],
		   '   +------------------------------------->',
		   '    {|||||||||||||||||||||||||||||||||||}',
				"Time";

__DATA__
      *
    *   *
   *     *
          
  *       *
           
 *         *
          
         
        
*           *

