#
# Maintainers.pm - show information about maintainers
#

package Maintainers;


use lib "Porting";

require "Maintainers.pl";
our (%Modules, %Maintainers);

our (@ISA, @EXPORT_OK);
@ISA = qw(Exporter);
@EXPORT_OK = qw(%Modules %Maintainers
		get_module_files get_module_pat
		show_results process_options);
require Exporter;

use File::Find;
use Getopt::Long;

my %MANIFEST;
if (open(MANIFEST, "<", "MANIFEST")) {
    while ( ~< *MANIFEST) {
	if (m/^(\S+)\t+(.+)$/) {
	    %MANIFEST{+$1}++;
	}
    }
    close MANIFEST;
} else {
    die "$^PROGRAM_NAME: Failed to open MANIFEST for reading: $^OS_ERROR\n";
}

sub get_module_pat {
    my $m = shift;
    split ' ', %Modules{$m}->{?FILES};
}

sub get_module_files {
    my $m = shift;
    sort { lc $a cmp lc $b }
 map {
	-f $_ ?? # Files as-is.
	    $_ !!
	    -d _ ?? # Recurse into directories.
	    do {
		my @files;
		find(
		     sub {
			 push @files, $File::Find::name
			     if -f $_ && exists %MANIFEST{$File::Find::name};
		     }, $_);
		@files;
	    }
	!! glob( <$_) # The rest are globbable patterns.
	} get_module_pat($m);
}

sub get_maintainer_modules {
    my $m = shift;
    sort { lc $a cmp lc $b }
 grep { %Modules{$_}->{?MAINTAINER} eq $m }
    keys %Modules;
}

sub usage {
    print <<__EOF__;
$^PROGRAM_NAME: Usage: $^PROGRAM_NAME [[--maintainer M --module M --files]|[--check] file ...]
--maintainer M	list all maintainers matching M
--module M	list all modules matching M
--files		list all files
--check		check consistency of Maintainers.pl
			with a file	checks if it has a maintainer
			with a dir	checks all files have a maintainer
			otherwise	checks for multiple maintainers
--opened	list all modules of files opened by perforce
Matching is case-ignoring regexp, author matching is both by
the short id and by the full name and email.  A "module" may
not be just a module, it may be a file or files or a subdirectory.
The options may be abbreviated to their unique prefixes
__EOF__
    exit(0);
}

my $Maintainer;
my $Module;
my $Files;
my $Check;
my $Opened;

