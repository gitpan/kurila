BEGIN {
    if(env::var('PERL_CORE')) {
        chdir 't';
        $^INCLUDE_PATH = @( '../lib' );
    }
}

use Pod::Simple::Search;
use Test::More;
BEGIN { plan tests => 5 }


#
#  "kleene" rhymes with "zany".  It's a fact!
#


print $^STDOUT, "# ", __FILE__,
 ": Testing limit_glob ...\n";

my $x = Pod::Simple::Search->new;
die "Couldn't make an object!?" unless ok defined $x;

$x->inc(0);
$x->shadows(1);

use File::Spec;
use Cwd;
my $cwd = cwd();
print $^STDOUT, "# CWD: $cwd\n";

sub source_path {
    my $file = shift;
    if (env::var('PERL_CORE')) {
        my $updir = File::Spec->updir;
        my $dir = File::Spec->catdir($updir, 'lib', 'Pod', 'Simple', 't');
        return File::Spec->catdir ($dir, $file);
    } else {
        return $file;
    }
}

my($here1, $here2, $here3);

if(        -e ($here1 = source_path(  'testlib1'      ))) {
  die "But where's $here2?"
    unless -e ($here2 = source_path (   'testlib2'));
  die "But where's $here3?"
    unless -e ($here3 = source_path(   'testlib3'));

} elsif(   -e ($here1 = File::Spec->catdir($cwd, 't', 'testlib1'      ))) {
  die "But where's $here2?"
    unless -e ($here2 = File::Spec->catdir($cwd, 't', 'testlib2'));
  die "But where's $here3?"
    unless -e ($here3 = File::Spec->catdir($cwd, 't', 'testlib3'));

} else {
  die "Can't find the test corpora";
}
print $^STDOUT, "# OK, found the test corpora\n#  as $here1\n# and $here2\n# and $here3\n#\n";
ok 1;

print $^STDOUT, $x->_state_as_string;
#$x->verbose(12);

use Pod::Simple;
*pretty = \&Pod::Simple::BlackBox::pretty;

my $glob = '*k';
print $^STDOUT, "# Limiting to $glob\n";
$x->limit_glob($glob);

my@($name2where, $where2name) = @($x->survey($here1, $here2, $here3), $x->path2name);

my $p = pretty( $where2name, $name2where )."\n";
$p =~ s/, +/,\n/g;
$p =~ s/^/#  /mg;
print $^STDOUT, $p;

do {
my $names = join "|", sort keys %$name2where;
is $names, "Zonk::Pronk|hinkhonk::Glunk|perlzuk|squaa::Glunk|zikzik";
};

do {
my $names = join "|", sort values %$where2name;
is $names, "Zonk::Pronk|hinkhonk::Glunk|hinkhonk::Glunk|perlzuk|squaa::Glunk|zikzik";
};

print $^STDOUT, "# OK, bye from ", __FILE__, "\n";
ok 1;

__END__

