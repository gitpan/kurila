case $PERL_CONFIG_SH in
'')
	if test -f config.sh; then TOP=.;
	elif test -f ../config.sh; then TOP=..;
	elif test -f ../../config.sh; then TOP=../..;
	elif test -f ../../../config.sh; then TOP=../../..;
	elif test -f ../../../../config.sh; then TOP=../../../..;
	else
		echo "Can't find config.sh."; exit 1
	fi
	. $TOP/config.sh
	;;
esac
: This forces SH files to create target in same directory as SH file.
: This is so that make depend always knows where to find SH derivatives.
case "$0" in
*/*) cd `expr X$0 : 'X\(.*\)/'` ;;
esac

if test -d pod; then
  cd pod || exit 1
fi
POD=`echo *.pod`
MAN=`echo $POD|sed 's/\.pod/\.man/g'`
HTML=`echo $POD|sed 's/kurilatoc.pod//'|sed 's/\.pod/\.html/g'`
TEX=`echo $POD|sed 's/\.pod/\.tex/g'`

echo "Extracting pod/Makefile (with variable substitutions)"
: This section of the file will have variable substitutions done on it.
: Move anything that needs config subs from !NO!SUBS! section to !GROK!THIS!.
: Protect any dollar signs and backticks that you do not want interpreted
: by putting a backslash in front.  You may delete these comments.

$spitshell >Makefile <<!GROK!THIS!
# pod/Makefile
# This file is derived from pod/Makefile.SH.  Any changes made here will
# be lost the next time you run Configure.

POD = $POD

MAN = $MAN

# no kurilatoc.html
HTML = $HTML

TEX  = $TEX  

# The following is used to include the current directory in
# the dynamic loader path you are building a shared libperl.
LDLIBPTH = $ldlibpth

CPS = $cp

!GROK!THIS!

## In the following dollars and backticks do not need the extra backslash.
$spitshell >>Makefile <<'!NO!SUBS!'

CONVERTERS = pod2html pod2latex pod2man pod2text checkpods \
		pod2usage podchecker podselect

HTMLROOT = /	# Change this to fix cross-references in HTML
POD2HTML = pod2html \
	    --htmlroot=$(HTMLROOT) \
	    --podroot=.. --podpath=pod:lib:ext:vms \
	    --libpods=perlfunc:perlguts:perlvar:perlrun:perlop

PERL = ../miniperl
PERLILIB = $(PERL) -I../lib
REALPERL = ../perl

# From MakeMaker.
FIXIN = "-I../lib" "-MExtUtils::MY" -e "MY->fixin(shift @ARGV)"

all: $(CONVERTERS) man

converters: $(CONVERTERS)

regen_pods: perlmodlib.pod toc

kurilatoc.pod:	buildtoc

man:	pod2man $(MAN)

html:	pod2html $(HTML)

tex:	pod2latex $(TEX)

toc kurilatoc.pod:	buildtoc
	$(PERLILIB) buildtoc --build-toc

.SUFFIXES: .pm .pod

.SUFFIXES: .man

.pm.man:     pod2man
	$(REALPERL) -I../lib pod2man $*.pm >$*.man

.pod.man:     pod2man
	$(REALPERL) -I../lib pod2man $*.pod >$*.man

.SUFFIXES: .html

.pm.html:    pod2html
	$(PERL) -I../lib $(POD2HTML) --infile=$*.pm --outfile=$*.html

.pod.html:    pod2html
	$(PERL) -I../lib $(POD2HTML) --infile=$*.pod --outfile=$*.html

.SUFFIXES: .tex

.pm.tex: pod2latex
	$(PERL) -I../lib pod2latex $*.pm

.pod.tex: pod2latex
	$(PERL) -I../lib pod2latex $*.pod

clean:
	rm -f $(MAN)
	rm -f $(HTML)
	rm -f $(TEX)
	rm -f pod2html-*cache
	rm -f *.aux *.log *.exe

realclean:	clean
	rm -f $(CONVERTERS)

distclean:	realclean

veryclean:	distclean
	-rm -f *~ *.orig

check:	checkpods
	@echo "checking..."; \
	$(PERL) -I../lib checkpods $(POD)

# Dependencies.
pod2html:	pod2html.pl ../config.sh
	$(CPS) $@.pl $@
	$(LDLIBPTH) $(PERL) $(FIXIN) $@

pod2latex:	pod2latex.pl ../config.sh
	$(CPS) $@.pl $@
	$(LDLIBPTH) $(PERL) $(FIXIN) $@

pod2man:	pod2man.pl ../config.sh
	$(CPS) $@.pl $@
	$(LDLIBPTH) $(PERL) $(FIXIN) $@

pod2text:	pod2text.pl ../config.sh
	$(CPS) $@.pl $@
	$(LDLIBPTH) $(PERL) $(FIXIN) $@

checkpods:	checkpods.pl ../config.sh
	$(CPS) $@.pl $@
	$(LDLIBPTH) $(PERL) $(FIXIN) $@

pod2usage:	pod2usage.pl ../config.sh
	$(CPS) $@.pl $@
	$(LDLIBPTH) $(PERL) $(FIXIN) $@

podchecker:	podchecker.pl ../config.sh
	$(CPS) $@.pl $@
	$(LDLIBPTH) $(PERL) $(FIXIN) $@

podselect:	podselect.pl ../config.sh
	$(CPS) $@.pl $@
	$(LDLIBPTH) $(PERL) $(FIXIN) $@

perlmodlib.pod:	$(PERL) perlmodlib.PL ../MANIFEST
	rm -f perlmodlib.pod
	$(PERL) -I ../lib perlmodlib.PL

!NO!SUBS!
