BEGIN {
    if(env::var('PERL_CORE')) {
        chdir 't';
        $^INCLUDE_PATH = @( '../lib' );
    }
}

use Test::More;
BEGIN { plan tests => 24 };

my $d;
#use Pod::Simple::Debug (\$d,0);

ok 1;

use Pod::Simple::DumpAsXML;
use Pod::Simple::XMLOutStream;
print $^STDOUT, "# Pod::Simple version $Pod::Simple::VERSION\n";
sub e ($x, $y) { Pod::Simple::DumpAsXML->_duo($x, $y) }

my $x = 'Pod::Simple::XMLOutStream';

print $^STDOUT, "##### Tests for =item directives via class $x\n";

$Pod::Simple::XMLOutStream::ATTR_PAD   = ' ';
$Pod::Simple::XMLOutStream::SORT_ATTRS = 1; # for predictably testable output


print $^STDOUT, "#\n# Tests for simple =item *'s\n";
is( $x->_out("\n=over\n\n=item *\n\nStuff\n\n=item *\n\nBar I<baz>!\n\n=back\n\n"),
    '<Document><over-bullet indent="4"><item-bullet>Stuff</item-bullet><item-bullet>Bar <I>baz</I>!</item-bullet></over-bullet></Document>'
);
is( $x->_out("\n=over\n\n=item *\n\nStuff\n\n=cut\n\nStuff\n\n=item *\n\nBar I<baz>!\n\n=back\n\n"),
    '<Document><over-bullet indent="4"><item-bullet>Stuff</item-bullet><item-bullet>Bar <I>baz</I>!</item-bullet></over-bullet></Document>'
);
is( $x->_out("\n=over 10\n\n=item *\n\nStuff\n\n=cut\n\nStuff\n\n=item *\n\nBar I<baz>!\n\n=back\n\n"),
    '<Document><over-bullet indent="10"><item-bullet>Stuff</item-bullet><item-bullet>Bar <I>baz</I>!</item-bullet></over-bullet></Document>'
);
is( $x->_out("\n=over\n\n=item *\n\nStuff\n=cut\nStuff\n\n=item *\n\nBar I<baz>!\n\n=back"),
    '<Document><over-bullet indent="4"><item-bullet>Stuff</item-bullet><item-bullet>Bar <I>baz</I>!</item-bullet></over-bullet></Document>'
);




print $^STDOUT, "#\n# Tests for simple =item 1.'s\n";
is( $x->_out("\n=over\n\n=item 1.\n\nStuff\n\n=item 2.\n\nBar I<baz>!\n\n=back\n\n"),
    '<Document><over-number indent="4"><item-number number="1">Stuff</item-number><item-number number="2">Bar <I>baz</I>!</item-number></over-number></Document>'
);
is( $x->_out("\n=over\n\n=item 1.\n\nStuff\n\n=cut\n\nStuff\n\n=item 2.\n\nBar I<baz>!\n\n=back\n\n"),
    '<Document><over-number indent="4"><item-number number="1">Stuff</item-number><item-number number="2">Bar <I>baz</I>!</item-number></over-number></Document>'
);
# Now without a dot
is( $x->_out("\n=over\n\n=item 1\n\nStuff\n\n=cut\n\nStuff\n\n=item 2\n\nBar I<baz>!\n\n=back\n\n"),
    '<Document><over-number indent="4"><item-number number="1">Stuff</item-number><item-number number="2">Bar <I>baz</I>!</item-number></over-number></Document>'
);
is( $x->_out("\n=over\n\n=item 1\n\nStuff\n=cut\nStuff\n\n=item 2\n\nBar I<baz>!\n\n=back"),
    '<Document><over-number indent="4"><item-number number="1">Stuff</item-number><item-number number="2">Bar <I>baz</I>!</item-number></over-number></Document>'
);



print $^STDOUT, "#\n# Tests for =over blocks (without =items)\n";
is( $x->_out("\n=over\n\nStuff\n\nBar I<baz>!\n\n=back\n\n"),
    '<Document><over-block indent="4"><Para>Stuff</Para><Para>Bar <I>baz</I>!</Para></over-block></Document>'
);
is( $x->_out("\n=over\n\n Stuff\n\nBar I<baz>!\n\n=back\n\n"),
    '<Document><over-block indent="4"><Verbatim xml:space="preserve"> Stuff</Verbatim><Para>Bar <I>baz</I>!</Para></over-block></Document>'
);
is( $x->_out("\n=over\n\nBar I<baz>!\n\n Stuff\n\n=back\n\n"),
    '<Document><over-block indent="4"><Para>Bar <I>baz</I>!</Para><Verbatim xml:space="preserve"> Stuff</Verbatim></over-block></Document>'
);




print $^STDOUT, "#\n# Tests for =item Text blocks...\n";
is( $x->_out("\n=over\n\n=item Foo\n\nStuff\n\n=cut\n\nCrunk\nZorp\n\n=item Bar I<baz>!\n\nQuux\n\n=back\n\n"),
    '<Document><over-text indent="4"><item-text>Foo</item-text><Para>Stuff</Para><item-text>Bar <I>baz</I>!</item-text><Para>Quux</Para></over-text></Document>'
);
is( $x->_out("\n=over\n\n=item Foo\n\n Stuff\n\tSnork\n\n=cut\n\nCrunk\nZorp\n\n=item Bar I<baz>!\n\nQuux\n\n=back\n\n"),
    qq{<Document><over-text indent="4"><item-text>Foo</item-text><Verbatim xml:space="preserve"> Stuff\n        Snork</Verbatim>}
  . qq{<item-text>Bar <I>baz</I>!</item-text><Para>Quux</Para></over-text></Document>}
);
is( $x->_out("\n=over\n\n=item Foo\n\n Stuff\n\tSnork\n=cut\n\nCrunk\nZorp\n\n=item Bar I<baz>!\n\nQuux\n\n=back\n\n"),
    qq{<Document><over-text indent="4"><item-text>Foo</item-text><Verbatim xml:space="preserve"> Stuff\n        Snork</Verbatim>}
  . qq{<item-text>Bar <I>baz</I>!</item-text><Para>Quux</Para></over-text></Document>}
);



