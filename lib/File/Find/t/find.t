#!./perl


my %Expect_File = %( () ); # what we expect for $_ 
my %Expect_Name = %( () ); # what we expect for $File::Find::name/fullname
my %Expect_Dir  = %( () ); # what we expect for $File::Find::dir
my $symlink_exists = try { symlink("",""); 1 };
my $warn_msg;

use Carp::Heavy (); # make sure Carp::Heavy is already loaded, because $^INCLUDE_PATH is relative

BEGIN {
    $^WARN_HOOK = sub { $warn_msg = @_[0]; warn "# @_[0]"; }
}

if ( $symlink_exists ) { print $^STDOUT, "1..193\n"; }
else                   { print $^STDOUT, "1..85\n";  }

# Uncomment this to see where File::Find is chdir'ing to.  Helpful for
# debugging its little jaunts around the filesystem.
# BEGIN {
#     use Cwd;
#     *CORE::GLOBAL::chdir = sub ($) { 
#         my($file, $line) = (caller)[1,2];
#
#         printf "# cwd:      %s\n", cwd();
#         print "# chdir: @_ from $file at $line\n";
#         my($return) = CORE::chdir($_[0]);
#         printf "# newcwd:   %s\n", cwd();
#
#         return $return;
#     };
# }


BEGIN {
    use File::Spec;
    if ($^OS_NAME eq 'MSWin32' || $^OS_NAME eq 'cygwin' || $^OS_NAME eq 'VMS')
     {
      # This is a hack - at present File::Find does not produce native names on 
      # Win32 or VMS, so force File::Spec to use Unix names.
      # must be set *before* importing File::Find
      require File::Spec::Unix;
      @File::Spec::ISA = @( 'File::Spec::Unix' );
     }
     require File::Find;
     File::Find->import();
}

cleanup();

$::count_commonsense = 0;
find(\%(wanted => sub { ++$::count_commonsense if $_ eq 'commonsense.t'; } ),
   File::Spec->curdir);
if ($::count_commonsense == 1) {
  print $^STDOUT, "ok 1\n";
} else {
  print $^STDOUT, "not ok 1 # found $::count_commonsense files named 'commonsense.t'\n";
}

$::count_commonsense = 0;
finddepth(\%(wanted => sub { ++$::count_commonsense if $_ eq 'commonsense.t'; } ),
	File::Spec->curdir);
if ($::count_commonsense == 1) {
  print $^STDOUT, "ok 2\n";
} else {
  print $^STDOUT, "not ok 2 # found $::count_commonsense files named 'commonsense.t'\n";
}

my $case = 2;
my $FastFileTests_OK = 0;

sub cleanup {
    if (-d dir_path('for_find')) {
        chdir(dir_path('for_find'));
    }
    if (-d dir_path('fa')) {
	unlink file_path('fa', 'fa_ord'),
	       file_path('fa', 'fsl'),
	       file_path('fa', 'faa', 'faa_ord'),
	       file_path('fa', 'fab', 'fab_ord'),
	       file_path('fa', 'fab', 'faba', 'faba_ord'),
	       file_path('fb', 'fb_ord'),
	       file_path('fb', 'fba', 'fba_ord');
	rmdir dir_path('fa', 'faa');
	rmdir dir_path('fa', 'fab', 'faba');
	rmdir dir_path('fa', 'fab');
	rmdir dir_path('fa');
	rmdir dir_path('fb', 'fba');
	rmdir dir_path('fb');
    }
    chdir(File::Spec->updir);
    if (-d dir_path('for_find')) {
	rmdir dir_path('for_find') or print $^STDOUT, "# Can't rmdir for_find: $^OS_ERROR\n";
    }
}

END {
    cleanup();
}

sub Check($ok) {
    $case++;
    if ($ok) { print $^STDOUT, "ok $case\n"; }
    else       { print $^STDOUT, "not ok $case\n"; }
}

sub CheckDie($ok) {
    $case++;
    if ($ok) { print $^STDOUT, "ok $case\n"; }
    else { print $^STDOUT, "not ok $case\n $^OS_ERROR\n"; exit 0; }
}

sub touch {
    CheckDie( open(my $T,'>',@_[0]) );
}

