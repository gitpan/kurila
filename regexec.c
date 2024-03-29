/*    regexec.c
 */

/*
 * "One Ring to rule them all, One Ring to find them..."
 */

/* This file contains functions for executing a regular expression.  See
 * also regcomp.c which funnily enough, contains functions for compiling
 * a regular expression.
 *
 * This file is also copied at build time to ext/re/re_exec.c, where
 * it's built with -DPERL_EXT_RE_BUILD -DPERL_EXT_RE_DEBUG -DPERL_EXT.
 * This causes the main functions to be compiled under new names and with
 * debugging support added, which makes "use re 'debug'" work.
 */

/* NOTE: this is derived from Henry Spencer's regexp code, and should not
 * confused with the original package (see point 3 below).  Thanks, Henry!
 */

/* Additional note: this code is very heavily munged from Henry's version
 * in places.  In some spots I've traded clarity for efficiency, so don't
 * blame Henry for some of the lack of readability.
 */

/* The names of the functions have been changed from regcomp and
 * regexec to  pregcomp and pregexec in order to avoid conflicts
 * with the POSIX routines of the same names.
*/

#ifdef PERL_EXT_RE_BUILD
#include "re_top.h"
#endif

/*
 * pregcomp and pregexec -- regsub and regerror are not used in perl
 *
 *	Copyright (c) 1986 by University of Toronto.
 *	Written by Henry Spencer.  Not derived from licensed software.
 *
 *	Permission is granted to anyone to use this software for any
 *	purpose on any computer system, and to redistribute it freely,
 *	subject to the following restrictions:
 *
 *	1. The author is not responsible for the consequences of use of
 *		this software, no matter how awful, even if they arise
 *		from defects in it.
 *
 *	2. The origin of this software must not be misrepresented, either
 *		by explicit claim or by omission.
 *
 *	3. Altered versions must be plainly marked as such, and must not
 *		be misrepresented as being the original software.
 *
 ****    Alterations to Henry's code are...
 ****
 ****    Copyright (C) 1991, 1992, 1993, 1994, 1995, 1996, 1997, 1998, 1999,
 ****    2000, 2001, 2002, 2003, 2004, 2005, 2006, 2007 by Larry Wall and others
 ****
 ****    You may distribute under the terms of either the GNU General Public
 ****    License or the Artistic License, as specified in the README file.
 *
 * Beware that some of this code is subtly aware of the way operator
 * precedence is structured in regular expressions.  Serious changes in
 * regular-expression syntax might require a total rethink.
 */
#include "EXTERN.h"
#define PERL_IN_REGEXEC_C
#include "perl.h"

#ifdef PERL_IN_XSUB_RE
#  include "re_comp.h"
#else
#  include "regcomp.h"
#endif

#define RF_warned	2		/* warned about big count? */

#define RF_utf8		8		/* Pattern contains multibyte chars? */

#define UTF ((PL_reg_flags & RF_utf8) != 0)

#define RS_init		1		/* eval environment created */
#define RS_set		2		/* replsv value is set */

#ifndef STATIC
#define	STATIC	static
#endif

/* #define REGINCLASS(prog,p,c)  (ANYOF_FLAGS(p) ? reginclass(prog,p,c,0) : ANYOF_BITMAP_TEST(p,*(c))) */
#define REGINCLASS(prog,p,c)  (ANYOF_BITMAP_TEST(p,*(c)))

/*
 * Forwards.
 */

#define CHR_DIST(a,b) (PL_reg_match_utf8 ? utf8_distance(a,b) : a - b)

#define HOP(pos,off) \
	(pos + off)
#define HOPc(pos,off) \
	(PL_reg_match_utf8 \
	    ? reghop3(pos, off, (off >= 0 ? PL_regeol : PL_bostr)) \
	    : (pos + off))
#define HOPBACKc(pos, off) \
	(PL_reg_match_utf8\
	    ? reghopmaybe3(pos, -off, PL_bostr) \
	    : (pos - off >= PL_bostr)		\
		? pos - off		\
		: NULL)

#define LOAD_UTF8_CHARCLASS(class,str) STMT_START { \
    if (!CAT2(PL_utf8_,class)) { \
       bool ok; ENTER; save_re_context(); ok=CAT2(is_utf8_,class)((const char*)str); assert(ok); LEAVE; \
    } } STMT_END
#define LOAD_UTF8_CHARCLASS_ALNUM() LOAD_UTF8_CHARCLASS(alnum,"a")
#define LOAD_UTF8_CHARCLASS_DIGIT() LOAD_UTF8_CHARCLASS(digit,"0")
#define LOAD_UTF8_CHARCLASS_SPACE() LOAD_UTF8_CHARCLASS(space," ")
#define LOAD_UTF8_CHARCLASS_MARK()  LOAD_UTF8_CHARCLASS(mark, "\xcd\x86")

/* TODO: Combine JUMPABLE and HAS_TEXT to cache OP(rn) */

/* for use after a quantifier and before an EXACT-like node -- japhy */
/* it would be nice to rework regcomp.sym to generate this stuff. sigh */
#define JUMPABLE(rn) (      \
    OP(rn) == OPEN ||       \
    (OP(rn) == CLOSE && (!cur_eval || cur_eval->u.eval.close_paren != ARG(rn))) || \
    OP(rn) == EVAL ||   \
    OP(rn) == SUSPEND || OP(rn) == IFMATCH || \
    OP(rn) == PLUS || OP(rn) == MINMOD || \
    OP(rn) == KEEPS || (PL_regkind[OP(rn)] == VERB) || \
    (PL_regkind[OP(rn)] == CURLY && ARG1(rn) > 0) \
)
#define IS_EXACT(rn) (PL_regkind[OP(rn)] == EXACT)

#define HAS_TEXT(rn) ( IS_EXACT(rn) || PL_regkind[OP(rn)] == REF )

#if 0 
/* Currently these are only used when PL_regkind[OP(rn)] == EXACT so
   we don't need this definition. */
#define IS_TEXT(rn)   ( OP(rn)==EXACT   || OP(rn)==REF   || OP(rn)==NREF   )
#define IS_TEXTF(rn)  ( 0  || OP(rn)==REFF  || OP(rn)==NREFF  )
#define IS_TEXTFL(rn) ( OP(rn)==REFFL || OP(rn)==NREFFL )

#else
/* ... so we use this as its faster. */
#define IS_TEXT(rn)   ( OP(rn)==EXACT   )
#define IS_TEXTF(rn)  ( 0  )
#define IS_TEXTFL(rn) ( 0 )

#endif

/*
  Search for mandatory following text node; for lookahead, the text must
  follow but for lookbehind (rn->flags != 0) we skip to the next step.
*/
#define FIND_NEXT_IMPT(rn) STMT_START { \
    while (JUMPABLE(rn)) { \
	const OPCODE type = OP(rn); \
	if (type == SUSPEND || PL_regkind[type] == CURLY) \
	    rn = NEXTOPER(NEXTOPER(rn)); \
	else if (type == PLUS) \
	    rn = NEXTOPER(rn); \
	else if (type == IFMATCH) \
	    rn = (rn->flags == 0) ? NEXTOPER(NEXTOPER(rn)) : rn + ARG(rn); \
	else rn += NEXT_OFF(rn); \
    } \
} STMT_END 


static void restore_pos(pTHX_ void *arg);

STATIC CHECKPOINT
S_regcppush(pTHX_ I32 parenfloor)
{
    dVAR;
    const int retval = PL_savestack_ix;
#define REGCP_PAREN_ELEMS 4
    const int paren_elems_to_push = (PL_regsize - parenfloor) * REGCP_PAREN_ELEMS;
    int p;
    GET_RE_DEBUG_FLAGS_DECL;

    if (paren_elems_to_push < 0)
	Perl_croak(aTHX_ "panic: paren_elems_to_push < 0");

#define REGCP_OTHER_ELEMS 7
    SSGROW(paren_elems_to_push + REGCP_OTHER_ELEMS);
    
    for (p = PL_regsize; p > parenfloor; p--) {
/* REGCP_PARENS_ELEMS are pushed per pairs of parentheses. */
	SSPUSHINT(PL_regoffs[p].end);
	SSPUSHINT(PL_regoffs[p].start);
	SSPUSHPTR(PL_reg_start_tmp[p]);
	SSPUSHINT(p);
	DEBUG_BUFFERS_r(PerlIO_printf(Perl_debug_log,
	  "     saving \\%"UVuf" %"IVdf"(%"IVdf")..%"IVdf"\n",
		      (UV)p, (IV)PL_regoffs[p].start,
		      (IV)(PL_reg_start_tmp[p] - PL_bostr),
		      (IV)PL_regoffs[p].end
	));
    }
/* REGCP_OTHER_ELEMS are pushed in any case, parentheses or no. */
    SSPUSHPTR(PL_regoffs);
    SSPUSHINT(PL_regsize);
    SSPUSHINT(*PL_reglastparen);
    SSPUSHINT(*PL_reglastcloseparen);
    SSPUSHPTR(PL_reginput);
#define REGCP_FRAME_ELEMS 2
/* REGCP_FRAME_ELEMS are part of the REGCP_OTHER_ELEMS and
 * are needed for the regexp context stack bookkeeping. */
    SSPUSHINT(paren_elems_to_push + REGCP_OTHER_ELEMS - REGCP_FRAME_ELEMS);
    SSPUSHINT(SAVEt_REGCONTEXT); /* Magic cookie. */

    return retval;
}

/* These are needed since we do not localize EVAL nodes: */
#define REGCP_SET(cp)                                           \
    DEBUG_STATE_r(                                              \
            PerlIO_printf(Perl_debug_log,		        \
	        "  Setting an EVAL scope, savestack=%"IVdf"\n",	\
	        (IV)PL_savestack_ix));                          \
    cp = PL_savestack_ix

#define REGCP_UNWIND(cp)                                        \
    DEBUG_STATE_r(                                              \
        if (cp != PL_savestack_ix) 		                \
    	    PerlIO_printf(Perl_debug_log,		        \
		"  Clearing an EVAL scope, savestack=%"IVdf"..%"IVdf"\n", \
	        (IV)(cp), (IV)PL_savestack_ix));                \
    regcpblow(cp)

STATIC char *
S_regcppop(pTHX_ const regexp *rex)
{
    dVAR;
    U32 i;
    char *input;
    GET_RE_DEBUG_FLAGS_DECL;

    PERL_ARGS_ASSERT_REGCPPOP;

    /* Pop REGCP_OTHER_ELEMS before the parentheses loop starts. */
    i = SSPOPINT;
    assert(i == SAVEt_REGCONTEXT); /* Check that the magic cookie is there. */
    i = SSPOPINT; /* Parentheses elements to pop. */
    input = (char *) SSPOPPTR;
    *PL_reglastcloseparen = SSPOPINT;
    *PL_reglastparen = SSPOPINT;
    PL_regsize = SSPOPINT;
    PL_regoffs=(regexp_paren_pair *) SSPOPPTR;

    
    /* Now restore the parentheses context. */
    for (i -= (REGCP_OTHER_ELEMS - REGCP_FRAME_ELEMS);
	 i > 0; i -= REGCP_PAREN_ELEMS) {
	I32 tmps;
	U32 paren = (U32)SSPOPINT;
	PL_reg_start_tmp[paren] = (char *) SSPOPPTR;
	PL_regoffs[paren].start = SSPOPINT;
	tmps = SSPOPINT;
	if (paren <= *PL_reglastparen)
	    PL_regoffs[paren].end = tmps;
	DEBUG_BUFFERS_r(
	    PerlIO_printf(Perl_debug_log,
			  "     restoring \\%"UVuf" to %"IVdf"(%"IVdf")..%"IVdf"%s\n",
			  (UV)paren, (IV)PL_regoffs[paren].start,
			  (IV)(PL_reg_start_tmp[paren] - PL_bostr),
			  (IV)PL_regoffs[paren].end,
			  (paren > *PL_reglastparen ? "(no)" : ""));
	);
    }
    DEBUG_BUFFERS_r(
	if (*PL_reglastparen + 1 <= rex->nparens) {
	    PerlIO_printf(Perl_debug_log,
			  "     restoring \\%"IVdf"..\\%"IVdf" to undef\n",
			  (IV)(*PL_reglastparen + 1), (IV)rex->nparens);
	}
    );
#if 1
    /* It would seem that the similar code in regtry()
     * already takes care of this, and in fact it is in
     * a better location to since this code can #if 0-ed out
     * but the code in regtry() is needed or otherwise tests
     * requiring null fields (pat.t#187 and split.t#{13,14}
     * (as of patchlevel 7877)  will fail.  Then again,
     * this code seems to be necessary or otherwise
     * this erroneously leaves $1 defined: "1" =~ /^(?:(\d)x)?\d$/
     * --jhi updated by dapm */
    for (i = *PL_reglastparen + 1; i <= rex->nparens; i++) {
	if (i > PL_regsize)
	    PL_regoffs[i].start = -1;
	PL_regoffs[i].end = -1;
    }
#endif
    return input;
}

#define regcpblow(cp) LEAVE_SCOPE(cp)	/* Ignores regcppush()ed data. */

/*
 * pregexec and friends
 */

#ifndef PERL_IN_XSUB_RE
/*
 - pregexec - match a regexp against a string
 */
I32
Perl_pregexec(pTHX_ REGEXP * const prog, char* stringarg, register char *strend,
	 char *strbeg, I32 minend, SV *screamer, U32 nosave)
/* strend: pointer to null at end of string */
/* strbeg: real beginning of string */
/* minend: end of match must be >=minend after stringarg. */
/* nosave: For optimizations. */
{
    PERL_ARGS_ASSERT_PREGEXEC;

    return
	regexec_flags(prog, stringarg, strend, strbeg, minend, screamer, NULL,
		      nosave ? 0 : REXEC_COPY_STR);
}
#endif

/*
 * Need to implement the following flags for reg_anch:
 *
 * USE_INTUIT_NOML		- Useful to call re_intuit_start() first
 * USE_INTUIT_ML
 * INTUIT_AUTORITATIVE_NOML	- Can trust a positive answer
 * INTUIT_AUTORITATIVE_ML
 * INTUIT_ONCE_NOML		- Intuit can match in one location only.
 * INTUIT_ONCE_ML
 *
 * Another flag for this function: SECOND_TIME (so that float substrs
 * with giant delta may be not rechecked).
 */

/* Assumptions: if ANCH_GPOS, then strpos is anchored. XXXX Check GPOS logic */

/* If SCREAM, then SvPVX_const(sv) should be compatible with strpos and strend.
   Otherwise, only SvCUR(sv) is used to get strbeg. */

/* XXXX We assume that strpos is strbeg unless sv. */

/* XXXX Some places assume that there is a fixed substring.
	An update may be needed if optimizer marks as "INTUITable"
	RExen without fixed substrings.  Similarly, it is assumed that
	lengths of all the strings are no more than minlen, thus they
	cannot come from lookahead.
	(Or minlen should take into account lookahead.) 
  NOTE: Some of this comment is not correct. minlen does now take account
  of lookahead/behind. Further research is required. -- demerphq

*/

/* A failure to find a constant substring means that there is no need to make
   an expensive call to REx engine, thus we celebrate a failure.  Similarly,
   finding a substring too deep into the string means that less calls to
   regtry() should be needed.

   REx compiler's optimizer found 4 possible hints:
	a) Anchored substring;
	b) Fixed substring;
	c) Whether we are anchored (beginning-of-line or \G);
	d) First node (of those at offset 0) which may distingush positions;
   We use a)b)d) and multiline-part of c), and try to find a position in the
   string which does not contradict any of them.
 */

/* Most of decisions we do here should have been done at compile time.
   The nodes of the REx which we used for the search should have been
   deleted from the finite automaton. */

