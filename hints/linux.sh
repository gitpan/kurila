# hints/linux.sh
# Original version by rsanders
# Additional support by Kenneth Albanowski <kjahds@kjahds.com>
#
# ELF support by H.J. Lu <hjl@nynexst.com>
# Additional info from Nigel Head <nhead@ESOC.bitnet>
# and Kenneth Albanowski <kjahds@kjahds.com>
#
# Consolidated by Andy Dougherty <doughera@lafayette.edu>
#
# Updated Thu Feb  8 11:56:10 EST 1996

# Updated Thu May 30 10:50:22 EDT 1996 by <doughera@lafayette.edu>

# Updated Fri Jun 21 11:07:54 EDT 1996
# NDBM support for ELF renabled by <kjahds@kjahds.com>

# No version of Linux supports setuid scripts.
d_suidsafe='undef'

# Debian and Red Hat, and perhaps other vendors, provide both runtime and
# development packages for some libraries.  The runtime packages contain shared
# libraries with version information in their names (e.g., libgdbm.so.1.7.3);
# the development packages supplement this with versionless shared libraries
# (e.g., libgdbm.so).
#
# If you want to link against such a library, you must install the development
# version of the package.
#
# These packages use a -dev naming convention in both Debian and Red Hat:
#   libgdbmg1  (non-development version of GNU libc 2-linked GDBM library)
#   libgdbmg1-dev (development version of GNU libc 2-linked GDBM library)
# So make sure that for any libraries you wish to link Perl with under
# Debian or Red Hat you have the -dev packages installed.

# SuSE Linux can be used as cross-compilation host for Cray XT4 Catamount/Qk.
if test -d /opt/xt-pe
then
  case "`cc -V 2>&1`" in
  *catamount*) . hints/catamount.sh; return ;;
  esac
fi

# Some operating systems (e.g., Solaris 2.6) will link to a versioned shared
# library implicitly.  For example, on Solaris, `ld foo.o -lgdbm' will find an
# appropriate version of libgdbm, if one is available; Linux, however, doesn't
# do the implicit mapping.
ignore_versioned_solibs='y'

# BSD compatibility library no longer needed
# 'kaffe' has a /usr/lib/libnet.so which is not at all relevant for perl.
# bind causes issues with several reentrant functions
set `echo X "$libswanted "| sed -e 's/ bsd / /' -e 's/ net / /' -e 's/ bind / /'`
shift
libswanted="$*"

# If you have glibc, then report the version for ./myconfig bug reporting.
# (Configure doesn't need to know the specific version since it just uses
# gcc to load the library for all tests.)
# We don't use __GLIBC__ and  __GLIBC_MINOR__ because they
# are insufficiently precise to distinguish things like
# libc-2.0.6 and libc-2.0.7.
if test -L /lib/libc.so.6; then
    libc=`ls -l /lib/libc.so.6 | awk '{print $NF}'`
    libc=/lib/$libc
fi

# Configure may fail to find lstat() since it's a static/inline
# function in <sys/stat.h>.
d_lstat=define

# malloc wrap works
case "$usemallocwrap" in
'') usemallocwrap='define' ;;
esac

# The system malloc() is about as fast and as frugal as perl's.
# Since the system malloc() has been the default since at least
# 5.001, we might as well leave it that way.  --AD  10 Jan 2002
case "$usemymalloc" in
'') usemymalloc='n' ;;
esac

# Check if we're about to use Intel's ICC compiler
case "`${cc:-cc} -V 2>&1`" in
*"Intel(R) C++ Compiler"*|*"Intel(R) C Compiler"*)
    # This is needed for Configure's prototype checks to work correctly
    # The -mp flag is needed to pass various floating point related tests
    # The -no-gcc flag is needed otherwise, icc pretends (poorly) to be gcc
    ccflags="-we147 -mp -no-gcc $ccflags"
    # If we're using ICC, we usually want the best performance
    case "$optimize" in
    '') optimize='-O3' ;;
    esac
    ;;
*"Sun C"*)
    optimize='-xO2'
    cccdlflags='-KPIC'
    lddlflags='-G -Bdynamic'
    # Sun C doesn't support gcc attributes, but, in many cases, doesn't
    # complain either.  Not all cases, though.
    d_attribute_format='undef'
    d_attribute_malloc='undef'
    d_attribute_nonnull='undef'
    d_attribute_noreturn='undef'
    d_attribute_pure='undef'
    d_attribute_unused='undef'
    d_attribute_warn_unused_result='undef'
    ;;
esac

case "$optimize" in
# use -O2 by default ; -O3 doesn't seem to bring significant benefits with gcc
'')
    optimize='-O2'
    case "`uname -m`" in
        ppc*)
            # on ppc, it seems that gcc (at least gcc 3.3.2) isn't happy
            # with -O2 ; so downgrade to -O1.
            optimize='-O1'
        ;;
        ia64*)
            # This architecture has had various problems with gcc's
            # in the 3.2, 3.3, and 3.4 releases when optimized to -O2.  See
            # RT #37156 for a discussion of the problem.
            case "`${cc:-gcc} -v 2>&1`" in
            *"version 3.2"*|*"version 3.3"*|*"version 3.4"*)
                ccflags="-fno-delete-null-pointer-checks $ccflags"
            ;;
            esac
        ;;
    esac
    ;;