sub MkDir($dir, $mode) {
    CheckDie( mkdir($dir, $mode) );
}

sub wanted_File_Dir {
    printf $^STDOUT, "# \$File::Find::dir => '$File::Find::dir'\t\$_ => '$_'\n";
    s#\.$## if ($^OS_NAME eq 'VMS' && $_ ne '.');
    s/(.dir)?$//i if ($^OS_NAME eq 'VMS' && -d _);
    Check( %Expect_File{?$_} );
    if ( $FastFileTests_OK ) {
        delete %Expect_File{ $_} 
          unless ( %Expect_Dir{?$_} && ! -d _ );
    } else {
        delete %Expect_File{$_} 
          unless ( %Expect_Dir{?$_} && ! -d $_ );
    }
}

sub wanted_File_Dir_prune {
    &wanted_File_Dir( < @_ );
    $File::Find::prune=1 if  $_ eq 'faba';
}

sub wanted_Name {
    my $n = $File::Find::name;
    $n =~ s#\.$## if ($^OS_NAME eq 'VMS' && $n ne '.');
    print $^STDOUT, "# \$File::Find::name => '$n'\n";
    my $i = rindex($n,'/');
    my $OK = exists(%Expect_Name{$n});
    unless ($^OS_NAME eq 'MacOS') {
        if ( $OK ) {
            $OK= exists(%Expect_Name{substr($n,0,$i)})  if $i +>= 0;    
        }
    }
    Check($OK);
    delete %Expect_Name{$n};
}

sub wanted_File {
    print $^STDOUT, "# \$_ => '$_'\n";
    s#\.$## if ($^OS_NAME eq 'VMS' && $_ ne '.');
    my $i = rindex($_,'/');
    my $OK = exists(%Expect_File{ $_});
    unless ($^OS_NAME eq 'MacOS') {
        if ( $OK ) {
            $OK= exists(%Expect_File{ substr($_,0,$i)})  if $i +>= 0;
        }
    }
    Check($OK);
    delete %Expect_File{ $_};
}

sub simple_wanted {
    print $^STDOUT, "# \$File::Find::dir => '$File::Find::dir'\n";
    print $^STDOUT, "# \$_ => '$_'\n";
}

sub noop_wanted {}

sub my_preprocess {
    my @files = @_;
    print $^STDOUT, "# --preprocess--\n";
    print $^STDOUT, "#   \$File::Find::dir => '$File::Find::dir' \n";
    foreach my $file ( @files) {
        $file =~ s/\.(dir)?$// if $^OS_NAME eq 'VMS';
        print $^STDOUT, "#   $file \n";
        delete %Expect_Dir{ $File::Find::dir }->{$file};
    }
    print $^STDOUT, "# --end preprocess--\n";
    Check((nkeys %{%Expect_Dir{?$File::Find::dir }}) == 0);
    if ((nkeys %{%Expect_Dir{?$File::Find::dir }}) == 0) {
        delete %Expect_Dir{ $File::Find::dir }
    }
    return @files;
}

sub my_postprocess {
    print $^STDOUT, "# postprocess: \$File::Find::dir => '$File::Find::dir' \n";
    delete %Expect_Dir{ $File::Find::dir};
}


# Use dir_path() to specify a directory path that's expected for
# $File::Find::dir (%Expect_Dir). Also use it in file operations like
# chdir, rmdir etc.
#
# dir_path() concatenates directory names to form a *relative*
# directory path, independent from the platform it's run on, although
# there are limitations. Don't try to create an absolute path,
# because that may fail on operating systems that have the concept of
# volume names (e.g. Mac OS). As a special case, you can pass it a "." 
# as first argument, to create a directory path like "./fa/dir" on
# operating systems other than Mac OS (actually, Mac OS will ignore
# the ".", if it's the first argument). If there's no second argument,
# this function will return the empty string on Mac OS and the string
# "./" otherwise.

