#/usr/bin/perl

use lib "$ENV{madpath}/mad";

use strict;
use warnings;

my $filename = shift @ARGV;

use XML::Twig;
use XML::Twig::XPath;

sub fst(@) {
    return $_[0];
}

sub entersub_handler {
    my ($twig, $sub) = @_;

    # Remove indirect object syntax.


    # check is method
    my ($method_named) = $twig->findnodes([$sub], "op_method_named");
    return if not $method_named;

    # skip special subs
    return if $sub->att("flags") =~ m/SPECIAL/;

    # check is indirect object syntax.
    return if (get_madprop($sub, "bigarrow") || '') eq "-&gt;";

    # make indirect object syntax.
    my $madprops = ($twig->findnodes([$sub], qq|madprops|))[0] || $sub->insert_new_elt("madprops");

    $madprops->insert_new_elt( "mad_sv", { key => "bigarrow", val => "-&gt;" } );
    $madprops->insert_new_elt( "mad_sv", { key => "round_open", val => "(" } );
    $madprops->insert_new_elt( "mad_sv", { key => "round_close", val => ")" } );

    # move widespace from method to object and visa versa.
    my ($method_ws) = $twig->findnodes([$method_named],
                                       qq|madprops/mad_op/op_method/op_const/madprops/mad_sv[\@key="wsbefore-value"]|);
    my ($obj_ws) = $twig->findnodes([$sub], qq|op_const/madprops/mad_sv[\@key="wsbefore-value"]|);
    if ($method_ws and $obj_ws) {
        my $x_method_ws = $method_ws->att('val');
        my $x_obj_ws = $obj_ws->att('val');
        $x_obj_ws =~ s/\s+$//;
        $method_ws->set_att("val" => $x_obj_ws);
        $obj_ws->set_att("val" => $x_method_ws);
    }
}

