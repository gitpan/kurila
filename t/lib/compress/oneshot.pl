use lib 't';
use warnings;
use bytes;

use Test::More ;
use CompTestUtils;

BEGIN {
    # use Test::NoWarnings, if available
    my $extra = 0 ;
    $extra = 1
        if try { require Test::NoWarnings ;  'Test::NoWarnings'->import(); 1 };

    plan tests => 970 + $extra ;

    use_ok('IO::Uncompress::AnyUncompress', qw(anyuncompress $AnyUncompressError)) ;

}

sub run
{

    my $CompressClass   = identify();
    my $UncompressClass = getInverse($CompressClass);
    my $Error           = getErrorRef($CompressClass);
    my $UnError         = getErrorRef($UncompressClass);
    my $TopFuncName     = getTopFuncName($CompressClass);


    my @MultiValues     = getMultiValues($CompressClass);

    foreach my $bit ($CompressClass, $UncompressClass,
                     'IO::Uncompress::AnyUncompress',
                    )
    {
        my $Error = getErrorRef($bit);
        my $Func = getTopFuncRef($bit);
        my $TopType = getTopFuncName($bit);

        title "Testing $TopType Error Cases";

        my $a;
        my $x ;

        try { $a = $Func->(\$a => \$x, Fred => 1) ;} ;
        like $@->{description}, qr/^$TopType: unknown key value\(s\) Fred/, '  Illegal Parameters';

        try { $a = $Func->() ;} ;
        like $@->{description}, qr/^$TopType: expected at least 1 parameters/, '  No Parameters';

        try { $a = $Func->(\$x, \1) ;} ;
        like $$Error, "/^$TopType: output buffer is read-only/", '  Output is read-only' ;

        my $in ;
        try { $a = $Func->($in, \$x) ;} ;
        like $@->{description}, mkErr("^$TopType: input filename is undef or null string"), 
            '  Input filename undef' ;

        $in = '';    
        try { $a = $Func->($in, \$x) ;} ;
        like $@->{description}, mkErr("^$TopType: input filename is undef or null string"), 
            '  Input filename empty' ;

        {
            my $lex1 = LexFile->new( my $in) ;
            writeFile($in, "abc");
            my $out = $in ;
            try { $a = $Func->($in, $out) ;} ;
            like $@->{description}, mkErr("^$TopType: input and output filename are identical"),
                '  Input and Output filename are the same';
        }

        {
            my $dir = "tmpdir";
            my $lex = LexDir->new( $dir) ;
            mkdir $dir, 0777 ;

            $a = $Func->($dir, \$x) ;
            is $a, undef, "  $TopType returned undef";
            like $$Error, "/input file '$dir' is a directory/",
                '  Input filename is a directory';

            $a = $Func->(\$x, $dir) ;
            is $a, undef, "  $TopType returned undef";
            like $$Error, "/output file '$dir' is a directory/",
                '  Output filename is a directory';
        }

        try { $a = $Func->(\$in, \$in) ;} ;
        like $@->{description}, mkErr("^$TopType: input and output buffer are identical"),
            '  Input and Output buffer are the same';
            
        SKIP:
        {
            use Config;

            my $lex = LexFile->new( my $out_file) ;
            open OUT, ">", "$out_file" ;
            try { $a = $Func->(\*OUT, \*OUT) ;} ;
            like $@->{description}, mkErr("^$TopType: input and output handle are identical"),
                '  Input and Output handle are the same';
                
            close OUT;
            is -s $out_file, 0, "  File zero length" ;
        }

        {
            my %x = %( () ) ;
            my $object = bless \%x, "someClass" ;

            # Buffer not a scalar reference
            #try { $a = $Func->(\$x, \%x) ;} ;
            try { $a = $Func->(\$x, $object) ;} ;
            like $@->{description}, mkErr("^$TopType: illegal output parameter"),
                '  Bad Output Param';
                
            # Buffer not a scalar reference
            try { $a = $Func->(\$x, \%x) ;} ;
            like $@->{description}, mkErr("^$TopType: illegal output parameter"),
                '  Bad Output Param';
                

            try { $a = $Func->(\%x, \$x) ;} ;
            like $@->{description}, mkErr("^$TopType: illegal input parameter"),
                '  Bad Input Param';

            #try { $a = $Func->(\%x, \$x) ;} ;
            try { $a = $Func->($object, \$x) ;} ;
            like $@->{description}, mkErr("^$TopType: illegal input parameter"),
                '  Bad Input Param';
        }

        my $filename = 'abc.def';
        ok ! -e $filename, "  input file '$filename' does not exist";
        $a = $Func->($filename, \$x) ;
        is $a, undef, "  $TopType returned undef";
        like $$Error, "/^input file '$filename' does not exist\$/", "  input File '$filename' does not exist";
            
        $filename = '/tmp/abd/abc.def';
        ok ! -e $filename, "  output File '$filename' does not exist";
        $a = $Func->(\$x, $filename) ;
        is $a, undef, "  $TopType returned undef";
        like $$Error, ("/^(cannot open file '$filename'|input file '$filename' does not exist):/"), "  output File '$filename' does not exist";
            
        try { $a = $Func->(\$x, '<abc>') } ;
        like $$Error, "/Need input fileglob for outout fileglob/",
                '  Output fileglob with no input fileglob';
        is $a, undef, "  $TopType returned undef";

        $a = $Func->('<abc)>', '<abc>') ;
        is $a, undef, "  $TopType returned undef";
        like $$Error, "/Unmatched \\) in input fileglob/",
                "  Unmatched ) in input fileglob";
    }

    foreach my $bit ($UncompressClass,
                     'IO::Uncompress::AnyUncompress',
                    )
    {
        my $Error = getErrorRef($bit);
        my $Func = getTopFuncRef($bit);
        my $TopType = getTopFuncName($bit);

        {
            my $in ;
            my $out ;
            my @x ;

            SKIP:
            {
                use Config;

                skip 'readonly + threads', 1
                    if %Config{useithreads};

                
                try { $a = $Func->(\$in, \$out, TrailingData => \"abc") ;} ;
                like $@->{description}, mkErr("^$TopType: Parameter 'TrailingData' not writable"),
                    '  TrailingData output not writable';
            }

            try { $a = $Func->(\$in, \$out, TrailingData => \@x) ;} ;
            like $@->{description}, mkErr("^$TopType: Parameter 'TrailingData' not a scalar reference"),
                '  TrailingData output not scaral reference';
        }
    }

    foreach my $bit ($UncompressClass,
                     'IO::Uncompress::AnyUncompress',
                    )
    {
        my $Error = getErrorRef($bit);
        my $Func = getTopFuncRef($bit);
        my $TopType = getTopFuncName($bit);

        my $data = "mary had a little lamb" ;
        my $keep = $data ;

        for my $trans ( 0, 1)
        {
            title "Non-compressed data with $TopType, Transparent => $trans ";
            my $a;
            my $x ;
            my $out = '' ;

            $a = $Func->(\$data, \$out, Transparent => $trans) ;

            is $data, $keep, "  Input buffer not changed" ;

            if ($trans)
            {
                ok $a, "  $TopType returned true" ;
                is $out, $data, "  got expected output" ;
                ok ! $$Error, "  no error [$$Error]" ;
            }
            else
            {
                ok ! $a, "  $TopType returned false" ;
                #like $$Error, '/xxx/', "  error" ;
                ok $$Error, "  error is '$$Error'" ;
            }
        }
    }

    foreach my $bit ($CompressClass
                    )
    {
        my $Error = getErrorRef($bit);
        my $Func = getTopFuncRef($bit);
        my $TopType = getTopFuncName($bit);
        my $TopTypeInverse = getInverse($bit);
        my $FuncInverse = getTopFuncRef($TopTypeInverse);
        my $ErrorInverse = getErrorRef($TopTypeInverse);

        title "$TopTypeInverse - corrupt data";

        my $data = "abcd" x 100 ;
        my $out;

        ok $Func->(\$data, \$out), "  $TopType ok";

        # corrupt the compressed data
        #substr($out, -10, 10) = "x" x 10 ;
        substr($out, int(length($out)/3), 10, 'abcdeabcde');

        my $result;
        ok ! $FuncInverse->(\$out => \$result, Transparent => 0), "  $TopTypeInverse ok";
        ok $$ErrorInverse, "  Got error '$$ErrorInverse'" ;

        #is $result, $data, "  data ok";

        ok ! anyuncompress(\$out => \$result, Transparent => 0), "anyuncompress ok";
        ok $AnyUncompressError, "  Got error '$AnyUncompressError'" ;
    }


    foreach my $bit ($CompressClass
                    )
    {
        my $Error = getErrorRef($bit);
        my $Func = getTopFuncRef($bit);
        my $TopType = getTopFuncName($bit);
        my $TopTypeInverse = getInverse($bit);
        my $FuncInverse = getTopFuncRef($TopTypeInverse);

        my @opts = @( () );
        @opts = @(RawInflate => 1)
            if $CompressClass eq 'IO::Compress::RawInflate';

        for my $append ( 1, 0 )
        {
            my $already = '';
            $already = 'abcde' if $append ;

            for my $buffer ( undef, '', "abcde" )
            {

                my $disp_content = defined $buffer ? $buffer : '<undef>' ;

                my $keep = $buffer;
                my $out_file = "abcde.out";
                my $in_file = "abcde.in";

                {
                    title "$TopType - From Buff to Buff content '$disp_content' Append $append" ;

                    my $output = $already;
                    ok &$Func(\$buffer, \$output, Append => $append), '  Compressed ok' ;

                    is $keep, $buffer, "  Input buffer not changed" ;
                    my $got = anyUncompress(\$output, $already);
                    $got = undef if ! defined $buffer && $got eq '' ;
                    is $got, $buffer, "  Uncompressed matches original";

                }

                {
                    title "$TopType - From Buff to Array Ref content '$disp_content' Append $append" ;

                    my @output = @('first') ;
                    ok &$Func(\$buffer, \@output, Append => $append), '  Compressed ok' ;

                    is @output[0], 'first', "  Array[0] unchanged";
                    is $keep, $buffer, "  Input buffer not changed" ;
                    my $got = anyUncompress(@output[1]);
                    $got = undef if ! defined $buffer && $got eq '' ;
                    is $got, $buffer, "  Uncompressed matches original";
                }

                {
                    title "$TopType - From Array Ref to Array Ref content '$disp_content' Append $append" ;

                    my $lex = LexFile->new( my $in_file) ;
                    writeFile($in_file, $buffer);
                    my @output = @('first') ;
                    my @input = @($in_file);
                    ok &$Func(\@input, \@output, Append => $append), '  Compressed ok' ;

                    is @output[0], 'first', "  Array[0] unchanged";
                    my $got = anyUncompress(@output[1]);
                    $got = undef if ! defined $buffer && $got eq '' ;
                    is $got, $buffer, "  Uncompressed matches original";
                }

                {
                    title "$TopType - From Buff to Filename content '$disp_content' Append $append" ;

                    my $lex = LexFile->new( my $out_file) ;
                    ok ! -e $out_file, "  Output file does not exist";
                    writeFile($out_file, $already);

                    ok &$Func(\$buffer, $out_file, Append => $append), '  Compressed ok' ;

                    ok -e $out_file, "  Created output file";
                    my $got = anyUncompress($out_file, $already);
                    $got = undef if ! defined $buffer && $got eq '' ;
                    is $got, $buffer, "  Uncompressed matches original";
                }

                {
                    title "$TopType - From Buff to Handle content '$disp_content' Append $append" ;

                    my $lex = LexFile->new( my $out_file) ;

                    ok ! -e $out_file, "  Output file does not exist";
                    writeFile($out_file, $already);
                    my $of = 'IO::File'->new( "$out_file", ">>") ;
                    ok $of, "  Created output filehandle" ;

                    ok &$Func(\$buffer, $of, AutoClose => 1, Append => $append), '  Compressed ok' ;

                    ok -e $out_file, "  Created output file";
                    my $got = anyUncompress($out_file, $already);
                    $got = undef if ! defined $buffer && $got eq '' ;
                    is $got, $buffer, "  Uncompressed matches original";
                }


                {
                    title "$TopType - From Filename to Filename content '$disp_content' Append $append" ;

                    my $lex = LexFile->new(my $in_file, my $out_file) ;
                    writeFile($in_file, $buffer);

                    ok ! -e $out_file, "  Output file does not exist";
                    writeFile($out_file, $already);

                    ok &$Func($in_file => $out_file, Append => $append), '  Compressed ok' ;

                    ok -e $out_file, "  Created output file";
                    my $got = anyUncompress($out_file, $already);
                    $got = undef if ! defined $buffer && $got eq '' ;
                    is $got, $buffer, "  Uncompressed matches original";

                }

                {
                    title "$TopType - From Filename to Handle content '$disp_content' Append $append" ;

                    my $lex = LexFile->new(my $in_file, my $out_file) ;
                    writeFile($in_file, $buffer);

                    ok ! -e $out_file, "  Output file does not exist";
                    writeFile($out_file, $already);
                    my $out = 'IO::File'->new( "$out_file", ">>") ;

                    ok &$Func($in_file, $out, AutoClose => 1, Append => $append), '  Compressed ok' ;

                    ok -e $out_file, "  Created output file";
                    my $got = anyUncompress($out_file, $already);
                    $got = undef if ! defined $buffer && $got eq '' ;
                    is $got, $buffer, "  Uncompressed matches original";

                }

                {
                    title "$TopType - From Filename to Buffer content '$disp_content' Append $append" ;

                    my $lex = LexFile->new(my $in_file, my $out_file) ;
                    writeFile($in_file, $buffer);

                    my $out = $already;

                    ok &$Func($in_file => \$out, Append => $append), '  Compressed ok' ;

                    my $got = anyUncompress(\$out, $already);
                    $got = undef if ! defined $buffer && $got eq '' ;
                    is $got, $buffer, "  Uncompressed matches original";

                }
                
                {
                    title "$TopType - From Handle to Filename content '$disp_content' Append $append" ;

                    my $lex = LexFile->new(my $in_file, my $out_file) ;
                    writeFile($in_file, $buffer);
                    my $in = 'IO::File'->new( "$in_file", "<") ;

                    ok ! -e $out_file, "  Output file does not exist";
                    writeFile($out_file, $already);

                    ok &$Func($in, $out_file, Append => $append), '  Compressed ok' 
                        or diag "error is $$Error" ;

                    ok -e $out_file, "  Created output file";
                    my $got = anyUncompress($out_file, $already);
                    $got = undef if ! defined $buffer && $got eq '' ;
                    is $buffer, $got, "  Uncompressed matches original";

                }

                {
                    title "$TopType - From Handle to Handle content '$disp_content' Append $append" ;

                    my $lex = LexFile->new(my $in_file, my $out_file) ;
                    writeFile($in_file, $buffer);
                    my $in = 'IO::File'->new( "$in_file", "<") ;

                    ok ! -e $out_file, "  Output file does not exist";
                    writeFile($out_file, $already);
                    my $out = 'IO::File'->new( "$out_file", ">>") ;

                    ok &$Func($in, $out, AutoClose => 1, Append => $append), '  Compressed ok' ;

                    ok -e $out_file, "  Created output file";
                    my $got = anyUncompress($out_file, $already);
                    $got = undef if ! defined $buffer && $got eq '' ;
                    is $buffer, $got, "  Uncompressed matches original";

                }

                {
                    title "$TopType - From Handle to Buffer content '$disp_content' Append $append" ;

                    my $lex = LexFile->new(my $in_file, my $out_file) ;
                    writeFile($in_file, $buffer);
                    my $in = 'IO::File'->new( "$in_file", "<") ;

                    my $out = $already ;

                    ok &$Func($in, \$out, Append => $append), '  Compressed ok' ;

                    my $got = anyUncompress(\$out, $already);
                    $got = undef if ! defined $buffer && $got eq '' ;
                    is $buffer, $got, "  Uncompressed matches original";

                }

                {
                    title "$TopType - From stdin (via '-') to Buffer content '$disp_content' Append $append" ;

                    my $lex = LexFile->new(my $in_file, my $out_file) ;
                    writeFile($in_file, $buffer);

                       open(SAVEIN, "<&", $^STDIN);
                    my $dummy = fileno SAVEIN ;
                    ok open(STDIN, "<", "$in_file"), "  redirect STDIN";

                    my $out = $already;

                    ok &$Func('-', \$out, Append => $append), '  Compressed ok' 
                        or diag $$Error ;

                       open(STDIN, "<&", \*SAVEIN);

                    my $got = anyUncompress(\$out, $already);
                    $got = undef if ! defined $buffer && $got eq '' ;
                    is $buffer, $got, "  Uncompressed matches original";

                }

            }
        }
    }

    foreach my $bit ($CompressClass)
    {
        my $Error = getErrorRef($bit);
        my $Func = getTopFuncRef($bit);
        my $TopType = getTopFuncName($bit);

        my $TopTypeInverse = getInverse($bit);
        my $FuncInverse = getTopFuncRef($TopTypeInverse);

        my $lex = LexFile->new(my $file1, my $file2) ;

        writeFile($file1, "data1");
        writeFile($file2, "data2");
        my $of = 'IO::File'->new( "$file1", "<") ;
        ok $of, "  Created output filehandle" ;

        #my @input = (   undef, "", $file2, \undef, \'', \"abcde", $of) ;
        #my @expected = ("", "", $file2, "", "", "abcde", "data1");
        #my @uexpected = ("", "", "data2", "", "", "abcde", "data1");
        #my @input = (   $file2, \"abcde", $of) ;
        #my @expected = ( $file2, "abcde", "data1");
        #my @uexpected = ("data2", "abcde", "data1");

        my @input = @(   $file1, $file2) ;
        #my @expected = ( $file1, $file2);
        my @expected = @("data1", "data2");
        my @uexpected = @("data1", "data2");

        my @keep =  @input;

        {
            title "$TopType - From Array Ref to Array Ref" ;

            my @output = @('first') ;
            ok &$Func(\@input, \@output, AutoClose => 0), '  Compressed ok' ;

            is @output[0], 'first', "  Array[0] unchanged";

            is_deeply \@input, \@keep, "  Input array not changed" ;
            my @got = @( shift @output );
            foreach (< @output) { push @got, < anyUncompress($_) }

            is_deeply \@got, \@('first', < @expected), "  Got Expected uncompressed data";

        }

        foreach my $ms (< @MultiValues)
        {
            {
                title "$TopType - From Array Ref to Buffer, MultiStream $ms" ;

                # rewind the filehandle
                $of->open("$file1", "<") ;

                my $output  ;
                ok &$Func(\@input, \$output, MultiStream => $ms, AutoClose => 0), '  Compressed ok' 
                    or diag $$Error;

                my $got = anyUncompress(\@( \$output, MultiStream => $ms ));

                is $got, join('', < @uexpected), "  Got Expected uncompressed data";
                my @headers = getHeaders(\$output);
                is (nelems @headers), $ms ? (nelems @input) : 1, "  Header count ok";
            }

            {
                title "$TopType - From Array Ref to Filename, MultiStream $ms" ;

                my $lex = LexFile->new( my $file3) ;

                # rewind the filehandle
                $of->open("$file1", "<") ;

                my $output  ;
                ok &$Func(\@input, $file3, MultiStream => $ms, AutoClose => 0), '  Compressed ok' ;

                my $got = anyUncompress(\@( $file3, MultiStream => $ms ));

                is $got, join('', < @uexpected), "  Got Expected uncompressed data";
                my @headers = getHeaders($file3);
                is (nelems @headers), $ms ? (nelems @input) : 1, "  Header count ok";
            }

            {
                title "$TopType - From Array Ref to Filehandle, MultiStream $ms" ;

                my $lex = LexFile->new(my $file3) ;

                my $fh3 = 'IO::File'->new( "$file3", ">");

                # rewind the filehandle
                $of->open("$file1", "<") ;

                my $output  ;
                ok &$Func(\@input, $fh3, MultiStream => $ms, AutoClose => 0), '  Compressed ok' ;

                $fh3->close();

                my $got = anyUncompress(\@( $file3, MultiStream => $ms ));

                is $got, join('', < @uexpected), "  Got Expected uncompressed data";
                my @headers = getHeaders($file3);
                is (nelems @headers), $ms ? (nelems @input) : 1, "  Header count ok";
            }
        }
    }

    foreach my $bit ($UncompressClass,
                    #'IO::Uncompress::AnyUncompress',
                    )
    {
        my $Error = getErrorRef($bit);
        my $Func = getTopFuncRef($bit);
        my $TopType = getTopFuncName($bit);
        my $CompressClass = getInverse($bit);
        my $C_Func = getTopFuncRef($CompressClass);


        
        my $data = "mary had a little lamb" ;
        my $keep = $data ;
        my $extra = "after the main event";

        foreach my $fb ( qw( filehandle buffer ) )
        {
            title "Trailingdata with $TopType, from $fb";

            my $lex = LexFile->new( my $name) ;
            my $input ;

            my $compressed ;
            ok &$C_Func(\$data, \$compressed), '  Compressed ok' ;
            $compressed .= $extra;

            if ($fb eq 'buffer')
            {
                $input = \$compressed;
            }
            else
            {
                writeFile($name, $compressed);

                $input = 'IO::File'->new( "$name", "<") ;
            }

            my $trailing;
            my $out;
            ok $Func->($input, \$out, TrailingData => $trailing), "  Uncompressed OK" ;
            is $out, $keep, "  Got uncompressed data";

            my $rest = '';
            if ($fb eq 'filehandle')
            {
                read($input, $rest, 10000) ;
            }

            is $trailing . $rest, $extra, "  Got trailing data";
            
        }
    }


#    foreach my $bit ($CompressClass)
#    {
#        my $Error = getErrorRef($bit);
#        my $Func = getTopFuncRef($bit);
#        my $TopType = getTopFuncName($bit);
#
#        my $TopTypeInverse = getInverse($bit);
#        my $FuncInverse = getTopFuncRef($TopTypeInverse);
#
#        my @inFiles  = map { "in$_.tmp"  } 1..4;
#        my @outFiles = map { "out$_.tmp" } 1..4;
#        my $lex = new LexFile(@inFiles, @outFiles);
#
#        writeFile($_, "data $_") foreach @inFiles ;
#        
#        {
#            title "$TopType - Hash Ref: to filename" ;
#
#            my $output ;
#            ok &$Func( { $inFiles[0] => $outFiles[0],
#                         $inFiles[1] => $outFiles[1],
#                         $inFiles[2] => $outFiles[2] } ), '  Compressed ok' ;
#
#            foreach (0 .. 2)
#            {
#                my $got = anyUncompress($outFiles[$_]);
#                is $got, "data $inFiles[$_]", "  Uncompressed $_ matches original";
#            }
#        }
#
#        {
#            title "$TopType - Hash Ref: to buffer" ;
#
#            my @buffer ;
#            ok &$Func( { $inFiles[0] => \$buffer[0],
#                         $inFiles[1] => \$buffer[1],
#                         $inFiles[2] => \$buffer[2] } ), '  Compressed ok' ;
#
#            foreach (0 .. 2)
#            {
#                my $got = anyUncompress(\$buffer[$_]);
#                is $got, "data $inFiles[$_]", "  Uncompressed $_ matches original";
#            }
#        }
#
#        {
#            title "$TopType - Hash Ref: to undef" ;
#
#            my @buffer ;
#            my %hash = ( $inFiles[0] => undef,
#                         $inFiles[1] => undef,
#                         $inFiles[2] => undef, 
#                     );  
#
#            ok &$Func( \%hash ), '  Compressed ok' ;
#
#            foreach (keys %hash)
#            {
#                my $got = anyUncompress(\$hash{$_});
#                is $got, "data $_", "  Uncompressed $_ matches original";
#            }
#        }
#
#        {
#            title "$TopType - Filename to Hash Ref" ;
#
#            my %output ;
#            ok &$Func( $inFiles[0] => \%output), '  Compressed ok' ;
#
#            is keys %output, 1, "  one pair in hash" ;
#            my ($k, $v) = each %output;
#            is $k, $inFiles[0], "  key is '$inFiles[0]'";
#            my $got = anyUncompress($v);
#            is $got, "data $inFiles[0]", "  Uncompressed matches original";
#        }
#
#        {
#            title "$TopType - File Glob to Hash Ref" ;
#
#            my %output ;
#            ok &$Func( '<in*.tmp>' => \%output), '  Compressed ok' ;
#
#            is keys %output, 4, "  four pairs in hash" ;
#            foreach my $fil (@inFiles)
#            {
#                ok exists $output{$fil}, "  key '$fil' exists" ;
#                my $got = anyUncompress($output{$fil});
#                is $got, "data $fil", "  Uncompressed matches original";
#            }
#        }
#
#
#    }

#    foreach my $bit ($CompressClass)
#    {
#        my $Error = getErrorRef($bit);
#        my $Func = getTopFuncRef($bit);
#        my $TopType = getTopFuncName($bit);
#
#        my $TopTypeInverse = getInverse($bit);
#        my $FuncInverse = getTopFuncRef($TopTypeInverse);
#
#        my @inFiles  = map { "in$_.tmp"  } 1..4;
#        my @outFiles = map { "out$_.tmp" } 1..4;
#        my $lex = new LexFile(@inFiles, @outFiles);
#
#        writeFile($_, "data $_") foreach @inFiles ;
#        
#
#
#    #    if (0)
#    #    {
#    #        title "$TopType - Hash Ref to Array Ref" ;
#    #
#    #        my @output = ('first') ;
#    #        ok &$Func( { \@input, \@output } , AutoClose => 0), '  Compressed ok' ;
#    #
#    #        is $output[0], 'first', "  Array[0] unchanged";
#    #
#    #        is_deeply \@input, \@keep, "  Input array not changed" ;
#    #        my @got = shift @output;
#    #        foreach (@output) { push @got, anyUncompress($_) }
#    #
#    #        is_deeply \@got, ['first', @expected], "  Got Expected uncompressed data";
#    #
#    #    }
#    #
#    #    if (0)
#    #    {
#    #        title "$TopType - From Array Ref to Buffer" ;
#    #
#    #        # rewind the filehandle
#    #        $of->open("<$file1") ;
#    #
#    #        my $output  ;
#    #        ok &$Func(\@input, \$output, AutoClose => 0), '  Compressed ok' ;
#    #
#    #        my $got = anyUncompress(\$output);
#    #
#    #        is $got, join('', @expected), "  Got Expected uncompressed data";
#    #    }
#    #
#    #    if (0)
#    #    {
#    #        title "$TopType - From Array Ref to Filename" ;
#    #
#    #        my ($file3) = ("file3");
#    #        my $lex = new LexFile($file3) ;
#    #
#    #        # rewind the filehandle
#    #        $of->open("<$file1") ;
#    #
#    #        my $output  ;
#    #        ok &$Func(\@input, $file3, AutoClose => 0), '  Compressed ok' ;
#    #
#    #        my $got = anyUncompress($file3);
#    #
#    #        is $got, join('', @expected), "  Got Expected uncompressed data";
#    #    }
#    #
#    #    if (0)
#    #    {
#    #        title "$TopType - From Array Ref to Filehandle" ;
#    #
#    #        my ($file3) = ("file3");
#    #        my $lex = new LexFile($file3) ;
#    #
#    #        my $fh3 = new IO::File ">$file3";
#    #
#    #        # rewind the filehandle
#    #        $of->open("<$file1") ;
#    #
#    #        my $output  ;
#    #        ok &$Func(\@input, $fh3, AutoClose => 0), '  Compressed ok' ;
#    #
#    #        $fh3->close();
#    #
#    #        my $got = anyUncompress($file3);
#    #
#    #        is $got, join('', @expected), "  Got Expected uncompressed data";
#    #    }
#    }

    foreach my $bit ($CompressClass
                    )
    {
        my $Error = getErrorRef($bit);
        my $Func = getTopFuncRef($bit);
        my $TopType = getTopFuncName($bit);

        for my $files ( \@(qw(a1)), \@(qw(a1 a2 a3)) )
        {

            my $tmpDir1 = 'tmpdir1';
            my $tmpDir2 = 'tmpdir2';
            my $lex = LexDir->new($tmpDir1, $tmpDir2) ;

            mkdir $tmpDir1, 0777;
            mkdir $tmpDir2, 0777;

            ok   -d $tmpDir1, "  Temp Directory $tmpDir1 exists";
            #ok ! -d $tmpDir2, "  Temp Directory $tmpDir2 does not exist";

            my @files = @( map { "$tmpDir1/$_.tmp" } < @$files ) ;
            foreach (< @files) { writeFile($_, "abc $_") }

            my @expected = @( map { "abc $_" } < @files ) ;
            my @outFiles = @( map { s/$tmpDir1/$tmpDir2/; $_ } < @files ) ;

            {
                title "$TopType - From FileGlob to FileGlob files [{join ' ', <@$files}]" ;

                ok &$Func("<$tmpDir1/a*.tmp>" => "<$tmpDir2/a#1.tmp>"), '  Compressed ok' 
                    or diag $$Error ;

                my @copy = @expected;
                for my $file (@outFiles)
                {
                    is anyUncompress($file), shift @copy, "  got expected from $file" ;
                }

                is (nelems @copy), 0, "  got all files";
            }

            {
                title "$TopType - From FileGlob to Array files [{join ' ', <@$files}]" ;

                my @buffer = @('first') ;
                ok &$Func("<$tmpDir1/a*.tmp>" => \@buffer), '  Compressed ok' 
                    or diag $$Error ;

                is shift @buffer, 'first';

                my @copy = @( < @expected );
                for my $buffer (< @buffer)
                {
                    is anyUncompress($buffer), shift @copy, "  got expected " ;
                }

                is (nelems @copy), 0, "  got all files";
            }

            foreach my $ms (< @MultiValues)
            {
                {
                    title "$TopType - From FileGlob to Buffer files [{join ' ', <@$files}], MS $ms" ;

                    my $buffer ;
                    ok &$Func("<$tmpDir1/a*.tmp>" => \$buffer, 
                               MultiStream => $ms), '  Compressed ok' 
                        or diag $$Error ;

                    #hexDump(\$buffer);

                    my $got = anyUncompress(\@( \$buffer, MultiStream => $ms ));

                    is $got, join("", < @expected), "  got expected" ;
                    my @headers = @( < getHeaders(\$buffer) );
                    is (nelems @headers), $ms ? (nelems @files) : 1, "  Header count ok";
                }

                {
                    title "$TopType - From FileGlob to Filename files [{join ' ', <@$files}], MS $ms" ;

                    my $filename = "abcde";
                    my $lex = LexFile->new($filename) ;
                    
                    ok &$Func("<$tmpDir1/a*.tmp>" => $filename,
                              MultiStream => $ms), '  Compressed ok' 
                        or diag $$Error ;

                    #hexDump(\$buffer);

                    my $got = anyUncompress(\@($filename, MultiStream => $ms));

                    is $got, join("", < @expected), "  got expected" ;
                    my @headers = @( < getHeaders($filename) );
                    is (nelems @headers), $ms ? (nelems @files) : 1, "  Header count ok";
                }

                {
                    title "$TopType - From FileGlob to Filehandle files [{join ' ', <@$files}], MS $ms" ;

                    my $filename = "abcde";
                    my $lex = LexFile->new($filename) ;
                    my $fh = 'IO::File'->new( "$filename", ">");
                    
                    ok &$Func("<$tmpDir1/a*.tmp>" => $fh, 
                              MultiStream => $ms, AutoClose => 1), '  Compressed ok' 
                        or diag $$Error ;

                    #hexDump(\$buffer);

                    my $got = anyUncompress(\@($filename, MultiStream => $ms));

                    is $got, join("", < @expected), "  got expected" ;
                    my @headers = @( < getHeaders($filename) );
                    is (nelems @headers), $ms ? (nelems @files) : 1, "  Header count ok";
                }
            }
        }

    }

    foreach my $bit ($UncompressClass,
                     'IO::Uncompress::AnyUncompress',
                    )
    {
        my $Error = getErrorRef($bit);
        my $Func = getTopFuncRef($bit);
        my $TopType = getTopFuncName($bit);

        my $buffer = "abcde" ;
        my $buffer2 = "ABCDE" ;
        my $keep_orig = $buffer;

        my $comp = compressBuffer( <getTopFuncName($UncompressClass), $buffer) ;
        my $comp2 = compressBuffer( <getTopFuncName($UncompressClass), $buffer2) ;
        my $keep_comp = $comp;

        my $incumbent = "incumbent data" ;

        my @opts = @(Strict => 1);
        push @opts,  (RawInflate => 1)
            if $bit eq 'IO::Uncompress::AnyUncompress';

        for my $append (0, 1)
        {
            my $expected = $buffer ;
            $expected = $incumbent . $buffer if $append ;

            {
                title "$TopType - From Buff to Buff, Append($append)" ;

                my $output ;
                $output = $incumbent if $append ;
                ok &$Func(\$comp, \$output, Append => $append, < @opts), '  Uncompressed ok' ;

                is $keep_comp, $comp, "  Input buffer not changed" ;
                is $output, $expected, "  Uncompressed matches original";
            }

            {
                title "$TopType - From Buff to Array, Append($append)" ;

                my @output = @('first');
                #$output = $incumbent if $append ;
                ok &$Func(\$comp, \@output, Append => $append, < @opts), '  Uncompressed ok' ;

                is $keep_comp, $comp, "  Input buffer not changed" ;
                is @output[0], 'first', "  Uncompressed matches original";
                is ${ @output[1] }, $buffer, "  Uncompressed matches original"
                    or diag @output[1] ;
                is (nelems @output), 2, "  only 2 elements in the array" ;
            }

            {
                title "$TopType - From Buff to Filename, Append($append)" ;

                my $lex = LexFile->new(my $out_file) ;
                if ($append)
                  { writeFile($out_file, $incumbent) }
                else
                  { ok ! -e $out_file, "  Output file does not exist" }

                ok &$Func(\$comp, $out_file, Append => $append, < @opts), '  Uncompressed ok' ;

                ok -e $out_file, "  Created output file";
                my $content = readFile($out_file) ;

                is $keep_comp, $comp, "  Input buffer not changed" ;
                is $content, $expected, "  Uncompressed matches original";
            }

            {
                title "$TopType - From Buff to Handle, Append($append)" ;

                my $lex = LexFile->new(my $out_file) ;
                my $of ;
                if ($append) {
                    writeFile($out_file, $incumbent) ;
                    $of = 'IO::File'->new( "$out_file", "+<") ;
                }
                else {
                    ok ! -e $out_file, "  Output file does not exist" ;
                    $of = 'IO::File'->new( "$out_file", ">") ;
                }
                isa_ok $of, 'IO::File', '  $of' ;

                ok &$Func(\$comp, $of, Append => $append, AutoClose => 1, < @opts), '  Uncompressed ok' ;

                ok -e $out_file, "  Created output file";
                my $content = readFile($out_file) ;

                is $keep_comp, $comp, "  Input buffer not changed" ;
                is $content, $expected, "  Uncompressed matches original";
            }

            {
                title "$TopType - From Filename to Filename, Append($append)" ;

                my $lex = LexFile->new(my $in_file, my $out_file) ;
                if ($append)
                  { writeFile($out_file, $incumbent) }
                else
                  { ok ! -e $out_file, "  Output file does not exist" }

                writeFile($in_file, $comp);

                ok &$Func($in_file, $out_file, Append => $append, < @opts), '  Uncompressed ok' ;

                ok -e $out_file, "  Created output file";
                my $content = readFile($out_file) ;

                is $keep_comp, $comp, "  Input buffer not changed" ;
                is $content, $expected, "  Uncompressed matches original";
            }

            {
                title "$TopType - From Filename to Handle, Append($append)" ;

                my $lex = LexFile->new(my $in_file, my $out_file) ;
                my $out ;
                if ($append) {
                    writeFile($out_file, $incumbent) ;
                    $out = 'IO::File'->new( "$out_file", "+<") ;
                }
                else {
                    ok ! -e $out_file, "  Output file does not exist" ;
                    $out = 'IO::File'->new( "$out_file", ">") ;
                }
                isa_ok $out, 'IO::File', '  $out' ;

                writeFile($in_file, $comp);

                ok &$Func($in_file, $out, Append => $append, AutoClose => 1, < @opts), '  Uncompressed ok' ;

                ok -e $out_file, "  Created output file";
                my $content = readFile($out_file) ;

                is $keep_comp, $comp, "  Input buffer not changed" ;
                is $content, $expected, "  Uncompressed matches original";
            }

            {
                title "$TopType - From Filename to Buffer, Append($append)" ;

                my $lex = LexFile->new(my $in_file) ;
                writeFile($in_file, $comp);

                my $output ;
                $output = $incumbent if $append ;

                ok &$Func($in_file, \$output, Append => $append, < @opts), '  Uncompressed ok' ;

                is $keep_comp, $comp, "  Input buffer not changed" ;
                is $output, $expected, "  Uncompressed matches original";
            }

            {
                title "$TopType - From Handle to Filename, Append($append)" ;

                my $lex = LexFile->new(my $in_file, my $out_file) ;
                if ($append)
                  { writeFile($out_file, $incumbent) }
                else
                  { ok ! -e $out_file, "  Output file does not exist" }

                writeFile($in_file, $comp);
                my $in = 'IO::File'->new( "$in_file", "<") ;

                ok &$Func($in, $out_file, Append => $append, < @opts), '  Uncompressed ok' ;

                ok -e $out_file, "  Created output file";
                my $content = readFile($out_file) ;

                is $keep_comp, $comp, "  Input buffer not changed" ;
                is $content, $expected, "  Uncompressed matches original";
            }

            {
                title "$TopType - From Handle to Handle, Append($append)" ;

                my $lex = LexFile->new(my $in_file, my $out_file) ;
                my $out ;
                if ($append) {
                    writeFile($out_file, $incumbent) ;
                    $out = 'IO::File'->new( "$out_file", "+<") ;
                }
                else {
                    ok ! -e $out_file, "  Output file does not exist" ;
                    $out = 'IO::File'->new( "$out_file", ">") ;
                }
                isa_ok $out, 'IO::File', '  $out' ;

                writeFile($in_file, $comp);
                my $in = 'IO::File'->new( "$in_file", "<") ;

                ok &$Func($in, $out, Append => $append, AutoClose => 1, < @opts), '  Uncompressed ok' ;

                ok -e $out_file, "  Created output file";
                my $content = readFile($out_file) ;

                is $keep_comp, $comp, "  Input buffer not changed" ;
                is $content, $expected, "  Uncompressed matches original";
            }

            {
                title "$TopType - From Filename to Buffer, Append($append)" ;

                my $lex = LexFile->new(my $in_file) ;
                writeFile($in_file, $comp);
                my $in = 'IO::File'->new( "$in_file", "<") ;

                my $output ;
                $output = $incumbent if $append ;

                ok &$Func($in, \$output, Append => $append, < @opts), '  Uncompressed ok' ;

                is $keep_comp, $comp, "  Input buffer not changed" ;
                is $output, $expected, "  Uncompressed matches original";
            }

            {
                title "$TopType - From stdin (via '-') to Buffer content, Append($append) " ;

                my $lex = LexFile->new(my $in_file) ;
                writeFile($in_file, $comp);

                open(SAVEIN, "<&", $^STDIN);
                my $dummy = fileno SAVEIN ;
                ok open(STDIN, "<", "$in_file"), "  redirect STDIN";

                my $output ;
                $output = $incumbent if $append ;

                ok &$Func('-', \$output, Append => $append, < @opts), '  Uncompressed ok' 
                    or diag $$Error ;

                open(STDIN, "<&", \*SAVEIN);

                is $keep_comp, $comp, "  Input buffer not changed" ;
                is $output, $expected, "  Uncompressed matches original";
            }
        }

        {
            title "$TopType - From Handle to Buffer, InputLength" ;

            my $lex = LexFile->new(my $in_file, my $out_file) ;
            my $out ;

            my $expected = $buffer ;
            my $appended = 'appended';
            my $len_appended = length $appended;
            writeFile($in_file, $comp . $appended . $comp . $appended) ;
            my $in = 'IO::File'->new( "$in_file", "<") ;

            ok &$Func($in, \$out, Transparent => 0, InputLength => length $comp, < @opts), '  Uncompressed ok' ;

            is $out, $expected, "  Uncompressed matches original";

            my $buff;
            is $in->read($buff, $len_appended), $len_appended, "  Length of Appended data ok";
            is $buff, $appended, "  Appended data ok";

            $out = '';
            ok &$Func($in, \$out, Transparent => 0, InputLength => length $comp, < @opts), '  Uncompressed ok' ;

            is $out, $expected, "  Uncompressed matches original";

            $buff = '';
            is $in->read($buff, $len_appended), $len_appended, "  Length of Appended data ok";
            is $buff, $appended, "  Appended data ok";
        }

        for my $stdin ('-', $^TDIN) # , \*STDIN)
        {
            title "$TopType - From stdin to Buffer content, InputLength" ;

            my $lex = LexFile->new( my $in_file) ;
            my $expected = $buffer ;
            my $appended = 'appended';
            my $len_appended = length $appended;
            writeFile($in_file, $comp . $appended ) ;

               open(SAVEIN, "<&", $^STDIN);
            my $dummy = fileno SAVEIN ;
            ok open(STDIN, "<", "$in_file"), "  redirect STDIN";

            my $output ;

            ok &$Func($stdin, \$output, Transparent => 0, InputLength => length $comp, < @opts), '  Uncompressed ok' 
                or diag $$Error ;

            my $buff ;
            is read(STDIN, $buff, $len_appended), $len_appended, "  Length of Appended data ok";

            is $output, $expected, "  Uncompressed matches original";
            is $buff, $appended, "  Appended data ok";

              open(STDIN, "<&", \*SAVEIN);
        }
    }

    foreach my $bit ($UncompressClass,
                     'IO::Uncompress::AnyUncompress',
                    )
    {
        # TODO -- Add Append mode tests

        my $Error = getErrorRef($bit);
        my $Func = getTopFuncRef($bit);
        my $TopType = getTopFuncName($bit);

        my $buffer = "abcde" ;
        my $keep_orig = $buffer;

        my $null = compressBuffer( <getTopFuncName($UncompressClass), "") ;
        my $undef = compressBuffer( <getTopFuncName($UncompressClass), undef) ;
        my $comp = compressBuffer( <getTopFuncName($UncompressClass), $buffer) ;
        my $keep_comp = $comp;

        my @opts = @( () );
        @opts = @(RawInflate => 1)
            if $bit eq 'IO::Uncompress::AnyUncompress';

        my $incumbent = "incumbent data" ;

        my $lex = LexFile->new(my $file1, my $file2) ;

        writeFile($file1, < compressBuffer( <getTopFuncName($UncompressClass),"data1"));
        writeFile($file2, < compressBuffer( <getTopFuncName($UncompressClass),"data2"));

        my $of = 'IO::File'->new( "$file1", "<") ;
        ok $of, "  Created output filehandle" ;

        #my @input    = ($file2, \$undef, \$null, \$comp, $of) ;
        #my @expected = ('data2', '',      '',    'abcde', 'data1');
        my @input    = @($file1, $file2);
        my @expected = @('data1', 'data2');

        my @keep = @( < @input ) ;

        {
            title "$TopType - From ArrayRef to Buffer" ;

            my $output  ;
            ok &$Func(\@input, \$output, AutoClose => 0, < @opts), '  UnCompressed ok' ;

            is $output, join('', < @expected)
        }

        {
            title "$TopType - From ArrayRef to Filename" ;

            my $lex = LexFile->new( my $output);
            $of->open("$file1", "<") ;

            ok &$Func(\@input, $output, AutoClose => 0, < @opts), '  UnCompressed ok' ;

            is readFile($output), join('', < @expected)
        }

        {
            title "$TopType - From ArrayRef to Filehandle" ;

            my $lex = LexFile->new( my $output);
            my $fh = 'IO::File'->new( "$output", ">") ;
            $of->open("$file1", "<") ;

            ok &$Func(\@input, $fh, AutoClose => 0, < @opts), '  UnCompressed ok' ;
            $fh->close;

            is readFile($output), join('', < @expected)
        }

        {
            title "$TopType - From Array Ref to Array Ref" ;

            my @output = @(\'first') ;
            $of->open("$file1", '<') ;
            ok &$Func(\@input, \@output, AutoClose => 0, < @opts), '  UnCompressed ok' ;

            is_deeply \@input, \@keep, "  Input array not changed" ;
            is_deeply \@(map { defined $$_ ? $$_ : "" } < @output), 
                      \@('first', < @expected), 
                      "  Got Expected uncompressed data";

        }
    }

    foreach my $bit ($UncompressClass,
                     'IO::Uncompress::AnyUncompress',
                    )
    {
        # TODO -- Add Append mode tests

        my $Error = getErrorRef($bit);
        my $Func = getTopFuncRef($bit);
        my $TopType = getTopFuncName($bit);

        my $tmpDir1 = 'tmpdir1';
        my $tmpDir2 = 'tmpdir2';
        my $lex = LexDir->new($tmpDir1, $tmpDir2) ;

        mkdir $tmpDir1, 0777;
        mkdir $tmpDir2, 0777;

        my @opts = @( () );
        @opts = @(RawInflate => 1)
            if $bit eq 'IO::Uncompress::AnyUncompress';

        ok   -d $tmpDir1, "  Temp Directory $tmpDir1 exists";
        #ok ! -d $tmpDir2, "  Temp Directory $tmpDir2 does not exist";

        my @files = @( map { "$tmpDir1/$_.tmp" } qw( a1 a2 a3) ) ;
        foreach (< @files) { writeFile($_, < compressBuffer( <getTopFuncName($UncompressClass), "abc $_")) }

        my @expected = @( map { "abc $_" } < @files ) ;
        my @outFiles = @( map { s/$tmpDir1/$tmpDir2/; $_ } < @files ) ;

        {
            title "$TopType - From FileGlob to FileGlob" ;

            ok &$Func("<$tmpDir1/a*.tmp>" => "<$tmpDir2/a#1.tmp>", < @opts), '  UnCompressed ok' 
                or diag $$Error ;

            my @copy = @( < @expected );
            for my $file (< @outFiles)
            {
                is readFile($file), shift @copy, "  got expected from $file" ;
            }

            is (nelems @copy), 0, "  got all files";
        }

        {
            title "$TopType - From FileGlob to Arrayref" ;

            my @output = @(\'first');
            ok &$Func("<$tmpDir1/a*.tmp>" => \@output, < @opts), '  UnCompressed ok' 
                or diag $$Error ;

            my @copy = @('first', < @expected);
            for my $data (< @output)
            {
                is $$data, shift @copy, "  got expected data" ;
            }

            is (nelems @copy), 0, "  got all files";
        }

        {
            title "$TopType - From FileGlob to Buffer" ;

            my $output ;
            ok &$Func("<$tmpDir1/a*.tmp>" => \$output, < @opts), '  UnCompressed ok' 
                or diag $$Error ;

            is $output, join('', < @expected), "  got expected uncompressed data";
        }

        {
            title "$TopType - From FileGlob to Filename" ;

            my $lex = LexFile->new( my $output) ;
            ok ! -e $output, "  $output does not exist" ;
            ok &$Func("<$tmpDir1/a*.tmp>" => $output, < @opts), '  UnCompressed ok' 
                or diag $$Error ;

            ok -e $output, "  $output does exist" ;
            is readFile($output), join('', < @expected), "  got expected uncompressed data";
        }

        {
            title "$TopType - From FileGlob to Filehandle" ;

            my $output = 'abc' ;
            my $lex = LexFile->new( $output) ;
            my $fh = 'IO::File'->new( "$output", ">") ;
            ok &$Func("<$tmpDir1/a*.tmp>" => $fh, AutoClose => 1, < @opts), '  UnCompressed ok' 
                or diag $$Error ;

            ok -e $output, "  $output does exist" ;
            is readFile($output), join('', < @expected), "  got expected uncompressed data";
        }

    }

    foreach my $TopType ($CompressClass
                         # TODO -- add the inflate classes
                        )
    {
        my $Error = getErrorRef($TopType);
        my $Func = getTopFuncRef($TopType);
        my $Name = getTopFuncName($TopType);

        title "More write tests" ;

        my $lex = LexFile->new(my $file1, my $file2, my $file3) ;

        writeFile($file1, "F1");
        writeFile($file2, "F2");
        writeFile($file3, "F3");

#        my @data = (
#              [ '[\"ab", \"cd"]',                        "abcd" ],
#
#              [ '[\"a", $fh1, \"bc"]',                   "aF1bc"],
#            ) ;
#
#
#        foreach my $data (@data)
#        {
#            my ($send, $get) = @$data ;
#
#            my $fh1 = new IO::File "< $file1" ;
#            my $fh2 = new IO::File "< $file2" ;
#            my $fh3 = new IO::File "< $file3" ;
#
#            title "$send";
#            my ($copy);
#            eval "\$copy = $send";
#            my $Answer ;
#            ok &$Func($copy, \$Answer), "  $Name ok";
#
#            my $got = anyUncompress(\$Answer);
#            is $got, $get, "  got expected output" ;
#            ok ! $$Error,  "  no error"
#                or diag "Error is $$Error";
#
#        }

        title "Array Input Error tests" ;

        my @data = @(
                   \@( '\@()',    "empty array reference"),
                   \@( '\@(\@())',    "unknown input parameter"),
                   \@( '\@(\@(\@()))',   "unknown input parameter"),
                   \@( '\@(\@(\"ab"), \@(\"cd"))', "unknown input parameter"),
                   \@( '\@(\"")',     "not a filename"),
                   \@( '\@(\undef)',  "not a filename"),
                   \@( '\@(\"abcd")', "not a filename"),
                   \@( '\@(\&xx)',      "unknown input parameter"),
                   \@( '\@($fh2)',      "not a filename"),
                ) ;


        foreach my $data (< @data)
        {
            my ($send, $get) = < @$data ;

            my $fh1 = 'IO::File'->new( "$file1", "<") ;
            my $fh2 = 'IO::File'->new( "$file2", "<") ;
            my $fh3 = 'IO::File'->new( "$file3", "<") ;

            title "$send";
            my($copy);
            eval "\$copy = $send";
            my $Answer ;
            my $a ;
            try { $a = &$Func($copy, \$Answer) };
            ok ! $a, "  $Name fails";

            is $$Error, $get, "  got error message";

        }

        @data = @(
                   '\@("")', 
                   '\@(undef)', 
                ) ;


        foreach my $send (< @data)
        {
            title "$send";
            my($copy);
            eval "\$copy = $send";
            my $Answer ;
            try { &$Func($copy, \$Answer) } ;
            like $@->{description}, mkErr("^$TopFuncName: input filename is undef or null string"), 
                "  got error message";

        }
    }

}

# TODO add more error cases

1;
