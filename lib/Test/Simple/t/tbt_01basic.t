#!/usr/bin/perl

use Test::Builder::Tester tests => 9;
use Test::More;

ok(1,"This is a basic test");

test_out("ok 1 - tested");
ok(1,"tested");
test_test("captured okay on basic");

test_out("ok 1 - tested");
ok(1,"tested");
test_test("captured okay again without changing number");

ok(1,"test unrelated to Test::Builder::Tester");

test_out("ok 1 - one");
test_out("ok 2 - two");
ok(1,"one");
ok(2,"two");
test_test("multiple tests");

test_out("not ok 1 - should fail");
test_err("#     Failed test ($^PROGRAM_NAME at line 28)");
test_err("#          got: 'foo'");
test_err("#     expected: 'bar'");
is("foo","bar","should fail");
test_test("testing failing");


test_out("not ok 1");
test_out("not ok 2");
test_fail(2);
test_fail(1);
fail();  fail();
test_test("testing failing on the same line with no name");


test_out("not ok 1 - name");
test_out("not ok 2 - name");
test_fail(2);
test_fail(1);
fail("name");  fail("name");
test_test("testing failing on the same line with the same name");


test_out("not ok 1 - name # TODO Something");
test_err("#     Failed (TODO) test ($^PROGRAM_NAME at line 52)");
TODO: do { 
    local $TODO = "Something";
    fail("name");
};
test_test("testing failing with todo");

