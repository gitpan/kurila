#! perl -w

 

$::VERSION = '2.006' ;

use private::MakeUtil;
use ExtUtils::MakeMaker v5.16 ;

UpDowngrade( <getPerlFiles('MANIFEST')) 
    unless env::var('PERL_CORE');

WriteMakefile( 
    NAME         => 'Compress::Zlib',
    VERSION_FROM => 'lib/Compress/Zlib.pm',


    PREREQ_PM   => \%( 'Compress::Raw::Zlib'           => $::VERSION,
		                    'IO::Compress::Base'            => $::VERSION,
		                    'IO::Compress::Base::Common'    => $::VERSION,
		                    'IO::Uncompress::Base'          => $::VERSION,
		                    'IO::Compress::Gzip'            => $::VERSION,
		                    'IO::Compress::Gzip::Constants' => $::VERSION,
		                    'IO::Uncompress::Gunzip'        => $::VERSION,
		                    'Scalar::Util'                  => 0,
                          ),
       
    ABSTRACT_FROM => 'lib/Compress/Zlib.pm',
    AUTHOR       => 'Paul Marquess <pmqs@cpan.org>',

    INSTALLDIRS => 'perl',
    LICENSE  => 'perl',

) ;

# end of file Makefile.PL

