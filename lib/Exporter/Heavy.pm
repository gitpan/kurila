package Exporter::Heavy;

use strict;
no strict 'refs';

# On one line so MakeMaker will see it.
require Exporter;  our $VERSION = $Exporter::VERSION;
# Carp does this now for us, so we can finally live w/o Carp
#$Carp::Internal{"Exporter::Heavy"} = 1;

=head1 NAME

Exporter::Heavy - Exporter guts

=head1 SYNOPSIS

(internal use only)

=head1 DESCRIPTION

No user-serviceable parts inside.

=cut

#
# We go to a lot of trouble not to 'require Carp' at file scope,
#  because Carp requires Exporter, and something has to give.
#

sub _rebuild_cache {
    my ($pkg, $exports, $cache) = @_;
    s/^&// foreach @$exports;
    @{$cache}{@$exports} = (1) x @$exports;
    my $ok = \@{*{Symbol::fetch_glob("${pkg}::EXPORT_OK")}};
    if (@$ok) {
	s/^&// foreach @$ok;
	@{$cache}{@$ok} = (1) x @$ok;
    }
}

sub heavy_export {

    my($pkg, $callpkg, @imports) = @_;
    my($type, $sym, $cache_is_current, $oops);
    my($exports, $export_cache) = (\@{*{Symbol::fetch_glob("${pkg}::EXPORT")}},
                                   $Exporter::Cache{$pkg} ||= {});

    if (@imports) {
	if (!%$export_cache) {
	    _rebuild_cache ($pkg, $exports, $export_cache);
	    $cache_is_current = 1;
	}

	if (grep m{^[/!:]}, @imports) {
	    my $tagsref = \%{*{Symbol::fetch_glob("${pkg}::EXPORT_TAGS")}};
	    my $tagdata;
	    my %imports;
	    my($remove, $spec, @names, @allexports);
	    # negated first item implies starting with default set:
	    unshift @imports, ':DEFAULT' if $imports[0] =~ m/^!/;
	    foreach $spec (@imports){
		$remove = $spec =~ s/^!//;

		if ($spec =~ s/^://){
		    if ($spec eq 'DEFAULT'){
			@names = @$exports;
		    }
		    elsif ($tagdata = $tagsref->{$spec}) {
			@names = @$tagdata;
		    }
		    else {
			warn qq["$spec" is not defined in %${pkg}::EXPORT_TAGS];
			++$oops;
			next;
		    }
		}
		elsif ($spec =~ m:^/(.*)/$:){
		    my $patn = $1;
		    @allexports = keys %$export_cache unless @allexports; # only do keys once
		    @names = grep(m/$patn/, @allexports); # not anchored by default
		}
		else {
		    @names = ($spec); # is a normal symbol name
		}

		warn "Import ".($remove ? "del":"add").": @names "
		    if $Exporter::Verbose;

		if ($remove) {
		   foreach $sym (@names) { delete $imports{$sym} } 
		}
		else {
		    @imports{@names} = (1) x @names;
		}
	    }
	    @imports = keys %imports;
	}

        my @carp;
	foreach $sym (@imports) {
	    if (!$export_cache->{$sym}) {
		if ($sym =~ m/^\d/) {
		    $pkg->VERSION($sym); # inherit from UNIVERSAL
		    # If the version number was the only thing specified
		    # then we should act as if nothing was specified:
		    if (@imports == 1) {
			@imports = @$exports;
			last;
		    }
		    # We need a way to emulate 'use Foo ()' but still
		    # allow an easy version check: "use Foo 1.23, ''";
		    if (@imports == 2 and !$imports[1]) {
			@imports = ();
			last;
		    }
		} elsif ($sym !~ s/^&// || !$export_cache->{$sym}) {
		    # Last chance - see if they've updated EXPORT_OK since we
		    # cached it.

		    unless ($cache_is_current) {
			%$export_cache = ();
			_rebuild_cache ($pkg, $exports, $export_cache);
			$cache_is_current = 1;
		    }

		    if (!$export_cache->{$sym}) {
			# accumulate the non-exports
			push @carp,
			  qq["$sym" is not exported by the $pkg module\n];
			$oops++;
		    }
		}
	    }
	}
	if ($oops) {
	    die("@{carp}Can't continue after import errors");
	}
    }
    else {
	@imports = @$exports;
    }

    my($fail, $fail_cache) = (\@{*{Symbol::fetch_glob("${pkg}::EXPORT_FAIL")}},
                              $Exporter::FailCache{$pkg} ||= {});

    if (@$fail) {
	if (!%$fail_cache) {
	    # Build cache of symbols. Optimise the lookup by adding
	    # barewords twice... both with and without a leading &.
	    # (Technique could be applied to $export_cache at cost of memory)
	    my @expanded = map { m/^\w/ ? ($_, '&'.$_) : $_ } @$fail;
	    warn "${pkg}::EXPORT_FAIL cached: @expanded" if $Exporter::Verbose;
	    @{$fail_cache}{@expanded} = (1) x @expanded;
	}
	my @failed;
	foreach $sym (@imports) { push(@failed, $sym) if $fail_cache->{$sym} }
	if (@failed) {
	    @failed = $pkg->export_fail(@failed);
	    foreach $sym (@failed) {
                require Carp;
		Carp::carp(qq["$sym" is not implemented by the $pkg module ],
			"on this architecture");
	    }
	    if (@failed) {
		require Carp;
		Carp::croak("Can't continue after import errors");
	    }
	}
    }

    warn "Importing into $callpkg from $pkg: ",
		join(", ",sort @imports) if $Exporter::Verbose;

    foreach $sym (@imports) {
	# shortcut for the common case of no type character
	(*{Symbol::fetch_glob("${callpkg}::$sym")} = \&{*{Symbol::fetch_glob("${pkg}::$sym")}}, next)
	    unless $sym =~ s/^(\W)//;
	$type = $1;
	no warnings 'once';
	*{Symbol::fetch_glob("${callpkg}::$sym")} =
	    $type eq '&' ? \&{*{Symbol::fetch_glob("${pkg}::$sym")}} :
	    $type eq '$' ? \${*{Symbol::fetch_glob("${pkg}::$sym")}} :
	    $type eq '@' ? \@{*{Symbol::fetch_glob("${pkg}::$sym")}} :
	    $type eq '%' ? \%{*{Symbol::fetch_glob("${pkg}::$sym")}} :
	    $type eq '*' ?  *{Symbol::fetch_glob("${pkg}::$sym")} :
	    do { require Carp; Carp::croak("Can't export symbol: $type$sym") };
    }
}

sub heavy_export_to_level
{
      my $pkg = shift;
      my $level = shift;
      (undef) = shift;			# XXX redundant arg
      my $callpkg = caller($level);
      $pkg->export($callpkg, @_);
}

# Utility functions

sub _push_tags {
    my($pkg, $var, $syms) = @_;
    my @nontag = ();
    my $export_tags = \%{*{Symbol::fetch_glob("${pkg}::EXPORT_TAGS")}};
    push(@{*{Symbol::fetch_glob("${pkg}::$var")}},
	map { $export_tags->{$_} ? @{$export_tags->{$_}} 
                                 : scalar(push(@nontag,$_),$_) }
		(@$syms) ? @$syms : keys %$export_tags);
    if (@nontag and $^W) {
	# This may change to a die one day
	require Carp;
	Carp::carp(join(", ", @nontag)." are not tags of $pkg");
    }
}

sub heavy_require_version {
    my($self, $wanted) = @_;
    my $pkg = ref $self || $self;
    return ${pkg}->VERSION($wanted);
}

sub heavy_export_tags {
  _push_tags((caller)[0], "EXPORT",    \@_);
}

sub heavy_export_ok_tags {
  _push_tags((caller)[0], "EXPORT_OK", \@_);
}

1;
