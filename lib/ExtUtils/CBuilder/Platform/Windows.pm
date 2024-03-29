package ExtUtils::CBuilder::Platform::Windows;

use warnings;

use File::Basename;
use File::Spec;

use ExtUtils::CBuilder::Base;

our ($VERSION, @ISA);
$VERSION = '0.22';
@ISA = qw(ExtUtils::CBuilder::Base);

sub new {
  my $class = shift;
  my $self = $class->SUPER::new(< @_);
  my $cf = $self->{?config};

  # Inherit from an appropriate compiler driver class
  unshift @ISA, "ExtUtils::CBuilder::Platform::Windows::" . $self->_compiler_type;

  return $self;
}

sub _compiler_type {
  my $self = shift;
  my $cc = $self->{config}->{?cc};

  return  @(  $cc =~ m/cl(\.exe)?$/ ?? 'MSVC'
	  !! $cc =~ m/bcc32(\.exe)?$/ ?? 'BCC'
	  !! 'GCC');
}

sub split_like_shell(my $self, local $_) {
  
  return @$_ if defined() && UNIVERSAL::isa($_, 'ARRAY');
  return unless defined() && length();
  return  @($_);
}

sub arg_defines($self, %< %args) {
  s/"/\\"/g foreach values %args;
  return map { qq{"-D$_=%args{?$_}"} }, keys %args;
}

sub compile($self, %< %args) {
  my $cf = $self->{?config};

  die "Missing 'source' argument to compile()" unless defined %args{?source};

  my @($basename, $srcdir) =
     (File::Basename::fileparse(%args{?source}, '\.[^.]+$') )[[0..1]];

  $srcdir ||= File::Spec->curdir();

  my @defines = $self->arg_defines( < %{ %args{?defines} || \%() } );

  my %spec = %(
    srcdir      => $srcdir,
    builddir    => $srcdir,
    basename    => $basename,
    source      => %args{?source},
    output      => File::Spec->catfile($srcdir, $basename) . $cf->{?obj_ext},
    cc          => $cf->{?cc},
    cflags      => \@( <
                     $self->split_like_shell($cf->{ccflags}), <
                     $self->split_like_shell($cf->{cccdlflags}), <
                     $self->split_like_shell(%args{extra_compiler_flags}),
                   ),
    optimize    => \ $self->split_like_shell($cf->{optimize}),
    defines     => \@defines,
    includes    => \ @{%args{?include_dirs} || \@()},
    perlinc     => \@( <
                     $self->perl_inc(), <
                     $self->split_like_shell($cf->{incpath}),
                   ),
    use_scripts => 1, # XXX provide user option to change this???
  );

  $self->normalize_filespecs(
    \%spec{+source},
    \%spec{+output},
     %spec{?includes},
     %spec{?perlinc},
  );

  my @cmds = $self->format_compiler_cmd(< %spec);
  while ( my $cmd = shift @cmds ) {
    $self->do_system( < @$cmd )
      or die "error building $cf->{?dlext} file from '%args{?source}'";
  }

  (my $out = %spec{?output}) =~ s/'|"//g;
  return $out;
}

sub need_prelink { 1 }

