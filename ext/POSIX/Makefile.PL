
use ExtUtils::MakeMaker;
use ExtUtils::Constant v0.11 'WriteConstants';
use Config;
my $rt_signals;
if (config_value("sig_name") =~ m/\bRTMIN\b/ 
      && config_value("sig_name") =~ m/\bRTMAX\b/) {
    # Yes, it really has them. (Some systems define the C pre-processor
    # symbols, but they depend on other macros that are only defined during
    # kernel compilation
    $rt_signals++;
}

my @libs;
if ($^OS_NAME ne 'MSWin32') {
    @libs = @('LIBS' => \@("-lm -lposix -lcposix"));
}
WriteMakefile(
    NAME	=> 'POSIX',
    < @libs,
    XSPROTOARG	=> '-noprototypes',	# XXX remove later?
    VERSION_FROM => 'POSIX.pm',
    realclean	=> \%(FILES=> 'const-c.inc const-xs.inc'),
);

my @names =
  @( <
   qw(ARG_MAX B0 B110 B1200 B134 B150 B1800 B19200 B200 B2400 B300 B38400 B4800
      B50 B600 B75 B9600 BRKINT BUFSIZ CHAR_BIT CHAR_MAX CHAR_MIN CHILD_MAX
      CLOCAL CLOCKS_PER_SEC CREAD CS5 CS6 CS7 CS8 CSIZE CSTOPB E2BIG
      EACCES EADDRINUSE EADDRNOTAVAIL EAFNOSUPPORT EAGAIN EALREADY EBADF
      EBUSY ECHILD ECHO ECHOE ECHOK ECHONL ECONNABORTED ECONNREFUSED
      ECONNRESET EDEADLK EDESTADDRREQ EDOM EDQUOT EEXIST EFAULT EFBIG
      EHOSTDOWN EHOSTUNREACH EINPROGRESS EINTR EINVAL EIO EISCONN EISDIR
      ELOOP EMFILE EMLINK EMSGSIZE ENAMETOOLONG ENETDOWN ENETRESET
      ENETUNREACH ENFILE ENOBUFS ENODEV ENOENT ENOEXEC ENOLCK ENOMEM
      ENOPROTOOPT ENOSPC ENOSYS ENOTBLK ENOTCONN ENOTDIR ENOTEMPTY ENOTSOCK
      ENOTTY ENXIO EOF EOPNOTSUPP EPERM EPFNOSUPPORT EPIPE EPROCLIM
      EPROTONOSUPPORT EPROTOTYPE ERANGE EREMOTE ERESTART EROFS ESHUTDOWN
      ESOCKTNOSUPPORT ESPIPE ESRCH ESTALE ETIMEDOUT ETOOMANYREFS ETXTBSY
      EUSERS EWOULDBLOCK EXDEV FILENAME_MAX F_OK HUPCL ICANON ICRNL IEXTEN
      IGNBRK IGNCR IGNPAR INLCR INPCK INT_MAX INT_MIN ISIG ISTRIP IXOFF IXON
      LC_ALL LC_COLLATE LC_CTYPE LC_MESSAGES LC_MONETARY LC_NUMERIC LC_TIME
      LINK_MAX LONG_MAX LONG_MIN L_ctermid L_cuserid L_tmpnam MAX_CANON
      MAX_INPUT MB_LEN_MAX MSG_CTRUNC MSG_DONTROUTE MSG_EOR MSG_OOB MSG_PEEK 
      MSG_TRUNC MSG_WAITALL NAME_MAX NCCS NGROUPS_MAX NOFLSH OPEN_MAX OPOST
      PARENB PARMRK PARODD PATH_MAX PIPE_BUF RAND_MAX R_OK SCHAR_MAX
      SCHAR_MIN SEEK_CUR SEEK_END SEEK_SET SHRT_MAX SHRT_MIN SIGABRT SIGALRM
      SIGCHLD SIGCONT SIGFPE SIGHUP SIGILL SIGINT SIGKILL SIGPIPE SIGQUIT
      SIGSEGV SIGSTOP SIGTERM SIGTSTP SIGTTIN SIGTTOU
      SIGUSR1 SIGUSR2 SIG_BLOCK SIG_SETMASK SIG_UNBLOCK SSIZE_MAX
      STDERR_FILENO STDIN_FILENO STDOUT_FILENO STREAM_MAX
      S_IRGRP S_IROTH S_IRUSR S_IRWXG S_IRWXO S_IRWXU S_ISGID S_ISUID
      S_IWGRP S_IWOTH S_IWUSR S_IXGRP S_IXOTH S_IXUSR TCIFLUSH TCIOFF
      TCIOFLUSH TCION TCOFLUSH TCOOFF TCOON TCSADRAIN TCSAFLUSH TCSANOW
      TMP_MAX TOSTOP TZNAME_MAX VEOF VEOL VERASE VINTR VKILL VMIN VQUIT
      VSTART VSTOP VSUSP VTIME WNOHANG WUNTRACED W_OK X_OK
      _PC_CHOWN_RESTRICTED _PC_LINK_MAX _PC_MAX_CANON _PC_MAX_INPUT
      _PC_NAME_MAX _PC_NO_TRUNC _PC_PATH_MAX _PC_PIPE_BUF _PC_VDISABLE
      _SC_ARG_MAX _SC_CHILD_MAX _SC_CLK_TCK _SC_JOB_CONTROL _SC_NGROUPS_MAX
      _SC_OPEN_MAX _SC_PAGESIZE _SC_SAVED_IDS _SC_STREAM_MAX _SC_TZNAME_MAX
      _SC_VERSION),
   \%(name=>"CLK_TCK", not_constant=>1),
   \%(name=>"MB_CUR_MAX", not_constant=>1),
   \%(name=>"EXIT_FAILURE", default=>\@("IV", "1")),
   \%(name=>"EXIT_SUCCESS", default=>\@("IV", "0")),
   \%(name=>"SIG_DFL", value=>"PTR2IV(SIG_DFL)", not_constant=>1),
   \%(name=>"SIG_ERR", value=>"PTR2IV(SIG_ERR)", not_constant=>1),
   \%(name=>"SIG_IGN", value=>"PTR2IV(SIG_IGN)", not_constant=>1),
   # L_tmpnam[e] was a typo--retained for compatibility
   \%(name=>"L_tmpname", value=>"L_tmpnam"),
   \%(name=>"NULL", value=>"0"),
   \%(name=>"_POSIX_JOB_CONTROL", type=>"YES", default=>\@("IV", "0")),
   \%(name=>"_POSIX_SAVED_IDS", type=>"YES", default=>\@("IV", "0")),
   \%(name=>'FLT_ROUNDS', type=>"NV", not_constant=>1),
   \%(name=>"HUGE_VAL", type=>"NV", not_constant=>1,
    macro=>\@(<<'END', "#endif\n"),
#if (defined(USE_LONG_DOUBLE) && defined(HUGE_VALL)) || defined(HUGE_VAL)
	/* HUGE_VALL is admittedly non-POSIX but if we are using long doubles
	 * we might as well use long doubles. --jhi */
END
    value=>'
#if defined(USE_LONG_DOUBLE) && defined(HUGE_VALL)
                   HUGE_VALL
#else
                   HUGE_VAL
#endif
                           '));

push @names, \%(name=>$_, type=>"UV")
  foreach @( ( <qw(SA_NOCLDSTOP SA_NOCLDWAIT SA_NODEFER SA_ONSTACK SA_RESETHAND
             SA_RESTART SA_SIGINFO UCHAR_MAX UINT_MAX ULONG_MAX USHRT_MAX)));
push @names, \%(name=>$_, type=>"NV", not_constant=>1)
  foreach @( ( <qw(DBL_MAX FLT_MAX LDBL_MAX LDBL_MIN LDBL_EPSILON
	       DBL_EPSILON DBL_MIN FLT_EPSILON FLT_MIN)));

push @names, \%(name=>$_, type=>"NV")
  foreach @( ( <qw(DBL_DIG DBL_MANT_DIG DBL_MAX_10_EXP DBL_MAX_EXP DBL_MIN_10_EXP
	      DBL_MIN_EXP FLT_DIG FLT_MANT_DIG FLT_MAX_10_EXP FLT_MAX_EXP
	      FLT_MIN_10_EXP FLT_MIN_EXP FLT_RADIX LDBL_DIG LDBL_MANT_DIG
	      LDBL_MAX_10_EXP LDBL_MAX_EXP LDBL_MIN_10_EXP LDBL_MIN_EXP)));

push @names, \%(name=>$_, type=>"IV", default=> \@("IV", "0"))
  foreach @( ( <qw(_POSIX_ARG_MAX _POSIX_CHILD_MAX _POSIX_CHOWN_RESTRICTED
              _POSIX_LINK_MAX _POSIX_MAX_CANON _POSIX_MAX_INPUT _POSIX_NAME_MAX
              _POSIX_NGROUPS_MAX _POSIX_NO_TRUNC _POSIX_OPEN_MAX
              _POSIX_PATH_MAX _POSIX_PIPE_BUF _POSIX_SSIZE_MAX
              _POSIX_STREAM_MAX _POSIX_TZNAME_MAX _POSIX_VDISABLE _POSIX_VERSION
             )));

if ($rt_signals) {
    push @names, \%(name=>"SIGRTMAX", not_constant=>1),
    \%(name=>"SIGRTMIN", not_constant=>1),
} else {
    # Well, it seems that your vendor has defined them, but not in a way that
    # would actually compile.
    push @names, \%(name=>"SIGRTMAX", macro=>0),
    \%(name=>"SIGRTMIN", macro=>0),
}

WriteConstants(
    PROXYSUBS => 1,
    NAME => 'POSIX',
    NAMES => \@names,
);