char *
Perl_re_intuit_start(pTHX_ REGEXP * const rx, SV *sv, char *strpos,
		     char *strend, const U32 flags, re_scream_pos_data *data)
{
    dVAR;
    struct regexp *const prog = (struct regexp *)SvANY(rx);
    register I32 start_shift = 0;
    /* Should be nonnegative! */
    register I32 end_shift   = 0;
    register char *s;
    register SV *check;
    char *strbeg;
    char *t;
    const bool do_utf8 = (prog->extflags & RXf_PMf_UTF8) != 0;
    I32 ml_anch;
    register char *other_last = NULL;	/* other substr checked before this */
    char *check_at = NULL;		/* check substr found at this pos */
    const I32 multiline = prog->extflags & RXf_PMf_MULTILINE;
    RXi_GET_DECL(prog,progi);
#ifdef DEBUGGING
    const char * const i_strpos = strpos;
#endif
    GET_RE_DEBUG_FLAGS_DECL;

    PERL_ARGS_ASSERT_RE_INTUIT_START;

    DEBUG_EXECUTE_r( 
        debug_start_match(rx, do_utf8, strpos, strend, 
            sv ? "Guessing start of match in sv for"
               : "Guessing start of match in string for");
	      );

    /* CHR_DIST() would be more correct here but it makes things slow. */
    if (prog->minlen > strend - strpos) {
	DEBUG_EXECUTE_r(PerlIO_printf(Perl_debug_log,
			      "String too short... [re_intuit_start]\n"));
	goto fail;
    }
                
    strbeg = (sv && SvPOK(sv)) ? strend - SvCUR(sv) : strpos;
    PL_regeol = strend;
    check = prog->check_substr;
    if (check == &PL_sv_undef) {
	DEBUG_EXECUTE_r(PerlIO_printf(Perl_debug_log,
		"Non-utf8 string cannot match utf8 check string\n"));
	goto fail;
    }
    if (prog->extflags & RXf_ANCH) {	/* Match at beg-of-str or after \n */
	ml_anch = !( (prog->extflags & RXf_ANCH_SINGLE)
		     || ( (prog->extflags & RXf_ANCH_BOL)
			  && !multiline ) );	/* Check after \n? */

	if (!ml_anch) {
	  if ( !(prog->extflags & RXf_ANCH_GPOS) /* Checked by the caller */
		&& !(prog->intflags & PREGf_IMPLICIT) /* not a real BOL */
	       /* SvCUR is not set on references: SvRV and SvPVX_const overlap */
	       && sv && !SvROK(sv)
	       && (strpos != strbeg)) {
	      DEBUG_EXECUTE_r(PerlIO_printf(Perl_debug_log, "Not at start...\n"));
	      goto fail;
	  }
	  if (prog->check_offset_min == prog->check_offset_max &&
	      !(prog->extflags & RXf_CANY_SEEN)) {
	    /* Substring at constant offset from beg-of-str... */
	    I32 slen;

	    s = reghop3(strpos, prog->check_offset_min, strend);
	    
	    if (SvTAIL(check)) {
		slen = SvCUR(check);	/* >= 1 */

		if ( strend - s > slen || strend - s < slen - 1
		     || (strend - s == slen && strend[-1] != '\n')) {
		    DEBUG_EXECUTE_r(PerlIO_printf(Perl_debug_log, "String too long...\n"));
		    goto fail_finish;
		}
		/* Now should match s[0..slen-2] */
		slen--;
		if (slen && (*SvPVX_const(check) != *s
			     || (slen > 1
				 && memNE(SvPVX_const(check), s, slen)))) {
		  report_neq:
		    DEBUG_EXECUTE_r(PerlIO_printf(Perl_debug_log, "String not equal...\n"));
		    goto fail_finish;
		}
	    }
	    else if (*SvPVX_const(check) != *s
		     || ((slen = SvCUR(check)) > 1
			 && memNE(SvPVX_const(check), s, slen)))
		goto report_neq;
	    check_at = s;
	    goto success_at_start;
	  }
	}
	/* Match is anchored, but substr is not anchored wrt beg-of-str. */
	s = strpos;
	start_shift = prog->check_offset_min; /* okay to underestimate on CC */
	end_shift = prog->check_end_shift;
	
	if (!ml_anch) {
	    const I32 end = prog->check_offset_max + SvCUR(check)
					 - (SvTAIL(check) != 0);
	    const I32 eshift = (strend - s) - end;

	    if (end_shift < eshift)
		end_shift = eshift;
	}
    }
    else {				/* Can match at random position */
	ml_anch = 0;
	s = strpos;
	start_shift = prog->check_offset_min;  /* okay to underestimate on CC */
	end_shift = prog->check_end_shift;
	
	/* end shift should be non negative here */
    }

#ifdef DEBUGGING	/* 7/99: reports of failure (with the older version) */
    if (end_shift < 0)
	Perl_croak(aTHX_ "panic: negative end_shift: %"IVdf"\n",
		   (IV)end_shift);
#endif

  restart:
    /* Find a possible match in the region s..strend by looking for
       the "check" substring in the region corrected by start/end_shift. */
    
    {
        I32 srch_start_shift = start_shift;
        I32 srch_end_shift = end_shift;
        if (srch_start_shift < 0 && strbeg - s > srch_start_shift) {
	    srch_end_shift -= ((strbeg - s) - srch_start_shift); 
	    srch_start_shift = strbeg - s;
	}
    DEBUG_OPTIMISE_MORE_r({
        PerlIO_printf(Perl_debug_log, "Check offset min: %"IVdf" Start shift: %"IVdf" End shift %"IVdf" Real End Shift: %"IVdf"\n",
            (IV)prog->check_offset_min,
            (IV)srch_start_shift,
            (IV)srch_end_shift, 
            (IV)prog->check_end_shift);
    });       
        
    if (flags & REXEC_SCREAM) {
	I32 p = -1;			/* Internal iterator of scream. */
	I32 * const pp = data ? data->scream_pos : &p;

	if (PL_screamfirst[BmRARE(check)] >= 0
	    || ( BmRARE(check) == '\n'
		 && (BmPREVIOUS(check) == SvCUR(check) - 1)
		 && SvTAIL(check) ))
	    s = screaminstr(sv, check,
			    srch_start_shift + (s - strbeg), srch_end_shift, pp, 0);
	else
	    goto fail_finish;
	/* we may be pointing at the wrong string */
	if (s && RXp_MATCH_COPIED(prog))
	    s = strbeg + (s - SvPVX_const(sv));
	if (data)
	    *data->scream_olds = s;
    }
    else {
        char* start_point;
        char* end_point;
        if (prog->extflags & RXf_CANY_SEEN) {
            start_point= (char*)(s + srch_start_shift);
            end_point= (char*)(strend - srch_end_shift);
        } else {
	    start_point= reghop4(s, srch_start_shift, strbeg, strend);
            end_point= reghop3(strend, -srch_end_shift, strbeg);
	}
	DEBUG_OPTIMISE_MORE_r({
            PerlIO_printf(Perl_debug_log, "fbm_instr len=%d str=<%.*s>\n", 
                (int)(end_point - start_point),
                (int)(end_point - start_point) > 20 ? 20 : (int)(end_point - start_point), 
                start_point);
        });

	s = fbm_instr( start_point, end_point,
		       check, multiline ? FBMrf_MULTILINE : 0);
    }
    }
    /* Update the count-of-usability, remove useless subpatterns,
	unshift s.  */

    DEBUG_EXECUTE_r({
        RE_PV_QUOTED_DECL(quoted, do_utf8, PERL_DEBUG_PAD_ZERO(0), 
            SvPVX_const(check), RE_SV_DUMPLEN(check), 30);
        PerlIO_printf(Perl_debug_log, "%s %s substr %s%s%s",
			  (s ? "Found" : "Did not find"),
	    (check == prog->anchored_substr 
	        ? "anchored" : "floating"),
	    quoted,
	    RE_SV_TAIL(check),
	    (s ? " at offset " : "...\n") ); 
    });

    if (!s)
	goto fail_finish;
    /* Finish the diagnostic message */
    DEBUG_EXECUTE_r(PerlIO_printf(Perl_debug_log, "%ld...\n", (long)(s - i_strpos)) );

    /* XXX dmq: first branch is for positive lookbehind...
       Our check string is offset from the beginning of the pattern.
       So we need to do any stclass tests offset forward from that 
       point. I think. :-(
     */
    
        
    
    check_at=s;
     

    /* Got a candidate.  Check MBOL anchoring, and the *other* substr.
       Start with the other substr.
       XXXX no SCREAM optimization yet - and a very coarse implementation
       XXXX /ttx+/ results in anchored="ttx", floating="x".  floating will
		*always* match.  Probably should be marked during compile...
       Probably it is right to do no SCREAM here...
     */

    if (prog->float_substr && prog->anchored_substr) 
    {
	/* Take into account the "other" substring. */
	/* XXXX May be hopelessly wrong for UTF... */
	if (!other_last)
	    other_last = strpos;
	if (check == prog->float_substr) {
	  do_other_anchored:
	    {
		char * const last = reghop3(s, -start_shift, strbeg);
		char *last1, *last2;
		char * const saved_s = s;
		SV* must;

		t = s - prog->check_offset_max;
		if (s - strpos > prog->check_offset_max  /* signed-corrected t > strpos */
		    && (!do_utf8
			|| ((t = reghopmaybe3(s, -(prog->check_offset_max), strpos))
			    && t > strpos)))
		    NOOP;
		else
		    t = strpos;
		t = reghop3(t, prog->anchored_offset, strend);
		if (t < other_last)	/* These positions already checked */
		    t = other_last;
		last2 = last1 = reghop3(strend, -prog->minlen, strbeg);
		if (last < last1)
		    last1 = last;
                /* XXXX It is not documented what units *_offsets are in.  
                   We assume bytes, but this is clearly wrong. 
                   Meaning this code needs to be carefully reviewed for errors.
                   dmq.
                  */
 
		/* On end-of-str: see comment below. */
		must = prog->anchored_substr;
		if (must == &PL_sv_undef) {
		    s = (char*)NULL;
		    DEBUG_r(must = prog->anchored_substr);	/* for debug */
		}
		else
		    s = fbm_instr(
			t,
			reghop3(reghop3(last1, prog->anchored_offset, strend)
				+ SvCUR(must), -(SvTAIL(must)!=0), strbeg),
			must,
			multiline ? FBMrf_MULTILINE : 0
		    );
                DEBUG_EXECUTE_r({
                    RE_PV_QUOTED_DECL(quoted, do_utf8, PERL_DEBUG_PAD_ZERO(0), 
                        SvPVX_const(must), RE_SV_DUMPLEN(must), 30);
                    PerlIO_printf(Perl_debug_log, "%s anchored substr %s%s",
			(s ? "Found" : "Contradicts"),
                        quoted, RE_SV_TAIL(must));
                });		    
		
			    
		if (!s) {
		    if (last1 >= last2) {
			DEBUG_EXECUTE_r(PerlIO_printf(Perl_debug_log,
						", giving up...\n"));
			goto fail_finish;
		    }
		    DEBUG_EXECUTE_r(PerlIO_printf(Perl_debug_log,
			", trying floating at offset %ld...\n",
			(long)(reghop3(saved_s, 1, strend) - i_strpos)));
		    other_last = reghop3(last1, prog->anchored_offset+1, strend);
		    s = reghop3(last, 1, strend);
		    goto restart;
		}
		else {
		    DEBUG_EXECUTE_r(PerlIO_printf(Perl_debug_log, " at offset %ld...\n",
			  (long)(s - i_strpos)));
		    t = reghop3(s, -prog->anchored_offset, strbeg);
		    other_last = reghop3(s, 1, strend);
		    s = saved_s;
		    if (t == strpos)
			goto try_at_start;
		    goto try_at_offset;
		}
	    }
	}
	else {		/* Take into account the floating substring. */
	    char *last, *last1;
	    char * const saved_s = s;
	    SV* must;

	    t = reghop3(s, -start_shift, strbeg);
	    last1 = last =
		reghop3(strend, -prog->minlen + prog->float_min_offset, strbeg);
	    if ((last - t) > prog->float_max_offset)
		last = reghop3(t, prog->float_max_offset, strend);
	    s = reghop3(t, prog->float_min_offset, strend);
	    if (s < other_last)
		s = other_last;
 /* XXXX It is not documented what units *_offsets are in.  Assume bytes.  */
	    must = prog->float_substr;
	    /* fbm_instr() takes into account exact value of end-of-str
	       if the check is SvTAIL(ed).  Since false positives are OK,
	       and end-of-str is not later than strend we are OK. */
	    if (must == &PL_sv_undef) {
		s = (char*)NULL;
		DEBUG_r(must = prog->float_substr);	/* for debug message */
	    }
	    else
		s = fbm_instr(s,
			      last + SvCUR(must)
				  - (SvTAIL(must)!=0),
			      must, multiline ? FBMrf_MULTILINE : 0);
	    DEBUG_EXECUTE_r({
	        RE_PV_QUOTED_DECL(quoted, do_utf8, PERL_DEBUG_PAD_ZERO(0), 
	            SvPVX_const(must), RE_SV_DUMPLEN(must), 30);
	        PerlIO_printf(Perl_debug_log, "%s floating substr %s%s",
		    (s ? "Found" : "Contradicts"),
		    quoted, RE_SV_TAIL(must));
            });
	    if (!s) {
		if (last1 == last) {
		    DEBUG_EXECUTE_r(PerlIO_printf(Perl_debug_log,
					    ", giving up...\n"));
		    goto fail_finish;
		}
		DEBUG_EXECUTE_r(PerlIO_printf(Perl_debug_log,
		    ", trying anchored starting at offset %ld...\n",
		    (long)(saved_s + 1 - i_strpos)));
		other_last = last;
		s = reghop3(t, 1, strend);
		goto restart;
	    }
	    else {
		DEBUG_EXECUTE_r(PerlIO_printf(Perl_debug_log, " at offset %ld...\n",
		      (long)(s - i_strpos)));
		other_last = s; /* Fix this later. --Hugo */
		s = saved_s;
		if (t == strpos)
		    goto try_at_start;
		goto try_at_offset;
	    }
	}
    }

    
    t= reghop4( s, -prog->check_offset_max, strpos, strend);
        
    DEBUG_OPTIMISE_MORE_r(
        PerlIO_printf(Perl_debug_log, 
            "Check offset min:%"IVdf" max:%"IVdf" S:%"IVdf" t:%"IVdf" D:%"IVdf" end:%"IVdf"\n",
            (IV)prog->check_offset_min,
            (IV)prog->check_offset_max,
            (IV)(s-strpos),
            (IV)(t-strpos),
            (IV)(t-s),
            (IV)(strend-strpos)
        )
    );

    if (s - strpos > prog->check_offset_max)  /* signed-corrected t > strpos */
    {
	/* Fixed substring is found far enough so that the match
	   cannot start at strpos. */
      try_at_offset:
	if (ml_anch && t[-1] != '\n') {
	    /* Eventually fbm_*() should handle this, but often
	       anchored_offset is not 0, so this check will not be wasted. */
	    /* XXXX In the code below we prefer to look for "^" even in
	       presence of anchored substrings.  And we search even
	       beyond the found float position.  These pessimizations
	       are historical artefacts only.  */
	  find_anchor:
	    while (t < strend - prog->minlen) {
		if (*t == '\n') {
		    if (t < check_at - prog->check_offset_min) {
			if (prog->anchored_substr) {
			    /* Since we moved from the found position,
			       we definitely contradict the found anchored
			       substr.  Due to the above check we do not
			       contradict "check" substr.
			       Thus we can arrive here only if check substr
			       is float.  Redo checking for "other"=="fixed".
			     */
			    strpos = t + 1;			
			    DEBUG_EXECUTE_r(PerlIO_printf(Perl_debug_log, "Found /%s^%s/m at offset %ld, rescanning for anchored from offset %ld...\n",
				PL_colors[0], PL_colors[1], (long)(strpos - i_strpos), (long)(strpos - i_strpos + prog->anchored_offset)));
			    goto do_other_anchored;
			}
			/* We don't contradict the found floating substring. */
			/* XXXX Why not check for STCLASS? */
			s = t + 1;
			DEBUG_EXECUTE_r(PerlIO_printf(Perl_debug_log, "Found /%s^%s/m at offset %ld...\n",
			    PL_colors[0], PL_colors[1], (long)(s - i_strpos)));
			goto set_useful;
		    }
		    /* Position contradicts check-string */
		    /* XXXX probably better to look for check-string
		       than for "\n", so one should lower the limit for t? */
		    DEBUG_EXECUTE_r(PerlIO_printf(Perl_debug_log, "Found /%s^%s/m, restarting lookup for check-string at offset %ld...\n",
			PL_colors[0], PL_colors[1], (long)(t + 1 - i_strpos)));
		    other_last = strpos = s = t + 1;
		    goto restart;
		}
		t++;
	    }
	    DEBUG_EXECUTE_r(PerlIO_printf(Perl_debug_log, "Did not find /%s^%s/m...\n",
			PL_colors[0], PL_colors[1]));
	    goto fail_finish;
	}
	else {
	    DEBUG_EXECUTE_r(PerlIO_printf(Perl_debug_log, "Starting position does not contradict /%s^%s/m...\n",
			PL_colors[0], PL_colors[1]));
	}
	s = t;
      set_useful:
	++BmUSEFUL(prog->check_substr);	/* hooray/5 */
    }
    else {
	/* The found string does not prohibit matching at strpos,
	   - no optimization of calling REx engine can be performed,
	   unless it was an MBOL and we are not after MBOL,
	   or a future STCLASS check will fail this. */
      try_at_start:
	/* Even in this situation we may use MBOL flag if strpos is offset
	   wrt the start of the string. */
	if (ml_anch && sv && !SvROK(sv)	/* See prev comment on SvROK */
	    && (strpos != strbeg) && strpos[-1] != '\n'
	    /* May be due to an implicit anchor of m{.*foo}  */
	    && !(prog->intflags & PREGf_IMPLICIT))
	{
	    t = strpos;
	    goto find_anchor;
	}
	DEBUG_EXECUTE_r( if (ml_anch)
	    PerlIO_printf(Perl_debug_log, "Position at offset %ld does not contradict /%s^%s/m...\n",
			  (long)(strpos - i_strpos), PL_colors[0], PL_colors[1]);
	);
      success_at_start:
	if (!(prog->intflags & PREGf_NAUGHTY)	/* XXXX If strpos moved? */
	    && (
		prog->check_substr		/* Could be deleted already */
		&& --BmUSEFUL(prog->check_substr) < 0
		&& (prog->check_substr == prog->float_substr)
	    ))
	{
	    /* If flags & SOMETHING - do not do it many times on the same match */
	    DEBUG_EXECUTE_r(PerlIO_printf(Perl_debug_log, "... Disabling check substring...\n"));
	    SvREFCNT_dec(prog->check_substr);
	    prog->check_substr = NULL;	/* disable */
	    prog->float_substr = NULL;	/* clear */
	    check = NULL;			/* abort */
	    s = strpos;
	    /* XXXX This is a remnant of the old implementation.  It
	            looks wasteful, since now INTUIT can use many
	            other heuristics. */
	    prog->extflags &= ~RXf_USE_INTUIT;
	}
	else
	    s = strpos;
    }

    /* Last resort... */
    /* XXXX BmUSEFUL already changed, maybe multiple change is meaningful... */
    /* trie stclasses are too expensive to use here, we are better off to
       leave it to regmatch itself */
    if (progi->regstclass && PL_regkind[OP(progi->regstclass)]!=TRIE) {
	/* minlen == 0 is possible if regstclass is \b or \B,
	   and the fixed substr is ''$.
	   Since minlen is already taken into account, s+1 is before strend;
	   accidentally, minlen >= 1 guaranties no false positives at s + 1
	   even for \b or \B.  But (minlen? 1 : 0) below assumes that
	   regstclass does not come from lookahead...  */
	/* If regstclass takes bytelength more than 1: If charlength==1, OK.
	   This leaves EXACTF only, which is dealt with in find_byclass().  */
        const char* const str = STRING(progi->regstclass);
        const int cl_l = (PL_regkind[OP(progi->regstclass)] == EXACT
		    ? CHR_DIST(str+STR_LEN(progi->regstclass), str)
		    :1);
	char * endpos;
	if (prog->anchored_substr || ml_anch)
            endpos= reghop3(s, (prog->minlen ? cl_l : 0), strend);
        else if (prog->float_substr)
	    endpos= reghop4(check_at, -start_shift + cl_l, strbeg, strend);
        else 
            endpos= strend;
		    
        DEBUG_EXECUTE_r(PerlIO_printf(Perl_debug_log, "start_shift: %"IVdf" check_at: %"IVdf" s: %"IVdf" endpos: %"IVdf" strend: %"IVdf"\n",
				      (IV)start_shift, (IV)(check_at - strbeg), (IV)(s - strbeg), (IV)(endpos - strbeg), (IV)(strend - strbeg)));
	
	t = s;
        s = find_byclass(prog, progi->regstclass, s, endpos, NULL);
	if (!s) {
#ifdef DEBUGGING
	    const char *what = NULL;
#endif
	    if (endpos == strend) {
		DEBUG_EXECUTE_r( PerlIO_printf(Perl_debug_log,
				"Could not match STCLASS...\n") );
		goto fail;
	    }
	    DEBUG_EXECUTE_r( PerlIO_printf(Perl_debug_log,
				   "This position contradicts STCLASS...\n") );
	    if ((prog->extflags & RXf_ANCH) && !ml_anch)
		goto fail;
	    /* Contradict one of substrings */
	    if (prog->anchored_substr) {
		if (prog->anchored_substr == check) {
		    DEBUG_EXECUTE_r( what = "anchored" );
		  hop_and_restart:
		    s = reghop3(t, 1, strend);
		    if (s + start_shift + end_shift > strend) {
			/* XXXX Should be taken into account earlier? */
			DEBUG_EXECUTE_r( PerlIO_printf(Perl_debug_log,
					       "Could not match STCLASS...\n") );
			goto fail;
		    }
		    if (!check)
			goto giveup;
		    DEBUG_EXECUTE_r( PerlIO_printf(Perl_debug_log,
				"Looking for %s substr starting at offset %ld...\n",
				 what, (long)(s + start_shift - i_strpos)) );
		    goto restart;
		}
		/* Have both, check_string is floating */
		if (t + start_shift >= check_at) /* Contradicts floating=check */
		    goto retry_floating_check;
		/* Recheck anchored substring, but not floating... */
		s = check_at;
		if (!check)
		    goto giveup;
		DEBUG_EXECUTE_r( PerlIO_printf(Perl_debug_log,
			  "Looking for anchored substr starting at offset %ld...\n",
			  (long)(other_last - i_strpos)) );
		goto do_other_anchored;
	    }
	    /* Another way we could have checked stclass at the
               current position only: */
	    if (ml_anch) {
		s = t = t + 1;
		if (!check)
		    goto giveup;
		DEBUG_EXECUTE_r( PerlIO_printf(Perl_debug_log,
			  "Looking for /%s^%s/m starting at offset %ld...\n",
			  PL_colors[0], PL_colors[1], (long)(t - i_strpos)) );
		goto try_at_offset;
	    }
	    if (!(prog->float_substr))	/* Could have been deleted */
		goto fail;
	    /* Check is floating subtring. */
	  retry_floating_check:
	    t = check_at - start_shift;
	    DEBUG_EXECUTE_r( what = "floating" );
	    goto hop_and_restart;
	}
	if (t != s) {
            DEBUG_EXECUTE_r(PerlIO_printf(Perl_debug_log,
			"By STCLASS: moving %ld --> %ld\n",
                                  (long)(t - i_strpos), (long)(s - i_strpos))
                   );
        }
        else {
            DEBUG_EXECUTE_r(PerlIO_printf(Perl_debug_log,
                                  "Does not contradict STCLASS...\n"); 
                   );
        }
    }
  giveup:
    DEBUG_EXECUTE_r(PerlIO_printf(Perl_debug_log, "%s%s:%s match at offset %ld\n",
			  PL_colors[4], (check ? "Guessed" : "Giving up"),
			  PL_colors[5], (long)(s - i_strpos)) );
    return s;

  fail_finish:				/* Substring not found */
    if (prog->check_substr)		/* could be removed already */
	BmUSEFUL(prog->check_substr) += 5; /* hooray */
  fail:
    DEBUG_EXECUTE_r(PerlIO_printf(Perl_debug_log, "%sMatch rejected by optimizer%s\n",
			  PL_colors[4], PL_colors[5]));
    return NULL;
}

#define DECL_TRIE_TYPE(scan) \
    const enum { trie_plain, trie_utf8, trie_utf8_fold, trie_latin_utf8_fold } \
		    trie_type = (scan->flags != EXACT) \
		              ? (do_utf8 ? trie_utf8_fold : (UTF ? trie_latin_utf8_fold : trie_plain)) \
                              : (do_utf8 ? trie_utf8 : trie_plain)

#define REXEC_FBC_UTF8_SCAN(CoDe)                     \
STMT_START {                                          \
    while (s + (uskip = UTF8SKIP(s)) <= strend) {     \
	CoDe                                          \
	s += uskip;                                   \
    }                                                 \
} STMT_END

#define REXEC_FBC_SCAN(CoDe)                          \
STMT_START {                                          \
    while (s < strend) {                              \
	CoDe                                          \
	s++;                                          \
    }                                                 \
} STMT_END

#define REXEC_FBC_UTF8_CLASS_SCAN(CoNd)               \
REXEC_FBC_UTF8_SCAN(                                  \
    if (CoNd) {                                       \
	if (tmp && (!reginfo || regtry(reginfo, &s)))  \
	    goto got_it;                              \
	else                                          \
	    tmp = doevery;                            \
    }                                                 \
    else                                              \
	tmp = 1;                                      \
)

#define REXEC_FBC_CLASS_SCAN(CoNd)                    \
REXEC_FBC_SCAN(                                       \
    if (CoNd) {                                       \
	if (tmp && (!reginfo || regtry(reginfo, &s)))  \
	    goto got_it;                              \
	else                                          \
	    tmp = doevery;                            \
    }                                                 \
    else                                              \
	tmp = 1;                                      \
)

#define REXEC_FBC_TRYIT               \
if ((!reginfo || regtry(reginfo, &s))) \
    goto got_it

#define REXEC_FBC_CSCAN(CoNdUtF8,CoNd)                         \
    if (do_utf8) {                                             \
	REXEC_FBC_UTF8_CLASS_SCAN(CoNdUtF8);                   \
    }                                                          \
    else {                                                     \
	REXEC_FBC_CLASS_SCAN(CoNd);                            \
    }                                                          \
    break
    
#define REXEC_FBC_CSCAN_PRELOAD(UtFpReLoAd,CoNdUtF8,CoNd)      \
    if (do_utf8) {                                             \
	UtFpReLoAd;                                            \
	REXEC_FBC_UTF8_CLASS_SCAN(CoNdUtF8);                   \
    }                                                          \
    else {                                                     \
	REXEC_FBC_CLASS_SCAN(CoNd);                            \
    }                                                          \
    break

#define REXEC_FBC_CSCAN_TAINT(CoNdUtF8,CoNd)                   \
    if (do_utf8) {                                             \
	REXEC_FBC_UTF8_CLASS_SCAN(CoNdUtF8);                   \
    }                                                          \
    else {                                                     \
	REXEC_FBC_CLASS_SCAN(CoNd);                            \
    }                                                          \
    break

#define DUMP_EXEC_POS(li,s,doutf8) \
    dump_exec_pos(li,s,(PL_regeol),(PL_bostr),(PL_reg_starttry),doutf8)

/* We know what class REx starts with.  Try to find this position... */
/* if reginfo is NULL, its a dryrun */
/* annoyingly all the vars in this routine have different names from their counterparts
   in regmatch. /grrr */

