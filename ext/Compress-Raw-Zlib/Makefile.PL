#! perl -w

 

use private::MakeUtil;
use ExtUtils::MakeMaker v5.16 ;
use ExtUtils::Install (); # only needed to check for version

my $ZLIB_LIB ;
my $ZLIB_INCLUDE ;
my $BUILD_ZLIB = 0 ;
my $OLD_ZLIB = '' ;
my $WALL = '' ;
my $GZIP_OS_CODE = -1 ;
my $USE_PPPORT_H = (env::var('PERL_CORE')) ?? '' !! '-DUSE_PPPORT_H';

#$WALL = ' -pedantic ' if $Config{'cc'} =~ /gcc/ ;
#$WALL = ' -Wall -Wno-comment ' if $Config{'cc'} =~ /gcc/ ;

# Ticket #18986 says that ExtUtils::Install 1.39 fixes the in-use issue
# on win32/cygwin, so make the code below conditional on the version of
# ExtUtils::Install.

# Don't ask if MM_USE_DEFAULT is set -- enables perl core building on cygwin
if ($^OS_NAME =~ m/cygwin/i and $ExtUtils::Install::VERSION +< 1.39 
        and not (env::var('PERL_MM_USE_DEFAULT') or env::var('PERL_CORE')))
{
    print $^STDOUT, <<EOM ;

I see you are running Cygwin.

Please note that this module cannot be installed on Cygwin using the CPAN
shell. The CPAN Shell uses Compress::Raw::Zlib internally and it is not
possible to delete an active DLL.

If you are running the CPAN shell, please exit it and install this module
by hand by running 'make install' under the directory

    ~/.cpan/build/Compress-Raw-Zlib-VERSION

EOM

    print $^STDOUT, "Do you want to continue? [Y/N]: " ;
    my $answer = ~< $^STDIN ;

    if ($answer =~ m/^yes|y/i)
    {
	print $^STDOUT, "continuing...\n" 
    }
    else
    {
	print $^STDOUT, "exiting...\n" ;
	exit 1 ;
    }


}

ParseCONFIG() ;

UpDowngrade( <getPerlFiles('MANIFEST')) 
    unless env::var('PERL_CORE');

WriteMakefile( 
    NAME         => 'Compress::Raw::Zlib',
    VERSION_FROM => 'lib/Compress/Raw/Zlib.pm',
    INC          => "-I$ZLIB_INCLUDE" ,
    DEFINE       => "$OLD_ZLIB $WALL -DGZIP_OS_CODE=$GZIP_OS_CODE $USE_PPPORT_H" ,
    XS           => \%( 'Zlib.xs' => 'Zlib.c'),
    'depend'     => \%( 'Makefile'   => 'config.in' ),
    'clean'      => \%( FILES        => '*.c constants.h constants.xs' ),

    (
      $BUILD_ZLIB
        ?? < zlib_files($ZLIB_LIB)
        !! (LIBS => \@( "-L$ZLIB_LIB -lz " ))
    ),
      
    ABSTRACT_FROM => 'lib/Compress/Raw/Zlib.pm',
    AUTHOR       => 'Paul Marquess <pmqs@cpan.org>',

    INSTALLDIRS => ('perl'),
    LICENSE  => 'perl',

) ;

my @names = qw(

    DEF_WBITS
    MAX_MEM_LEVEL
    MAX_WBITS
    OS_CODE

    Z_ASCII
    Z_BEST_COMPRESSION
    Z_BEST_SPEED
    Z_BINARY
    Z_BLOCK
    Z_BUF_ERROR
    Z_DATA_ERROR
    Z_DEFAULT_COMPRESSION
    Z_DEFAULT_STRATEGY
    Z_DEFLATED
    Z_ERRNO
    Z_FILTERED
    Z_FINISH
    Z_FIXED
    Z_FULL_FLUSH
    Z_HUFFMAN_ONLY
    Z_MEM_ERROR
    Z_NEED_DICT
    Z_NO_COMPRESSION
    Z_NO_FLUSH
    Z_NULL
    Z_OK
    Z_PARTIAL_FLUSH
    Z_RLE
    Z_STREAM_END
    Z_STREAM_ERROR
    Z_SYNC_FLUSH
    Z_UNKNOWN
    Z_VERSION_ERROR

);
    #ZLIB_VERNUM

require ExtUtils::Constant;

