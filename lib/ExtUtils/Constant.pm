package ExtUtils::Constant;
use vars qw (@ISA $VERSION @EXPORT_OK %EXPORT_TAGS);
$VERSION = 0.20;

=head1 NAME

ExtUtils::Constant - generate XS code to import C header constants

=head1 SYNOPSIS

    use ExtUtils::Constant qw (WriteConstants);
    WriteConstants(
        NAME => 'Foo',
        NAMES => [qw(FOO BAR BAZ)],
    );
    # Generates wrapper code to make the values of the constants FOO BAR BAZ
    #  available to perl

=head1 DESCRIPTION

ExtUtils::Constant facilitates generating C and XS wrapper code to allow
perl modules to AUTOLOAD constants defined in C library header files.
It is principally used by the C<h2xs> utility, on which this code is based.
It doesn't contain the routines to scan header files to extract these
constants.

=head1 USAGE

Generally one only needs to call the C<WriteConstants> function, and then

    #include "const-c.inc"

in the C section of C<Foo.xs>

    INCLUDE: const-xs.inc

in the XS section of C<Foo.xs>.

For greater flexibility use C<constant_types()>, C<C_constant> and
C<XS_constant>, with which C<WriteConstants> is implemented.

Currently this module understands the following types. h2xs may only know
a subset. The sizes of the numeric types are chosen by the C<Configure>
script at compile time.

=over 4

=item IV

signed integer, at least 32 bits.

=item UV

unsigned integer, the same size as I<IV>

=item NV

floating point type, probably C<double>, possibly C<long double>

=item PV

NUL terminated string, length will be determined with C<strlen>

=item PVN

A fixed length thing, given as a [pointer, length] pair. If you know the
length of a string at compile time you may use this instead of I<PV>

=item SV

A B<mortal> SV.

=item YES

Truth.  (C<PL_sv_yes>)  The value is not needed (and ignored).

=item NO

Defined Falsehood.  (C<PL_sv_no>)  The value is not needed (and ignored).

=item UNDEF

C<undef>.  The value of the macro is not needed.

=back

=head1 FUNCTIONS

=over 4

=cut

if ($] >= 5.006) {
  eval "use warnings; 1" or die $@;
}
use strict;
use Carp qw(croak cluck);

use Exporter;
use ExtUtils::Constant::Utils qw(C_stringify);
use ExtUtils::Constant::XS qw(%XS_Constant %XS_TypeSet);

@ISA = 'Exporter';

%EXPORT_TAGS = ( 'all' => [ qw(
	XS_constant constant_types return_clause memEQ_clause C_stringify
	C_constant autoload WriteConstants WriteMakefileSnippet
) ] );

@EXPORT_OK = ( @{ $EXPORT_TAGS{'all'} } );

=item constant_types

A function returning a single scalar with C<#define> definitions for the
constants used internally between the generated C and XS functions.

=cut

sub constant_types {
  ExtUtils::Constant::XS->header();
}

sub memEQ_clause {
  cluck "ExtUtils::Constant::memEQ_clause is deprecated";
  ExtUtils::Constant::XS->memEQ_clause({name=>$_[0], checked_at=>$_[1],
					indent=>$_[2]});
}

sub return_clause ($$) {
  cluck "ExtUtils::Constant::return_clause is deprecated";
  my $indent = shift;
  ExtUtils::Constant::XS->return_clause({indent=>$indent}, @_);
}

sub switch_clause {
  cluck "ExtUtils::Constant::switch_clause is deprecated";
  my $indent = shift;
  my $comment = shift;
  ExtUtils::Constant::XS->switch_clause({indent=>$indent, comment=>$comment},
					@_);
}

sub C_constant {
  my ($package, $subname, $default_type, $what, $indent, $breakout, @items)
    = @_;
  ExtUtils::Constant::XS->C_constant({package => $package, subname => $subname,
				      default_type => $default_type,
				      types => $what, indent => $indent,
				      breakout => $breakout}, @items);
}

=item XS_constant PACKAGE, TYPES, SUBNAME, C_SUBNAME

A function to generate the XS code to implement the perl subroutine
I<PACKAGE>::constant used by I<PACKAGE>::AUTOLOAD to load constants.
This XS code is a wrapper around a C subroutine usually generated by
C<C_constant>, and usually named C<constant>.