STATIC char *
S_find_byclass(pTHX_ regexp * prog, const regnode *c, char *s, 
    const char *strend, regmatch_info *reginfo)
{
	dVAR;
	const I32 doevery = (prog->intflags & PREGf_SKIP) == 0;
	register STRLEN uskip;
	register I32 tmp = 1;	/* Scratch variable? */
	register const bool do_utf8 = (prog->extflags & RXf_PMf_UTF8) != 0;
        RXi_GET_DECL(prog,progi);

	GET_RE_DEBUG_FLAGS_DECL;

	PERL_ARGS_ASSERT_FIND_BYCLASS;
        
	DEBUG_EXECUTE_r( {
            RE_PV_QUOTED_DECL(quoted, do_utf8, PERL_DEBUG_PAD_ZERO(0), 
                s, strend -s + 1, 30);
	    PerlIO_printf( Perl_debug_log,
					" find by class. class: %s, %s\n", PL_reg_name[OP(c)], quoted); } );
	/* We know what class it must start with. */
	switch (OP(c)) {
	case ANYOFU:
	    REXEC_FBC_UTF8_CLASS_SCAN(!UTF8_IS_INVARIANT(s[0]) ?
				      reginclass(prog, c, s, 0) :
				      REGINCLASS(prog, c, s));
	    break;
	case ANYOF:
	    while (s < strend) {
		STRLEN skip = 1;

		if (REGINCLASS(prog, c, s) ||
		    (ANYOF_FOLD_SHARP_S(c, s, strend) &&
		     /* The assignment of 2 is intentional:
		      * for the folded sharp s, the skip is 2. */
		     (skip = SHARP_S_SKIP))) {
		    if (tmp && (!reginfo || regtry(reginfo, &s)))
			goto got_it;
		    else
			tmp = doevery;
		}
		else 
		    tmp = 1;
		s += skip;
	    }
	    break;
	case CANY:
	    REXEC_FBC_SCAN(
	        if (tmp && (!reginfo || regtry(reginfo, &s)))
		    goto got_it;
		else
		    tmp = doevery;
	    );
	    break;
	case BOUNDL:
	    /* FALL THROUGH */
	case BOUND:
	    if (do_utf8) {
		if (s == PL_bostr)
		    tmp = '\n';
		else {
		    char * const r = reghop3c(s, -1, PL_bostr);
		    tmp = utf8n_to_uvchr(r, UTF8SKIP(r), 0, UTF8_ALLOW_DEFAULT | UTF8_CHECK_ONLY);
		}
		tmp = ((OP(c) == BOUND ?
			isALNUM_uni(tmp) : isALNUM_LC_uvchr(UNI_TO_NATIVE(tmp))) != 0);
		LOAD_UTF8_CHARCLASS_ALNUM();
		REXEC_FBC_UTF8_SCAN(
		    if (tmp == !(OP(c) == BOUND ?
				 (bool)swash_fetch(PL_utf8_alnum, s, do_utf8) :
				 isALNUM_LC_utf8(s)))
		    {
			tmp = !tmp;
			REXEC_FBC_TRYIT;
		}
		);
	    }
	    else {
		tmp = (s != PL_bostr) ? UCHARAT(s - 1) : '\n';
		tmp = ((OP(c) == BOUND ? isALNUM(tmp) : isALNUM_LC(tmp)) != 0);
		REXEC_FBC_SCAN(
		    if (tmp ==
			!(OP(c) == BOUND ? isALNUM(*s) : isALNUM_LC(*s))) {
			tmp = !tmp;
			REXEC_FBC_TRYIT;
		}
		);
	    }
	    if ((!prog->minlen && tmp) && (!reginfo || regtry(reginfo, &s)))
		goto got_it;
	    break;
	case NBOUNDL:
	    /* FALL THROUGH */
	case NBOUND:
	    if (do_utf8) {
		if (s == PL_bostr)
		    tmp = '\n';
		else {
		    char * const r = reghop3c(s, -1, PL_bostr);
		    tmp = utf8n_to_uvchr(r, UTF8SKIP(r), 0, UTF8_ALLOW_DEFAULT | UTF8_CHECK_ONLY);
		}
		tmp = ((OP(c) == NBOUND ?
			isALNUM_uni(tmp) : isALNUM_LC_uvchr(UNI_TO_NATIVE(tmp))) != 0);
		LOAD_UTF8_CHARCLASS_ALNUM();
		REXEC_FBC_UTF8_SCAN(
		    if (tmp == !(OP(c) == NBOUND ?
				 (bool)swash_fetch(PL_utf8_alnum, s, do_utf8) :
				 isALNUM_LC_utf8(s)))
			tmp = !tmp;
		    else REXEC_FBC_TRYIT;
		);
	    }
	    else {
		tmp = (s != PL_bostr) ? UCHARAT(s - 1) : '\n';
		tmp = ((OP(c) == NBOUND ?
			isALNUM(tmp) : isALNUM_LC(tmp)) != 0);
		REXEC_FBC_SCAN(
		    if (tmp ==
			!(OP(c) == NBOUND ? isALNUM(*s) : isALNUM_LC(*s)))
			tmp = !tmp;
		    else REXEC_FBC_TRYIT;
		);
	    }
	    if ((!prog->minlen && !tmp) && (!reginfo || regtry(reginfo, &s)))
		goto got_it;
	    break;
	case LNBREAK:
	    Perl_croak(aTHX_ "foobar");
	    REXEC_FBC_CSCAN(
		is_LNBREAK_utf8(s),
		is_LNBREAK_latin1(s)
	    );
	case AHOCORASICKC:
	case AHOCORASICK: 
	    {
                /* what trie are we using right now */
        	reg_ac_data *aho
        	    = (reg_ac_data*)progi->data->data[ ARG( c ) ];
        	reg_trie_data *trie
		    = (reg_trie_data*)progi->data->data[ aho->trie ];

		const char *last_start = strend - trie->minlen;
#ifdef DEBUGGING
		const char *real_start = s;
#endif
		STRLEN maxlen = trie->maxlen;
		SV *sv_points;
		char **points; /* map of where we were in the input string
		                when reading a given char. For ASCII this
		                is unnecessary overhead as the relationship
		                is always 1:1, but for Unicode, especially
		                case folded Unicode this is not true. */
		char *bitmap=NULL;


                GET_RE_DEBUG_FLAGS_DECL;

                /* We can't just allocate points here. We need to wrap it in
                 * an SV so it gets freed properly if there is a croak while
                 * running the match */
                ENTER;
	        SAVETMPS;
                sv_points=newSV(maxlen * sizeof(char *));
                SvCUR_set(sv_points,
                    maxlen * sizeof(char *));
                SvPOK_on(sv_points);
                sv_2mortal(sv_points);
                points=(char**)SvPV_nolen(sv_points );
                if ( (trie->bitmap || OP(c)==AHOCORASICKC) ) 
                {
                    if (trie->bitmap) 
                        bitmap=trie->bitmap;
                    else
                        bitmap=ANYOF_BITMAP(c);
                }
                /* this is the Aho-Corasick algorithm modified a touch
                   to include special handling for long "unknown char" 
                   sequences. The basic idea being that we use AC as long
                   as we are dealing with a possible matching char, when
                   we encounter an unknown char (and we have not encountered
                   an accepting state) we scan forward until we find a legal 
                   starting char. 
                   AC matching is basically that of trie matching, except
                   that when we encounter a failing transition, we fall back
                   to the current states "fail state", and try the current char 
                   again, a process we repeat until we reach the root state, 
                   state 1, or a legal transition. If we fail on the root state 
                   then we can either terminate if we have reached an accepting 
                   state previously, or restart the entire process from the beginning 
                   if we have not.

                 */
                while (s <= last_start) {
                    char *uc = s;
                    U16 charid = 0;
                    U32 base = 1;
                    U32 state = 1;
                    U8 uvc = 0;
                    STRLEN len = 0;
                    char *leftmost = NULL;
#ifdef DEBUGGING                    
                    U32 accepted_word= 0;
#endif
                    U32 pointpos = 0;

                    while ( state && uc <= strend ) {
                        int failed=0;
                        U32 word = aho->states[ state ].wordnum;

                        if( state==1 ) {
                            if ( bitmap ) {
                                DEBUG_TRIE_EXECUTE_r(
                                    if ( uc <= last_start && !BITMAP_TEST(bitmap,*uc) ) {
                                        dump_exec_pos( uc, c, strend, real_start, 
                                            uc, do_utf8 );
                                        PerlIO_printf( Perl_debug_log,
                                            " Scanning for legal start char...\n");
                                    }
                                );            
                                while ( uc <= last_start  && !BITMAP_TEST(bitmap,*uc) ) {
                                    uc++;
                                }
                                s= uc;
                            }
                            if (uc >last_start) break;
                        }
                                            
                        if ( word ) {
                            char *lpos= points[ (pointpos - trie->wordlen[word-1] ) % maxlen ];
                            if (!leftmost || lpos < leftmost) {
                                DEBUG_r(accepted_word=word);
                                leftmost= lpos;
                            }
                            if (base==0) break;
                            
                        }
                        points[pointpos++ % maxlen]= uc;

			uvc = (U8)*uc;
			charid = trie->charmap[ uvc ];
			len = 1;

                        DEBUG_TRIE_EXECUTE_r({
                            dump_exec_pos( (char *)uc, c, strend, real_start, 
                                s,   do_utf8 );
                            PerlIO_printf(Perl_debug_log,
                                " Charid:%3u CP:%x ",
                                 charid, uvc);
                        });

                        do {
#ifdef DEBUGGING
                            word = aho->states[ state ].wordnum;
#endif
                            base = aho->states[ state ].trans.base;

                            DEBUG_TRIE_EXECUTE_r({
                                if (failed) 
                                    dump_exec_pos( (char *)uc, c, strend, real_start, 
                                        s,   do_utf8 );
                                PerlIO_printf( Perl_debug_log,
                                    "%sState: %4"UVxf", word=%"UVxf,
                                    failed ? " Fail transition to " : "",
                                    (UV)state, (UV)word);
                            });
                            if ( base ) {
                                U32 tmp;
                                if (charid &&
                                     (base + charid > trie->uniquecharcount )
                                     && (base + charid - 1 - trie->uniquecharcount
                                            < trie->lasttrans)
                                     && trie->trans[base + charid - 1 -
                                            trie->uniquecharcount].check == state
                                     && (tmp=trie->trans[base + charid - 1 -
                                        trie->uniquecharcount ].next))
                                {
                                    DEBUG_TRIE_EXECUTE_r(
                                        PerlIO_printf( Perl_debug_log," - legal\n"));
                                    state = tmp;
                                    break;
                                }
                                else {
                                    DEBUG_TRIE_EXECUTE_r(
                                        PerlIO_printf( Perl_debug_log," - fail\n"));
                                    failed = 1;
                                    state = aho->fail[state];
                                }
                            }
                            else {
                                /* we must be accepting here */
                                DEBUG_TRIE_EXECUTE_r(
                                        PerlIO_printf( Perl_debug_log," - accepting\n"));
                                failed = 1;
                                break;
                            }
                        } while(state);
                        uc += len;
                        if (failed) {
                            if (leftmost)
                                break;
                            if (!state) state = 1;
                        }
                    }
                    if ( aho->states[ state ].wordnum ) {
                        char *lpos = points[ (pointpos - trie->wordlen[aho->states[ state ].wordnum-1]) % maxlen ];
                        if (!leftmost || lpos < leftmost) {
                            DEBUG_r(accepted_word=aho->states[ state ].wordnum);
                            leftmost = lpos;
                        }
                    }
                    if (leftmost) {
                        s = (char*)leftmost;
                        DEBUG_TRIE_EXECUTE_r({
                            PerlIO_printf( 
                                Perl_debug_log,"Matches word #%"UVxf" at position %"IVdf". Trying full pattern...\n",
                                (UV)accepted_word, (IV)(s - real_start)
                            );
                        });
                        if (!reginfo || regtry(reginfo, &s)) {
                            FREETMPS;
		            LEAVE;
                            goto got_it;
                        }
                        s = HOP(s,1);
                        DEBUG_TRIE_EXECUTE_r({
                            PerlIO_printf( Perl_debug_log,"Pattern failed. Looking for new start point...\n");
                        });
                    } else {
                        DEBUG_TRIE_EXECUTE_r(
                            PerlIO_printf( Perl_debug_log,"No match.\n"));
                        break;
                    }
                }
                FREETMPS;
                LEAVE;
	    }
	    break;
	default:
	    Perl_croak(aTHX_ "panic: unknown regstclass %d", (int)OP(c));
	    break;
	}
	return 0;
      got_it:
	return s;
}

static void 
S_swap_match_buff (pTHX_ regexp *prog)
{
    regexp_paren_pair *t;

    PERL_ARGS_ASSERT_SWAP_MATCH_BUFF;

    if (!prog->swap) {
    /* We have to be careful. If the previous successful match
       was from this regex we don't want a subsequent paritally
       successful match to clobber the old results. 
       So when we detect this possibility we add a swap buffer
       to the re, and switch the buffer each match. If we fail
       we switch it back, otherwise we leave it swapped.
    */
        Newxz(prog->swap, (prog->nparens + 1), regexp_paren_pair);
    }
    t = prog->swap;
    prog->swap = prog->offs;
    prog->offs = t;
}    


/*
 - regexec_flags - match a regexp against a string
 */
I32
Perl_regexec_flags(pTHX_ REGEXP * const rx, char *stringarg, register char *strend,
	      char *strbeg, I32 minend, SV *sv, void *data, U32 flags)
/* strend: pointer to null at end of string */
/* strbeg: real beginning of string */
/* minend: end of match must be >=minend after stringarg. */
/* data: May be used for some additional optimizations. 
         Currently its only used, with a U32 cast, for transmitting 
         the ganch offset when doing a /g match. This will change */
/* nosave: For optimizations. */
{
    dVAR;
    struct regexp *const prog = (struct regexp *)SvANY(rx);
    /*register*/ char *s;
    register regnode *c;
    /*register*/ char *startpos = stringarg;
    I32 minlen;		/* must match at least this many chars */
    I32 dontbother = 0;	/* how many characters not to try at end */
    I32 end_shift = 0;			/* Same for the end. */		/* CC */
    I32 scream_pos = -1;		/* Internal iterator of scream. */
    char *scream_olds = NULL;
    const bool do_utf8 = (prog->extflags & RXf_PMf_UTF8) != 0;
    I32 multiline;
    RXi_GET_DECL(prog,progi);
    regmatch_info reginfo;  /* create some info to pass to regtry etc */
    bool swap_on_fail = 0;
    GET_RE_DEBUG_FLAGS_DECL;

    PERL_ARGS_ASSERT_REGEXEC_FLAGS;
    PERL_UNUSED_ARG(data);

    /* Be paranoid... */
    if (prog == NULL || startpos == NULL) {
	Perl_croak(aTHX_ "NULL regexp parameter");
	return 0;
    }

    multiline = prog->extflags & RXf_PMf_MULTILINE;
    reginfo.prog = rx;	 /* Yes, sorry that this is confusing.  */

    DEBUG_EXECUTE_r( 
        debug_start_match(rx, do_utf8, startpos, strend, 
        "Matching");
    );

    minlen = prog->minlen;
    
    if (strend - startpos < (minlen+(prog->check_offset_min<0?prog->check_offset_min:0))) {
        DEBUG_EXECUTE_r(PerlIO_printf(Perl_debug_log,
			      "String too short [regexec_flags]...\n"));
	goto phooey;
    }

    
    /* Check validity of program. */
    if ((*(U8*)progi->program) != REG_MAGIC) {
	Perl_croak(aTHX_ "corrupted regexp program");
    }

    PL_reg_flags = 0;
    PL_reg_eval_set = 0;
    PL_reg_maxiter = 0;

    /* Mark beginning of line for ^ and lookbehind. */
    reginfo.bol = startpos; /* XXX not used ??? */
    PL_bostr  = strbeg;
    reginfo.sv = sv;

    /* Mark end of line for $ (and such) */
    PL_regeol = strend;

    /* see how far we have to get to not match where we matched before */
    reginfo.till = startpos+minend;

    /* If there is a "must appear" string, look for it. */
    s = startpos;

    if (prog->extflags & RXf_GPOS_SEEN) { /* Need to set reginfo->ganch */
	MAGIC *mg;

	if (flags & REXEC_IGNOREPOS)	/* Means: check only at start */
	    reginfo.ganch = startpos + prog->gofs;
	else if (sv && SvTYPE(sv) >= SVt_PVMG
		  && SvMAGIC(sv)
		  && (mg = mg_find(sv, PERL_MAGIC_regex_global))
		  && mg->mg_len >= 0) {
	    reginfo.ganch = strbeg + mg->mg_len;	/* Defined pos() */
	    if (prog->extflags & RXf_ANCH_GPOS) {
	        if (s > reginfo.ganch)
		    goto phooey;
		s = reginfo.ganch - prog->gofs;
	    }
	}
	else if (data) {
	    reginfo.ganch = strbeg + PTR2UV(data);
	} else				/* pos() not defined */
	    reginfo.ganch = strbeg;
    }
    if (PL_curpm && (PM_GETRE(PL_curpm) == rx)) {
        swap_on_fail = 1;
        swap_match_buff(prog); /* do we need a save destructor here for
                                  eval dies? */
    }
    if (!(flags & REXEC_CHECKED) && (prog->check_substr != NULL)) {
	re_scream_pos_data d;

	d.scream_olds = &scream_olds;
	d.scream_pos = &scream_pos;
	s = re_intuit_start(rx, sv, s, strend, flags, &d);
	if (!s) {
	    DEBUG_EXECUTE_r(PerlIO_printf(Perl_debug_log, "Not present...\n"));
	    goto phooey;	/* not present */
	}
    }



    /* Simplest case:  anchored match need be tried only once. */
    /*  [unless only anchor is BOL and multiline is set] */
    if (prog->extflags & (RXf_ANCH & ~RXf_ANCH_GPOS)) {
	if (s == startpos && regtry(&reginfo, &startpos))
	    goto got_it;
	else if (multiline || (prog->intflags & PREGf_IMPLICIT)
		 || (prog->extflags & RXf_ANCH_MBOL)) /* XXXX SBOL? */
	{
	    char *end;

	    if (minlen)
		dontbother = minlen - 1;
	    end = reghop3(strend, -dontbother, strbeg) - 1;
	    /* for multiline we only have to try after newlines */
	    if (prog->check_substr) {
		if (s == startpos)
		    goto after_try;
		while (1) {
		    if (regtry(&reginfo, &s))
			goto got_it;
		  after_try:
		    if (s > end)
			goto phooey;
		    if (prog->extflags & RXf_USE_INTUIT) {
			s = re_intuit_start(rx, sv, s + 1, strend, flags, NULL);
			if (!s)
			    goto phooey;
		    }
		    else
			s++;
		}		
	    } else {
		if (s > startpos)
		    s--;
		while (s < end) {
		    if (*s++ == '\n') {	/* don't need PL_utf8skip here */
			if (regtry(&reginfo, &s))
			    goto got_it;
		    }
		}		
	    }
	}
	goto phooey;
    } else if (RXf_GPOS_CHECK == (prog->extflags & RXf_GPOS_CHECK)) 
    {
        /* the warning about reginfo.ganch being used without intialization
           is bogus -- we set it above, when prog->extflags & RXf_GPOS_SEEN 
           and we only enter this block when the same bit is set. */
        char *tmp_s = reginfo.ganch - prog->gofs;
	if (regtry(&reginfo, &tmp_s))
	    goto got_it;
	goto phooey;
    }

    /* Messy cases:  unanchored match. */
    if ((prog->anchored_substr) && prog->intflags & PREGf_SKIP) {
	/* we have /x+whatever/ */
	/* it must be a one character string (XXXX Except UTF?) */
	char ch;
#ifdef DEBUGGING
	int did_match = 0;
#endif
	ch = SvPVX_const(prog->anchored_substr)[0];

	if (do_utf8) {
	    REXEC_FBC_SCAN(
		if (*s == ch) {
		    DEBUG_EXECUTE_r( did_match = 1 );
		    if (regtry(&reginfo, &s)) goto got_it;
		    s += UTF8SKIP(s);
		    while (s < strend && *s == ch)
			s += UTF8SKIP(s);
		}
	    );
	}
	else {
	    REXEC_FBC_SCAN(
		if (*s == ch) {
		    DEBUG_EXECUTE_r( did_match = 1 );
		    if (regtry(&reginfo, &s)) goto got_it;
		    s++;
		    while (s < strend && *s == ch)
			s++;
		}
	    );
	}
	DEBUG_EXECUTE_r(if (!did_match)
		PerlIO_printf(Perl_debug_log,
                                  "Did not find anchored character...\n")
               );
    }
    else if (prog->anchored_substr != NULL
	      || ((prog->float_substr != NULL)
		  && prog->float_max_offset < strend - s)) {
	SV *must;
	I32 back_max;
	I32 back_min;
	char *last;
	char *last1;		/* Last position checked before */
#ifdef DEBUGGING
	int did_match = 0;
#endif
	if (prog->anchored_substr) {
	    must = prog->anchored_substr;
	    back_max = back_min = prog->anchored_offset;
	} else {
	    must = prog->float_substr;
	    back_max = prog->float_max_offset;
	    back_min = prog->float_min_offset;
	}
	
	if (must == &PL_sv_undef)
	    /* could not downgrade utf8 check substring, so must fail */
	    goto phooey;

        if (back_min<0) {
	    last = strend;
	} else {
            last = reghop3(strend,	/* Cannot start after this */
        	  -(I32)(SvCUR(must)
        		 - (SvTAIL(must) != 0) + back_min), strbeg);
        }
	if (s > PL_bostr)
	    last1 = HOP(s, -1);
	else
	    last1 = s - 1;	/* bogus */

	/* XXXX check_substr already used to find "s", can optimize if
	   check_substr==must. */
	scream_pos = -1;
	dontbother = end_shift;
	strend = HOP(strend, -dontbother);
	DEBUG_EXECUTE_r(PerlIO_printf(Perl_debug_log, "foobar '%p' '%p' %p\n", s, strend,last));
	while ( (s <= last) &&
		((flags & REXEC_SCREAM)
		 ? (s = screaminstr(sv, must, reghop3(s, back_min, (back_min<0 ? strbeg : strend)) - strbeg,
				    end_shift, &scream_pos, 0))
		 : (s = fbm_instr(reghop3(s, back_min, (back_min<0 ? strbeg : strend)),
				  strend, must,
				  multiline ? FBMrf_MULTILINE : 0))) ) {
	    /* we may be pointing at the wrong string */
	    if ((flags & REXEC_SCREAM) && RXp_MATCH_COPIED(prog))
		s = strbeg + (s - SvPVX_const(sv));
	    DEBUG_EXECUTE_r( did_match = 1 );
	    if (HOPc(s, -back_max) > last1) {
		last1 = HOP(s, -back_min);
		s = HOP(s, -back_max);
	    }
	    else {
		char * const t = (last1 >= PL_bostr) ? HOPc(last1, 1) : last1 + 1;

		last1 = HOPc(s, -back_min);
		s = t;
	    }
	    if (do_utf8) {
		while (s <= last1) {
		    if (regtry(&reginfo, &s))
			goto got_it;
		    s += UTF8SKIP(s);
		}
	    }
	    else {
		while (s <= last1) {
		    if (regtry(&reginfo, &s))
			goto got_it;
		    s++;
		}
	    }
	}
	DEBUG_EXECUTE_r(if (!did_match) {
            RE_PV_QUOTED_DECL(quoted, do_utf8, PERL_DEBUG_PAD_ZERO(0), 
                SvPVX_const(must), RE_SV_DUMPLEN(must), 30);
            PerlIO_printf(Perl_debug_log, "Did not find %s substr %s%s...\n",
			      ((must == prog->anchored_substr)
			       ? "anchored" : "floating"),
                quoted, RE_SV_TAIL(must));
        });		    
	goto phooey;
    }
    else if ( (c = progi->regstclass) ) {
	if (minlen) {
	    const OPCODE op = OP(progi->regstclass);
	    /* don't bother with what can't match */
	    if (PL_regkind[op] != EXACT && op != CANY && PL_regkind[op] != TRIE)
	        strend = HOPc(strend, -(minlen - 1));
	}
	DEBUG_EXECUTE_r({
	    SV * const prop = sv_newmortal();
	    regprop(prog, prop, c);
	    {
		RE_PV_QUOTED_DECL(quoted,do_utf8,PERL_DEBUG_PAD_ZERO(1),
		    s,strend-s,60);
		PerlIO_printf(Perl_debug_log,
		    "Matching stclass %.*s against %s (%d chars)\n",
		    (int)SvCUR(prop), SvPVX_const(prop),
		     quoted, (int)(strend - s));
	    }
	});
        if (find_byclass(prog, c, s, strend, &reginfo))
	    goto got_it;
	DEBUG_EXECUTE_r(PerlIO_printf(Perl_debug_log, "Contradicts stclass... [regexec_flags]\n"));
    }
    else {
	dontbother = 0;
	if (prog->float_substr != NULL) {
	    /* Trim the end. */
	    char *last;
	    SV* float_real;

	    float_real = prog->float_substr;

	    if (flags & REXEC_SCREAM) {
		last = screaminstr(sv, float_real, s - strbeg,
				   end_shift, &scream_pos, 1); /* last one */
		if (!last)
		    last = scream_olds; /* Only one occurrence. */
		/* we may be pointing at the wrong string */
		else if (RXp_MATCH_COPIED(prog))
		    s = strbeg + (s - SvPVX_const(sv));
	    }
	    else {
		STRLEN len;
                const char * const little = SvPV_const(float_real, len);

		if (SvTAIL(float_real)) {
		    if (memEQ(strend - len + 1, little, len - 1))
			last = strend - len + 1;
		    else if (!multiline)
			last = memEQ(strend - len, little, len)
			    ? strend - len : NULL;
		    else
			goto find_last;
		} else {
		  find_last:
		    if (len)
			last = rninstr(s, strend, little, little + len);
		    else
			last = strend;	/* matching "$" */
		}
	    }
	    if (last == NULL) {
		DEBUG_EXECUTE_r(
		    PerlIO_printf(Perl_debug_log,
			"%sCan't trim the tail %s, match fails (should not happen)%s\n",
	                PL_colors[4], SvPVX_const(prog->float_substr), PL_colors[5]));
		goto phooey; /* Should not happen! */
	    }
	    dontbother = strend - last + prog->float_min_offset;
	}
	if (minlen && (dontbother < minlen))
	    dontbother = minlen - 1;
	strend -= dontbother; 		   /* this one's always in bytes! */
	/* We don't know much -- general case. */
	do {
	    if (regtry(&reginfo, &s))
		goto got_it;
	    s++;
	    if (do_utf8) {
		while (s <= strend && UTF8_IS_CONTINUATION(*s)) 
		    s++;
	    }
	} while (s <= strend);
    }

    /* Failure. */
    goto phooey;

got_it:
    if (PL_reg_eval_set)
	restore_pos(aTHX_ prog);
    if (RXp_PAREN_NAMES(prog)) 
        (void)hv_iterinit(RXp_PAREN_NAMES(prog));

    /* make sure $`, $&, $', and $digit will work later */
    if ( !(flags & REXEC_NOT_FIRST) ) {
	RX_MATCH_COPY_FREE(rx);
	if (flags & REXEC_COPY_STR) {
	    const I32 i = PL_regeol - startpos + (stringarg - strbeg);
#ifdef PERL_OLD_COPY_ON_WRITE
	    if ((SvIsCOW(sv)
		 || (SvFLAGS(sv) & CAN_COW_MASK) == CAN_COW_FLAGS)) {
		if (DEBUG_C_TEST) {
		    PerlIO_printf(Perl_debug_log,
				  "Copy on write: regexp capture, type %d\n",
				  (int) SvTYPE(sv));
		}
		prog->saved_copy = sv_setsv_cow(prog->saved_copy, sv);
		prog->subbeg = (char *)SvPVX_const(prog->saved_copy);
		assert (SvPOKp(prog->saved_copy));
	    } else
#endif
	    {
		RX_MATCH_COPIED_on(rx);
		s = savepvn(strbeg, i);
		prog->subbeg = s;
	    }
	    prog->sublen = i;
	}
	else {
	    prog->subbeg = strbeg;
	    prog->sublen = PL_regeol - strbeg;	/* strend may have been modified */
	}
    }

    return 1;

phooey:
    DEBUG_EXECUTE_r(PerlIO_printf(Perl_debug_log, "%sMatch failed%s\n",
			  PL_colors[4], PL_colors[5]));
    if (PL_reg_eval_set)
	restore_pos(aTHX_ prog);
    if (swap_on_fail) 
        /* we failed :-( roll it back */
        swap_match_buff(prog);
    
    return 0;
}