sub process_options {
    usage()
	unless
	    GetOptions(
		       'maintainer=s'	=> \$Maintainer,
		       'module=s'	=> \$Module,
		       'files'		=> \$Files,
		       'check'		=> \$Check,
		       'opened'		=> \$Opened,
		      );

    my @Files;
   
    if ($Opened) {
	my @raw = @( `p4 opened` );
	die if $^CHILD_ERROR;
	@Files = map {s!#.*!!s; s!^//depot/.*?/perl/!!; $_} @raw;
    } else {
	@Files = @ARGV;
    }

    usage() if (nelems @Files) && ($Maintainer || $Module || $Files);

    for my $mean (@($Maintainer, $Module)) {
	warn "$^PROGRAM_NAME: Did you mean '$^PROGRAM_NAME $mean'?\n"
	    if $mean && -e $mean && $mean ne '.' && !$Files;
    }

    warn "$^PROGRAM_NAME: Did you mean '$^PROGRAM_NAME -mo $Maintainer'?\n"
	if defined $Maintainer && exists %Modules{$Maintainer};

    warn "$^PROGRAM_NAME: Did you mean '$^PROGRAM_NAME -ma $Module'?\n"
	if defined $Module     && exists %Maintainers{$Module};

    return  @($Maintainer, $Module, $Files, @Files);
}

sub show_results {
    my @($Maintainer, $Module, $Files, @< @Files) =  @_;

    if ($Maintainer) {
	for my $m (sort keys %Maintainers) {
	    if ($m =~ m/$Maintainer/io || %Maintainers{?$m} =~ m/$Maintainer/io) {
		my @modules = get_maintainer_modules($m);
		if ($Module) {
		    @modules = grep { m/$Module/io } @modules;
		}
		if ($Files) {
		    my @files;
		    for my $module ( @modules) {
			push @files, < get_module_files($module);
		    }
		    printf "\%-15s $(join ' ',@files)\n", $m;
		} else {
		    if ($Module) {
			printf "\%-15s $(join ' ',@modules)\n", $m;
		    } else {
			printf "\%-15s %Maintainers{?$m}\n", $m;
		    }
		}
	    }
	}
    } elsif ($Module) {
	for my $m (sort { lc $a cmp lc $b } keys %Modules) {
	    if ($m =~ m/$Module/io) {
		if ($Files) {
		    my @files = get_module_files($m);
		    printf "\%-15s $(join ' ',@files)\n", $m;
		} else {
		    printf "\%-15s %Modules{$m}->{?MAINTAINER}\n", $m;
		}
	    }
	}
    } elsif ($Check) {
        if( (nelems @Files) ) {
	    missing_maintainers( qr{\.(?:[chty]|p[lm]|xs)\z}msx, < @Files)
	}
	else { 
	    duplicated_maintainers();
	}
    } elsif ((nelems @Files)) {
	my %ModuleByFile;

	for ( @Files) { s:^\./:: }
 
	%ModuleByFile{[ @Files]} = @();

	# First try fast match.

	my %ModuleByPat;
	for my $module (keys %Modules) {
	    for my $pat ( get_module_pat($module)) {
		%ModuleByPat{+$pat} = $module;
	    }
	}
	# Expand any globs.
	my %ExpModuleByPat;
	for my $pat (keys %ModuleByPat) {
	    if (-e $pat) {
		%ExpModuleByPat{+$pat} = %ModuleByPat{?$pat};
	    } else {
		for my $exp (@(glob( <$pat))) {
		    %ExpModuleByPat{+$exp} = %ModuleByPat{?$pat};
		}
	    }
	}
	%ModuleByPat = %( < %ExpModuleByPat );
	for my $file ( @Files) {
	    %ModuleByFile{+$file} = %ModuleByPat{?$file}
	        if exists %ModuleByPat{$file};
	}

	# If still unresolved files...
	if (my @ToDo = grep { !defined %ModuleByFile{?$_} } keys %ModuleByFile) {

	    # Cannot match what isn't there.
	    @ToDo = grep { -e $_ } @ToDo;

	    if ((nelems @ToDo)) {
		# Try prefix matching.

		# Remove trailing slashes.
		for ( @ToDo) { s|/$|| }

		my %ToDo;
 		%ToDo{[ @ToDo]} = @();

		for my $pat (keys %ModuleByPat) {
		    last unless keys %ToDo;
		    if (-d $pat) {
			my @Done;
			for my $file (keys %ToDo) {
			    if ($file =~ m|^$pat|i) {
				%ModuleByFile{+$file} = %ModuleByPat{?$pat};
				push @Done, $file;
			    }
			}
			delete %ToDo{[< @Done]};
		    }
		}
	    }
	}

	for my $file ( @Files) {
	    if (defined %ModuleByFile{?$file}) {
		my $module     = %ModuleByFile{?$file};
		my $maintainer = %Modules{%ModuleByFile{?$file}}->{?MAINTAINER};
		printf "\%-15s $module $maintainer %Maintainers{?$maintainer}\n", $file;
	    } else {
		printf "\%-15s ?\n", $file;
	    }
	}
    }
    else {
	usage();
    }
}

my %files;

sub maintainers_files {
    %files = %( () );
    for my $k (keys %Modules) {
	for my $f ( get_module_files($k)) {
	    ++%files{+$f};
	}
    }
}

sub duplicated_maintainers {
    maintainers_files();
    for my $f (keys %files) {
	if (%files{?$f} +> 1) {
	    warn "File $f appears %files{?$f} times in Maintainers.pl\n";
	}
    }
}

sub warn_maintainer {
    my $name = shift;
    warn "File $name has no maintainer\n" if not %files{?$name};
}

sub missing_maintainers {
    my@($check, @< @path) =  @_;
    maintainers_files();
    my @dir;
    for my $d ( @path) {
	if( -d $d ) { push @dir, $d } else { warn_maintainer($d) }
    }
    find sub { warn_maintainer($File::Find::name) if m/$check/; }, < @dir
	if (nelems @dir);
}

1;