print $^STDOUT, "#\n# Test for mixed =item blocks...\n";
is( $x->_out("\n=over\n\n=item Foo\n\nStuff\n\n=item 2.\n\nBar I<baz>!\n\nQuux\n\n=item *\n\nThwoong\n\n=back\n\n"),
    qq{<Document><over-text indent="4"><item-text>Foo</item-text><Para>Stuff</Para>}
  . qq{<item-text>2.</item-text><Para>Bar <I>baz</I>!</Para><Para>Quux</Para>}
  . qq{<item-text>*</item-text><Para>Thwoong</Para></over-text></Document>}
);

# is( $x->_out("\n=over\n\n=item *\n\nStuff\n\n=item 2.\n\nBar I<baz>!\n\nQuux\n\n=item *\n\nThwoong\n\n=back\n\n"),
# is( $x->_out("\n=over\n\n=item 1.\n\nStuff\n\n=item 2.\n\nBar I<baz>!\n\nQuux\n\n=item *\n\nThwoong\n\n=back\n\n"),

print $^STDOUT, "#\n# Tests for indenting\n";
is( $x->_out("\n=over 19\n\n=item *\n\nStuff\n\n=item *\n\nBar I<baz>!\n\n=back\n\n"),
    '<Document><over-bullet indent="19"><item-bullet>Stuff</item-bullet><item-bullet>Bar <I>baz</I>!</item-bullet></over-bullet></Document>'
);
is( $x->_out("\n=over 19\n\n=item 1.\n\nStuff\n\n=item 2.\n\nBar I<baz>!\n\n=back\n\n"),
    '<Document><over-number indent="19"><item-number number="1">Stuff</item-number><item-number number="2">Bar <I>baz</I>!</item-number></over-number></Document>'
);
is( $x->_out("\n=over 19\n\nStuff\n\nBar I<baz>!\n\n=back\n\n"),
    '<Document><over-block indent="19"><Para>Stuff</Para><Para>Bar <I>baz</I>!</Para></over-block></Document>'
);
is( $x->_out("\n=over 19\n\n=item Foo\n\nStuff\n\n=cut\n\nCrunk\nZorp\n\n=item Bar I<baz>!\n\nQuux\n\n=back\n\n"),
    '<Document><over-text indent="19"><item-text>Foo</item-text><Para>Stuff</Para><item-text>Bar <I>baz</I>!</item-text><Para>Quux</Para></over-text></Document>'
);

#~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

print $^STDOUT, "# Now testing nesting...\n";
is( $x->_out(join "\n\n", @( '',
  '=over',
    '=item *',
    'Stuff',
    '=cut',
    'Stuff',
    '=over',
      '=item 1.',
      '=item 2.',
      'Bar I<baz>!',
    '=back',
    '=item *',
    'Bar I<baz>!',
    '=back', '')
  ), join '', @(
   '<Document>',
   '<over-bullet indent="4">',
     '<item-bullet>Stuff</item-bullet>',
     '<over-number indent="4">',
       '<item-number number="1"></item-number>',
       '<item-number number="2">Bar <I>baz</I>!</item-number>',
     '</over-number>',
     '<item-bullet>Bar <I>baz</I>!</item-bullet>',
   '</over-bullet></Document>')
);

#~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

is( $x->_out( join "\n\n", @( '', '', 
  '=over',
    '=item *',
    'Stuff',
    '=cut',
    'Stuff',
    '=over',
      '=item 1.',
        '=over 19',
        'Gleiven',
        'Squim F<.thingrc>!',
        '=back',
      '=item 2.',
      'Bar I<baz>!',
      '=back',
    '=item *',
    'Bar I<baz>!',
  '=back',
  '', '')
  ), join '', @(
   '<Document>',
   '<over-bullet indent="4">',
     '<item-bullet>Stuff</item-bullet>',
     '<over-number indent="4">',
       '<item-number number="1"></item-number>',

       '<over-block indent="19">',
         '<Para>Gleiven</Para>',
         '<Para>Squim <F>.thingrc</F>!</Para>',
       '</over-block>',

       '<item-number number="2">Bar <I>baz</I>!</item-number>',
     '</over-number>',
     '<item-bullet>Bar <I>baz</I>!</item-bullet>',
   '</over-bullet></Document>')
);

#~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

$d = 11;
print $^STDOUT, "# Now checking that document-end closes things right...\n";

is( $x->_out( join "\n\n", @( '', '', 
  '=over',
    '=item *',
    'Stuff',
    '=cut',
    'Stuff',
    '=over',
      '=item 1.',
        '=over 19',
        'Gleiven',
        'Squim F<.thingrc>!',
  '', '')
  ), join '', @(
   '<Document>',
   '<over-bullet indent="4">',
     '<item-bullet>Stuff</item-bullet>',
     '<over-number indent="4">',
       '<item-number number="1"></item-number>',

       '<over-block indent="19">',
         '<Para>Gleiven</Para>',
         '<Para>Squim <F>.thingrc</F>!</Para>',
       '</over-block>',
     '</over-number>',
   '</over-bullet></Document>')
);



# TODO: more checking of coercion in nesting?



print $^STDOUT, "# Wrapping up... one for the road...\n";
ok 1;
print $^STDOUT, "# --- Done with ", __FILE__, " --- \n";