esac

# Are we using ELF?  Thanks to Kenneth Albanowski <kjahds@kjahds.com>
# for this test.
cat >try.c <<'EOM'
/* Test for whether ELF binaries are produced */
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
main() {
	char buffer[4];
	int i=open("a.out",O_RDONLY);
	if(i==-1)
		exit(1); /* fail */
	if(read(i,&buffer[0],4)<4)
		exit(1); /* fail */
	if(buffer[0] != 127 || buffer[1] != 'E' ||
           buffer[2] != 'L' || buffer[3] != 'F')
		exit(1); /* fail */
	exit(0); /* succeed (yes, it's ELF) */
}
EOM
if ${cc:-gcc} try.c >/dev/null 2>&1 && $run ./a.out; then
    cat <<'EOM' >&4

You appear to have ELF support.  I'll try to use it for dynamic loading.
If dynamic loading doesn't work, read hints/linux.sh for further information.
EOM

else
    cat <<'EOM' >&4

You don't have an ELF gcc.  I will use dld if possible.  If you are
using a version of DLD earlier than 3.2.6, or don't have it at all, you
should probably upgrade. If you are forced to use 3.2.4, you should
uncomment a couple of lines in hints/linux.sh and restart Configure so
that shared libraries will be disallowed.

EOM
    lddlflags="-r $lddlflags"
    # These empty values are so that Configure doesn't put in the
    # Linux ELF values.
    ccdlflags=' '
    cccdlflags=' '
    ccflags="-DOVR_DBL_DIG=14 $ccflags"
    so='sa'
    dlext='o'
    nm_so_opt=' '
    ## If you are using DLD 3.2.4 which does not support shared libs,
    ## uncomment the next two lines:
    #ldflags="-static"
    #so='none'

	# In addition, on some systems there is a problem with perl and NDBM
	# which causes AnyDBM and NDBM_File to lock up. This is evidenced
	# in the tests as AnyDBM just freezing.  Apparently, this only
	# happens on a.out systems, so we disable NDBM for all a.out linux
	# systems.  If someone can suggest a more robust test
	#  that would be appreciated.
	#
	# More info:
	# Date: Wed, 7 Feb 1996 03:21:04 +0900
	# From: Jeffrey Friedl <jfriedl@nff.ncl.omron.co.jp>
	#
	# I tried compiling with DBM support and sure enough things locked up
	# just as advertised. Checking into it, I found that the lockup was
	# during the call to dbm_open. Not *in* dbm_open -- but between the call
	# to and the jump into.
	#
	# To make a long story short, making sure that the *.a and *.sa pairs of
	#   /usr/lib/lib{m,db,gdbm}.{a,sa}
	# were perfectly in sync took care of it.
	#
	# This will generate a harmless Whoa There! message
	case "$d_dbm_open" in
	'')	cat <<'EOM' >&4

Disabling ndbm.  This will generate a Whoa There message in Configure.
Read hints/linux.sh for further information.
EOM
		# You can override this with Configure -Dd_dbm_open
		d_dbm_open=undef
		;;
	esac
fi

rm -f try.c a.out

if /bin/sh -c exit; then
  echo ''
  echo 'You appear to have a working bash.  Good.'
else
  cat << 'EOM' >&4

*********************** Warning! *********************
It would appear you have a defective bash shell installed. This is likely to
give you a failure of op/exec test #5 during the test phase of the build,
Upgrading to a recent version (1.14.4 or later) should fix the problem.
******************************************************
EOM

fi

# On SPARClinux,
# The following csh consistently coredumped in the test directory
# "/home/mikedlr/perl5.003_94/t", though not most other directories.

#Name        : csh                    Distribution: Red Hat Linux (Rembrandt)
#Version     : 5.2.6                        Vendor: Red Hat Software
#Release     : 3                        Build Date: Fri May 24 19:42:14 1996
#Install date: Thu Jul 11 16:20:14 1996 Build Host: itchy.redhat.com
#Group       : Shells                   Source RPM: csh-5.2.6-3.src.rpm
#Size        : 184417
#Description : BSD c-shell

# For this reason I suggest using the much bug-fixed tcsh for globbing
# where available.

# November 2001:  That warning's pretty old now and probably not so
# relevant, especially since perl now uses File::Glob for globbing.
# We'll still look for tcsh, but tone down the warnings.
# Andy Dougherty, Nov. 6, 2001
if $csh -c 'echo $version' >/dev/null 2>&1; then
    echo 'Your csh is really tcsh.  Good.'
else
    if xxx=`./UU/loc tcsh blurfl $pth`; $test -f "$xxx"; then
	echo "Found tcsh.  I'll use it for globbing."
	# We can't change Configure's setting of $csh, due to the way
	# Configure handles $d_portable and commands found in $loclist.
	# We can set the value for CSH in config.h by setting full_csh.
	full_csh=$xxx
    elif [ -f "$csh" ]; then
	echo "Couldn't find tcsh.  Csh-based globbing might be broken."
    fi