sub link($self, %< %args) {
  my $cf = $self->{?config};

  my @objects = @( ref %args{?objects} eq 'ARRAY' ?? < @{%args{?objects}} !! %args{?objects} );
  my $to = join '', File::Spec->splitpath(@objects[0])[[0..1]];
  $to ||= File::Spec->curdir();

  (my $file_base = %args{?module_name}) =~ s/.*:://;
  my $output = %args{?lib_file} ||
    File::Spec->catfile($to, "$file_base.$cf->{dlext}");

  # if running in perl source tree, look for libs there, not installed
  my $lddlflags = $cf->{?lddlflags};
  my $perl_src = $self->perl_src();
  $lddlflags =~ s/\Q$cf->{?archlibexp}\E[\\\/]CORE/$perl_src/ if $perl_src;

  my %spec = %(
    srcdir        => $to,
    builddir      => $to,
    startup       => \@( ),
    objects       => \@objects,
    libs          => \@( ),
    output        => $output,
    ld            => $cf->{?ld},
    libperl       => $cf->{?libperl},
    perllibs      => \ $self->split_like_shell($cf->{perllibs}),
    libpath       => \ $self->split_like_shell($cf->{libpth}),
    lddlflags     => \ $self->split_like_shell($lddlflags),
    other_ldflags => \ $self->split_like_shell(%args{?extra_linker_flags} || ''),
    use_scripts   => 1, # XXX provide user option to change this???
  );

  unless ( %spec{?basename} ) {
    (%spec{+basename} = %args{?module_name}) =~ s/.*:://;
  }

  %spec{+srcdir}   = File::Spec->canonpath( %spec{srcdir}   );
  %spec{+builddir} = File::Spec->canonpath( %spec{builddir} );

  %spec{+output}    ||= File::Spec->catfile( %spec{?builddir},
                                            %spec{?basename}  . '.'.$cf->{dlext}   );
  %spec{+manifest}  ||= File::Spec->catfile( %spec{?builddir},
                                            %spec{?basename}  . '.'.$cf->{?dlext}.'.manifest');
  %spec{+implib}    ||= File::Spec->catfile( %spec{?builddir},
                                            %spec{?basename}  . $cf->{lib_ext} );
  %spec{+explib}    ||= File::Spec->catfile( %spec{?builddir},
                                            %spec{?basename}  . '.exp'  );
  if ($cf->{?cc} eq 'cl') {
    %spec{+dbg_file}  ||= File::Spec->catfile( %spec{?builddir},
                                            %spec{?basename}  . '.pdb'  );
  }
  elsif ($cf->{?cc} eq 'bcc32') {
    %spec{+dbg_file}  ||= File::Spec->catfile( %spec{?builddir},
                                            %spec{?basename}  . '.tds'  );
  }
  %spec{+def_file}  ||= File::Spec->catfile( %spec{?srcdir}  ,
                                            %spec{?basename}  . '.def'  );
  %spec{+base_file} ||= File::Spec->catfile( %spec{?srcdir}  ,
                                            %spec{?basename}  . '.base' );

  $self->add_to_cleanup(
    < grep { defined }, @{\ %spec{[qw(manifest implib explib dbg_file def_file base_file map_file)]}}
  );

  foreach my $opt ( qw(output manifest implib explib dbg_file def_file map_file base_file) ) {
    $self->normalize_filespecs( \%spec{+$opt} );
  }

  foreach my $opt ( qw(libpath startup objects) ) {
    $self->normalize_filespecs( %spec{$opt} );
  }

  (my $def_base = %spec{?def_file}) =~ s/'|"//g;
  $def_base =~ s/\.def$//;
  $self->prelink( dl_name => %args{?module_name},
                  dl_file => $def_base,
                  dl_base => %spec{basename} );

  my @cmds = $self->format_linker_cmd(< %spec);
  while ( my $cmd = shift @cmds ) {
    $self->do_system( < @$cmd );
  }

  %spec{+output} =~ s/'|"//g;
  return grep { defined }, %spec{[qw[output manifest implib explib dbg_file def_file map_file base_file]]}
}

# canonize & quote paths
sub normalize_filespecs($self, @< @specs) {
  foreach my $spec ( grep { defined }, @specs ) {
    if ( ref $spec eq 'ARRAY') {
      $self->normalize_filespecs( < map {\$_}, grep { defined }, @$spec )
    } elsif ( ref $spec eq 'SCALAR' ) {
      $$spec =~ s/"//g if $$spec;
      next unless $$spec;
      $$spec = '"' . File::Spec->canonpath($$spec) . '"';
    } elsif ( ref $spec eq '' ) {
      $spec = '"' . File::Spec->canonpath($spec) . '"';
    } else {
      die "Don't know how to normalize " . (ref $spec || $spec) . "\n";
    }
  }
}