/*
 - regtry - try match at specific point
 */
STATIC I32			/* 0 failure, 1 success */
S_regtry(pTHX_ regmatch_info *reginfo, char **startpos)
{
    dVAR;
    CHECKPOINT lastcp;
    REGEXP *const rx = reginfo->prog;
    regexp *const prog = (struct regexp *)SvANY(rx);
    RXi_GET_DECL(prog,progi);
    GET_RE_DEBUG_FLAGS_DECL;

    PERL_ARGS_ASSERT_REGTRY;

    reginfo->cutpoint=NULL;

    DEBUG_EXECUTE_r( PerlIO_printf(Perl_debug_log, "regtry") );
    assert(*startpos <= PL_regeol);

    if ((prog->extflags & RXf_EVAL_SEEN) && !PL_reg_eval_set) {
	MAGIC *mg;

	PL_reg_eval_set = RS_init;
	DEBUG_EXECUTE_r(DEBUG_s(
	    PerlIO_printf(Perl_debug_log, "  setting stack tmpbase at %"IVdf"\n",
			  (IV)(PL_stack_sp - PL_stack_base));
	    ));
	SAVESTACK_CXPOS();
	cxstack[cxstack_ix].blk_oldsp = PL_stack_sp - PL_stack_base;
	/* Otherwise OP_NEXTSTATE will free whatever on stack now.  */
	SAVETMPS;
	/* Apparently this is not needed, judging by wantarray. */
	/* SAVEI8(cxstack[cxstack_ix].blk_gimme);
	   cxstack[cxstack_ix].blk_gimme = G_SCALAR; */

	if (reginfo->sv) {
	    /* Make $_ available to executed code. */
	    if (reginfo->sv != DEFSV) {
		SAVE_DEFSV;
		SVcpREPLACE(DEFSV, reginfo->sv);
	    }
	
	    if (!(SvTYPE(reginfo->sv) >= SVt_PVMG && SvMAGIC(reginfo->sv)
		  && (mg = mg_find(reginfo->sv, PERL_MAGIC_regex_global)))) {
		/* prepare for quick setting of pos */
#ifdef PERL_OLD_COPY_ON_WRITE
		if (SvIsCOW(reginfo->sv))
		    sv_force_normal_flags(reginfo->sv, 0);
#endif
		mg = sv_magicext(reginfo->sv, NULL, PERL_MAGIC_regex_global,
				 &PL_vtbl_mglob, NULL, 0);
		mg->mg_len = -1;
	    }
	    PL_reg_magic    = mg;
	    PL_reg_oldpos   = mg->mg_len;
	    SAVEDESTRUCTOR_X(restore_pos, prog);
        }
        if (!PL_reg_curpm) {
	    Newxz(PL_reg_curpm, 1, PMOP);
        }
	PM_SETRE(PL_reg_curpm, rx);
	PL_reg_oldcurpm = PL_curpm;
	PL_curpm = PL_reg_curpm;
	if (RXp_MATCH_COPIED(prog)) {
	    /*  Here is a serious problem: we cannot rewrite subbeg,
		since it may be needed if this match fails.  Thus
		$` inside (?{}) could fail... */
	    PL_reg_oldsaved = prog->subbeg;
	    PL_reg_oldsavedlen = prog->sublen;
#ifdef PERL_OLD_COPY_ON_WRITE
	    PL_nrs = prog->saved_copy;
#endif
	    RXp_MATCH_COPIED_off(prog);
	}
	else
	    PL_reg_oldsaved = NULL;
	prog->subbeg = PL_bostr;
	prog->sublen = PL_regeol - PL_bostr; /* strend may have been modified */
    }
    DEBUG_EXECUTE_r(PL_reg_starttry = *startpos);
    prog->offs[0].start = *startpos - PL_bostr;
    PL_reginput = *startpos;
    PL_reglastparen = &prog->lastparen;
    PL_reglastcloseparen = &prog->lastcloseparen;
    prog->lastparen = 0;
    prog->lastcloseparen = 0;
    PL_regsize = 0;
    PL_regoffs = prog->offs;
    if (PL_reg_start_tmpl <= prog->nparens) {
	PL_reg_start_tmpl = prog->nparens*3/2 + 3;
        if(PL_reg_start_tmp)
            Renew(PL_reg_start_tmp, PL_reg_start_tmpl, char*);
        else
            Newx(PL_reg_start_tmp, PL_reg_start_tmpl, char*);
    }

    /* XXXX What this code is doing here?!!!  There should be no need
       to do this again and again, PL_reglastparen should take care of
       this!  --ilya*/

    /* Tests pat.t#187 and split.t#{13,14} seem to depend on this code.
     * Actually, the code in regcppop() (which Ilya may be meaning by
     * PL_reglastparen), is not needed at all by the test suite
     * (op/regexp, op/pat, op/split), but that code is needed otherwise
     * this erroneously leaves $1 defined: "1" =~ /^(?:(\d)x)?\d$/
     * Meanwhile, this code *is* needed for the
     * above-mentioned test suite tests to succeed.  The common theme
     * on those tests seems to be returning null fields from matches.
     * --jhi updated by dapm */
#if 1
    if (prog->nparens) {
	regexp_paren_pair *pp = PL_regoffs;
	register I32 i;
	for (i = prog->nparens; i > (I32)*PL_reglastparen; i--) {
	    ++pp;
	    pp->start = -1;
	    pp->end = -1;
	}
    }
#endif
    REGCP_SET(lastcp);
    assert(PL_reginput <= PL_regeol);
    if (regmatch(reginfo, progi->program + 1)) {
	PL_regoffs[0].end = PL_reginput - PL_bostr;
	return 1;
    }
    if (reginfo->cutpoint)
        *startpos= reginfo->cutpoint;
    REGCP_UNWIND(lastcp);
    return 0;
}


#define sayYES goto yes
#define sayNO goto no
#define sayNO_SILENT goto no_silent

/* we dont use STMT_START/END here because it leads to 
   "unreachable code" warnings, which are bogus, but distracting. */
#define CACHEsayNO \
    if (ST.cache_mask) \
       PL_reg_poscache[ST.cache_offset] |= ST.cache_mask; \
    sayNO

/* this is used to determine how far from the left messages like
   'failed...' are printed. It should be set such that messages 
   are inline with the regop output that created them.
*/
#define REPORT_CODE_OFF 32


/* Make sure there is a test for this +1 options in re_tests */
#define TRIE_INITAL_ACCEPT_BUFFLEN 4;

#define CHRTEST_UNINIT -1001 /* c1/c2 haven't been calculated yet */
#define CHRTEST_VOID   -1000 /* the c1/c2 "next char" test should be skipped */

#define SLAB_FIRST(s) (&(s)->states[0])
#define SLAB_LAST(s)  (&(s)->states[PERL_REGMATCH_SLAB_SLOTS-1])

/* grab a new slab and return the first slot in it */

STATIC regmatch_state *
S_push_slab(pTHX)
{
#if PERL_VERSION < 9 && !defined(PERL_CORE)
    dMY_CXT;
#endif
    regmatch_slab *s = PL_regmatch_slab->next;
    if (!s) {
	Newx(s, 1, regmatch_slab);
	s->prev = PL_regmatch_slab;
	s->next = NULL;
	PL_regmatch_slab->next = s;
    }
    PL_regmatch_slab = s;
    return SLAB_FIRST(s);
}


/* push a new state then goto it */

#define PUSH_STATE_GOTO(state, node) \
    scan = node; \
    st->resume_state = state; \
    goto push_state;

/* push a new state with success backtracking, then goto it */

#define PUSH_YES_STATE_GOTO(state, node) \
    scan = node; \
    st->resume_state = state; \
    goto push_yes_state;



/*

regmatch() - main matching routine

This is basically one big switch statement in a loop. We execute an op,
set 'next' to point the next op, and continue. If we come to a point which
we may need to backtrack to on failure such as (A|B|C), we push a
backtrack state onto the backtrack stack. On failure, we pop the top
state, and re-enter the loop at the state indicated. If there are no more
states to pop, we return failure.

Sometimes we also need to backtrack on success; for example /A+/, where
after successfully matching one A, we need to go back and try to
match another one; similarly for lookahead assertions: if the assertion
completes successfully, we backtrack to the state just before the assertion
and then carry on.  In these cases, the pushed state is marked as
'backtrack on success too'. This marking is in fact done by a chain of
pointers, each pointing to the previous 'yes' state. On success, we pop to
the nearest yes state, discarding any intermediate failure-only states.
Sometimes a yes state is pushed just to force some cleanup code to be
called at the end of a successful match or submatch; e.g. (??{$re}) uses
it to free the inner regex.

Note that failure backtracking rewinds the cursor position, while
success backtracking leaves it alone.

A pattern is complete when the END op is executed, while a subpattern
such as (?=foo) is complete when the SUCCESS op is executed. Both of these
ops trigger the "pop to last yes state if any, otherwise return true"
behaviour.

A common convention in this function is to use A and B to refer to the two
subpatterns (or to the first nodes thereof) in patterns like /A*B/: so A is
the subpattern to be matched possibly multiple times, while B is the entire
rest of the pattern. Variable and state names reflect this convention.

The states in the main switch are the union of ops and failure/success of
substates associated with with that op.  For example, IFMATCH is the op
that does lookahead assertions /(?=A)B/ and so the IFMATCH state means
'execute IFMATCH'; while IFMATCH_A is a state saying that we have just
successfully matched A and IFMATCH_A_fail is a state saying that we have
just failed to match A. Resume states always come in pairs. The backtrack
state we push is marked as 'IFMATCH_A', but when that is popped, we resume
at IFMATCH_A or IFMATCH_A_fail, depending on whether we are backtracking
on success or failure.

The struct that holds a backtracking state is actually a big union, with
one variant for each major type of op. The variable st points to the
top-most backtrack struct. To make the code clearer, within each
block of code we #define ST to alias the relevant union.

Here's a concrete example of a (vastly oversimplified) IFMATCH
implementation:

    switch (state) {
    ....

#define ST st->u.ifmatch

    case IFMATCH: // we are executing the IFMATCH op, (?=A)B
	ST.foo = ...; // some state we wish to save
	...
	// push a yes backtrack state with a resume value of
	// IFMATCH_A/IFMATCH_A_fail, then continue execution at the
	// first node of A:
	PUSH_YES_STATE_GOTO(IFMATCH_A, A);
	// NOTREACHED

    case IFMATCH_A: // we have successfully executed A; now continue with B
	next = B;
	bar = ST.foo; // do something with the preserved value
	break;

    case IFMATCH_A_fail: // A failed, so the assertion failed
	...;   // do some housekeeping, then ...
	sayNO; // propagate the failure

#undef ST

    ...
    }

For any old-timers reading this who are familiar with the old recursive
approach, the code above is equivalent to:

    case IFMATCH: // we are executing the IFMATCH op, (?=A)B
    {
	int foo = ...
	...
	if (regmatch(A)) {
	    next = B;
	    bar = foo;
	    break;
	}
	...;   // do some housekeeping, then ...
	sayNO; // propagate the failure
    }

The topmost backtrack state, pointed to by st, is usually free. If you
want to claim it, populate any ST.foo fields in it with values you wish to
save, then do one of

	PUSH_STATE_GOTO(resume_state, node);
	PUSH_YES_STATE_GOTO(resume_state, node);

which sets that backtrack state's resume value to 'resume_state', pushes a
new free entry to the top of the backtrack stack, then goes to 'node'.
On backtracking, the free slot is popped, and the saved state becomes the
new free state. An ST.foo field in this new top state can be temporarily
accessed to retrieve values, but once the main loop is re-entered, it
becomes available for reuse.

Note that the depth of the backtrack stack constantly increases during the
left-to-right execution of the pattern, rather than going up and down with
the pattern nesting. For example the stack is at its maximum at Z at the
end of the pattern, rather than at X in the following:

    /(((X)+)+)+....(Y)+....Z/

The only exceptions to this are lookahead/behind assertions and the cut,
(?>A), which pop all the backtrack states associated with A before
continuing.
 
Bascktrack state structs are allocated in slabs of about 4K in size.
PL_regmatch_state and st always point to the currently active state,
and PL_regmatch_slab points to the slab currently containing
PL_regmatch_state.  The first time regmatch() is called, the first slab is
allocated, and is never freed until interpreter destruction. When the slab
is full, a new one is allocated and chained to the end. At exit from
regmatch(), slabs allocated since entry are freed.

*/
 

#define DEBUG_STATE_pp(pp)				    \
    DEBUG_STATE_r({					    \
	DUMP_EXEC_POS(locinput, scan, do_utf8);		    \
	PerlIO_printf(Perl_debug_log,			    \
	    "    %*s"pp" %s%s%s%s%s\n",			    \
	    depth*2, "",				    \
	    PL_reg_name[st->resume_state],                     \
	    ((st==yes_state||st==mark_state) ? "[" : ""),   \
	    ((st==yes_state) ? "Y" : ""),                   \
	    ((st==mark_state) ? "M" : ""),                  \
	    ((st==yes_state||st==mark_state) ? "]" : "")    \
	);                                                  \
    });


#define REG_NODE_NUM(x) ((x) ? (int)((x)-prog) : -1)

#ifdef DEBUGGING

STATIC void
S_debug_start_match(pTHX_ REGEXP *prog, const bool do_utf8, 
    const char *start, const char *end, const char *blurb)
{
    const bool utf8_pat= RX_EXTFLAGS(prog) & RXf_PMf_UTF8 ? 1 : 0;

    PERL_ARGS_ASSERT_DEBUG_START_MATCH;

    if (!PL_colorset)   
            reginitcolors();    
    {
        RE_PV_QUOTED_DECL(s0, utf8_pat, PERL_DEBUG_PAD_ZERO(0), 
            RX_PRECOMP(prog), RX_PRELEN(prog), 60);   
        
        RE_PV_QUOTED_DECL(s1, do_utf8, PERL_DEBUG_PAD_ZERO(1), 
            start, end - start, 60); 
        
        PerlIO_printf(Perl_debug_log, 
            "%s%s REx%s %s against %s\n", 
		       PL_colors[4], blurb, PL_colors[5], s0, s1); 
        
        if (do_utf8||utf8_pat) 
            PerlIO_printf(Perl_debug_log, "UTF-8 %s%s%s...\n",
                utf8_pat ? "pattern" : "",
                utf8_pat && do_utf8 ? " and " : "",
                do_utf8 ? "string" : ""
            ); 
    }
}

STATIC void
S_dump_exec_pos(pTHX_ const char *locinput, 
                      const regnode *scan, 
                      const char *loc_regeol, 
                      const char *loc_bostr, 
                      const char *loc_reg_starttry,
                      const bool do_utf8)
{
    const int docolor = *PL_colors[0] || *PL_colors[2] || *PL_colors[4];
    const int taill = (docolor ? 10 : 7); /* 3 chars for "> <" */
    int l = (loc_regeol - locinput) > taill ? taill : (loc_regeol - locinput);
    /* The part of the string before starttry has one color
       (pref0_len chars), between starttry and current
       position another one (pref_len - pref0_len chars),
       after the current position the third one.
       We assume that pref0_len <= pref_len, otherwise we
       decrease pref0_len.  */
    int pref_len = (locinput - loc_bostr) > (5 + taill) - l
	? (5 + taill) - l : locinput - loc_bostr;
    int pref0_len;

    PERL_ARGS_ASSERT_DUMP_EXEC_POS;

    while (do_utf8 && UTF8_IS_CONTINUATION(*(locinput - pref_len)))
	pref_len++;
    pref0_len = pref_len  - (locinput - loc_reg_starttry);
    if (l + pref_len < (5 + taill) && l < loc_regeol - locinput)
	l = ( loc_regeol - locinput > (5 + taill) - pref_len
	      ? (5 + taill) - pref_len : loc_regeol - locinput);
    while (do_utf8 && UTF8_IS_CONTINUATION(*(locinput + l)))
	l--;
    if (pref0_len < 0)
	pref0_len = 0;
    if (pref0_len > pref_len)
	pref0_len = pref_len;
    {
	const int is_uni = (do_utf8 && OP(scan) != CANY) ? 1 : 0;

	RE_PV_COLOR_DECL(s0,len0,is_uni,PERL_DEBUG_PAD(0),
	    (locinput - pref_len),pref0_len, 60, 4, 5);
	
	RE_PV_COLOR_DECL(s1,len1,is_uni,PERL_DEBUG_PAD(1),
		    (locinput - pref_len + pref0_len),
		    pref_len - pref0_len, 60, 2, 3);
	
	RE_PV_COLOR_DECL(s2,len2,is_uni,PERL_DEBUG_PAD(2),
		    locinput, loc_regeol - locinput, 10, 0, 1);

	const STRLEN tlen=len0+len1+len2;
	PerlIO_printf(Perl_debug_log,
		    "%4"IVdf" <%.*s%.*s%s%.*s>%*s|",
		    (IV)(locinput - loc_bostr),
		    len0, s0,
		    len1, s1,
		    (docolor ? "" : "> <"),
		    len2, s2,
		    (int)(tlen > 19 ? 0 :  19 - tlen),
		    "");
    }
}

#endif

/* reg_check_named_buff_matched()
 * Checks to see if a named buffer has matched. The data array of 
 * buffer numbers corresponding to the buffer is expected to reside
 * in the regexp->data->data array in the slot stored in the ARG() of
 * node involved. Note that this routine doesn't actually care about the
 * name, that information is not preserved from compilation to execution.
 * Returns the index of the leftmost defined buffer with the given name
 * or 0 if non of the buffers matched.
 */
STATIC I32
S_reg_check_named_buff_matched(pTHX_ const regexp *rex, const regnode *scan)
{
    I32 n;
    RXi_GET_DECL(rex,rexi);
    SV *sv_dat=(SV*)rexi->data->data[ ARG( scan ) ];
    I32 *nums=(I32*)SvPVX_mutable(sv_dat);

    PERL_ARGS_ASSERT_REG_CHECK_NAMED_BUFF_MATCHED;

    for ( n=0; n<SvIVX(sv_dat); n++ ) {
        if ((I32)*PL_reglastparen >= nums[n] &&
            PL_regoffs[nums[n]].end != -1)
        {
            return nums[n];
        }
    }
    return 0;
}


/* free all slabs above current one  - called during LEAVE_SCOPE */

STATIC void
S_clear_backtrack_stack(pTHX_ void *p)
{
    regmatch_slab *s = PL_regmatch_slab->next;
    PERL_UNUSED_ARG(p);

    if (!s)
	return;
    PL_regmatch_slab->next = NULL;
    while (s) {
	regmatch_slab * const osl = s;
	s = s->next;
	Safefree(osl);
    }
}


#define SETREX(Re1,Re2) \
    if (PL_reg_eval_set) PM_SETRE((PL_reg_curpm), (Re2)); \
    Re1 = (Re2)

