
package Pod::Simple::HTMLBatch;
use strict;
use vars qw( $VERSION $HTML_RENDER_CLASS $HTML_EXTENSION
 $CSS $JAVASCRIPT $SLEEPY $SEARCH_CLASS @ISA
);
$VERSION = '3.02';
@ISA = @( () );  # Yup, we're NOT a subclass of Pod::Simple::HTML!

# TODO: nocontents stylesheets. Strike some of the color variations?

use Pod::Simple::HTML ();
BEGIN {*esc = \&Pod::Simple::HTML::esc }
use File::Spec ();
use UNIVERSAL ();
  # "Isn't the Universe an amazing place?  I wouldn't live anywhere else!"

use Pod::Simple::Search;
$SEARCH_CLASS ||= 'Pod::Simple::Search';

BEGIN {
  if(defined &DEBUG) { } # no-op
  elsif( defined &Pod::Simple::DEBUG ) { *DEBUG = \&Pod::Simple::DEBUG }
  else { *DEBUG = sub () {0}; }
}

$SLEEPY = 1 if !defined $SLEEPY and $^O =~ m/mswin|mac/i;
# flag to occasionally sleep for $SLEEPY - 1 seconds.

$HTML_RENDER_CLASS ||= "Pod::Simple::HTML";

#
# Methods beginning with "_" are particularly internal and possibly ugly.
#

Pod::Simple::_accessorize( __PACKAGE__,
 'verbose', # how verbose to be during batch conversion
 'html_render_class', # what class to use to render
 'contents_file', # If set, should be the name of a file (in current directory)
                  # to write the list of all modules to
 'index', # will set $htmlpage->index(...) to this (true or false)
 'progress', # progress object
 'contents_page_start',  'contents_page_end',

 'css_flurry', '_css_wad', 'javascript_flurry', '_javascript_wad',
 'no_contents_links', # set to true to suppress automatic adding of << links.
 '_contents',
);

# - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
# Just so we can run from the command line more easily
sub go {
  (nelems @ARGV) == 2 or die sprintf(
    "Usage: perl -M\%s -e \%s:go indirs outdir\n  (or use \"\@INC\" for indirs)\n",
    __PACKAGE__, __PACKAGE__, 
  );
  
  if(defined(@ARGV[1]) and length(@ARGV[1])) {
    my $d = @ARGV[1];
    -e $d or die "I see no output directory named \"$d\"\nAborting";
    -d $d or die "But \"$d\" isn't a directory!\nAborting";
    -w $d or die "Directory \"$d\" isn't writeable!\nAborting";
  }
  
  __PACKAGE__->batch_convert(< @ARGV);
}
# - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -


sub new {
  my $new = bless \%(), ref(@_[0]) || @_[0];
  $new->html_render_class($HTML_RENDER_CLASS);
  $new->verbose(1 + DEBUG);
  $new->_contents(\@());
  
  $new->index(1);

  $new->       _css_wad(\@());         $new->css_flurry(1);
  $new->_javascript_wad(\@());  $new->javascript_flurry(1);
  
  $new->contents_file(
    'index' . ($HTML_EXTENSION || $Pod::Simple::HTML::HTML_EXTENSION)
  );
  
  $new->contents_page_start( join "\n", grep $_,
    $Pod::Simple::HTML::Doctype_decl,
    "<html><head>",
    "<title>Perl Documentation</title>",
    $Pod::Simple::HTML::Content_decl,
    "</head>",
    "\n<body class='contentspage'>\n<h1>Perl Documentation</h1>\n"
  ); # override if you need a different title
  
  
  $new->contents_page_end( sprintf(
    "\n\n<p class='contentsfooty'>Generated by \%s v\%s under Perl v\%s\n<br >At \%s GMT, which is \%s local time.</p>\n\n</body></html>\n",
    (map { esc($_) }
      ref($new),
      try {$new->VERSION} || $VERSION,
      $^V, scalar(gmtime), scalar(localtime), 
  )));

  return $new;
}

# - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

sub muse {
  my $self = shift;
  if($self->verbose) {
    print 'T+', int(time() - $self->{'_batch_start_time'}), "s: ", < @_, "\n";
  }
  return 1;
}

# - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

sub batch_convert {
  my($self, $dirs, $outdir) = < @_;
  $self ||= __PACKAGE__; # tolerate being called as an optionless function
  $self = $self->new unless ref $self; # tolerate being used as a class method

  if(ref $dirs) {
    # OK, it's an explicit set of dirs to scan, specified as an arrayref.
  } elsif(!defined($dirs)  or  $dirs eq ''  or  $dirs eq '@INC' ) {
    $dirs = '';
  } else {
    # OK, it's an explicit set of dirs to scan, specified as a
    #  string like "/thing:/also:/whatever/perl" (":"-delim, as usual)
    #  or, under MSWin, like "c:/thing;d:/also;c:/whatever/perl" (";"-delim!)
    require Config;
    my $ps = quotemeta( %Config::Config{'path_sep'} || ":" );
    $dirs = \@( grep length($_), split qr/$ps/, $dirs );
  }

  $outdir = $self->filespecsys->curdir
   unless defined $outdir and length $outdir;

  $self->_batch_convert_main($dirs, $outdir);
}

# - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