# directory of perl's include files
sub perl_inc {
  my $self = shift;

  my $perl_src = $self->perl_src();

  if ($perl_src) {
    File::Spec->catdir($perl_src, "lib", "CORE");
  } else {
    File::Spec->catdir($self->{config}->{?archlibexp},"CORE");
  }
}

1;

########################################################################

=begin comment

The packages below implement functions for generating properly
formatted commandlines for the compiler being used. Each package
defines two primary functions 'format_linker_cmd()' &
'format_compiler_cmd()' that accepts a list of named arguments (a
hash) and returns a list of formatted options suitable for invoking the
compiler. By default, if the compiler supports scripting of its
operation then a script file is built containing the options while
those options are removed from the commandline, and a reference to the
script is pushed onto the commandline in their place. Scripting the
compiler in this way helps to avoid the problems associated with long
commandlines under some shells.

=end comment

=cut

########################################################################
package ExtUtils::CBuilder::Platform::Windows::MSVC;

sub format_compiler_cmd($self, %< %spec) {

    foreach my $path (  @{ %spec{?includes} || \@() }
                        +@+ @{ %spec{?perlinc}  || \@() } ) {
    $path = '-I' . $path;
  }

  %spec = %( < $self->write_compiler_script(< %spec) )
    if %spec{?use_scripts};

  return \ grep {defined && length}, @( (
    %spec{?cc},'-nologo','-c',
    < @{%spec{?includes}}      ,
    < @{%spec{?cflags}}        ,
    < @{%spec{?optimize}}      ,
    < @{%spec{?defines}}       ,
    < @{%spec{?perlinc}}       ,
    "-Fo%spec{?output}"      ,
    %spec{?source}           ,
  ));
}

sub write_compiler_script($self, %< %spec) {

  my $script = File::Spec->catfile( %spec{?srcdir},
                                    %spec{?basename} . '.ccs' );

  $self->add_to_cleanup($script);
  print $^STDOUT, "Generating script '$script'\n" if !$self->{?quiet};

  open( my $scriptfh, ">$script" )
    or die( "Could not create script '$script': $^OS_ERROR" );

  print $scriptfh, join( "\n", map { ref $_ ?? < @{$_} !! $_ },
 grep { defined }, @(
    delete(
      %spec{[ <qw(includes cflags optimize defines perlinc) ]} ))
  );

  close $scriptfh;

  push @{%spec{includes}}, '@"' . $script . '"';

  return %spec;
}

sub format_linker_cmd($self, %< %spec) {
  my $cf = $self->{?config};

  foreach my $path (  @{%spec{libpath}} ) {
    $path = "-libpath:$path";
  }

  my $output = %spec{?output};

  %spec{+def_file}  &&= '-def:'      . %spec{?def_file};
  %spec{+output}    &&= '-out:'      . %spec{?output};
  %spec{+manifest}  &&= '-manifest ' . %spec{?manifest};
  %spec{+implib}    &&= '-implib:'   . %spec{?implib};
  %spec{+map_file}  &&= '-map:'      . %spec{?map_file};

  %spec = %( < $self->write_linker_script(< %spec) )
    if %spec{?use_scripts};

  my @cmds; # Stores the series of commands needed to build the module.

  push @cmds, \ grep {defined && length}, @( (
    %spec{?ld}               ,
    < @{%spec{?lddlflags}}     ,
    < @{%spec{?libpath}}       ,
    < @{%spec{?other_ldflags}} ,
    < @{%spec{?startup}}       ,
    < @{%spec{?objects}}       ,
    %spec{?map_file}         ,
    %spec{?libperl}          ,
    < @{%spec{?perllibs}}      ,
    %spec{?def_file}         ,
    %spec{?implib}           ,
    %spec{?output}           ,
  ));

  # Embed the manifest file for VC 2005 (aka VC 8) or higher, but not for the 64-bit Platform SDK compiler
  if ($cf->{?ivsize} == 4 && $cf->{?cc} eq 'cl' and $cf->{?ccversion} =~ m/^(\d+)/ and $1 +>= 14) {
    push @cmds, \@(
      'mt', '-nologo', %spec{?manifest}, '-outputresource:' . "$output;2"
    );
  }

  return @cmds;
}