STATIC I32			/* 0 failure, 1 success */
S_regmatch(pTHX_ regmatch_info *reginfo, regnode *prog)
{
#if PERL_VERSION < 9 && !defined(PERL_CORE)
    dMY_CXT;
#endif
    dVAR;
    const U32 uniflags = UTF8_ALLOW_DEFAULT | UTF8_CHECK_ONLY;

    REGEXP *rex_sv = reginfo->prog;
    regexp *rex = (struct regexp *)SvANY(rex_sv);
    RXi_GET_DECL(rex,rexi);

    register const bool do_utf8 = (RX_EXTFLAGS(reginfo->prog) & RXf_PMf_UTF8) != 0;
    
    I32	oldsave;
    /* the current state. This is a cached copy of PL_regmatch_state */
    register regmatch_state *st;
    /* cache heavy used fields of st in registers */
    register regnode *scan;
    register regnode *next;
    register U32 n = 0;	/* general value; init to avoid compiler warning */
    register I32 ln = 0; /* len or last;  init to avoid compiler warning */
    register char *locinput = PL_reginput;
    register U8 nextchr;   /* is always set to UCHARAT(locinput) */

    bool result = 0;	    /* return value of S_regmatch */
    int depth = 0;	    /* depth of backtrack stack */
    U32 nochange_depth = 0; /* depth of GOSUB recursion with nochange */
    const U32 max_nochange_depth =
        (3 * rex->nparens > MAX_RECURSE_EVAL_NOCHANGE_DEPTH) ?
        3 * rex->nparens : MAX_RECURSE_EVAL_NOCHANGE_DEPTH;
    regmatch_state *yes_state = NULL; /* state to pop to on success of
							    subpattern */
    /* mark_state piggy backs on the yes_state logic so that when we unwind 
       the stack on success we can update the mark_state as we go */
    regmatch_state *mark_state = NULL; /* last mark state we have seen */
    regmatch_state *cur_eval = NULL; /* most recent EVAL_AB state */
    struct regmatch_state  *cur_curlyx = NULL; /* most recent curlyx */
    U32 state_num;
    bool no_final = 0;      /* prevent failure from backtracking? */
    bool do_cutgroup = 0;   /* no_final only until next branch/trie entry */
    char *startpoint = PL_reginput;
    SV *popmark = NULL;     /* are we looking for a mark? */
    SV *sv_commit = NULL;   /* last mark name seen in failure */
    SV *sv_yes_mark = NULL; /* last mark name we have seen 
                               during a successfull match */
    U32 lastopen = 0;       /* last open we saw */
    bool has_cutgroup = RX_HAS_CUTGROUP(rex) ? 1 : 0;   
    /* these three flags are set by various ops to signal information to
     * the very next op. They have a useful lifetime of exactly one loop
     * iteration, and are not preserved or restored by state pushes/pops
     */
    bool sw = 0;	    /* the condition value in (?(cond)a|b) */
    bool minmod = 0;	    /* the next "{n,m}" is a "{n,m}?" */
    int logical = 0;	    /* the following EVAL is:
				0: (?{...})
				1: (?(?{...})X|Y)
				2: (??{...})
			       or the following IFMATCH/UNLESSM is:
			        false: plain (?=foo)
				true:  used as a condition: (?(?=foo))
			    */
#ifdef DEBUGGING
    GET_RE_DEBUG_FLAGS_DECL;
#endif

    PERL_ARGS_ASSERT_REGMATCH;

    DEBUG_OPTIMISE_r( DEBUG_EXECUTE_r({
	    PerlIO_printf(Perl_debug_log,"regmatch start\n");
    }));
    /* on first ever call to regmatch, allocate first slab */
    if (!PL_regmatch_slab) {
	Newx(PL_regmatch_slab, 1, regmatch_slab);
	PL_regmatch_slab->prev = NULL;
	PL_regmatch_slab->next = NULL;
	PL_regmatch_state = SLAB_FIRST(PL_regmatch_slab);
    }

    oldsave = PL_savestack_ix;
    SAVEDESTRUCTOR_X(S_clear_backtrack_stack, NULL);
    SAVEVPTR(PL_regmatch_slab);
    SAVEVPTR(PL_regmatch_state);

    /* grab next free state slot */
    st = ++PL_regmatch_state;
    if (st >  SLAB_LAST(PL_regmatch_slab))
	st = PL_regmatch_state = S_push_slab(aTHX);

    /* Note that nextchr is a byte even in UTF */
    assert(locinput <= PL_regeol);
    nextchr = UCHARAT(locinput);
    scan = prog;
    while (scan != NULL) {

        DEBUG_EXECUTE_r( {
	    SV * const prop = sv_newmortal();
	    regnode *rnext=regnext(scan);
	    DUMP_EXEC_POS( locinput, scan, do_utf8 );
	    regprop(rex, prop, scan);
            
	    PerlIO_printf(Perl_debug_log,
		    "%3"IVdf":%*s%s(%"IVdf")\n",
		    (IV)(scan - rexi->program), depth*2, "",
		    SvPVX_const(prop),
		    (PL_regkind[OP(scan)] == END || !rnext) ? 
		        0 : (IV)(rnext - rexi->program));
	});

	next = scan + NEXT_OFF(scan);
	if (next == scan)
	    next = NULL;
	state_num = OP(scan);

      reenter_switch:
	switch (state_num) {
	case BOL:
	    if (locinput == PL_bostr)
	    {
		/* reginfo->till = reginfo->bol; */
		break;
	    }
	    sayNO;
	case MBOL:
	    if (locinput == PL_bostr ||
		((nextchr || locinput < PL_regeol) && locinput[-1] == '\n'))
	    {
		break;
	    }
	    sayNO;
	case SBOL:
	    if (locinput == PL_bostr)
		break;
	    sayNO;
	case GPOS:
	    if (locinput == reginfo->ganch)
		break;
	    sayNO;

	case KEEPS:
	    /* update the startpoint */
	    st->u.keeper.val = PL_regoffs[0].start;
	    PL_reginput = locinput;
	    PL_regoffs[0].start = locinput - PL_bostr;
	    PUSH_STATE_GOTO(KEEPS_next, next);
	    /*NOT-REACHED*/
	case KEEPS_next_fail:
	    /* rollback the start point change */
	    PL_regoffs[0].start = st->u.keeper.val;
	    sayNO_SILENT;
	    /*NOT-REACHED*/
	case EOL:
		goto seol;
	case MEOL:
	    if ((nextchr || locinput < PL_regeol) && nextchr != '\n')
		sayNO;
	    break;
	case SEOL:
	  seol:
	    if ((nextchr || locinput < PL_regeol) && nextchr != '\n')
		sayNO;
	    if (PL_regeol - locinput > 1)
		sayNO;
	    break;
	case EOS:
	    if (PL_regeol != locinput)
		sayNO;
	    break;
	case SANY:
	    if (!nextchr && locinput >= PL_regeol)
		sayNO;
 	    if (do_utf8) {
	        locinput += PL_utf8skip[nextchr];
		if (locinput > PL_regeol)
 		    sayNO;
 		nextchr = UCHARAT(locinput);
 	    }
 	    else
 		nextchr = UCHARAT(++locinput);
	    break;
	case CANY:
	    if (!nextchr && locinput >= PL_regeol)
		sayNO;
	    nextchr = UCHARAT(++locinput);
	    break;
	case REG_ANY:
	    if ((!nextchr && locinput >= PL_regeol) || nextchr == '\n')
		sayNO;
	    nextchr = UCHARAT(++locinput);
	    break;

	case REG_ANYU:
	    if ((!nextchr && locinput >= PL_regeol) || nextchr == '\n')
		sayNO;
	    locinput += PL_utf8skip[nextchr];
	    if (locinput > PL_regeol)
		sayNO;
	    nextchr = UCHARAT(locinput);
	    break;

#undef  ST
#define ST st->u.trie
        case TRIEC:
            /* In this case the charclass data is available inline so
               we can fail fast without a lot of extra overhead. 
             */
            if (scan->flags == EXACT || !do_utf8) {
                if(!ANYOF_BITMAP_TEST(scan, *locinput)) {
                    DEBUG_EXECUTE_r(
                        PerlIO_printf(Perl_debug_log,
                    	          "%*s  %sfailed to match trie start class...%s\n",
                    	          REPORT_CODE_OFF+depth*2, "", PL_colors[4], PL_colors[5])
                    );
                    sayNO_SILENT;
                    /* NOTREACHED */
                }        	        
            }
            /* FALL THROUGH */
	case TRIE:
	    {
                /* what trie are we using right now */
		reg_trie_data * const trie
        	    = (reg_trie_data*)rexi->data->data[ ARG( scan ) ];
                U32 state = trie->startstate;

        	if (trie->bitmap &&
        	    !TRIE_BITMAP_TEST(trie,*locinput)
        	) {
        	    if (trie->states[ state ].wordnum) {
        	         DEBUG_EXECUTE_r(
                            PerlIO_printf(Perl_debug_log,
                        	          "%*s  %smatched empty string...%s\n",
                        	          REPORT_CODE_OFF+depth*2, "", PL_colors[4], PL_colors[5])
                        );
        	        break;
        	    } else {
        	        DEBUG_EXECUTE_r(
                            PerlIO_printf(Perl_debug_log,
                        	          "%*s  %sfailed to match trie start class...%s\n",
                        	          REPORT_CODE_OFF+depth*2, "", PL_colors[4], PL_colors[5])
                        );
        	        sayNO_SILENT;
        	   }
                }

            { 
		char *uc = locinput;

		STRLEN len = 0;
		STRLEN bufflen=0;
		SV *sv_accept_buff = NULL;

	    	ST.accepted = 0; /* how many accepting states we have seen */
		ST.B = next;
		ST.jump = trie->jump;
		ST.me = scan;
	        /*
        	   traverse the TRIE keeping track of all accepting states
        	   we transition through until we get to a failing node.
        	*/

		while ( state && uc <= PL_regeol ) {
                    U32 base = trie->states[ state ].trans.base;
                    U16 charid;
		    U8 uvc = 0;
                    /* We use charid to hold the wordnum as we don't use it
                       for charid until after we have done the wordnum logic. 
                       We define an alias just so that the wordnum logic reads
                       more naturally. */

#define got_wordnum charid
                    got_wordnum = trie->states[ state ].wordnum;

		    if ( got_wordnum ) {
			if ( ! ST.accepted ) {
			    ENTER;
			    /* SAVETMPS; */ /* XXX is this necessary? dmq */
			    bufflen = TRIE_INITAL_ACCEPT_BUFFLEN;
			    sv_accept_buff=newSV(bufflen *
					    sizeof(reg_trie_accepted) - 1);
			    SvCUR_set(sv_accept_buff, 0);
			    SvPOK_on(sv_accept_buff);
			    sv_2mortal(sv_accept_buff);
			    SAVETMPS;
			    ST.accept_buff =
				(reg_trie_accepted*)SvPV_nolen(sv_accept_buff );
			}
			do {
			    if (ST.accepted >= bufflen) {
				bufflen *= 2;
				ST.accept_buff =(reg_trie_accepted*)
				    SvGROW(sv_accept_buff,
				       	bufflen * sizeof(reg_trie_accepted));
			    }
			    SvCUR_set(sv_accept_buff,SvCUR(sv_accept_buff)
				+ sizeof(reg_trie_accepted));


			    ST.accept_buff[ST.accepted].wordnum = got_wordnum;
			    ST.accept_buff[ST.accepted].endpos = uc;
			    ++ST.accepted;
		        } while (trie->nextword && (got_wordnum= trie->nextword[got_wordnum]));
		    }
#undef got_wordnum 

		    DEBUG_TRIE_EXECUTE_r({
		                DUMP_EXEC_POS( (char *)uc, scan, do_utf8 );
			        PerlIO_printf( Perl_debug_log,
			            "%*s  %sState: %4"UVxf" Accepted: %4"UVxf" ",
			            2+depth * 2, "", PL_colors[4],
			            (UV)state, (UV)ST.accepted );
		    });

		    if ( base ) {
                                                      
			uvc = (U8)*uc;
			charid = trie->charmap[ uvc ];
			len = 1;

			if (charid &&
			     (base + charid > trie->uniquecharcount )
			     && (base + charid - 1 - trie->uniquecharcount
				    < trie->lasttrans)
			     && trie->trans[base + charid - 1 -
				    trie->uniquecharcount].check == state)
			{
			    state = trie->trans[base + charid - 1 -
				trie->uniquecharcount ].next;
			}
			else {
			    state = 0;
			}
			uc += len;

		    }
		    else {
			state = 0;
		    }
		    DEBUG_TRIE_EXECUTE_r(
		        PerlIO_printf( Perl_debug_log,
		            "Charid:%3x CP:%x After State: %4"UVxf"%s\n",
		            charid, uvc, (UV)state, PL_colors[5] );
		    );
		}
		if (!ST.accepted )
		   sayNO;

		DEBUG_EXECUTE_r(
		    PerlIO_printf( Perl_debug_log,
			"%*s  %sgot %"IVdf" possible matches%s\n",
			REPORT_CODE_OFF + depth * 2, "",
			PL_colors[4], (IV)ST.accepted, PL_colors[5] );
		);
	    }}
            goto trie_first_try; /* jump into the fail handler */
	    /* NOTREACHED */
	case TRIE_next_fail: /* we failed - try next alterative */
            if ( ST.jump) {
                REGCP_UNWIND(ST.cp);
	        for (n = *PL_reglastparen; n > ST.lastparen; n--)
		    PL_regoffs[n].end = -1;
	        *PL_reglastparen = n;
	    }
          trie_first_try:
            if (do_cutgroup) {
                do_cutgroup = 0;
                no_final = 0;
            }

            if ( ST.jump) {
                ST.lastparen = *PL_reglastparen;
	        REGCP_SET(ST.cp);
            }	        
	    if ( ST.accepted == 1 ) {
		/* only one choice left - just continue */
		DEBUG_EXECUTE_r({
		    AV *const trie_words
			= (AV *) rexi->data->data[ARG(ST.me)+TRIE_WORDS_OFFSET];
		    SV ** const tmp = av_fetch( trie_words, 
		        ST.accept_buff[ 0 ].wordnum-1, 0 );
		    SV *sv= tmp ? sv_newmortal() : NULL;
		    
		    PerlIO_printf( Perl_debug_log,
			"%*s  %sonly one match left: #%d <%s>%s\n",
			REPORT_CODE_OFF+depth*2, "", PL_colors[4],
			ST.accept_buff[ 0 ].wordnum,
			tmp ? pv_pretty(sv, SvPV_nolen_const(*tmp), SvCUR(*tmp), 0, 
	                        PL_colors[0], PL_colors[1],
	                        (IN_CODEPOINTS ? PERL_PV_ESCAPE_UNI : 0)
                            ) 
			: "not compiled under -Dr",
			PL_colors[5] );
		});
		PL_reginput = (char *)ST.accept_buff[ 0 ].endpos;
		/* in this case we free tmps/leave before we call regmatch
		   as we wont be using accept_buff again. */
		
		locinput = PL_reginput;
		nextchr = UCHARAT(locinput);
    		if ( !ST.jump || !ST.jump[ST.accept_buff[0].wordnum]) 
    		    scan = ST.B;
    		else
    		    scan = ST.me + ST.jump[ST.accept_buff[0].wordnum];
		if (!has_cutgroup) {
		    FREETMPS;
		    LEAVE;
                } else {
                    ST.accepted--;
                    PUSH_YES_STATE_GOTO(TRIE_next, scan);
                }
		
		continue; /* execute rest of RE */
	    }
	    
	    if ( !ST.accepted-- ) {
	        DEBUG_EXECUTE_r({
		    PerlIO_printf( Perl_debug_log,
			"%*s  %sTRIE failed...%s\n",
			REPORT_CODE_OFF+depth*2, "", 
			PL_colors[4],
			PL_colors[5] );
		});
		FREETMPS;
		LEAVE;
		sayNO_SILENT;
		/*NOTREACHED*/
	    } 

	    /*
	       There are at least two accepting states left.  Presumably
	       the number of accepting states is going to be low,
	       typically two. So we simply scan through to find the one
	       with lowest wordnum.  Once we find it, we swap the last
	       state into its place and decrement the size. We then try to
	       match the rest of the pattern at the point where the word
	       ends. If we succeed, control just continues along the
	       regex; if we fail we return here to try the next accepting
	       state
	     */

	    {
		U32 best = 0;
		U32 cur;
		for( cur = 1 ; cur <= ST.accepted ; cur++ ) {
		    DEBUG_TRIE_EXECUTE_r(
			PerlIO_printf( Perl_debug_log,
			    "%*s  %sgot %"IVdf" (%d) as best, looking at %"IVdf" (%d)%s\n",
			    REPORT_CODE_OFF + depth * 2, "", PL_colors[4],
			    (IV)best, ST.accept_buff[ best ].wordnum, (IV)cur,
			    ST.accept_buff[ cur ].wordnum, PL_colors[5] );
		    );

		    if (ST.accept_buff[cur].wordnum <
			    ST.accept_buff[best].wordnum)
			best = cur;
		}

		DEBUG_EXECUTE_r({
		    AV *const trie_words
			= (AV *) rexi->data->data[ARG(ST.me)+TRIE_WORDS_OFFSET];
		    SV ** const tmp = av_fetch( trie_words, 
		        ST.accept_buff[ best ].wordnum - 1, 0 );
		    regnode *nextop=(!ST.jump || !ST.jump[ST.accept_buff[best].wordnum]) ? 
		                    ST.B : 
		                    ST.me + ST.jump[ST.accept_buff[best].wordnum];    
		    SV *sv= tmp ? sv_newmortal() : NULL;
		    
		    PerlIO_printf( Perl_debug_log, 
		        "%*s  %strying alternation #%d <%s> at node #%d %s\n",
			REPORT_CODE_OFF+depth*2, "", PL_colors[4],
			ST.accept_buff[best].wordnum,
			tmp ? pv_pretty(sv, SvPV_nolen_const(*tmp), SvCUR(*tmp), 0, 
	                        PL_colors[0], PL_colors[1],
	                        (IN_CODEPOINTS ? PERL_PV_ESCAPE_UNI : 0)
                            ) : "not compiled under -Dr", 
			    REG_NODE_NUM(nextop),
			PL_colors[5] );
		});

		if ( best<ST.accepted ) {
		    reg_trie_accepted tmp = ST.accept_buff[ best ];
		    ST.accept_buff[ best ] = ST.accept_buff[ ST.accepted ];
		    ST.accept_buff[ ST.accepted ] = tmp;
		    best = ST.accepted;
		}
		PL_reginput = (char *)ST.accept_buff[ best ].endpos;
		if ( !ST.jump || !ST.jump[ST.accept_buff[best].wordnum]) {
		    scan = ST.B;
		} else {
		    scan = ST.me + ST.jump[ST.accept_buff[best].wordnum];
                }
                PUSH_YES_STATE_GOTO(TRIE_next, scan);    
                /* NOTREACHED */
	    }
	    /* NOTREACHED */
        case TRIE_next:
            FREETMPS;
	    LEAVE;
	    sayYES;
#undef  ST

	case EXACT: {

	    char *s = STRING(scan);
	    ln = STR_LEN(scan);
	    /* Inline the first character, for speed. */
	    if (UCHARAT(s) != nextchr)
		sayNO;
	    if (PL_regeol - locinput < ln)
		sayNO;
	    if (ln > 1 && memNE(s, locinput, ln))
		sayNO;
	    locinput += ln;
	    nextchr = UCHARAT(locinput);
	    break;
	    }
	case ANYOFU: {
	    STRLEN inclasslen = PL_regeol - locinput;

	    if (!reginclass(rex, scan, locinput, &inclasslen))
		goto anyof_fail;
	    if (locinput >= PL_regeol)
		sayNO;
	    locinput += inclasslen ? inclasslen : UTF8SKIP(locinput);
	    nextchr = UCHARAT(locinput);
	    break;
	    }
	anyof_fail:
	    /* If we might have the case of the German sharp s
	     * in a casefolding Unicode character class. */

	    if (ANYOF_FOLD_SHARP_S(scan, locinput, PL_regeol)) {
		 locinput += SHARP_S_SKIP;
		 nextchr = UCHARAT(locinput);
	    }
	    else
		 sayNO;
	    break;
	case ANYOF:
	    if (!REGINCLASS(rex, scan, locinput))
		sayNO;
	    if (!nextchr && locinput >= PL_regeol)
		sayNO;
	    nextchr = UCHARAT(++locinput);
	    break;
	case BOUNDL:
	case NBOUNDL:
	    /* FALL THROUGH */
	case BOUND:
	case NBOUND:
	    /* was last char in word? */
	    if (do_utf8) {
		if (locinput == PL_bostr)
		    ln = '\n';
		else {
		    const char * const r = reghop3c(locinput, -1, PL_bostr);
		
		    ln = utf8n_to_uvchr(r, UTF8SKIP(r), 0, uniflags);
		}
		if (OP(scan) == BOUND || OP(scan) == NBOUND) {
		    ln = isALNUM_uni(ln);
		    LOAD_UTF8_CHARCLASS_ALNUM();
		    n = swash_fetch(PL_utf8_alnum, locinput, do_utf8);
		}
		else {
		    ln = isALNUM_LC_uvchr(UNI_TO_NATIVE(ln));
		    n = isALNUM_LC_utf8(locinput);
		}
	    }
	    else {
		ln = (locinput != PL_bostr) ?
		    UCHARAT(locinput - 1) : '\n';
		if (OP(scan) == BOUND || OP(scan) == NBOUND) {
		    ln = isALNUM(ln);
		    n = isALNUM(nextchr);
		}
		else {
		    ln = isALNUM_LC(ln);
		    n = isALNUM_LC(nextchr);
		}
	    }
	    if (((!ln) == (!n)) == (OP(scan) == BOUND ||
				    OP(scan) == BOUNDL))
		    sayNO;
	    break;
	case CLUMP:
	    if (locinput >= PL_regeol)
		sayNO;
	    if  (do_utf8) {
		LOAD_UTF8_CHARCLASS_MARK();
		if (swash_fetch(PL_utf8_mark,locinput, do_utf8))
		    sayNO;
		locinput += PL_utf8skip[nextchr];
		while (locinput < PL_regeol &&
		       swash_fetch(PL_utf8_mark,locinput, do_utf8))
		    locinput += UTF8SKIP(locinput);
		if (locinput > PL_regeol)
		    sayNO;
	    } 
	    else
	       locinput++;
	    nextchr = UCHARAT(locinput);
	    break;
            
	case NREFFL:
	{
	    char *s;
	    char type;
	    /* FALL THROUGH */
	case NREF:
	case NREFF:
	    type = OP(scan);
	    n = reg_check_named_buff_matched(rex,scan);

            if ( n ) {
                type = REF + ( type - NREF );
                goto do_ref;
            } else {
                sayNO;
            }
            /* unreached */
	case REFFL:
	    /* FALL THROUGH */
        case REF:
	case REFF: 
	    n = ARG(scan);  /* which paren pair */
	    type = OP(scan);
	  do_ref:  
	    ln = PL_regoffs[n].start;
	    PL_reg_leftiter = PL_reg_maxiter;		/* Void cache */
	    if (*PL_reglastparen < n || ln == -1)
		sayNO;			/* Do not match unless seen CLOSEn. */
	    if (ln == PL_regoffs[n].end)
		break;

	    s = PL_bostr + ln;
	    if (do_utf8 && type != REF) {	/* REF can do byte comparison */
		char *l = locinput;
		const char *e = PL_bostr + PL_regoffs[n].end;
		/*
		 * Note that we can't do the "other character" lookup trick as
		 * in the 8-bit case (no pun intended) because in Unicode we
		 * have to map both upper and title case to lower case.
		 */
		if (type == REFF) {
		    while (s < e) {
			STRLEN ulen1, ulen2;
			char tmpbuf1[UTF8_MAXBYTES_CASE+1];
			char tmpbuf2[UTF8_MAXBYTES_CASE+1];

			if (l >= PL_regeol)
			    sayNO;
			toLOWER_utf8(s, tmpbuf1, &ulen1);
			toLOWER_utf8(l, tmpbuf2, &ulen2);
			if (ulen1 != ulen2 || memNE((char *)tmpbuf1, (char *)tmpbuf2, ulen1))
			    sayNO;
			s += ulen1;
			l += ulen2;
		    }
		}
		locinput = l;
		nextchr = UCHARAT(locinput);
		break;
	    }

	    /* Inline the first character, for speed. */
	    if (UCHARAT(s) != nextchr &&
		(type == REF ||
		 (UCHARAT(s) != (type == REFF
				  ? PL_fold : PL_fold_locale)[nextchr])))
		sayNO;
	    ln = PL_regoffs[n].end - ln;
	    if (locinput + ln > PL_regeol)
		sayNO;
	    if (ln > 1 && (type == REF
			   ? memNE(s, locinput, ln)
			   : (type == REFF
			      ? ibcmp(s, locinput, ln)
			      : ibcmp_locale(s, locinput, ln))))
		sayNO;
	    locinput += ln;
	    nextchr = UCHARAT(locinput);
	    break;
	}
	case NOTHING:
	case TAIL:
	    break;
	case BACK:
	    break;

#undef  ST
#define ST st->u.eval
	{
	    SV *ret;
	    REGEXP *re_sv;
            regexp *re;
            regexp_internal *rei;
            regnode *startpoint;

	case GOSTART:
	case GOSUB: /*    /(...(?1))/   /(...(?&foo))/   */
	    if (cur_eval && cur_eval->locinput==locinput) {
                if (cur_eval->u.eval.close_paren == (U32)ARG(scan)) 
                    Perl_croak(aTHX_ "Infinite recursion in regex");
                if ( ++nochange_depth > max_nochange_depth )
                    Perl_croak(aTHX_ 
                        "Pattern subroutine nesting without pos change"
                        " exceeded limit in regex");
            } else {
                nochange_depth = 0;
            }
	    re_sv = rex_sv;
            re = rex;
            rei = rexi;
            (void)ReREFCNT_inc(rex_sv);
            if (OP(scan)==GOSUB) {
                startpoint = scan + ARG2L(scan);
                ST.close_paren = ARG(scan);
            } else {
                startpoint = rei->program+1;
                ST.close_paren = 0;
            }
            goto eval_recurse_doit;
            /* NOTREACHED */
        case EVAL:  /*   /(?{A})B/   /(??{A})B/  and /(?(?{A})X|Y)B/   */        
            if (cur_eval && cur_eval->locinput==locinput) {
		if ( ++nochange_depth > max_nochange_depth )
                    Perl_croak(aTHX_ "EVAL without pos change exceeded limit in regex");
            } else {
                nochange_depth = 0;
            }    
	    {
		/* execute the code in the {...} */
		dSP;
		SV ** const before = SP;
		OP * const oop = PL_op;
		COP * const ocurcop = PL_curcop;
		PAD *old_comppad;
	    
		n = ARG(scan);
		PL_op = ((OP*)rexi->data->data[n])->op_next;
		DEBUG_STATE_r( PerlIO_printf(Perl_debug_log, 
		    "  re_eval 0x%"UVxf"\n", PTR2UV(PL_op)) );
		PAD_SAVE_LOCAL(old_comppad, (PAD*)rexi->data->data[n + 2]);
		PL_regoffs[0].end = PL_reg_magic->mg_len = locinput - PL_bostr;

                if (sv_yes_mark) {
                    SV *sv_mrk = get_sv("REGMARK", 1);
                    sv_setsv(sv_mrk, sv_yes_mark);
                }

		CALLRUNOPS(aTHX);			/* Scalar context. */
		SPAGAIN;
		if (SP == before)
		    ret = &PL_sv_undef;   /* protect against empty (?{}) blocks. */
		else {
		    ret = POPs;
		    PUTBACK;
		}

		PL_op = oop;
		PAD_RESTORE_LOCAL(old_comppad);
		PL_curcop = ocurcop;
		if (!logical) {
		    /* /(?{...})/ */
		    break;
		}
	    }
	    if (logical == 2) { /* Postponed subexpression: /(??{...})/ */
		logical = 0;
		{
		    /* extract RE object from returned value; compiling if
		     * necessary */
		    MAGIC *mg = NULL;
		    REGEXP *rx = NULL;

		    if (SvROK(ret)) {
			SV *const sv = SvRV(ret);

			if (SvTYPE(sv) == SVt_REGEXP) {
			    rx = (REGEXP*) sv;
			} else if (SvSMAGICAL(sv)) {
			    mg = mg_find(sv, PERL_MAGIC_qr);
			    assert(mg);
			}
		    } else if (SvTYPE(ret) == SVt_REGEXP) {
			rx = (REGEXP*) ret;
		    } else if (SvSMAGICAL(ret)) {
			mg = mg_find(ret, PERL_MAGIC_qr);
			    /* testing suggests mg only ends up non-NULL for
			       scalars who were upgraded and compiled in the
			       else block below. In turn, this is only
			       triggered in the "postponed utf8 string" tests
			       in t/op/pat.t  */
		    }

		    if (mg) {
			rx = (REGEXP *) mg->mg_obj; /*XXX:dmq*/
			assert(rx);
		    }
		    if (rx) {
			rx = reg_temp_copy(rx);
		    }
		    else {
			U32 pm_flags = 0;
			const I32 osize = PL_regsize;

			if (DO_UTF8(ret)) pm_flags |= RXf_PMf_UTF8;
			rx = CALLREGCOMP(ret, pm_flags);
			if (!(SvFLAGS(ret)
			      & (SVs_TEMP | SVs_PADTMP | SVf_READONLY ))) {
			    /* This isn't a first class regexp. Instead, it's
			       caching a regexp onto an existing, Perl visible
			       scalar.  */
			    sv_magic(ret, (SV*) rx, PERL_MAGIC_qr, 0, 0);
			}
			PL_regsize = osize;
		    }
		    re_sv = rx;
		    re = (struct regexp *)SvANY(rx);
		}
                RXp_MATCH_COPIED_off(re);
                re->subbeg = rex->subbeg;
                re->sublen = rex->sublen;
		rei = RXi_GET(re);
                DEBUG_EXECUTE_r(
                    debug_start_match(rex_sv, do_utf8, locinput, PL_regeol, 
                        "Matching embedded");
		);		
		startpoint = rei->program + 1;
               	ST.close_paren = 0; /* only used for GOSUB */
               	/* borrowed from regtry */
                if (PL_reg_start_tmpl <= re->nparens) {
                    PL_reg_start_tmpl = re->nparens*3/2 + 3;
                    if(PL_reg_start_tmp)
                        Renew(PL_reg_start_tmp, PL_reg_start_tmpl, char*);
                    else
                        Newx(PL_reg_start_tmp, PL_reg_start_tmpl, char*);
                }               	

        eval_recurse_doit: /* Share code with GOSUB below this line */                		
		/* run the pattern returned from (??{...}) */
		ST.cp = regcppush(0);	/* Save *all* the positions. */
		REGCP_SET(ST.lastcp);
		
		PL_regoffs = re->offs; /* essentially NOOP on GOSUB */
		
		/* see regtry, specifically PL_reglast(?:close)?paren is a pointer! (i dont know why) :dmq */
		PL_reglastparen = &re->lastparen;
		PL_reglastcloseparen = &re->lastcloseparen;
		re->lastparen = 0;
		re->lastcloseparen = 0;

		PL_reginput = locinput;
		PL_regsize = 0;

		/* XXXX This is too dramatic a measure... */
		PL_reg_maxiter = 0;

		ST.toggle_reg_flags = PL_reg_flags;
		if (re->extflags & RXf_PMf_UTF8)
		    PL_reg_flags |= RF_utf8;
		else
		    PL_reg_flags &= ~RF_utf8;
		ST.toggle_reg_flags ^= PL_reg_flags; /* diff of old and new */

		ST.prev_rex = rex_sv;
		ST.prev_curlyx = cur_curlyx;
		SETREX(rex_sv,re_sv);
		rex = re;
		rexi = rei;
		cur_curlyx = NULL;
		ST.B = next;
		ST.prev_eval = cur_eval;
		cur_eval = st;
		/* now continue from first node in postoned RE */
		PUSH_YES_STATE_GOTO(EVAL_AB, startpoint);
		/* NOTREACHED */
	    }
	    /* logical is 1,   /(?(?{...})X|Y)/ */
	    sw = (bool)SvTRUE(ret);
	    logical = 0;
	    break;
	}

	case EVAL_AB: /* cleanup after a successful (??{A})B */
	    /* note: this is called twice; first after popping B, then A */
	    PL_reg_flags ^= ST.toggle_reg_flags; 
	    ReREFCNT_dec(rex_sv);
	    SETREX(rex_sv,ST.prev_rex);
	    rex = (struct regexp *)SvANY(rex_sv);
	    rexi = RXi_GET(rex);
	    regcpblow(ST.cp);
	    cur_eval = ST.prev_eval;
	    cur_curlyx = ST.prev_curlyx;
	    
	    PL_reglastparen = &rex->lastparen;
	    PL_reglastcloseparen = &rex->lastcloseparen;
	    
	    /* XXXX This is too dramatic a measure... */
	    PL_reg_maxiter = 0;
            if ( nochange_depth )
	        nochange_depth--;
	    sayYES;


	case EVAL_AB_fail: /* unsuccessfully ran A or B in (??{A})B */
	    /* note: this is called twice; first after popping B, then A */
	    PL_reg_flags ^= ST.toggle_reg_flags; 
	    ReREFCNT_dec(rex_sv);
	    SETREX(rex_sv,ST.prev_rex);
	    rex = (struct regexp *)SvANY(rex_sv);
	    rexi = RXi_GET(rex); 
	    PL_reglastparen = &rex->lastparen;
	    PL_reglastcloseparen = &rex->lastcloseparen;

	    PL_reginput = locinput;
	    REGCP_UNWIND(ST.lastcp);
	    regcppop(rex);
	    cur_eval = ST.prev_eval;
	    cur_curlyx = ST.prev_curlyx;
	    /* XXXX This is too dramatic a measure... */
	    PL_reg_maxiter = 0;
	    if ( nochange_depth )
	        nochange_depth--;
	    sayNO_SILENT;
#undef ST

	case OPEN:
	    n = ARG(scan);  /* which paren pair */
	    PL_reg_start_tmp[n] = locinput;
	    if (n > PL_regsize)
		PL_regsize = n;
            lastopen = n;
	    break;
	case CLOSE:
	    n = ARG(scan);  /* which paren pair */
	    PL_regoffs[n].start = PL_reg_start_tmp[n] - PL_bostr;
	    PL_regoffs[n].end = locinput - PL_bostr;
	    /*if (n > PL_regsize)
		PL_regsize = n;*/
	    if (n > *PL_reglastparen)
		*PL_reglastparen = n;
	    *PL_reglastcloseparen = n;
            if (cur_eval && cur_eval->u.eval.close_paren == n) {
	        goto fake_end;
	    }    
	    break;
        case ACCEPT:
            if (ARG(scan)){
                regnode *cursor;
                for (cursor=scan;
                     cursor && OP(cursor)!=END; 
                     cursor=regnext(cursor)) 
                {
                    if ( OP(cursor)==CLOSE ){
                        n = ARG(cursor);
                        if ( n <= lastopen ) {
                            PL_regoffs[n].start
				= PL_reg_start_tmp[n] - PL_bostr;
                            PL_regoffs[n].end = locinput - PL_bostr;
                            /*if (n > PL_regsize)
                            PL_regsize = n;*/
                            if (n > *PL_reglastparen)
                                *PL_reglastparen = n;
                            *PL_reglastcloseparen = n;
                            if ( n == ARG(scan) || (cur_eval &&
                                cur_eval->u.eval.close_paren == n))
                                break;
                        }
                    }
                }
            }
	    goto fake_end;
	    /*NOTREACHED*/	    
	case GROUPP:
	    n = ARG(scan);  /* which paren pair */
	    sw = (bool)(*PL_reglastparen >= n && PL_regoffs[n].end != -1);
	    break;
	case NGROUPP:
	    /* reg_check_named_buff_matched returns 0 for no match */
	    sw = (bool)(0 < reg_check_named_buff_matched(rex,scan));
	    break;
        case INSUBP:
            n = ARG(scan);
            sw = (cur_eval && (!n || cur_eval->u.eval.close_paren == n));
            break;
        case DEFINEP:
            sw = 0;
            break;
	case IFTHEN:
	    PL_reg_leftiter = PL_reg_maxiter;		/* Void cache */
	    if (sw)
		next = NEXTOPER(NEXTOPER(scan));
	    else {
		next = scan + ARG(scan);
		if (OP(next) == IFTHEN) /* Fake one. */
		    next = NEXTOPER(NEXTOPER(next));
	    }
	    break;
	case LOGICAL:
	    logical = scan->flags;
	    break;

/*******************************************************************

The CURLYX/WHILEM pair of ops handle the most generic case of the /A*B/
pattern, where A and B are subpatterns. (For simple A, CURLYM or
STAR/PLUS/CURLY/CURLYN are used instead.)

A*B is compiled as <CURLYX><A><WHILEM><B>

On entry to the subpattern, CURLYX is called. This pushes a CURLYX
state, which contains the current count, initialised to -1. It also sets
cur_curlyx to point to this state, with any previous value saved in the
state block.

CURLYX then jumps straight to the WHILEM op, rather than executing A,
since the pattern may possibly match zero times (i.e. it's a while {} loop
rather than a do {} while loop).

Each entry to WHILEM represents a successful match of A. The count in the
CURLYX block is incremented, another WHILEM state is pushed, and execution
passes to A or B depending on greediness and the current count.

For example, if matching against the string a1a2a3b (where the aN are
substrings that match /A/), then the match progresses as follows: (the
pushed states are interspersed with the bits of strings matched so far):

    <CURLYX cnt=-1>
    <CURLYX cnt=0><WHILEM>
    <CURLYX cnt=1><WHILEM> a1 <WHILEM>
    <CURLYX cnt=2><WHILEM> a1 <WHILEM> a2 <WHILEM>
    <CURLYX cnt=3><WHILEM> a1 <WHILEM> a2 <WHILEM> a3 <WHILEM>
    <CURLYX cnt=3><WHILEM> a1 <WHILEM> a2 <WHILEM> a3 <WHILEM> b

(Contrast this with something like CURLYM, which maintains only a single
backtrack state:

    <CURLYM cnt=0> a1
    a1 <CURLYM cnt=1> a2
    a1 a2 <CURLYM cnt=2> a3
    a1 a2 a3 <CURLYM cnt=3> b
)

Each WHILEM state block marks a point to backtrack to upon partial failure
of A or B, and also contains some minor state data related to that
iteration.  The CURLYX block, pointed to by cur_curlyx, contains the
overall state, such as the count, and pointers to the A and B ops.

This is complicated slightly by nested CURLYX/WHILEM's. Since cur_curlyx
must always point to the *current* CURLYX block, the rules are:

When executing CURLYX, save the old cur_curlyx in the CURLYX state block,
and set cur_curlyx to point the new block.

When popping the CURLYX block after a successful or unsuccessful match,
restore the previous cur_curlyx.

When WHILEM is about to execute B, save the current cur_curlyx, and set it
to the outer one saved in the CURLYX block.

When popping the WHILEM block after a successful or unsuccessful B match,
restore the previous cur_curlyx.

Here's an example for the pattern (AI* BI)*BO
I and O refer to inner and outer, C and W refer to CURLYX and WHILEM:

cur_
curlyx backtrack stack
------ ---------------
NULL   
CO     <CO prev=NULL> <WO>
CI     <CO prev=NULL> <WO> <CI prev=CO> <WI> ai 
CO     <CO prev=NULL> <WO> <CI prev=CO> <WI> ai <WI prev=CI> bi 
NULL   <CO prev=NULL> <WO> <CI prev=CO> <WI> ai <WI prev=CI> bi <WO prev=CO> bo

At this point the pattern succeeds, and we work back down the stack to
clean up, restoring as we go:

CO     <CO prev=NULL> <WO> <CI prev=CO> <WI> ai <WI prev=CI> bi 
CI     <CO prev=NULL> <WO> <CI prev=CO> <WI> ai 
CO     <CO prev=NULL> <WO>
NULL   

*******************************************************************/

#define ST st->u.curlyx

	case CURLYX:    /* start of /A*B/  (for complex A) */
	{
	    /* No need to save/restore up to this paren */
	    I32 parenfloor = scan->flags;
	    
	    assert(next); /* keep Coverity happy */
	    if (OP(PREVOPER(next)) == NOTHING) /* LONGJMP */
		next += ARG(next);

	    /* XXXX Probably it is better to teach regpush to support
	       parenfloor > PL_regsize... */
	    if (parenfloor > (I32)*PL_reglastparen)
		parenfloor = *PL_reglastparen; /* Pessimization... */

	    ST.prev_curlyx= cur_curlyx;
	    cur_curlyx = st;
	    ST.cp = PL_savestack_ix;

	    /* these fields contain the state of the current curly.
	     * they are accessed by subsequent WHILEMs */
	    ST.parenfloor = parenfloor;
	    ST.min = ARG1(scan);
	    ST.max = ARG2(scan);
	    ST.A = NEXTOPER(scan) + EXTRA_STEP_2ARGS;
	    ST.B = next;
	    ST.minmod = minmod;
	    minmod = 0;
	    ST.count = -1;	/* this will be updated by WHILEM */
	    ST.lastloc = NULL;  /* this will be updated by WHILEM */

	    PL_reginput = locinput;
	    PUSH_YES_STATE_GOTO(CURLYX_end, PREVOPER(next));
	    /* NOTREACHED */
	}

	case CURLYX_end: /* just finished matching all of A*B */
	    cur_curlyx = ST.prev_curlyx;
	    sayYES;
	    /* NOTREACHED */

	case CURLYX_end_fail: /* just failed to match all of A*B */
	    regcpblow(ST.cp);
	    cur_curlyx = ST.prev_curlyx;
	    sayNO;
	    /* NOTREACHED */


#undef ST
#define ST st->u.whilem

	case WHILEM:     /* just matched an A in /A*B/  (for complex A) */
	{
	    /* see the discussion above about CURLYX/WHILEM */
	    I32 n;
	    assert(cur_curlyx); /* keep Coverity happy */
	    n = ++cur_curlyx->u.curlyx.count; /* how many A's matched */
	    ST.save_lastloc = cur_curlyx->u.curlyx.lastloc;
	    ST.cache_offset = 0;
	    ST.cache_mask = 0;
	    
	    PL_reginput = locinput;

	    DEBUG_EXECUTE_r( PerlIO_printf(Perl_debug_log,
		  "%*s  whilem: matched %ld out of %ld..%ld\n",
		  REPORT_CODE_OFF+depth*2, "", (long)n,
		  (long)cur_curlyx->u.curlyx.min,
		  (long)cur_curlyx->u.curlyx.max)
	    );

	    /* First just match a string of min A's. */

	    if (n < cur_curlyx->u.curlyx.min) {
		cur_curlyx->u.curlyx.lastloc = locinput;
		PUSH_STATE_GOTO(WHILEM_A_pre, cur_curlyx->u.curlyx.A);
		/* NOTREACHED */
	    }

	    /* If degenerate A matches "", assume A done. */

	    if (locinput == cur_curlyx->u.curlyx.lastloc) {
		DEBUG_EXECUTE_r( PerlIO_printf(Perl_debug_log,
		   "%*s  whilem: empty match detected, trying continuation...\n",
		   REPORT_CODE_OFF+depth*2, "")
		);
		goto do_whilem_B_max;
	    }

	    /* super-linear cache processing */

	    if (scan->flags) {

		if (!PL_reg_maxiter) {
		    /* start the countdown: Postpone detection until we
		     * know the match is not *that* much linear. */
		    PL_reg_maxiter = (PL_regeol - PL_bostr + 1) * (scan->flags>>4);
		    /* possible overflow for long strings and many CURLYX's */
		    if (PL_reg_maxiter < 0)
			PL_reg_maxiter = I32_MAX;
		    PL_reg_leftiter = PL_reg_maxiter;
		}

		if (PL_reg_leftiter-- == 0) {
		    /* initialise cache */
		    const I32 size = (PL_reg_maxiter + 7)/8;
		    if (PL_reg_poscache) {
			if ((I32)PL_reg_poscache_size < size) {
			    Renew(PL_reg_poscache, size, char);
			    PL_reg_poscache_size = size;
			}
			Zero(PL_reg_poscache, size, char);
		    }
		    else {
			PL_reg_poscache_size = size;
			Newxz(PL_reg_poscache, size, char);
		    }
		    DEBUG_EXECUTE_r( PerlIO_printf(Perl_debug_log,
      "%swhilem: Detected a super-linear match, switching on caching%s...\n",
			      PL_colors[4], PL_colors[5])
		    );
		}

		if (PL_reg_leftiter < 0) {
		    /* have we already failed at this position? */
		    I32 offset, mask;
		    offset  = (scan->flags & 0xf) - 1
		  		+ (locinput - PL_bostr)  * (scan->flags>>4);
		    mask    = 1 << (offset % 8);
		    offset /= 8;
		    if (PL_reg_poscache[offset] & mask) {
			DEBUG_EXECUTE_r( PerlIO_printf(Perl_debug_log,
			    "%*s  whilem: (cache) already tried at this position...\n",
			    REPORT_CODE_OFF+depth*2, "")
			);
			sayNO; /* cache records failure */
		    }
		    ST.cache_offset = offset;
		    ST.cache_mask   = mask;
		}
	    }

	    /* Prefer B over A for minimal matching. */

	    if (cur_curlyx->u.curlyx.minmod) {
		ST.save_curlyx = cur_curlyx;
		cur_curlyx = cur_curlyx->u.curlyx.prev_curlyx;
		ST.cp = regcppush(ST.save_curlyx->u.curlyx.parenfloor);
		REGCP_SET(ST.lastcp);
		PUSH_YES_STATE_GOTO(WHILEM_B_min, ST.save_curlyx->u.curlyx.B);
		/* NOTREACHED */
	    }

	    /* Prefer A over B for maximal matching. */

	    if (n < cur_curlyx->u.curlyx.max) { /* More greed allowed? */
		ST.cp = regcppush(cur_curlyx->u.curlyx.parenfloor);
		cur_curlyx->u.curlyx.lastloc = locinput;
		REGCP_SET(ST.lastcp);
		PUSH_STATE_GOTO(WHILEM_A_max, cur_curlyx->u.curlyx.A);
		/* NOTREACHED */
	    }
	    goto do_whilem_B_max;
	}
	/* NOTREACHED */

	case WHILEM_B_min: /* just matched B in a minimal match */
	case WHILEM_B_max: /* just matched B in a maximal match */
	    cur_curlyx = ST.save_curlyx;
	    sayYES;
	    /* NOTREACHED */

	case WHILEM_B_max_fail: /* just failed to match B in a maximal match */
	    cur_curlyx = ST.save_curlyx;
	    cur_curlyx->u.curlyx.lastloc = ST.save_lastloc;
	    cur_curlyx->u.curlyx.count--;
	    CACHEsayNO;
	    /* NOTREACHED */

	case WHILEM_A_min_fail: /* just failed to match A in a minimal match */
	    REGCP_UNWIND(ST.lastcp);
	    regcppop(rex);
	    /* FALL THROUGH */
	case WHILEM_A_pre_fail: /* just failed to match even minimal A */
	    cur_curlyx->u.curlyx.lastloc = ST.save_lastloc;
	    cur_curlyx->u.curlyx.count--;
	    CACHEsayNO;
	    /* NOTREACHED */

	case WHILEM_A_max_fail: /* just failed to match A in a maximal match */
	    REGCP_UNWIND(ST.lastcp);
	    regcppop(rex);	/* Restore some previous $<digit>s? */
	    PL_reginput = locinput;
	    DEBUG_EXECUTE_r(PerlIO_printf(Perl_debug_log,
		"%*s  whilem: failed, trying continuation...\n",
		REPORT_CODE_OFF+depth*2, "")
	    );
	  do_whilem_B_max:
	    if (cur_curlyx->u.curlyx.count >= REG_INFTY
		&& ckWARN(WARN_REGEXP)
		&& !(PL_reg_flags & RF_warned))
	    {
		PL_reg_flags |= RF_warned;
		Perl_warner(aTHX_ packWARN(WARN_REGEXP), "%s limit (%d) exceeded",
		     "Complex regular subexpression recursion",
		     REG_INFTY - 1);
	    }

	    /* now try B */
	    ST.save_curlyx = cur_curlyx;
	    cur_curlyx = cur_curlyx->u.curlyx.prev_curlyx;
	    PUSH_YES_STATE_GOTO(WHILEM_B_max, ST.save_curlyx->u.curlyx.B);
	    /* NOTREACHED */

	case WHILEM_B_min_fail: /* just failed to match B in a minimal match */
	    cur_curlyx = ST.save_curlyx;
	    REGCP_UNWIND(ST.lastcp);
	    regcppop(rex);

	    if (cur_curlyx->u.curlyx.count >= cur_curlyx->u.curlyx.max) {
		/* Maximum greed exceeded */
		if (cur_curlyx->u.curlyx.count >= REG_INFTY
		    && ckWARN(WARN_REGEXP)
		    && !(PL_reg_flags & RF_warned))
		{
		    PL_reg_flags |= RF_warned;
		    Perl_warner(aTHX_ packWARN(WARN_REGEXP),
			"%s limit (%d) exceeded",
			"Complex regular subexpression recursion",
			REG_INFTY - 1);
		}
		cur_curlyx->u.curlyx.count--;
		CACHEsayNO;
	    }

	    DEBUG_EXECUTE_r(PerlIO_printf(Perl_debug_log,
		"%*s  trying longer...\n", REPORT_CODE_OFF+depth*2, "")
	    );
	    /* Try grabbing another A and see if it helps. */
	    PL_reginput = locinput;
	    cur_curlyx->u.curlyx.lastloc = locinput;
	    ST.cp = regcppush(cur_curlyx->u.curlyx.parenfloor);
	    REGCP_SET(ST.lastcp);
	    PUSH_STATE_GOTO(WHILEM_A_min, ST.save_curlyx->u.curlyx.A);
	    /* NOTREACHED */

#undef  ST
#define ST st->u.branch

	case BRANCHJ:	    /*  /(...|A|...)/ with long next pointer */
	    next = scan + ARG(scan);
	    if (next == scan)
		next = NULL;
	    scan = NEXTOPER(scan);
	    /* FALL THROUGH */

	case BRANCH:	    /*  /(...|A|...)/ */
	    scan = NEXTOPER(scan); /* scan now points to inner node */
	    ST.lastparen = *PL_reglastparen;
	    ST.next_branch = next;
	    REGCP_SET(ST.cp);
	    PL_reginput = locinput;

	    /* Now go into the branch */
	    if (has_cutgroup) {
	        PUSH_YES_STATE_GOTO(BRANCH_next, scan);    
	    } else {
	        PUSH_STATE_GOTO(BRANCH_next, scan);
	    }
	    /* NOTREACHED */
        case CUTGROUP:
            PL_reginput = locinput;
            sv_yes_mark = st->u.mark.mark_name = scan->flags ? NULL :
                (SV*)rexi->data->data[ ARG( scan ) ];
            PUSH_STATE_GOTO(CUTGROUP_next,next);
            /* NOTREACHED */
        case CUTGROUP_next_fail:
            do_cutgroup = 1;
            no_final = 1;
            if (st->u.mark.mark_name)
                sv_commit = st->u.mark.mark_name;
            sayNO;	    
            /* NOTREACHED */
        case BRANCH_next:
            sayYES;
            /* NOTREACHED */
	case BRANCH_next_fail: /* that branch failed; try the next, if any */
	    if (do_cutgroup) {
	        do_cutgroup = 0;
	        no_final = 0;
	    }
	    REGCP_UNWIND(ST.cp);
	    for (n = *PL_reglastparen; n > ST.lastparen; n--)
		PL_regoffs[n].end = -1;
	    *PL_reglastparen = n;
	    /*dmq: *PL_reglastcloseparen = n; */
	    scan = ST.next_branch;
	    /* no more branches? */
	    if (!scan || (OP(scan) != BRANCH && OP(scan) != BRANCHJ)) {
	        DEBUG_EXECUTE_r({
		    PerlIO_printf( Perl_debug_log,
			"%*s  %sBRANCH failed...%s\n",
			REPORT_CODE_OFF+depth*2, "", 
			PL_colors[4],
			PL_colors[5] );
		});
		sayNO_SILENT;
            }
	    continue; /* execute next BRANCH[J] op */
	    /* NOTREACHED */
    
	case MINMOD:
	    minmod = 1;
	    break;

#undef  ST
#define ST st->u.curlym

	case CURLYM:	/* /A{m,n}B/ where A is fixed-length */

	    /* This is an optimisation of CURLYX that enables us to push
	     * only a single backtracking state, no matter now many matches
	     * there are in {m,n}. It relies on the pattern being constant
	     * length, with no parens to influence future backrefs
	     */

	    ST.me = scan;
	    scan = NEXTOPER(scan) + NODE_STEP_REGNODE;

	    /* if paren positive, emulate an OPEN/CLOSE around A */
	    if (ST.me->flags) {
		U32 paren = ST.me->flags;
		if (paren > PL_regsize)
		    PL_regsize = paren;
		if (paren > *PL_reglastparen)
		    *PL_reglastparen = paren;
		scan += NEXT_OFF(scan); /* Skip former OPEN. */
	    }
	    ST.A = scan;
	    ST.B = next;
	    ST.alen = 0;
	    ST.count = 0;
	    ST.minmod = minmod;
	    minmod = 0;
	    ST.c1 = CHRTEST_UNINIT;
	    REGCP_SET(ST.cp);

	    if (!(ST.minmod ? ARG1(ST.me) : ARG2(ST.me))) /* min/max */
		goto curlym_do_B;

	  curlym_do_A: /* execute the A in /A{m,n}B/  */
	    PL_reginput = locinput;
	    PUSH_YES_STATE_GOTO(CURLYM_A, ST.A); /* match A */
	    /* NOTREACHED */

	case CURLYM_A: /* we've just matched an A */
	    locinput = st->locinput;
	    nextchr = UCHARAT(locinput);

	    ST.count++;
	    /* after first match, determine A's length: u.curlym.alen */
	    if (ST.count == 1) {
		if (PL_reg_match_utf8) {
		    char *s = locinput;
		    while (s < PL_reginput) {
			ST.alen++;
			s += UTF8SKIP(s);
		    }
		}
		else {
		    ST.alen = PL_reginput - locinput;
		}
		if (ST.alen == 0)
		    ST.count = ST.minmod ? ARG1(ST.me) : ARG2(ST.me);
	    }
	    DEBUG_EXECUTE_r(
		PerlIO_printf(Perl_debug_log,
			  "%*s  CURLYM now matched %"IVdf" times, len=%"IVdf"...\n",
			  (int)(REPORT_CODE_OFF+(depth*2)), "",
			  (IV) ST.count, (IV)ST.alen)
	    );

	    locinput = PL_reginput;
	                
	    if (cur_eval && cur_eval->u.eval.close_paren && 
	        cur_eval->u.eval.close_paren == (U32)ST.me->flags) 
	        goto fake_end;
	        
	    if ( ST.count < (ST.minmod ? ARG1(ST.me) : ARG2(ST.me)) )
		goto curlym_do_A; /* try to match another A */
	    goto curlym_do_B; /* try to match B */

	case CURLYM_A_fail: /* just failed to match an A */
	    REGCP_UNWIND(ST.cp);

	    if (ST.minmod || ST.count < ARG1(ST.me) /* min*/ 
	        || (cur_eval && cur_eval->u.eval.close_paren &&
	            cur_eval->u.eval.close_paren == (U32)ST.me->flags))
		sayNO;

	  curlym_do_B: /* execute the B in /A{m,n}B/  */
	    PL_reginput = locinput;
	    if (ST.c1 == CHRTEST_UNINIT) {
		/* calculate c1 and c2 for possible match of 1st char
		 * following curly */
		ST.c1 = ST.c2 = CHRTEST_VOID;
		if (HAS_TEXT(ST.B) || JUMPABLE(ST.B)) {
		    regnode *text_node = ST.B;
		    if (! HAS_TEXT(text_node))
			FIND_NEXT_IMPT(text_node);
	            /* this used to be 
	                
	                (HAS_TEXT(text_node) && PL_regkind[OP(text_node)] == EXACT)
	                
	            	But the former is redundant in light of the latter.
	            	
	            	if this changes back then the macro for 
	            	IS_TEXT and friends need to change.
	             */
		    if (PL_regkind[OP(text_node)] == EXACT)
		    {
		        
			ST.c1 = *STRING(text_node);
			ST.c2 = ST.c1;
		    }
		}
	    }

	    DEBUG_EXECUTE_r(
		PerlIO_printf(Perl_debug_log,
		    "%*s  CURLYM trying tail with matches=%"IVdf"...\n",
		    (int)(REPORT_CODE_OFF+(depth*2)),
		    "", (IV)ST.count)
		);
	    if (ST.c1 != CHRTEST_VOID
		    && *PL_reginput != ST.c1
		    && *PL_reginput != ST.c2)
	    {
		/* simulate B failing */
		DEBUG_OPTIMISE_r(
		    PerlIO_printf(Perl_debug_log,
		        "%*s  CURLYM Fast bail c1=%"IVdf" c2=%"IVdf"\n",
		        (int)(REPORT_CODE_OFF+(depth*2)),"",
		        (IV)ST.c1,(IV)ST.c2
		));
		state_num = CURLYM_B_fail;
		goto reenter_switch;
	    }

	    if (ST.me->flags) {
		/* mark current A as captured */
		I32 paren = ST.me->flags;
		if (ST.count) {
		    PL_regoffs[paren].start
			= HOPc(PL_reginput, -ST.alen) - PL_bostr;
		    PL_regoffs[paren].end = PL_reginput - PL_bostr;
		    /*dmq: *PL_reglastcloseparen = paren; */
		}
		else
		    PL_regoffs[paren].end = -1;
		if (cur_eval && cur_eval->u.eval.close_paren &&
		    cur_eval->u.eval.close_paren == (U32)ST.me->flags) 
		{
		    if (ST.count) 
	                goto fake_end;
	            else
	                sayNO;
	        }
	    }
	    
	    PUSH_STATE_GOTO(CURLYM_B, ST.B); /* match B */
	    /* NOTREACHED */

	case CURLYM_B_fail: /* just failed to match a B */
	    REGCP_UNWIND(ST.cp);
	    if (ST.minmod) {
		if (ST.count == ARG2(ST.me) /* max */)
		    sayNO;
		goto curlym_do_A; /* try to match a further A */
	    }
	    /* backtrack one A */
	    if (ST.count == ARG1(ST.me) /* min */)
		sayNO;
	    ST.count--;
	    locinput = HOPc(locinput, -ST.alen);
	    goto curlym_do_B; /* try to match B */

#undef ST
#define ST st->u.curly

#define CURLY_SETPAREN(paren, success) \
    if (paren) { \
	if (success) { \
	    PL_regoffs[paren].start = HOPc(locinput, -1) - PL_bostr; \
	    PL_regoffs[paren].end = locinput - PL_bostr; \
	    *PL_reglastcloseparen = paren; \
	} \
	else \
	    PL_regoffs[paren].end = -1; \
    }

	case STAR:		/*  /A*B/ where A is width 1 */
	    ST.paren = 0;
	    ST.min = 0;
	    ST.max = REG_INFTY;
	    scan = NEXTOPER(scan);
	    goto repeat;
	case PLUS:		/*  /A+B/ where A is width 1 */
	    ST.paren = 0;
	    ST.min = 1;
	    ST.max = REG_INFTY;
	    scan = NEXTOPER(scan);
	    goto repeat;
	case CURLYN:		/*  /(A){m,n}B/ where A is width 1 */
	    ST.paren = scan->flags;	/* Which paren to set */
	    if (ST.paren > PL_regsize)
		PL_regsize = ST.paren;
	    if (ST.paren > *PL_reglastparen)
		*PL_reglastparen = ST.paren;
	    ST.min = ARG1(scan);  /* min to match */
	    ST.max = ARG2(scan);  /* max to match */
	    if (cur_eval && cur_eval->u.eval.close_paren &&
	        cur_eval->u.eval.close_paren == (U32)ST.paren) {
	        ST.min=1;
	        ST.max=1;
	    }
            scan = regnext(NEXTOPER(scan) + NODE_STEP_REGNODE);
	    goto repeat;
	case CURLY:		/*  /A{m,n}B/ where A is width 1 */
	    ST.paren = 0;
	    ST.min = ARG1(scan);  /* min to match */
	    ST.max = ARG2(scan);  /* max to match */
	    scan = NEXTOPER(scan) + NODE_STEP_REGNODE;
	  repeat:
	    /*
	    * Lookahead to avoid useless match attempts
	    * when we know what character comes next.
	    *
	    * Used to only do .*x and .*?x, but now it allows
	    * for )'s, ('s and (?{ ... })'s to be in the way
	    * of the quantifier and the EXACT-like node.  -- japhy
	    */

	    if (ST.min > ST.max) /* XXX make this a compile-time check? */
		sayNO;
	    if (HAS_TEXT(next) || JUMPABLE(next)) {
		char *s;
		regnode *text_node = next;

		if (! HAS_TEXT(text_node)) 
		    FIND_NEXT_IMPT(text_node);

		if (! HAS_TEXT(text_node))
		    ST.c1 = ST.c2 = CHRTEST_VOID;
		else {
		    if ( PL_regkind[OP(text_node)] != EXACT ) {
			ST.c1 = ST.c2 = CHRTEST_VOID;
			goto assume_ok_easy;
		    }
		    else
			s = STRING(text_node);
                    
                    /*  Currently we only get here when 
                        
                        PL_rekind[OP(text_node)] == EXACT
                    
                        if this changes back then the macro for IS_TEXT and 
                        friends need to change. */
		    if (!UTF) {
			ST.c2 = ST.c1 = *s;
			if (IS_TEXTF(text_node))
			    ST.c2 = PL_fold[ST.c1];
			else if (IS_TEXTFL(text_node))
			    ST.c2 = PL_fold_locale[ST.c1];
		    }
		    else { /* UTF */
			if (IS_TEXTF(text_node)) {
			     STRLEN ulen1, ulen2;
			     char tmpbuf1[UTF8_MAXBYTES_CASE+1];
			     char tmpbuf2[UTF8_MAXBYTES_CASE+1];

			     to_utf8_lower(s, tmpbuf1, &ulen1);
			     to_utf8_upper(s, tmpbuf2, &ulen2);
#ifdef EBCDIC
			     ST.c1 = utf8n_to_uvchr(tmpbuf1, UTF8_MAXLEN, 0,
						    ckWARN(WARN_UTF8) ?
						    0 | UTF8_CHECK_ONLY : UTF8_ALLOW_ANY | UTF8_CHECK_ONLY);
			     ST.c2 = utf8n_to_uvchr(tmpbuf2, UTF8_MAXLEN, 0,
                                                    ckWARN(WARN_UTF8) ?
                                                    0 | UTF8_CHECK_ONLY : UTF8_ALLOW_ANY | UTF8_CHECK_ONLY);
#else
			     ST.c1 = utf8n_to_uvuni(tmpbuf1, UTF8_MAXBYTES, 0,
						    uniflags);
			     ST.c2 = utf8n_to_uvuni(tmpbuf2, UTF8_MAXBYTES, 0,
						    uniflags);
#endif
			}
			else {
			    ST.c2 = ST.c1 = utf8n_to_uvchr(s, UTF8_MAXBYTES, 0,
						     uniflags);
			}
		    }
		}
	    }
	    else
		ST.c1 = ST.c2 = CHRTEST_VOID;
	assume_ok_easy:

	    ST.A = scan;
	    ST.B = next;
	    PL_reginput = locinput;
	    if (minmod) {
		minmod = 0;
		if (ST.min && regrepeat(rex, ST.A, ST.min, depth) < ST.min)
		    sayNO;
		ST.count = ST.min;
		locinput = PL_reginput;
		REGCP_SET(ST.cp);
		if (ST.c1 == CHRTEST_VOID)
		    goto curly_try_B_min;

		ST.oldloc = locinput;

		/* set ST.maxpos to the furthest point along the
		 * string that could possibly match */
		if  (ST.max == REG_INFTY) {
		    ST.maxpos = PL_regeol - 1;
		    if (do_utf8)
			while (UTF8_IS_CONTINUATION(*ST.maxpos))
			    ST.maxpos--;
		}
		else if (do_utf8) {
		    int m = ST.max - ST.min;
		    for (ST.maxpos = locinput;
			 m >0 && ST.maxpos + UTF8SKIP(ST.maxpos) <= PL_regeol; m--)
			ST.maxpos += UTF8SKIP(ST.maxpos);
		}
		else {
		    ST.maxpos = locinput + ST.max - ST.min;
		    if (ST.maxpos >= PL_regeol)
			ST.maxpos = PL_regeol - 1;
		}
		goto curly_try_B_min_known;

	    }
	    else {
		ST.count = regrepeat(rex, ST.A, ST.max, depth);
		locinput = PL_reginput;
		if (ST.count < ST.min)
		    sayNO;
		if ((ST.count > ST.min)
		    && (PL_regkind[OP(ST.B)] == EOL) && (OP(ST.B) != MEOL))
		{
		    /* A{m,n} must come at the end of the string, there's
		     * no point in backing off ... */
		    ST.min = ST.count;
		    /* ...except that $ and \Z can match before *and* after
		       newline at the end.  Consider "\n\n" =~ /\n+\Z\n/.
		       We may back off by one in this case. */
		    if (UCHARAT(PL_reginput - 1) == '\n' && OP(ST.B) != EOS)
			ST.min--;
		}
		REGCP_SET(ST.cp);
		goto curly_try_B_max;
	    }
	    /* NOTREACHED */


	case CURLY_B_min_known_fail:
	    /* failed to find B in a non-greedy match where c1,c2 valid */
	    if (ST.paren && ST.count)
		PL_regoffs[ST.paren].end = -1;

	    PL_reginput = locinput;	/* Could be reset... */
	    REGCP_UNWIND(ST.cp);
	    /* Couldn't or didn't -- move forward. */
	    ST.oldloc = locinput;
	    if (do_utf8)
		locinput += UTF8SKIP(locinput);
	    else
		locinput++;
	    ST.count++;
	  curly_try_B_min_known:
	     /* find the next place where 'B' could work, then call B */
	    {
		int n;
		if (do_utf8) {
		    n = (ST.oldloc == locinput) ? 0 : 1;
		    if (ST.c1 == ST.c2) {
			STRLEN len;
			/* set n to utf8_distance(oldloc, locinput) */
			while (locinput <= ST.maxpos &&
			       utf8n_to_uvchr(locinput,
					      UTF8_MAXBYTES, &len,
					      uniflags) != (UV)ST.c1) {
			    locinput += len;
			    n++;
			}
		    }
		    else {
			/* set n to utf8_distance(oldloc, locinput) */
			while (locinput <= ST.maxpos) {
			    STRLEN len;
			    const UV c = utf8n_to_uvchr(locinput,
						  UTF8_MAXBYTES, &len,
						  uniflags);
			    if (c == (UV)ST.c1 || c == (UV)ST.c2)
				break;
			    locinput += len;
			    n++;
			}
		    }
		}
		else {
		    if (ST.c1 == ST.c2) {
			while (locinput <= ST.maxpos &&
			       *locinput != ST.c1)
			    locinput++;
		    }
		    else {
			while (locinput <= ST.maxpos
			       && *locinput != ST.c1
			       && *locinput != ST.c2)
			    locinput++;
		    }
		    n = locinput - ST.oldloc;
		}
		if (locinput > ST.maxpos)
		    sayNO;
		/* PL_reginput == oldloc now */
		if (n) {
		    ST.count += n;
		    if (regrepeat(rex, ST.A, n, depth) < n)
			sayNO;
		}
		PL_reginput = locinput;
		CURLY_SETPAREN(ST.paren, ST.count);
		if (cur_eval && cur_eval->u.eval.close_paren && 
		    cur_eval->u.eval.close_paren == (U32)ST.paren) {
		    goto fake_end;
	        }
		PUSH_STATE_GOTO(CURLY_B_min_known, ST.B);
	    }
	    /* NOTREACHED */


	case CURLY_B_min_fail:
	    /* failed to find B in a non-greedy match where c1,c2 invalid */
	    if (ST.paren && ST.count)
		PL_regoffs[ST.paren].end = -1;

	    REGCP_UNWIND(ST.cp);
	    /* failed -- move forward one */
	    PL_reginput = locinput;
	    if (regrepeat(rex, ST.A, 1, depth)) {
		ST.count++;
		locinput = PL_reginput;
		if (ST.count <= ST.max || (ST.max == REG_INFTY &&
			ST.count > 0)) /* count overflow ? */
		{
		  curly_try_B_min:
		    CURLY_SETPAREN(ST.paren, ST.count);
		    if (cur_eval && cur_eval->u.eval.close_paren &&
		        cur_eval->u.eval.close_paren == (U32)ST.paren) {
                        goto fake_end;
                    }
		    PUSH_STATE_GOTO(CURLY_B_min, ST.B);
		}
	    }
	    sayNO;
	    /* NOTREACHED */


	curly_try_B_max:
	    /* a successful greedy match: now try to match B */
            if (cur_eval && cur_eval->u.eval.close_paren &&
                cur_eval->u.eval.close_paren == (U32)ST.paren) {
                goto fake_end;
            }
	    {
		UV c = 0;
		if (ST.c1 != CHRTEST_VOID)
		    c = do_utf8 ? utf8n_to_uvchr(PL_reginput,
					   UTF8_MAXBYTES, 0, uniflags)
				: (UV) UCHARAT(PL_reginput);
		/* If it could work, try it. */
		if (ST.c1 == CHRTEST_VOID || c == (UV)ST.c1 || c == (UV)ST.c2) {
		    CURLY_SETPAREN(ST.paren, ST.count);
		    PUSH_STATE_GOTO(CURLY_B_max, ST.B);
		    /* NOTREACHED */
		}
	    }
	    /* FALL THROUGH */
	case CURLY_B_max_fail:
	    /* failed to find B in a greedy match */
	    if (ST.paren && ST.count)
		PL_regoffs[ST.paren].end = -1;

	    REGCP_UNWIND(ST.cp);
	    /*  back up. */
	    if (--ST.count < ST.min)
		sayNO;
	    PL_reginput = locinput = HOPc(locinput, -1);
	    goto curly_try_B_max;

#undef ST

	case END:
	    fake_end:
	    if (cur_eval) {
		/* we've just finished A in /(??{A})B/; now continue with B */
		I32 tmpix;
		st->u.eval.toggle_reg_flags
			    = cur_eval->u.eval.toggle_reg_flags;
		PL_reg_flags ^= st->u.eval.toggle_reg_flags; 

		st->u.eval.prev_rex = rex_sv;		/* inner */
		SETREX(rex_sv,cur_eval->u.eval.prev_rex);
		rex = (struct regexp *)SvANY(rex_sv);
		rexi = RXi_GET(rex);
		cur_curlyx = cur_eval->u.eval.prev_curlyx;
		ReREFCNT_inc(rex_sv);
		st->u.eval.cp = regcppush(0);	/* Save *all* the positions. */
		REGCP_SET(st->u.eval.lastcp);
		PL_reginput = locinput;

		/* Restore parens of the outer rex without popping the
		 * savestack */
		tmpix = PL_savestack_ix;
		PL_savestack_ix = cur_eval->u.eval.lastcp;
		regcppop(rex);
		PL_savestack_ix = tmpix;

		st->u.eval.prev_eval = cur_eval;
		cur_eval = cur_eval->u.eval.prev_eval;
		DEBUG_EXECUTE_r(
		    PerlIO_printf(Perl_debug_log, "%*s  EVAL trying tail ... %"UVxf"\n",
				      REPORT_CODE_OFF+depth*2, "",PTR2UV(cur_eval)););
                if ( nochange_depth )
	            nochange_depth--;

                PUSH_YES_STATE_GOTO(EVAL_AB,
			st->u.eval.prev_eval->u.eval.B); /* match B */
	    }

	    if (locinput < reginfo->till) {
		DEBUG_EXECUTE_r(PerlIO_printf(Perl_debug_log,
				      "%sMatch possible, but length=%ld is smaller than requested=%ld, failing!%s\n",
				      PL_colors[4],
				      (long)(locinput - PL_reg_starttry),
				      (long)(reginfo->till - PL_reg_starttry),
				      PL_colors[5]));
               				      
		sayNO_SILENT;		/* Cannot match: too short. */
	    }
	    PL_reginput = locinput;	/* put where regtry can find it */
	    sayYES;			/* Success! */

	case SUCCEED: /* successful SUSPEND/UNLESSM/IFMATCH/CURLYM */
	    DEBUG_EXECUTE_r(
	    PerlIO_printf(Perl_debug_log,
		"%*s  %ssubpattern success...%s\n",
		REPORT_CODE_OFF+depth*2, "", PL_colors[4], PL_colors[5]));
	    PL_reginput = locinput;	/* put where regtry can find it */
	    sayYES;			/* Success! */

#undef  ST
#define ST st->u.ifmatch

	case SUSPEND:	/* (?>A) */
	    ST.wanted = 1;
	    PL_reginput = locinput;
	    goto do_ifmatch;	

	case UNLESSM:	/* -ve lookaround: (?!A), or with flags, (?<!A) */
	    ST.wanted = 0;
	    goto ifmatch_trivial_fail_test;

	case IFMATCH:	/* +ve lookaround: (?=A), or with flags, (?<=A) */
	    ST.wanted = 1;
	  ifmatch_trivial_fail_test:
	    if (scan->flags) {
		char * const s = HOPBACKc(locinput, scan->flags);
		if (!s) {
		    /* trivial fail */
		    if (logical) {
			logical = 0;
			sw = 1 - (bool)ST.wanted;
		    }
		    else if (ST.wanted)
			sayNO;
		    next = scan + ARG(scan);
		    if (next == scan)
			next = NULL;
		    break;
		}
		PL_reginput = s;
	    }
	    else
		PL_reginput = locinput;

	  do_ifmatch:
	    ST.me = scan;
	    ST.logical = logical;
	    /* execute body of (?...A) */
	    PUSH_YES_STATE_GOTO(IFMATCH_A, NEXTOPER(NEXTOPER(scan)));
	    /* NOTREACHED */

	case IFMATCH_A_fail: /* body of (?...A) failed */
	    ST.wanted = !ST.wanted;
	    /* FALL THROUGH */

	case IFMATCH_A: /* body of (?...A) succeeded */
	    if (ST.logical) {
		sw = (bool)ST.wanted;
	    }
	    else if (!ST.wanted)
		sayNO;

	    if (OP(ST.me) == SUSPEND)
		locinput = PL_reginput;
	    else {
		locinput = PL_reginput = st->locinput;
		nextchr = UCHARAT(locinput);
	    }
	    scan = ST.me + ARG(ST.me);
	    if (scan == ST.me)
		scan = NULL;
	    continue; /* execute B */

#undef ST

	case LONGJMP:
	    next = scan + ARG(scan);
	    if (next == scan)
		next = NULL;
	    break;
	case COMMIT:
	    reginfo->cutpoint = PL_regeol;
	    /* FALLTHROUGH */
	case PRUNE:
	    PL_reginput = locinput;
	    if (!scan->flags)
	        sv_yes_mark = sv_commit = (SV*)rexi->data->data[ ARG( scan ) ];
	    PUSH_STATE_GOTO(COMMIT_next,next);
	    /* NOTREACHED */
	case COMMIT_next_fail:
	    no_final = 1;    
	    /* FALLTHROUGH */	    
	case OPFAIL:
	    sayNO;
	    /* NOTREACHED */

#define ST st->u.mark
        case MARKPOINT:
            ST.prev_mark = mark_state;
            ST.mark_name = sv_commit = sv_yes_mark 
                = (SV*)rexi->data->data[ ARG( scan ) ];
            mark_state = st;
            ST.mark_loc = PL_reginput = locinput;
            PUSH_YES_STATE_GOTO(MARKPOINT_next,next);
            /* NOTREACHED */
        case MARKPOINT_next:
            mark_state = ST.prev_mark;
            sayYES;
            /* NOTREACHED */
        case MARKPOINT_next_fail:
            if (popmark && sv_eq(ST.mark_name,popmark)) 
            {
                if (ST.mark_loc > startpoint)
	            reginfo->cutpoint = HOPBACKc(ST.mark_loc, 1);
                popmark = NULL; /* we found our mark */
                sv_commit = ST.mark_name;

                DEBUG_EXECUTE_r({
                        PerlIO_printf(Perl_debug_log,
		            "%*s  %ssetting cutpoint to mark:%"SVf"...%s\n",
		            REPORT_CODE_OFF+depth*2, "", 
		            PL_colors[4], SVfARG(sv_commit), PL_colors[5]);
		});
            }
            mark_state = ST.prev_mark;
            sv_yes_mark = mark_state ? 
                mark_state->u.mark.mark_name : NULL;
            sayNO;
            /* NOTREACHED */
        case SKIP:
            PL_reginput = locinput;
            if (scan->flags) {
                /* (*SKIP) : if we fail we cut here*/
                ST.mark_name = NULL;
                ST.mark_loc = locinput;
                PUSH_STATE_GOTO(SKIP_next,next);    
            } else {
                /* (*SKIP:NAME) : if there is a (*MARK:NAME) fail where it was, 
                   otherwise do nothing.  Meaning we need to scan 
                 */
                regmatch_state *cur = mark_state;
                SV *find = (SV*)rexi->data->data[ ARG( scan ) ];
                
                while (cur) {
                    if ( sv_eq( cur->u.mark.mark_name, 
                                find ) ) 
                    {
                        ST.mark_name = find;
                        PUSH_STATE_GOTO( SKIP_next, next );
                    }
                    cur = cur->u.mark.prev_mark;
                }
            }    
            /* Didn't find our (*MARK:NAME) so ignore this (*SKIP:NAME) */
            break;    
	case SKIP_next_fail:
	    if (ST.mark_name) {
	        /* (*CUT:NAME) - Set up to search for the name as we 
	           collapse the stack*/
	        popmark = ST.mark_name;	   
	    } else {
	        /* (*CUT) - No name, we cut here.*/
	        if (ST.mark_loc > startpoint)
	            reginfo->cutpoint = HOPBACKc(ST.mark_loc, 1);
	        /* but we set sv_commit to latest mark_name if there
	           is one so they can test to see how things lead to this
	           cut */    
                if (mark_state) 
                    sv_commit=mark_state->u.mark.mark_name;	            
            } 
            no_final = 1; 
            sayNO;
            /* NOTREACHED */
#undef ST
        case LNBREAK:
            if ((n=is_LNBREAK(locinput,do_utf8))) {
                locinput += n;
                nextchr = UCHARAT(locinput);
            } else
                sayNO;
            break;

	default:
	    PerlIO_printf(Perl_error_log, "%"UVxf" %d\n",
			  PTR2UV(scan), OP(scan));
	    Perl_croak(aTHX_ "regexp memory corruption");
	    
	} /* end switch */ 

        /* switch break jumps here */
	scan = next; /* prepare to execute the next op and ... */
	continue;    /* ... jump back to the top, reusing st */
	/* NOTREACHED */

      push_yes_state:
	/* push a state that backtracks on success */
	st->u.yes.prev_yes_state = yes_state;
	yes_state = st;
	/* FALL THROUGH */
      push_state:
	/* push a new regex state, then continue at scan  */
	{
	    regmatch_state *newst;

	    DEBUG_STACK_r({
	        regmatch_state *cur = st;
	        regmatch_state *curyes = yes_state;
	        int curd = depth;
	        regmatch_slab *slab = PL_regmatch_slab;
                for (;curd > -1;cur--,curd--) {
                    if (cur < SLAB_FIRST(slab)) {
                	slab = slab->prev;
                	cur = SLAB_LAST(slab);
                    }
                    PerlIO_printf(Perl_error_log, "%*s#%-3d %-10s %s\n",
                        REPORT_CODE_OFF + 2 + depth * 2,"",
                        curd, PL_reg_name[cur->resume_state],
                        (curyes == cur) ? "yes" : ""
                    );
                    if (curyes == cur)
	                curyes = cur->u.yes.prev_yes_state;
                }
            } else 
                DEBUG_STATE_pp("push")
            );
	    depth++;
	    st->locinput = locinput;
	    newst = st+1; 
	    if (newst >  SLAB_LAST(PL_regmatch_slab))
		newst = S_push_slab(aTHX);
	    PL_regmatch_state = newst;

	    locinput = PL_reginput;
	    nextchr = UCHARAT(locinput);
	    st = newst;
	    continue;
	    /* NOTREACHED */
	}
    }

    /*
    * We get here only if there's trouble -- normally "case END" is
    * the terminating point.
    */
    Perl_croak(aTHX_ "corrupted regexp pointers");
    /*NOTREACHED*/
    sayNO;

yes:
    if (yes_state) {
	/* we have successfully completed a subexpression, but we must now
	 * pop to the state marked by yes_state and continue from there */
	assert(st != yes_state);
#ifdef DEBUGGING
	while (st != yes_state) {
	    st--;
	    if (st < SLAB_FIRST(PL_regmatch_slab)) {
		PL_regmatch_slab = PL_regmatch_slab->prev;
		st = SLAB_LAST(PL_regmatch_slab);
	    }
	    DEBUG_STATE_r({
	        if (no_final) {
	            DEBUG_STATE_pp("pop (no final)");        
	        } else {
	            DEBUG_STATE_pp("pop (yes)");
	        }
	    });
	    depth--;
	}
#else
	while (yes_state < SLAB_FIRST(PL_regmatch_slab)
	    || yes_state > SLAB_LAST(PL_regmatch_slab))
	{
	    /* not in this slab, pop slab */
	    depth -= (st - SLAB_FIRST(PL_regmatch_slab) + 1);
	    PL_regmatch_slab = PL_regmatch_slab->prev;
	    st = SLAB_LAST(PL_regmatch_slab);
	}
	depth -= (st - yes_state);
#endif
	st = yes_state;
	yes_state = st->u.yes.prev_yes_state;
	PL_regmatch_state = st;
        
        if (no_final) {
            locinput= st->locinput;
            nextchr = UCHARAT(locinput);
        }
	state_num = st->resume_state + no_final;
	goto reenter_switch;
    }

    DEBUG_EXECUTE_r(PerlIO_printf(Perl_debug_log, "%sMatch successful!%s\n",
			  PL_colors[4], PL_colors[5]));

    result = 1;
    goto final_exit;

no:
    DEBUG_EXECUTE_r(
	PerlIO_printf(Perl_debug_log,
            "%*s  %sfailed...%s\n",
            REPORT_CODE_OFF+depth*2, "", 
            PL_colors[4], PL_colors[5])
	);

no_silent:
    if (no_final) {
        if (yes_state) {
            goto yes;
        } else {
            goto final_exit;
        }
    }    
    if (depth) {
	/* there's a previous state to backtrack to */
	st--;
	if (st < SLAB_FIRST(PL_regmatch_slab)) {
	    PL_regmatch_slab = PL_regmatch_slab->prev;
	    st = SLAB_LAST(PL_regmatch_slab);
	}
	PL_regmatch_state = st;
	locinput= st->locinput;
	nextchr = UCHARAT(locinput);

	DEBUG_STATE_pp("pop");
	depth--;
	if (yes_state == st)
	    yes_state = st->u.yes.prev_yes_state;

	state_num = st->resume_state + 1; /* failure = success + 1 */
	goto reenter_switch;
    }
    result = 0;

  final_exit:
    if (rex->intflags & PREGf_VERBARG_SEEN) {
        SV *sv_err = get_sv("REGERROR", 1);
        SV *sv_mrk = get_sv("REGMARK", 1);
        if (result) {
            sv_commit = &PL_sv_no;
            if (!sv_yes_mark) 
                sv_yes_mark = &PL_sv_yes;
        } else {
            if (!sv_commit) 
                sv_commit = &PL_sv_yes;
            sv_yes_mark = &PL_sv_no;
        }
        sv_setsv(sv_err, sv_commit);
        sv_setsv(sv_mrk, sv_yes_mark);
    }

    /* clean up; in particular, free all slabs above current one */
    LEAVE_SCOPE(oldsave);

    return result;
}

