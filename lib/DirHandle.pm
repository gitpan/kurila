package DirHandle;

our $VERSION = '1.01';
=head1 NAME 

DirHandle - supply object methods for directory handles

=head1 SYNOPSIS

    use DirHandle;
    $d = new DirHandle ".";
    if (defined $d) {
        while (defined($_ = $d->read)) { something($_); }
        $d->rewind;
        while (defined($_ = $d->read)) { something_else($_); }
        undef $d;
    }

=head1 DESCRIPTION

The C<DirHandle> method provide an alternative interface to the
opendir(), closedir(), readdir(), and rewinddir() functions.

The only objective benefit to using C<DirHandle> is that it avoids
namespace pollution by creating globs to hold directory handles.

=head1 NOTES

=over 4

=item *

On Mac OS (Classic), the path separator is ':', not '/', and the 
current directory is denoted as ':', not '.'. You should be careful 
about specifying relative pathnames. While a full path always begins 
with a volume name, a relative pathname should always begin with a 
':'.  If specifying a volume name only, a trailing ':' is required.

=back

=cut

use Symbol;

sub new {
    (nelems @_) +>= 1 && (nelems @_) +<= 2 or die 'usage: new DirHandle [DIRNAME]';
    my $class = shift;
    my $dh = gensym;
    if ((nelems @_)) {
	DirHandle::open($dh, @_[0])
	    or return undef;
    }
    bless $dh, $class;
}

sub DESTROY {
    # Don't warn about already being closed as it may have been closed 
    # correctly, or maybe never opened at all.
    no warnings 'io';
    closedir(@_[0]);
}

sub open {
    (nelems @_) == 2 or die 'usage: $dh->open(DIRNAME)';
    my @($dh, $dirname) =  @_;
    opendir($dh, $dirname);
}

sub close {
    (nelems @_) == 1 or die 'usage: $dh->close()';
    my @($dh) =  @_;
    closedir($dh);
}

sub readdir {
    (nelems @_) == 1 or die 'usage: $dh->read()';
    my @($dh) =  @_;
    return $( CORE::readdir($dh) );
}

sub readdirs {
    (nelems @_) == 1 or die 'usage: $dh->read()';
    my @($dh) =  @_;
    return @( CORE::readdir($dh) ); # Force list context.
}

sub rewind {
    (nelems @_) == 1 or die 'usage: $dh->rewind()';
    my @($dh) =  @_;
    rewinddir($dh);
}

1;