sub _batch_convert_main {
  my($self, $dirs, $outdir) = < @_;
  # $dirs is either false, or an arrayref.    
  # $outdir is a pathspec.
  
  $self->{'_batch_start_time'} ||= time();

  $self->muse( "= ", scalar(localtime) );
  $self->muse( "Starting batch conversion to \"$outdir\"" );

  my $progress = $self->progress;
  if(!$progress and $self->verbose +> 0 and $self->verbose() +<= 5) {
    require Pod::Simple::Progress;
    $progress = Pod::Simple::Progress->new(
        ($self->verbose  +< 2) ? () # Default omission-delay
      : ($self->verbose == 2) ? 1  # Reduce the omission-delay
                              : 0  # Eliminate the omission-delay
    );
    $self->progress($progress);
  }
  
  if($dirs) {
    $self->muse(scalar(nelems @$dirs), " dirs to scan: {join ' ', <@$dirs}");
  } else {
    $self->muse("Scanning \@INC.  This could take a minute or two.");
  }
  my $mod2path = $self->find_all_pods($dirs ? $dirs : ());
  $self->muse("Done scanning.");

  my $total = nkeys %$mod2path;
  unless($total) {
    $self->muse("No pod found.  Aborting batch conversion.\n");
    return $self;
  }

  $progress and $progress->goal($total);
  $self->muse("Now converting pod files to HTML.",
    ($total +> 25) ? "  This will take a while more." : ()
  );

  $self->_spray_css(        $outdir );
  $self->_spray_javascript( $outdir );

  $self->_do_all_batch_conversions($mod2path, $outdir);

  $progress and $progress->done(sprintf (
    "Done converting \%d files.",  $self->{"__batch_conv_page_count"}
  ));
  return $self->_batch_convert_finish($outdir);
  return $self;
}


sub _do_all_batch_conversions {
  my($self, $mod2path, $outdir) = < @_;
  $self->{"__batch_conv_page_count"} = 0;

  foreach my $module (sort {lc($a) cmp lc($b)} keys %$mod2path) {
    $self->_do_one_batch_conversion($module, $mod2path, $outdir);
    sleep($SLEEPY - 1) if $SLEEPY;
  }

  return;
}

sub _batch_convert_finish {
  my($self, $outdir) = < @_;
  $self->write_contents_file($outdir);
  $self->muse("Done with batch conversion.  %$self{'__batch_conv_page_count'} files done.");
  $self->muse( "= ", scalar(localtime) );
  $self->progress and $self->progress->done("All done!");
  return;
}

# - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

sub _do_one_batch_conversion {
  my($self, $module, $mod2path, $outdir, $outfile) = < @_;

  my $retval;
  my $total    = nkeys %$mod2path;
  my $infile   = $mod2path->{$module};
  my @namelets = @( grep m/\S/, split "::", $module );
        # this can stick around in the contents LoL
  my $depth    = scalar nelems @namelets;
  die "Contentless thingie?! $module $infile" unless (nelems @namelets); #sanity
    
  $outfile  ||= do {
    my @n = @( < @namelets );
    @n[-1] .= $HTML_EXTENSION || $Pod::Simple::HTML::HTML_EXTENSION;
    $self->filespecsys->catfile( $outdir, < @n );
  };

  my $progress = $self->progress;

  my $page = $self->html_render_class->new;
  if(DEBUG +> 5) {
    $self->muse($self->{"__batch_conv_page_count"} + 1, "/$total: ",
      ref($page), " render ($depth) $module => $outfile");
  } elsif(DEBUG +> 2) {
    $self->muse($self->{"__batch_conv_page_count"} + 1, "/$total: $module => $outfile")
  }

  # Give each class a chance to init the converter:
  
  $page->batch_mode_page_object_init($self, $module, $infile, $outfile, $depth)
   if $page->can('batch_mode_page_object_init');
  $self->batch_mode_page_object_init($page, $module, $infile, $outfile, $depth)
   if $self->can('batch_mode_page_object_init');
    
  # Now get busy...
  $self->makepath($outdir => \@namelets);

  $progress and $progress->reach($self->{"__batch_conv_page_count"}, "Rendering $module");

  if( $retval = $page->parse_from_file($infile, $outfile) ) {
    ++ $self->{"__batch_conv_page_count"} ;
    $self->note_for_contents_file( \@namelets, $infile, $outfile );
  } else {
    $self->muse("Odd, parse_from_file(\"$infile\", \"$outfile\") returned false.");
  }

  $page->batch_mode_page_object_kill($self, $module, $infile, $outfile, $depth)
   if $page->can('batch_mode_page_object_kill');
  # The following isn't a typo.  Note that it switches $self and $page.
  $self->batch_mode_page_object_kill($page, $module, $infile, $outfile, $depth)
   if $self->can('batch_mode_page_object_kill');
    
  DEBUG +> 4 and printf "\%s \%sb < $infile \%s \%sb\n",
     $outfile, -s $outfile, $infile, -s $infile
  ;

  undef($page);
  return $retval;
}

# - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
sub filespecsys { @_[0]->{'_filespecsys'} || 'File::Spec' }

# - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

sub note_for_contents_file {
  my($self, $namelets, $infile, $outfile) = < @_;

  # I think the infile and outfile parts are never used. -- SMB
  # But it's handy to have them around for debugging.

  if( $self->contents_file ) {
    my $c = $self->_contents();
    push @$c,
     \@( join("::", < @$namelets), $infile, $outfile, $namelets )
     #            0               1         2         3
    ;
    DEBUG +> 3 and print "Noting @$c[[-1]]\n";
  }
  return;
}