/*
 - regrepeat - repeatedly match something simple, report how many
 */
/*
 * [This routine now assumes that it will only match on things of length 1.
 * That was true before, but now we assume scan - reginput is the count,
 * rather than incrementing count on every character.  [Er, except utf8.]]
 */
STATIC I32
S_regrepeat(pTHX_ const regexp *prog, const regnode *p, I32 max, int depth)
{
    dVAR;
    register char *scan;
    register I32 c;
    register char *loceol = PL_regeol;
    register I32 hardcount = 0;
    register const bool do_utf8 = (prog->extflags & RXf_PMf_UTF8) != 0;
#ifndef DEBUGGING
    PERL_UNUSED_ARG(depth);
#endif

    PERL_ARGS_ASSERT_REGREPEAT;

    scan = PL_reginput;
    if (max == REG_INFTY)
	max = I32_MAX;
    else if (max < loceol - scan)
	loceol = scan + max;
    switch (OP(p)) {
    case REG_ANY:
	while (scan < loceol && *scan != '\n')
	    scan++;
	break;
    case REG_ANYU:
	loceol = PL_regeol;
	while (scan < loceol && hardcount < max && *scan != '\n') {
	    scan += UTF8SKIP(scan);
	    hardcount++;
	}
	break;
    case SANY:
        if (do_utf8) {
	    loceol = PL_regeol;
	    while (scan < loceol && hardcount < max) {
	        scan += UTF8SKIP(scan);
		hardcount++;
	    }
	}
	else
	    scan = loceol;
	break;
    case CANY:
	scan = loceol;
	break;
    case EXACT:		/* length of string is 1 */
    {
	char ch = *STRING(p);
	while (scan < loceol && *scan == ch)
	    scan++;
	break;
    }
    case ANYOFU:
	loceol = PL_regeol;
	while (hardcount < max && scan < loceol &&
	       reginclass(prog, p, scan, 0)) {
	    scan += UTF8SKIP(scan);
	    hardcount++;
	}
	break;
    case ANYOF:
	while (scan < loceol && REGINCLASS(prog, p, scan))
	    scan++;
	break;
    case LNBREAK:
        if (do_utf8) {
	    loceol = PL_regeol;
	    while (hardcount < max && scan < loceol && (c=is_LNBREAK_utf8(scan))) {
		scan += c;
		hardcount++;
	    }
	} else {
	    /*
	      LNBREAK can match two latin chars, which is ok,
	      because we have a null terminated string, but we
	      have to use hardcount in this situation
	    */
	    while (scan < loceol && (c=is_LNBREAK_latin1(scan)))  {
		scan+=c;
		hardcount++;
	    }
	}	
	break;

    default:		/* Called on something of 0 width. */
	break;		/* So match right here or not at all. */
    }

    if (hardcount)
	c = hardcount;
    else
	c = scan - PL_reginput;
    PL_reginput = scan;

    DEBUG_r({
	GET_RE_DEBUG_FLAGS_DECL;
	DEBUG_EXECUTE_r({
	    SV * const prop = sv_newmortal();
	    regprop(prog, prop, p);
	    PerlIO_printf(Perl_debug_log,
			"%*s  %s can match %"IVdf" times out of %"IVdf"...\n",
			REPORT_CODE_OFF + depth*2, "", SvPVX_const(prop),(IV)c,(IV)max);
	});
    });

    return(c);
}


