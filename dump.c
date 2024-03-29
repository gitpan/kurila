/*    dump.c
 *
 *    Copyright (C) 1991, 1992, 1993, 1994, 1995, 1996, 1997, 1998, 1999,
 *    2000, 2001, 2002, 2003, 2004, 2005, 2006, 2007, by Larry Wall and others
 *
 *    You may distribute under the terms of either the GNU General Public
 *    License or the Artistic License, as specified in the README file.
 *
 */

/*
 * "'You have talked long in your sleep, Frodo,' said Gandalf gently, 'and
 * it has not been hard for me to read your mind and memory.'"
 */

/* This file contains utility routines to dump the contents of SV and OP
 * structures, as used by command-line options like -Dt and -Dx, and
 * by Devel::Peek.
 *
 * It also holds the debugging version of the  runops function.
 */

#include "EXTERN.h"
#define PERL_IN_DUMP_C
#include "perl.h"
#include "regcomp.h"
#include "proto.h"


static const char* const svtypenames[SVt_LAST] = {
    "NULL",
    "BIND",
    "IV",
    "NV",
    "PV",
    "PVIV",
    "PVNV",
    "PVMG",
    "REGEXP",
    "PVGV",
    "PVAV",
    "PVHV",
    "PVCV",
    "PVIO"
};


static const char* const svshorttypenames[SVt_LAST] = {
    "UNDEF",
    "BIND",
    "IV",
    "NV",
    "PV",
    "PVIV",
    "PVNV",
    "PVMG",
    "REGEXP",
    "GV",
    "AV",
    "HV",
    "CV",
    "IO"
};

#define Sequence PL_op_sequence

void
Perl_dump_indent(pTHX_ I32 level, PerlIO *file, const char* pat, ...)
{
    va_list args;
    PERL_ARGS_ASSERT_DUMP_INDENT;
    va_start(args, pat);
    dump_vindent(level, file, pat, &args);
    va_end(args);
}

void
Perl_dump_vindent(pTHX_ I32 level, PerlIO *file, const char* pat, va_list *args)
{
    dVAR;
    PERL_ARGS_ASSERT_DUMP_VINDENT;
    PerlIO_printf(file, "%*s", (int)(level*PL_dumpindent), "");
    PerlIO_vprintf(file, pat, *args);
}

void
Perl_dump_all(pTHX)
{
    dVAR;
    PerlIO_setlinebuf(Perl_debug_log);
    if (PL_main_root)
	op_dump(RootopOp(PL_main_root));
/*     dump_packsubs(PL_defstash); */
}

void
Perl_dump_packsubs(pTHX_ const HV *stash)
{
    dVAR;
    I32	i;

    PERL_ARGS_ASSERT_DUMP_PACKSUBS;

    if (!HvARRAY(stash))
	return;
    for (i = 0; i <= (I32) HvMAX(stash); i++) {
        const HE *entry;
	for (entry = HvARRAY(stash)[i]; entry; entry = HeNEXT(entry)) {
	    const GV * const gv = (GV*)HeVAL(entry);
	    if (SvTYPE(gv) != SVt_PVGV || !GvGP(gv))
		continue;
	    if (GvCVu(gv))
		dump_sub(gv);
	    if (HeKEY(entry)[HeKLEN(entry)-1] == ':') {
		const HV * const hv = GvHV(gv);
		if (hv && (hv != PL_defstash))
		    dump_packsubs(hv);		/* nested package */
	    }
	}
    }
}

void
Perl_dump_sub(pTHX_ const GV *gv)
{
    SV * const sv = sv_newmortal();

    PERL_ARGS_ASSERT_DUMP_SUB;

    gv_fullname3(sv, gv, NULL);
    Perl_dump_indent(aTHX_ 0, Perl_debug_log, "SUB %s = ", SvPVX_const(sv));
    if (CvISXSUB(GvCV(gv)))
	Perl_dump_indent(aTHX_ 0, Perl_debug_log, "(xsub 0x%"UVxf" %d)\n",
	    PTR2UV(CvXSUB(GvCV(gv))),
	    (int)CvXSUBANY(GvCV(gv)).any_i32);
    else if (CvROOT(GvCV(gv)))
	op_dump(RootopOp(CvROOT(GvCV(gv))));
    else
	Perl_dump_indent(aTHX_ 0, Perl_debug_log, "<undef>\n");
}

void
Perl_dump_eval(pTHX)
{
    dVAR;
    op_dump((OP*)PL_eval_root);
}


/*
=for apidoc Apd|char*|pv_escape|NN SV *dsv|NN const char const *str\
               |const STRLEN count|const STRLEN max
               |STRLEN const *escaped, const U32 flags

Escapes at most the first "count" chars of pv and puts the results into
dsv such that the size of the escaped string will not exceed "max" chars
and will not contain any incomplete escape sequences.

If flags contains PERL_PV_ESCAPE_QUOTE then any double quotes in the string
will also be escaped.

Normally the SV will be cleared before the escaped string is prepared,
but when PERL_PV_ESCAPE_NOCLEAR is set this will not occur.

If PERL_PV_ESCAPE_UNI is set then the input string is treated as Unicode,
if PERL_PV_ESCAPE_UNI_DETECT is set then the input string is scanned
using C<is_utf8_string()> to determine if it is Unicode.

If PERL_PV_ESCAPE_ALL is set then all input chars will be output
using C<\x01F1> style escapes, otherwise only chars above 255 will be
escaped using this style, other non printable chars will use octal or
common escaped patterns like C<\n>. If PERL_PV_ESCAPE_NOBACKSLASH
then all chars below 255 will be treated as printable and 
will be output as literals.

If PERL_PV_ESCAPE_FIRSTCHAR is set then only the first char of the
string will be escaped, regardles of max. If the string is utf8 and 
the chars value is >255 then it will be returned as a plain hex 
sequence. Thus the output will either be a single char, 
an octal escape sequence, a special escape like C<\n> or a 3 or 
more digit hex value. 

If PERL_PV_ESCAPE_RE is set then the escape char used will be a '%' and
not a '\\'. This is because regexes very often contain backslashed
sequences, whereas '%' is not a particularly common character in patterns.

Returns a pointer to the escaped text as held by dsv.

=cut
*/
#define PV_ESCAPE_OCTBUFSIZE 32

const char *
Perl_pv_escape( pTHX_ SV *dsv, char const * const str, 
                const STRLEN count, const STRLEN max, 
                STRLEN * const escaped, const U32 flags ) 
{
    const char esc = (flags & PERL_PV_ESCAPE_RE) ? '%' : '\\';
    const char dq = (flags & PERL_PV_ESCAPE_QUOTE) ? '"' : esc;
    char octbuf[PV_ESCAPE_OCTBUFSIZE] = "%123456789ABCDF";
    STRLEN wrote = 0;    /* chars written so far */
    STRLEN chsize = 0;   /* size of data to be written */
    STRLEN readsize = 1; /* size of data just read */
    bool isuni= flags & PERL_PV_ESCAPE_UNI ? 1 : 0; /* is this Unicode */
    const char *pv  = str;
    const char * const end = pv + count; /* end of string */
    octbuf[0] = esc;

    PERL_ARGS_ASSERT_PV_ESCAPE;

    if (!(flags & PERL_PV_ESCAPE_NOCLEAR)) {
	    /* This won't alter the UTF-8 flag */
	    sv_setpvn(dsv, "", 0);
    }
    
    if ((flags & PERL_PV_ESCAPE_UNI_DETECT) && is_utf8_string(pv, count))
        isuni = 1;
    
    for ( ; (pv < end && (!max || (wrote < max))) ; pv += readsize ) {
        const UV u= (isuni) ? utf8n_to_uvchr(pv, end - pv, &readsize, UTF8_CHECK_ONLY) : (U8)*pv;            
        const U8 c = (U8)u & 0xFF;
        
	if ( readsize == (STRLEN)-1 ) {
	    chsize = my_snprintf( octbuf, PV_ESCAPE_OCTBUFSIZE, 
                                      "%cx[%c]", esc, *pv);
	    readsize = 1;
        } else if ( ( u > 255 ) || (flags & PERL_PV_ESCAPE_ALL)) {
            if (flags & PERL_PV_ESCAPE_FIRSTCHAR) 
                chsize = my_snprintf( octbuf, PV_ESCAPE_OCTBUFSIZE, 
                                      "%c%"UVxf, esc, u);
            else
                chsize = my_snprintf( octbuf, PV_ESCAPE_OCTBUFSIZE, 
                                      "%cx{%"UVxf"}", esc, u);
        } else if (flags & PERL_PV_ESCAPE_NOBACKSLASH) {
            chsize = 1;            
        } else {         
            if ( (c == dq) || (c == esc) || !isPRINT(c) ) {
	        chsize = 2;
                switch (c) {
                
		case '\\' : /* fallthrough */
		case '%'  : if ( c == esc )  {
		                octbuf[1] = esc;  
		            } else {
		                chsize = 1;
		            }
		            break;
		case '\v' : octbuf[1] = 'v';  break;
		case '\t' : octbuf[1] = 't';  break;
		case '\r' : octbuf[1] = 'r';  break;
		case '\n' : octbuf[1] = 'n';  break;
		case '\f' : octbuf[1] = 'f';  break;
                case '"'  : 
                        if ( dq == '"' ) 
				octbuf[1] = '"';
                        else 
                            chsize = 1;
                        break;
		default:
                        if ( isuni )
                            chsize = my_snprintf( octbuf, PV_ESCAPE_OCTBUFSIZE, 
                                                  "%cx{%02x}", esc, c);
			else
                            chsize = my_snprintf( octbuf, PV_ESCAPE_OCTBUFSIZE, 
                                                  "%cx[%x]", esc, c);
                }
            } else {
                chsize = 1;
            }
	}
	if ( max && (wrote + chsize > max) ) {
	    break;
        } else if (chsize > 1) {
            sv_catpvn(dsv, octbuf, chsize);
            wrote += chsize;
	} else {
	    /* If PERL_PV_ESCAPE_NOBACKSLASH is set then bytes in the range
	       128-255 can be appended raw to the dsv. If dsv happens to be
	       UTF-8 then we need catpvf to upgrade them for us.
	       Or add a new API call sv_catpvc(). Think about that name, and
	       how to keep it clear that it's unlike the s of catpvs, which is
	       really an array octets, not a string.  */
            Perl_sv_catpvf( aTHX_ dsv, "%c", c);
	    wrote++;
	}
        if ( flags & PERL_PV_ESCAPE_FIRSTCHAR ) 
            break;
    }
    if (escaped != NULL)
        *escaped= pv - str;
    return SvPVX_const(dsv);
}
/*
=for apidoc Apd|char *|pv_pretty|NN SV *dsv|NN const char const *str\
           |const STRLEN count|const STRLEN max\
           |const char const *start_color| const char const *end_color\
           |const U32 flags

Converts a string into something presentable, handling escaping via
pv_escape() and supporting quoting and ellipses.

If the PERL_PV_PRETTY_QUOTE flag is set then the result will be 
double quoted with any double quotes in the string escaped. Otherwise
if the PERL_PV_PRETTY_LTGT flag is set then the result be wrapped in
angle brackets. 
           
If the PERL_PV_PRETTY_ELLIPSES flag is set and not all characters in
string were output then an ellipsis C<...> will be appended to the
string. Note that this happens AFTER it has been quoted.
           
If start_color is non-null then it will be inserted after the opening
quote (if there is one) but before the escaped text. If end_color
is non-null then it will be inserted after the escaped text but before
any quotes or ellipses.

Returns a pointer to the prettified text as held by dsv.
           
=cut           
*/

const char *
Perl_pv_pretty( pTHX_ SV *dsv, char const * const str, const STRLEN count, 
  const STRLEN max, char const * const start_color, char const * const end_color, 
  const U32 flags ) 
{
    const U8 dq = (flags & PERL_PV_PRETTY_QUOTE) ? '"' : '%';
    STRLEN escaped;
 
    PERL_ARGS_ASSERT_PV_PRETTY;
   
    if (!(flags & PERL_PV_PRETTY_NOCLEAR)) {
	    /* This won't alter the UTF-8 flag */
	    sv_setpvn(dsv, "", 0);
    }

    if ( dq == '"' )
        sv_catpvn(dsv, "\"", 1);
    else if ( flags & PERL_PV_PRETTY_LTGT )
        sv_catpvn(dsv, "<", 1);
        
    if ( start_color != NULL ) 
        Perl_sv_catpv( aTHX_ dsv, start_color);
    
    pv_escape( dsv, str, count, max, &escaped, flags | PERL_PV_ESCAPE_NOCLEAR );    
    
    if ( end_color != NULL ) 
        Perl_sv_catpv( aTHX_ dsv, end_color);

    if ( dq == '"' ) 
	sv_catpvn( dsv, "\"", 1 );
    else if ( flags & PERL_PV_PRETTY_LTGT )
        sv_catpvn( dsv, ">", 1);         
    
    if ( (flags & PERL_PV_PRETTY_ELLIPSES) && ( escaped < count ) )
	    sv_catpvn( dsv, "...", 3 );
 
    return SvPVX_const(dsv);
}

/*
=for apidoc pv_display

  char *pv_display(SV *dsv, const char *pv, STRLEN cur, STRLEN len,
                   STRLEN pvlim, U32 flags)

Similar to

  pv_escape(dsv,pv,cur,pvlim,PERL_PV_ESCAPE_QUOTE);

except that an additional "\0" will be appended to the string when
len > cur and pv[cur] is "\0".

Note that the final string may be up to 7 chars longer than pvlim.

=cut
*/