sub const_handler {
    my ($twig, $const) = @_;

    # Convert BARE words

    # real bareword
    return unless $const->att('private') && ($const->att('private') =~ m/BARE/);
    return unless $const->att('flags') =~ "SCALAR";

    return if $const->findnodes(q|madprops/mad_sv[@key="forcedword"][@val="forced"]|);

    # no conversion if 'use strict' is active.
    return if $const->att('private') && ($const->att('private') =~ m/STRICT/);

    # negate: -Level
    # method: $aap->SUPER::noot()
    return if $const->parent->tag =~ m/^op_(negate|method)$/;
    return if $const->parent->tag eq "op_null" 
      and ($const->parent->att("was") || '') =~ m/^(negate|method)$/;
    # open IN, "filename";
    return if $const->parent->tag =~ m/^mad_op$/ and $const->parent->att("key") eq "key";

    {
        # keep qq| foo => |
        my $x = ($const->parent->tag eq "op_null" and ! $const->parent->att('was')) ? $const->parent : $const;
        my ($next) = $x->parent->child($x->pos + 1);
        if (get_madprop($const, "value") =~ m/^\w+$/) {
            return if $next && $twig->findnodes([$next], q|madprops/mad_sv[@key="comma"][@val="=&gt;"]|);
            return if $twig->findnodes([$const->parent], q|madprops/mad_sv[@key="bigarrow"]|); # [@val="-&gt;"]|);
        }
    }

    # "-x XX"
    if ($const->parent->tag =~ m/^op_(ft.*|truncate|chdir|stat|lstat)$/ or
        $const->findnodes(q|madprops/mad_sv[@key="prototyped"][@val="*"]|)
       ) {
        my ($val) = $twig->findnodes([$const], q|madprops/mad_sv[@key="value"]|);
        $val->att("val") eq "_" and return; # not for -x '_'

        # Add '*' to make it a glob
        $const->set_tag("op_rv2gv");
        $val->set_att( "val", "*" . $val->att("val") );
        $val->set_att( "key", "star" );
        my ($wsval) = $twig->findnodes([$const], q|madprops/mad_sv[@key="wsbefore-value"]|);
        $wsval->set_att( "key", "wsbefore-star" ) if $wsval;
        $const->insert_new_elt( "op_const" );
        return;
    }

    # keep Foo::Bar->new()
    return if $const->parent->tag eq "op_entersub";

    # keep qq| $aap{noot} |
    if (($const->parent->tag eq "op_helem" or
         ($const->parent->tag eq "op_null" and ($const->parent->att("was") || '') eq "helem"))
        and get_madprop($const, "value") =~ m/^\w+$/) {
        return;
    }

    # keep qq| -Level |
    return if $const->parent->tag eq "op_negate";
    return if $const->parent->tag eq "op_null" and ($const->parent->att("was") || '') eq "negate";

    # Make it a string constant
    my ($madprops) = $twig->findnodes([$const], q|madprops|);
    $const->del_att('private');
    my ($const_ws) = $twig->findnodes([$const], q|madprops/mad_sv[@key="wsbefore-value"]|);
    my $ws = $const_ws && $const_ws->att('val');
    $const_ws->delete if $const_ws;
    $madprops->insert_new_elt( "mad_sv", { key => 'wsbefore-quote_open', val => $ws } );
    $madprops->insert_new_elt( "mad_sv", { key => 'quote_open', val => q|'| } );
    $madprops->insert_new_elt( 'last_child', "mad_sv", { key => 'quote_close', val => q|'| } );
    set_madprop($const, "assign" => get_madprop($const, "value"));
    del_madprop($const, "value");
}

sub add_encoding_latin1 {
    my $twig = shift;
    my ($root) = $twig->findnodes(q|/op_leave/|);

    # check already existing encoding pragma.
    return if $twig->findnodes(q|//mad_op[@key="use"]/op_const[@PV="encoding.pm"]|);

    my $latin1 = 0;
    for my $item ($twig->findnodes(q|//|)) {
        if (grep { m/&#x..[;]/ } values %{ $item->atts() || {} }) {
            $latin1 = 1;
        }
    }
    return if not $latin1;
    my $madprops = $root->insert_new_elt("op_null")->insert_new_elt("madprops");
    $madprops->insert_new_elt("mad_sv", { key => 'p', val => qq|use encoding 'latin1';&#xA;| });
}

sub del_madprop {
    my ($op, $key) = @_;
    my ($madsv) = $op->findnodes(qq|madprops/mad_sv[\@key="$key"]|);
    $madsv->delete if $madsv;
}

sub set_madprop {
    my ($op, $key, $val) = @_;
    my ($madprops) = $op->findnodes("madprops");
    $madprops ||= $op->insert_new_elt("madprops");
    my ($madsv) = $op->findnodes(qq|madprops/mad_sv[\@key="$key"]|);
    if ($madsv) {
        $madsv->set_att("val", $val);
    } else {
        $madprops->insert_new_elt("mad_sv", { key => $key, val => $val } );
    }
}

sub get_madprop {
    my ($op, $key) = @_;
    my ($madsv) = $op->findnodes(qq|madprops/mad_sv[\@key="$key"]|);
    return $madsv && $madsv->att("val");
}

sub rename_madprop {
    my ($op, $oldkey, $newkey) = @_;
    set_madprop($op, $newkey, get_madprop($op, $oldkey));
    del_madprop($op, $oldkey);
}

sub make_glob_sub {
    my $twig = shift;
    for my $op_glob ($twig->findnodes(q|//op_null[@was="glob"]|)) {
        next if not get_madprop($op_glob, "quote_open");
        set_madprop($op_glob, "round_open", "(");
        set_madprop($op_glob, "round_close", ")");
        set_madprop($op_glob, "operator", "glob");
        del_madprop($op_glob, "assign");
        del_madprop($op_glob, "quote_open");
        del_madprop($op_glob, "quote_close");
        rename_madprop($op_glob, "wsbefore-quote_open", "wsbefore-operator");

        my ($op_c) = $op_glob->findnodes(q|op_entersub/op_null/op_concat|);
        if ($op_c) {
            # TODO quote the op_concat by using op_strigify

        } else {
            $op_c = ($op_glob->findnodes(q|op_entersub/op_null/op_const|))[0];
            set_madprop($op_c, "quote_open", "&#34;");
            set_madprop($op_c, "quote_close", "&#34;");
            set_madprop($op_c, "assign", get_madprop($op_c, "value"));
        }
    }
}

sub is_string_op {
    my $op = shift;

    # string constants, concatenations
    return 1 if $op->tag =~ m/^op_(const|concat)$/;
    # core functions returning a string
    return 1 if $op->tag =~ m/^op_(sprintf|join)$/;
    # stringify
    return 1 if $op->tag eq "op_null" and ($op->att('was') || '') eq "stringify";

    if ($op->tag eq "op_padsv") {
        # lookup last change to variable and if string assignment

        # is variable a string?
        my $targ = $op->att("targ");
        if ($targ) {
            for my $op_x (reverse $op->findnodes("ancestor-or-self::*")) {
                last if $op_x->tag eq "op_leavesub";
                for (reverse $op_x->findnodes("preceding-sibling::*")) {
                    next unless ($_->findnodes("*[\@targ='$targ']"));
                    last if $_->tag eq "op_leavesub";
                    # assignment of string to $var
                    if ($_->tag eq "op_sassign") {
                        my ($src, $dst) = $_->findnodes("*[\@seq]");
                        if ($dst->tag eq "op_padsv" and $dst->att("targ") eq $targ
                            and is_string_op($src)) {
                            return 1;
                        }
                    }
                    # substitute on $var
                    if ($_->tag eq "op_subst") {
                        my $dst = fst $_->findnodes("*[\@seq]");
                        if ($dst->tag eq "op_padsv" and $dst->att("targ") eq $targ) {
                            return 1;
                        }
                    }
                    # unknown fail.
                    return 0;
                }
            }
            return 0;
        }
    }

    return 0;
}

sub remove_rv2gv {
    my $twig = shift;
    # stash
    for my $op_rv2hv (map { $twig->findnodes(qq|//$_|) } (qw|op_rv2hv|)) {
        my ($op_const) = $op_rv2hv->findnodes(q*op_const*);
        next unless $op_const and $op_const->att('PV') =~ m/[:][:]$/;

        my $op_scope = $op_rv2hv->insert_new_elt("op_scope");
        set_madprop($op_scope, curly_open => '{');
        set_madprop($op_scope, curly_close => '}');

        my $op_sub = $op_scope->insert_new_elt("op_entersub");

        # ()
        my $madprops = $op_sub->insert_new_elt("madprops");
        $madprops->insert_new_elt("mad_sv", { key => "round_open", val => "(" });
        $madprops->insert_new_elt("mad_sv", { key => "round_close", val => ")" });

        #args
        my $args = $op_sub->insert_new_elt("op_null", { was => "list" });
        $args->insert_new_elt("op_gv")->insert_new_elt("madprops")
          ->insert_new_elt("mad_sv", { key => "value", val => "Symbol::stash" });
        $op_const->move($args);

        $_->set_att('val', '%') for $op_rv2hv->findnodes(q*madprops/mad_sv[@key='hsh']*);
        set_madprop($op_const, quote_open => '&#34;');
        my $name = $op_const->att('PV');
        $name =~ s/::$//;
        set_madprop($op_const, assign => $name);
        set_madprop($op_const, quote_close => '&#34;');
    }

    # strict refs
    for my $op_rv2gv (map { $twig->findnodes(qq|//$_|) } (qw|op_rv2gv op_rv2sv op_rv2hv op_rv2cv op_rv2av|,
                                                          q{op_null[@was="rv2cv"]}) ) {

        my $op_scope = fst $op_rv2gv->findnodes(q|op_scope|);
        my $op_const;
        if (($op_const) = (map { ($op_scope || $op_rv2gv)->findnodes($_) } qw|op_null[@was='rv2sv'] op_padsv|)) {
            # Special case *$AUTOLOAD

            # is variable a string?
            next unless (get_madprop($op_const, "variable") || '') =~ m/^\$(AUTOLOAD|name)$/
              or is_string_op($op_const);

            next if ($op_rv2gv->att("private") || '') =~ m/STRICT_REFS/;

            if (not $op_scope) {
                $op_scope = $op_rv2gv->insert_new_elt("op_scope");
                set_madprop($op_scope, "curly_open" => "{");
                set_madprop($op_scope, "curly_close" => "}");
                $op_const->move($op_scope);
            }
        } else {
            next if not $op_scope;
            $op_const = ($op_scope->findnodes(q*op_const*))[0] || ($op_scope->findnodes(q*op_concat*))[0]
              || ($op_scope->findnodes(q*op_null[@was="stringify"]*))[0];
            next if not $op_const;
        }

        my $op_sub = $op_scope->insert_new_elt("op_entersub");

        # ()
        set_madprop($op_sub, "round_open", "(");
        set_madprop($op_sub, "round_close", ")");

        #args
        my $args = $op_sub->insert_new_elt("op_null", { was => "list" });
        my $op_gv = $args->insert_new_elt("op_gv");
        $op_gv->set_att("gv", "Symbol::fetch_glob");
        set_madprop( $op_gv, "value", "Symbol::fetch_glob" );
        $op_const->move($args);

    }

    for my $op_rv2gv (map { $twig->findnodes(qq|//$_|) } (qw|op_rv2sv op_rv2hv op_rv2cv op_rv2av op_null[@was="rv2cv"]|)) {
        next unless ($op_rv2gv->findnodes(q|op_scope/op_entersub/op_null/op_null/op_gv[@gv="Symbol::fetch_glob"]|)
                     or $op_rv2gv->findnodes(q|op_scope/op_entersub/op_null/op_gv[@gv="Symbol::fetch_glob"]|));

        my ($op_scope) = $op_rv2gv->findnodes(q|op_scope|);
        my ($op_sub) = $op_scope->findnodes(q|op_entersub|);

        my $new_gv = $op_scope->insert_new_elt("op_rv2gv");
        set_madprop($new_gv, "star", '*');
        my $new_scope = $new_gv->insert_new_elt("op_scope");
        set_madprop($new_scope, "curly_open", "{");
        set_madprop($new_scope, "curly_close", "}");
        $op_sub->move($new_scope);
    }
}

sub remove_vstring {
    my $twig = shift;

    for my $op_const ($twig->findnodes(q|//op_const|), $twig->findnodes(q|op_null[@was="const"]|)) {
        next unless (get_madprop($op_const, "value") || '') =~ m/\Av/;
        next if get_madprop($op_const, "forcedword");
        next if $op_const->att('private') && ($op_const->att('private') =~ m/BARE/);
        next if get_madprop($op_const->parent, "quote_open");
        next if $op_const->parent->tag eq "mad_op";
        next if $op_const->parent->tag eq "op_require";
        next if $op_const->parent->tag eq "op_concat";

        set_madprop($op_const, "wsbefore-quote_open", get_madprop($op_const, "wsbefore-value"));
        set_madprop($op_const, "quote_open", "&#34;");
        set_madprop($op_const, "quote_close", "&#34;");
        my $v = get_madprop($op_const, "value");
        $v =~ s/^v//;
        $v =~ m/^[\d.]+$/ or die "Invalid string '$v'";
        $v =~ s/(\d+)/ sprintf '\x{%x}', $1 /ge;
        $v =~ s/[.]//g;
        set_madprop($op_const, "assign", $v);
        del_madprop($op_const, "value");
    }
}

sub remove_typed_declaration {
    my $twig = shift;
    for my $op_pad (map { $twig->findnodes(qq|//$_|) } (qw|op_padsv op_list|)) {
        if ((get_madprop($op_pad, "defintion") || '') =~ m/^(my|our).+$/) {
            set_madprop($op_pad, "defintion", $1);
        }
    }
}

# parsing
my $twig= XML::Twig->new( keep_spaces => 1, keep_encoding => 1 );

$twig->parsefile( "-" );

# replacing.
for my $op ($twig->findnodes(q|//op_entersub|)) {
    entersub_handler($twig, $op);
}

for my $op_const ($twig->findnodes(q|//op_const|)) {
    const_handler($twig, $op_const);
}

make_glob_sub( $twig );
remove_vstring( $twig );

# add_encoding_latin1($twig);

remove_rv2gv($twig);
remove_typed_declaration($twig);

# print
$twig->print;
