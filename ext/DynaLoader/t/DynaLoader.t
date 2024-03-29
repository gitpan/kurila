#!/usr/bin/perl -w


use Config;
use Test::More;
my %modules;

my $db_file;
BEGIN {
    use Config;
    foreach (qw/DB_File/) {
        if (config_value('extensions') =~ m/\b$_\b/) {
            $db_file = $_;
            last;
        }
    }
}

%modules = %(
   # ModuleName  => q| code to check that it was loaded |,
    'List::Util' => q| ::is( ref List::Util->can('first'), 'CODE' ) |,  # 5.7.2
    'Cwd'        => q| ::is( ref Cwd->can('fastcwd'),'CODE' ) |,         # 5.7 ?
    'File::Glob' => q| ::is( ref File::Glob->can('doglob'),'CODE' ) |,   # 5.6
    ($db_file ?? ( $db_file     => q| ::is( ref $db_file->can('TIEHASH'), 'CODE' ) | ) !! () ),  # 5.0
    'Socket'     => q| ::is( ref Socket->can('inet_aton'),'CODE' ) |,    # 5.0
    'Time::HiRes'=> q| ::is( ref Time::HiRes->can('usleep'),'CODE' ) |,  # 5.7.3
    'Fcntl'      => q| ::is( ref Fcntl->can('O_BINARY'),'CODE' ) |,
);

plan tests => 22 + nelems(keys(%modules)) * 3;

# Try to load the module
use_ok( 'DynaLoader' );

# Check functions
can_ok( 'DynaLoader' => 'bootstrap'               ); # defined in Perl section
can_ok( 'DynaLoader' => 'dl_error'                ); # defined in XS section
can_ok( 'DynaLoader' => 'dl_find_symbol'          ); # defined in XS section
can_ok( 'DynaLoader' => 'dl_install_xsub'         ); # defined in XS section
can_ok( 'DynaLoader' => 'dl_load_file'            ); # defined in XS section
can_ok( 'DynaLoader' => 'dl_load_flags'           ); # defined in Perl section
can_ok( 'DynaLoader' => 'dl_undef_symbols'        ); # defined in XS section
SKIP: do {
    skip "unloading unsupported on $^OS_NAME", 1 if ($^OS_NAME eq 'VMS' || $^OS_NAME eq 'darwin');
    can_ok( 'DynaLoader' => 'dl_unload_file'          ); # defined in XS section
};

TODO: do {
local $TODO = "Test::More::can_ok() seems to have trouble dealing with AutoLoaded functions";
can_ok( 'DynaLoader' => 'dl_expandspec'           ); # defined in AutoLoaded section
can_ok( 'DynaLoader' => 'dl_findfile'             ); # defined in AutoLoaded section
can_ok( 'DynaLoader' => 'dl_find_symbol_anywhere' ); # defined in AutoLoaded section
};


# Check error messages
# .. for bootstrap()
try { DynaLoader::bootstrap() };
like( $^EVAL_ERROR->{?description}, q{/^Usage: DynaLoader::bootstrap\(module\)/},
        "calling DynaLoader::bootstrap() with no argument" );

try { package egg_bacon_sausage_and_spam; DynaLoader::bootstrap("egg_bacon_sausage_and_spam") };
like( $^EVAL_ERROR->{?description}, q{/^Can't locate loadable object for module egg_bacon_sausage_and_spam/},
        "calling DynaLoader::bootstrap() with a package without binary object" );

# .. for dl_load_file()
try { DynaLoader::dl_load_file() };
like( $^EVAL_ERROR->{?description}, q{/^Usage: DynaLoader::dl_load_file\(filename, flags=0\)/},
        "calling DynaLoader::dl_load_file() with no argument" );

try { no warnings 'uninitialized'; DynaLoader::dl_load_file(undef) };
is( $^EVAL_ERROR, '', "calling DynaLoader::dl_load_file() with undefined argument" );     # is this expected ?

my ($dlhandle, $dlerr);
try { $dlhandle = DynaLoader::dl_load_file("egg_bacon_sausage_and_spam") };
$dlerr = DynaLoader::dl_error();
SKIP: do {
    skip "dl_load_file() does not attempt to load file on VMS (and thus does not fail) when \@dl_require_symbols is empty", 1 if $^OS_NAME eq 'VMS';
    ok( !$dlhandle, "calling DynaLoader::dl_load_file() without an existing library should fail" );
};
ok( defined $dlerr, "dl_error() returning an error message: '$dlerr'" );

# Checking for any particular error messages or numeric codes
# is very unportable, please do not try to do that.  A failing
# dl_load_file() is not even guaranteed to set the $! or the $^E.

# ... dl_findfile()
SKIP: do {
    my @files = @( () );
    try { @files = DynaLoader::dl_findfile("c") };
    is( $^EVAL_ERROR, '', "calling dl_findfile()" );
    # Some platforms are known to not have a "libc"
    # (not at least by that name) that the dl_findfile()
    # could find.
    skip "dl_findfile test not appropriate on $^OS_NAME", 1
	if $^OS_NAME =~ m/(win32|vms|openbsd|cygwin)/i;
    # Play safe and only try this test if this system
    # looks pretty much Unix-like.
    skip "dl_findfile test not appropriate on $^OS_NAME", 1
	unless -d '/usr' && -f '/bin/ls';
    cmp_ok( scalar nelems @files, '+>=', 1, "array should contain one result result or more: libc => ($(join ' ',@files))" );
};

# Now try to load well known XS modules
my $extensions = config_value('dynamic_ext');
$extensions =~ s|/|::|g;

for my $module (sort keys %modules) {
    SKIP: do {
        if ($extensions !~ m/\b$module\b/) {
            delete(%modules{$module});
            skip "$module not available", 3;
        }
        eval "use $module";
        is( $^EVAL_ERROR && $^EVAL_ERROR->message, '', "loading $module" );
    };
}

# checking internal consistency
is( nelems @DynaLoader::dl_librefs, nelems( keys %modules), "checking number of items in \@dl_librefs" );
is( nelems @DynaLoader::dl_modules, nelems( keys %modules), "checking number of items in \@dl_modules" );

my @loaded_modules = @DynaLoader::dl_modules;
for my $libref (reverse @DynaLoader::dl_librefs) {
  SKIP: do {
    skip "unloading unsupported on $^OS_NAME", 2 if ($^OS_NAME eq 'VMS' || $^OS_NAME eq 'darwin');
    my $module = pop @loaded_modules;
    my $r = try { DynaLoader::dl_unload_file($libref) };
    is( $^EVAL_ERROR, '', "calling dl_unload_file() for $module" );
    is( $r,  1, " - unload was successful" );
  };
}

