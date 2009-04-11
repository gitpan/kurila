#!./perl

our @tests = split(m/\n/, <<EOF);
0 3,			0 1 2,		3 4 5 6 7
0 0 a b c,		,		a b c 0 1 2 3 4 5 6 7
8 0 a b c,		,		0 1 2 3 4 5 6 7 a b c
7 0 6.5,		,		0 1 2 3 4 5 6 6.5 7
1 0 a b c d e f g h i j,,		0 a b c d e f g h i j 1 2 3 4 5 6 7
0 1 a,			0,		a 1 2 3 4 5 6 7
1 6 x y z,		1 2 3 4 5 6,	0 x y z 7
0 7 x y z,		0 1 2 3 4 5 6,	x y z 7
1 7 x y z,		1 2 3 4 5 6 7,	0 x y z
4,			4 5 6 7,	0 1 2 3
-4,			4 5 6 7,	0 1 2 3
EOF

print $^STDOUT, "1..", 2 + nelems @tests, "\n";
die "blech" unless (nelems @tests);

our @x = @(1,2,3);
push(@x,< @x);
if (join(':', @x) eq '1:2:3:1:2:3') {print $^STDOUT, "ok 1\n";} else {print $^STDOUT, "not ok 1\n";}
push(@x,4);
if (join(':', @x) eq '1:2:3:1:2:3:4') {print $^STDOUT, "ok 2\n";} else {print $^STDOUT, "not ok 2\n";}

our $test = 3;
foreach my $line ( @tests) {
    my @($list,$get,$leave) =  split(m/,\t*/,$line);
    my @($pos, ?$len, @< @list) =  split(' ',$list);
    my @get = split(' ',$get);
    my @leave = split(' ',$leave);
    @x = @(0,1,2,3,4,5,6,7);
    my @got;
    if (defined $len) {
	@got = @( splice(@x, $pos, $len, < @list) );
    }
    else {
	@got = @( splice(@x, $pos) );
    }
    if (join(':', @got) eq join(':', @get) &&
	join(':', @x) eq join(':', @leave)) {
	print $^STDOUT, "ok ",$test++,"\n";
    }
    else {
	print $^STDOUT, "not ok ",$test++," got: $(join ' ',@got) == $(join ' ',@get) left: $(join ' ',@x) == $(join ' ',@leave)\n";
    }
}

1;  # this file is require'd by lib/tie-stdpush.t