sub dir_path {
    my $first_arg = shift @_;

    if ($first_arg eq '.') {
        if ($^OS_NAME eq 'MacOS') {
            return '' unless (nelems @_);
            # ignore first argument; return a relative path
            # with leading ":" and with trailing ":"
            return File::Spec->catdir(< @_); 
        } else { # other OS
            return './' unless (nelems @_);
            my $path = File::Spec->catdir(< @_);
            # add leading "./"
            $path = "./$path";
            return $path;
        }

    } else { # $first_arg ne '.'
        return $first_arg unless (nelems @_); # return plain filename
        return File::Spec->catdir($first_arg, < @_); # relative path
    }
}


# Use topdir() to specify a directory path that you want to pass to
# find/finddepth. Basically, topdir() does the same as dir_path() (see
# above), except that there's no trailing ":" on Mac OS.

sub topdir {
    my $path = dir_path(< @_);
    $path =~ s/:$// if ($^OS_NAME eq 'MacOS');
    return $path;
}


# Use file_path() to specify a file path that's expected for $_
# (%Expect_File). Also suitable for file operations like unlink etc.
#
# file_path() concatenates directory names (if any) and a filename to
# form a *relative* file path (the last argument is assumed to be a
# file). It's independent from the platform it's run on, although
# there are limitations. As a special case, you can pass it a "." as 
# first argument, to create a file path like "./fa/file" on operating 
# systems other than Mac OS (actually, Mac OS will ignore the ".", if 
# it's the first argument). If there's no second argument, this 
# function will return the empty string on Mac OS and the string "./" 
# otherwise.

sub file_path {
    my $first_arg = shift @_;

    if ($first_arg eq '.') {
        if ($^OS_NAME eq 'MacOS') {
            return '' unless (nelems @_);
            # ignore first argument; return a relative path  
            # with leading ":", but without trailing ":"
            return File::Spec->catfile(< @_); 
        } else { # other OS
            return './' unless (nelems @_);
            my $path = File::Spec->catfile(< @_);
            # add leading "./" 
            $path = "./$path"; 
            return $path;
        }

    } else { # $first_arg ne '.'
        return $first_arg unless (nelems @_); # return plain filename
        return File::Spec->catfile($first_arg, < @_); # relative path
    }
}


# Use file_path_name() to specify a file path that's expected for
# $File::Find::Name (%Expect_Name). Note: When the no_chdir => 1
# option is in effect, $_ is the same as $File::Find::Name. In that
# case, also use this function to specify a file path that's expected
# for $_.
#
# Basically, file_path_name() does the same as file_path() (see
# above), except that there's always a leading ":" on Mac OS, even for
# plain file/directory names.

sub file_path_name {
    my $path = file_path(< @_);
    $path = ":$path" if (($^OS_NAME eq 'MacOS') && ($path !~ m/:/));
    return $path;
}


MkDir( dir_path('for_find'), 0770 );
CheckDie(chdir( dir_path('for_find')));
MkDir( dir_path('fa'), 0770 );
MkDir( dir_path('fb'), 0770  );
touch( file_path('fb', 'fb_ord') );
MkDir( dir_path('fb', 'fba'), 0770  );
touch( file_path('fb', 'fba', 'fba_ord') );
if ($^OS_NAME eq 'MacOS') {
      CheckDie( symlink(':fb',':fa:fsl') ) if $symlink_exists;
} else {
      CheckDie( symlink('../fb','fa/fsl') ) if $symlink_exists;
}
touch( file_path('fa', 'fa_ord') );

MkDir( dir_path('fa', 'faa'), 0770  );
touch( file_path('fa', 'faa', 'faa_ord') );
MkDir( dir_path('fa', 'fab'), 0770  );
touch( file_path('fa', 'fab', 'fab_ord') );
MkDir( dir_path('fa', 'fab', 'faba'), 0770  );
touch( file_path('fa', 'fab', 'faba', 'faba_ord') );


%Expect_File = %(File::Spec->curdir => 1, file_path('fsl') => 1,
                file_path('fa_ord') => 1, file_path('fab') => 1,
                file_path('fab_ord') => 1, file_path('faba') => 1,
                file_path('faa') => 1, file_path('faa_ord') => 1);

delete %Expect_File{ file_path('fsl') } unless $symlink_exists;
%Expect_Name = %( () );