do {
    # Check the constants above all appear in @EXPORT in Zlib.pm
    my %names = %( < @+: map { @: $_, 1 }, @( < @names, 'ZLIB_VERSION') );
    open my $f, "<", "lib/Compress/Raw/Zlib.pm" or die "Cannot open Zlib.pm: $^OS_ERROR\n";
    while (~< $f)
    {
        last if m/^ \s* \@EXPORT \s+ = \s+ qw \( /x ;
    }

    while (~< $f)
    {
        last if m/^\s*\)/ ;
        m/(\S+)/ ;
        delete %names{$1} if defined $1 ;
    }
    close $f ;

    if ( %names )
    {
        my $missing = join ("\n\t", sort keys %names) ;
        die "The following names are missing from \@EXPORT in Zlib.pm\n" .
            "\t$missing\n" ;
    }
    
    push @names, \%( name => 'ZLIB_VERSION', type => 'PV' );

    ExtUtils::Constant::WriteConstants(
                                       NAME => 'Compress::Raw::Zlib',
                                       NAMES => \@names,
                                       C_FILE  => 'constants.h',
                                       XS_FILE  => 'constants.xs',
                                       PROXYSUBS => 1,
                                    );
}; 

sub ParseCONFIG
{
    my ($k, $v) ;
    my @badkey = @( () ) ;
    my %Info = %( () ) ;
    my @Options = qw( INCLUDE LIB BUILD_ZLIB OLD_ZLIB GZIP_OS_CODE ) ;
    my %ValidOption = %( < @+: map { @: $_, 1 }, @Options ) ;
    my %Parsed = %( < %ValidOption ) ;
    my $CONFIG = 'config.in' ;

    print $^STDOUT, "Parsing $CONFIG...\n" ;

    open(my $f, "<", "$CONFIG") or die "Cannot open file $CONFIG: $^OS_ERROR\n" ;
    while (~< $f) {
	s/^\s*|\s*$//g ;
	next if m/^\s*$/ or m/^\s*#/ ;
	s/\s*#\s*$// ;

	@($k, $v) =  split(m/\s+=\s+/, $_, 2) ;
	$k = uc $k ;
	if (%ValidOption{?$k}) {
	    delete %Parsed{$k} ;
	    %Info{+$k} = $v ;
	}
	else {
	    push(@badkey, $k) ;
	}
    }
    close $f ;

    print $^STDOUT, "Unknown keys in $CONFIG ignored [$(join ' ',@badkey)]\n"
	if (nelems @badkey) ;

    # check parsed values
    my @missing = @( () ) ;
    die "The following keys are missing from $CONFIG  [$(join ' ',@missing)]\n" 
        if @missing = keys %Parsed ;

    $ZLIB_INCLUDE = env::var('ZLIB_INCLUDE') || %Info{?'INCLUDE'} ;
    $ZLIB_LIB = env::var('ZLIB_LIB') || %Info{?'LIB'} ;

    if ($^OS_NAME eq 'VMS') {
        $ZLIB_INCLUDE = VMS::Filespec::vmspath($ZLIB_INCLUDE);
        $ZLIB_LIB = VMS::Filespec::vmspath($ZLIB_LIB);
    }

    my $y = env::var('OLD_ZLIB') || %Info{?'OLD_ZLIB'} ;
    $OLD_ZLIB = '-DOLD_ZLIB' if $y and $y =~ m/^yes|on|true|1$/i;

    my $x = env::var('BUILD_ZLIB') || %Info{?'BUILD_ZLIB'} ;

    if ($x and $x =~ m/^yes|on|true|1$/i ) {

        $BUILD_ZLIB = 1 ;

	# ZLIB_LIB & ZLIB_INCLUDE must point to the same place when 
	# BUILD_ZLIB is specified.
	die "INCLUDE & LIB must be the same when BUILD_ZLIB is True\n"
	    if $ZLIB_LIB ne $ZLIB_INCLUDE ;

	# Check the zlib source directory exists
	die "LIB/INCLUDE directory '$ZLIB_LIB' does not exits\n"
	   unless -d $ZLIB_LIB ;

	# check for a well known file
	die "LIB/INCLUDE directory, '$ZLIB_LIB', doesn't seem to have the zlib source files\n"
	   unless -e catfile($ZLIB_LIB, 'zlib.h') ;


	# write the Makefile
	print $^STDOUT, "Building Zlib enabled\n" ;
    }

    $GZIP_OS_CODE = defined env::var('GZIP_OS_CODE') 
                          ?? env::var('GZIP_OS_CODE') 
                          !! %Info{?'GZIP_OS_CODE'} ;

	die "GZIP_OS_CODE not 'AUTO_DETECT' or a number between 0 and 255\n"
	   unless uc $GZIP_OS_CODE eq 'AUTO_DETECT'
                    || ( $GZIP_OS_CODE =~ m/^(\d+)$/ && $1 +>= 0 && $1 +<= 255) ;

    if (uc $GZIP_OS_CODE eq 'AUTO_DETECT')
    {
        print $^STDOUT, "Auto Detect Gzip OS Code..\n" ;
        $GZIP_OS_CODE = getOSCode() ;
    }
    
    my $name = getOSname($GZIP_OS_CODE);
    print $^STDOUT, "Setting Gzip OS Code to $GZIP_OS_CODE [$name]\n" ;

    print $^STDOUT, <<EOM if 0 ;
    INCLUDE         [$ZLIB_INCLUDE]
    LIB             [$ZLIB_LIB]
    GZIP_OS_CODE    [$GZIP_OS_CODE]
    OLD_ZLIB        [$OLD_ZLIB]
    BUILD_ZLIB      [$BUILD_ZLIB]

EOM

    print $^STDOUT, "Looks Good.\n" ;

}



