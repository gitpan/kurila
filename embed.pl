#!/usr/bin/perl -w

                # we build the new one

use kurila;


BEGIN {
    # Get function prototypes
    require 'regen_lib.pl';
}

my @az = map { chr }, ord('a')..ord('z');

my $SPLINT = 0; # Turn true for experimental splint support http://www.splint.org

#
# See database of global and static function prototypes in embed.fnc
# This is used to generate prototype headers under various configurations,
# export symbols lists for different platforms, and macros to provide an
# implicit interpreter context argument.
#

sub do_not_edit($file)
{
    my $years = '1993, 1994, 1995, 1996, 1997, 1998, 1999, 2000, 2001, 2002, 2003, 2004, 2005, 2006, 2007';

    $years =~ s/1999,/1999,\n  / if length $years +> 40;

    my $warning = <<EOW;
 -*- buffer-read-only: t -*-

   $file

   Copyright (C) $years, by Larry Wall and others

   You may distribute under the terms of either the GNU General Public
   License or the Artistic License, as specified in the README file.

!!!!!!!   DO NOT EDIT THIS FILE   !!!!!!!
This file is built by embed.pl from data in embed.fnc, embed.pl,
pp.sym, intrpvar.h, and perlvars.h.
Any changes made here will be lost!

Edit those files and run 'make regen_headers' to effect changes.

EOW

    $warning .= <<EOW if $file eq 'perlapi.c';

Up to the threshold of the door there mounted a flight of twenty-seven
broad stairs, hewn by some unknown art of the same black stone.  This
was the only entrance to the tower.


EOW

    if ($file =~ m:\.[ch]$:) {
	$warning =~ s:^: * :gm;
	$warning =~ s: +$::gm;
	$warning =~ s: :/:;
	$warning =~ s:$:/:;
    }
    else {
	$warning =~ s:^:# :gm;
	$warning =~ s: +$::gm;
    }
    $warning;
} # do_not_edit

open my $in, '<', "embed.fnc" or die $^OS_ERROR;