sub write_linker_script($self, %< %spec) {

  my $script = File::Spec->catfile( %spec{?srcdir},
                                    %spec{?basename} . '.lds' );

  $self->add_to_cleanup($script);

  print $^STDOUT, "Generating script '$script'\n" if !$self->{?quiet};

  open( my $scriptfh, ">$script" )
    or die( "Could not create script '$script': $^OS_ERROR" );

  print $scriptfh, join( "\n", map { ref $_ ?? < @{$_} !! $_ },
 grep { defined }, @(
    delete(
      %spec{[ <qw(lddlflags libpath other_ldflags
                startup objects libperl perllibs
                def_file implib map_file)            ]} ))
  );

  close $scriptfh;

  push @{%spec{lddlflags}}, '@"' . $script . '"';

  return %spec;
}

1;

########################################################################
package ExtUtils::CBuilder::Platform::Windows::BCC;

sub format_compiler_cmd($self, %< %spec) {

    foreach my $path (  @{ %spec{?includes} || \@() }
                        +@+ @{ %spec{?perlinc}  || \@() } ) {
    $path = '-I' . $path;
  }

  %spec = %( < $self->write_compiler_script(< %spec) )
    if %spec{?use_scripts};

  return \ grep {defined && length}, @( (
    %spec{?cc}, '-c'         ,
    < @{%spec{?includes}}      ,
    < @{%spec{?cflags}}        ,
    < @{%spec{?optimize}}      ,
    < @{%spec{?defines}}       ,
    < @{%spec{?perlinc}}       ,
    "-o%spec{?output}"       ,
    %spec{?source}           ,
  ));
}

sub write_compiler_script($self, %< %spec) {

  my $script = File::Spec->catfile( %spec{?srcdir},
                                    %spec{?basename} . '.ccs' );

  $self->add_to_cleanup($script);

  print $^STDOUT, "Generating script '$script'\n" if !$self->{?quiet};

  open( my $scriptfh, ">$script" )
    or die( "Could not create script '$script': $^OS_ERROR" );

  # XXX Borland "response files" seem to be unable to accept macro
  # definitions containing quoted strings. Escaping strings with
  # backslash doesn't work, and any level of quotes are stripped. The
  # result is is a floating point number in the source file where a
  # string is expected. So we leave the macros on the command line.
  print $scriptfh, join( "\n", map { ref $_ ?? < @{$_} !! $_ },
 grep { defined }, @(
    delete(
      %spec{[ <qw(includes cflags optimize perlinc) ]} ))
  );

  close $scriptfh;

  push @{%spec{includes}}, '@"' . $script . '"';

  return %spec;
}

sub format_linker_cmd($self, %< %spec) {

  foreach my $path (  @{%spec{libpath}} ) {
    $path = "-L$path";
  }

  push( @{%spec{startup}}, 'c0d32.obj' )
    unless ( %spec{?starup} && nelems @{%spec{?startup}} );

  %spec = %( < $self->write_linker_script(< %spec) )
    if %spec{?use_scripts};

  return \ grep {defined && length}, @( (
    %spec{?ld}               ,
    < @{%spec{?lddlflags}}     ,
    < @{%spec{?libpath}}       ,
    < @{%spec{?other_ldflags}} ,
    < @{%spec{?startup}}       ,
    < @{%spec{?objects}}       , ',',
    %spec{?output}           , ',',
    %spec{?map_file}         , ',',
    %spec{?libperl}          ,
    < @{%spec{?perllibs}}      , ',',
    %spec{?def_file}
  ));
}

