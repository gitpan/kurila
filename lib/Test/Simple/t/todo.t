#!perl -w

BEGIN {
    if( %ENV{PERL_CORE} ) {
        chdir 't';
        @INC = '../lib';
    }
}

use Test::More;

plan tests => 19;


$Why = 'Just testing the todo interface.';

my $is_todo;
TODO: {
    local $TODO = $Why;

    fail("Expected failure");
    fail("Another expected failure");

    $is_todo = Test::More->builder->todo;
}

pass("This is not todo");
ok( $is_todo, 'TB->todo' );


TODO: {
    local $TODO = $Why;

    fail("Yet another failure");
}

pass("This is still not todo");


TODO: {
    local $TODO = "testing that error messages don't leak out of todo";

    ok( 'this' eq 'that',   'ok' );

    like( 'this', '/that/', 'like' );
    is(   'this', 'that',   'is' );
    isnt( 'this', 'this',   'isnt' );

    can_ok('Fooble', 'yarble');
    isa_ok('Fooble', 'yarble');
    use_ok('Fooble');
    require_ok('Fooble');
}


TODO: {
    todo_skip "Just testing todo_skip", 2;

    fail("Just testing todo");
    die "todo_skip should prevent this";
    pass("Again");
}


{
    my $warning;
    local $^WARN_HOOK = sub { $warning = @_[0]->message };
    TODO: {
        # perl gets the line number a little wrong on the first
        # statement inside a block.
        1 == 1;
#line 73
        todo_skip "Just testing todo_skip";
        fail("So very failed");
    }
    like( $warning, qr/^\Qtodo_skip() needs to know \E\$how_many tests are in the block/ms,
        'todo_skip without $how_many warning' );
}


TODO: {
    Test::More->builder->exported_to("Wibble");
    
    local $TODO = "testing \$TODO with an incorrect exported_to()";
    
    fail("Just testing todo");
}
