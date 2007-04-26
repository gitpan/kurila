use ExtUtils::MakeMaker;

my @libs;
push @libs, '-L/lib/w32api -lole32 -lversion' if $^O eq "cygwin";

WriteMakefile(
    NAME         => 'Win32',
    VERSION_FROM => 'Win32.pm',
    LIBS         => \@libs,
);