I<TYPES> should be given either as a comma separated list of types that the
C subroutine C<constant> will generate or as a reference to a hash. It should
be the same list of types as C<C_constant> was given.
[Otherwise C<XS_constant> and C<C_constant> may have different ideas about
the number of parameters passed to the C function C<constant>]

You can call the perl visible subroutine something other than C<constant> if
you give the parameter I<SUBNAME>. The C subroutine it calls defaults to
the name of the perl visible subroutine, unless you give the parameter
I<C_SUBNAME>.

=cut

sub XS_constant {
  my $package = shift;
  my $what = shift;
  my $subname = shift;
  my $C_subname = shift;
  $subname ||= 'constant';
  $C_subname ||= $subname;

  if (!ref $what) {
    # Convert line of the form IV,UV,NV to hash
    $what = {map {$_ => 1} split /,\s*/, ($what)};
  }
  my $params = ExtUtils::Constant::XS->params ($what);
  my $type;

  my $xs = <<"EOT";
void
$subname(sv)
    PREINIT:
#ifdef dXSTARG
	dXSTARG; /* Faster if we have it.  */
#else
	dTARGET;
#endif
	STRLEN		len;
        int		type;
EOT

  if ($params->{IV}) {
    $xs .= "	IV		iv;\n";
  } else {
    $xs .= "	/* IV\t\tiv;\tUncomment this if you need to return IVs */\n";
  }
  if ($params->{NV}) {
    $xs .= "	NV		nv;\n";
  } else {
    $xs .= "	/* NV\t\tnv;\tUncomment this if you need to return NVs */\n";
  }
  if ($params->{PV}) {
    $xs .= "	const char	*pv;\n";
  } else {
    $xs .=
      "	/* const char\t*pv;\tUncomment this if you need to return PVs */\n";
  }

  $xs .= << 'EOT';
    INPUT:
	SV *		sv;
        const char *	s = SvPV(sv, len);
EOT
  $xs .= << 'EOT';
    PPCODE:
EOT

  if ($params->{IV} xor $params->{NV}) {
    $xs .= << "EOT";
        /* Change this to $C_subname(aTHX_ s, len, &iv, &nv);
           if you need to return both NVs and IVs */
EOT
  }
  $xs .= "	type = $C_subname(aTHX_ s, len";
  $xs .= ', &iv' if $params->{IV};
  $xs .= ', &nv' if $params->{NV};
  $xs .= ', &pv' if $params->{PV};
  $xs .= ', &sv' if $params->{SV};
  $xs .= ");\n";

  # If anyone is insane enough to suggest a package name containing %
  my $package_sprintf_safe = $package;
  $package_sprintf_safe =~ s/%/%%/g;

  $xs .= << "EOT";
      /* Return 1 or 2 items. First is error message, or undef if no error.
           Second, if present, is found value */
        switch (type) {
        case PERL_constant_NOTFOUND:
          sv =
	    sv_2mortal(newSVpvf("%s is not a valid $package_sprintf_safe macro", s));
          PUSHs(sv);
          break;
        case PERL_constant_NOTDEF:
          sv = sv_2mortal(newSVpvf(
	    "Your vendor has not defined $package_sprintf_safe macro %s, used",
				   s));
          PUSHs(sv);
          break;
EOT

  foreach $type (sort keys %XS_Constant) {
    # '' marks utf8 flag needed.
    next if $type eq '';
    $xs .= "\t/* Uncomment this if you need to return ${type}s\n"
      unless $what->{$type};
    $xs .= "        case PERL_constant_IS$type:\n";
    if (length $XS_Constant{$type}) {
      $xs .= << "EOT";
          EXTEND(SP, 1);
          PUSHs(&PL_sv_undef);
          $XS_Constant{$type};
EOT
    } else {
      # Do nothing. return (), which will be correctly interpreted as
      # (undef, undef)
    }
    $xs .= "          break;\n";
    unless ($what->{$type}) {
      chop $xs; # Yes, another need for chop not chomp.
      $xs .= " */\n";
    }
  }
  $xs .= << "EOT";
        default:
          sv = sv_2mortal(newSVpvf(
	    "Unexpected return type %d while processing $package_sprintf_safe macro %s, used",
               type, s));
          PUSHs(sv);
        }
EOT

  return $xs;
}