fi

# Shimpei Yamashita <shimpei@socrates.patnet.caltech.edu>
# Message-Id: <33EF1634.B36B6500@pobox.com>
#
# The DR2 of MkLinux (osname=linux,archname=ppc-linux) may need
# special flags passed in order for dynamic loading to work.
# instead of the recommended:
#
# ccdlflags='-rdynamic'
#
# it should be:
# ccdlflags='-Wl,-E'
#
# So if your DR2 (DR3 came out summer 1998, consider upgrading)
# has problems with dynamic loading, uncomment the
# following three lines, make distclean, and re-Configure:
#case "`uname -r | sed 's/^[0-9.-]*//'``arch`" in
#'osfmach3ppc') ccdlflags='-Wl,-E' ;;
#esac

case "`uname -m`" in
sparc*)
	case "$cccdlflags" in
	*-fpic*) cccdlflags="`echo $cccdlflags|sed 's/-fpic/-fPIC/'`" ;;
	*-fPIC*) ;;
	*)	 cccdlflags="$cccdlflags -fPIC" ;;
	esac
	;;
esac

# SuSE8.2 has /usr/lib/libndbm* which are ld scripts rather than
# true libraries. The scripts cause binding against static
# version of -lgdbm which is a bad idea. So if we have 'nm'
# make sure it can read the file
# NI-S 2003/08/07
if [ -r /usr/lib/libndbm.so  -a  -x /usr/bin/nm ] ; then
   if /usr/bin/nm /usr/lib/libndbm.so >/dev/null 2>&1 ; then
    echo 'Your shared -lndbm seems to be a real library.'
   else
    echo 'Your shared -lndbm is not a real library.'
    set `echo X "$libswanted "| sed -e 's/ ndbm / /'`
    shift
    libswanted="$*"
   fi
fi


# This script UU/usethreads.cbu will get 'called-back' by Configure
# after it has prompted the user for whether to use threads.
cat > UU/usethreads.cbu <<'EOCBU'
if getconf GNU_LIBPTHREAD_VERSION | grep NPTL >/dev/null 2>/dev/null
then
    threadshavepids=""
else
    threadshavepids="-DTHREADS_HAVE_PIDS"
fi
case "$usethreads" in
$define|true|[yY]*)
        ccflags="-D_REENTRANT -D_GNU_SOURCE $threadshavepids $ccflags"
        if echo $libswanted | grep -v pthread >/dev/null
        then
            set `echo X "$libswanted "| sed -e 's/ c / pthread c /'`
            shift
            libswanted="$*"
        fi

	# Somehow at least in Debian 2.2 these manage to escape
	# the #define forest of <features.h> and <time.h> so that
	# the hasproto macro of Configure doesn't see these protos,
	# even with the -D_GNU_SOURCE.

	d_asctime_r_proto="$define"
	d_crypt_r_proto="$define"
	d_ctime_r_proto="$define"
	d_gmtime_r_proto="$define"
	d_localtime_r_proto="$define"
	d_random_r_proto="$define"

	;;
esac
EOCBU

cat > UU/uselargefiles.cbu <<'EOCBU'
# This script UU/uselargefiles.cbu will get 'called-back' by Configure
# after it has prompted the user for whether to use large files.
case "$uselargefiles" in
''|$define|true|[yY]*)
# Keep this in the left margin.
ccflags_uselargefiles="-D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64"

	ccflags="$ccflags $ccflags_uselargefiles"
	;;
esac
EOCBU

# Purify fails to link Perl if a "-lc" is passed into its linker
# due to duplicate symbols.
case "$PURIFY" in
$define|true|[yY]*)
    set `echo X "$libswanted "| sed -e 's/ c / /'`
    shift
    libswanted="$*"
    ;;
esac

# If we are using g++ we must use nm and force ourselves to use
# the /usr/lib/libc.a (resetting the libc below to an empty string
# makes Configure to look for the right one) because the symbol
# scanning tricks of Configure will crash and burn horribly.
case "$cc" in
*g++*) usenm=true
       libc=''
       ;;
esac

# If using g++, the Configure scan for dlopen() and (especially)
# dlerror() might fail, easier just to forcibly hint them in.
case "$cc" in
*g++*)
  d_dlopen='define'
  d_dlerror='define'
  ;;
esac

# Under some circumstances libdb can get built in such a way as to
# need pthread explicitly linked.

libdb_needs_pthread="N"

if echo " $libswanted " | grep -v " pthread " >/dev/null
then
   if echo " $libswanted " | grep " db " >/dev/null
   then
     for DBDIR in $glibpth
     do
       DBLIB="$DBDIR/libdb.so"
       if [ -f $DBLIB ]
       then
         if nm -u $DBLIB | grep pthread >/dev/null
         then
           if ldd $DBLIB | grep pthread >/dev/null
           then
             libdb_needs_pthread="N"
           else
             libdb_needs_pthread="Y"
           fi
         fi
       fi
     done
   fi
fi

case "$libdb_needs_pthread" in
  "Y")
    libswanted="$libswanted pthread"
    ;;
esac