#_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-

sub write_contents_file {
  my($self, $outdir) = < @_;
  my $outfile  = $self->_contents_filespec($outdir) || return;

  $self->muse("Preparing list of modules for ToC");

  my($toplevel,           # maps  toplevelbit => [all submodules]
     $toplevel_form_freq, # ends up being  'foo' => 'Foo'
    ) = < $self->_prep_contents_breakdown;

  my $Contents = try { $self->_wopen($outfile) };
  if( $Contents ) {
    $self->muse( "Writing contents file $outfile" );
  } else {
    warn "Couldn't write-open contents file $outfile: $!\nAbort writing to $outfile at all";
    return;
  }

  $self->_write_contents_start(  $Contents, $outfile, );
  $self->_write_contents_middle( $Contents, $outfile, $toplevel, $toplevel_form_freq );
  $self->_write_contents_end(    $Contents, $outfile, );
  return $outfile;
}

# - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

sub _write_contents_start {
  my($self, $Contents, $outfile) = < @_;
  my $starter = $self->contents_page_start || '';
  
  {
    my $css_wad = $self->_css_wad_to_markup(1);
    if( $css_wad ) {
      $starter =~ s{(</head>)}{\n$css_wad\n$1}i;  # otherwise nevermind
    }
    
    my $javascript_wad = $self->_javascript_wad_to_markup(1);
    if( $javascript_wad ) {
      $starter =~ s{(</head>)}{\n$javascript_wad\n$1}i;   # otherwise nevermind
    }
  }

  unless(print $Contents $starter, "<dl class='superindex'>\n" ) {
    warn "Couldn't print to $outfile: $!\nAbort writing to $outfile at all";
    close($Contents);
    return 0;
  }
  return 1;
}

# - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

sub _write_contents_middle {
  my($self, $Contents, $outfile, $toplevel2submodules, $toplevel_form_freq) = < @_;

  foreach my $t (sort keys %$toplevel2submodules) {
    my @downlines = @( sort {$a->[-1] cmp $b->[-1]}
                          < @{ $toplevel2submodules->{$t} } );
    
    printf $Contents qq[<dt><a name="\%s">\%s</a></dt>\n<dd>\n],
      esc( $t ), esc( $toplevel_form_freq->{$t} )
    ;
    
    my($path, $name);
    foreach my $e (< @downlines) {
      $name = $e->[0];
      $path = join( "/", '.', map { esc($_) } < @{$e->[3]} )
        . ($HTML_EXTENSION || $Pod::Simple::HTML::HTML_EXTENSION);
      print $Contents qq{  <a href="$path">}, esc($name), "</a>&nbsp;&nbsp;\n";
    }
    print $Contents "</dd>\n\n";
  }
  return 1;
}

# - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

sub _write_contents_end {
  my($self, $Contents, $outfile) = < @_;
  unless(
    print $Contents "</dl>\n",
      $self->contents_page_end || '',
  ) {
    warn "Couldn't write to $outfile: $!";
  }
  close($Contents) or warn "Couldn't close $outfile: $!";
  return 1;
}

# - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

sub _prep_contents_breakdown {
  my($self) = < @_;
  my $contents = $self->_contents;
  my %toplevel; # maps  lctoplevelbit => [all submodules]
  my %toplevel_form_freq; # ends up being  'foo' => 'Foo'
                               # (mapping anycase forms to most freq form)
  
  foreach my $entry (< @$contents) {
    my $toplevel = 
      $entry->[0] =~ m/^perl\w*$/ ? 'perl_core_docs'
          # group all the perlwhatever docs together
      : $entry->[3]->[0] # normal case
    ;
    ++%toplevel_form_freq{ lc $toplevel }->{ $toplevel };
    push @{ %toplevel{ lc $toplevel } }, $entry;
    push @$entry, lc($entry->[0]); # add a sort-order key to the end
  }

  foreach my $toplevel (sort keys %toplevel) {
    my $fgroup = %toplevel_form_freq{$toplevel};
    %toplevel_form_freq{$toplevel} =
    (
      sort { $fgroup->{$b} <+> $fgroup->{$a}  or  $a cmp $b }
        keys %$fgroup
      # This hash is extremely unlikely to have more than 4 members, so this
      # sort isn't so very wasteful
    )[[0]];
  }

  return @(\%toplevel, \%toplevel_form_freq);
}

# - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

sub _contents_filespec {
  my($self, $outdir) = < @_;
  my $outfile = $self->contents_file;
  return unless $outfile;
  return $self->filespecsys->catfile( $outdir, $outfile );
}

#_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-

sub makepath {
  my($self, $outdir, $namelets) = < @_;
  return unless (nelems @$namelets) +> 1;
  for my $i (0 .. ((nelems @$namelets) - 2)) {
    my $dir = $self->filespecsys->catdir( $outdir, @$namelets[[0 .. $i]] );
    if(-e $dir) {
      die "$dir exists but not as a directory!?" unless -d $dir;
      next;
    }
    DEBUG +> 3 and print "  Making $dir\n";
    mkdir $dir, 0777
     or die "Can't mkdir $dir: $!\nAborting"
    ;
  }
  return;
}

#_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-