%Expect_Dir = %( dir_path('fa') => 1, dir_path('faa') => 1,
                dir_path('fab') => 1, dir_path('faba') => 1,
                dir_path('fb') => 1, dir_path('fba') => 1);

delete %Expect_Dir{[@: dir_path('fb'), dir_path('fba') ]} unless $symlink_exists;
File::Find::find( \%(wanted => \&wanted_File_Dir_prune), topdir('fa') ); 
Check( (nkeys %Expect_File) == 0 );


print $^STDOUT, "# check re-entrancy\n";

%Expect_File = %(File::Spec->curdir => 1, file_path('fsl') => 1,
                file_path('fa_ord') => 1, file_path('fab') => 1,
                file_path('fab_ord') => 1, file_path('faba') => 1,
                file_path('faa') => 1, file_path('faa_ord') => 1);

delete %Expect_File{ file_path('fsl') } unless $symlink_exists;
%Expect_Name = %( () );

%Expect_Dir = %( dir_path('fa') => 1, dir_path('faa') => 1,
                dir_path('fab') => 1, dir_path('faba') => 1,
                dir_path('fb') => 1, dir_path('fba') => 1);

delete %Expect_Dir{[@: dir_path('fb'), dir_path('fba') ]} unless $symlink_exists;

File::Find::find( \%(wanted => sub { wanted_File_Dir_prune();
                                    File::Find::find( \%(wanted => sub
                                    {} ), File::Spec->curdir ); } ),
                                    topdir('fa') );

Check( (nkeys %Expect_File) == 0 ); 


# no_chdir is in effect, hence we use file_path_name to specify the expected paths for %Expect_File

%Expect_File = %(file_path_name('fa') => 1,
		file_path_name('fa', 'fsl') => 1,
                file_path_name('fa', 'fa_ord') => 1,
                file_path_name('fa', 'fab') => 1,
		file_path_name('fa', 'fab', 'fab_ord') => 1,
		file_path_name('fa', 'fab', 'faba') => 1,
		file_path_name('fa', 'fab', 'faba', 'faba_ord') => 1,
		file_path_name('fa', 'faa') => 1,
                file_path_name('fa', 'faa', 'faa_ord') => 1,);

delete %Expect_File{ file_path_name('fa', 'fsl') } unless $symlink_exists;
%Expect_Name = %( () );

%Expect_Dir = %(dir_path('fa') => 1,
	       dir_path('fa', 'faa') => 1,
               dir_path('fa', 'fab') => 1,
	       dir_path('fa', 'fab', 'faba') => 1,
	       dir_path('fb') => 1,
	       dir_path('fb', 'fba') => 1);

delete %Expect_Dir{[@: dir_path('fb'), dir_path('fb', 'fba') ]}
    unless $symlink_exists;

File::Find::find( \%(wanted => \&wanted_File_Dir, no_chdir => 1),
		  topdir('fa') ); Check( (nkeys %Expect_File) == 0 );


%Expect_File = %( () );

%Expect_Name = %(File::Spec->curdir => 1,
		file_path_name('.', 'fa') => 1,
                file_path_name('.', 'fa', 'fsl') => 1,
                file_path_name('.', 'fa', 'fa_ord') => 1,
                file_path_name('.', 'fa', 'fab') => 1,
                file_path_name('.', 'fa', 'fab', 'fab_ord') => 1,
                file_path_name('.', 'fa', 'fab', 'faba') => 1,
                file_path_name('.', 'fa', 'fab', 'faba', 'faba_ord') => 1,
                file_path_name('.', 'fa', 'faa') => 1,
                file_path_name('.', 'fa', 'faa', 'faa_ord') => 1,
                file_path_name('.', 'fb') => 1,
		file_path_name('.', 'fb', 'fba') => 1,
		file_path_name('.', 'fb', 'fba', 'fba_ord') => 1,
		file_path_name('.', 'fb', 'fb_ord') => 1);

delete %Expect_Name{ file_path('.', 'fa', 'fsl') } unless $symlink_exists;
%Expect_Dir = %( () ); 
File::Find::finddepth( \%(wanted => \&wanted_Name), File::Spec->curdir );
Check( (nkeys %Expect_Name) == 0 );