=item autoload PACKAGE, VERSION, AUTOLOADER

A function to generate the AUTOLOAD subroutine for the module I<PACKAGE>
I<VERSION> is the perl version the code should be backwards compatible with.
It defaults to the version of perl running the subroutine.  If I<AUTOLOADER>
is true, the AUTOLOAD subroutine falls back on AutoLoader::AUTOLOAD for all
names that the constant() routine doesn't recognise.

=cut

# ' # Grr. syntax highlighters that don't grok pod.

sub autoload {
  my ($module, $compat_version, $autoloader) = @_;
  $compat_version ||= $];
  croak "Can't maintain compatibility back as far as version $compat_version"
    if $compat_version < 5;
  my $func = "sub AUTOLOAD {\n"
  . "    # This AUTOLOAD is used to 'autoload' constants from the constant()\n"
  . "    # XS function.";
  $func .= "  If a constant is not found then control is passed\n"
  . "    # to the AUTOLOAD in AutoLoader." if $autoloader;


  $func .= "\n\n"
  . "    my \$constname;\n";
  $func .=
    "    our \$AUTOLOAD;\n"  if ($compat_version >= 5.006);

  $func .= <<"EOT";
    (\$constname = \$AUTOLOAD) =~ s/.*:://;
    croak "&${module}::constant not defined" if \$constname eq 'constant';
    my (\$error, \$val) = constant(\$constname);
EOT

  if ($autoloader) {
    $func .= <<'EOT';
    if ($error) {
	if ($error =~  /is not a valid/) {
	    $AutoLoader::AUTOLOAD = $AUTOLOAD;
	    goto &AutoLoader::AUTOLOAD;
	} else {
	    croak $error;
	}
    }
EOT
  } else {
    $func .=
      "    if (\$error) { croak \$error; }\n";
  }

  $func .= <<'END';
    {
	no strict 'refs';
	# Fixed between 5.005_53 and 5.005_61
#XXX	if ($] >= 5.00561) {
#XXX	    *$AUTOLOAD = sub () { $val };
#XXX	}
#XXX	else {
	    *{Symbol::fetch_glob($AUTOLOAD)} = sub { $val };
#XXX	}
    }
    goto &{Symbol::fetch_glob($AUTOLOAD)};
}

END

  return $func;
}


=item WriteMakefileSnippet

WriteMakefileSnippet ATTRIBUTE =E<gt> VALUE [, ...] 

A function to generate perl code for Makefile.PL that will regenerate
the constant subroutines.  Parameters are named as passed to C<WriteConstants>,
with the addition of C<INDENT> to specify the number of leading spaces
(default 2).

Currently only C<INDENT>, C<NAME>, C<DEFAULT_TYPE>, C<NAMES>, C<C_FILE> and
C<XS_FILE> are recognised.

=cut

sub WriteMakefileSnippet {
  my %args = @_;
  my $indent = $args{INDENT} || 2;

  my $result = <<"EOT";
ExtUtils::Constant::WriteConstants(
                                   NAME         => '$args{NAME}',
                                   NAMES        => \\\@names,
                                   DEFAULT_TYPE => '$args{DEFAULT_TYPE}',
EOT
  foreach (qw (C_FILE XS_FILE)) {
    next unless exists $args{$_};
    $result .= sprintf "                                   %-12s => '%s',\n",
      $_, $args{$_};
  }
  $result .= <<'EOT';
                                );
EOT

  $result =~ s/^/' 'x$indent/gem;
  return ExtUtils::Constant::XS->dump_names({default_type=>$args{DEFAULT_TYPE},
					     indent=>$indent,},
					    @{$args{NAMES}})
    . $result;
}

=item WriteConstants ATTRIBUTE =E<gt> VALUE [, ...]

Writes a file of C code and a file of XS code which you should C<#include>
and C<INCLUDE> in the C and XS sections respectively of your module's XS
code.  You probably want to do this in your C<Makefile.PL>, so that you can
easily edit the list of constants without touching the rest of your module.
The attributes supported are

=over 4

=item NAME

Name of the module.  This must be specified

=item DEFAULT_TYPE

