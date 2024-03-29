BEGIN {
    if(env::var('PERL_CORE')) {
        chdir 't';
        $^INCLUDE_PATH = @( '../lib' );
    }
}

use Test::More;
BEGIN { plan tests => 30 };

#use Pod::Simple::Debug (6);

ok 1;

require Pod::Simple::BlackBox;
ok 1;

require Pod::Simple; ok 1;

Pod::Simple->VERSION(.90); ok 1;

#print "# Pod::Simple version $Pod::Simple::VERSION\n";

require Pod::Simple::DumpAsXML; ok 1;

require Pod::Simple::XMLOutStream; ok 1;

sub e ($x, $y) { Pod::Simple::DumpAsXML->_duo($x, $y) }

print $^STDOUT, "# Simple identity tests...\n";

is( < e "", "" );
is( < e "\n", "", );
is( < e "\n", "\n", );
is( < e "puppies\n\n\n\n", "", );


print $^STDOUT, "# Contentful identity tests...\n";

is( < e "=pod\n\nFoo\n",         "=pod\n\nFoo\n"         );
is( < e "=pod\n\n\n\nFoo\n\n\n", "=pod\n\n\n\nFoo\n\n\n" );
is( < e "=pod\n\n\n\nFoo\n\n\n", "=pod\n\nFoo\n"         );

# Now with some more newlines
is( < e "\n\n=pod\n\nFoo\n",     "\n\n=pod\n\nFoo\n"     );
is( < e "=pod\n\n\n\nFoo\n\n\n", "=pod\n\n\n\nFoo\n\n\n" );
is( < e "=pod\n\n\n\nFoo\n\n\n", "\n\n=pod\n\nFoo\n"     );


is( < e "=head1 Foo\n",          "=head1 Foo\n"          );
is( < e "=head1 Foo\n\n=cut\n",  "=head1 Foo\n\n=cut\n"  );
is( < e "=head1 Foo\n\n=cut\n",  "=head1 Foo\n"          );

# Now just add some newlines...
is( < e "\n\n\n\n=head1 Foo\n",  "\n\n\n\n=head1 Foo\n"  );
is( < e "=head1 Foo\n\n=cut\n",  "=head1 Foo\n\n=cut\n"  );
is( < e "=head1 Foo\n\n=cut\n",  "\n\n\n\n=head1 Foo\n"  );


print $^STDOUT, "# Simple XMLification tests...\n";

is( Pod::Simple::XMLOutStream->_out("\n\n\nprint \$^T;\n\n\n"),
    qq{<Document\ncontentless="1"></Document>}
     # make sure the contentless flag is set
);
is( Pod::Simple::XMLOutStream->_out("\n\n"),
    qq{<Document\ncontentless="1"></Document>}
     # make sure the contentless flag is set
);
is( Pod::Simple::XMLOutStream->_out("\n"),
    qq{<Document\ncontentless="1"></Document>}
     # make sure the contentless flag is set
);
is( Pod::Simple::XMLOutStream->_out(""),
    qq{<Document\ncontentless="1"></Document>}
     # make sure the contentless flag is set
);

ok( Pod::Simple::XMLOutStream->_out('', '<Document></Document>' ) );

is( Pod::Simple::XMLOutStream->_out("=pod\n\nFoo\n"),
    '<Document><Para>Foo</Para></Document>'
);

is( Pod::Simple::XMLOutStream->_out("=head1 Chacha\n\nFoo\n"),
    '<Document><head1>Chacha</head1><Para>Foo</Para></Document>'
);


print $^STDOUT, "# Wrapping up... one for the road...\n";
ok 1;
print $^STDOUT, "# --- Done with ", __FILE__, " --- \n";


