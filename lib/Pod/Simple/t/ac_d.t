BEGIN {
    if(env::var('PERL_CORE')) {
        chdir 't';
        $^INCLUDE_PATH = @( '../lib' );
    }
}

use Test::More;
BEGIN { plan tests => 14 };

#use Pod::Simple::Debug (6);

use Pod::Simple::DumpAsXML;
use Pod::Simple::XMLOutStream;
print $^STDOUT, "# Pod::Simple version $Pod::Simple::VERSION\n";

$Pod::Simple::XMLOutStream::ATTR_PAD   = ' ';
$Pod::Simple::XMLOutStream::SORT_ATTRS = 1; # for predictably testable output

#sub e ($$) { Pod::Simple::DumpAsXML->_duo(@_) }

$Pod::Simple::XMLOutStream::ATTR_PAD   = ' ';
$Pod::Simple::XMLOutStream::SORT_ATTRS = 1; # for predictably testable output

my $x = 'Pod::Simple::XMLOutStream';

ok 1;

print $^STDOUT, "# Testing exceptions being thrown...\n";

try { $x->new->accept_directive('head1') };
if($^EVAL_ERROR) { ok 1 } # print " # Good: exception thrown: $@\n" }
else   { is 0,1, 'No exception thrown!' }

try { $x->new->accept_directive('I like pie') };
if($^EVAL_ERROR) { ok 1 } # print " # Good: exception thrown: $@\n" }
else   { is 0,1, 'No exception thrown!' }

#~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
# print "Testing basic directive behavior...\n";

sub Pd { shift->accept_directive_as_data(     'freepies') }
sub Pv { shift->accept_directive_as_verbatim( 'freepies') }
sub Pp { shift->accept_directive_as_processed('freepies') }

like( $x->_out( "\n=freepies Mmmmpie\n\n") => '/POD ERROR/' );

is( $x->_out(\&Pp, "\n=freepies Mmmmpie\n\n"),
  '<Document><freepies>Mmmmpie</freepies></Document>'
);
is( $x->_out(\&Pv, "\n=freepies Mmmmpie\n\n"),
  '<Document><freepies xml:space="preserve">Mmmmpie</freepies></Document>'
);
is( $x->_out(\&Pd, "\n=freepies Mmmmpie\n\n"),
  '<Document><freepies xml:space="preserve">Mmmmpie</freepies></Document>'
);

# print "Testing more complex directive behavior...\n";

is( $x->_out(\&Pp, "\n=freepies Mmmmpie \n\tI<is good>!  \n\n"),
  '<Document><freepies>Mmmmpie <I>is good</I>!</freepies></Document>'
);
is( $x->_out(\&Pd, "\n=freepies Mmmmpie \n\tI<is good>!  \n\n"),
 qq{<Document><freepies xml:space="preserve">Mmmmpie \n\tI&#60;is good&#62;!  </freepies></Document>}
);
is( $x->_out(\&Pv, "\n=freepies Mmmmpie \n\tI<is good>!  \n\n"),
 qq{<Document><freepies xml:space="preserve">Mmmmpie \n        I&#60;is good&#62;!  </freepies></Document>}
);

# print "Testing within larger documents...\n";


is( $x->_out(\&Pp, "\n=head1 NAME\n\nPie Consortium -- me gustan pasteles\n\n=freepies Mmmmpie \n\tI<is good>!  \n\nGoody!"),
  '<Document><head1>NAME</head1><Para>Pie Consortium -- me gustan pasteles</Para><freepies>Mmmmpie <I>is good</I>!</freepies><Para>Goody!</Para></Document>'
);
is( $x->_out(\&Pd, "\n=head1 NAME\n\nPie Consortium -- me gustan pasteles\n\n=freepies Mmmmpie \n\tI<is good>!  \n\nGoody!"),
 qq{<Document><head1>NAME</head1><Para>Pie Consortium -- me gustan pasteles</Para><freepies xml:space="preserve">Mmmmpie \n\tI&#60;is good&#62;!  </freepies><Para>Goody!</Para></Document>}
);
is( $x->_out(\&Pv, "\n=head1 NAME\n\nPie Consortium -- me gustan pasteles\n\n=freepies Mmmmpie \n\tI<is good>!  \n\nGoody!"),
 qq{<Document><head1>NAME</head1><Para>Pie Consortium -- me gustan pasteles</Para><freepies xml:space="preserve">Mmmmpie \n        I&#60;is good&#62;!  </freepies><Para>Goody!</Para></Document>}
);


#~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

print $^STDOUT, "# Wrapping up... one for the road...\n";
ok 1;
print $^STDOUT, "# --- Done with ", __FILE__, " --- \n";


__END__

