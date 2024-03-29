package MakeMaker::Test::Setup::Recurs;

our @ISA = qw(Exporter);
require Exporter;
our @EXPORT = qw(setup_recurs teardown_recurs);

use File::Path;
use File::Basename;
use MakeMaker::Test::Utils;

my %Files = %(
             'Recurs/Makefile.PL'          => <<'END',
use ExtUtils::MakeMaker;

WriteMakefile(
    NAME          => 'Recurs',
    VERSION       => 1.00,
);
END

             'Recurs/prj2/Makefile.PL'     => <<'END',
use ExtUtils::MakeMaker;

WriteMakefile(
    NAME => 'Recurs::prj2',
    VERSION => 1.00,
);
END

             # Check if a test failure in a subdir causes make test to fail
             'Recurs/prj2/t/fail.t'         => <<'END',
#!/usr/bin/perl -w

print "1..1\n";
print "not ok 1\n";
END
            );

sub setup_recurs {
    setup_mm_test_root();
    chdir 'MM_TEST_ROOT:[t]' if $^OS_NAME eq 'VMS';

    while(my@(?$file, ?$text) =@( each %Files)) {
        # Convert to a relative, native file path.
        $file = 'File::Spec'->catfile('File::Spec'->curdir, < split m{\/}, $file);

        my $dir = dirname($file);
        mkpath $dir;
        open(my $fh, ">", "$file") || die "Can't create $file: $^OS_ERROR";
        print $fh, $text;
        close $fh;
    }

    return 1;
}

sub teardown_recurs { 
    foreach my $file (keys %Files) {
        my $dir = dirname($file);
        if( -e $dir ) {
            rmtree($dir) || return;
        }
    }
    return 1;
}


1;
