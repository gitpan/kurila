/*    mg.c
 *
 *    Copyright (C) 1991, 1992, 1993, 1994, 1995, 1996, 1997, 1998, 1999,
 *    2000, 2001, 2002, 2003, 2004, 2005, 2006, 2007, by Larry Wall and others
 *
 *    You may distribute under the terms of either the GNU General Public
 *    License or the Artistic License, as specified in the README file.
 *
 */

/*
 * "Sam sat on the ground and put his head in his hands.  'I wish I had never
 * come here, and I don't want to see no more magic,' he said, and fell silent."
 */

/*
=head1 Magical Functions

"Magic" is special data attached to SV structures in order to give them
"magical" properties.  When any Perl code tries to read from, or assign to,
an SV marked as magical, it calls the 'get' or 'set' function associated
with that SV's magic. A get is called prior to reading an SV, in order to
give it a chance to update its internal value (get on $. writes the line
number of the last read filehandle into to the SV's IV slot), while
set is called after an SV has been written to, in order to allow it to make
use of its changed value (set on $/ copies the SV's new value to the
PL_rs global variable).

Magic is implemented as a linked list of MAGIC structures attached to the
SV. Each MAGIC struct holds the type of the magic, a pointer to an array
of functions that implement the get(), set(), length() etc functions,
plus space for some flags and pointers. For example, a tied variable has
a MAGIC structure that contains a pointer to the object associated with the
tie.

*/

#include "EXTERN.h"
#define PERL_IN_MG_C
#include "perl.h"

#if defined(HAS_GETGROUPS) || defined(HAS_SETGROUPS)
#  ifdef I_GRP
#    include <grp.h>
#  endif
#endif

#if defined(HAS_SETGROUPS)
#  ifndef NGROUPS
#    define NGROUPS 32
#  endif
#endif

#ifdef __hpux
#  include <sys/pstat.h>
#endif

#if defined(HAS_SIGACTION) && defined(SA_SIGINFO)
Signal_t Perl_csighandler(int sig, siginfo_t *, void *);
#else
Signal_t Perl_csighandler(int sig);
#endif

#ifdef __Lynx__
/* Missing protos on LynxOS */
void setruid(uid_t id);
void seteuid(uid_t id);
void setrgid(uid_t id);
void setegid(uid_t id);
#endif

/*
 * Use the "DESTRUCTOR" scope cleanup to reinstate magic.
 */

struct magic_state {
    SV* mgs_sv;
    U32 mgs_flags;
    I32 mgs_ss_ix;
};
/* MGS is typedef'ed to struct magic_state in perl.h */

STATIC void
S_save_magic(pTHX_ I32 mgs_ix, SV *sv)
{
    dVAR;
    MGS* mgs;

    PERL_ARGS_ASSERT_SAVE_MAGIC;

    assert(SvMAGICAL(sv));
    /* Turning READONLY off for a copy-on-write scalar (including shared
       hash keys) is a bad idea.  */
    if (SvIsCOW(sv))
      sv_force_normal_flags(sv, 0);

    SAVEDESTRUCTOR_X(S_restore_magic, INT2PTR(void*, (IV)mgs_ix));

    mgs = SSPTR(mgs_ix, MGS*);
    mgs->mgs_sv = sv;
    mgs->mgs_flags = SvMAGICAL(sv) | SvREADONLY(sv);
    mgs->mgs_ss_ix = PL_savestack_ix;   /* points after the saved destructor */

    SvMAGICAL_off(sv);
    SvREADONLY_off(sv);
    if (!(SvFLAGS(sv) & (SVf_IOK|SVf_NOK|SVf_POK))) {
	/* No public flags are set, so promote any private flags to public.  */
	SvFLAGS(sv) |= (SvFLAGS(sv) & (SVp_IOK|SVp_NOK|SVp_POK)) >> PRIVSHIFT;
    }
}

/*
=for apidoc mg_magical

Turns on the magical status of an SV.  See C<sv_magic>.

=cut
*/

void
Perl_mg_magical(pTHX_ SV *sv)
{
    const MAGIC* mg;
    PERL_ARGS_ASSERT_MG_MAGICAL;
    PERL_UNUSED_CONTEXT;
    if ((mg = SvMAGIC(sv))) {
	SvRMAGICAL_off(sv);
	do {
	    const MGVTBL* const vtbl = mg->mg_virtual;
	    if (vtbl) {
		if (vtbl->svt_get && !(mg->mg_flags & MGf_GSKIP))
		    SvGMAGICAL_on(sv);
		if (vtbl->svt_set)
		    SvSMAGICAL_on(sv);
		if (vtbl->svt_clear)
		    SvRMAGICAL_on(sv);
	    }
	} while ((mg = mg->mg_moremagic));
	if (!(SvFLAGS(sv) & (SVs_GMG|SVs_SMG)))
	    SvRMAGICAL_on(sv);
    }
}


/* is this container magic (%ENV, $1 etc), or value magic (pos, taint etc)? */

STATIC bool
S_is_container_magic(const MAGIC *mg)
{
    assert(mg);
    switch (mg->mg_type) {
    case PERL_MAGIC_bm:
    case PERL_MAGIC_fm:
    case PERL_MAGIC_regex_global:
    case PERL_MAGIC_qr:
    case PERL_MAGIC_taint:
    case PERL_MAGIC_vec:
    case PERL_MAGIC_vstring:
    case PERL_MAGIC_utf8:
    case PERL_MAGIC_defelem:
    case PERL_MAGIC_pos:
    case PERL_MAGIC_backref:
    case PERL_MAGIC_rhash:
    case PERL_MAGIC_symtab:
	return 0;
    default:
	return 1;
    }
}

/*
=for apidoc mg_get

Do magic after a value is retrieved from the SV.  See C<sv_magic>.

=cut
*/

int
Perl_mg_get(pTHX_ SV *sv)
{
    dVAR;
    const I32 mgs_ix = SSNEW(sizeof(MGS));
    const bool was_temp = (bool)SvTEMP(sv);
    int have_new = 0;
    MAGIC *newmg, *head, *cur, *mg;
    /* guard against sv having being freed midway by holding a private
       reference. */

    PERL_ARGS_ASSERT_MG_GET;

    /* sv_2mortal has this side effect of turning on the TEMP flag, which can
       cause the SV's buffer to get stolen (and maybe other stuff).
       So restore it.
    */
    sv_2mortal(SvREFCNT_inc_simple_NN(sv));
    if (!was_temp) {
	SvTEMP_off(sv);
    }

    save_magic(mgs_ix, sv);

    /* We must call svt_get(sv, mg) for each valid entry in the linked
       list of magic. svt_get() may delete the current entry, add new
       magic to the head of the list, or upgrade the SV. AMS 20010810 */

    newmg = cur = head = mg = SvMAGIC(sv);
    while (mg) {
	const MGVTBL * const vtbl = mg->mg_virtual;

	if (!(mg->mg_flags & MGf_GSKIP) && vtbl && vtbl->svt_get) {
	    CALL_FPTR(vtbl->svt_get)(aTHX_ sv, mg);

	    /* guard against magic having been deleted - eg FETCH calling
	     * untie */
	    if (!SvMAGIC(sv))
		break;

	    /* Don't restore the flags for this entry if it was deleted. */
	    if (mg->mg_flags & MGf_GSKIP)
		(SSPTR(mgs_ix, MGS *))->mgs_flags = 0;
	}

	mg = mg->mg_moremagic;

	if (have_new) {
	    /* Have we finished with the new entries we saw? Start again
	       where we left off (unless there are more new entries). */
	    if (mg == head) {
		have_new = 0;
		mg   = cur;
		head = newmg;
	    }
	}

	/* Were any new entries added? */
	if (!have_new && (newmg = SvMAGIC(sv)) != head) {
	    have_new = 1;
	    cur = mg;
	    mg  = newmg;
	}
    }

    restore_magic(INT2PTR(void *, (IV)mgs_ix));

    if (SvREFCNT(sv) == 1) {
	/* We hold the last reference to this SV, which implies that the
	   SV was deleted as a side effect of the routines we called.  */
	SvOK_off(sv);
    }
    return 0;
}

/*
=for apidoc mg_set

Do magic after a value is assigned to the SV.  See C<sv_magic>.

=cut
*/

int
Perl_mg_set(pTHX_ SV *sv)
{
    dVAR;
    const I32 mgs_ix = SSNEW(sizeof(MGS));
    MAGIC* mg;
    MAGIC* nextmg;

    PERL_ARGS_ASSERT_MG_SET;

    save_magic(mgs_ix, sv);

    for (mg = SvMAGIC(sv); mg; mg = nextmg) {
        const MGVTBL* vtbl = mg->mg_virtual;
	nextmg = mg->mg_moremagic;	/* it may delete itself */
	if (mg->mg_flags & MGf_GSKIP) {
	    mg->mg_flags &= ~MGf_GSKIP;	/* setting requires another read */
	    (SSPTR(mgs_ix, MGS*))->mgs_flags = 0;
	}
	if (PL_localizing == 2 && !S_is_container_magic(mg))
	    continue;
	if (vtbl && vtbl->svt_set)
	    CALL_FPTR(vtbl->svt_set)(aTHX_ sv, mg);
    }

    restore_magic(INT2PTR(void*, (IV)mgs_ix));
    return 0;
}

/*
=for apidoc mg_length

Report on the SV's length.  See C<sv_magic>.

=cut
*/

U32
Perl_mg_length(pTHX_ SV *sv)
{
    dVAR;
    MAGIC* mg;
    STRLEN len;

    PERL_ARGS_ASSERT_MG_LENGTH;

    for (mg = SvMAGIC(sv); mg; mg = mg->mg_moremagic) {
        const MGVTBL * const vtbl = mg->mg_virtual;
	if (vtbl && vtbl->svt_len) {
            const I32 mgs_ix = SSNEW(sizeof(MGS));
	    save_magic(mgs_ix, sv);
	    /* omit MGf_GSKIP -- not changed here */
	    len = CALL_FPTR(vtbl->svt_len)(aTHX_ sv, mg);
	    restore_magic(INT2PTR(void*, (IV)mgs_ix));
	    return len;
	}
    }

    {
	/* You can't know whether it's UTF-8 until you get the string again...
	 */
        const char *s = SvPV_const(sv, len);

	if (IN_CODEPOINTS) {
	    len = utf8_length(s, s + len);
	}
    }
    return len;
}

I32
Perl_mg_size(pTHX_ SV *sv)
{
    MAGIC* mg;

    PERL_ARGS_ASSERT_MG_SIZE;

    for (mg = SvMAGIC(sv); mg; mg = mg->mg_moremagic) {
        const MGVTBL* const vtbl = mg->mg_virtual;
	if (vtbl && vtbl->svt_len) {
            const I32 mgs_ix = SSNEW(sizeof(MGS));
            I32 len;
	    save_magic(mgs_ix, sv);
	    /* omit MGf_GSKIP -- not changed here */
	    len = CALL_FPTR(vtbl->svt_len)(aTHX_ sv, mg);
	    restore_magic(INT2PTR(void*, (IV)mgs_ix));
	    return len;
	}
    }

    switch(SvTYPE(sv)) {
	case SVt_PVAV:
	    return AvFILLp((AV *) sv); /* Fallback to non-tied array */
	case SVt_PVHV:
	    /* FIXME */
	default:
	    Perl_croak(aTHX_ "Size magic not implemented");
	    break;
    }
    return 0;
}

/*
=for apidoc mg_clear

Clear something magical that the SV represents.  See C<sv_magic>.

=cut
*/

int
Perl_mg_clear(pTHX_ SV *sv)
{
    const I32 mgs_ix = SSNEW(sizeof(MGS));
    MAGIC* mg;

    PERL_ARGS_ASSERT_MG_CLEAR;

    save_magic(mgs_ix, sv);

    for (mg = SvMAGIC(sv); mg; mg = mg->mg_moremagic) {
        const MGVTBL* const vtbl = mg->mg_virtual;
	/* omit GSKIP -- never set here */

	if (vtbl && vtbl->svt_clear)
	    CALL_FPTR(vtbl->svt_clear)(aTHX_ sv, mg);
    }

    restore_magic(INT2PTR(void*, (IV)mgs_ix));
    return 0;
}

/*
=for apidoc mg_find

Finds the magic pointer for type matching the SV.  See C<sv_magic>.

=cut
*/