sub zlib_files
{
    my $dir = shift ;

    my @h_files = @( () );
    my @c_files = @( () );
    
    if (-f catfile($dir, "infback.c")) {
        # zlib 1.2.0 or greater
        #
        @h_files = qw(crc32.h    inffast.h inflate.h  trees.h    zconf.in.h 
    	              zutil.h    deflate.h inffixed.h inftrees.h zconf.h  
    		      zlib.h 
    		 );
        @c_files = qw(adler32  crc32   infback  inflate  uncompr
    		      compress deflate inffast  inftrees  
    		      trees    zutil 
    		 );
    }
    else {
        # zlib 1.1.x
    
        @h_files = qw(deflate.h  infcodes.h inftrees.h zconf.h zutil.h
    		      infblock.h inffast.h  infutil.h  zlib.h
    		 );
        @c_files = qw(adler32  compress crc32    uncompr
    		      deflate  trees    zutil    inflate infblock
    		      inftrees infcodes infutil  inffast
    		 );
    }
    
    @h_files = map { catfile($dir, $_)  }, @h_files ;
    my @o_files = map { "$_\$(OBJ_EXT)" }, @( 'Zlib', < @c_files);
    @c_files = map { "$_.c" }, @( 'Zlib', < @c_files) ;

    foreach my $file ( @c_files)
      { copy( catfile($dir, $file), '.') }
    
    return  @(
        #'H'         =>  [ @h_files ],
    	'C'         =>  \ @c_files ,
        #'OBJECT'    => qq[ @o_files ],
        'OBJECT'    => q[ $(O_FILES) ],
        

           ) ;
}



our (@GZIP_OS_Names,  %OSnames) ;

BEGIN
{
  @GZIP_OS_Names = @(
    \@( ''        => 0,    'MS-DOS'                       ),
    \@( 'amigaos' => 1,    'Amiga'                        ),
    \@( 'VMS'     => 2,    'VMS'                          ),
    \@( ''        => 3,    'Unix/Default'                 ),
    \@( ''        => 4,    'VM/CMS'                       ),
    \@( ''        => 5,    'Atari TOS'                    ),
    \@( 'os2'     => 6,    'HPFS (OS/2, NT)'              ),
    \@( 'MacOS'   => 7,    'Macintosh'                    ),
    \@( ''        => 8,    'Z-System'                     ),
    \@( ''        => 9,    'CP/M'                         ),
    \@( ''        => 10,   'TOPS-20'                      ),
    \@( ''        => 11,   'NTFS (NT)'                    ),
    \@( ''        => 12,   'SMS QDOS'                     ),
    \@( ''        => 13,   'Acorn RISCOS'                 ),
    \@( 'MSWin32' => 14,   'VFAT file system (Win95, NT)' ),
    \@( ''        => 15,   'MVS'                          ),
    \@( 'beos'    => 16,   'BeOS'                         ),
    \@( ''        => 17,   'Tandem/NSK'                   ),
    \@( ''        => 18,   'THEOS'                        ),
    \@( ''        => 255,  'Unknown OS'                   ),
  );

  %OSnames = %( < @+: map { @: @$_[1] => @$_[2] },
 @GZIP_OS_Names ) ;
}

sub getOSCode
{
    my $default = 3 ; # Unix is the default

    my $uname = $^OS_NAME;

    for my $h ( @GZIP_OS_Names)
    {
        my @($pattern, $code, $name) =  @$h;

        return $code
            if $pattern && $uname eq $pattern ;
    }

    return $default ;
}

sub getOSname
{
    my $code = shift ;

    return %OSnames{?$code} || 'Unknown OS' ;
}

# end of file Makefile.PL

