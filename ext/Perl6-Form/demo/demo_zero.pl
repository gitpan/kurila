use Perl6::Form;

my @nums = @(0, 1, 1.2345, 1234.56, -1234.56, 1234567.89);

print $^STDOUT, < form
	'{$]]]].[}     {$]]]].0}     {$0]]].[}     {$0]]].0}',
	  \@nums, 	    \@nums,      \@nums,      \@nums;