MAGIC*
Perl_mg_find(pTHX_ const SV *sv, int type)
{
    PERL_UNUSED_CONTEXT;
    if (sv) {
        MAGIC *mg;
        for (mg = SvMAGIC(sv); mg; mg = mg->mg_moremagic) {
            if (mg->mg_type == type)
                return mg;
        }
    }
    return NULL;
}

/*
=for apidoc mg_copy

Copies the magic from one SV to another.  See C<sv_magic>.

=cut
*/

int
Perl_mg_copy(pTHX_ SV *sv, SV *nsv, const char *key, I32 klen)
{
    int count = 0;
    MAGIC* mg;

    PERL_ARGS_ASSERT_MG_COPY;

    for (mg = SvMAGIC(sv); mg; mg = mg->mg_moremagic) {
        const MGVTBL* const vtbl = mg->mg_virtual;
	if ((mg->mg_flags & MGf_COPY) && vtbl->svt_copy){
	    count += CALL_FPTR(vtbl->svt_copy)(aTHX_ sv, mg, nsv, key, klen);
	}
	else {
	    const char type = mg->mg_type;
	    if (isUPPER(type) && type != PERL_MAGIC_uvar) {
		sv_magic(nsv,
		     (type == PERL_MAGIC_tied)
			? SvTIED_obj(sv, mg)
			: mg->mg_obj,
		     toLOWER(type), key, klen);
		count++;
	    }
	}
    }
    return count;
}

/*
=for apidoc mg_localize

Copy some of the magic from an existing SV to new localized version of
that SV. Container magic (eg %ENV, $1, tie) gets copied, value magic
doesn't (eg taint, pos).

=cut
*/

void
Perl_mg_localize(pTHX_ SV *sv, SV *nsv)
{
    dVAR;
    MAGIC *mg;

    PERL_ARGS_ASSERT_MG_LOCALIZE;

    for (mg = SvMAGIC(sv); mg; mg = mg->mg_moremagic) {
	const MGVTBL* const vtbl = mg->mg_virtual;
	if (!S_is_container_magic(mg))
	    continue;
		
	if ((mg->mg_flags & MGf_LOCAL) && vtbl->svt_local)
	    (void)CALL_FPTR(vtbl->svt_local)(aTHX_ nsv, mg);
	else
	    sv_magicext(nsv, mg->mg_obj, mg->mg_type, vtbl,
			    mg->mg_ptr, mg->mg_len);

	/* container types should remain read-only across localization */
	SvFLAGS(nsv) |= SvREADONLY(sv);
    }

    if (SvTYPE(nsv) >= SVt_PVMG && SvMAGIC(nsv)) {
	SvFLAGS(nsv) |= SvMAGICAL(sv);
	PL_localizing = 1;
	SvSETMAGIC(nsv);
	PL_localizing = 0;
    }	    
}

/*
=for apidoc mg_free

Free any magic storage used by the SV.  See C<sv_magic>.

=cut
*/

int
Perl_mg_free(pTHX_ SV *sv)
{
    MAGIC* mg;
    MAGIC* moremagic;

    PERL_ARGS_ASSERT_MG_FREE;

    for (mg = SvMAGIC(sv); mg; mg = moremagic) {
        const MGVTBL* const vtbl = mg->mg_virtual;
	moremagic = mg->mg_moremagic;
	if (vtbl && vtbl->svt_free)
	    CALL_FPTR(vtbl->svt_free)(aTHX_ sv, mg);
	if (mg->mg_ptr && mg->mg_type != PERL_MAGIC_regex_global) {
	    if (mg->mg_len > 0 || mg->mg_type == PERL_MAGIC_utf8)
		Safefree(mg->mg_ptr);
	    else if (mg->mg_len == HEf_SVKEY)
		SvREFCNT_dec((SV*)mg->mg_ptr);
	}
	if (mg->mg_flags & MGf_REFCOUNTED)
	    SvREFCNT_dec(mg->mg_obj);
	Safefree(mg);
	SvMAGIC_set(sv, moremagic);
    }
    SvMAGIC_set(sv, NULL);
    return 0;
}

void
Perl_mg_tmprefcnt(pTHX_ SV *sv)
{
    MAGIC* mg;
    MAGIC* moremagic;

    PERL_ARGS_ASSERT_MG_TMPREFCNT;

    for (mg = SvMAGIC(sv); mg; mg = moremagic) {
	moremagic = mg->mg_moremagic;
	if (mg->mg_ptr && mg->mg_type != PERL_MAGIC_regex_global) {
	    if (mg->mg_len == HEf_SVKEY)
		SvTMPREFCNT_inc((SV*)mg->mg_ptr);
	}
	if (mg->mg_flags & MGf_REFCOUNTED)
	    SvTMPREFCNT_inc(mg->mg_obj);
    }
}

#include <signal.h>

U32
Perl_magic_regdata_cnt(pTHX_ SV *sv, MAGIC *mg)
{
    dVAR;
    PERL_UNUSED_ARG(sv);

    PERL_ARGS_ASSERT_MAGIC_REGDATA_CNT;

    if (PL_curpm) {
	register const REGEXP * const rx = PM_GETRE(PL_curpm);
	if (rx) {
	    if (mg->mg_obj) {			/* @+ */
		/* return the number possible */
		return RX_NPARENS(rx);
	    } else {				/* @- */
		I32 paren = RX_LASTPAREN(rx);

		/* return the last filled */
		while ( paren >= 0
			&& (RX_OFFS(rx)[paren].start == -1
			    || RX_OFFS(rx)[paren].end == -1) )
		    paren--;
		return (U32)paren;
	    }
	}
    }

    return (U32)-1;
}

int
Perl_magic_regdatum_get(pTHX_ SV *sv, MAGIC *mg)
{
    dVAR;

    PERL_ARGS_ASSERT_MAGIC_REGDATUM_GET;

    if (PL_curpm) {
	register const REGEXP * const rx = PM_GETRE(PL_curpm);
	if (rx) {
	    register const I32 paren = mg->mg_len;
	    register I32 s;
	    register I32 t;
	    if (paren < 0)
		return 0;
	    if (paren <= (I32)RX_NPARENS(rx) &&
		(s = RX_OFFS(rx)[paren].start) != -1 &&
		(t = RX_OFFS(rx)[paren].end) != -1)
		{
		    register I32 i;
		    if (mg->mg_obj)		/* @+ */
			i = t;
		    else			/* @- */
			i = s;

		    if (i > 0 && IN_CODEPOINTS) {
			const char * const b = RX_SUBBEG(rx);
			if (b)
			    i = utf8_length(b, (b+i));
		    }

		    sv_setiv(sv, i);
		}
	}
    }
    return 0;
}

int
Perl_magic_regdatum_set(pTHX_ SV *sv, MAGIC *mg)
{
    PERL_ARGS_ASSERT_MAGIC_REGDATUM_SET;
    PERL_UNUSED_ARG(sv);
    PERL_UNUSED_ARG(mg);
    Perl_croak(aTHX_ PL_no_modify);
    NORETURN_FUNCTION_END;
}

U32
Perl_magic_len(pTHX_ SV *sv, MAGIC *mg)
{
    dVAR;
    register I32 paren;
    register I32 i;
    register const REGEXP * rx;
    const char * const remaining = mg->mg_ptr + 1;

    PERL_ARGS_ASSERT_MAGIC_LEN;

    switch (*mg->mg_ptr) {
    case '\020':		
      if (*remaining == '\0') { /* ^P */
          break;
      } else if (strEQ(remaining, "REMATCH")) { /* $^PREMATCH */
          goto do_prematch;
      } else if (strEQ(remaining, "OSTMATCH")) { /* $^POSTMATCH */
          goto do_postmatch;
      }
      break;
    case '\015': /* $^MATCH */
	if (strEQ(remaining, "ATCH")) {
        goto do_match;
    } else {
        break;
    }
    case '`':
      do_prematch:
      paren = RX_BUFF_IDX_PREMATCH;
      goto maybegetparen;
    case '\'':
      do_postmatch:
      paren = RX_BUFF_IDX_POSTMATCH;
      goto maybegetparen;
    case '&':
      do_match:
      paren = RX_BUFF_IDX_FULLMATCH;
      goto maybegetparen;
    case '1': case '2': case '3': case '4':
    case '5': case '6': case '7': case '8': case '9':
      paren = atoi(mg->mg_ptr);
    maybegetparen:
	if (PL_curpm && (rx = PM_GETRE(PL_curpm))) {
      getparen:
        i = CALLREG_NUMBUF_LENGTH((REGEXP * const)rx, sv, paren);

		if (i < 0)
		    Perl_croak(aTHX_ "panic: magic_len: %"IVdf, (IV)i);
		return i;
	} else {
		if (ckWARN(WARN_UNINITIALIZED))
		    report_uninit(sv);
		return 0;
	}
    case '+':
	if (PL_curpm && (rx = PM_GETRE(PL_curpm))) {
	    paren = RX_LASTPAREN(rx);
	    if (paren)
		goto getparen;
	}
	return 0;
    case '\016': /* ^N */
	if (PL_curpm && (rx = PM_GETRE(PL_curpm))) {
	    paren = RX_LASTCLOSEPAREN(rx);
	    if (paren)
		goto getparen;
	}
	return 0;
    }
    magic_get(sv,mg);
    if (!SvPOK(sv) && SvNIOK(sv)) {
	sv_2pv(sv, 0);
    }
    if (SvPOK(sv))
	return SvCUR(sv);
    return 0;
}

#define SvRTRIM(sv) STMT_START { \
    if (SvPOK(sv)) { \
        STRLEN len = SvCUR(sv); \
        char * const p = SvPVX(sv); \
	while (len > 0 && isSPACE(p[len-1])) \
	   --len; \
	SvCUR_set(sv, len); \
	p[len] = '\0'; \
    } \
} STMT_END

void
Perl_emulate_cop_io(pTHX_ const COP *const c, SV *const sv)
{
    PERL_ARGS_ASSERT_EMULATE_COP_IO;

    if (!(CopHINTS_get(c) & (HINT_LEXICAL_IO_IN|HINT_LEXICAL_IO_OUT)))
	sv_setsv(sv, &PL_sv_undef);
    else {
	sv_setpvs(sv, "");
	if ((CopHINTS_get(c) & HINT_LEXICAL_IO_IN)) {
	    SV **const value = hv_fetch(c->cop_hints_hash, "open<", 5, 0);
	    assert(*value);
	    sv_catsv(sv, *value);
	}
	sv_catpvs(sv, "\0");
	if ((CopHINTS_get(c) & HINT_LEXICAL_IO_OUT)) {
	    SV **const value = hv_fetch(c->cop_hints_hash, "open>", 5, 0);
	    assert(*value);
	    sv_catsv(sv, *value);
	}
    }
}

