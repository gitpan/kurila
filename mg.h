/*    mg.h
 *
 *    Copyright (C) 1991, 1992, 1993, 1994, 1995, 1996, 1997, 1999,
 *    2000, 2002, 2005, 2006, by Larry Wall and others
 *
 *    You may distribute under the terms of either the GNU General Public
 *    License or the Artistic License, as specified in the README file.
 *
 */

#ifdef STRUCT_MGVTBL_DEFINITION
STRUCT_MGVTBL_DEFINITION;
#else
struct mgvtbl {
    int		(CPERLscope(*svt_get))	(pTHX_ SV *sv, MAGIC* mg);
    int		(CPERLscope(*svt_set))	(pTHX_ SV *sv, MAGIC* mg);
    U32		(CPERLscope(*svt_len))	(pTHX_ SV *sv, MAGIC* mg);
    int		(CPERLscope(*svt_clear))(pTHX_ SV *sv, MAGIC* mg);
    int		(CPERLscope(*svt_free))	(pTHX_ SV *sv, MAGIC* mg);
    int		(CPERLscope(*svt_copy))	(pTHX_ SV *sv, MAGIC* mg,
    					SV *nsv, const char *name, I32 namlen);
    int		(CPERLscope(*svt_dup))	(pTHX_ MAGIC *mg, CLONE_PARAMS *param);
    int		(CPERLscope(*svt_local))(pTHX_ SV *nsv, MAGIC *mg);
};
#endif

struct magic {
    MAGIC*	mg_moremagic;
    MGVTBL*	mg_virtual;	/* pointer to magic functions */
    U16		mg_private;
    char	mg_type;
    U8		mg_flags;
    I32		mg_len;
    SV*		mg_obj;
    char*	mg_ptr;
};

#define MGf_MINMATCH   1        /* PERL_MAGIC_regex_global only */
#define MGf_REFCOUNTED 2
#define MGf_GSKIP      4
#define MGf_COPY       8	/* has an svt_copy  MGVTBL entry */
#define MGf_DUP     0x10 	/* has an svt_dup   MGVTBL entry */
#define MGf_LOCAL   0x20	/* has an svt_local MGVTBL entry */

#define MgPV(mg,lp)		((((int)(lp = (mg)->mg_len)) == HEf_SVKEY) ?   \
				 SvPV((SV*)((mg)->mg_ptr),lp) :		\
				 (mg)->mg_ptr)
#define MgPV_const(mg,lp)	((((int)(lp = (mg)->mg_len)) == HEf_SVKEY) ? \
				 SvPV_const((SV*)((mg)->mg_ptr),lp) :        \
				 (const char*)(mg)->mg_ptr)
#define MgPV_nolen_const(mg)	(((((int)(mg)->mg_len)) == HEf_SVKEY) ?   \
				 SvPV_nolen_const((SV*)((mg)->mg_ptr)) :  \
				 (const char*)(mg)->mg_ptr)

/*
 * Local variables:
 * c-indentation-style: bsd
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 *
 * ex: set ts=8 sts=4 sw=4 noet:
 */