# walk table providing an array of components in each line to
# subroutine, printing the result
sub walk_table ($function, ?$filename, ?$leader, ?$trailer) {
    $filename //= '-';
    $leader //= do_not_edit($filename);
    my $F;
    if (ref $filename) {	# filehandle
	$F = $filename;
    }
    else {
	# safer_unlink $filename if $filename ne '/dev/null';
	$F = safer_open("$filename-new");
    }
    print $F, $leader if $leader;
    seek $in, 0, 0;		# so we may restart
    while ( ~< *$in) {
	chomp;
	next if m/^:/;
	while (s|\\$||) {
	    $_ .= ~< *$in;
	    chomp;
	}
	s/\s+$//;
	my @args;
	if (m/^\s*(#|$)/) {
	    @args = @( $_ );
	}
	else {
	    @args = split m/\s*\|\s*/, $_;
	}
	my @outs = @( $function->(< @args) );
	print $F, < @outs;
    }
    print $F, $trailer if $trailer;
    unless (ref $filename) {
	safer_close($F);
	rename_if_different("$filename-new", $filename);
    }
}

sub munge_c_files () {
    my $functions = \%();
    unless (nelems @ARGV) {
	warn "\@ARGV empty, nothing to do\n";
	return;
    }
    walk_table {
	if ((nelems @_) +> 1) {
	    $functions->{+@_[2]} = \@_ if @_[(nelems @_)-1] =~ m/\.\.\./;
	}
    }, '/dev/null', '', '';
    while ( ~< *ARGV) {
	s{(\b(\w+)[ \t]*\([ \t]*(?!aTHX))}
	 {$( do {
	    my $repl = $1;
	    my $f = $2;
	    if (exists $functions->{$f}) {
		$repl .= "aTHX_ ";
		die("$ARGV:#$repl");
	    }
	    $repl;
          })}g;
	print $^STDOUT, $_;
	close *ARGV if eof;	# restart $.
    }
    exit;
}

#munge_c_files();

# generate proto.h
my $wrote_protected = 0;

sub write_protos {
    my $ret = "";
    if ((nelems @_) == 1) {
	my $arg = shift;
	$ret .= "$arg\n";
    }
    else {
	my @($flags,$retval,$plain_func,@< @args) =  @_;
	my @nonnull;
	my $has_context = ( $flags !~ m/n/ );
	my $never_returns = ( $flags =~ m/r/ );
	my $commented_out = ( $flags =~ m/m/ );
	my $is_malloc = ( $flags =~ m/a/ );
	my $can_ignore = ( $flags !~ m/R/ ) && !$is_malloc;
	my @names_of_nn;
	my $func;

	my $splint_flags = "";
	if ( $SPLINT && !$commented_out ) {
	    $splint_flags .= '/*@noreturn@*/ ' if $never_returns;
	    if ($can_ignore && ($retval ne 'void') && ($retval !~ m/\*/)) {
		$retval .= " /*\@alt void\@*/";
	    }
	}

	if ($flags =~ m/s/) {
	    $retval = "STATIC $splint_flags$retval";
	    $func = "S_$plain_func";
	}
	else {
	    $retval = ($flags =~ m/i/ ?? "PERL_INLINE_CALLCONV" !! "PERL_CALLCONV" )
              . " $splint_flags$retval";
	    if ($flags =~ m/[bp]/) {
		$func = "Perl_$plain_func";
	    } else {
		$func = $plain_func;
	    }
	}
        my $xv_macros = $func =~ m/Xv/;
        if ($xv_macros) {
            $func =~ s/Xv/Sv/;
        }
        if ($flags =~ m/S/) {
            for (@args) {
                s/\bXV\b/SV/;
            }
        }
	$ret .= "$retval\t$func(";
	if ( $has_context ) {
	    $ret .= (nelems @args) ?? "pTHX_ " !! "pTHX";
	}
	if ((nelems @args)) {
	    my $n;
	    for my $arg (  @args ) {
		++$n;
		if ( $arg =~ m/\*/ && $arg !~ m/\b(NN|NULLOK)\b/ ) {
		    warn "$func: $arg needs NN or NULLOK\n";
		    our $unflagged_pointers;
		    ++$unflagged_pointers;
		}
		my $nn = ( $arg =~ s/\s*\bNN\b\s+// );
		push( @nonnull, $n ) if $nn;

		my $nullok = ( $arg =~ s/\s*\bNULLOK\b\s+// ); # strip NULLOK with no effect

		# Make sure each arg has at least a type and a var name.
		# An arg of "int" is valid C, but want it to be "int foo".
		my $temp_arg = $arg;
		$temp_arg =~ s/\*//g;
		$temp_arg =~ s/\s*\bstruct\b\s*/ /g;
		if ( ($temp_arg ne "...")
		     && ($temp_arg !~ m/\w+\s+(\w+)(?:\[\d+\])?\s*$/) ) {
		    warn "$func: $arg ($n) doesn't have a name\n";
		}
		if ( $SPLINT && $nullok && !$commented_out ) {
		    $arg = '/*@null@*/ ' . $arg;
		}
		if (defined $1 && $nn) {
		    push @names_of_nn, $1;
		}
	    }
	    $ret .= join ", ", @args;
	}
	else {
	    $ret .= "void" if !$has_context;
	}
	$ret .= ")";
	my @attrs;
	if ( $flags =~ m/r/ ) {
	    push @attrs, "__attribute__noreturn__";
	}
	if ( $is_malloc ) {
	    push @attrs, "__attribute__malloc__";
	}
	if ( !$can_ignore ) {
	    push @attrs, "__attribute__warn_unused_result__";
	}
	if ( $flags =~ m/P/ ) {
	    push @attrs, "__attribute__pure__";
	}
	if( $flags =~ m/f/ ) {
	    my $prefix	= $has_context ?? 'pTHX_' !! '';
	    my $args	= scalar nelems @args;
 	    my $pat	= $args - 1;
	    my $macro	= (nelems @nonnull) && @nonnull[-1] == $pat  
				?? '__attribute__format__'
				!! '__attribute__format__null_ok__';
	    push @attrs, sprintf "\%s(__printf__,\%s\%d,\%s\%d)", $macro,
				$prefix, $pat, $prefix, $args;
	}
	if ( (nelems @nonnull) ) {
	    my @pos = map { $has_context ?? "pTHX_$_" !! $_ }, @nonnull;
	    push @attrs, < map { sprintf( "__attribute__nonnull__(\%s)", $_ ) }, @pos;
	}
	if ( (nelems @attrs) ) {
	    $ret .= "\n";
	    $ret .= join( "\n", map { "\t\t\t$_" }, @attrs );
	}
	$ret .= ";";
	$ret = "/* $ret */" if $commented_out;
	if ((nelems @names_of_nn)) {
	    $ret .= "\n#define PERL_ARGS_ASSERT_\U$plain_func\E\t\\\n\t"
		. join '; ', map { "assert($_)" }, @names_of_nn;
	}
	$ret .= (nelems @attrs) ?? "\n\n" !! "\n";
    }
    $ret;
}

# generates global.sym (API export list)
do {
  my %seen;
  sub write_global_sym {
      my $ret = "";
      if ((nelems @_) +> 1) {
	  my @($flags,$retval,$func,@< @args) =  @_;
	  # If a function is defined twice, for example before and after an
	  # #else, only process the flags on the first instance for global.sym
	  return $ret if %seen{+$func}++;
	  if ($flags =~ m/[AX]/ && $flags !~ m/[xm]/
	      || $flags =~ m/b/) { # public API, so export
	      $func = "Perl_$func" if $flags =~ m/[pbX]/;
	      $ret = "$func\n";
	  }
      }
      $ret;
  }
};


our $unflagged_pointers;
walk_table(\&write_protos,     "proto.h", undef, "/* ex: set ro: */\n");
warn "$unflagged_pointers pointer arguments to clean up\n" if $unflagged_pointers;
walk_table(\&write_global_sym, "global.sym", undef, "# ex: set ro:\n");

# XXX others that may need adding
#       warnhook
#       hints
my @extvars = qw(sv_undef sv_yes sv_no na dowarn
		 curcop compiling
		 tainting tainted stack_base stack_sp sv_arenaroot
		 no_modify
		 curstash DBsub DBsingle DBassertion debstash
		 rsfp
		 stdingv
		 defgv
		 errgv
		 rsfp_filters
		 perldb
		 diehook
		 dirty
		 perl_destruct_level
		 ppaddr
                );

sub readsyms($syms, $file) {
    local ($_);
    my $fh;
    open($fh, "<", "$file")
	or die "embed.pl: Can't open $file: $^OS_ERROR\n";
    while ( ~< *$fh) {
	s/[ \t]*#.*//;		# Delete comments.
	if (m/^\s*(\S+)\s*$/) {
	    my $sym = $1;
	    warn "duplicate symbol $sym while processing $file line $(iohandle::input_line_number(\*FILE)).\n"
		if exists $syms->{$sym};
	    $syms->{+$sym} = 1;
	}
    }
    close($fh);
}

# Perl_pp_* and Perl_ck_* are in pp.sym
readsyms \my %ppsym, 'pp.sym';

sub readvars($syms, $file,$pre,?$keep_pre) {
    local ($_);
    open(my $fh, "<", "$file")
	or die "embed.pl: Can't open $file: $^OS_ERROR\n";
    while ( ~< *$fh) {
	s/[ \t]*#.*//;		# Delete comments.
	if (m/PERLVARA?I?S?C?\($pre(\w+)/) {
	    my $sym = $1;
	    $sym = $pre . $sym if $keep_pre;
	    warn "duplicate symbol $sym while processing $file line $(iohandle::input_line_number(\*FILE))\n"
		if exists %$syms{$sym};
	    %$syms{+$sym} = $pre || 1;
	}
    }
    close($fh);
}

my %intrp;
my %globvar;

readvars \%intrp,  'intrpvar.h','I';
readvars \%globvar, 'perlvars.h','G';

sub undefine($sym) {
    "#undef  $sym\n";
}

sub hide($from, $to) {
    my $t = int(length($from) / 8);
    "#define $from" . "\t" x ($t +< 3 ?? 3 - $t !! 1) . "$to\n";
}

sub bincompat_var($pfx, $sym) {
    my $arg = ($pfx eq 'G' ?? 'NULL' !! 'aTHX');
    undefine("PL_$sym") . hide("PL_$sym", "(*Perl_$($pfx)$($sym)_ptr($arg))");
}

sub multon($sym,$pre,$ptr) {
    hide("PL_$sym", "($ptr$pre$sym)");
}

sub multoff($sym,$pre) {
    return hide("PL_$pre$sym", "PL_$sym");
}

my $em = safer_open('embed.h-new');

print $em, do_not_edit ("embed.h"), <<'END';

/* (Doing namespace management portably in C is really gross.) */

/* By defining PERL_NO_SHORT_NAMES (not done by default) the short forms
 * (like warn instead of Perl_warn) for the API are not defined.
 * Not defining the short forms is a good thing for cleaner embedding. */

#ifndef PERL_NO_SHORT_NAMES

/* Hide global symbols */

#if !defined(PERL_IMPLICIT_CONTEXT)

END

# Try to elimiate lots of repeated
# #ifdef PERL_CORE
# foo
# #endif
# #ifdef PERL_CORE
# bar
# #endif
# by tracking state and merging foo and bar into one block.
my $ifdef_state = '';

sub write_xv_defines($retval, $func, @args) {
    for my $xv (qw[Av Hv Cv Gv Io Re]) {
        my $i = 0;
        my @arglist = map {
            my $n = @az[$i++];
            if ($_ =~ m/XV\s*([*]+)/) {
                my $p = 'p' x (length($1)-1);
                @: $n, "$(lc $xv)$($p)Tsv$p($n)";
            }
            else {
                @: $n, $n;
            }
        }, @args;
        my $dlist = join(",", map { $_[0] }, @arglist);
        my $alist = join(",", map { $_[1] }, @arglist);
        my $xvname = $func;
        $xvname =~ s/^Sv/$xv/;
        $xvname =~ s/^sv/$(lc $xv)/;
        my $ret_convert = $retval =~ m/SV/;
        my $call = "Perl_" . $func . "(aTHX_ $alist)";
        print $em, "#define $xvname($dlist)\t\t" . ($ret_convert ?? "svT$(lc $xv)" . "($call)" !! $call ) . "\n";
    }
}

walk_table sub {
    my $ret = "";
    my $new_ifdef_state = '';
    if ((nelems @_) == 1) {
	my $arg = shift;
	$ret .= "$arg\n" if $arg =~ m/^#\s*(if|ifn?def|else|endif)\b/;
    }
    else {
	my @($flags,$retval,$func,@< @args) =  @_;
	unless ($flags =~ m/[om]/) {
	    if ($flags =~ m/s/) {
		$ret .= hide($func,"S_$func");
	    }
	    elsif ($flags =~ m/p/) {
		$ret .= hide($func,"Perl_$func");
	    }
            if ($flags =~ m/S/) {
                write_xv_defines($retval, $func, @args);
            }
	}
	if ($ret ne '' && $flags !~ m/A/) {
	    if ($flags =~ m/E/) {
		$new_ifdef_state
		    = "#if defined(PERL_CORE) || defined(PERL_EXT)\n";
	    }
	    else {
		$new_ifdef_state = "#ifdef PERL_CORE\n";
	    }

	    if ($new_ifdef_state ne $ifdef_state) {
		$ret = $new_ifdef_state . $ret;
	    }
        }
    }
    if ($ifdef_state && $new_ifdef_state ne $ifdef_state) {
	# Close the old one ahead of opening the new one.
	$ret = "#endif\n$ret";
    }
    # Remember the new state.
    $ifdef_state = $new_ifdef_state;
    $ret;
}, $em, "";

if ($ifdef_state) {
    print $em, "#endif\n";
}

for my $sym (sort keys %ppsym) {
    $sym =~ s/^Perl_//;
    print $em, hide($sym, "Perl_$sym");
}

print $em, <<'END';

#else	/* PERL_IMPLICIT_CONTEXT */

END

$ifdef_state = '';
walk_table sub {
    my $ret = "";
    my $new_ifdef_state = '';
    if ((nelems @_) == 1) {
	my $arg = shift;
	$ret .= "$arg\n" if $arg =~ m/^#\s*(if|ifn?def|else|endif)\b/;
    }
    else {
	my @($flags,$retval,$func,@< @args) =  @_;
	unless ($flags =~ m/[om]/) {
	    my $args = scalar nelems @args;
	    if ($args and @args[$args-1] =~ m/\.\.\./) {
	        # we're out of luck for varargs functions under CPP
	    }
	    elsif ($flags =~ m/n/) {
		if ($flags =~ m/s/) {
		    $ret .= hide($func,"S_$func");
		}
		elsif ($flags =~ m/p/) {
		    $ret .= hide($func,"Perl_$func");
		}
	    }
	    else {
		my $alist = join(",", @az[[0..$args-1]]);
		$ret = "#define $func($alist)";
		my $t = int(length($ret) / 8);
		$ret .=  "\t" x ($t +< 4 ?? 4 - $t !! 1);
		if ($flags =~ m/s/) {
		    $ret .= "S_$func(aTHX";
		}
		elsif ($flags =~ m/p/) {
		    $ret .= "Perl_$func(aTHX";
		}
		$ret .= "_ " if $alist;
		$ret .= $alist . ")\n";
	    }
            if ($flags =~ m/S/) {
                write_xv_defines($retval, $func, @args);
            }
	}
	unless ($flags =~ m/A/) {
	    if ($flags =~ m/E/) {
		$new_ifdef_state
		    = "#if defined(PERL_CORE) || defined(PERL_EXT)\n";
	    }
	    else {
		$new_ifdef_state = "#ifdef PERL_CORE\n";
	    }

	    if ($new_ifdef_state ne $ifdef_state) {
		$ret = $new_ifdef_state . $ret;
	    }
        }
    }
    if ($ifdef_state && $new_ifdef_state ne $ifdef_state) {
	# Close the old one ahead of opening the new one.
	$ret = "#endif\n$ret";
    }
    # Remember the new state.
    $ifdef_state = $new_ifdef_state;
    $ret;
}, $em, "";

if ($ifdef_state) {
    print $em, "#endif\n";
}

for my $sym (sort keys %ppsym) {
    $sym =~ s/^Perl_//;
    if ($sym =~ m/^ck_/) {
	print $em, hide("$sym(a)", "Perl_$sym(aTHX_ a)");
    }
    elsif ($sym =~ m/^pp_/) {
	print $em, hide("$sym()", "Perl_$sym(aTHX)");
    }
    else {
	warn "Illegal symbol '$sym' in pp.sym";
    }
}

print $em, <<'END';

#endif	/* PERL_IMPLICIT_CONTEXT */

#endif	/* #ifndef PERL_NO_SHORT_NAMES */

END

print $em, <<'END';

/* Compatibility stubs.  Compile extensions with -DPERL_NOCOMPAT to
   disable them.
 */

#if !defined(PERL_CORE)
#  define sv_setptrobj(rv,ptr,name)	sv_setref_iv(rv,name,PTR2IV(ptr))
#  define sv_setptrref(rv,ptr)		sv_setref_iv(rv,NULL,PTR2IV(ptr))
#endif

#if !defined(PERL_CORE) && !defined(PERL_NOCOMPAT)

/* Compatibility for various misnamed functions.  All functions
   in the API that begin with "perl_" (not "Perl_") take an explicit
   interpreter context pointer.
   The following are not like that, but since they had a "perl_"
   prefix in previous versions, we provide compatibility macros.
 */
#  define perl_atexit(a,b)		call_atexit(a,b)
#  define perl_call_argv(a,b,c)		call_argv(a,b,c)
#  define perl_call_pv(a,b)		call_pv(a,b)
#  define perl_call_method(a,b)		call_method(a,b)
#  define perl_call_sv(a,b)		call_sv(a,b)
#  define perl_eval_sv(a,b)		eval_sv(a,b)
#  define perl_eval_pv(a,b)		eval_pv(a,b)
#  define perl_require_pv(a)		require_pv(a)
#  define perl_get_sv(a,b)		get_sv(a,b)
#  define perl_get_av(a,b)		get_av(a,b)
#  define perl_get_hv(a,b)		get_hv(a,b)
#  define perl_get_cv(a,b)		get_cv(a,b)
#  define perl_init_i18nl10n(a)		init_i18nl10n(a)
#  define perl_init_i18nl14n(a)		init_i18nl14n(a)
#  define perl_new_ctype(a)		new_ctype(a)
#  define perl_new_collate(a)		new_collate(a)
#  define perl_new_numeric(a)		new_numeric(a)

/* varargs functions can't be handled with CPP macros. :-(
   This provides a set of compatibility functions that don't take
   an extra argument but grab the context pointer using the macro
   dTHX.
 */
#if defined(PERL_IMPLICIT_CONTEXT) && !defined(PERL_NO_SHORT_NAMES)
#  define croak				Perl_croak_nocontext
#  define deb				Perl_deb_nocontext
#  define die				Perl_die_nocontext
#  define form				Perl_form_nocontext
#  define load_module			Perl_load_module_nocontext
#  define mess				Perl_mess_nocontext
#  define newSVpvf			Perl_newSVpvf_nocontext
#  define sv_catpvf			Perl_sv_catpvf_nocontext
#  define sv_setpvf			Perl_sv_setpvf_nocontext
#  define warn				Perl_warn_nocontext
#  define warner			Perl_warner_nocontext
#  define sv_catpvf_mg			Perl_sv_catpvf_mg_nocontext
#  define sv_setpvf_mg			Perl_sv_setpvf_mg_nocontext
#endif

#endif /* !defined(PERL_CORE) && !defined(PERL_NOCOMPAT) */

#if !defined(PERL_IMPLICIT_CONTEXT)
/* undefined symbols, point them back at the usual ones */
#  define Perl_croak_nocontext		Perl_croak
#  define Perl_die_nocontext		Perl_die
#  define Perl_deb_nocontext		Perl_deb
#  define Perl_form_nocontext		Perl_form
#  define Perl_load_module_nocontext	Perl_load_module
#  define Perl_mess_nocontext		Perl_mess
#  define Perl_newSVpvf_nocontext	Perl_newSVpvf
#  define Perl_sv_catpvf_nocontext	Perl_sv_catpvf
#  define Perl_sv_setpvf_nocontext	Perl_sv_setpvf
#  define Perl_warn_nocontext		Perl_warn
#  define Perl_warner_nocontext		Perl_warner
#  define Perl_sv_catpvf_mg_nocontext	Perl_sv_catpvf_mg
#  define Perl_sv_setpvf_mg_nocontext	Perl_sv_setpvf_mg
#endif

/* ex: set ro: */
END

safer_close($em);
rename_if_different('embed.h-new', 'embed.h');

$em = safer_open('embedvar.h-new');

print $em, do_not_edit ("embedvar.h"), <<'END';

/* (Doing namespace management portably in C is really gross.) */

/*
   The following combinations of MULTIPLICITY and PERL_IMPLICIT_CONTEXT
   are supported:
     1) none
     2) MULTIPLICITY	# supported for compatibility
     3) MULTIPLICITY && PERL_IMPLICIT_CONTEXT

   All other combinations of these flags are errors.

   only #3 is supported directly, while #2 is a special
   case of #3 (supported by redefining vTHX appropriately).
*/

#if defined(MULTIPLICITY)
/* cases 2 and 3 above */

#  if defined(PERL_IMPLICIT_CONTEXT)
#    define vTHX	aTHX
#  else
#    define vTHX	PERL_GET_INTERP
#  endif

END

for my $sym (sort keys %intrp) {
    print $em, multon($sym,'I','vTHX->');
}

print $em, <<'END';

#else	/* !MULTIPLICITY */

/* case 1 above */

END

for my $sym (sort keys %intrp) {
    print $em, multoff($sym,'I');
}

print $em, <<'END';

END

print $em, <<'END';

#endif	/* MULTIPLICITY */

#if defined(PERL_GLOBAL_STRUCT)

END

for my $sym (sort keys %globvar) {
    print $em, multon($sym,   'G','my_vars->');
    print $em, multon("G$sym",'', 'my_vars->');
}

print $em, <<'END';

#else /* !PERL_GLOBAL_STRUCT */

END

for my $sym (sort keys %globvar) {
    print $em, multoff($sym,'G');
}

print $em, <<'END';

#endif /* PERL_GLOBAL_STRUCT */

#ifdef PERL_POLLUTE		/* disabled by default in 5.6.0 */

END

for my $sym (sort @extvars) {
    print $em, hide($sym,"PL_$sym");
}

print $em, <<'END';

#endif /* PERL_POLLUTE */

/* ex: set ro: */
END

safer_close($em);
rename_if_different('embedvar.h-new', 'embedvar.h');

my $capi = safer_open('perlapi.c-new');
my $capih = safer_open('perlapi.h-new');

print $capih, do_not_edit ("perlapi.h"), <<'EOT';

/* declare accessor functions for Perl variables */
#ifndef __perlapi_h__
#define __perlapi_h__

#if defined (MULTIPLICITY)

START_EXTERN_C

#undef PERLVAR
#undef PERLVARA
#undef PERLVARI
#undef PERLVARIC
#undef PERLVARISC
#define PERLVAR(v,t)	EXTERN_C t* Perl_##v##_ptr(pTHX);
#define PERLVARA(v,n,t)	typedef t PL_##v##_t[n];			\
			EXTERN_C PL_##v##_t* Perl_##v##_ptr(pTHX);
#define PERLVARI(v,t,i)	PERLVAR(v,t)
#define PERLVARIC(v,t,i) PERLVAR(v, const t)
#define PERLVARISC(v,i)	typedef const char PL_##v##_t[sizeof(i)];	\
			EXTERN_C PL_##v##_t* Perl_##v##_ptr(pTHX);

#include "intrpvar.h"
#include "perlvars.h"

#undef PERLVAR
#undef PERLVARA
#undef PERLVARI
#undef PERLVARIC
#undef PERLVARISC

#ifndef PERL_GLOBAL_STRUCT
EXTERN_C Perl_ppaddr_t** Perl_Gppaddr_ptr(pTHX);
EXTERN_C Perl_check_t**  Perl_Gcheck_ptr(pTHX);
EXTERN_C unsigned char** Perl_Gfold_locale_ptr(pTHX);
#define Perl_ppaddr_ptr      Perl_Gppaddr_ptr
#define Perl_check_ptr       Perl_Gcheck_ptr
#define Perl_fold_locale_ptr Perl_Gfold_locale_ptr
#endif

END_EXTERN_C

#if defined(PERL_CORE)

/* accessor functions for Perl variables (provide binary compatibility) */

/* these need to be mentioned here, or most linkers won't put them in
   the perl executable */

#ifndef PERL_NO_FORCE_LINK

START_EXTERN_C

#ifndef DOINIT
EXTCONST void * const PL_force_link_funcs[];
#else
EXTCONST void * const PL_force_link_funcs[] = {
#undef PERLVAR
#undef PERLVARA
#undef PERLVARI
#undef PERLVARIC
#define PERLVAR(v,t)	(void*)Perl_##v##_ptr,
#define PERLVARA(v,n,t)	PERLVAR(v,t)
#define PERLVARI(v,t,i)	PERLVAR(v,t)
#define PERLVARIC(v,t,i) PERLVAR(v,t)
#define PERLVARISC(v,i) PERLVAR(v,char)

/* In Tru64 (__DEC && __osf__) the cc option -std1 causes that one
 * cannot cast between void pointers and function pointers without
 * info level warnings.  The PL_force_link_funcs[] would cause a few
 * hundred of those warnings.  In code one can circumnavigate this by using
 * unions that overlay the different pointers, but in declarations one
 * cannot use this trick.  Therefore we just disable the warning here
 * for the duration of the PL_force_link_funcs[] declaration. */

#if defined(__DECC) && defined(__osf__)
#pragma message save
#pragma message disable (nonstandcast)
#endif

#include "intrpvar.h"
#include "perlvars.h"

#if defined(__DECC) && defined(__osf__)
#pragma message restore
#endif

#undef PERLVAR
#undef PERLVARA
#undef PERLVARI
#undef PERLVARIC
#undef PERLVARISC
};
#endif	/* DOINIT */

END_EXTERN_C

#endif	/* PERL_NO_FORCE_LINK */

#else	/* !PERL_CORE */

EOT

foreach my $sym (sort keys %intrp) {
    print $capih, bincompat_var('I',$sym);
}

foreach my $sym (sort keys %globvar) {
    print $capih, bincompat_var('G',$sym);
}

print $capih, <<'EOT';

#endif /* !PERL_CORE */
#endif /* MULTIPLICITY */

#endif /* __perlapi_h__ */

/* ex: set ro: */
EOT
safer_close($capih);
rename_if_different('perlapi.h-new', 'perlapi.h');

print $capi, do_not_edit ("perlapi.c"), <<'EOT';

#include "EXTERN.h"
#include "perl.h"
#include "perlapi.h"

#if defined (MULTIPLICITY)

/* accessor functions for Perl variables (provides binary compatibility) */
START_EXTERN_C

#undef PERLVAR
#undef PERLVARA
#undef PERLVARI
#undef PERLVARIC
#undef PERLVARISC

#define PERLVAR(v,t)	t* Perl_##v##_ptr(pTHX)				\
			{ dVAR; PERL_UNUSED_CONTEXT; return &(aTHX->v); }
#define PERLVARA(v,n,t)	PL_##v##_t* Perl_##v##_ptr(pTHX)		\
			{ dVAR; PERL_UNUSED_CONTEXT; return &(aTHX->v); }

#define PERLVARI(v,t,i)	PERLVAR(v,t)
#define PERLVARIC(v,t,i) PERLVAR(v, const t)
#define PERLVARISC(v,i)	PL_##v##_t* Perl_##v##_ptr(pTHX)		\
			{ dVAR; PERL_UNUSED_CONTEXT; return &(aTHX->v); }

#include "intrpvar.h"

#undef PERLVAR
#undef PERLVARA
#define PERLVAR(v,t)	t* Perl_##v##_ptr(pTHX)				\
			{ dVAR; PERL_UNUSED_CONTEXT; return &(PL_##v); }
#define PERLVARA(v,n,t)	PL_##v##_t* Perl_##v##_ptr(pTHX)		\
			{ dVAR; PERL_UNUSED_CONTEXT; return &(PL_##v); }
#undef PERLVARIC
#undef PERLVARISC
#define PERLVARIC(v,t,i)	\
			const t* Perl_##v##_ptr(pTHX)		\
			{ PERL_UNUSED_CONTEXT; return (const t *)&(PL_##v); }
#define PERLVARISC(v,i)	PL_##v##_t* Perl_##v##_ptr(pTHX)	\
			{ dVAR; PERL_UNUSED_CONTEXT; return &(PL_##v); }
#include "perlvars.h"

#undef PERLVAR
#undef PERLVARA
#undef PERLVARI
#undef PERLVARIC
#undef PERLVARISC

#ifndef PERL_GLOBAL_STRUCT
/* A few evil special cases.  Could probably macrofy this. */
#undef PL_ppaddr
#undef PL_check
#undef PL_fold_locale
Perl_ppaddr_t** Perl_Gppaddr_ptr(pTHX) {
    static Perl_ppaddr_t* const ppaddr_ptr = PL_ppaddr;
    PERL_UNUSED_CONTEXT;
    return (Perl_ppaddr_t**)&ppaddr_ptr;
}
Perl_check_t**  Perl_Gcheck_ptr(pTHX) {
    static Perl_check_t* const check_ptr  = PL_check;
    PERL_UNUSED_CONTEXT;
    return (Perl_check_t**)&check_ptr;
}
unsigned char** Perl_Gfold_locale_ptr(pTHX) {
    static unsigned char* const fold_locale_ptr = PL_fold_locale;
    PERL_UNUSED_CONTEXT;
    return (unsigned char**)&fold_locale_ptr;
}
#endif

END_EXTERN_C

#endif /* MULTIPLICITY */

/* ex: set ro: */
EOT

safer_close($capi);
rename_if_different('perlapi.c-new', 'perlapi.c');

# functions that take va_list* for implementing vararg functions
# NOTE: makedef.pl must be updated if you add symbols to %vfuncs
# XXX %vfuncs currently unused
my %vfuncs = %( < qw(
    Perl_croak			Perl_vcroak
    Perl_warn			Perl_vwarn
    Perl_warner			Perl_vwarner
    Perl_die			Perl_vdie
    Perl_form			Perl_vform
    Perl_load_module		Perl_vload_module
    Perl_mess			Perl_vmess
    Perl_deb			Perl_vdeb
    Perl_newSVpvf		Perl_vnewSVpvf
    Perl_sv_setpvf		Perl_sv_vsetpvf
    Perl_sv_setpvf_mg		Perl_sv_vsetpvf_mg
    Perl_sv_catpvf		Perl_sv_vcatpvf
    Perl_sv_catpvf_mg		Perl_sv_vcatpvf_mg
    Perl_dump_indent		Perl_dump_vindent
    Perl_default_protect	Perl_vdefault_protect
) );

# ex: set ts=8 sts=4 sw=4 noet:
