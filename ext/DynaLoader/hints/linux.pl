# XXX Configure test needed.
# Some Linux releases like to hide their <nlist.h>
our $self;
$self->{CCFLAGS} = %Config{ccflags} . ' -I/usr/include/libelf'
	if -f "/usr/include/libelf/nlist.h";
1;