# no_chdir is in effect, hence we use file_path_name to specify the
# expected paths for %Expect_File

%Expect_File = %(File::Spec->curdir => 1,
		file_path_name('.', 'fa') => 1,
                file_path_name('.', 'fa', 'fsl') => 1,
                file_path_name('.', 'fa', 'fa_ord') => 1,
                file_path_name('.', 'fa', 'fab') => 1,
                file_path_name('.', 'fa', 'fab', 'fab_ord') => 1,
                file_path_name('.', 'fa', 'fab', 'faba') => 1,
                file_path_name('.', 'fa', 'fab', 'faba', 'faba_ord') => 1,
                file_path_name('.', 'fa', 'faa') => 1,
                file_path_name('.', 'fa', 'faa', 'faa_ord') => 1,
                file_path_name('.', 'fb') => 1,
		file_path_name('.', 'fb', 'fba') => 1,
		file_path_name('.', 'fb', 'fba', 'fba_ord') => 1,
		file_path_name('.', 'fb', 'fb_ord') => 1);

delete %Expect_File{ file_path_name('.', 'fa', 'fsl') } unless $symlink_exists;
%Expect_Name = %( () );
%Expect_Dir = %( () ); 

File::Find::finddepth( \%(wanted => \&wanted_File, no_chdir => 1),
		     File::Spec->curdir );

Check( (nkeys %Expect_File) == 0 );


print $^STDOUT, "# check preprocess\n";
%Expect_File = %( () );
%Expect_Name = %( () );
%Expect_Dir = %(
          File::Spec->curdir                 => \%(fa => 1, fb => 1), 
          dir_path('.', 'fa')                => \%(faa => 1, fab => 1, fa_ord => 1),
          dir_path('.', 'fa', 'faa')         => \%(faa_ord => 1),
          dir_path('.', 'fa', 'fab')         => \%(faba => 1, fab_ord => 1),
          dir_path('.', 'fa', 'fab', 'faba') => \%(faba_ord => 1),
          dir_path('.', 'fb')                => \%(fba => 1, fb_ord => 1),
          dir_path('.', 'fb', 'fba')         => \%(fba_ord => 1)
          );

File::Find::find( \%(wanted => \&noop_wanted,
		   preprocess => \&my_preprocess), File::Spec->curdir );

Check( (nkeys %Expect_Dir) == 0 );


print $^STDOUT, "# check postprocess\n";
%Expect_File = %( () );
%Expect_Name = %( () );
%Expect_Dir = %(
          File::Spec->curdir                 => 1,
          dir_path('.', 'fa')                => 1,
          dir_path('.', 'fa', 'faa')         => 1,
          dir_path('.', 'fa', 'fab')         => 1,
          dir_path('.', 'fa', 'fab', 'faba') => 1,
          dir_path('.', 'fb')                => 1,
          dir_path('.', 'fb', 'fba')         => 1
          );

File::Find::find( \%(wanted => \&noop_wanted,
		   postprocess => \&my_postprocess), File::Spec->curdir );

Check( (nkeys %Expect_Dir) == 0 );

do {
    print $^STDOUT, "# checking argument localization\n";

    ### this checks the fix of perlbug [19977] ###
    my @foo = qw( a b c d e f );
    my %pre = %( < map { $_ => }, @foo );

    File::Find::find( sub {  } , 'fa' ) for  @foo;
    delete %pre{$_} for  @foo;

    Check( ( nkeys %pre ) == 0 );
};