const char *
Perl_pv_display(pTHX_ SV *dsv, const char *pv, STRLEN cur, STRLEN len, STRLEN pvlim)
{
    PERL_ARGS_ASSERT_PV_DISPLAY;

    pv_pretty( dsv, pv, cur, pvlim, NULL, NULL, PERL_PV_PRETTY_DUMP);
    if (len > cur && pv[cur] == '\0')
            sv_catpvn( dsv, "\\0", 2 );
    return SvPVX_const(dsv);
}

const char *
Perl_sv_peek(pTHX_ SV *sv)
{
    dVAR;
    SV * const t = sv_newmortal();
    int unref = 0;
    U32 type;

    sv_setpvn(t, "", 0);
  retry:
    if (!sv) {
	sv_catpv(t, "VOID");
	goto finish;
    }
    else if (sv == (SV*)0x55555555 || SvTYPE(sv) == 'U') {
	sv_catpv(t, "WILD");
	goto finish;
    }
    else if (sv == &PL_sv_undef || sv == &PL_sv_no || sv == &PL_sv_yes || sv == &PL_sv_placeholder) {
	if (sv == &PL_sv_undef) {
	    sv_catpv(t, "SV_UNDEF");
	    if (!(SvFLAGS(sv) & (SVf_OK|SVf_OOK|SVs_OBJECT|
				 SVs_SMG|SVs_RMG)) &&
		SvREADONLY(sv))
		goto finish;
	}
	else if (sv == &PL_sv_no) {
	    sv_catpv(t, "SV_NO");
	    if (!(SvFLAGS(sv) & (SVf_ROK|SVf_OOK|SVs_OBJECT|
				 SVs_SMG|SVs_RMG)) &&
		!(~SvFLAGS(sv) & (SVf_POK|SVf_NOK|SVf_READONLY|
				  SVp_POK|SVp_NOK)) &&
		SvCUR(sv) == 0 &&
		SvNVX(sv) == 0.0)
		goto finish;
	}
	else if (sv == &PL_sv_yes) {
	    sv_catpv(t, "SV_YES");
	    if (!(SvFLAGS(sv) & (SVf_ROK|SVf_OOK|SVs_OBJECT|
				 SVs_SMG|SVs_RMG)) &&
		!(~SvFLAGS(sv) & (SVf_POK|SVf_NOK|SVf_READONLY|
				  SVp_POK|SVp_NOK)) &&
		SvCUR(sv) == 1 &&
		SvPVX_const(sv) && *SvPVX_const(sv) == '1' &&
		SvNVX(sv) == 1.0)
		goto finish;
	}
	else {
	    sv_catpv(t, "SV_PLACEHOLDER");
	    if (!(SvFLAGS(sv) & (SVf_OK|SVf_OOK|SVs_OBJECT|
				 SVs_SMG|SVs_RMG)) &&
		SvREADONLY(sv))
		goto finish;
	}
	sv_catpv(t, ":");
    }
    else if (SvREFCNT(sv) == 0) {
	sv_catpv(t, "(");
	unref++;
    }
    else if (DEBUG_R_TEST_) {
	int is_tmp = 0;
	I32 ix;
	/* is this SV on the tmps stack? */
	for (ix=PL_tmps_ix; ix>=0; ix--) {
	    if (PL_tmps_stack[ix] == sv) {
		is_tmp = 1;
		break;
	    }
	}
	if (SvREFCNT(sv) > 1)
	    Perl_sv_catpvf(aTHX_ t, "<%"UVuf"%s>", (UV)SvREFCNT(sv),
		    is_tmp ? "T" : "");
	else if (is_tmp)
	    sv_catpv(t, "<T>");
    }

    if (SvROK(sv)) {
	sv_catpv(t, "\\");
	if (SvCUR(t) + unref > 10) {
	    SvCUR_set(t, unref + 3);
	    *SvEND(t) = '\0';
	    sv_catpv(t, "...");
	    goto finish;
	}
	sv = (SV*)SvRV(sv);
	goto retry;
    }
    type = SvTYPE(sv);
    if (type == SVt_PVCV) {
	Perl_sv_catpvf(aTHX_ t, "CV()");
	goto finish;
    } else if (type < SVt_LAST) {
	sv_catpv(t, svshorttypenames[type]);

	if (type == SVt_NULL)
	    goto finish;
    } else {
	sv_catpv(t, "FREED");
	goto finish;
    }

    if (SvPOKp(sv)) {
	if (!SvPVX_const(sv))
	    sv_catpv(t, "(null)");
	else {
	    SV * const tmp = newSVpvs("");
	    sv_catpv(t, "(");
	    if (SvOOK(sv))
		Perl_sv_catpvf(aTHX_ t, "[%s]", pv_display(tmp, SvPVX_const(sv)-SvIVX(sv), SvIVX(sv), 0, 127));
	    Perl_sv_catpvf(aTHX_ t, "%s)", pv_display(tmp, SvPVX_const(sv), SvCUR(sv), SvLEN(sv), 127));
	    Perl_sv_catpvf(aTHX_ t, " [UTF8 \"%s\"]",
			   sv_uni_display(tmp, sv, 8 * sv_len_utf8(sv),
					  UNI_DISPLAY_QQ));
	    SvREFCNT_dec(tmp);
	}
    }
    else if (SvNOKp(sv)) {
	Perl_sv_catpvf(aTHX_ t, "(%"NVgf")",SvNVX(sv));
    }
    else if (SvIOKp(sv)) {
	if (SvIsUV(sv))
	    Perl_sv_catpvf(aTHX_ t, "(%"UVuf")", (UV)SvUVX(sv));
	else
            Perl_sv_catpvf(aTHX_ t, "(%"IVdf")", (IV)SvIVX(sv));
    }
    else
	sv_catpv(t, "()");

  finish:
    while (unref--)
	sv_catpv(t, ")");
    return SvPVX_const(t);
}

void
Perl_do_pmop_dump(pTHX_ I32 level, PerlIO *file, const PMOP *pm)
{
    char ch;

    PERL_ARGS_ASSERT_DO_PMOP_DUMP;

    if (!pm) {
	Perl_dump_indent(aTHX_ level, file, "{}\n");
	return;
    }
    Perl_dump_indent(aTHX_ level, file, "{\n");
    level++;
    ch = '/';
    if (PM_GETRE(pm))
	Perl_dump_indent(aTHX_ level, file, "PMf_PRE %c%s%c%s\n",
	     ch, RX_PRECOMP(PM_GETRE(pm)), ch,
	     (pm->op_private & OPpRUNTIME) ? " (RUNTIME)" : "");
    else
	Perl_dump_indent(aTHX_ level, file, "PMf_PRE (RUNTIME)\n");
    if (pm->op_type != OP_PUSHRE && pm->op_pmreplrootu.op_pmreplroot) {
	Perl_dump_indent(aTHX_ level, file, "PMf_REPL = ");
	op_dump(pm->op_pmreplrootu.op_pmreplroot);
    }
    if (pm->op_pmflags || (PM_GETRE(pm) && RX_CHECK_SUBSTR(PM_GETRE(pm)))) {
	SV * const tmpsv = pm_description(pm);
	Perl_dump_indent(aTHX_ level, file, "PMFLAGS = (%s)\n", SvCUR(tmpsv) ? SvPVX_const(tmpsv) + 1 : "");
	SvREFCNT_dec(tmpsv);
    }

    Perl_dump_indent(aTHX_ level-1, file, "}\n");
}

static SV *
S_pm_description(pTHX_ const PMOP *pm)
{
    SV * const desc = newSVpvs("");
    const REGEXP * const regex = PM_GETRE(pm);
    const U32 pmflags = pm->op_pmflags;

    PERL_ARGS_ASSERT_PM_DESCRIPTION;

    if (regex) {
        if (RX_CHECK_SUBSTR(regex)) {
            if (!(RX_EXTFLAGS(regex) & RXf_NOSCAN))
                sv_catpv(desc, ",SCANFIRST");
            if (RX_EXTFLAGS(regex) & RXf_CHECK_ALL)
                sv_catpv(desc, ",ALL");
        }
        if (RX_EXTFLAGS(regex) & RXf_SKIPWHITE)
            sv_catpv(desc, ",SKIPWHITE");
    }

    if (pmflags & PMf_CONST)
	sv_catpv(desc, ",CONST");
    if (pmflags & PMf_KEEP)
	sv_catpv(desc, ",KEEP");
    if (pmflags & PMf_GLOBAL)
	sv_catpv(desc, ",GLOBAL");
    if (pmflags & PMf_CONTINUE)
	sv_catpv(desc, ",CONTINUE");
    return desc;
}

void
Perl_pmop_dump(pTHX_ PMOP *pm)
{
    do_pmop_dump(0, Perl_debug_log, pm);
}

/* An op sequencer.  We visit the ops in the order they're to execute. */

STATIC void
S_sequence(pTHX_ register const OP *o)
{
    dVAR;
    const OP *oldop = NULL;

    if (!o)
	return;

    if (!Sequence)
	Sequence = newHV();

    for (; o; o = o->op_next) {
	STRLEN len;
	SV * const op = sv_2mortal(newSVuv(PTR2UV(o)));
	const char * const key = SvPV_const(op, len);

	if (hv_exists(Sequence, key, len))
	    break;

	switch (o->op_type) {
	case OP_STUB:
	    if ((o->op_flags & OPf_WANT) != OPf_WANT_LIST) {
		(void)hv_store(Sequence, key, len, newSVuv(++PL_op_seq), 0);
		break;
	    }
	    goto nothin;
	case OP_NULL:
#ifdef PERL_MAD
	    if (o == o->op_next)
		return;
#endif
	    if (oldop && o->op_next)
		continue;
	    break;
	case OP_SCALAR:
	case OP_LINESEQ:
	case OP_SCOPE:
	  nothin:
	    if (oldop && o->op_next)
		continue;
	    (void)hv_store(Sequence, key, len, newSVuv(++PL_op_seq), 0);
	    break;

	case OP_MAPWHILE:
	case OP_GREPWHILE:
	case OP_AND:
	case OP_OR:
	case OP_DOR:
	case OP_ANDASSIGN:
	case OP_ORASSIGN:
	case OP_DORASSIGN:
	case OP_COND_EXPR:
	case OP_RANGE:
	    (void)hv_store(Sequence, key, len, newSVuv(++PL_op_seq), 0);
	    sequence_tail(cLOGOPo->op_other);
	    break;

	case OP_ENTERLOOP:
	case OP_ENTERITER:
	    (void)hv_store(Sequence, key, len, newSVuv(++PL_op_seq), 0);
	    sequence_tail(cLOOPo->op_redoop);
	    sequence_tail(cLOOPo->op_nextop);
	    sequence_tail(cLOOPo->op_lastop);
	    break;

	case OP_SUBST:
	    (void)hv_store(Sequence, key, len, newSVuv(++PL_op_seq), 0);
	    sequence_tail(cPMOPo->op_pmstashstartu.op_pmreplstart);
	    break;

	case OP_QR:
	case OP_MATCH:
	case OP_HELEM:
	    break;

	default:
	    (void)hv_store(Sequence, key, len, newSVuv(++PL_op_seq), 0);
	    break;
	}
	oldop = o;
    }
}

static void
S_sequence_tail(pTHX_ const OP *o)
{
    while (o && (o->op_type == OP_NULL))
	o = o->op_next;
    sequence(o);
}

STATIC UV
S_sequence_num(pTHX_ const OP *o)
{
    dVAR;
    SV     *op,
          **seq;
    const char *key;
    STRLEN  len;
    if (!o) return 0;
    op = newSVuv(PTR2UV(o));
    key = SvPV_const(op, len);
    seq = hv_fetch(Sequence, key, len, 0);
    SVcpNULL(op);
    return seq ? SvUV(*seq): 0;
}

void
Perl_do_op_dump(pTHX_ I32 level, PerlIO *file, const OP *o)
{
    dVAR;
    UV      seq;
    const OPCODE optype = o->op_type;

    PERL_ARGS_ASSERT_DO_OP_DUMP;

    sequence(o);
    Perl_dump_indent(aTHX_ level, file, "{\n");
    level++;
    seq = sequence_num(o);
    if (seq)
	PerlIO_printf(file, "%-4"UVuf, seq);
    else
	PerlIO_printf(file, "    ");
    PerlIO_printf(file,
		  "%*sTYPE = %s  ===> ",
		  (int)(PL_dumpindent*level-4), "", OP_NAME(o));
    if (o->op_next)
	PerlIO_printf(file, seq ? "%"UVuf"\n" : "(%"UVuf")\n",
				sequence_num(o->op_next));
    else
	PerlIO_printf(file, "DONE\n");
    if (o->op_targ) {
	if (optype == OP_NULL) {
	    Perl_dump_indent(aTHX_ level, file, "  (was %s)\n", PL_op_name[o->op_targ]);
	    if (o->op_targ == OP_NEXTSTATE) {
		if (CopSTASHPV(cCOPo))
		    Perl_dump_indent(aTHX_ level, file, "PACKAGE = \"%s\"\n",
				     CopSTASHPV(cCOPo));
		if (cCOPo->cop_label)
		    Perl_dump_indent(aTHX_ level, file, "LABEL = \"%s\"\n",
				     cCOPo->cop_label);
	    }
	}
	else
	    Perl_dump_indent(aTHX_ level, file, "TARG = %ld\n", (long)o->op_targ);
    }
    {
	SV* loc = o->op_location;
	Perl_dump_indent(aTHX_ level, file, "LOCATION = ");
	if (loc && SvAVOK(loc)) {
	    SV** ary = AvARRAY((AV*)loc);
	    I32 len = av_len((AV*)loc);
	    int i;
	    for (i=0; i <= len; i++) {
		if (SvPOK(ary[i])) {
		    PerlIO_write(file, SvPVX_const(ary[i]), SvCUR(ary[i]));
		}
		else if (SvIOK(ary[i])) {
		    PerlIO_printf(file, "%"IVdf, (IV)SvIVX(ary[i]));
		}
		PerlIO_write(file, STR_WITH_LEN(" "));
	    }
	}
	PerlIO_printf(file, "\n");
    }
#ifdef DUMPADDR
    Perl_dump_indent(aTHX_ level, file, "ADDR = 0x%"UVxf" => 0x%"UVxf"\n", (UV)o, (UV)o->op_next);
#endif
    if (o->op_flags) {

	/* call the refactored bits */
	SV * const tmpsv = S_dump_op_flags(aTHX_ o);
	Perl_dump_indent(aTHX_ level, file, "FLAGS = (%s)\n", SvCUR(tmpsv) ? SvPVX_const(tmpsv) + 1 : "");
    }
    if (o->op_private) {
	SV * const tmpsv = S_dump_op_flags_private(aTHX_ o);
	if (SvCUR(tmpsv))
	    Perl_dump_indent(aTHX_ level, file, "PRIVATE = (%s)\n", SvPVX_const(tmpsv) + 1);
    }
    S_dump_op_mad(aTHX_ level, file, o);
    S_dump_op_rest(aTHX_ level, file, o);
}
    
STATIC SV* S_dump_op_flags(pTHX_ const OP* o)
{
    SV * const tmpsv = sv_2mortal(newSVpvs(""));
    PERL_ARGS_ASSERT_DUMP_OP_FLAGS;
    switch (o->op_flags & OPf_WANT) {
    case OPf_WANT_VOID:
	sv_catpv(tmpsv, ",VOID");
	break;
    case OPf_WANT_SCALAR:
	sv_catpv(tmpsv, ",SCALAR");
	break;
    case OPf_WANT_LIST:
	sv_catpv(tmpsv, ",LIST");
	break;
    default:
	sv_catpv(tmpsv, ",UNKNOWN");
	break;
    }
    if (o->op_flags & OPf_KIDS)
	sv_catpv(tmpsv, ",KIDS");
    if (o->op_flags & OPf_PARENS)
	sv_catpv(tmpsv, ",PARENS");
    if (o->op_flags & OPf_STACKED)
	sv_catpv(tmpsv, ",STACKED");
    if (o->op_flags & OPf_REF)
	sv_catpv(tmpsv, ",REF");
    if (o->op_flags & OPf_MOD)
	sv_catpv(tmpsv, ",MOD");
    if (o->op_flags & OPf_ASSIGN)
	sv_catpv(tmpsv, ",ASSIGN");
    if (o->op_flags & OPf_ASSIGN_PART)
	sv_catpv(tmpsv, ",ASSIGN_PART");
    if (o->op_flags & OPf_SPECIAL)
	sv_catpv(tmpsv, ",SPECIAL");
    
    return tmpsv;
}

STATIC SV* S_dump_op_flags_private(pTHX_ const OP* o)
{
    const OPCODE optype = o->op_type;
    SV * const tmpsv = sv_2mortal(newSVpvs(""));
    PERL_ARGS_ASSERT_DUMP_OP_FLAGS_PRIVATE;

    if (PL_opargs[optype] & OA_TARGLEX) {
	if (o->op_flags & OPf_TARGET_MY)
	    sv_catpv(tmpsv, ",TARGET_MY");
    }
    else if (optype == OP_REPEAT) {
	if (o->op_private & OPpREPEAT_DOLIST)
	    sv_catpv(tmpsv, ",DOLIST");
    }
    if (optype == OP_ENTERSUB_SAVE) {
	if (o->op_private & OPpENTERSUB_SAVE_DISCARD)
	    sv_catpv(tmpsv, ",DISCARD");
    }
    else if (optype == OP_ENTERSUB ||
	     optype == OP_RV2SV ||
	     optype == OP_GVSV ||
	     optype == OP_RV2AV ||
	     optype == OP_RV2HV ||
	     optype == OP_RV2GV ||
	     optype == OP_AELEM ||
	     optype == OP_HELEM )
    {
	if (optype == OP_ENTERSUB) {
	    if (o->op_private & OPpENTERSUB_AMPER)
		sv_catpv(tmpsv, ",AMPER");
	    if (o->op_private & OPpENTERSUB_DB)
		sv_catpv(tmpsv, ",DB");
	    if (o->op_private & OPpENTERSUB_HASTARG)
		sv_catpv(tmpsv, ",HASTARG");
	    if (o->op_private & OPpENTERSUB_INARGS)
		sv_catpv(tmpsv, ",INARGS");
	}
	else {
	    switch (o->op_private & OPpDEREF) {
	    case OPpDEREF_SV:
		sv_catpv(tmpsv, ",SV");
		break;
	    case OPpDEREF_AV:
		sv_catpv(tmpsv, ",AV");
		break;
	    case OPpDEREF_HV:
		sv_catpv(tmpsv, ",HV");
		break;
	    }
	}
	if (optype == OP_AELEM || optype == OP_HELEM) {
	    if (o->op_private & OPpELEM_ADD)
		sv_catpv(tmpsv, ",ELEM_ADD");
	    if (o->op_private & OPpELEM_OPTIONAL)
		sv_catpv(tmpsv, ",ELEM_OPTIONAL");
	}
	else {
	    if (o->op_private & OPpOUR_INTRO)
		sv_catpv(tmpsv, ",OUR_INTRO");
	}
    }
    else if (optype == OP_CONST) {
	if (o->op_private & OPpCONST_BARE)
	    sv_catpv(tmpsv, ",BARE");
	if (o->op_private & OPpCONST_STRICT)
	    sv_catpv(tmpsv, ",STRICT");
	if (o->op_private & OPpCONST_ENTERED)
	    sv_catpv(tmpsv, ",ENTERED");
    }
    else if (optype == OP_RV2CV) {
	if (o->op_private & OPpLVAL_INTRO)
	    sv_catpv(tmpsv, ",LVAL_INTRO");
    }
    else if (optype == OP_GV) {
	if (o->op_private & OPpEARLY_CV)
	    sv_catpv(tmpsv, ",EARLY_CV");
    }
    else if (optype == OP_LIST) {
	if (o->op_private & OPpLIST_GUESSED)
	    sv_catpv(tmpsv, ",GUESSED");
    }
    else if (optype == OP_DELETE) {
	if (o->op_private & OPpSLICE)
	    sv_catpv(tmpsv, ",SLICE");
    }
    else if (optype == OP_EXISTS) {
	if (o->op_private & OPpEXISTS_SUB)
	    sv_catpv(tmpsv, ",EXISTS_SUB");
    }
    else if (optype == OP_SORT) {
	if (o->op_private & OPpSORT_NUMERIC)
	    sv_catpv(tmpsv, ",NUMERIC");
	if (o->op_private & OPpSORT_INTEGER)
	    sv_catpv(tmpsv, ",INTEGER");
	if (o->op_private & OPpSORT_REVERSE)
	    sv_catpv(tmpsv, ",REVERSE");
    }
    else if (optype == OP_OPEN || optype == OP_BACKTICK) {
	if (o->op_private & OPpOPEN_IN_RAW)
	    sv_catpv(tmpsv, ",IN_RAW");
	if (o->op_private & OPpOPEN_IN_CRLF)
	    sv_catpv(tmpsv, ",IN_CRLF");
	if (o->op_private & OPpOPEN_OUT_RAW)
	    sv_catpv(tmpsv, ",OUT_RAW");
	if (o->op_private & OPpOPEN_OUT_CRLF)
	    sv_catpv(tmpsv, ",OUT_CRLF");
    }
    else if (optype == OP_EXIT) {
	if (o->op_private & OPpEXIT_VMSISH)
	    sv_catpv(tmpsv, ",EXIT_VMSISH");
	if (o->op_private & OPpHUSH_VMSISH)
	    sv_catpv(tmpsv, ",HUSH_VMSISH");
    }
    else if (optype == OP_DIE) {
	if (o->op_private & OPpHUSH_VMSISH)
	    sv_catpv(tmpsv, ",HUSH_VMSISH");
    }
    else if (PL_check[optype] != MEMBER_TO_FPTR(Perl_ck_ftst)) {
	if (OP_IS_FILETEST_ACCESS(optype) && o->op_private & OPpFT_ACCESS)
	    sv_catpv(tmpsv, ",FT_ACCESS");
	if (o->op_private & OPpFT_STACKED)
	    sv_catpv(tmpsv, ",FT_STACKED");
    }
    if (o->op_flags & OPf_MOD && o->op_private & OPpLVAL_INTRO)
	sv_catpv(tmpsv, ",LVAL_INTRO");
    
    return tmpsv;
}

static void S_dump_op_mad (pTHX_ I32 level, PerlIO *file, const OP *o)
{
    PERL_ARGS_ASSERT_DUMP_OP_MAD;
#ifndef PERL_MAD
    PERL_UNUSED_ARG(level);
    PERL_UNUSED_ARG(file);
    PERL_UNUSED_ARG(o);
#else
    if (PL_madskills && o->op_madprop) {
	SV * const tmpsv = sv_2mortal(newSVpvn("", 0));
	MADPROP* mp = o->op_madprop;
	Perl_dump_indent(aTHX_ level, file, "MADPROPS = {\n");
	level++;
	while (mp) {
	    const char tmp = mp->mad_key;
	    sv_setpvn(tmpsv,"'",1);
	    if (tmp)
		sv_catpvn(tmpsv, &tmp, 1);
	    sv_catpv(tmpsv, "'=");
	    switch (mp->mad_type) {
	    case MAD_SV:
		sv_catpv(tmpsv, "<");
		sv_catpvn(tmpsv, (char*)mp->mad_val, mp->mad_vlen);
		sv_catpv(tmpsv, ">");
		Perl_dump_indent(aTHX_ level, file, "%s\n", SvPVX_const(tmpsv));
		break;
	    case MAD_OP:
		if ((OP*)mp->mad_val) {
		    Perl_dump_indent(aTHX_ level, file, "%s\n", SvPVX_const(tmpsv));
		    do_op_dump(level, file, (OP*)mp->mad_val);
		}
		break;
	    default:
		sv_catpv(tmpsv, "(UNK)");
		Perl_dump_indent(aTHX_ level, file, "%s\n", SvPVX_const(tmpsv));
		break;
	    }
	    mp = mp->mad_next;
	}
	level--;
	Perl_dump_indent(aTHX_ level, file, "}\n");
    }
#endif
}

static void S_dump_op_rest (pTHX_ I32 level, PerlIO *file, const OP *o)
{
    const OPCODE optype = o->op_type;
    PERL_ARGS_ASSERT_DUMP_OP_REST;

    switch (optype) {
    case OP_AELEMFAST:
    case OP_GVSV:
    case OP_GV:
	if ( ! PL_op->op_flags & OPf_SPECIAL) { /* not lexical */
	    if (cSVOPo->op_sv) {
		SV * const tmpsv = sv_2mortal(newSV(0));
		ENTER;
		gv_fullname3(tmpsv, (GV*)cSVOPo->op_sv, NULL);
		Perl_dump_indent(aTHX_ level, file, "GV = %s\n",
				 SvPV_nolen_const(tmpsv));
		LEAVE;
	    }
	    else
		Perl_dump_indent(aTHX_ level, file, "GV = NULL\n");
	}
	break;
    case OP_CONST:
    case OP_HINTSEVAL:
    case OP_METHOD_NAMED:
	Perl_dump_indent(aTHX_ level, file, "SV = %s\n", SvPEEK(cSVOPo_sv));
	break;
    case OP_NEXTSTATE:
    case OP_DBSTATE:
	if (CopSTASHPV(cCOPo))
	    Perl_dump_indent(aTHX_ level, file, "PACKAGE = \"%s\"\n",
			     CopSTASHPV(cCOPo));
	if (cCOPo->cop_label)
	    Perl_dump_indent(aTHX_ level, file, "LABEL = \"%s\"\n",
			     cCOPo->cop_label);
	break;
    case OP_ENTERLOOP:
	Perl_dump_indent(aTHX_ level, file, "REDO ===> ");
	if (cLOOPo->op_redoop)
	    PerlIO_printf(file, "%"UVuf"\n", sequence_num(cLOOPo->op_redoop));
	else
	    PerlIO_printf(file, "DONE\n");
	Perl_dump_indent(aTHX_ level, file, "NEXT ===> ");
	if (cLOOPo->op_nextop)
	    PerlIO_printf(file, "%"UVuf"\n", sequence_num(cLOOPo->op_nextop));
	else
	    PerlIO_printf(file, "DONE\n");
	Perl_dump_indent(aTHX_ level, file, "LAST ===> ");
	if (cLOOPo->op_lastop)
	    PerlIO_printf(file, "%"UVuf"\n", sequence_num(cLOOPo->op_lastop));
	else
	    PerlIO_printf(file, "DONE\n");
	break;
    case OP_COND_EXPR:
    case OP_RANGE:
    case OP_MAPWHILE:
    case OP_GREPWHILE:
    case OP_OR:
    case OP_AND:
	Perl_dump_indent(aTHX_ level, file, "OTHER ===> ");
	if (cLOGOPo->op_other)
	    PerlIO_printf(file, "%"UVuf"\n", sequence_num(cLOGOPo->op_other));
	else
	    PerlIO_printf(file, "DONE\n");
	break;
    case OP_PUSHRE:
    case OP_MATCH:
    case OP_QR:
    case OP_SUBST:
	do_pmop_dump(level, file, cPMOPo);
	break;
    case OP_LEAVE:
    case OP_LEAVEEVAL:
    case OP_LEAVESUB:
    case OP_SCOPE:
	break;
    case OP_ROOT:
	Perl_dump_indent(aTHX_ level, file, "REFCNT = %"UVuf"\n", (UV)o->op_targ);
    default:
	break;
    }

    if (o->op_flags & OPf_KIDS) {
	OP *kid;
	for (kid = cUNOPo->op_first; kid; kid = kid->op_sibling)
	    do_op_dump(level, file, kid);
    }
    Perl_dump_indent(aTHX_ level-1, file, "}\n");
}

void
Perl_op_dump(pTHX_ const OP *o)
{
    PERL_ARGS_ASSERT_OP_DUMP;
    do_op_dump(0, Perl_debug_log, o);
}

void
Perl_gv_dump(pTHX_ GV *gv)
{
    SV *sv;

    PERL_ARGS_ASSERT_GV_DUMP;

    if (!gv) {
	PerlIO_printf(Perl_debug_log, "{}\n");
	return;
    }
    sv = sv_newmortal();
    PerlIO_printf(Perl_debug_log, "{\n");
    gv_fullname3(sv, gv, NULL);
    Perl_dump_indent(aTHX_ 1, Perl_debug_log, "GV_NAME = %s", SvPVX_const(sv));
    if (gv != GvEGV(gv)) {
	gv_efullname3(sv, GvEGV(gv), NULL);
	Perl_dump_indent(aTHX_ 1, Perl_debug_log, "-> %s", SvPVX_const(sv));
    }
    PerlIO_putc(Perl_debug_log, '\n');
    Perl_dump_indent(aTHX_ 0, Perl_debug_log, "}\n");
}


/* map magic types to the symbolic names
 * (with the PERL_MAGIC_ prefixed stripped)
 */

static const struct { const char type; const char *name; } magic_names[] = {
	{ PERL_MAGIC_rhash,          "rhash(%)" },
	{ PERL_MAGIC_symtab,         "symtab(:)" },
	{ PERL_MAGIC_backref,        "backref(<)" },
	{ PERL_MAGIC_bm,             "bm(B)" },
	{ PERL_MAGIC_isa,            "isa(I)" },
	{ PERL_MAGIC_dbfile,         "dbfile(L)" },
	{ PERL_MAGIC_shared,         "shared(N)" },
	{ PERL_MAGIC_uvar,           "uvar(U)" },
	{ PERL_MAGIC_regex_global,   "regex_global(g)" },
	{ PERL_MAGIC_isaelem,        "isaelem(i)" },
	{ PERL_MAGIC_dbline,         "dbline(l)" },
	{ PERL_MAGIC_shared_scalar,  "shared_scalar(n)" },
	{ PERL_MAGIC_qr,             "qr(r)" },
	{ PERL_MAGIC_uvar_elem,      "uvar_elem(u)" },
	{ PERL_MAGIC_vstring,        "vstring(V)" },
	{ PERL_MAGIC_utf8,           "utf8(w)" },
	{ PERL_MAGIC_ext,            "ext(~)" },
	/* this null string terminates the list */
	{ 0,                         NULL },
};

void
Perl_do_magic_dump(pTHX_ I32 level, PerlIO *file, const MAGIC *mg, I32 nest, I32 maxnest, bool dumpops, STRLEN pvlim)
{
    PERL_ARGS_ASSERT_DO_MAGIC_DUMP;

    for (; mg; mg = mg->mg_moremagic) {
 	Perl_dump_indent(aTHX_ level, file,
			 "  MAGIC = 0x%"UVxf"\n", PTR2UV(mg));
 	if (mg->mg_virtual) {
            const MGVTBL * const v = mg->mg_virtual;
 	    const char *s;
            if (v == &PL_vtbl_dbline)     s = "dbline";
            else if (v == &PL_vtbl_isa)        s = "isa";
            else if (v == &PL_vtbl_mglob)      s = "mglob";
            else if (v == &PL_vtbl_bm)         s = "bm";
            else if (v == &PL_vtbl_uvar)       s = "uvar";
	    else if (v == &PL_vtbl_backref)    s = "backref";
	    else if (v == &PL_vtbl_utf8)       s = "utf8";
	    else			       s = NULL;
	    if (s)
	        Perl_dump_indent(aTHX_ level, file, "    MG_VIRTUAL = &PL_vtbl_%s\n", s);
	    else
	        Perl_dump_indent(aTHX_ level, file, "    MG_VIRTUAL = 0x%"UVxf"\n", PTR2UV(v));
        }
	else
	    Perl_dump_indent(aTHX_ level, file, "    MG_VIRTUAL = 0\n");

	if (mg->mg_private)
	    Perl_dump_indent(aTHX_ level, file, "    MG_PRIVATE = %d\n", mg->mg_private);

	{
	    int n;
	    const char *name = NULL;
	    for (n = 0; magic_names[n].name; n++) {
		if (mg->mg_type == magic_names[n].type) {
		    name = magic_names[n].name;
		    break;
		}
	    }
	    if (name)
		Perl_dump_indent(aTHX_ level, file,
				"    MG_TYPE = PERL_MAGIC_%s\n", name);
	    else
		Perl_dump_indent(aTHX_ level, file,
				"    MG_TYPE = UNKNOWN(\\%o)\n", mg->mg_type);
	}

        if (mg->mg_flags) {
            Perl_dump_indent(aTHX_ level, file, "    MG_FLAGS = 0x%02X\n", mg->mg_flags);
	    if (mg->mg_flags & MGf_REFCOUNTED)
	        Perl_dump_indent(aTHX_ level, file, "      REFCOUNTED\n");
            if (mg->mg_flags & MGf_GSKIP)
	        Perl_dump_indent(aTHX_ level, file, "      GSKIP\n");
	    if (mg->mg_type == PERL_MAGIC_regex_global &&
		mg->mg_flags & MGf_MINMATCH)
	        Perl_dump_indent(aTHX_ level, file, "      MINMATCH\n");
        }
	if (mg->mg_obj) {
	    Perl_dump_indent(aTHX_ level, file, "    MG_OBJ = 0x%"UVxf"\n", 
	        PTR2UV(mg->mg_obj));
            if (mg->mg_type == PERL_MAGIC_qr) {
		REGEXP* const re = (REGEXP *)mg->mg_obj;
		SV * const dsv = sv_newmortal();
                const char * const s
		    = pv_pretty(dsv, RX_WRAPPED(re), RX_WRAPLEN(re), 
                    60, NULL, NULL,
                    ( PERL_PV_PRETTY_QUOTE | PERL_PV_ESCAPE_RE | PERL_PV_PRETTY_ELLIPSES |
                    (PERL_PV_ESCAPE_UNI))
                );
		Perl_dump_indent(aTHX_ level+1, file, "    PAT = %s\n", s);
		Perl_dump_indent(aTHX_ level+1, file, "    REFCNT = %"IVdf"\n",
			(IV)RX_REFCNT(re));
            }
            if (mg->mg_flags & MGf_REFCOUNTED)
		do_sv_dump(level+2, file, mg->mg_obj, nest+1, maxnest, dumpops, pvlim); /* MG is already +1 */
	}
        if (mg->mg_len)
	    Perl_dump_indent(aTHX_ level, file, "    MG_LEN = %ld\n", (long)mg->mg_len);
        if (mg->mg_ptr) {
	    Perl_dump_indent(aTHX_ level, file, "    MG_PTR = 0x%"UVxf, PTR2UV(mg->mg_ptr));
	    if (mg->mg_len >= 0) {
		if (mg->mg_type != PERL_MAGIC_utf8) {
		    SV * const sv = newSVpvs("");
		    PerlIO_printf(file, " %s", pv_display(sv, mg->mg_ptr, mg->mg_len, 0, pvlim));
		    SvREFCNT_dec(sv);
		}
            }
	    else if (mg->mg_len == HEf_SVKEY) {
		PerlIO_puts(file, " => HEf_SVKEY\n");
		do_sv_dump(level+2, file, (SV*)((mg)->mg_ptr), nest+1, maxnest, dumpops, pvlim); /* MG is already +1 */
		continue;
	    }
	    else
		PerlIO_puts(file, " ???? - please notify IZ");
            PerlIO_putc(file, '\n');
        }
	if (mg->mg_type == PERL_MAGIC_utf8) {
	    const STRLEN * const cache = (STRLEN *) mg->mg_ptr;
	    if (cache) {
		IV i;
		for (i = 0; i < PERL_MAGIC_UTF8_CACHESIZE; i++)
		    Perl_dump_indent(aTHX_ level, file,
				     "      %2"IVdf": %"UVuf" -> %"UVuf"\n",
				     i,
				     (UV)cache[i * 2],
				     (UV)cache[i * 2 + 1]);
	    }
	}
    }
}

void
Perl_magic_dump(pTHX_ const MAGIC *mg)
{
    do_magic_dump(0, Perl_debug_log, mg, 0, 0, FALSE, 0);
}

void
Perl_do_hv_dump(pTHX_ I32 level, PerlIO *file, const char *name, HV *sv)
{
    const char *hvname;

    PERL_ARGS_ASSERT_DO_HV_DUMP;

    Perl_dump_indent(aTHX_ level, file, "%s = 0x%"UVxf, name, PTR2UV(sv));
    if (sv && (hvname = HvNAME_get(sv)))
	PerlIO_printf(file, "\t\"%s\"\n", hvname);
    else
	PerlIO_putc(file, '\n');
}

void
Perl_do_gv_dump(pTHX_ I32 level, PerlIO *file, const char *name, GV *sv)
{
    PERL_ARGS_ASSERT_DO_GV_DUMP;

    Perl_dump_indent(aTHX_ level, file, "%s = 0x%"UVxf, name, PTR2UV(sv));
    if (sv && GvNAME(sv))
	PerlIO_printf(file, "\t\"%s\"\n", GvNAME(sv));
    else
	PerlIO_putc(file, '\n');
}

void
Perl_do_gvgv_dump(pTHX_ I32 level, PerlIO *file, const char *name, GV *sv)
{
    PERL_ARGS_ASSERT_DO_GVGV_DUMP;

    Perl_dump_indent(aTHX_ level, file, "%s = 0x%"UVxf, name, PTR2UV(sv));
    if (sv && GvNAME(sv)) {
	const char *hvname;
	PerlIO_printf(file, "\t\"");
	if (GvSTASH(sv) && (hvname = HvNAME_get(GvSTASH(sv))))
	    PerlIO_printf(file, "%s\" :: \"", hvname);
	PerlIO_printf(file, "%s\"\n", GvNAME(sv));
    }
    else
	PerlIO_putc(file, '\n');
}

void
Perl_do_sv_dump(pTHX_ I32 level, PerlIO *file, SV *sv, I32 nest, I32 maxnest, bool dumpops, STRLEN pvlim)
{
    dVAR;
    SV *d;
    const char *s;
    U32 flags;
    U32 type;

    PERL_ARGS_ASSERT_DO_SV_DUMP;

    if (!sv) {
	Perl_dump_indent(aTHX_ level, file, "SV = 0\n");
	return;
    }

    flags = SvFLAGS(sv);
    type = SvTYPE(sv);

    d = Perl_newSVpvf(aTHX_
		   "(0x%"UVxf") at 0x%"UVxf"\n%*s  REFCNT = %"IVdf"\n%*s  FLAGS = (",
		   PTR2UV(SvANY(sv)), PTR2UV(sv),
		   (int)(PL_dumpindent*level), "", (IV)SvREFCNT(sv),
		   (int)(PL_dumpindent*level), "");

    if (!(flags & SVpad_NAME && (type == SVt_PVMG || type == SVt_PVNV))) {
	if (flags & SVs_PADSTALE)	sv_catpv(d, "PADSTALE,");
    }
    if (!(flags & SVpad_NAME && type == SVt_PVMG)) {
	if (flags & SVs_PADTMP)	sv_catpv(d, "PADTMP,");
	if (flags & SVs_PADMY)	sv_catpv(d, "PADMY,");
    }
    if (flags & SVs_TEMP)	sv_catpv(d, "TEMP,");
    if (flags & SVs_OBJECT)	sv_catpv(d, "OBJECT,");
    if (flags & SVs_SMG)	sv_catpv(d, "SMG,");
    if (flags & SVs_RMG)	sv_catpv(d, "RMG,");

    if (flags & SVf_IOK)	sv_catpv(d, "IOK,");
    if (flags & SVf_NOK)	sv_catpv(d, "NOK,");
    if (flags & SVf_POK)	sv_catpv(d, "POK,");
    if (flags & SVf_ROK)  {	
    				sv_catpv(d, "ROK,");
	if (SvWEAKREF(sv))	sv_catpv(d, "WEAKREF,");
    }
    if (flags & SVf_OOK)	sv_catpv(d, "OOK,");
    if (flags & SVf_FAKE)	sv_catpv(d, "FAKE,");
    if (flags & SVf_READONLY)	sv_catpv(d, "READONLY,");
    if (flags & SVf_BREAK)	sv_catpv(d, "BREAK,");

    if (flags & SVp_IOK)	sv_catpv(d, "pIOK,");
    if (flags & SVp_NOK)	sv_catpv(d, "pNOK,");
    if (flags & SVp_POK)	sv_catpv(d, "pPOK,");
    if (flags & SVp_SCREAM && type != SVt_PVHV && !isGV_with_GP(sv)) {
	if (SvPCS_IMPORTED(sv))
				sv_catpv(d, "PCS_IMPORTED,");
	else
				sv_catpv(d, "SCREAM,");
    }

    switch (type) {
    case SVt_PVCV:
	if (CvANON(sv))		sv_catpv(d, "ANON,");
	if (CvUNIQUE(sv))	sv_catpv(d, "UNIQUE,");
	if (CvCLONE(sv))	sv_catpv(d, "CLONE,");
	if (CvCLONED(sv))	sv_catpv(d, "CLONED,");
	if (CvCONST(sv))	sv_catpv(d, "CONST,");
	if (CvNODEBUG(sv))	sv_catpv(d, "NODEBUG,");
	break;
    case SVt_PVHV:
	if (HvSHAREKEYS(sv))	sv_catpv(d, "SHAREKEYS,");
	if (HvLAZYDEL(sv))	sv_catpv(d, "LAZYDEL,");
	if (HvREHASH(sv))	sv_catpv(d, "REHASH,");
	if (flags & SVphv_CLONEABLE) sv_catpv(d, "CLONEABLE,");
	break;
    case SVt_PVGV:
	if (isGV_with_GP(sv)) {
	    if (GvINTRO(sv))	sv_catpv(d, "INTRO,");
	    if (GvMULTI(sv))	sv_catpv(d, "MULTI,");
	    if (GvUNIQUE(sv))   sv_catpv(d, "UNIQUE,");
	    if (GvASSUMECV(sv))	sv_catpv(d, "ASSUMECV,");
	    if (GvIN_PAD(sv))   sv_catpv(d, "IN_PAD,");
	}
	if (isGV_with_GP(sv) && GvIMPORTED(sv)) {
	    sv_catpv(d, "IMPORT");
	    if (GvIMPORTED(sv) == GVf_IMPORTED)
		sv_catpv(d, "ALL,");
	    else {
		sv_catpv(d, "(");
		if (GvIMPORTED_SV(sv))	sv_catpv(d, " SV");
		if (GvIMPORTED_AV(sv))	sv_catpv(d, " AV");
		if (GvIMPORTED_HV(sv))	sv_catpv(d, " HV");
		if (GvIMPORTED_CV(sv))	sv_catpv(d, " CV");
		sv_catpv(d, " ),");
	    }
	}
	if (SvTAIL(sv))		sv_catpv(d, "TAIL,");
	if (SvVALID(sv))	sv_catpv(d, "VALID,");
	/* FALL THROUGH */
    default:
    evaled_or_uv:
	if (SvIsUV(sv) && !(flags & SVf_ROK))	sv_catpv(d, "IsUV,");
	break;
    case SVt_PVMG:
	if (SvPAD_OUR(sv))	sv_catpv(d, "OUR,");
	/* FALL THROUGH */
    case SVt_PVNV:
	goto evaled_or_uv;
    case SVt_PVAV:
	break;
    }

    if (*(SvEND(d) - 1) == ',') {
        SvCUR_set(d, SvCUR(d) - 1);
	SvPVX_mutable(d)[SvCUR(d)] = '\0';
    }
    sv_catpv(d, ")");
    s = SvPVX_const(d);
    
    Perl_dump_indent(aTHX_ level, file, "SV = ");
    if (type < SVt_LAST) {
	PerlIO_printf(file, "%s%s\n", svtypenames[type], s);

	if (type ==  SVt_NULL) {
	    SvREFCNT_dec(d);
	    return;
	}
    } else {
	PerlIO_printf(file, "UNKNOWN(0x%"UVxf") %s\n", (UV)type, s);
	SvREFCNT_dec(d);
	return;
    }
    {
	SV* loc = SvLOCATION(sv);
	Perl_dump_indent(aTHX_ level, file, "  LOCATION = ");
	if (loc && SvAVOK(loc)) {
	    SV** ary = AvARRAY((AV*)loc);
	    I32 len = av_len((AV*)loc);
	    int i;
	    for (i=0; i <= len; i++) {
		if (SvPOK(ary[i])) {
		    PerlIO_write(file, SvPVX_const(ary[i]), SvCUR(ary[i]));
		}
		else if (SvIOK(ary[i])) {
		    PerlIO_printf(file, "%"IVdf, (IV)SvIVX(ary[i]));
		}
		PerlIO_write(file, STR_WITH_LEN(" "));
	    }
	}
	PerlIO_printf(file, "\n");
    }
    if ((type >= SVt_PVIV && type != SVt_PVAV && type != SVt_PVHV
	 && type != SVt_PVCV && !isGV_with_GP(sv))
	|| (type == SVt_IV && !SvROK(sv))) {
	if (SvIsUV(sv)
#ifdef PERL_OLD_COPY_ON_WRITE
	               || SvIsCOW(sv)
#endif
	                             )
	    Perl_dump_indent(aTHX_ level, file, "  UV = %"UVuf, (UV)SvUVX(sv));
	else
	    Perl_dump_indent(aTHX_ level, file, "  IV = %"IVdf, (IV)SvIVX(sv));
#ifdef PERL_OLD_COPY_ON_WRITE
	if (SvIsCOW_shared_hash(sv))
	    PerlIO_printf(file, "  (HASH)");
	else if (SvIsCOW_normal(sv))
	    PerlIO_printf(file, "  (COW from 0x%"UVxf")", (UV)SvUVX(sv));
#endif
	PerlIO_putc(file, '\n');
    }
    if ((type == SVt_PVNV || type == SVt_PVMG) && SvFLAGS(sv) & SVpad_NAME) {
	Perl_dump_indent(aTHX_ level, file, "  COP_LOW = %"UVuf"\n",
			 (UV) COP_SEQ_RANGE_LOW(sv));
	Perl_dump_indent(aTHX_ level, file, "  COP_HIGH = %"UVuf"\n",
			 (UV) COP_SEQ_RANGE_HIGH(sv));
    } else if ((type >= SVt_PVNV && type != SVt_PVAV && type != SVt_PVHV
		&& type != SVt_PVCV && type != SVt_REGEXP
		&& type != SVt_PVIO && !isGV_with_GP(sv) && !SvVALID(sv))
	       || type == SVt_NV) {
	/* %Vg doesn't work? --jhi */
#ifdef USE_LONG_DOUBLE
	Perl_dump_indent(aTHX_ level, file, "  NV = %.*" PERL_PRIgldbl "\n", LDBL_DIG, SvNVX(sv));
#else
	Perl_dump_indent(aTHX_ level, file, "  NV = %.*g\n", DBL_DIG, SvNVX(sv));
#endif
    }
    if (SvROK(sv)) {
	Perl_dump_indent(aTHX_ level, file, "  RV = 0x%"UVxf"\n", PTR2UV(SvRV(sv)));
	if (nest < maxnest)
	    do_sv_dump(level+1, file, SvRV(sv), nest+1, maxnest, dumpops, pvlim);
    }
    if (type < SVt_PV) {
	SvREFCNT_dec(d);
	return;
    }
    if (type <= SVt_PVGV && !isGV_with_GP(sv)) {
	if (SvPVX_const(sv)) {
	    STRLEN delta;
	    if (SvOOK(sv)) {
		SvOOK_offset(sv, delta);
		Perl_dump_indent(aTHX_ level, file,"  OFFSET = %"UVuf"\n",
				 (UV) delta);
	    } else {
		delta = 0;
	    }
	    Perl_dump_indent(aTHX_ level, file,"  PV = 0x%"UVxf" ", PTR2UV(SvPVX_const(sv)));
	    if (SvOOK(sv)) {
		PerlIO_printf(file, "( %s . ) ",
			      pv_display(d, SvPVX_const(sv) - delta, delta, 0,
					 pvlim));
	    }
	    PerlIO_printf(file, "%s", pv_display(d, SvPVX_const(sv), SvCUR(sv), SvLEN(sv), pvlim));
	    PerlIO_printf(file, " [UTF8 \"%s\"]", sv_uni_display(d, sv, 6 * SvCUR(sv), UNI_DISPLAY_QQ)); /* the 6?  \x{....} */
	    PerlIO_printf(file, "\n");
	    Perl_dump_indent(aTHX_ level, file, "  CUR = %"IVdf"\n", (IV)SvCUR(sv));
	    Perl_dump_indent(aTHX_ level, file, "  LEN = %"IVdf"\n", (IV)SvLEN(sv));
	}
	else
	    Perl_dump_indent(aTHX_ level, file, "  PV = 0\n");
    }
    if (type == SVt_REGEXP) {
	/* FIXME dumping
	    Perl_dump_indent(aTHX_ level, file, "  REGEXP = 0x%"UVxf"\n",
			     PTR2UV(((struct regexp *)SvANY(sv))->xrx_regexp));
	*/
    }
    if (type >= SVt_PVMG) {
	if (type == SVt_PVMG && SvPAD_OUR(sv)) {
	    GV * const ogv = SvOURGV(sv);
	    if (ogv)
		do_gv_dump(level, file, "  OURGV", ogv);
	} else {
	    if (SvMAGIC(sv))
		do_magic_dump(level, file, SvMAGIC(sv), nest, maxnest, dumpops, pvlim);
	}
	if (SvSTASH(sv))
	    do_hv_dump(level, file, "  STASH", SvSTASH(sv));
    }
    switch (type) {
    case SVt_PVAV:
	Perl_dump_indent(aTHX_ level, file, "  ARRAY = 0x%"UVxf, PTR2UV(AvARRAY(sv)));
	if (AvARRAY(sv) != AvALLOC(sv)) {
	    PerlIO_printf(file, " (offset=%"IVdf")\n", (IV)(AvARRAY(sv) - AvALLOC(sv)));
	    Perl_dump_indent(aTHX_ level, file, "  ALLOC = 0x%"UVxf"\n", PTR2UV(AvALLOC(sv)));
	}
	else
	    PerlIO_putc(file, '\n');
	Perl_dump_indent(aTHX_ level, file, "  FILL = %"IVdf"\n", (IV)AvFILLp(sv));
	Perl_dump_indent(aTHX_ level, file, "  MAX = %"IVdf"\n", (IV)AvMAX(sv));
	sv_setpvn(d, "", 0);
	if (AvREAL(sv))	sv_catpv(d, ",REAL");
	if (AvREIFY(sv))	sv_catpv(d, ",REIFY");
	Perl_dump_indent(aTHX_ level, file, "  FLAGS = (%s)\n",
			 SvCUR(d) ? SvPVX_const(d) + 1 : "");
	if (nest < maxnest && av_len((AV*)sv) >= 0) {
	    int count;
	    for (count = 0; count <=  av_len((AV*)sv) && count < maxnest; count++) {
		SV** const elt = av_fetch((AV*)sv,count,0);

		Perl_dump_indent(aTHX_ level + 1, file, "Elt No. %"IVdf"\n", (IV)count);
		if (elt)
		    do_sv_dump(level+1, file, *elt, nest+1, maxnest, dumpops, pvlim);
	    }
	}
	break;
    case SVt_PVHV:
	Perl_dump_indent(aTHX_ level, file, "  ARRAY = 0x%"UVxf, PTR2UV(HvARRAY(sv)));
	if (HvARRAY(sv) && HvKEYS(sv)) {
	    /* Show distribution of HEs in the ARRAY */
	    int freq[200];
#define FREQ_MAX ((int)(sizeof freq / sizeof freq[0] - 1))
	    int i;
	    int max = 0;
	    U32 pow2 = 2, keys = HvKEYS(sv);
	    NV theoret, sum = 0;

	    PerlIO_printf(file, "  (");
	    Zero(freq, FREQ_MAX + 1, int);
	    for (i = 0; (STRLEN)i <= HvMAX(sv); i++) {
		HE* h;
		int count = 0;
                for (h = HvARRAY(sv)[i]; h; h = HeNEXT(h))
		    count++;
		if (count > FREQ_MAX)
		    count = FREQ_MAX;
	        freq[count]++;
	        if (max < count)
		    max = count;
	    }
	    for (i = 0; i <= max; i++) {
		if (freq[i]) {
		    PerlIO_printf(file, "%d%s:%d", i,
				  (i == FREQ_MAX) ? "+" : "",
				  freq[i]);
		    if (i != max)
			PerlIO_printf(file, ", ");
		}
            }
	    PerlIO_putc(file, ')');
	    /* The "quality" of a hash is defined as the total number of
	       comparisons needed to access every element once, relative
	       to the expected number needed for a random hash.

	       The total number of comparisons is equal to the sum of
	       the squares of the number of entries in each bucket.
	       For a random hash of n keys into k buckets, the expected
	       value is
				n + n(n-1)/2k
	    */

	    for (i = max; i > 0; i--) { /* Precision: count down. */
		sum += freq[i] * i * i;
            }
	    while ((keys = keys >> 1))
		pow2 = pow2 << 1;
	    theoret = HvKEYS(sv);
	    theoret += theoret * (theoret-1)/pow2;
	    PerlIO_putc(file, '\n');
	    Perl_dump_indent(aTHX_ level, file, "  hash quality = %.1"NVff"%%", theoret/sum*100);
	}
	PerlIO_putc(file, '\n');
	Perl_dump_indent(aTHX_ level, file, "  KEYS = %"IVdf"\n", (IV)HvKEYS(sv));
	Perl_dump_indent(aTHX_ level, file, "  FILL = %"IVdf"\n", (IV)HvFILL(sv));
	Perl_dump_indent(aTHX_ level, file, "  MAX = %"IVdf"\n", (IV)HvMAX(sv));
	Perl_dump_indent(aTHX_ level, file, "  RITER = %"IVdf"\n", (IV)HvRITER_get(sv));
	Perl_dump_indent(aTHX_ level, file, "  EITER = 0x%"UVxf"\n", PTR2UV(HvEITER_get(sv)));
	{
	    MAGIC * const mg = mg_find(sv, PERL_MAGIC_symtab);
	    if (mg && mg->mg_obj) {
		Perl_dump_indent(aTHX_ level, file, "  PMROOT = 0x%"UVxf"\n", PTR2UV(mg->mg_obj));
	    }
	}
	{
	    const char * const hvname = HvNAME_get(sv);
	    if (hvname)
		Perl_dump_indent(aTHX_ level, file, "  NAME = \"%s\"\n", hvname);
	}
	if (SvOOK(sv)) {
	    const AV * const backrefs = *Perl_hv_backreferences_p(aTHX_ (HV*)sv);
	    if (backrefs) {
		Perl_dump_indent(aTHX_ level, file, "  BACKREFS = 0x%"UVxf"\n",
				 PTR2UV(backrefs));
		do_sv_dump(level+1, file, (SV*)backrefs, nest+1, maxnest,
			   dumpops, pvlim);
	    }
	}
	if (nest < maxnest && !HvEITER_get(sv)) { /* Try to preserve iterator */
	    HE *he;
	    HV * const hv = (HV*)sv;
	    int count = maxnest - nest;

	    hv_iterinit(hv);
	    while ((he = hv_iternext_flags(hv, HV_ITERNEXT_WANTPLACEHOLDERS))
                   && count--) {
		STRLEN len;
		const U32 hash = HeHASH(he);
		SV * const keysv = hv_iterkeysv(he);
		const char * const keypv = SvPV_const(keysv, len);
		SV * const elt = hv_iterval(hv, he);

		Perl_dump_indent(aTHX_ level+1, file, "Elt %s ", pv_display(d, keypv, len, 0, pvlim));
		PerlIO_printf(file, "[UTF8 \"%s\"] ", sv_uni_display(d, keysv, 8 * sv_len_utf8(keysv), UNI_DISPLAY_QQ));
		if (HeKREHASH(he))
		    PerlIO_printf(file, "[REHASH] ");
		PerlIO_printf(file, "HASH = 0x%"UVxf"\n", (UV)hash);
		do_sv_dump(level+1, file, elt, nest+1, maxnest, dumpops, pvlim);
	    }
	    hv_iterinit(hv);		/* Return to status quo */
	}
	break;
    case SVt_PVCV:
	if (!CvISXSUB(sv)) {
	    if (CvSTART(sv)) {
		Perl_dump_indent(aTHX_ level, file,
				 "  START = 0x%"UVxf" ===> %"IVdf"\n",
				 PTR2UV(CvSTART(sv)),
				 (IV)sequence_num(CvSTART(sv)));
	    }
	    Perl_dump_indent(aTHX_ level, file, "  ROOT = 0x%"UVxf"\n",
			     PTR2UV(CvROOT(sv)));
	    if (CvROOT(sv) && dumpops) {
		do_op_dump(level+1, file, RootopOp(CvROOT(sv)));
	    }
	} else {
	    SV * const constant = cv_const_sv((CV *)sv);

	    Perl_dump_indent(aTHX_ level, file, "  XSUB = 0x%"UVxf"\n", PTR2UV(CvXSUB(sv)));

	    if (constant) {
		Perl_dump_indent(aTHX_ level, file, "  XSUBANY = 0x%"UVxf
				 " (CONST SV)\n",
				 PTR2UV(CvXSUBANY(sv).any_ptr));
		do_sv_dump(level+1, file, constant, nest+1, maxnest, dumpops,
			   pvlim);
	    } else {
		Perl_dump_indent(aTHX_ level, file, "  XSUBANY = %"IVdf"\n",
				 (IV)CvXSUBANY(sv).any_i32);
	    }
	}
	Perl_dump_indent(aTHX_ level, file, "  DEPTH = %"IVdf"\n", (IV)CvDEPTH(sv));
	Perl_dump_indent(aTHX_ level, file, "  FLAGS = 0x%"UVxf"\n", (UV)CvFLAGS(sv));
	Perl_dump_indent(aTHX_ level, file, "  PADLIST = 0x%"UVxf"\n", PTR2UV(CvPADLIST(sv)));
	if (nest < maxnest) {
	    do_dump_pad(level+1, file, CvPADLIST(sv), 0);
	}
	break;
    case SVt_PVGV:
	if (SvVALID(sv)) {
	    Perl_dump_indent(aTHX_ level, file, "  FLAGS = %u\n", (U8)BmFLAGS(sv));
	    Perl_dump_indent(aTHX_ level, file, "  RARE = %u\n", (U8)BmRARE(sv));
	    Perl_dump_indent(aTHX_ level, file, "  PREVIOUS = %"UVuf"\n", (UV)BmPREVIOUS(sv));
	    Perl_dump_indent(aTHX_ level, file, "  USEFUL = %"IVdf"\n", (IV)BmUSEFUL(sv));
	}
	if (!isGV_with_GP(sv))
	    break;
	Perl_dump_indent(aTHX_ level, file, "  NAME = \"%s\"\n", GvNAME(sv));
	Perl_dump_indent(aTHX_ level, file, "  NAMELEN = %"IVdf"\n", (IV)GvNAMELEN(sv));
	do_hv_dump (level, file, "  GvSTASH", GvSTASH(sv));
	Perl_dump_indent(aTHX_ level, file, "  GP = 0x%"UVxf"\n", PTR2UV(GvGP(sv)));
	if (!GvGP(sv))
	    break;
	Perl_dump_indent(aTHX_ level, file, "    SV = 0x%"UVxf"\n", PTR2UV(GvSV(sv)));
	Perl_dump_indent(aTHX_ level, file, "    REFCNT = %"IVdf"\n", (IV)GvREFCNT(sv));
	Perl_dump_indent(aTHX_ level, file, "    IO = 0x%"UVxf"\n", PTR2UV(GvIOp(sv)));
	Perl_dump_indent(aTHX_ level, file, "    AV = 0x%"UVxf"\n", PTR2UV(GvAV(sv)));
	Perl_dump_indent(aTHX_ level, file, "    HV = 0x%"UVxf"\n", PTR2UV(GvHV(sv)));
	Perl_dump_indent(aTHX_ level, file, "    CV = 0x%"UVxf"\n", PTR2UV(GvCV(sv)));
	Perl_dump_indent(aTHX_ level, file, "    CVGEN = 0x%"UVxf"\n", (UV)GvCVGEN(sv));
	Perl_dump_indent(aTHX_ level, file, "    FLAGS = 0x%"UVxf"\n", (UV)GvFLAGS(sv));
	do_gv_dump (level, file, "    EGV", GvEGV(sv));
	break;
    case SVt_PVIO:
	Perl_dump_indent(aTHX_ level, file, "  IFP = 0x%"UVxf"\n", PTR2UV(IoIFP(sv)));
	Perl_dump_indent(aTHX_ level, file, "  OFP = 0x%"UVxf"\n", PTR2UV(IoOFP(sv)));
	Perl_dump_indent(aTHX_ level, file, "  DIRP = 0x%"UVxf"\n", PTR2UV(IoDIRP(sv)));
	Perl_dump_indent(aTHX_ level, file, "  LINES = %"IVdf"\n", (IV)IoLINES(sv));
	if (isPRINT(IoTYPE(sv)))
            Perl_dump_indent(aTHX_ level, file, "  TYPE = '%c'\n", IoTYPE(sv));
	else
            Perl_dump_indent(aTHX_ level, file, "  TYPE = '\\%o'\n", IoTYPE(sv));
	Perl_dump_indent(aTHX_ level, file, "  FLAGS = 0x%"UVxf"\n", (UV)IoFLAGS(sv));
	break;
    }
    SvREFCNT_dec(d);
}

void
Perl_sv_dump(pTHX_ SV *sv)
{
    dVAR;

    PERL_ARGS_ASSERT_SV_DUMP;

    if (SvROK(sv))
	do_sv_dump(0, Perl_debug_log, sv, 0, 4, 0, 0);
    else
	do_sv_dump(0, Perl_debug_log, sv, 0, 0, 0, 0);
}

int
Perl_runops_debug(pTHX)
{
    dVAR;
    if (!PL_op) {
	if (ckWARN_d(WARN_DEBUGGING))
	    Perl_warner(aTHX_ packWARN(WARN_DEBUGGING), "NULL OP IN RUN");
	return 0;
    }

    DEBUG_l(Perl_deb(aTHX_ "Entering new RUNOPS level\n"));
    do {
	if (PL_destroyav)
	    call_destructors();
	DEBUG_R(refcnt_check(aTHX));
	PERL_ASYNC_CHECK();
	if (PL_debug) {
	    if (PL_watchaddr && (*PL_watchaddr != PL_watchok))
		PerlIO_printf(Perl_debug_log,
			      "WARNING: %"UVxf" changed from %"UVxf" to %"UVxf"\n",
			      PTR2UV(PL_watchaddr), PTR2UV(PL_watchok),
			      PTR2UV(*PL_watchaddr));
	    if (DEBUG_s_TEST_) {
		if (DEBUG_v_TEST_) {
		    PerlIO_printf(Perl_debug_log, "\n");
		    deb_stack_all();
		}
		else
		    debstack();
	    }


	    if (DEBUG_t_TEST_) debop(PL_op);
	    if (DEBUG_P_TEST_) debprof(PL_op);
	}
    } while ((PL_op = CALL_FPTR(PL_op->op_ppaddr)(aTHX)));
    DEBUG_R(refcnt_check(aTHX));
    DEBUG_l(Perl_deb(aTHX_ "leaving RUNOPS level\n"));

    return 0;
}

I32
Perl_debop(pTHX_ const OP *o)
{
    dVAR;

    PERL_ARGS_ASSERT_DEBOP;

    if (CopSTASH_eq(PL_curcop, PL_debstash) && !DEBUG_J_TEST_)
	return 0;

    Perl_deb(aTHX_ "%s", OP_NAME(o));
    switch (o->op_type) {
    case OP_CONST:
    case OP_HINTSEVAL:
	PerlIO_printf(Perl_debug_log, "(%s)", SvPEEK(cSVOPo_sv));
	break;
    case OP_GVSV:
    case OP_GV:
	if (cGVOPo_gv) {
	    SV * const sv = newSV(0);
	    gv_fullname3(sv, cGVOPo_gv, NULL);
	    PerlIO_printf(Perl_debug_log, "(%s)", SvPV_nolen_const(sv));
	    SvREFCNT_dec(sv);
	}
	else
	    PerlIO_printf(Perl_debug_log, "(NULL)");
	break;
    case OP_PADSV:
	{
	/* print the lexical's name */
	CV * const cv = deb_curcv(cxstack_ix);
	SV *sv;
        if (cv) {
	    AV * const padlist = CvPADLIST(cv);
            AV * const comppad = (AV*)(*av_fetch(padlist, 0, FALSE));
            sv = *av_fetch(comppad, o->op_targ, FALSE);
        } else
            sv = NULL;
        if (sv)
	    PerlIO_printf(Perl_debug_log, "(%s)", SvPV_nolen_const(sv));
        else
	    PerlIO_printf(Perl_debug_log, "[%"UVuf"]", (UV)o->op_targ);
	}
        break;
    default:
	break;
    }
    PerlIO_printf(Perl_debug_log, "\n");
    return 0;
}

STATIC CV*
S_deb_curcv(pTHX_ const I32 ix)
{
    dVAR;
    const PERL_CONTEXT * const cx = &cxstack[ix];
    if (CxTYPE(cx) == CXt_SUB)
        return cx->blk_sub.cv;
    else if (CxTYPE(cx) == CXt_EVAL && !CxTRYBLOCK(cx))
        return PL_compcv;
    else if (ix == 0 && PL_curstackinfo->si_type == PERLSI_MAIN)
        return PL_main_cv;
    else if (ix <= 0)
        return NULL;
    else
        return deb_curcv(ix - 1);
}

void
Perl_watch(pTHX_ char **addr)
{
    dVAR;

    PERL_ARGS_ASSERT_WATCH;

    PL_watchaddr = addr;
    PL_watchok = *addr;
    PerlIO_printf(Perl_debug_log, "WATCHING, %"UVxf" is currently %"UVxf"\n",
	PTR2UV(PL_watchaddr), PTR2UV(PL_watchok));
}

STATIC void
S_debprof(pTHX_ const OP *o)
{
    dVAR;

    PERL_ARGS_ASSERT_DEBPROF;

    if (!DEBUG_J_TEST_ && CopSTASH_eq(PL_curcop, PL_debstash))
	return;
    if (!PL_profiledata)
	Newxz(PL_profiledata, MAXO, U32);
    ++PL_profiledata[o->op_type];
}

void
Perl_debprofdump(pTHX)
{
    dVAR;
    unsigned i;
    if (!PL_profiledata)
	return;
    for (i = 0; i < MAXO; i++) {
	if (PL_profiledata[i])
	    PerlIO_printf(Perl_debug_log,
			  "%5lu %s\n", (unsigned long)PL_profiledata[i],
                                       PL_op_name[i]);
    }
}

#ifdef PERL_MAD
/*
 *    XML variants of most of the above routines
 */

STATIC void
S_xmldump_attr(pTHX_ I32 level, PerlIO *file, const char* pat, ...)
{
    va_list args;

    PERL_ARGS_ASSERT_XMLDUMP_ATTR;

    PerlIO_printf(file, "\n    ");
    va_start(args, pat);
    xmldump_vindent(level, file, pat, &args);
    va_end(args);
}


void
Perl_xmldump_indent(pTHX_ I32 level, PerlIO *file, const char* pat, ...)
{
    va_list args;
    PERL_ARGS_ASSERT_XMLDUMP_INDENT;
    va_start(args, pat);
    xmldump_vindent(level, file, pat, &args);
    va_end(args);
}

void
Perl_xmldump_vindent(pTHX_ I32 level, PerlIO *file, const char* pat, va_list *args)
{
    PERL_ARGS_ASSERT_XMLDUMP_VINDENT;

    PerlIO_printf(file, "%*s", (int)(level*PL_dumpindent), "");
    PerlIO_vprintf(file, pat, *args);
}

void
Perl_xmldump_all(pTHX)
{
    PerlIO_setlinebuf(PL_xmlfp);
    if (PL_main_root)
	op_xmldump(PL_main_root);
    if (PL_xmlfp != (PerlIO*)PerlIO_stdout())
	PerlIO_close(PL_xmlfp);
    PL_xmlfp = 0;
}

void
Perl_xmldump_packsubs(pTHX_ const HV *stash)
{
    I32	i;
    HE	*entry;

    PERL_ARGS_ASSERT_XMLDUMP_PACKSUBS;

    if (!HvARRAY(stash))
	return;
    for (i = 0; i <= (I32) HvMAX(stash); i++) {
	for (entry = HvARRAY(stash)[i]; entry; entry = HeNEXT(entry)) {
	    GV *gv = (GV*)HeVAL(entry);
	    HV *hv;
	    if (SvTYPE(gv) != SVt_PVGV || !GvGP(gv))
		continue;
	    if (GvCVu(gv))
		xmldump_sub(gv);
	    if (HeKEY(entry)[HeKLEN(entry)-1] == ':'
		&& (hv = GvHV(gv)) && hv != PL_defstash)
		xmldump_packsubs(hv);		/* nested package */
	}
    }
}

void
Perl_xmldump_sub(pTHX_ const GV *gv)
{
    SV * const sv = sv_newmortal();

    PERL_ARGS_ASSERT_XMLDUMP_SUB;

    gv_fullname3(sv, gv, NULL);
    Perl_xmldump_indent(aTHX_ 0, PL_xmlfp, "\nSUB %s = ", SvPVX_const(sv));
    if (CvXSUB(GvCV(gv)))
	Perl_xmldump_indent(aTHX_ 0, PL_xmlfp, "(xsub 0x%"UVxf" %d)\n",
	    PTR2UV(CvXSUB(GvCV(gv))),
	    (int)CvXSUBANY(GvCV(gv)).any_i32);
    else if (CvROOT(GvCV(gv)))
	op_xmldump(CvROOT(GvCV(gv)));
    else
	Perl_xmldump_indent(aTHX_ 0, PL_xmlfp, "<undef>\n");
}

void
Perl_xmldump_eval(pTHX)
{
    op_xmldump(PL_eval_root);
}

const char *
Perl_sv_catxmlsv(pTHX_ SV *dsv, SV *ssv)
{
    PERL_ARGS_ASSERT_SV_CATXMLSV;
    return sv_catxmlpvn(dsv, SvPVX_const(ssv), SvCUR(ssv));
}

const char *
Perl_sv_catxmlpvn(pTHX_ SV *dsv, const char *pv, STRLEN len)
{
    unsigned int c;
    const char * const e = pv + len;
    STRLEN cl;

    PERL_ARGS_ASSERT_SV_CATXMLPVN;

    sv_catpvn(dsv,"",0);

  retry:
    while (pv < e) {
	c = utf8n_to_uvchr(pv, UTF8_MAXBYTES, &cl, UTF8_CHECK_ONLY);
	if ( (cl == (STRLEN)-1) ) {
	    c = ((U8)*pv & 0xFF);
	    Perl_sv_catpvf(aTHX_ dsv, "STUPIDXML(#x[%X])", c);
	    pv++;

	    goto retry;
	}

	switch (c) {
	case 0x00:
	case 0x01:
	case 0x02:
	case 0x03:
	case 0x04:
	case 0x05:
	case 0x06:
	case 0x07:
	case 0x08:
	case 0x0b:
	case 0x0c:
	case 0x0e:
	case 0x0f:
	case 0x10:
	case 0x11:
	case 0x12:
	case 0x13:
	case 0x14:
	case 0x15:
	case 0x16:
	case 0x17:
	case 0x18:
	case 0x19:
	case 0x1a:
	case 0x1b:
	case 0x1c:
	case 0x1d:
	case 0x1e:
	case 0x1f:
	case 0x7f:
	case 0x80:
	case 0x81:
	case 0x82:
	case 0x83:
	case 0x84:
	case 0x86:
	case 0x87:
	case 0x88:
	case 0x89:
	case 0x90:
	case 0x91:
	case 0x92:
	case 0x93:
	case 0x94:
	case 0x95:
	case 0x96:
	case 0x97:
	case 0x98:
	case 0x99:
	case 0x9a:
	case 0x9b:
	case 0x9c:
	case 0x9d:
	case 0x9e:
	case 0x9f:
	    Perl_sv_catpvf(aTHX_ dsv, "STUPIDXML(#x%X)", c);
	    break;
	case '<':
	    sv_catpvs(dsv, "&lt;");
	    break;
	case '>':
	    sv_catpvs(dsv, "&gt;");
	    break;
	case '&':
	    sv_catpvs(dsv, "&amp;");
	    break;
	case '"':
	    sv_catpvs(dsv, "&#34;");
	    break;
	default:
	    if (c < 0xD800) {
		if (c < 32 || c > 127) {
		    Perl_sv_catpvf(aTHX_ dsv, "&#x%X;", c);
		}
		else {
		    const char string = (char) c;
		    sv_catpvn(dsv, &string, 1);
		}
		break;
	    }
	    if ((c >= 0xD800 && c <= 0xDB7F) ||
		(c >= 0xDC00 && c <= 0xDFFF) ||
		(c >= 0xFFF0 && c <= 0xFFFF) ||
		 c > 0x10ffff)
		Perl_sv_catpvf(aTHX_ dsv, "STUPIDXML(#x%X)", c);
	    else
		Perl_sv_catpvf(aTHX_ dsv, "&#x%X;", c);
	}

	pv += cl;
    }

    return SvPVX_const(dsv);
}

const char *
Perl_sv_xmlpeek(pTHX_ SV *sv)
{
    SV * const t = sv_newmortal();
    int unref = 0;

    PERL_ARGS_ASSERT_SV_XMLPEEK;

    sv_setpvn(t, "", 0);
    /* retry: */
    if (!sv) {
	sv_catpv(t, "VOID=\"\"");
	goto finish;
    }
    else if (sv == (SV*)0x55555555 || SvTYPE(sv) == 'U') {
	sv_catpv(t, "WILD=\"\"");
	goto finish;
    }
    else if (sv == &PL_sv_undef || sv == &PL_sv_no || sv == &PL_sv_yes || sv == &PL_sv_placeholder) {
	if (sv == &PL_sv_undef) {
	    sv_catpv(t, "SV_UNDEF=\"1\"");
	    if (!(SvFLAGS(sv) & (SVf_OK|SVf_OOK|SVs_OBJECT|
				 SVs_SMG|SVs_RMG)) &&
		SvREADONLY(sv))
		goto finish;
	}
	else if (sv == &PL_sv_no) {
	    sv_catpv(t, "SV_NO=\"1\"");
	    if (!(SvFLAGS(sv) & (SVf_ROK|SVf_OOK|SVs_OBJECT|
				 SVs_SMG|SVs_RMG)) &&
		!(~SvFLAGS(sv) & (SVf_POK|SVf_NOK|SVf_READONLY|
				  SVp_POK|SVp_NOK)) &&
		SvCUR(sv) == 0 &&
		SvNVX(sv) == 0.0)
		goto finish;
	}
	else if (sv == &PL_sv_yes) {
	    sv_catpv(t, "SV_YES=\"1\"");
	    if (!(SvFLAGS(sv) & (SVf_ROK|SVf_OOK|SVs_OBJECT|
				 SVs_SMG|SVs_RMG)) &&
		!(~SvFLAGS(sv) & (SVf_POK|SVf_NOK|SVf_READONLY|
				  SVp_POK|SVp_NOK)) &&
		SvCUR(sv) == 1 &&
		*SvPVX_const(sv) == '1' &&
		SvNVX(sv) == 1.0)
		goto finish;
	}
	else {
	    sv_catpv(t, "SV_PLACEHOLDER=\"1\"");
	    if (!(SvFLAGS(sv) & (SVf_OK|SVf_OOK|SVs_OBJECT|
				 SVs_SMG|SVs_RMG)) &&
		SvREADONLY(sv))
		goto finish;
	}
	sv_catpv(t, " XXX=\"\" ");
    }
    else if (SvREFCNT(sv) == 0) {
	sv_catpv(t, " refcnt=\"0\"");
	unref++;
    }
    else if (DEBUG_R_TEST_) {
	int is_tmp = 0;
	I32 ix;
	/* is this SV on the tmps stack? */
	for (ix=PL_tmps_ix; ix>=0; ix--) {
	    if (PL_tmps_stack[ix] == sv) {
		is_tmp = 1;
		break;
	    }
	}
	if (SvREFCNT(sv) > 1)
	    Perl_sv_catpvf(aTHX_ t, " DRT=\"<%"UVuf"%s>\"", (UV)SvREFCNT(sv),
		    is_tmp ? "T" : "");
	else if (is_tmp)
	    sv_catpv(t, " DRT=\"<T>\"");
    }

    if (SvROK(sv)) {
	sv_catpv(t, " ROK=\"\"");
    }
    switch (SvTYPE(sv)) {
    default:
	sv_catpv(t, " FREED=\"1\"");
	goto finish;

    case SVt_NULL:
	sv_catpv(t, " UNDEF=\"1\"");
	goto finish;
    case SVt_IV:
	sv_catpv(t, " IV=\"");
	break;
    case SVt_NV:
	sv_catpv(t, " NV=\"");
	break;
    case SVt_PV:
	sv_catpv(t, " PV=\"");
	break;
    case SVt_PVIV:
	sv_catpv(t, " PVIV=\"");
	break;
    case SVt_PVNV:
	sv_catpv(t, " PVNV=\"");
	break;
    case SVt_PVMG:
	sv_catpv(t, " PVMG=\"");
	break;
    case SVt_PVAV:
	sv_catpv(t, " AV=\"");
	break;
    case SVt_PVHV:
	sv_catpv(t, " HV=\"");
	break;
    case SVt_PVCV:
	sv_catpv(t, " CV=\"()\"");
	goto finish;
    case SVt_PVGV:
	sv_catpv(t, " GV=\"");
	break;
    case SVt_BIND:
	sv_catpv(t, " BIND=\"");
	break;
    case SVt_REGEXP:
	sv_catpv(t, " ORANGE=\"");
	break;
    case SVt_PVIO:
	sv_catpv(t, " IO=\"");
	break;
    }

    if (SvPOKp(sv)) {
	if (SvPVX_const(sv)) {
	    sv_catxmlsv(t, sv);
	}
    }
    else if (SvNOKp(sv)) {
	Perl_sv_catpvf(aTHX_ t, "%"NVgf"",SvNVX(sv));
    }
    else if (SvIOKp(sv)) {
	if (SvIsUV(sv))
	    Perl_sv_catpvf(aTHX_ t, "%"UVuf"", (UV)SvUVX(sv));
	else
            Perl_sv_catpvf(aTHX_ t, "%"IVdf"", (IV)SvIVX(sv));
    }
    else
	sv_catpv(t, "");
    sv_catpv(t, "\"");

  finish:
    while (unref--)
	sv_catpv(t, ")");
    return SvPVX_const(t);
}

void
Perl_do_pmop_xmldump(pTHX_ I32 level, PerlIO *file, const PMOP *pm)
{
    PERL_ARGS_ASSERT_DO_PMOP_XMLDUMP;

    if (!pm) {
	Perl_xmldump_indent(aTHX_ level, file, "<pmop/>\n");
	return;
    }
    Perl_xmldump_indent(aTHX_ level, file, "<pmop \n");
    level++;
    if (PM_GETRE(pm)) {
	REGEXP *const r = PM_GETRE(pm);
	SV * const tmpsv = newSVpvn("", 0);
	sv_catxmlsv(tmpsv, (SV*)r);
	Perl_xmldump_indent(aTHX_ level, file, "pre=\"%s\"\n",
	     SvPVX_const(tmpsv));
	SvREFCNT_dec(tmpsv);
	Perl_xmldump_indent(aTHX_ level, file, "when=\"%s\"\n",
	     (pm->op_private & OPpRUNTIME) ? "RUN" : "COMP");
    }
    else
	Perl_xmldump_indent(aTHX_ level, file, "pre=\"\" when=\"RUN\"\n");
    if (pm->op_pmflags || (PM_GETRE(pm) && RX_CHECK_SUBSTR(PM_GETRE(pm)))) {
	SV * const tmpsv = pm_description(pm);
	Perl_xmldump_indent(aTHX_ level, file, "pmflags=\"%s\"\n", SvCUR(tmpsv) ? SvPVX_const(tmpsv) + 1 : "");
	SvREFCNT_dec(tmpsv);
    }

    level--;
    if (pm->op_type != OP_PUSHRE && pm->op_pmreplrootu.op_pmreplroot) {
	Perl_xmldump_indent(aTHX_ level, file, ">\n");
	Perl_xmldump_indent(aTHX_ level+1, file, "<pm_repl>\n");
	do_op_xmldump(level+2, file, pm->op_pmreplrootu.op_pmreplroot);
	Perl_xmldump_indent(aTHX_ level+1, file, "</pm_repl>\n");
	Perl_xmldump_indent(aTHX_ level, file, "</pmop>\n");
    }
    else
	Perl_xmldump_indent(aTHX_ level, file, "/>\n");
}

void
Perl_pmop_xmldump(pTHX_ const PMOP *pm)
{
    do_pmop_xmldump(0, PL_xmlfp, pm);
}

static struct { const char slot; const char* name; } const slotnames[] =
{
    { '#', "wsafter" },
    { '$', "variable" },
    { '%', "hsh" },
    { '&', "ampersand" },
    { '(', "round_open" },
    { ')', "round_close" },
    { '*', "star" },
    { '+', "unary_plus" },
    { ',', "comma" },
    { '1', "arg_1" },
    { '2', "arg_2" },
    { '3', "arg_3" },
    { ':', "attribute" },
    { ';', "semicolon" },
    { '<', "null_type_first" },
    { '=', "assign" },
    { '>', "null_type" },
    { '?', "conditional_op" },
    { '@', "ary" },
    { 'A', "bigarrow" },
    { 'B', "block" },
    { 'C', "const" },
    { 'D', "do" },
    { 'E', "evaluated" },
    { 'F', "format" },
    { 'G', "endsection" },
    { 'H', "optional_assign" },
    { 'I', "if" },
    { 'K', "key" },
    { 'L', "label" },
    { 'O', "replacedoperator" },
    { 'P', "package" },
    { 'Q', "quote_close" },
    { 'R', "subst_replacement" },
    { 'S', "fakesub" },
    { 'U', "use" },
    { 'V', "version" },
    { 'W', "while" },
    { 'X', "value" },
    { 'Z', "subst_close" },
    { '[', "square_open" },
    { ']', "square_close" },
    { '^', "hat" },
    { '_', "wsbefore" },
    { 'a', "arrow" },
    { 'b', "unknown_b" },
    { 'c', "prototyped" },
    { 'd', "defintion" },
    { 'e', "trans_something_e" },
    { 'f', "fold" },
    { 'g', "forcedword" },
    { 'h', "constsub_args" },
    { 'i', "ifpost" },
    { 'j', "slice_close" },
    { 'k', "local" },
    { 'm', "match" },
    { 'n', "name" },
    { 'o', "operator" },
    { 'p', "peg" },
    { 'q', "quote_open" },
    { 'r', "trans_something_r" },
    { 's', "sub" },
    { 't', "something_t" },
    { 'u', "fake_semicolon" },
    { 'v', "for" },
    { 'w', "whilepost" },
    { 'z', "subst_open" },
    { '{', "curly_open" },
    { '}', "curly_close" },
    { '~', "tilde" },
    { 0,   NULL }
};
const char* slotname(char s) {
    int i=0;
    while (slotnames[i].slot != 0) {
	if (slotnames[i].slot == s)
	    return slotnames[i].name;
	i++;
    }
    return NULL;
}

void
Perl_do_op_xmldump(pTHX_ I32 level, PerlIO *file, const OP *o)
{
    UV      seq;
    int     contents = 0;

    PERL_ARGS_ASSERT_DO_OP_XMLDUMP;

    if (!o)
	return;
    sequence(o);
    seq = sequence_num(o);
    Perl_xmldump_indent(aTHX_ level, file,
	"<op_%s seq=\"%"UVuf" -> ",
	     OP_NAME(o),
	              seq);
    level++;
    if (o->op_next)
	PerlIO_printf(file, seq ? "%"UVuf"\"" : "(%"UVuf")\"",
		      sequence_num(o->op_next));
    else
	PerlIO_printf(file, "DONE\"");

    if (o->op_targ) {
	if (o->op_type == OP_NULL)
	{
	    PerlIO_printf(file, " was=\"%s\"", PL_op_name[o->op_targ]);
	    if (o->op_targ == OP_NEXTSTATE)
	    {
		if (CopSTASHPV(cCOPo))
		    PerlIO_printf(file, " package=\"%s\"",
				     CopSTASHPV(cCOPo));
		if (cCOPo->cop_label)
		    PerlIO_printf(file, " label=\"%s\"",
				     cCOPo->cop_label);
	    }
	}
	else
	    PerlIO_printf(file, " targ=\"%ld\"", (long)o->op_targ);
    }
#ifdef DUMPADDR
    PerlIO_printf(file, " addr=\"0x%"UVxf" => 0x%"UVxf"\"", (UV)o, (UV)o->op_next);
#endif
    if (o->op_flags ) {  /* || o->op_latefree || o->op_latefreed || o->op_attached) { */
	SV * const tmpsv = S_dump_op_flags(aTHX_ o);
	PerlIO_printf(file, " flags=\"%s\"", SvCUR(tmpsv) ? SvPVX_const(tmpsv) + 1 : "");
    }
    if (o->op_private) {
	SV * const tmpsv = S_dump_op_flags_private(aTHX_ o);
	if (SvCUR(tmpsv))
	    S_xmldump_attr(aTHX_ level, file, "private=\"%s\"", SvPVX_const(tmpsv) + 1);
    }

    switch (o->op_type) {
    case OP_AELEMFAST:
	if (o->op_flags & OPf_SPECIAL) {
	    break;
	}
    case OP_GVSV:
    case OP_GV:
	if (cSVOPo->op_sv) {
	    SV * const tmpsv1 = newSVpvn(NULL, 0);
	    SV * const tmpsv2 = newSVpvn("", 0);
	    char *s;
	    STRLEN len;
	    ENTER;
	    SAVEFREESV(tmpsv1);
	    SAVEFREESV(tmpsv2);
	    gv_fullname3(tmpsv1, (GV*)cSVOPo->op_sv, NULL);
	    s = SvPV(tmpsv1,len);
	    sv_catxmlpvn(tmpsv2, s, len);
	    S_xmldump_attr(aTHX_ level, file, "gv=\"%s\"", SvPV(tmpsv2, len));
	    LEAVE;
	}
	else
	    S_xmldump_attr(aTHX_ level, file, "gv=\"NULL\"");
	break;
    case OP_CONST:
    case OP_HINTSEVAL:
    case OP_METHOD_NAMED:
	S_xmldump_attr(aTHX_ level, file, "%s", sv_xmlpeek(cSVOPo_sv));
	break;
    case OP_ANONCODE:
	if (!contents) {
	    contents = 1;
	    PerlIO_printf(file, ">\n");
	}
	do_op_xmldump(level+1, file, CvROOT(cSVOPo_sv));
	break;
    case OP_NEXTSTATE:
    case OP_DBSTATE:
	if (CopSTASHPV(cCOPo))
	    S_xmldump_attr(aTHX_ level, file, "package=\"%s\"",
			     CopSTASHPV(cCOPo));
	if (cCOPo->cop_label)
	    S_xmldump_attr(aTHX_ level, file, "label=\"%s\"",
			     cCOPo->cop_label);
	break;
    case OP_ENTERLOOP:
	S_xmldump_attr(aTHX_ level, file, "redo=\"");
	if (cLOOPo->op_redoop)
	    PerlIO_printf(file, "%"UVuf"\"", sequence_num(cLOOPo->op_redoop));
	else
	    PerlIO_printf(file, "DONE\"");
	S_xmldump_attr(aTHX_ level, file, "next=\"");
	if (cLOOPo->op_nextop)
	    PerlIO_printf(file, "%"UVuf"\"", sequence_num(cLOOPo->op_nextop));
	else
	    PerlIO_printf(file, "DONE\"");
	S_xmldump_attr(aTHX_ level, file, "last=\"");
	if (cLOOPo->op_lastop)
	    PerlIO_printf(file, "%"UVuf"\"", sequence_num(cLOOPo->op_lastop));
	else
	    PerlIO_printf(file, "DONE\"");
	break;
    case OP_COND_EXPR:
    case OP_RANGE:
    case OP_MAPWHILE:
    case OP_GREPWHILE:
    case OP_OR:
    case OP_AND:
	S_xmldump_attr(aTHX_ level, file, "other=\"");
	if (cLOGOPo->op_other)
	    PerlIO_printf(file, "%"UVuf"\"", sequence_num(cLOGOPo->op_other));
	else
	    PerlIO_printf(file, "DONE\"");
	break;
    case OP_LEAVE:
    case OP_LEAVEEVAL:
    case OP_LEAVESUB:
    case OP_SCOPE:
	break;
    default:
	break;
    }

    if (PL_madskills && o->op_madprop) {
	SV * const tmpsv = newSVpvn("", 0);
	const MADPROP* mp = o->op_madprop;

	if (!contents) {
	    contents = 1;
	    PerlIO_printf(file, ">\n");
	}
	Perl_xmldump_indent(aTHX_ level, file, "<madprops>\n");
	level++;
	while (mp) {
	    char tmp = mp->mad_key;
	    sv_setpvn(tmpsv,"",0);
	    if (tmp) {
		if (slotname(tmp))
		    sv_catpv(tmpsv, slotname(tmp));
		else
		    Perl_croak(aTHX_ "madprop error: Unknow slot '%c'.", tmp);
	    }
	    switch (mp->mad_type) {
	    case MAD_SV:
		sv_catpv(tmpsv, " val=\"");
		sv_catxmlsv(tmpsv, (SV*)mp->mad_val);
		sv_catpv(tmpsv, "\" ");

		{
		    const MADPROP* next = mp->mad_next;
		    if (next) {
			char xnext = next->mad_key;
			if ((xnext == '_') || (xnext == '#')) { /* '_' '#' whitespace belong to the previous token. */
			    sv_catpv(tmpsv, slotname(xnext));
			    sv_catpv(tmpsv, "=\"");
			    sv_catxmlsv(tmpsv, (SV*)next->mad_val);
			    sv_catpv(tmpsv, "\" ");
			    mp = next;
			}
		    }
		    next = mp->mad_next;
		    if (next) {
			char xnext = next->mad_key;
			if ((xnext == '_') || (xnext == '#')) { /* '_' '#' whitespace belong to the previous token. */
			    sv_catpv(tmpsv, slotname(xnext));
			    sv_catpv(tmpsv, "=\"");
			    sv_catxmlsv(tmpsv, (SV*)next->mad_val);
			    sv_catpv(tmpsv, "\" ");
			    mp = next;
			}
		    }
		}

		sv_catpv(tmpsv, "/>\n");
		Perl_xmldump_indent(aTHX_ level, file, "<mad_%s", SvPVX_const(tmpsv));
		break;
	    case MAD_OP:
		/* in next loop */
		if ((OP*)mp->mad_val) {
		    Perl_xmldump_indent(aTHX_ level, file, "<mad_op key=\"%s\">\n", SvPVX_const(tmpsv));
		    do_op_xmldump(level+1, file, (OP*)mp->mad_val);
		    Perl_xmldump_indent(aTHX_ level, file, "</mad_op>\n");
		}
		break;
	    default:
		Perl_croak(aTHX_ "unknown MAD_type");
		break;
	    }
	    mp = mp->mad_next;
	}

	level--;
 	Perl_xmldump_indent(aTHX_ level, file, "</madprops>\n");

	SvREFCNT_dec(tmpsv);
    }

    switch (o->op_type) {
    case OP_PUSHRE:
    case OP_MATCH:
    case OP_QR:
    case OP_SUBST:
	if (!contents) {
	    contents = 1;
	    PerlIO_printf(file, ">\n");
	}
	do_pmop_xmldump(level, file, cPMOPo);
	break;
    default:
	break;
    }

    if (o->op_flags & OPf_KIDS) {
	OP *kid;
	if (!contents) {
	    contents = 1;
	    PerlIO_printf(file, ">\n");
	}
	for (kid = cUNOPo->op_first; kid; kid = kid->op_sibling)
	    do_op_xmldump(level, file, kid);
    }

    if (contents)
	Perl_xmldump_indent(aTHX_ level-1, file, "</op_%s>\n", OP_NAME(o));
    else
	PerlIO_printf(file, " />\n");
}

void
Perl_op_xmldump(pTHX_ const OP *o)
{
    PERL_ARGS_ASSERT_OP_XMLDUMP;

    do_op_xmldump(0, PL_xmlfp, o);
}
#endif

/*
 * Local variables:
 * c-indentation-style: bsd
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 *
 * ex: set ts=8 sts=4 sw=4 noet:
 */