The default type for the constants.  If not specified C<IV> is assumed.

=item BREAKOUT_AT

The names of the constants are grouped by length.  Generate child subroutines
for each group with this number or more names in.

=item NAMES

An array of constants' names, either scalars containing names, or hashrefs
as detailed in L<"C_constant">.

=item C_FH

A filehandle to write the C code to.  If not given, then I<C_FILE> is opened
for writing.

=item C_FILE

The name of the file to write containing the C code.  The default is
C<const-c.inc>.  The C<-> in the name ensures that the file can't be
mistaken for anything related to a legitimate perl package name, and
not naming the file C<.c> avoids having to override Makefile.PL's
C<.xs> to C<.c> rules.

=item XS_FH

A filehandle to write the XS code to.  If not given, then I<XS_FILE> is opened
for writing.

=item XS_FILE

The name of the file to write containing the XS code.  The default is
C<const-xs.inc>.

=item SUBNAME

The perl visible name of the XS subroutine generated which will return the
constants. The default is C<constant>.

=item C_SUBNAME

The name of the C subroutine generated which will return the constants.
The default is I<SUBNAME>.  Child subroutines have C<_> and the name
length appended, so constants with 10 character names would be in
C<constant_10> with the default I<XS_SUBNAME>.

=back

=cut

sub WriteConstants {
  my %ARGS =
    ( # defaults
     C_FILE =>       'const-c.inc',
     XS_FILE =>      'const-xs.inc',
     SUBNAME =>      'constant',
     DEFAULT_TYPE => 'IV',
     @_);

  $ARGS{C_SUBNAME} ||= $ARGS{SUBNAME}; # No-one sane will have C_SUBNAME eq '0'

  croak "Module name not specified" unless length $ARGS{NAME};

  my $c_fh = $ARGS{C_FH};
  if (!$c_fh) {
      if ($] <= 5.008) {
	  # We need these little games, rather than doing things
	  # unconditionally, because we're used in core Makefile.PLs before
	  # IO is available (needed by filehandle), but also we want to work on
	  # older perls where undefined scalars do not automatically turn into
	  # anonymous file handles.
	  require FileHandle;
	  $c_fh = FileHandle->new();
      }
      open $c_fh, ">$ARGS{C_FILE}" or die "Can't open $ARGS{C_FILE}: $!";
  }

  my $xs_fh = $ARGS{XS_FH};
  if (!$xs_fh) {
      if ($] <= 5.008) {
	  require FileHandle;
	  $xs_fh = FileHandle->new();
      }
      open $xs_fh, ">$ARGS{XS_FILE}" or die "Can't open $ARGS{XS_FILE}: $!";
  }

  # As this subroutine is intended to make code that isn't edited, there's no
  # need for the user to specify any types that aren't found in the list of
  # names.
  
  if ($ARGS{PROXYSUBS}) {
      require ExtUtils::Constant::ProxySubs;
      $ARGS{C_FH} = $c_fh;
      $ARGS{XS_FH} = $xs_fh;
      ExtUtils::Constant::ProxySubs->WriteConstants(%ARGS);
  } else {
      my $types = {};

      print $c_fh constant_types(); # macro defs
      print $c_fh "\n";

      # indent is still undef. Until anyone implements indent style rules with
      # it.
      foreach (ExtUtils::Constant::XS->C_constant({package => $ARGS{NAME},
						   subname => $ARGS{C_SUBNAME},
						   default_type =>
						       $ARGS{DEFAULT_TYPE},
						       types => $types,
						       breakout =>
						       $ARGS{BREAKOUT_AT}},
						  @{$ARGS{NAMES}})) {
	  print $c_fh $_, "\n"; # C constant subs
      }
      print $xs_fh XS_constant ($ARGS{NAME}, $types, $ARGS{XS_SUBNAME},
				$ARGS{C_SUBNAME});
  }

  close $c_fh or warn "Error closing $ARGS{C_FILE}: $!" unless $ARGS{C_FH};
  close $xs_fh or warn "Error closing $ARGS{XS_FILE}: $!" unless $ARGS{XS_FH};
}

1;
__END__

=back

=head1 AUTHOR

Nicholas Clark <nick@ccl4.org> based on the code in C<h2xs> by Larry Wall and
others

=cut
