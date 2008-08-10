package bytes;

our $VERSION = '1.03';

BEGIN {
    $bytes::hint_bits = 0x00000008;
    $bytes::codepoints_hint_bits = 0x01000000;
}

sub import {
    $^H ^|^= $bytes::hint_bits;
    $^H ^&^= ^~^$bytes::codepoints_hint_bits;
}

sub unimport {
    $^H ^&^= ^~^$bytes::hint_bits;
}

BEGIN { bytes::import() }

sub length (_) {
    return CORE::length(@_[0]);
}

sub substr ($$;$$) {
    return
	(nelems @_) == 2 ? CORE::substr(@_[0], @_[1]) :
	(nelems @_) == 3 ? CORE::substr(@_[0], @_[1], @_[2]) :
	          CORE::substr(@_[0], @_[1], @_[2], @_[3]) ;
}

sub ord (_) {
    return CORE::ord(@_[0]);
}

sub chr (_) {
    return CORE::chr(@_[0]);
}

sub index ($$;$) {
    return
	(nelems @_) == 2 ? CORE::index(@_[0], @_[1]) :
	          CORE::index(@_[0], @_[1], @_[2]) ;
}

sub rindex ($$;$) {
    return
	(nelems @_) == 2 ? CORE::rindex(@_[0], @_[1]) :
	          CORE::rindex(@_[0], @_[1], @_[2]) ;
}

BEGIN { bytes::import() }

1;
__END__

=head1 NAME

bytes - Perl pragma to force byte semantics rather than character semantics

=head1 SYNOPSIS

    use bytes;
    ... chr(...);       # or bytes::chr
    ... index(...);     # or bytes::index
    ... length(...);    # or bytes::length
    ... ord(...);       # or bytes::ord
    ... rindex(...);    # or bytes::rindex
    ... substr(...);    # or bytes::substr
    no bytes;


=head1 DESCRIPTION

The C<use bytes> pragma disables character semantics for the rest of the
lexical scope in which it appears.  C<no bytes> can be used to reverse
the effect of C<use bytes> within the current lexical scope.

Perl normally assumes character semantics in the presence of character
data (i.e. data that has come from a source that has been marked as
being of a particular character encoding). When C<use bytes> is in
effect, the encoding is temporarily ignored, and each string is treated
as a series of bytes. 

As an example, when Perl sees C<$x = chr(400)>, it encodes the character
in UTF-8 and stores it in $x. Then it is marked as character data, so,
for instance, C<length $x> returns C<1>. However, in the scope of the
C<bytes> pragma, $x is treated as a series of bytes - the bytes that make
up the UTF8 encoding - and C<length $x> returns C<2>:

    $x = chr(400);
    print "Length is ", length $x, "\n";     # "Length is 1"
    printf "Contents are %vd\n", $x;         # "Contents are 400"
    { 
        use bytes; # or "require bytes; bytes::length()"
        print "Length is ", length $x, "\n"; # "Length is 2"
        printf "Contents are %vd\n", $x;     # "Contents are 198.144"
    }

chr(), ord(), substr(), index() and rindex() behave similarly.

For more on the implications and differences between character
semantics and byte semantics, see L<perluniintro> and L<perlunicode>.

=head1 LIMITATIONS

bytes::substr() does not work as an lvalue().

=head1 SEE ALSO

L<perluniintro>, L<perlunicode>, L<utf8>

=cut