int
Perl_magic_get(pTHX_ SV *sv, MAGIC *mg)
{
    dVAR;
    register I32 paren;
    register REGEXP *rx;
    const char * const remaining = mg->mg_ptr + 1;

    PERL_ARGS_ASSERT_MAGIC_GET;

    switch (*mg->mg_ptr) {
    case '^':
	if (remaining[1] != '\0') {
	    switch (*remaining) {
	    case 'C':
		if (strEQ(remaining, "CHILD_ERROR_NATIVE")) { /* $^CHILD_ERROR_NATIVE */
		    sv_setiv(sv, (IV)STATUS_NATIVE);
		}
		break;
	    case 'D':
		if (strEQ(remaining, "DIE_HOOK")) { /* $^DIE_HOOK */
		    sv_setsv(sv, PL_diehook);
		    break;
		}
		break;
	    case 'E':
		if (strEQ(remaining, "EGID")) { /* $^EGID */
		    sv_setiv(sv, (IV)PL_egid);
		  add_groups:
#ifdef HAS_GETGROUPS
		    {
			Groups_t *gary = NULL;
			I32 i, num_groups = getgroups(0, gary);
			Newx(gary, num_groups, Groups_t);
			num_groups = getgroups(num_groups, gary);
			for (i = 0; i < num_groups; i++)
			    Perl_sv_catpvf(aTHX_ sv, " %"IVdf, (IV)gary[i]);
			Safefree(gary);
		    }
		    (void)SvIOK_on(sv);	/* what a wonderful hack! */
#endif
		    break;
		}
		break;
	    case 'G':
		if (strEQ(remaining, "GID")) { /* $^GID */
		    sv_setiv(sv, (IV)PL_gid);
		    goto add_groups;
		}
		break;
	    case 'M': /* $^MATCH */
		if (strEQ(remaining, "MATCH")) {
		    if (PL_curpm && (rx = PM_GETRE(PL_curpm))) {
			/*
			 * Pre-threads, this was paren = atoi(GvENAME((GV*)mg->mg_obj));
			 * XXX Does the new way break anything?
			 */
			paren = atoi(mg->mg_ptr); /* $& is in [0] */
			CALLREG_NUMBUF_FETCH(rx,paren,sv);
			break;
		    }
		    sv_setsv(sv,&PL_sv_undef);
		}
		break;
	    case 'O':
		if (strEQ(remaining, "OPEN")) { /* $^OPEN */
		    Perl_emulate_cop_io(aTHX_ &PL_compiling, sv);
		}
		break;
	    case 'P':
		if (strEQ(remaining, "PREMATCH")) { /* $^PREMATCH */
		    goto do_prematch_fetch;
		} else if (strEQ(remaining, "POSTMATCH")) { /* $^POSTMATCH */
		    goto do_postmatch_fetch;
		}
		break;
	    case 'T':
		if (strEQ(remaining, "TAINT")) /* $^TAINT */
		    sv_setiv(sv, PL_tainting
			     ? (PL_taint_warn || PL_unsafe ? -1 : 1)
			     : 0);
		break;
	    case 'U':		/* $^UNICODE, $^UTF8LOCALE, $^UTF8CACHE */
		if (strEQ(remaining, "UNICODE"))
		    sv_setuv(sv, (UV) PL_unicode);
		else if (strEQ(remaining, "UTF8LOCALE"))
		    sv_setuv(sv, (UV) PL_utf8locale);
		else if (strEQ(remaining, "UTF8CACHE"))
		    sv_setiv(sv, (IV) PL_utf8cache);
		break;
	    case 'W':
		if (strEQ(remaining, "WARNING_BITS")) { /* $^WARNING_BITS */
		    if (PL_compiling.cop_warnings == pWARN_NONE) {
			sv_setpvn(sv, WARN_NONEstring, WARNsize) ;
		    }
		    else if (PL_compiling.cop_warnings == pWARN_STD) {
			sv_setpvn(
			    sv, 
			    (PL_dowarn & G_WARN_ON) ? WARN_ALLstring : WARN_NONEstring,
			    WARNsize
			    );
		    }
		    else if (PL_compiling.cop_warnings == pWARN_ALL) {
			/* Get the bit mask for $warnings::Bits{all}, because
			 * it could have been extended by warnings::register */
			HV * const bits=get_hv("warnings::Bits", FALSE);
			if (bits) {
			    SV ** const bits_all = hv_fetchs(bits, "all", FALSE);
			    if (bits_all)
				sv_setsv(sv, *bits_all);
			}
			else {
			    sv_setpvn(sv, WARN_ALLstring, WARNsize) ;
			}
		    }
		    else {
			sv_setpvn(sv, (char *) (PL_compiling.cop_warnings + 1),
				  *PL_compiling.cop_warnings);
		    }
		    SvPOK_only(sv);
		} else if (strEQ(remaining, "WARN_HOOK")) { /* $^WARN_HOOK */
		    break;
		}
		break;
	    }
	}
	else {
	    switch (*remaining) {
	    case 'C':		/* $^C */
		sv_setiv(sv, (IV)PL_minus_c);
		break;
	    case 'D':		/* $^D */
		sv_setiv(sv, (IV)(PL_debug & DEBUG_MASK));
		break;
	    case 'E':  /* $^E */
#if defined(MACOS_TRADITIONAL)
		{
		    char msg[256];

		    sv_setnv(sv,(double)gMacPerl_OSErr);
		    sv_setpv(sv, gMacPerl_OSErr ? GetSysErrText(gMacPerl_OSErr, msg) : "");
		}
#elif defined(VMS)
		{
#	            include <descrip.h>
#	            include <starlet.h>
		    char msg[255];
		    $DESCRIPTOR(msgdsc,msg);
		    sv_setnv(sv,(NV) vaxc$errno);
		    if (sys$getmsg(vaxc$errno,&msgdsc.dsc$w_length,&msgdsc,0,0) & 1)
			sv_setpvn(sv,msgdsc.dsc$a_pointer,msgdsc.dsc$w_length);
		    else
			sv_setpvn(sv,"",0);
		}
#elif defined(OS2)
		if (!(_emx_env & 0x200)) {	/* Under DOS */
		    sv_setnv(sv, (NV)errno);
		    sv_setpv(sv, errno ? Strerror(errno) : "");
		} else {
		    if (errno != errno_isOS2) {
			const int tmp = _syserrno();
			if (tmp)	/* 2nd call to _syserrno() makes it 0 */
			    Perl_rc = tmp;
		    }
		    sv_setnv(sv, (NV)Perl_rc);
		    sv_setpv(sv, os2error(Perl_rc));
		}
#elif defined(WIN32)
		{
		    const DWORD dwErr = GetLastError();
		    sv_setnv(sv, (NV)dwErr);
		    if (dwErr) {
			PerlProc_GetOSError(sv, dwErr);
		    }
		    else
			sv_setpvn(sv, "", 0);
		    SetLastError(dwErr);
		}
#else
		{
		    const int saveerrno = errno;
		    sv_setnv(sv, (NV)errno);
		    sv_setpv(sv, errno ? Strerror(errno) : "");
		    errno = saveerrno;
		}
#endif
		SvRTRIM(sv);
		SvNOK_on(sv);	/* what a wonderful hack! */
		break;
	    case 'F':		/* $^F */
		sv_setiv(sv, (IV)PL_maxsysfd);
		break;
	    case 'H':		/* $^H */
		sv_setiv(sv, (IV)PL_hints);
		break;
	    case 'I':		/* $^I */
		sv_setpv(sv, PL_inplace); /* Will undefine sv if PL_inplace is NULL */
		break;
	    case 'O':		/* $^O */
		sv_setpv(sv, PL_osname);
		SvTAINTED_off(sv);
		break;
	    case 'P':		/* $^P */
		sv_setiv(sv, (IV)PL_perldb);
		break;
	    case 'S':		/* $^S */
		if (PL_parser && PL_parser->lex_state != LEX_NOTPARSING)
		    SvOK_off(sv);
		else if (PL_in_eval)
		    sv_setiv(sv, PL_in_eval & ~(EVAL_INREQUIRE));
		else
		    sv_setiv(sv, 0);
		break;
	    case 'T':		/* $^T */
#ifdef BIG_TIME
		sv_setnv(sv, PL_basetime);
#else
		sv_setiv(sv, (IV)PL_basetime);
#endif
		break;
	    case 'W':		/* $^W */
		sv_setiv(sv, (IV)((PL_dowarn & G_WARN_ON) ? TRUE : FALSE));
		break;
	    case 'N':		/* ^N */
		if (PL_curpm && (rx = PM_GETRE(PL_curpm))) {
		    if (RX_LASTCLOSEPAREN(rx)) {
			CALLREG_NUMBUF_FETCH(rx,RX_LASTCLOSEPAREN(rx),sv);
			break;
		    }
		}
		sv_setsv(sv,&PL_sv_undef);
		break;
	    }
	}
	break;
    case '1': case '2': case '3': case '4':
    case '5': case '6': case '7': case '8': case '9': case '&':
	if (PL_curpm && (rx = PM_GETRE(PL_curpm))) {
	    /*
	     * Pre-threads, this was paren = atoi(GvENAME((GV*)mg->mg_obj));
	     * XXX Does the new way break anything?
	     */
	    paren = atoi(mg->mg_ptr); /* $& is in [0] */
	    CALLREG_NUMBUF_FETCH(rx,paren,sv);
	    break;
	}
	sv_setsv(sv,&PL_sv_undef);
	break;
    case '+':
	if (PL_curpm && (rx = PM_GETRE(PL_curpm))) {
	    if (RX_LASTPAREN(rx)) {
	        CALLREG_NUMBUF_FETCH(rx,RX_LASTPAREN(rx),sv);
	        break;
	    }
	}
	sv_setsv(sv,&PL_sv_undef);
	break;
    case '`':
      do_prematch_fetch:
	if (PL_curpm && (rx = PM_GETRE(PL_curpm))) {
	    CALLREG_NUMBUF_FETCH(rx,-2,sv);
	    break;
	}
	sv_setsv(sv,&PL_sv_undef);
	break;
    case '\'':
      do_postmatch_fetch:
	if (PL_curpm && (rx = PM_GETRE(PL_curpm))) {
	    CALLREG_NUMBUF_FETCH(rx,-1,sv);
	    break;
	}
	sv_setsv(sv,&PL_sv_undef);
	break;
    case '?':
	{
	    sv_setiv(sv, (IV)STATUS_CURRENT);
#ifdef COMPLEX_STATUS
	    LvTARGOFF(sv) = PL_statusvalue;
	    LvTARGLEN(sv) = PL_statusvalue_vms;
#endif
	}
	break;
    case ':':
	break;
    case '/':
	break;
    case '|':
	if (GvIOp(PL_defoutgv))
	    sv_setiv(sv, (IV)(IoFLAGS(GvIOp(PL_defoutgv)) & IOf_FLUSH) != 0 );
	break;
    case ',':
	break;
    case '\\':
	if (PL_ors_sv)
	    sv_copypv(sv, PL_ors_sv);
	break;
    case '!':
#ifdef VMS
	sv_setnv(sv, (NV)((errno == EVMSERR) ? vaxc$errno : errno));
	sv_setpv(sv, errno ? Strerror(errno) : "");
#else
	{
	const int saveerrno = errno;
	sv_setnv(sv, (NV)errno);
#ifdef OS2
	if (errno == errno_isOS2 || errno == errno_isOS2_set)
	    sv_setpv(sv, os2error(Perl_rc));
	else
#endif
	sv_setpv(sv, errno ? Strerror(errno) : "");
	errno = saveerrno;
	}
#endif
	SvRTRIM(sv);
	SvNOK_on(sv);	/* what a wonderful hack! */
	break;
    case '<':
	sv_setiv(sv, (IV)PL_uid);
	break;
    case '>':
	sv_setiv(sv, (IV)PL_euid);
	break;
#ifndef MACOS_TRADITIONAL
    case '0':
	break;
#endif
    }
    return 0;
}

int
Perl_magic_getuvar(pTHX_ SV *sv, MAGIC *mg)
{
    struct ufuncs * const uf = (struct ufuncs *)mg->mg_ptr;

    PERL_ARGS_ASSERT_MAGIC_GETUVAR;

    if (uf && uf->uf_val)
	(*uf->uf_val)(aTHX_ uf->uf_index, sv);
    return 0;
}

int
Perl_magic_setenv(pTHX_ SV *sv, MAGIC *mg)
{
    dVAR;
    STRLEN len = 0, klen;
    const char *s = SvOK(sv) ? SvPV_const(sv,len) : "";
    const char * const ptr = MgPV_const(mg,klen);
    my_setenv(ptr, s);

    PERL_ARGS_ASSERT_MAGIC_SETENV;

#ifdef DYNAMIC_ENV_FETCH
     /* We just undefd an environment var.  Is a replacement */
     /* waiting in the wings? */
    if (!len) {
	SV ** const valp = hv_fetch(GvHVn(PL_envgv), ptr, klen, FALSE);
	if (valp)
	    s = SvOK(*valp) ? SvPV_const(*valp, len) : "";
    }
#endif

#if !defined(OS2) && !defined(AMIGAOS) && !defined(WIN32) && !defined(MSDOS)
			    /* And you'll never guess what the dog had */
			    /*   in its mouth... */
    if (PL_tainting) {
	MgTAINTEDDIR_off(mg);
#ifdef VMS
	if (s && klen == 8 && strEQ(ptr, "DCL$PATH")) {
	    char pathbuf[256], eltbuf[256], *cp, *elt;
	    Stat_t sbuf;
	    int i = 0, j = 0;

	    my_strlcpy(eltbuf, s, sizeof(eltbuf));
	    elt = eltbuf;
	    do {          /* DCL$PATH may be a search list */
		while (1) {   /* as may dev portion of any element */
		    if ( ((cp = strchr(elt,'[')) || (cp = strchr(elt,'<'))) ) {
			if ( *(cp+1) == '.' || *(cp+1) == '-' ||
			     cando_by_name(S_IWUSR,0,elt) ) {
			    MgTAINTEDDIR_on(mg);
			    return 0;
			}
		    }
		    if ((cp = strchr(elt, ':')) != NULL)
			*cp = '\0';
		    if (my_trnlnm(elt, eltbuf, j++))
			elt = eltbuf;
		    else
			break;
		}
		j = 0;
	    } while (my_trnlnm(s, pathbuf, i++) && (elt = pathbuf));
	}
#endif /* VMS */
	if (s && klen == 4 && strEQ(ptr,"PATH")) {
	    const char * const strend = s + len;

	    while (s < strend) {
		char tmpbuf[256];
		Stat_t st;
		I32 i;
#ifdef VMS  /* Hmm.  How do we get $Config{path_sep} from C? */
		const char path_sep = '|';
#else
		const char path_sep = ':';
#endif
		s = delimcpy(tmpbuf, tmpbuf + sizeof tmpbuf,
			     s, strend, path_sep, &i);
		s++;
		if (i >= (I32)sizeof tmpbuf   /* too long -- assume the worst */
#ifdef VMS
		      || !strchr(tmpbuf, ':') /* no colon thus no device name -- assume relative path */
#else
		      || *tmpbuf != '/'       /* no starting slash -- assume relative path */
#endif
		      || (PerlLIO_stat(tmpbuf, &st) == 0 && (st.st_mode & 2)) ) {
		    MgTAINTEDDIR_on(mg);
		    return 0;
		}
	    }
	}
    }
#endif /* neither OS2 nor AMIGAOS nor WIN32 nor MSDOS */

    return 0;
}