/*
 - reginclass - determine if a character falls into a character class
 
  The n is the ANYOF regnode, the p is the target string, lenp
  is pointer to the maximum length of how far to go in the p
  (if the lenp is zero, UTF8SKIP(p) is used),

 */

STATIC bool
S_reginclass(pTHX_ const regexp *prog, register const regnode *n, register const char* p, STRLEN* lenp)
{
    dVAR;
    const char flags = ANYOF_FLAGS(n);
    bool match = FALSE;
    UV c = *p;
    STRLEN len = 0;
    STRLEN plen;

    RXi_GET_DECL(prog, progi);
    GET_RE_DEBUG_FLAGS_DECL;
    PERL_ARGS_ASSERT_REGINCLASS;

    if ((flags & ANYOF_UNICODE) && (!UTF8_IS_INVARIANT(c))) {
	c = utf8n_to_uvchr(p, UTF8_MAXBYTES, &len,
		(UTF8_ALLOW_DEFAULT & UTF8_ALLOW_ANYUV) | UTF8_CHECK_ONLY);
		/* see [perl #37836] for UTF8_ALLOW_ANYUV */
	if (len == (STRLEN)-1) {
/*  	   Perl_croak(aTHX_ "Malformed UTF-8 character (fatal)");  */
	    if (lenp)
		*lenp = 1;
	    return FALSE;
	}
    }

    plen = lenp ? *lenp : UNISKIP(NATIVE_TO_UNI(c));
    if (flags & ANYOF_UNICODE) {
        if (lenp)
	    *lenp = 0;
	if (c < 256) {
	    return ANYOF_BITMAP_TEST(n, c) ? TRUE : FALSE;
	}
	else if ((flags & ANYOF_UNICODE_ALL) && c >= 256)
	    match = TRUE;
	else {
	    /* get the swash */
	    const U32 arg_n = ARG(n);
	    SV * const rv = (SV*)progi->data->data[arg_n];
	    AV * const av = (AV*)SvRV((SV*)rv);

	    SV **const ary = AvARRAY(av);
	    SV * const sw = ary[1];

	    if (sw) {
		if (swash_fetch(sw, p, 1))
		    match = TRUE;
		else if (flags & ANYOF_FOLD) {
		    AV** const unicode_alternate = (AV**) av_fetch(av, 2, FALSE);
		    if (!match && lenp 
			&& unicode_alternate && SvAVOK(*unicode_alternate)) {
		        I32 i;
			for (i = 0; i <= av_len(*unicode_alternate); i++) {
			    SV* const sv = *av_fetch(*unicode_alternate, i, FALSE);
			    STRLEN len;
			    const char * const s = SvPV_const(sv, len);
			    if (len <= plen && memEQ(s, (char*)p, len)) {
			        *lenp = len;
				match = TRUE;
				break;
			    }
			}
		    }
		    if (!match) {
		        char tmpbuf[UTF8_MAXBYTES_CASE+1];
			STRLEN tmplen;

		        to_utf8_fold(p, tmpbuf, &tmplen);
			if (swash_fetch(sw, tmpbuf, 1))
			    match = TRUE;
		    }
		}
	    }
 	    else
		Perl_croak(aTHX_ "Swash not found");
	}
	if (match && lenp && *lenp == 0)
	    *lenp = UNISKIP(NATIVE_TO_UNI(c));
    }
    else {
	return ANYOF_BITMAP_TEST(n, c) ? TRUE : FALSE;
    }

    DEBUG_EXECUTE_r({
    PerlIO_printf(Perl_debug_log, "reg in class %d %d\n", match, (flags & ANYOF_INVERT));
    });
    return (flags & ANYOF_INVERT) ? !match : match;
}