sub write_linker_script($self, %< %spec) {

  # To work around Borlands "unique" commandline syntax,
  # two scripts are used:

  my $ld_script = File::Spec->catfile( %spec{?srcdir},
                                       %spec{?basename} . '.lds' );
  my $ld_libs   = File::Spec->catfile( %spec{?srcdir},
                                       %spec{?basename} . '.lbs' );

  $self->add_to_cleanup($ld_script, $ld_libs);

  print $^STDOUT, "Generating scripts '$ld_script' and '$ld_libs'.\n" if !$self->{?quiet};

  # Script 1: contains options & names of object files.
  open( my $ld_scriptfh, ">$ld_script" )
    or die( "Could not create linker script '$ld_script': $^OS_ERROR" );

  print $ld_scriptfh, join( " +\n", map { < @{$_} },
 grep { defined }, @(
    delete(
      %spec{[ <qw(lddlflags libpath other_ldflags startup objects) ]} ))
  );

  close $ld_scriptfh;

  # Script 2: contains name of libs to link against.
  open( my $ld_libs_fh, ">$ld_libs" )
    or die( "Could not create linker script '$ld_libs': $^OS_ERROR" );

  print $ld_libs_fh, join( " +\n", @(
     (delete %spec{libperl}  || ''),
    < @{delete %spec{perllibs} || \@()},)
  );

  close $ld_libs_fh;

  push @{%spec{lddlflags}}, '@"' . $ld_script  . '"';
  push @{%spec{perllibs}},  '@"' . $ld_libs    . '"';

  return %spec;
}

1;

########################################################################
package ExtUtils::CBuilder::Platform::Windows::GCC;

sub format_compiler_cmd($self, %< %spec) {

    foreach my $path (  @{ %spec{?includes} || \@() }
                        +@+ @{ %spec{?perlinc}  || \@() } ) {
    $path = '-I' . $path;
  }

  # split off any -arguments included in cc
  my @cc = split m/ (?=-)/, %spec{?cc};

  return \ grep {defined && length}, @( (
    < @cc, '-c'               ,
    < @{%spec{?includes}}      ,
    < @{%spec{?cflags}}        ,
    < @{%spec{?optimize}}      ,
    < @{%spec{?defines}}       ,
    < @{%spec{?perlinc}}       ,
    '-o', %spec{?output}     ,
    %spec{?source}           ,
  ));
}