if ( $symlink_exists ) {
    print $^STDOUT, "# --- symbolic link tests --- \n";
    $FastFileTests_OK= 1;


    # Verify that File::Find::find will call wanted even if the topdir of
    # is a symlink to a directory, and it shouldn't follow the link
    # unless follow is set, which it isn't in this case
    %Expect_File = %( file_path('fsl') => 1 );
    %Expect_Name = %( () );
    %Expect_Dir = %( () );
    File::Find::find( \%(wanted => \&wanted_File_Dir), topdir('fa', 'fsl') );
    Check( (nkeys %Expect_File) == 0 );

 
    %Expect_File = %(File::Spec->curdir => 1, file_path('fa_ord') => 1,
                    file_path('fsl') => 1, file_path('fb_ord') => 1,
                    file_path('fba') => 1, file_path('fba_ord') => 1,
                    file_path('fab') => 1, file_path('fab_ord') => 1,
                    file_path('faba') => 1, file_path('faa') => 1,
                    file_path('faa_ord') => 1);

    %Expect_Name = %( () );

    %Expect_Dir = %(File::Spec->curdir => 1, dir_path('fa') => 1,
                   dir_path('faa') => 1, dir_path('fab') => 1,
                   dir_path('faba') => 1, dir_path('fb') => 1,
                   dir_path('fba') => 1);

    File::Find::find( \%(wanted => \&wanted_File_Dir_prune,
		       follow_fast => 1), topdir('fa') );

    Check( (nkeys %Expect_File) == 0 );  


    # no_chdir is in effect, hence we use file_path_name to specify
    # the expected paths for %Expect_File

    %Expect_File = %(file_path_name('fa') => 1,
		    file_path_name('fa', 'fa_ord') => 1,
		    file_path_name('fa', 'fsl') => 1,
                    file_path_name('fa', 'fsl', 'fb_ord') => 1,
                    file_path_name('fa', 'fsl', 'fba') => 1,
                    file_path_name('fa', 'fsl', 'fba', 'fba_ord') => 1,
                    file_path_name('fa', 'fab') => 1,
                    file_path_name('fa', 'fab', 'fab_ord') => 1,
                    file_path_name('fa', 'fab', 'faba') => 1,
                    file_path_name('fa', 'fab', 'faba', 'faba_ord') => 1,
                    file_path_name('fa', 'faa') => 1,
                    file_path_name('fa', 'faa', 'faa_ord') => 1);

    %Expect_Name = %( () );

    %Expect_Dir = %(dir_path('fa') => 1,
		   dir_path('fa', 'faa') => 1,
                   dir_path('fa', 'fab') => 1,
		   dir_path('fa', 'fab', 'faba') => 1,
		   dir_path('fb') => 1,
		   dir_path('fb', 'fba') => 1);

    File::Find::find( \%(wanted => \&wanted_File_Dir, follow_fast => 1,
		       no_chdir => 1), topdir('fa') );

    Check( (nkeys %Expect_File) == 0 );

    %Expect_File = %( () );

    %Expect_Name = %(file_path_name('fa') => 1,
		    file_path_name('fa', 'fa_ord') => 1,
		    file_path_name('fa', 'fsl') => 1,
                    file_path_name('fa', 'fsl', 'fb_ord') => 1,
                    file_path_name('fa', 'fsl', 'fba') => 1,
                    file_path_name('fa', 'fsl', 'fba', 'fba_ord') => 1,
                    file_path_name('fa', 'fab') => 1,
                    file_path_name('fa', 'fab', 'fab_ord') => 1,
                    file_path_name('fa', 'fab', 'faba') => 1,
                    file_path_name('fa', 'fab', 'faba', 'faba_ord') => 1,
                    file_path_name('fa', 'faa') => 1,
                    file_path_name('fa', 'faa', 'faa_ord') => 1);

    %Expect_Dir = %( () );

    File::Find::finddepth( \%(wanted => \&wanted_Name,
			    follow_fast => 1), topdir('fa') );

    Check( (nkeys %Expect_Name) == 0 );

    # no_chdir is in effect, hence we use file_path_name to specify
    # the expected paths for %Expect_File

    %Expect_File = %(file_path_name('fa') => 1,
		    file_path_name('fa', 'fa_ord') => 1,
		    file_path_name('fa', 'fsl') => 1,
                    file_path_name('fa', 'fsl', 'fb_ord') => 1,
                    file_path_name('fa', 'fsl', 'fba') => 1,
                    file_path_name('fa', 'fsl', 'fba', 'fba_ord') => 1,
                    file_path_name('fa', 'fab') => 1,
                    file_path_name('fa', 'fab', 'fab_ord') => 1,
                    file_path_name('fa', 'fab', 'faba') => 1,
                    file_path_name('fa', 'fab', 'faba', 'faba_ord') => 1,
                    file_path_name('fa', 'faa') => 1,
                    file_path_name('fa', 'faa', 'faa_ord') => 1);

    %Expect_Name = %( () );
    %Expect_Dir = %( () );

    File::Find::finddepth( \%(wanted => \&wanted_File, follow_fast => 1,
			    no_chdir => 1), topdir('fa') );

    Check( (nkeys %Expect_File) == 0 );     

 
    print $^STDOUT, "# check dangling symbolic links\n";
    MkDir( dir_path('dangling_dir'), 0770 );
    CheckDie( symlink( dir_path('dangling_dir'),
		       file_path('dangling_dir_sl') ) );
    rmdir dir_path('dangling_dir');
    touch(file_path('dangling_file'));  
    if ($^OS_NAME eq 'MacOS') {
        CheckDie( symlink('dangling_file', ':fa:dangling_file_sl') );
    } else {
        CheckDie( symlink('../dangling_file','fa/dangling_file_sl') );
    }      
    unlink file_path('dangling_file');

    do { 
        # these tests should also emit a warning
	use warnings;

        %Expect_File = %(File::Spec->curdir => 1,
	                file_path('dangling_file_sl') => 1,
			file_path('fa_ord') => 1,
                        file_path('fsl') => 1,
                        file_path('fb_ord') => 1,
			file_path('fba') => 1,
                        file_path('fba_ord') => 1,
			file_path('fab') => 1,
                        file_path('fab_ord') => 1,
                        file_path('faba') => 1,
			file_path('faba_ord') => 1,
                        file_path('faa') => 1,
                        file_path('faa_ord') => 1);

        %Expect_Name = %( () );
        %Expect_Dir = %( () );
        undef $warn_msg;

        File::Find::find( \%(wanted => \&wanted_File, follow => 1,
			   dangling_symlinks =>
			       sub { $warn_msg = "@_[0] is a dangling symbolic link" }
                           ),
                           topdir('dangling_dir_sl'), topdir('fa') );

        Check( (nkeys %Expect_File) == 0 );
        Check( $warn_msg =~ m|dangling_file_sl is a dangling symbolic link| );  
        unlink file_path('fa', 'dangling_file_sl'),
                         file_path('dangling_dir_sl');

    };


    print $^STDOUT, "# check recursion\n";
    if ($^OS_NAME eq 'MacOS') {
        CheckDie( symlink(':fa:faa',':fa:faa:faa_sl') );
    } else {
        CheckDie( symlink('../faa','fa/faa/faa_sl') );
    }
    undef $^EVAL_ERROR;
    try {File::Find::find( \%(wanted => \&simple_wanted, follow => 1,
                             no_chdir => 1), topdir('fa') ); };
    Check( $^EVAL_ERROR->{?description} =~ m|for_find[:/]fa[:/]faa[:/]faa_sl is a recursive symbolic link|i );  
    unlink file_path('fa', 'faa', 'faa_sl'); 


    print $^STDOUT, "# check follow_skip (file)\n";
    if ($^OS_NAME eq 'MacOS') {
        CheckDie( symlink(':fa:fa_ord',':fa:fa_ord_sl') ); # symlink to a file
    } else {
        CheckDie( symlink('./fa_ord','fa/fa_ord_sl') ); # symlink to a file
    }
    undef $^EVAL_ERROR;

    try {File::Find::finddepth( \%(wanted => \&simple_wanted,
                                  follow => 1,
                                  follow_skip => 0, no_chdir => 1),
                                  topdir('fa') );};

    Check( $^EVAL_ERROR->{?description} =~ m|for_find[:/]fa[:/]fa_ord encountered a second time|i );


    # no_chdir is in effect, hence we use file_path_name to specify
    # the expected paths for %Expect_File

    %Expect_File = %(file_path_name('fa') => 1,
		    file_path_name('fa', 'fa_ord') => 2,
		    # We may encounter the symlink first
		    file_path_name('fa', 'fa_ord_sl') => 2,
		    file_path_name('fa', 'fsl') => 1,
                    file_path_name('fa', 'fsl', 'fb_ord') => 1,
                    file_path_name('fa', 'fsl', 'fba') => 1,
                    file_path_name('fa', 'fsl', 'fba', 'fba_ord') => 1,
                    file_path_name('fa', 'fab') => 1,
                    file_path_name('fa', 'fab', 'fab_ord') => 1,
                    file_path_name('fa', 'fab', 'faba') => 1,
                    file_path_name('fa', 'fab', 'faba', 'faba_ord') => 1,
                    file_path_name('fa', 'faa') => 1,
                    file_path_name('fa', 'faa', 'faa_ord') => 1);

    %Expect_Name = %( () );

    %Expect_Dir = %(dir_path('fa') => 1,
		   dir_path('fa', 'faa') => 1,
                   dir_path('fa', 'fab') => 1,
		   dir_path('fa', 'fab', 'faba') => 1,
		   dir_path('fb') => 1,
		   dir_path('fb','fba') => 1);

    File::Find::finddepth( \%(wanted => \&wanted_File_Dir, follow => 1,
                           follow_skip => 1, no_chdir => 1),
                           topdir('fa') );
    Check( (nkeys %Expect_File) == 0 );
    unlink file_path('fa', 'fa_ord_sl');


    print $^STDOUT, "# check follow_skip (directory)\n";
    if ($^OS_NAME eq 'MacOS') {
        CheckDie( symlink(':fa:faa',':fa:faa_sl') ); # symlink to a directory
    } else {
        CheckDie( symlink('./faa','fa/faa_sl') ); # symlink to a directory
    }
    undef $^EVAL_ERROR;

    try {File::Find::find( \%(wanted => \&simple_wanted, follow => 1,
                            follow_skip => 0, no_chdir => 1),
                            topdir('fa') );};

    Check( $^EVAL_ERROR->{?description} =~ m|for_find[:/]fa[:/]faa[:/]? encountered a second time|i );

  
    undef $^EVAL_ERROR;

    try {File::Find::find( \%(wanted => \&simple_wanted, follow => 1,
                            follow_skip => 1, no_chdir => 1),
                            topdir('fa') );};

    Check( $^EVAL_ERROR->{?description} =~ m|for_find[:/]fa[:/]faa[:/]? encountered a second time|i );  

    # no_chdir is in effect, hence we use file_path_name to specify
    # the expected paths for %Expect_File

    %Expect_File = %(file_path_name('fa') => 1,
		    file_path_name('fa', 'fa_ord') => 1,
		    file_path_name('fa', 'fsl') => 1,
                    file_path_name('fa', 'fsl', 'fb_ord') => 1,
                    file_path_name('fa', 'fsl', 'fba') => 1,
                    file_path_name('fa', 'fsl', 'fba', 'fba_ord') => 1,
                    file_path_name('fa', 'fab') => 1,
                    file_path_name('fa', 'fab', 'fab_ord') => 1,
                    file_path_name('fa', 'fab', 'faba') => 1,
                    file_path_name('fa', 'fab', 'faba', 'faba_ord') => 1,
                    file_path_name('fa', 'faa') => 1,
                    file_path_name('fa', 'faa', 'faa_ord') => 1,
		    # We may actually encounter the symlink first.
                    file_path_name('fa', 'faa_sl') => 1,
                    file_path_name('fa', 'faa_sl', 'faa_ord') => 1);

    %Expect_Name = %( () );

    %Expect_Dir = %(dir_path('fa') => 1,
		   dir_path('fa', 'faa') => 1,
                   dir_path('fa', 'fab') => 1,
		   dir_path('fa', 'fab', 'faba') => 1,
		   dir_path('fb') => 1,
		   dir_path('fb', 'fba') => 1);

    File::Find::find( \%(wanted => \&wanted_File_Dir, follow => 1,
		       follow_skip => 2, no_chdir => 1), topdir('fa') );

    # If we encountered the symlink first, then the entries corresponding to
    # the real name remain, if the real name first then the symlink
    my @names = sort keys %Expect_File;
    Check( (nelems @names) == 1 );
    # Normalise both to the original name
    s/_sl// foreach  @names;
    Check (@names[0] eq file_path_name('fa', 'faa', 'faa_ord'));
    unlink file_path('fa', 'faa_sl');

}