int
Perl_magic_clearenv(pTHX_ SV *sv, MAGIC *mg)
{
    PERL_ARGS_ASSERT_MAGIC_CLEARENV;
    PERL_UNUSED_ARG(sv);
    my_setenv(MgPV_nolen_const(mg),NULL);
    return 0;
}

int
Perl_magic_set_all_env(pTHX_ SV *sv, MAGIC *mg)
{
    dVAR;
    PERL_ARGS_ASSERT_MAGIC_SET_ALL_ENV;
    PERL_UNUSED_ARG(mg);
#if defined(VMS)
    Perl_die(aTHX_ "Can't make list assignment to %%ENV on this system");
#else
    if (PL_localizing) {
	HE* entry;
	my_clearenv();
	hv_iterinit((HV*)sv);
	while ((entry = hv_iternext((HV*)sv))) {
	    I32 keylen;
	    my_setenv(hv_iterkey(entry, &keylen),
		      SvPV_nolen_const(hv_iterval((HV*)sv, entry)));
	}
    }
#endif
    return 0;
}

int
Perl_magic_clear_all_env(pTHX_ SV *sv, MAGIC *mg)
{
    dVAR;
    PERL_ARGS_ASSERT_MAGIC_CLEAR_ALL_ENV;
    PERL_UNUSED_ARG(sv);
    PERL_UNUSED_ARG(mg);
#if defined(VMS)
    Perl_die(aTHX_ "Can't make list assignment to %%ENV on this system");
#else
    my_clearenv();
#endif
    return 0;
}

#ifndef PERL_MICRO
#ifdef HAS_SIGPROCMASK
static void
restore_sigmask(pTHX_ SV *save_sv)
{
    const sigset_t * const ossetp = (const sigset_t *) SvPV_nolen_const( save_sv );
    (void)sigprocmask(SIG_SETMASK, ossetp, NULL);
}
#endif
int
Perl_magic_getsig(pTHX_ SV *sv, MAGIC *mg)
{
    dVAR;
    /* Are we fetching a signal entry? */
    const I32 i = whichsig(MgPV_nolen_const(mg));

    PERL_ARGS_ASSERT_MAGIC_GETSIG;

    if (i > 0) {
    	if(PL_psig_ptr[i])
    	    sv_setsv(sv,sv_2mortal(newRV_inc(PL_psig_ptr[i])));
    	else {
	    Sighandler_t sigstate = rsignal_state(i);
#ifdef FAKE_PERSISTENT_SIGNAL_HANDLERS
	    if (PL_sig_handlers_initted && PL_sig_ignoring[i])
		sigstate = SIG_IGN;
#endif
#ifdef FAKE_DEFAULT_SIGNAL_HANDLERS
	    if (PL_sig_handlers_initted && PL_sig_defaulting[i])
		sigstate = SIG_DFL;
#endif
    	    /* cache state so we don't fetch it again */
    	    if(sigstate == (Sighandler_t) SIG_IGN)
    	    	sv_setpvs(sv,"IGNORE");
    	    else
    	    	sv_setsv(sv,&PL_sv_undef);
    	}
    }
    return 0;
}
int
Perl_magic_clearsig(pTHX_ SV *sv, MAGIC *mg)
{
    /* XXX Some of this code was copied from Perl_magic_setsig. A little
     * refactoring might be in order.
     */
    dVAR;
    register const char * const s = MgPV_nolen_const(mg);
    PERL_ARGS_ASSERT_MAGIC_CLEARSIG;
    PERL_UNUSED_ARG(sv);
    {
        /* Are we clearing a signal entry? */
        const I32 i = whichsig(s);
        if (i > 0) {
#ifdef HAS_SIGPROCMASK
            sigset_t set, save;
            SV* save_sv;
            /* Avoid having the signal arrive at a bad time, if possible. */
            sigemptyset(&set);
            sigaddset(&set,i);
            sigprocmask(SIG_BLOCK, &set, &save);
            ENTER;
            save_sv = newSVpvn((char *)(&save), sizeof(sigset_t));
            SAVEFREESV(save_sv);
            SAVEDESTRUCTOR_X(restore_sigmask, save_sv);
#endif
            PERL_ASYNC_CHECK();
#if defined(FAKE_PERSISTENT_SIGNAL_HANDLERS) || defined(FAKE_DEFAULT_SIGNAL_HANDLERS)
            if (!PL_sig_handlers_initted) Perl_csighandler_init();
#endif
#ifdef FAKE_DEFAULT_SIGNAL_HANDLERS
            PL_sig_defaulting[i] = 1;
            (void)rsignal(i, PL_csighandlerp);
#else
            (void)rsignal(i, (Sighandler_t) SIG_DFL);
#endif
            if(PL_psig_name[i]) {
                SvREFCNT_dec(PL_psig_name[i]);
                PL_psig_name[i]=0;
            }
            if(PL_psig_ptr[i]) {
                SV * const to_dec=PL_psig_ptr[i];
                PL_psig_ptr[i]=NULL;
                LEAVE;
                SvREFCNT_dec(to_dec);
            }
            else
                LEAVE;
        }
    }
    return 0;
}

/*
 * The signal handling nomenclature has gotten a bit confusing since the advent of
 * safe signals.  S_raise_signal only raises signals by analogy with what the 
 * underlying system's signal mechanism does.  It might be more proper to say that
 * it defers signals that have already been raised and caught.  
 *
 * PL_sig_pending and PL_psig_pend likewise do not track signals that are pending 
 * in the sense of being on the system's signal queue in between raising and delivery.  
 * They are only pending on Perl's deferral list, i.e., they track deferred signals 
 * awaiting delivery after the current Perl opcode completes and say nothing about
 * signals raised but not yet caught in the underlying signal implementation.
 */

#ifndef SIG_PENDING_DIE_COUNT
#  define SIG_PENDING_DIE_COUNT 120
#endif

static void
S_raise_signal(pTHX_ int sig)
{
    dVAR;
    /* Set a flag to say this signal is pending */
    PL_psig_pend[sig]++;
    /* And one to say _a_ signal is pending */
    if (++PL_sig_pending >= SIG_PENDING_DIE_COUNT)
        Perl_croak(aTHX_ "Maximal count of pending signals (%lu) exceeded",
                (unsigned long)SIG_PENDING_DIE_COUNT);
}

Signal_t
#if defined(HAS_SIGACTION) && defined(SA_SIGINFO)
Perl_csighandler(int sig, siginfo_t *sip PERL_UNUSED_DECL, void *uap PERL_UNUSED_DECL)
#else
Perl_csighandler(int sig)
#endif
{
#ifdef PERL_GET_SIG_CONTEXT
    dTHXa(PERL_GET_SIG_CONTEXT);
#else
    dTHX;
#endif
#if defined(HAS_SIGACTION) && defined(SA_SIGINFO)
#endif
#ifdef FAKE_PERSISTENT_SIGNAL_HANDLERS
    (void) rsignal(sig, PL_csighandlerp);
    if (PL_sig_ignoring[sig]) return;
#endif
#ifdef FAKE_DEFAULT_SIGNAL_HANDLERS
    if (PL_sig_defaulting[sig])
#ifdef KILL_BY_SIGPRC
            exit((Perl_sig_to_vmscondition(sig)&STS$M_COND_ID)|STS$K_SEVERE|STS$M_INHIB_MSG);
#else
            exit(1);
#endif
#endif
#if defined(HAS_SIGACTION) && defined(SA_SIGINFO)
#endif
   if (
#ifdef SIGILL
           sig == SIGILL ||
#endif
#ifdef SIGBUS
           sig == SIGBUS ||
#endif
#ifdef SIGSEGV
           sig == SIGSEGV ||
#endif
           (PL_signals & PERL_SIGNALS_UNSAFE_FLAG))
        /* Call the perl level handler now--
         * with risk we may be in malloc() etc. */
#if defined(HAS_SIGACTION) && defined(SA_SIGINFO)
        (*PL_sighandlerp)(sig, NULL, NULL);
#else
        (*PL_sighandlerp)(sig);
#endif
   else
        S_raise_signal(aTHX_ sig);
}

#if defined(FAKE_PERSISTENT_SIGNAL_HANDLERS) || defined(FAKE_DEFAULT_SIGNAL_HANDLERS)
void
Perl_csighandler_init(void)
{
    int sig;
    if (PL_sig_handlers_initted) return;

    for (sig = 1; sig < SIG_SIZE; sig++) {
#ifdef FAKE_DEFAULT_SIGNAL_HANDLERS
        dTHX;
        PL_sig_defaulting[sig] = 1;
        (void) rsignal(sig, PL_csighandlerp);
#endif
#ifdef FAKE_PERSISTENT_SIGNAL_HANDLERS
        PL_sig_ignoring[sig] = 0;
#endif
    }
    PL_sig_handlers_initted = 1;
}
#endif

void
Perl_despatch_signals(pTHX)
{
    dVAR;
    int sig;
    PL_sig_pending = 0;
    for (sig = 1; sig < SIG_SIZE; sig++) {
        if (PL_psig_pend[sig]) {
            PERL_BLOCKSIG_ADD(set, sig);
            PL_psig_pend[sig] = 0;
            PERL_BLOCKSIG_BLOCK(set);
#if defined(HAS_SIGACTION) && defined(SA_SIGINFO)
            (*PL_sighandlerp)(sig, NULL, NULL);
#else
            (*PL_sighandlerp)(sig);
#endif
            PERL_BLOCKSIG_UNBLOCK(set);
        }
    }
}

int
Perl_magic_setsig(pTHX_ SV *sv, MAGIC *mg)
{
    dVAR;
    I32 i;
    /* Need to be careful with SvREFCNT_dec(), because that can have side
     * effects (due to closures). We must make sure that the new disposition
     * is in place before it is called.
     */
    SV* to_dec = NULL;
    STRLEN len;
    const char *s;
    bool set_to_ignore = FALSE;
    bool set_to_default = FALSE;
#ifdef HAS_SIGPROCMASK
    sigset_t set, save;
    SV* save_sv;
#endif

    PERL_ARGS_ASSERT_MAGIC_SETSIG;

    if ( SvROK(sv) ) {
	if ( SvTYPE(SvRV(sv)) != SVt_PVCV )
	    Perl_croak(aTHX_ "signal handler should be a code refernce, 'DEFAULT' or 'IGNORE'");
    } else {
        const char *s = SvOK(sv) ? SvPV_force(sv,len) : "DEFAULT";
        if ( strEQ(s,"IGNORE") )
	    set_to_ignore = TRUE;
	else if (strEQ(s,"DEFAULT"))
	    set_to_default = TRUE;
	else
            Perl_croak(aTHX_  "signal handler should be a code reference or 'DEFAULT or 'IGNORE'");
    }

    s = MgPV_const(mg,len);
    i = whichsig(s);        /* ...no, a brick */
    if (i <= 0) {
	if (ckWARN(WARN_SIGNAL))
	    Perl_warner(aTHX_ packWARN(WARN_SIGNAL), "No such signal: SIG%s", s);
	return 0;
    }
#ifdef HAS_SIGPROCMASK
    /* Avoid having the signal arrive at a bad time, if possible. */
    sigemptyset(&set);
    sigaddset(&set,i);
    sigprocmask(SIG_BLOCK, &set, &save);
    ENTER;
    save_sv = newSVpvn((char *)(&save), sizeof(sigset_t));
    SAVEFREESV(save_sv);
    SAVEDESTRUCTOR_X(restore_sigmask, save_sv);
#endif
    PERL_ASYNC_CHECK();
#if defined(FAKE_PERSISTENT_SIGNAL_HANDLERS) || defined(FAKE_DEFAULT_SIGNAL_HANDLERS)
    if (!PL_sig_handlers_initted) Perl_csighandler_init();
#endif
#ifdef FAKE_PERSISTENT_SIGNAL_HANDLERS
    PL_sig_ignoring[i] = 0;
#endif
#ifdef FAKE_DEFAULT_SIGNAL_HANDLERS
    PL_sig_defaulting[i] = 0;
#endif
    SvREFCNT_dec(PL_psig_name[i]);
    to_dec = PL_psig_ptr[i];
    PL_psig_ptr[i] = NULL;
    PL_psig_name[i] = newSVpvn(s, len);
    SvREADONLY_on(PL_psig_name[i]);

    if (SvROK(sv)) {
	PL_psig_ptr[i] = SvREFCNT_inc(SvRV(sv));
	(void)rsignal(i, PL_csighandlerp);
#ifdef HAS_SIGPROCMASK
	LEAVE;
#endif
        if(to_dec)
            SvREFCNT_dec(to_dec);
        return 0;
    }
    if (set_to_ignore) {
#ifdef FAKE_PERSISTENT_SIGNAL_HANDLERS
	PL_sig_ignoring[i] = 1;
	(void)rsignal(i, PL_csighandlerp);
#else
	(void)rsignal(i, (Sighandler_t) SIG_IGN);
#endif
    }
    else {
#ifdef FAKE_DEFAULT_SIGNAL_HANDLERS
	PL_sig_defaulting[i] = 1;
	(void)rsignal(i, PL_csighandlerp);
#else
	(void)rsignal(i, (Sighandler_t) SIG_DFL);
#endif
    }
#ifdef HAS_SIGPROCMASK
    if(i)
        LEAVE;
#endif
    if(to_dec)
        SvREFCNT_dec(to_dec);
    return 0;
}
#endif /* !PERL_MICRO */

int
Perl_magic_setisa(pTHX_ SV *sv, MAGIC *mg)
{
    dVAR;
    HV* stash;

    PERL_ARGS_ASSERT_MAGIC_SETISA;
    PERL_UNUSED_ARG(sv);

    /* Bail out if destruction is going on */
    if(PL_dirty) return 0;

    /* Skip _isaelem because _isa will handle it shortly */
    if (PL_delaymagic & DM_ARRAY && mg->mg_type == PERL_MAGIC_isaelem)
        return 0;

    /* XXX Once it's possible, we need to
       detect that our @ISA is aliased in
       other stashes, and act on the stashes
       of all of the aliases */

    /* The first case occurs via setisa,
       the second via setisa_elem, which
       calls this same magic */
    stash = GvSTASH(
        SvTYPE(mg->mg_obj) == SVt_PVGV
            ? (GV*)mg->mg_obj
            : (GV*)SvMAGIC(mg->mg_obj)->mg_obj
    );

    mro_isa_changed_in(stash);

    return 0;
}

int
Perl_magic_clearisa(pTHX_ SV *sv, MAGIC *mg)
{
    dVAR;
    HV* stash;

    PERL_ARGS_ASSERT_MAGIC_CLEARISA;

    /* Bail out if destruction is going on */
    if(PL_dirty) return 0;

    av_clear((AV*)sv);

    /* XXX see comments in magic_setisa */
    stash = GvSTASH(
        SvTYPE(mg->mg_obj) == SVt_PVGV
            ? (GV*)mg->mg_obj
            : (GV*)SvMAGIC(mg->mg_obj)->mg_obj
    );

    mro_isa_changed_in(stash);

    return 0;
}

/* caller is responsible for stack switching/cleanup */
STATIC int
S_magic_methcall(pTHX_ SV *sv, const MAGIC *mg, const char *meth, I32 flags, int n, SV *val)
{
    dVAR;
    dSP;

    PERL_ARGS_ASSERT_MAGIC_METHCALL;

    PUSHMARK(SP);
    EXTEND(SP, n);
    PUSHs(SvTIED_obj(sv, mg));
    if (n > 1) {
	if (mg->mg_ptr) {
	    if (mg->mg_len >= 0)
		mPUSHp(mg->mg_ptr, mg->mg_len);
	    else if (mg->mg_len == HEf_SVKEY)
		PUSHs((SV*)mg->mg_ptr);
	}
	else if (mg->mg_type == PERL_MAGIC_tiedelem) {
	    mPUSHi(mg->mg_len);
	}
    }
    if (n > 2) {
        PUSHs(val);
    }
    PUTBACK;

    return call_method(meth, flags);
}

STATIC int
S_magic_methpack(pTHX_ SV *sv, const MAGIC *mg, const char *meth)
{
    dVAR; dSP;

    PERL_ARGS_ASSERT_MAGIC_METHPACK;

    ENTER;
    SAVETMPS;
    PUSHSTACKi(PERLSI_MAGIC);

    if (magic_methcall(sv, mg, meth, G_SCALAR, 2, NULL)) {
        sv_setsv(sv, *PL_stack_sp--);
    }

    POPSTACK;
    FREETMPS;
    LEAVE;
    return 0;
}

int
Perl_magic_getpack(pTHX_ SV *sv, MAGIC *mg)
{
    PERL_ARGS_ASSERT_MAGIC_GETPACK;

    if (mg->mg_ptr)
        mg->mg_flags |= MGf_GSKIP;
    magic_methpack(sv,mg,"FETCH");
    return 0;
}

int
Perl_magic_setpack(pTHX_ SV *sv, MAGIC *mg)
{
    dVAR; dSP;

    PERL_ARGS_ASSERT_MAGIC_SETPACK;

    ENTER;
    PUSHSTACKi(PERLSI_MAGIC);
    magic_methcall(sv, mg, "STORE", G_SCALAR|G_DISCARD, 3, sv);
    POPSTACK;
    LEAVE;
    return 0;
}

int
Perl_magic_clearpack(pTHX_ SV *sv, MAGIC *mg)
{
    PERL_ARGS_ASSERT_MAGIC_CLEARPACK;

    return magic_methpack(sv,mg,"DELETE");
}


U32
Perl_magic_sizepack(pTHX_ SV *sv, MAGIC *mg)
{
    dVAR; dSP;
    I32 retval = 0;

    PERL_ARGS_ASSERT_MAGIC_SIZEPACK;

    ENTER;
    SAVETMPS;
    PUSHSTACKi(PERLSI_MAGIC);
    if (magic_methcall(sv, mg, "FETCHSIZE", G_SCALAR, 2, NULL)) {
        sv = *PL_stack_sp--;
        retval = SvIV(sv)-1;
        if (retval < -1)
            Perl_croak(aTHX_ "FETCHSIZE returned a negative value");
    }
    POPSTACK;
    FREETMPS;
    LEAVE;
    return (U32) retval;
}

int
Perl_magic_wipepack(pTHX_ SV *sv, MAGIC *mg)
{
    dVAR; dSP;

    PERL_ARGS_ASSERT_MAGIC_WIPEPACK;

    ENTER;
    PUSHSTACKi(PERLSI_MAGIC);
    PUSHMARK(SP);
    XPUSHs(SvTIED_obj(sv, mg));
    PUTBACK;
    call_method("CLEAR", G_SCALAR|G_DISCARD);
    POPSTACK;
    LEAVE;

    return 0;
}

int
Perl_magic_nextpack(pTHX_ SV *sv, MAGIC *mg, SV *key)
{
    dVAR; dSP;
    const char * const meth = SvOK(key) ? "NEXTKEY" : "FIRSTKEY";

    PERL_ARGS_ASSERT_MAGIC_NEXTPACK;

    ENTER;
    SAVETMPS;
    PUSHSTACKi(PERLSI_MAGIC);
    PUSHMARK(SP);
    EXTEND(SP, 2);
    PUSHs(SvTIED_obj(sv, mg));
    if (SvOK(key))
        PUSHs(key);
    PUTBACK;

    if (call_method(meth, G_SCALAR))
        sv_setsv(key, *PL_stack_sp--);

    POPSTACK;
    FREETMPS;
    LEAVE;
    return 0;
}

int
Perl_magic_existspack(pTHX_ SV *sv, const MAGIC *mg)
{
    PERL_ARGS_ASSERT_MAGIC_EXISTSPACK;

    return magic_methpack(sv,mg,"EXISTS");
}

SV *
Perl_magic_scalarpack(pTHX_ HV *hv, MAGIC *mg)
{
    dVAR; dSP;
    SV *retval;
    SV * const tied = SvTIED_obj((SV*)hv, mg);
    HV * const pkg = SvSTASH((SV*)SvRV(tied));
   
    PERL_ARGS_ASSERT_MAGIC_SCALARPACK;

    if (!gv_fetchmethod(pkg, "SCALAR")) {
        SV *key;
        if (HvEITER_get(hv))
            /* we are in an iteration so the hash cannot be empty */
            return &PL_sv_yes;
        /* no xhv_eiter so now use FIRSTKEY */
        key = sv_newmortal();
        magic_nextpack((SV*)hv, mg, key);
        HvEITER_set(hv, NULL);     /* need to reset iterator */
        return SvOK(key) ? &PL_sv_yes : &PL_sv_no;
    }
   
    /* there is a SCALAR method that we can call */
    ENTER;
    PUSHSTACKi(PERLSI_MAGIC);
    PUSHMARK(SP);
    EXTEND(SP, 1);
    PUSHs(tied);
    PUTBACK;

    if (call_method("SCALAR", G_SCALAR))
        retval = *PL_stack_sp--; 
    else
        retval = &PL_sv_undef;
    POPSTACK;
    LEAVE;
    return retval;
}

int
Perl_magic_setdbline(pTHX_ SV *sv, MAGIC *mg)
{
    dVAR;
    GV * const gv = PL_DBline;
    const I32 i = SvTRUE(sv);
    SV ** const svp = av_fetch(GvAV(gv),
                     atoi(MgPV_nolen_const(mg)), FALSE);

    PERL_ARGS_ASSERT_MAGIC_SETDBLINE;

    if (svp && SvIOKp(*svp)) {
        OP * const o = INT2PTR(OP*,SvIVX(*svp));
        if (o) {
            /* set or clear breakpoint in the relevant control op */
            if (i)
                o->op_flags |= OPf_SPECIAL;
            else
                o->op_flags &= ~OPf_SPECIAL;
        }
    }
    return 0;
}

int
Perl_magic_getpos(pTHX_ SV *sv, MAGIC *mg)
{
    dVAR;
    SV* const lsv = LvTARG(sv);

    PERL_ARGS_ASSERT_MAGIC_GETPOS;
    PERL_UNUSED_ARG(mg);

    if (SvTYPE(lsv) >= SVt_PVMG && SvMAGIC(lsv)) {
        MAGIC * const found = mg_find(lsv, PERL_MAGIC_regex_global);
        if (found && found->mg_len >= 0) {
            I32 i = found->mg_len;
            if (DO_UTF8(lsv))
                sv_pos_b2u(lsv, &i);
            sv_setiv(sv, i);
            return 0;
        }
    }
    SvOK_off(sv);
    return 0;
}

int
Perl_magic_setpos(pTHX_ SV *sv, MAGIC *mg)
{
    dVAR;
    SV* const lsv = LvTARG(sv);
    SSize_t pos;
    STRLEN len;
    STRLEN ulen = 0;
    MAGIC* found;

    PERL_ARGS_ASSERT_MAGIC_SETPOS;
    PERL_UNUSED_ARG(mg);

    if (SvTYPE(lsv) >= SVt_PVMG && SvMAGIC(lsv))
        found = mg_find(lsv, PERL_MAGIC_regex_global);
    else
        found = NULL;
    if (!found) {
        if (!SvOK(sv))
            return 0;
#ifdef PERL_OLD_COPY_ON_WRITE
    if (SvIsCOW(lsv))
        sv_force_normal_flags(lsv, 0);
#endif
        found = sv_magicext(lsv, NULL, PERL_MAGIC_regex_global, &PL_vtbl_mglob,
                            NULL, 0);
    }
    else if (!SvOK(sv)) {
        found->mg_len = -1;
        return 0;
    }
    len = SvPOK(lsv) ? SvCUR(lsv) : sv_len(lsv);

    pos = SvIV(sv);

    if (DO_UTF8(lsv)) {
        ulen = sv_len_utf8(lsv);
        if (ulen)
            len = ulen;
    }

    if (pos < 0) {
        pos += len;
        if (pos < 0)
            pos = 0;
    }
    else if (pos > (SSize_t)len)
        pos = len;

    if (ulen) {
        I32 p = pos;
        sv_pos_u2b(lsv, &p, 0);
        pos = p;
    }

    found->mg_len = pos;
    found->mg_flags &= ~MGf_MINMATCH;

    return 0;
}

int
Perl_magic_gettaint(pTHX_ SV *sv, MAGIC *mg)
{
    dVAR;

    PERL_ARGS_ASSERT_MAGIC_GETTAINT;
    PERL_UNUSED_ARG(sv);

    TAINT_IF((PL_localizing != 1) && (mg->mg_len & 1));
    return 0;
}

int
Perl_magic_settaint(pTHX_ SV *sv, MAGIC *mg)
{
    dVAR;

    PERL_ARGS_ASSERT_MAGIC_SETTAINT;
    PERL_UNUSED_ARG(sv);

    /* update taint status */
    if (PL_tainted)
        mg->mg_len |= 1;
    else
        mg->mg_len &= ~1;
    return 0;
}

int
Perl_magic_getvec(pTHX_ SV *sv, MAGIC *mg)
{
    SV * const lsv = LvTARG(sv);

    PERL_ARGS_ASSERT_MAGIC_GETVEC;
    PERL_UNUSED_ARG(mg);

    if (lsv)
        sv_setuv(sv, do_vecget(lsv, LvTARGOFF(sv), LvTARGLEN(sv)));
    else
        SvOK_off(sv);

    return 0;
}

int
Perl_magic_setvec(pTHX_ SV *sv, MAGIC *mg)
{
    PERL_ARGS_ASSERT_MAGIC_SETVEC;
    PERL_UNUSED_ARG(mg);
    do_vecset(sv);      /* XXX slurp this routine */
    return 0;
}

int
Perl_magic_getdefelem(pTHX_ SV *sv, MAGIC *mg)
{
    dVAR;
    SV *targ = NULL;

    PERL_ARGS_ASSERT_MAGIC_GETDEFELEM;

    if (LvTARGLEN(sv)) {
        if (mg->mg_obj) {
            SV * const ahv = LvTARG(sv);
            HE * const he = hv_fetch_ent((HV*)ahv, mg->mg_obj, FALSE, 0);
            if (he)
                targ = HeVAL(he);
        }
        else {
            AV* const av = (AV*)LvTARG(sv);
            if ((I32)LvTARGOFF(sv) <= AvFILL(av))
                targ = AvARRAY(av)[LvTARGOFF(sv)];
        }
        if (targ && (targ != &PL_sv_undef)) {
            /* somebody else defined it for us */
            SvREFCNT_dec(LvTARG(sv));
            LvTARG(sv) = SvREFCNT_inc_simple_NN(targ);
            LvTARGLEN(sv) = 0;
            SvREFCNT_dec(mg->mg_obj);
            mg->mg_obj = NULL;
            mg->mg_flags &= ~MGf_REFCOUNTED;
        }
    }
    else
        targ = LvTARG(sv);
    sv_setsv(sv, targ ? targ : &PL_sv_undef);
    return 0;
}

int
Perl_magic_setdefelem(pTHX_ SV *sv, MAGIC *mg)
{
    PERL_ARGS_ASSERT_MAGIC_SETDEFELEM;
    PERL_UNUSED_ARG(mg);
    if (LvTARGLEN(sv))
        vivify_defelem(sv);
    if (LvTARG(sv)) {
        sv_setsv(LvTARG(sv), sv);
        SvSETMAGIC(LvTARG(sv));
    }
    return 0;
}

void
Perl_vivify_defelem(pTHX_ SV *sv)
{
    dVAR;
    MAGIC *mg;
    SV *value = NULL;

    PERL_ARGS_ASSERT_VIVIFY_DEFELEM;

    if (!LvTARGLEN(sv) || !(mg = mg_find(sv, PERL_MAGIC_defelem)))
        return;
    if (mg->mg_obj) {
        SV * const ahv = LvTARG(sv);
        HE * const he = hv_fetch_ent((HV*)ahv, mg->mg_obj, TRUE, 0);
        if (he)
            value = HeVAL(he);
        if (!value || value == &PL_sv_undef)
            Perl_croak(aTHX_ PL_no_helem_sv, SVfARG(mg->mg_obj));
    }
    else {
        AV* const av = (AV*)LvTARG(sv);
        if ((I32)LvTARGLEN(sv) < 0 && (I32)LvTARGOFF(sv) > AvFILL(av))
            LvTARG(sv) = NULL;  /* array can't be extended */
        else {
            SV* const * const svp = av_fetch(av, LvTARGOFF(sv), TRUE);
            if (!svp || (value = *svp) == &PL_sv_undef)
                Perl_croak(aTHX_ PL_no_aelem, (I32)LvTARGOFF(sv));
        }
    }
    SvREFCNT_inc_simple_void(value);
    SvREFCNT_dec(LvTARG(sv));
    LvTARG(sv) = value;
    LvTARGLEN(sv) = 0;
    SvREFCNT_dec(mg->mg_obj);
    mg->mg_obj = NULL;
    mg->mg_flags &= ~MGf_REFCOUNTED;
}

int
Perl_magic_killbackrefs(pTHX_ SV *sv, MAGIC *mg)
{
    PERL_ARGS_ASSERT_MAGIC_KILLBACKREFS;
    return Perl_sv_kill_backrefs(aTHX_ sv, (AV*)mg->mg_obj);
}

int
Perl_magic_setmglob(pTHX_ SV *sv, MAGIC *mg)
{
    PERL_ARGS_ASSERT_MAGIC_SETMGLOB;
    PERL_UNUSED_CONTEXT;
    mg->mg_len = -1;
    SvSCREAM_off(sv);
    return 0;
}

int
Perl_magic_setuvar(pTHX_ SV *sv, MAGIC *mg)
{
    const struct ufuncs * const uf = (struct ufuncs *)mg->mg_ptr;

    PERL_ARGS_ASSERT_MAGIC_SETUVAR;

    if (uf && uf->uf_set)
        (*uf->uf_set)(aTHX_ uf->uf_index, sv);
    return 0;
}

int
Perl_magic_setregexp(pTHX_ SV *sv, MAGIC *mg)
{
    const char type = mg->mg_type;

    PERL_ARGS_ASSERT_MAGIC_SETREGEXP;

    if (type == PERL_MAGIC_qr) {
    } else {
	assert(type == PERL_MAGIC_bm);
	SvTAIL_off(sv);
	SvVALID_off(sv);
    }
    return sv_unmagic(sv, type);
}

/* Just clear the UTF-8 cache data. */
int
Perl_magic_setutf8(pTHX_ SV *sv, MAGIC *mg)
{
    PERL_ARGS_ASSERT_MAGIC_SETUTF8;
    PERL_UNUSED_CONTEXT;
    PERL_UNUSED_ARG(sv);
    Safefree(mg->mg_ptr);       /* The mg_ptr holds the pos cache. */
    mg->mg_ptr = NULL;
    mg->mg_len = -1;            /* The mg_len holds the len cache. */
    return 0;
}