sub batch_mode_page_object_init {
  my $self = shift;
  my($page, $module, $infile, $outfile, $depth) = < @_;
  
  # TODO: any further options to percolate onto this new object here?

  $page->default_title($module);
  $page->index( $self->index );

  $page->html_css(         $self->       _css_wad_to_markup($depth) );
  $page->html_javascript(  $self->_javascript_wad_to_markup($depth) );

  $self->add_header_backlink($page, $module, $infile, $outfile, $depth);
  $self->add_footer_backlink($page, $module, $infile, $outfile, $depth);


  return $self;
}

# - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

sub add_header_backlink {
  my $self = shift;
  return if $self->no_contents_links;
  my($page, $module, $infile, $outfile, $depth) = < @_;
  $page->html_header_after_title( join '',
    $page->html_header_after_title || '',

    qq[<p class="backlinktop"><b><a name="___top" href="],
    $self->url_up_to_contents($depth),
    qq[" accesskey="1" title="All Documents">&lt;&lt;</a></b></p>\n],
  )
   if $self->contents_file
  ;
  return;
}

sub add_footer_backlink {
  my $self = shift;
  return if $self->no_contents_links;
  my($page, $module, $infile, $outfile, $depth) = < @_;
  $page->html_footer( join '',
    qq[<p class="backlinkbottom"><b><a name="___bottom" href="],
    $self->url_up_to_contents($depth),
    qq[" title="All Documents">&lt;&lt;</a></b></p>\n],
    
    $page->html_footer || '',
  )
   if $self->contents_file
  ;
  return;
}

sub url_up_to_contents {
  my($self, $depth) = < @_;
  --$depth;
  return join '/', ('..') x $depth, esc($self->contents_file);
}

#_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-

sub find_all_pods {
  my($self, $dirs) = < @_;
  # You can override find_all_pods in a subclass if you want to
  #  do extra filtering or whatnot.  But for the moment, we just
  #  pass to modnames2paths:
  return $self->modnames2paths($dirs);
}

#_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-_-

sub modnames2paths { # return a hashref mapping modulenames => paths
  my($self, $dirs) = < @_;

  my $m2p;
  {
    my $search = $SEARCH_CLASS->new;
    DEBUG and print "Searching via $search\n";
    $search->verbose(1) if DEBUG +> 10;
    $search->progress( < $self->progress->copy->goal(0) ) if $self->progress;
    $search->shadows(0);  # don't bother noting shadowed files
    $search->inc(     $dirs ? 0      :  1 );
    $search->survey(  $dirs ? < @$dirs : () );
    $m2p = $search->name2path;
    die "What, no name2path?!" unless $m2p;
  }

  $self->muse("That's odd... no modules found!") unless %$m2p;
  if( DEBUG +> 4 ) {
    print "Modules found (name => path):\n";
    foreach my $m (sort {lc($a) cmp lc($b)} keys %$m2p) {
      print "  $m  %$m2p{$m}\n";
    }
    print "(total ",     nkeys %$m2p, ")\n\n";
  } elsif( DEBUG ) {
    print      "Found ", nkeys %$m2p, " modules.\n";
  }
  $self->muse( "Found ", nkeys %$m2p, " modules." );
  
  # return the Foo::Bar => /whatever/Foo/Bar.pod|pm hashref
  return $m2p;
}

#===========================================================================

sub _wopen {
  # this is abstracted out so that the daemon class can override it
  my($self, $outpath) = < @_;
  require Symbol;
  my $out_fh = Symbol::gensym();
  DEBUG +> 5 and print "Write-opening to $outpath\n";
  return $out_fh if open($out_fh, ">", "$outpath");
  require Carp;  
  Carp::croak("Can't write-open $outpath: $!");
}

#==========================================================================

sub add_css {
  my($self, $url, $is_default, $name, $content_type, $media, $_code) = < @_;
  return unless $url;
  unless($name) {
    # cook up a reasonable name based on the URL
    $name = $url;
    if( $name !~ m/\?/ and $name =~ m{([^/]+)$}s ) {
      $name = $1;
      $name =~ s/\.css//i;
    }
  }
  $media        ||= 'all';
  $content_type ||= 'text/css';
  
  my $bunch = \@($url, $name, $content_type, $media, $_code);
  if($is_default) { unshift @{ $self->_css_wad }, $bunch }
  else            { push    @{ $self->_css_wad }, $bunch }
  return;
}

# - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

sub _spray_css {
  my($self, $outdir) = < @_;

  return unless $self->css_flurry();
  $self->_gen_css_wad();

  my $lol = $self->_css_wad;
  foreach my $chunk (< @$lol) {
    my $url = $chunk->[0];
    my $outfile;
    if( ref($chunk->[-1]) and $url =~ m{^(_[-a-z0-9_]+\.css$)} ) {
      $outfile = $self->filespecsys->catfile( $outdir, $1 );
      DEBUG +> 5 and print "Noting @$chunk[0] as a file I'll create.\n";
    } else {
      DEBUG +> 5 and print "OK, noting @$chunk[0] as an external CSS.\n";
      # Requires no further attention.
      next;
    }
    
    #$self->muse( "Writing autogenerated CSS file $outfile" );
    my $Cssout = $self->_wopen($outfile);
    print $Cssout ${$chunk->[-1]}
     or warn "Couldn't print to $outfile: $!\nAbort writing to $outfile at all";
    close($Cssout);
    DEBUG +> 5 and print "Wrote $outfile\n";
  }

  return;
}

# - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

sub _css_wad_to_markup {
  my($self, $depth) = < @_;
  
  my @css  = @( < @{ $self->_css_wad || return '' } );
  return '' unless (nelems @css);
  
  my $rel = 'stylesheet';
  my $out = '';

  --$depth;
  my $uplink = $depth ? ('../' x $depth) : '';

  foreach my $chunk (< @css) {
    next unless $chunk and nelems @$chunk;

    my( $url1, $url2, $title, $type, $media) = (
      $self->_maybe_uplink( $chunk->[0], $uplink ),
      map { esc($_) } (grep !ref($_), < @$chunk)
    );

    $out .= qq{<link rel="$rel" title="$title" type="$type" href="$url1$url2" media="$media" >\n};

    $rel = 'alternate stylesheet'; # alternates = all non-first iterations
  }
  return $out;
}

# - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
sub _maybe_uplink {
  # if the given URL looks relative, return the given uplink string --
  # otherwise return emptystring
  my($self, $url, $uplink) = < @_;
  ($url =~ m{^\./} or $url !~ m{[/\:]} )
    ? $uplink
    : ''
    # qualify it, if/as needed
}

# - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
sub _gen_css_wad {
  my $self = @_[0];
  my $css_template = $self->_css_template;
  foreach my $variation (

   # Commented out for sake of concision:
   #
   #  011n=black_with_red_on_white
   #  001n=black_with_yellow_on_white
   #  101n=black_with_green_on_white
   #  110=white_with_yellow_on_black
   #  010=white_with_green_on_black
   #  011=white_with_blue_on_black
   #  100=white_with_red_on_black
  
   qw[
    110n=black_with_blue_on_white
    010n=black_with_magenta_on_white
    100n=black_with_cyan_on_white

    101=white_with_purple_on_black
    001=white_with_navy_blue_on_black

    010a=grey_with_green_on_black
    010b=white_with_green_on_grey
    101an=black_with_green_on_grey
    101bn=grey_with_green_on_white
  ]) {

    my $outname = $variation;
    my($flipmode, < @swap) = ( ($4 || ''), $1,$2,$3)
      if $outname =~ s/^([012])([012])([[012])([a-z]*)=?//s;
    @swap = @( () ) if '010' eq join '', < @swap; # 010 is a swop-no-op!
  
    my $this_css =
      "/* This file is autogenerated.  Do not edit.  $variation */\n\n"
      . $css_template;

    # Only look at three-digitty colors, for now at least.
    if( $flipmode =~ m/n/ ) {
      $this_css =~ s/(#[0-9a-fA-F]{3})\b/{_color_negate($1)}/g;
      $this_css =~ s/\bthin\b/medium/g;
    }
    $this_css =~ s<#([0-9a-fA-F])([0-9a-fA-F])([0-9a-fA-F])\b>
                  |{ join '', '#', ($1,$2,$3)[[< @swap]] }|g   if (nelems @swap);

    if(   $flipmode =~ m/a/)
       { $this_css =~ s/#fff\b/#999/gi } # black -> dark grey
    elsif($flipmode =~ m/b/)
       { $this_css =~ s/#000\b/#666/gi } # white -> light grey

    my $name = $outname;    
    $name =~ s/-|_/ /g;
    $self->add_css( "_$outname.css", 0, $name, 0, 0, \$this_css);
  }

  # Now a few indexless variations:
  foreach my $variation (qw[
    black_with_blue_on_white  white_with_purple_on_black
    white_with_green_on_grey  grey_with_green_on_white
  ]) {
    my $outname = "indexless_$variation";
    my $this_css = join "\n",
      "/* This file is autogenerated.  Do not edit.  $outname */\n",
      "\@import url(\"./_$variation.css\");",
      ".indexgroup \{ display: none; \}",
      "\n",
    ;
    my $name = $outname;    
    $name =~ s/-|_/ /g;
    $self->add_css( "_$outname.css", 0, $name, 0, 0, \$this_css);
  }

  return;
}

sub _color_negate {
  my $x = lc @_[0];
  $x =~ s/([0123456789abcdef])/{ 
     %( qw| 0 f 1 e 2 d 3 c 4 b 5 a 6 9 7 8 8 7 9 6 a 5 b 4 c 3 d 2 e 1 f 0 | ){$1} }/g;
  return $x;
}

#===========================================================================

sub add_javascript {
  my($self, $url, $content_type, $_code) = < @_;
  return unless $url;
  push  @{ $self->_javascript_wad }, \@(
    $url, $content_type || 'text/javascript', $_code
  );
  return;
}

sub _spray_javascript {
  my($self, $outdir) = < @_;
  return unless $self->javascript_flurry();
  $self->_gen_javascript_wad();

  my $lol = $self->_javascript_wad;
  foreach my $script (< @$lol) {
    my $url = $script->[0];
    my $outfile;
    
    if( ref($script->[-1]) and $url =~ m{^(_[-a-z0-9_]+\.js$)} ) {
      $outfile = $self->filespecsys->catfile( $outdir, $1 );
      DEBUG +> 5 and print "Noting @$script[0] as a file I'll create.\n";
    } else {
      DEBUG +> 5 and print "OK, noting @$script[0] as an external JavaScript.\n";
      next;
    }
    
    #$self->muse( "Writing JavaScript file $outfile" );
    my $Jsout = $self->_wopen($outfile);

    print $Jsout ${$script->[-1]}
     or warn "Couldn't print to $outfile: $!\nAbort writing to $outfile at all";
    close($Jsout);
    DEBUG +> 5 and print "Wrote $outfile\n";
  }

  return;
}

sub _gen_javascript_wad {
  my $self = @_[0];
  my $js_code = $self->_javascript || return;
  $self->add_javascript( "_podly.js", 0, \$js_code);
  return;
}

sub _javascript_wad_to_markup {
  my($self, $depth) = < @_;
  
  my @scripts  = @( < @{ $self->_javascript_wad || return '' } );
  return '' unless (nelems @scripts);
  
  my $out = '';

  --$depth;
  my $uplink = $depth ? ('../' x $depth) : '';

  foreach my $s (< @scripts) {
    next unless $s and nelems @$s;

    my( $url1, $url2, $type, $media) = (
      $self->_maybe_uplink( $s->[0], $uplink ),
      map { esc($_) } (grep !ref($_), < @$s)
    );

    $out .= qq{<script type="$type" src="$url1$url2"></script>\n};
  }
  return $out;
}

#===========================================================================

sub _css_template { return $CSS }
sub _javascript   { return $JAVASCRIPT }

$CSS = <<'EOCSS';
/* For accessibility reasons, never specify text sizes in px/pt/pc/in/cm/mm */

@media all { .hide { display: none; } }

@media print {
  .noprint, div.indexgroup, .backlinktop, .backlinkbottom { display: none }

  * {
    border-color: black !important;
    color: black !important;
    background-color: transparent !important;
    background-image: none !important;
  }

  dl.superindex > dd  {
    word-spacing: .6em;
  }
}

@media aural, braille, embossed {
  div.indexgroup  { display: none; }  /* Too noisy, don't you think? */
  dl.superindex > dt:before { content: "Group ";  }
  dl.superindex > dt:after  { content: " contains:"; }
  .backlinktop    a:before  { content: "Back to contents"; }
  .backlinkbottom a:before  { content: "Back to contents"; }
}

@media aural {
  dl.superindex > dt  { pause-before: 600ms; }
}

@media screen, tty, tv, projection {
  .noscreen { display: none; }

  a:link    { color: #7070ff; text-decoration: underline; }
  a:visited { color: #e030ff; text-decoration: underline; }
  a:active  { color: #800000; text-decoration: underline; }
  body.contentspage a            { text-decoration: none; }
  a.u { color: #fff !important; text-decoration: none; }

  body.pod {
    margin: 0 5px;
    color:            #fff;
    background-color: #000;
  }

  body.pod h1, body.pod h2, body.pod h3, body.pod h4  {
    font-family: Tahoma, Verdana, Helvetica, Arial, sans-serif;
    font-weight: normal;
    margin-top: 1.2em;
    margin-bottom: .1em;
    border-top: thin solid transparent;
    /* margin-left: -5px;  border-left: 2px #7070ff solid;  padding-left: 3px; */
  }
  
  body.pod h1  { border-top-color: #0a0; }
  body.pod h2  { border-top-color: #080; }
  body.pod h3  { border-top-color: #040; }
  body.pod h4  { border-top-color: #010; }

  p.backlinktop + h1 { border-top: none; margin-top: 0em;  }
  p.backlinktop + h2 { border-top: none; margin-top: 0em;  }
  p.backlinktop + h3 { border-top: none; margin-top: 0em;  }
  p.backlinktop + h4 { border-top: none; margin-top: 0em;  }

  body.pod dt {
    font-size: 105%; /* just a wee bit more than normal */
  }

  .indexgroup { font-size: 80%; }

  .backlinktop,   .backlinkbottom    {
    margin-left:  -5px;
    margin-right: -5px;
    background-color:         #040;
    border-top:    thin solid #050;
    border-bottom: thin solid #050;
  }
  
  .backlinktop a, .backlinkbottom a  {
    text-decoration: none;
    color: #080;
    background-color:  #000;
    border: thin solid #0d0;
  }
  .backlinkbottom { margin-bottom: 0; padding-bottom: 0; }
  .backlinktop    { margin-top:    0; padding-top:    0; }

  body.contentspage {
    color:            #fff;
    background-color: #000;
  }
  
  body.contentspage h1  {
    color:            #0d0;
    margin-left: 1em;
    margin-right: 1em;
    text-indent: -.9em;
    font-family: Tahoma, Verdana, Helvetica, Arial, sans-serif;
    font-weight: normal;
    border-top:    thin solid #fff;
    border-bottom: thin solid #fff;
    text-align: center;
  }

  dl.superindex > dt  {
    font-family: Tahoma, Verdana, Helvetica, Arial, sans-serif;
    font-weight: normal;
    font-size: 90%;
    margin-top: .45em;
    /* margin-bottom: -.15em; */
  }
  dl.superindex > dd  {
    word-spacing: .6em;    /* most important rule here! */
  }
  dl.superindex > a:link  {
    text-decoration: none;
    color: #fff;
  }

  .contentsfooty {
    border-top: thin solid #999;
    font-size: 90%;
  }
  
}

/* The End */

EOCSS

#==========================================================================

$JAVASCRIPT = <<'EOJAVASCRIPT';

// From http://www.alistapart.com/articles/alternate/

function setActiveStyleSheet(title) {
  var i, a, main;
  for(i=0  ;  (a = document.getElementsByTagName("link")[i])  ;  i++) {
    if(a.getAttribute("rel").indexOf("style") != -1 && a.getAttribute("title")) {
      a.disabled = true;
      if(a.getAttribute("title") == title) a.disabled = false;
    }
  }
}

function getActiveStyleSheet() {
  var i, a;
  for(i=0  ;  (a = document.getElementsByTagName("link")[i])  ;  i++) {
    if(   a.getAttribute("rel").indexOf("style") != -1
       && a.getAttribute("title")
       && !a.disabled
       ) return a.getAttribute("title");
  }
  return null;
}

function getPreferredStyleSheet() {
  var i, a;
  for(i=0  ;  (a = document.getElementsByTagName("link")[i])  ;  i++) {
    if(   a.getAttribute("rel").indexOf("style") != -1
       && a.getAttribute("rel").indexOf("alt") == -1
       && a.getAttribute("title")
       ) return a.getAttribute("title");
  }
  return null;
}

function createCookie(name,value,days) {
  if (days) {
    var date = new Date();
    date.setTime(date.getTime()+(days*24*60*60*1000));
    var expires = "; expires="+date.toGMTString();
  }
  else expires = "";
  document.cookie = name+"="+value+expires+"; path=/";
}

function readCookie(name) {
  var nameEQ = name + "=";
  var ca = document.cookie.split(';');
  for(var i=0  ;  i < ca.length  ;  i++) {
    var c = ca[i];
    while (c.charAt(0)==' ') c = c.substring(1,c.length);
    if (c.indexOf(nameEQ) == 0) return c.substring(nameEQ.length,c.length);
  }
  return null;
}

window.onload = function(e) {
  var cookie = readCookie("style");
  var title = cookie ? cookie : getPreferredStyleSheet();
  setActiveStyleSheet(title);
}

window.onunload = function(e) {
  var title = getActiveStyleSheet();
  createCookie("style", title, 365);
}

var cookie = readCookie("style");
var title = cookie ? cookie : getPreferredStyleSheet();
setActiveStyleSheet(title);

// The End

EOJAVASCRIPT

# - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
1;
__END__


=head1 NAME

Pod::Simple::HTMLBatch - convert several Pod files to several HTML files

=head1 SYNOPSIS

  perl -MPod::Simple::HTMLBatch -e 'Pod::Simple::HTMLBatch::go' in out


=head1 DESCRIPTION

This module is used for running batch-conversions of a lot of HTML
documents 

This class is NOT a subclass of Pod::Simple::HTML
(nor of bad old Pod::Html) -- although it uses
Pod::Simple::HTML for doing the conversion of each document.

The normal use of this class is like so:

  use Pod::Simple::HTMLBatch;
  my $batchconv = Pod::Simple::HTMLBatch->new;
  $batchconv->some_option( some_value );
  $batchconv->some_other_option( some_other_value );
  $batchconv->batch_convert( \@search_dirs, $output_dir );

=head2 FROM THE COMMAND LINE

Note that this class also provides
(but does not export) the function Pod::Simple::HTMLBatch::go.
This is basically just a shortcut for C<<
Pod::Simple::HTMLBatch->batch_convert(@ARGV) >>.
It's meant to be handy for calling from the command line.

However, the shortcut requires that you specify exactly two command-line
arguments, C<indirs> and C<outdir>.

Example:

  % mkdir out_html
  % perl -MPod::Simple::HTMLBatch -e Pod::Simple::HTMLBatch::go @INC out_html
      (to convert the pod from Perl's @INC
       files under the directory ../htmlversion)

(Note that the command line there contains a literal atsign-I-N-C.  This
is handled as a special case by batch_convert, in order to save you having
to enter the odd-looking "" as the first command-line parameter when you
mean "just use whatever's in @INC".)

Example:

  % mkdir ../seekrut
  % chmod og-rx ../seekrut
  % perl -MPod::Simple::HTMLBatch -e Pod::Simple::HTMLBatch::go . ../htmlversion
      (to convert the pod under the current dir into HTML
       files under the directory ../htmlversion)

Example:

  % perl -MPod::Simple::HTMLBatch -e Pod::Simple::HTMLBatch::go happydocs .
      (to convert all pod from happydocs into the current directory)



=head1 MAIN METHODS

=over

=item $batchconv = Pod::Simple::HTMLBatch->new;

This TODO


=item $batchconv->batch_convert( I<indirs>, I<outdir> );

this TODO

=item $batchconv->batch_convert( undef    , ...);

=item $batchconv->batch_convert( q{@INC}, ...);

These two values for I<indirs> specify that the normal Perl @INC

=item $batchconv->batch_convert( \@dirs , ...);

This specifies that the input directories are the items in
the arrayref C<\@dirs>.

=item $batchconv->batch_convert( "somedir" , ...);

This specifies that the director "somedir" is the input.
(This can be an absolute or relative path, it doesn't matter.)

A common value you might want would be just "." for the current
directory:

     $batchconv->batch_convert( "." , ...);


=item $batchconv->batch_convert( 'somedir:someother:also' , ...);

This specifies that you want the dirs "somedir", "somother", and "also"
scanned, just as if you'd passed the arrayref
C<[qw( somedir someother also)]>.  Note that a ":"-separator is normal
under Unix, but Under MSWin, you'll need C<'somedir;someother;also'>
instead, since the pathsep on MSWin is ";" instead of ":".  (And
I<that> is because ":" often comes up in paths, like
C<"c:/perl/lib">.)

(Exactly what separator character should be used, is gotten from
C<$Config::Config{'path_sep'}>, via the L<Config> module.)

=item $batchconv->batch_convert( ... , undef );

This specifies that you want the HTML output to go into the current
directory.

(Note that a missing or undefined value means a different thing in
the first slot than in the second.  That's so that C<batch_convert()>
with no arguments (or undef arguments) means "go from @INC, into
the current directory.)

=item $batchconv->batch_convert( ... , 'somedir' );

This specifies that you want the HTML output to go into the
directory 'somedir'.
(This can be an absolute or relative path, it doesn't matter.)

=back


Note that you can also call C<batch_convert> as a class method,
like so:

  Pod::Simple::HTMLBatch->batch_convert( ... );

That is just short for this:

  Pod::Simple::HTMLBatch-> new-> batch_convert(...);

That is, it runs a conversion with default options, for
whatever inputdirs and output dir you specify.


=head2 ACCESSOR METHODS

The following are all accessor methods -- that is, they don't do anything
on their own, but just alter the contents of the conversion object,
which comprises the options for this particular batch conversion.

We show the "put" form of the accessors below (i.e., the syntax you use
for setting the accessor to a specific value).  But you can also
call each method with no parameters to get its current value.  For
example, C<< $self->contents_file() >> returns the current value of
the contents_file attribute.

=over


=item $batchconv->verbose( I<nonnegative_integer> );

This controls how verbose to be during batch conversion, as far as
notes to STDOUT (or whatever is C<select>'d) about how the conversion
is going.  If 0, no progress information is printed.
If 1 (the default value), some progress information is printed.
Higher values print more information.


=item $batchconv->index( I<true-or-false> );

This controls whether or not each HTML page is liable to have a little
table of contents at the top (which we call an "index" for historical
reasons).  This is true by default.


=item $batchconv->contents_file( I<filename> );

If set, should be the name of a file (in the output directory)
to write the HTML index to.  The default value is "index.html".
If you set this to a false value, no contents file will be written.

=item $batchconv->contents_page_start( I<HTML_string> );

This specifies what string should be put at the beginning of
the contents page.
The default is a string more or less like this:
  
  <html>
  <head><title>Perl Documentation</title></head>
  <body class='contentspage'>
  <h1>Perl Documentation</h1>

=item $batchconv->contents_page_end( I<HTML_string> );

This specifies what string should be put at the end of the contents page.
The default is a string more or less like this:

  <p class='contentsfooty'>Generated by
  Pod::Simple::HTMLBatch v3.01 under Perl v5.008
  <br >At Fri May 14 22:26:42 2004 GMT,
  which is Fri May 14 14:26:42 2004 local time.</p>



=item $batchconv->add_css( $url );

TODO

=item $batchconv->add_javascript( $url );

TODO

=item $batchconv->css_flurry( I<true-or-false> );

If true (the default value), we autogenerate some CSS files in the
output directory, and set our HTML files to use those.
TODO: continue

=item $batchconv->javascript_flurry( I<true-or-false> );

If true (the default value), we autogenerate a JavaScript in the
output directory, and set our HTML files to use it.  Currently,
the JavaScript is used only to get the browser to remember what
stylesheet it prefers.
TODO: continue

=item $batchconv->no_contents_links( I<true-or-false> );

TODO

=item $batchconv->html_render_class( I<classname> );

This sets what class is used for rendering the files.
The default is "Pod::Simple::Search".  If you set it to something else,
it should probably be a subclass of Pod::Simple::Search, and you should
C<require> or C<use> that class so that's it's loaded before
Pod::Simple::HTMLBatch tries loading it.

=back




=head1 NOTES ON CUSTOMIZATION

TODO

  call add_css($someurl) to add stylesheet as alternate
  call add_css($someurl,1) to add as primary stylesheet

  call add_javascript

  subclass Pod::Simple::HTML and set $batchconv->html_render_class to
    that classname
  and maybe override
    $page->batch_mode_page_object_init($self, $module, $infile, $outfile, $depth)
  or maybe override
    $batchconv->batch_mode_page_object_init($page, $module, $infile, $outfile, $depth)



=head1 ASK ME!

If you want to do some kind of big pod-to-HTML version with some
particular kind of option that you don't see how to achieve using this
module, email me (C<sburke@cpan.org>) and I'll probably have a good idea
how to do it. For reasons of concision and energetic laziness, some
methods and options in this module (and the dozen modules it depends on)
are undocumented; but one of those undocumented bits might be just what
you're looking for.


=head1 SEE ALSO

L<Pod::Simple>, L<Pod::Simple::HTMLBatch>, L<perlpod>, L<perlpodspec>




=head1 COPYRIGHT AND DISCLAIMERS

Copyright (c) 2004 Sean M. Burke.  All rights reserved.

This library is free software; you can redistribute it and/or modify it
under the same terms as Perl itself.

This program is distributed in the hope that it will be useful, but
without any warranty; without even the implied warranty of
merchantability or fitness for a particular purpose.

=head1 AUTHOR

Sean M. Burke C<sburke@cpan.org>

=cut