sub format_linker_cmd($self, %< %spec) {

  # The Config.pm variable 'libperl' is hardcoded to the full name
  # of the perl import library (i.e. 'libperl56.a'). GCC will not
  # find it unless the 'lib' prefix & the extension are stripped.
  %spec{+libperl} =~ s/^(?:lib)?([^.]+).*$/-l$1/;

  unshift( @{%spec{other_ldflags}}, '-nostartfiles' )
    if ( %spec{?startup} && nelems @{%spec{?startup}} );

  # From ExtUtils::MM_Win32:
  #
  ## one thing for GCC/Mingw32:
  ## we try to overcome non-relocateable-DLL problems by generating
  ##    a (hopefully unique) image-base from the dll's name
  ## -- BKS, 10-19-1999
  File::Basename::basename( %spec{?output} ) =~ m/(....)(.{0,4})/;
  %spec{+image_base} = sprintf( "0x\%x0000", unpack('n', $1 ^^^ $2) );

  %spec = %( < $self->write_linker_script(< %spec) )
    if %spec{?use_scripts};

  foreach my $path (  @{%spec{libpath}} ) {
    $path = "-L$path";
  }

  my @cmds; # Stores the series of commands needed to build the module.

  push @cmds, \@(
    'dlltool', '--def'        , %spec{?def_file},
               '--output-exp' , %spec{?explib}
  );

  # split off any -arguments included in ld
  my @ld = split m/ (?=-)/, %spec{?ld};

  push @cmds, \ grep {defined && length}, @( (
    < @ld                       ,
    '-o', %spec{?output}       ,
    "-Wl,--base-file,%spec{?base_file}"   ,
    "-Wl,--image-base,%spec{?image_base}" ,
    < @{%spec{?lddlflags}}       ,
    < @{%spec{?libpath}}         ,
    < @{%spec{?startup}}         ,
    < @{%spec{?objects}}         ,
    < @{%spec{?other_ldflags}}   ,
    %spec{?libperl}            ,
    < @{%spec{?perllibs}}        ,
    %spec{?explib}             ,
    %spec{?map_file} ?? ('-Map', %spec{?map_file}) !! ''
  ));

  push @cmds, \@(
    'dlltool', '--def'        , %spec{?def_file},
               '--output-exp' , %spec{?explib},
               '--base-file'  , %spec{?base_file}
  );

  push @cmds, \ grep {defined && length}, @( (
    < @ld                       ,
    '-o', %spec{?output}       ,
    "-Wl,--image-base,%spec{?image_base}" ,
    < @{%spec{?lddlflags}}       ,
    < @{%spec{?libpath}}         ,
    < @{%spec{?startup}}         ,
    < @{%spec{?objects}}         ,
    < @{%spec{?other_ldflags}}   ,
    %spec{?libperl}            ,
    < @{%spec{?perllibs}}        ,
    %spec{?explib}             ,
    %spec{?map_file} ?? ('-Map', %spec{?map_file}) !! ''
  ));

  return @cmds;
}

sub write_linker_script($self, %< %spec) {

  my $script = File::Spec->catfile( %spec{?srcdir},
                                    %spec{?basename} . '.lds' );

  $self->add_to_cleanup($script);

  print $^STDOUT, "Generating script '$script'\n" if !$self->{?quiet};

  open( my $scriptfh, ">$script" )
    or die( "Could not create script '$script': $^OS_ERROR" );

  print( $scriptfh, 'SEARCH_DIR(' . $_ . ")\n" )
    for  @{delete %spec{libpath} || \@()};

  # gcc takes only one startup file, so the first object in startup is
  # specified as the startup file and any others are shifted into the
  # beginning of the list of objects.
  if ( %spec{?startup} && nelems @{%spec{?startup}} ) {
    print $scriptfh, 'STARTUP(' . shift( @{%spec{startup}} ) . ")\n";
    unshift @{%spec{objects}},
      < @{delete %spec{startup} || \@()};
  }

  print $scriptfh, 'INPUT(' . join( ',', @{delete %spec{objects}  || \@()} ) . ")\n";

  print $scriptfh, 'INPUT(' . join( ' ', @(
     (delete %spec{libperl}  || ''),
    < @{delete %spec{perllibs} || \@()},)
  ) . ")\n";

  close $scriptfh;

  push @{%spec{other_ldflags}}, '"' . $script . '"';

  return %spec;
}

1;

__END__

=head1 NAME

ExtUtils::CBuilder::Platform::Windows - Builder class for Windows platforms

=head1 DESCRIPTION

This module implements the Windows-specific parts of ExtUtils::CBuilder.
Most of the Windows-specific stuff has to do with compiling and
linking C code.  Currently we support the 3 compilers perl itself
supports: MSVC, BCC, and GCC.

This module inherits from C<ExtUtils::CBuilder::Base>, so any functionality
not implemented here will be implemented there.  The interfaces are
defined by the L<ExtUtils::CBuilder> documentation.

=head1 AUTHOR

Ken Williams <ken@mathforum.org>

Most of the code here was written by Randy W. Sims <RandyS@ThePierianSpring.org>.

=head1 SEE ALSO

perl(1), ExtUtils::CBuilder(3), ExtUtils::MakeMaker(3)

=cut