int
Perl_magic_set(pTHX_ SV *sv, MAGIC *mg)
{
    dVAR;
    register const char *s;
    register I32 paren;
    register const REGEXP * rx;
    const char * const remaining = mg->mg_ptr + 1;
    I32 i;
    STRLEN len;

    PERL_ARGS_ASSERT_MAGIC_SET;

    switch (*mg->mg_ptr) {
    case '^':
	if (remaining[1] != '\0') {
	    switch (*remaining) {
	    case 'D':   /* $^DIE_HOOK */
		if (strEQ(remaining, "DIE_HOOK")) {
		    SvREFCNT_dec(PL_diehook);
		    PL_diehook = newSVsv(sv);
		}
		break;
	    case 'G':
		if (strEQ(remaining, "GID")) { /* $^GID */
		    PL_gid = SvIV(sv);
		    if (PL_delaymagic) {
			PL_delaymagic |= DM_RGID;
			break;                              /* don't do magic till later */
		    }
#ifdef HAS_SETRGID
		    (void)setrgid((Gid_t)PL_gid);
#else
#ifdef HAS_SETREGID
		    (void)setregid((Gid_t)PL_gid, (Gid_t)-1);
#else
#ifdef HAS_SETRESGID
		    (void)setresgid((Gid_t)PL_gid, (Gid_t)-1, (Gid_t) 1);
#else
		    if (PL_gid == PL_egid)                  /* special case $( = $) */
			(void)PerlProc_setgid(PL_gid);
		    else {
			PL_gid = PerlProc_getgid();
			Perl_croak(aTHX_ "setrgid() not implemented");
		    }
#endif
#endif
#endif
		    PL_gid = PerlProc_getgid();
		    PL_tainting |= (PL_uid && (PL_euid != PL_uid || PL_egid != PL_gid));
		}
		break;
	    case 'E':
		if (strEQ(remaining, "EGID")) {  /* $^EGID */
#ifdef HAS_SETGROUPS
		    const char *p = SvPV_const(sv, len);
		    Groups_t *gary = NULL;

		    while (isSPACE(*p))
			++p;
		    PL_egid = Atol(p);
		    for (i = 0; i < NGROUPS; ++i) {
			while (*p && !isSPACE(*p))
			    ++p;
			while (isSPACE(*p))
			    ++p;
			if (!*p)
			    break;
			if(!gary)
			    Newx(gary, i + 1, Groups_t);
			else
			    Renew(gary, i + 1, Groups_t);
			gary[i] = Atol(p);
		    }
		    if (i)
			(void)setgroups(i, gary);
		    Safefree(gary);
#else  /* HAS_SETGROUPS */
		    PL_egid = SvIV(sv);
#endif /* HAS_SETGROUPS */
		    if (PL_delaymagic) {
			PL_delaymagic |= DM_EGID;
			break;                              /* don't do magic till later */
		    }
#ifdef HAS_SETEGID
		    (void)setegid((Gid_t)PL_egid);
#else
#ifdef HAS_SETREGID
		    (void)setregid((Gid_t)-1, (Gid_t)PL_egid);
#else
#ifdef HAS_SETRESGID
		    (void)setresgid((Gid_t)-1, (Gid_t)PL_egid, (Gid_t)-1);
#else
		    if (PL_egid == PL_gid)                  /* special case $) = $( */
			(void)PerlProc_setgid(PL_egid);
		    else {
			PL_egid = PerlProc_getegid();
			Perl_croak(aTHX_ "setegid() not implemented");
		    }
#endif
#endif
#endif
		    PL_egid = PerlProc_getegid();
		    PL_tainting |= (PL_uid && (PL_euid != PL_uid || PL_egid != PL_gid));
		    break;
		}
		break;
	    case 'M':   /* $^MATCH */
		if (strEQ(remaining, "MATCH"))
		    goto do_match;
		break;
	    case 'O':   /* $^OPEN */
		if (strEQ(remaining, "OPEN")) {
		    STRLEN len;
		    const char *const start = SvPV(sv, len);
		    const char *out = (const char*)memchr(start, '\0', len);
		    SV *tmp;
		    HV* old_cop_hints_hash;


		    PL_compiling.cop_hints |= HINT_LEXICAL_IO_IN | HINT_LEXICAL_IO_OUT;
		    PL_hints
			|= HINT_LOCALIZE_HH | HINT_LEXICAL_IO_IN | HINT_LEXICAL_IO_OUT;

		    /* Opening for input is more common than opening for output, so
		       ensure that hints for input are sooner on linked list.  */

		    old_cop_hints_hash = PL_compiling.cop_hints_hash;
		    PL_compiling.cop_hints_hash = newHVhv(PL_compiling.cop_hints_hash);
		    SvREFCNT_dec(old_cop_hints_hash);

		    tmp = out ? newSVpvn_flags(out + 1, start + len - out - 1, 0) : newSVpvs_flags("", 0);
		    (void)hv_store_ent(PL_compiling.cop_hints_hash, 
				       newSVpvs_flags("open>", SVs_TEMP), tmp, 0);

		    tmp = newSVpvn_flags(start, out ? (STRLEN)(out - start) : len, 0);
		    (void)hv_store_ent(PL_compiling.cop_hints_hash,
				       newSVpvs_flags("open<", SVs_TEMP), tmp, 0);
		}
		break;
	    case 'P':
		if (strEQ(remaining, "PREMATCH")) { /* $^PREMATCH */
		    goto do_prematch;
		} 
		if (strEQ(remaining, "POSTMATCH")) { /* $^POSTMATCH */
		    goto do_postmatch;
		}
		break;
	    case 'U':        /* ^UTF8CACHE */
		if (strEQ(remaining, "UTF8CACHE")) {
		    PL_utf8cache = (signed char) sv_2iv(sv);
		}
		break;
	    case 'W':
		if (strEQ(remaining, "WARNING_BITS")) { /* $^WARNING_BITS */
		    if ( ! (PL_dowarn & G_WARN_ALL_MASK)) {
			if (!SvPOK(sv) && PL_localizing) {
			    sv_setpvn(sv, WARN_NONEstring, WARNsize);
			    PL_compiling.cop_warnings = pWARN_NONE;
			    break;
			}
			{
			    STRLEN len, i;
			    int accumulate = 0 ;
			    int any_fatals = 0 ;
			    const char * const ptr = SvPV_const(sv, len) ;
			    for (i = 0 ; i < len ; ++i) {
				accumulate |= ptr[i] ;
				any_fatals |= (ptr[i] & 0xAA) ;
			    }
			    if (!accumulate) {
				if (!specialWARN(PL_compiling.cop_warnings))
				    PerlMemShared_free(PL_compiling.cop_warnings);
				PL_compiling.cop_warnings = pWARN_NONE;
			    }
			    /* Yuck. I can't see how to abstract this:  */
			    else if (isWARN_on(((STRLEN *)SvPV_nolen_const(sv)) - 1,
					       WARN_ALL) && !any_fatals) {
				if (!specialWARN(PL_compiling.cop_warnings))
				    PerlMemShared_free(PL_compiling.cop_warnings);
				PL_compiling.cop_warnings = pWARN_ALL;
				PL_dowarn |= G_WARN_ONCE ;
			    }
			    else {
				STRLEN len;
				const char *const p = SvPV_const(sv, len);
				
				PL_compiling.cop_warnings
				    = Perl_new_warnings_bitfield(aTHX_ PL_compiling.cop_warnings,
								 p, len);

				if (isWARN_on(PL_compiling.cop_warnings, WARN_ONCE))
				    PL_dowarn |= G_WARN_ONCE ;
			    }
			    
			}
		    }
		}
		else if (strEQ(remaining, "WARN_HOOK")) { /* $^WARN_HOOK */
		    SvREFCNT_dec(PL_warnhook);
		    PL_warnhook = newSVsv(sv);
		}
		break;
	    }
	}
	else {
	    switch (*remaining) {
	    case 'C':        /* ^C */
		PL_minus_c = (bool)SvIV(sv);
		break;
	    
	    case 'D':        /* ^D */
#ifdef DEBUGGING
		s = SvPV_nolen_const(sv);
		PL_debug = get_debug_opts(&s, 0) | DEBUG_TOP_FLAG;
		DEBUG_x(dump_all());
#else
		PL_debug = (SvIV(sv)) | DEBUG_TOP_FLAG;
#endif
		break;

	    case 'E':  /* ^E */
#ifdef MACOS_TRADITIONAL
		gMacPerl_OSErr = SvIV(sv);
#else
#  ifdef VMS
		set_vaxc_errno(SvIV(sv));
#  else
#    ifdef WIN32
		SetLastError( SvIV(sv) );
#    else
#      ifdef OS2
		os2_setsyserrno(SvIV(sv));
#      else
		/* will anyone ever use this? */
		SETERRNO(SvIV(sv), 4);
#      endif
#    endif
#  endif
#endif
		break;
	    case 'F':        /* ^F */
		PL_maxsysfd = SvIV(sv);
		break;
	    case 'H':        /* ^H */
		PL_hints = SvIV(sv);
		break;
	    case 'I':        /* ^I */ /* NOT \t in EBCDIC */
		Safefree(PL_inplace);
		PL_inplace = SvOK(sv) ? savesvpv(sv) : NULL;
		break;
	    case 'O':        /* ^O */
		Safefree(PL_osname);
		PL_osname = NULL;
		if (SvOK(sv)) {
		    TAINT_PROPER("assigning to $^O");
		    PL_osname = savesvpv(sv);
		}
		break;
	    case 'P':        /* ^P */
		PL_perldb = SvIV(sv);
		if (PL_perldb && !PL_DBsingle)
		    init_debugger();
		break;
	    case 'T':        /* ^T */
#ifdef BIG_TIME
		PL_basetime = (Time_t)(SvNOK(sv) ? SvNVX(sv) : sv_2nv(sv));
#else
		PL_basetime = (Time_t)SvIV(sv);
#endif
		break;
	    case 'W':        /* ^W */
		if ( ! (PL_dowarn & G_WARN_ALL_MASK)) {
		    i = SvIV(sv);
		    PL_dowarn = (PL_dowarn & ~G_WARN_ON)
			| (i ? G_WARN_ON : G_WARN_OFF) ;
		}
	    }
	}
	break;
    case '`': /* $^PREMATCH caught below */
      do_prematch:
      paren = RX_BUFF_IDX_PREMATCH;
      goto setparen;
    case '\'': /* $^POSTMATCH caught below */
      do_postmatch:
      paren = RX_BUFF_IDX_POSTMATCH;
      goto setparen;
    case '&':
      do_match:
      paren = RX_BUFF_IDX_FULLMATCH;
      goto setparen;
    case '1': case '2': case '3': case '4':
    case '5': case '6': case '7': case '8': case '9':
      paren = atoi(mg->mg_ptr);
      setparen:
        if (PL_curpm && (rx = PM_GETRE(PL_curpm))) {
            CALLREG_NUMBUF_STORE((REGEXP * const)rx,paren,sv);
            break;
        } else {
            /* Croak with a READONLY error when a numbered match var is
             * set without a previous pattern match. Unless it's C<local $1>
             */
            if (!PL_localizing) {
                Perl_croak(aTHX_ PL_no_modify);
            }
        }
    case '|':
        {
            IO * const io = GvIOp(PL_defoutgv);
            if(!io)
              break;
            if ((SvIV(sv)) == 0)
                IoFLAGS(io) &= ~IOf_FLUSH;
            else {
                if (!(IoFLAGS(io) & IOf_FLUSH)) {
                    PerlIO *ofp = IoOFP(io);
                    if (ofp)
                        (void)PerlIO_flush(ofp);
                    IoFLAGS(io) |= IOf_FLUSH;
                }
            }
        }
        break;
    case '/':
        SVcpSTEAL(PL_rs, newSVsv(sv));
        break;
    case '\\':
        if (PL_ors_sv)
            SvREFCNT_dec(PL_ors_sv);
        if (SvOK(sv) || SvGMAGICAL(sv)) {
            PL_ors_sv = newSVsv(sv);
        }
        else {
            PL_ors_sv = NULL;
        }
        break;
    case ',':
        if (PL_ofs_sv)
            SvREFCNT_dec(PL_ofs_sv);
        if (SvOK(sv) || SvGMAGICAL(sv)) {
            PL_ofs_sv = newSVsv(sv);
        }
        else {
            PL_ofs_sv = NULL;
        }
        break;
    case '?':
#ifdef COMPLEX_STATUS
        if (PL_localizing == 2) {
            PL_statusvalue = LvTARGOFF(sv);
            PL_statusvalue_vms = LvTARGLEN(sv);
        }
        else
#endif
#ifdef VMSISH_STATUS
        if (VMSISH_STATUS)
            STATUS_NATIVE_CHILD_SET((U32)SvIV(sv));
        else
#endif
            STATUS_UNIX_EXIT_SET(SvIV(sv));
        break;
    case '!':
        {
#ifdef VMS
#   define PERL_VMS_BANG vaxc$errno
#else
#   define PERL_VMS_BANG 0
#endif
        SETERRNO(SvIOK(sv) ? SvIVX(sv) : SvOK(sv) ? sv_2iv(sv) : 0,
                 (SvIV(sv) == EVMSERR) ? 4 : PERL_VMS_BANG);
        }
        break;
    case '<':
        PL_uid = SvIV(sv);
        if (PL_delaymagic) {
            PL_delaymagic |= DM_RUID;
            break;                              /* don't do magic till later */
        }
#ifdef HAS_SETRUID
        (void)setruid((Uid_t)PL_uid);
#else
#ifdef HAS_SETREUID
        (void)setreuid((Uid_t)PL_uid, (Uid_t)-1);
#else
#ifdef HAS_SETRESUID
      (void)setresuid((Uid_t)PL_uid, (Uid_t)-1, (Uid_t)-1);
#else
        if (PL_uid == PL_euid) {                /* special case $< = $> */
#ifdef PERL_DARWIN
            /* workaround for Darwin's setuid peculiarity, cf [perl #24122] */
            if (PL_uid != 0 && PerlProc_getuid() == 0)
                (void)PerlProc_setuid(0);
#endif
            (void)PerlProc_setuid(PL_uid);
        } else {
            PL_uid = PerlProc_getuid();
            Perl_croak(aTHX_ "setruid() not implemented");
        }
#endif
#endif
#endif
        PL_uid = PerlProc_getuid();
        PL_tainting |= (PL_uid && (PL_euid != PL_uid || PL_egid != PL_gid));
        break;
    case '>':
        PL_euid = SvIV(sv);
        if (PL_delaymagic) {
            PL_delaymagic |= DM_EUID;
            break;                              /* don't do magic till later */
        }
#ifdef HAS_SETEUID
        (void)seteuid((Uid_t)PL_euid);
#else
#ifdef HAS_SETREUID
        (void)setreuid((Uid_t)-1, (Uid_t)PL_euid);
#else
#ifdef HAS_SETRESUID
        (void)setresuid((Uid_t)-1, (Uid_t)PL_euid, (Uid_t)-1);
#else
        if (PL_euid == PL_uid)          /* special case $> = $< */
            PerlProc_setuid(PL_euid);
        else {
            PL_euid = PerlProc_geteuid();
            Perl_croak(aTHX_ "seteuid() not implemented");
        }
#endif
#endif
#endif
        PL_euid = PerlProc_geteuid();
        PL_tainting |= (PL_uid && (PL_euid != PL_uid || PL_egid != PL_gid));
        break;
    case ':':
        PL_chopset = SvPV_force(sv,len);
        break;
#ifndef MACOS_TRADITIONAL
    case '0':
        LOCK_DOLLARZERO_MUTEX;
#ifdef HAS_SETPROCTITLE
        /* The BSDs don't show the argv[] in ps(1) output, they
         * show a string from the process struct and provide
         * the setproctitle() routine to manipulate that. */
        if (PL_origalen != 1) {
            s = SvPV_const(sv, len);
#   if __FreeBSD_version > 410001
            /* The leading "-" removes the "perl: " prefix,
             * but not the "(perl) suffix from the ps(1)
             * output, because that's what ps(1) shows if the
             * argv[] is modified. */
            setproctitle("-%s", s);
#   else        /* old FreeBSDs, NetBSD, OpenBSD, anyBSD */
            /* This doesn't really work if you assume that
             * $0 = 'foobar'; will wipe out 'perl' from the $0
             * because in ps(1) output the result will be like
             * sprintf("perl: %s (perl)", s)
             * I guess this is a security feature:
             * one (a user process) cannot get rid of the original name.
             * --jhi */
            setproctitle("%s", s);
#   endif
        }
#elif defined(__hpux) && defined(PSTAT_SETCMD)
        if (PL_origalen != 1) {
             union pstun un;
             s = SvPV_const(sv, len);
             un.pst_command = (char *)s;
             pstat(PSTAT_SETCMD, un, len, 0, 0);
        }
#else
        if (PL_origalen > 1) {
            /* PL_origalen is set in perl_parse(). */
            s = SvPV_force(sv,len);
            if (len >= (STRLEN)PL_origalen-1) {
                /* Longer than original, will be truncated. We assume that
                 * PL_origalen bytes are available. */
                Copy(s, PL_origargv[0], PL_origalen-1, char);
            }
            else {
                /* Shorter than original, will be padded. */
#ifdef PERL_DARWIN
                /* Special case for Mac OS X: see [perl #38868] */
                const int pad = 0;
#else
                /* Is the space counterintuitive?  Yes.
                 * (You were expecting \0?)
                 * Does it work?  Seems to.  (In Linux 2.4.20 at least.)
                 * --jhi */
                const int pad = ' ';
#endif
                Copy(s, PL_origargv[0], len, char);
                PL_origargv[0][len] = 0;
                memset(PL_origargv[0] + len + 1,
                       pad,  PL_origalen - len - 1);
            }
            PL_origargv[0][PL_origalen-1] = 0;
            for (i = 1; i < PL_origargc; i++)
                PL_origargv[i] = 0;
        }
#endif
        UNLOCK_DOLLARZERO_MUTEX;
        break;
#endif
    }
    return 0;
}