STATIC char*
S_reghop3(char *s, I32 off, char* lim)
{
    PERL_ARGS_ASSERT_REGHOP3;
    return s + off;
    if (off >= 0) {
	return s + off > lim ? lim : s + off;
    }
    else {
	return s + off < lim ? lim : s + off;
    }
}

STATIC char *
S_reghop3c(char *s, I32 off, char* lim)
{
    dVAR;

    PERL_ARGS_ASSERT_REGHOP3C;

    if (off >= 0) {
	while (off-- && s < lim) {
	    /* XXX could check well-formedness here */
	    s += UTF8SKIP(s);
	}
    }
    else {
        while (off++ && s > lim) {
            s--;
            if (UTF8_IS_CONTINUED(*s)) {
                while (s > lim && UTF8_IS_CONTINUATION(*s))
                    s--;
	    }
            /* XXX could check well-formedness here */
	}
    }
    return s;
}

STATIC char *
S_reghop4(char *s, I32 off, char* llim, char* rlim)
{
    PERL_ARGS_ASSERT_REGHOP4;
    return (off >= 0)
	? s + off > rlim ? rlim : s + off
	: s + off < llim ? llim : s + off
	      ;
}

STATIC char *
S_reghopmaybe3(char* s, I32 off, const char* lim)
{
    PERL_ARGS_ASSERT_REGHOPMAYBE3;
    if (off >= 0) {
	return s + off > lim ? NULL : s + off;
    }
    else {
	return s + off < lim ? NULL : s + off;
    }
}

static void
restore_pos(pTHX_ void *arg)
{
    dVAR;
    regexp * const rex = (regexp *)arg;
    if (PL_reg_eval_set) {
	if (PL_reg_oldsaved) {
	    rex->subbeg = PL_reg_oldsaved;
	    rex->sublen = PL_reg_oldsavedlen;
#ifdef PERL_OLD_COPY_ON_WRITE
	    rex->saved_copy = PL_nrs;
#endif
	    RXp_MATCH_COPIED_on(rex);
	}
	PL_reg_magic->mg_len = PL_reg_oldpos;
	PL_reg_eval_set = 0;
	PL_curpm = PL_reg_oldcurpm;
    }	
}

/*
 * Local variables:
 * c-indentation-style: bsd
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 *
 * ex: set ts=8 sts=4 sw=4 noet:
 */