I32
Perl_whichsig(pTHX_ const char *sig)
{
    register char* const* sigv;

    PERL_ARGS_ASSERT_WHICHSIG;
    PERL_UNUSED_CONTEXT;

    for (sigv = (char* const*)PL_sig_name; *sigv; sigv++)
        if (strEQ(sig,*sigv))
            return PL_sig_num[sigv - (char* const*)PL_sig_name];
#ifdef SIGCLD
    if (strEQ(sig,"CHLD"))
        return SIGCLD;
#endif
#ifdef SIGCHLD
    if (strEQ(sig,"CLD"))
        return SIGCHLD;
#endif
    return -1;
}

Signal_t
#if defined(HAS_SIGACTION) && defined(SA_SIGINFO)
Perl_sighandler(int sig, siginfo_t *sip, void *uap PERL_UNUSED_DECL)
#else
Perl_sighandler(int sig)
#endif
{
#ifdef PERL_GET_SIG_CONTEXT
    dTHXa(PERL_GET_SIG_CONTEXT);
#else
    dTHX;
#endif
    dSP;
    GV *gv = NULL;
    SV *sv = NULL;
    SV * const tSv = PL_Sv;
    CV *cv = NULL;
    OP *myop = PL_op;
    U32 flags = 0;
    XPV * const tXpv = PL_Xpv;

    if (PL_savestack_ix + 15 <= PL_savestack_max)
        flags |= 1;
    if (PL_markstack_ptr < PL_markstack_max - 2)
        flags |= 4;
    if (PL_scopestack_ix < PL_scopestack_max - 3)
        flags |= 16;

    if (!PL_psig_ptr[sig]) {
/* 	PerlIO_printf(Perl_error_log, "Signal SIG%s received, but no signal handler set.\n", */
/* 	    PL_sig_name[sig]); */
/* 	exit(sig); */
	return;
    }

    /* Max number of items pushed there is 3*n or 4. We cannot fix
       infinity, so we fix 4 (in fact 5): */
    if (flags & 1) {
        PL_savestack_ix += 5;           /* Protect save in progress. */
        SAVEDESTRUCTOR_X(S_unwind_handler_stack, (void*)&flags);
    }
    if (flags & 4)
        PL_markstack_ptr++;             /* Protect mark. */
    if (flags & 16)
        PL_scopestack_ix += 1;

    if (!(cv = (CV*)PL_psig_ptr[sig])
        || SvTYPE(cv) != SVt_PVCV) {
	Perl_croak(aTHX "SIG%s handler is not valid", PL_sig_name[sig]);
    }

    if (!cv || !CvROOT(cv)) {
        if (ckWARN(WARN_SIGNAL))
            Perl_warner(aTHX_ packWARN(WARN_SIGNAL), "SIG%s handler \"%s\" not defined.\n",
                PL_sig_name[sig], (gv ? GvENAME(gv)
                                : ((cv && CvGV(cv))
                                   ? GvENAME(CvGV(cv))
                                   : "__ANON__")));
        goto cleanup;
    }

    if(PL_psig_name[sig]) {
        sv = SvREFCNT_inc_NN(PL_psig_name[sig]);
        flags |= 64;
#if !defined(PERL_IMPLICIT_CONTEXT)
        PL_sig_sv = sv;
#endif
    } else {
        sv = sv_newmortal();
        sv_setpv(sv,PL_sig_name[sig]);
    }

    PUSHSTACKi(PERLSI_SIGNAL);
    PUSHMARK(SP);
    PUSHs(sv);
#if defined(HAS_SIGACTION) && defined(SA_SIGINFO)
    {
         struct sigaction oact;

         if (sigaction(sig, 0, &oact) == 0 && oact.sa_flags & SA_SIGINFO) {
              if (sip) {
                   HV *sih = newHV();
                   SV *rv  = newRV_noinc((SV*)sih);
                   /* The siginfo fields signo, code, errno, pid, uid,
                    * addr, status, and band are defined by POSIX/SUSv3. */
                   (void)hv_stores(sih, "signo", newSViv(sip->si_signo));
                   (void)hv_stores(sih, "code", newSViv(sip->si_code));
#if 0 /* XXX TODO: Configure scan for the existence of these, but even that does not help if the SA_SIGINFO is not implemented according to the spec. */
		   hv_stores(sih, "errno",      newSViv(sip->si_errno));
		   hv_stores(sih, "status",     newSViv(sip->si_status));
		   hv_stores(sih, "uid",        newSViv(sip->si_uid));
		   hv_stores(sih, "pid",        newSViv(sip->si_pid));
		   hv_stores(sih, "addr",       newSVuv(PTR2UV(sip->si_addr)));
		   hv_stores(sih, "band",       newSViv(sip->si_band));
#endif
		   EXTEND(SP, 2);
		   PUSHs((SV*)rv);
		   mPUSHp((char *)sip, sizeof(*sip));
	      }

         }
    }
#endif
    PUTBACK;

    call_sv((SV*)cv, G_DISCARD|G_EVAL);

    POPSTACK;
    if (SvTRUE(ERRSV)) {
#ifndef PERL_MICRO
#ifdef HAS_SIGPROCMASK
        /* Handler "died", for example to get out of a restart-able read().
         * Before we re-do that on its behalf re-enable the signal which was
         * blocked by the system when we entered.
         */
        sigset_t set;
        sigemptyset(&set);
        sigaddset(&set,sig);
        sigprocmask(SIG_UNBLOCK, &set, NULL);
#else
        /* Not clear if this will work */
        (void)rsignal(sig, SIG_IGN);
        (void)rsignal(sig, PL_csighandlerp);
#endif
#endif /* !PERL_MICRO */
        Perl_vdie_common(aTHX_ ERRSV, FALSE);
	die_where(ERRSV);
    }
cleanup:
    if (flags & 1)
	PL_savestack_ix -= 8; /* Unprotect save in progress. */
    if (flags & 4)
	PL_markstack_ptr--;
    if (flags & 16)
	PL_scopestack_ix -= 1;
    if (flags & 64)
	SvREFCNT_dec(sv);
    PL_op = myop;			/* Apparently not needed... */

    PL_Sv = tSv;			/* Restore global temporaries. */
    PL_Xpv = tXpv;
    return;
}


static void
S_restore_magic(pTHX_ const void *p)
{
    dVAR;
    MGS* const mgs = SSPTR(PTR2IV(p), MGS*);
    SV* const sv = mgs->mgs_sv;

    if (!sv)
        return;

    if (SvTYPE(sv) >= SVt_PVMG && SvMAGIC(sv))
    {
#ifdef PERL_OLD_COPY_ON_WRITE
	/* While magic was saved (and off) sv_setsv may well have seen
	   this SV as a prime candidate for COW.  */
	if (SvIsCOW(sv))
	    sv_force_normal_flags(sv, 0);
#endif

	if (mgs->mgs_flags)
	    SvFLAGS(sv) |= mgs->mgs_flags;
	else
	    mg_magical(sv);
	if (SvGMAGICAL(sv)) {
	    /* downgrade public flags to private,
	       and discard any other private flags */

	    const U32 pubflags = SvFLAGS(sv) & (SVf_IOK|SVf_NOK|SVf_POK);
	    if (pubflags) {
		SvFLAGS(sv) &= ~( pubflags | (SVp_IOK|SVp_NOK|SVp_POK) );
		SvFLAGS(sv) |= ( pubflags << PRIVSHIFT );
	    }
	}
    }

    mgs->mgs_sv = NULL;  /* mark the MGS structure as restored */

    /* If we're still on top of the stack, pop us off.  (That condition
     * will be satisfied if restore_magic was called explicitly, but *not*
     * if it's being called via leave_scope.)
     * The reason for doing this is that otherwise, things like sv_2cv()
     * may leave alloc gunk on the savestack, and some code
     * (e.g. sighandler) doesn't expect that...
     */
    if (PL_savestack_ix == mgs->mgs_ss_ix)
    {
	I32 popval = SSPOPINT;
        assert(popval == SAVEt_DESTRUCTOR_X);
        PL_savestack_ix -= 2;
	popval = SSPOPINT;
        assert(popval == SAVEt_ALLOC);
	popval = SSPOPINT;
        PL_savestack_ix -= popval;
    }

}

static void
S_unwind_handler_stack(pTHX_ const void *p)
{
    dVAR;
    const U32 flags = *(const U32*)p;

    PERL_ARGS_ASSERT_UNWIND_HANDLER_STACK;

    if (flags & 1)
	PL_savestack_ix -= 5; /* Unprotect save in progress. */
#if !defined(PERL_IMPLICIT_CONTEXT)
    if (flags & 64)
	SvREFCNT_dec(PL_sig_sv);
#endif
}

/*
=for apidoc magic_sethint

Triggered by a store to %^H, records the key/value pair to
C<PL_compiling.cop_hints_hash>.  It is assumed that hints aren't storing
anything that would need a deep copy.  Maybe we should warn if we find a
reference.

=cut
*/
int
Perl_magic_sethint(pTHX_ SV *sv, MAGIC *mg)
{
    dVAR;
    HV * new_hinthash;
    if(!(mg->mg_len == HEf_SVKEY))
	assert(mg->mg_len == HEf_SVKEY);

    PERL_ARGS_ASSERT_MAGIC_SETHINT;

    /* mg->mg_obj isn't being used.  If needed, it would be possible to store
       an alternative leaf in there, with PL_compiling.cop_hints being used if
       it's NULL. If needed for threads, the alternative could lock a mutex,
       or take other more complex action.  */

    /* Something changed in %^H, so it will need to be restored on scope exit.
       Doing this here saves a lot of doing it manually in perl code (and
       forgetting to do it, and consequent subtle errors.  */
    PL_hints |= HINT_LOCALIZE_HH;

    /* copy the hash, to preserve the old one */
    new_hinthash = newHVhv(PL_compiling.cop_hints_hash);
    SvREFCNT_dec(PL_compiling.cop_hints_hash);
    PL_compiling.cop_hints_hash = new_hinthash;

    (void)hv_store_ent(PL_compiling.cop_hints_hash, (SV *)mg->mg_ptr, newSVsv(sv), 0);
    return 0;
}

/*
=for apidoc magic_sethint

Triggered by a delete from %^H, records the key to
C<PL_compiling.cop_hints_hash>.

=cut
*/
int
Perl_magic_clearhint(pTHX_ SV *sv, MAGIC *mg)
{
    dVAR;
    HV * new_hinthash;

    PERL_ARGS_ASSERT_MAGIC_CLEARHINT;
    PERL_UNUSED_ARG(sv);

    assert(mg->mg_len == HEf_SVKEY);

    PERL_UNUSED_ARG(sv);

    PL_hints |= HINT_LOCALIZE_HH;

    /* copy the hash, to preserve the old one */
    new_hinthash = newHVhv(PL_compiling.cop_hints_hash);
    SvREFCNT_dec(PL_compiling.cop_hints_hash);
    PL_compiling.cop_hints_hash = new_hinthash;

    (void)hv_delete_ent(PL_compiling.cop_hints_hash, (SV *)mg->mg_ptr, 0, 0);
    return 0;
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
