/*    xsutils.c
 *
 *    Copyright (C) 1999, 2000, 2001, 2002, 2003, 2004, 2005, 2006,
 *    by Larry Wall and others
 *
 *    You may distribute under the terms of either the GNU General Public
 *    License or the Artistic License, as specified in the README file.
 *
 */

/*
 * "Perilous to us all are the devices of an art deeper than we possess
 * ourselves." --Gandalf
 */


#include "EXTERN.h"
#define PERL_IN_XSUTILS_C
#include "perl.h"

/*
 * Contributed by Spider Boardman (spider.boardman@orb.nashua.nh.us).
 */

/* package attributes; */
PERL_XS_EXPORT_C void XS_attributes_reftype(pTHX_ CV *cv);
PERL_XS_EXPORT_C void XS_attributes__modify_attrs(pTHX_ CV *cv);
PERL_XS_EXPORT_C void XS_attributes__guess_stash(pTHX_ CV *cv);
PERL_XS_EXPORT_C void XS_attributes__fetch_attrs(pTHX_ CV *cv);
PERL_XS_EXPORT_C void XS_attributes_bootstrap(pTHX_ CV *cv);


/*
 * Note that only ${pkg}::bootstrap definitions should go here.
 * This helps keep down the start-up time, which is especially
 * relevant for users who don't invoke any features which are
 * (partially) implemented here.
 *
 * The various bootstrap definitions can take care of doing
 * package-specific newXS() calls.  Since the layout of the
 * bundled *.pm files is in a version-specific directory,
 * version checks in these bootstrap calls are optional.
 */

static const char file[] = __FILE__;

void
Perl_boot_core_xsutils(pTHX)
{
    newXS("attributes::bootstrap",	XS_attributes_bootstrap,	file);
}

#include "XSUB.h"

static void
modify_SV_attributes(pTHX_ SV *sv, AV *retlist, AV *attrlist)
{
    dVAR;
    int numattrs = av_len(attrlist);
    int i;
    
    for (i = 0 ; i <= numattrs; i++) {
	STRLEN len;
        SV *attr = *(av_fetch(attrlist, i, FALSE));
	const char *name = SvPV_const(attr, len);
	const bool negated = (*name == '-');

	if (negated) {
	    name++;
	    len--;
	}
	switch (SvTYPE(sv)) {
	case SVt_PVCV:
	    switch ((int)len) {
	    case 6:
		switch (name[3]) {
		case 'k':
		    if (memEQ(name, "locked", 6)) {
			if (negated)
			    CvFLAGS((CV*)sv) &= ~CVf_LOCKED;
			else
			    CvFLAGS((CV*)sv) |= CVf_LOCKED;
			continue;
		    }
		    break;
		case 'h':
		    if (memEQ(name, "method", 6)) {
			if (negated)
			    CvFLAGS((CV*)sv) &= ~CVf_METHOD;
			else
			    CvFLAGS((CV*)sv) |= CVf_METHOD;
			continue;
		    }
		    break;
		}
		break;
	    }
	    break;
	default:
	    switch ((int)len) {
	    case 6:
		switch (name[5]) {
		case 'd':
		    if (memEQ(name, "share", 5)) {
			if (negated)
			    Perl_croak(aTHX_ "A variable may not be unshared");
			SvSHARE(sv);
                        continue;
                    }
		    break;
		case 'e':
		    if (memEQ(name, "uniqu", 5)) {
			if (SvTYPE(sv) == SVt_PVGV) {
			    if (negated) {
				GvUNIQUE_off(sv);
			    } else {
				GvUNIQUE_on(sv);
			    }
			}
			/* Hope this came from toke.c if not a GV. */
                        continue;
                    }
                }
            }
	    break;
	}
	/* anything recognized had a 'continue' above */
        av_push(retlist, SvREFCNT_inc(attr));
    }
    return;
}



/* package attributes; */

XS(XS_attributes_bootstrap)
{
    dVAR;
    dXSARGS;
    PERL_UNUSED_ARG(cv);

    if( items > 1 )
        Perl_croak(aTHX_ "Usage: attributes::bootstrap $module");

    newXS("attributes::_modify_attrs",	XS_attributes__modify_attrs,	file);
    newXSproto("attributes::_guess_stash", XS_attributes__guess_stash, file, "$");
    newXSproto("attributes::_fetch_attrs", XS_attributes__fetch_attrs, file, "$");
    newXSproto("attributes::reftype",	XS_attributes_reftype,	file, "$");

    XSRETURN(0);
}

XS(XS_attributes__modify_attrs)
{
    dVAR;
    dXSARGS;
    SV *rv, *sv, *attrs, *res;
    PERL_UNUSED_ARG(cv);

    if (items != 2) {
usage:
	Perl_croak(aTHX_
		   "Usage: attributes::_modify_attrs $reference, @attributes");
    }

    rv = ST(0);
    attrs = ST(1);
    if (!(SvOK(rv) && SvROK(rv)))
	goto usage;
    if (!(SvAVOK(attrs)))
	goto usage;
        
    sv = SvRV(rv);
    res = sv_2mortal(newAV());
    modify_SV_attributes(aTHX_ sv, res, attrs);

    ST(0) = res;
    XSRETURN(1);
}

XS(XS_attributes__fetch_attrs)
{
    dVAR;
    dXSARGS;
    SV *rv, *sv;
    cv_flags_t cvflags;
    PERL_UNUSED_ARG(cv);

    if (items != 1) {
usage:
	Perl_croak(aTHX_
		   "Usage: attributes::_fetch_attrs $reference");
    }

    rv = ST(0);
    SP -= items;
    if (!(SvOK(rv) && SvROK(rv)))
	goto usage;
    sv = SvRV(rv);

    switch (SvTYPE(sv)) {
    case SVt_PVCV:
	cvflags = CvFLAGS((CV*)sv);
	if (cvflags & CVf_LOCKED)
	    XPUSHs(newSVpvs_flags("locked", SVs_TEMP));
	if (cvflags & CVf_METHOD)
	    XPUSHs(newSVpvs_flags("method", SVs_TEMP));
        if (GvUNIQUE(CvGV((CV*)sv)))
	    XPUSHs(newSVpvs_flags("unique", SVs_TEMP));
	break;
    case SVt_PVGV:
	if (GvUNIQUE(sv))
	    XPUSHs(newSVpvs_flags("unique", SVs_TEMP));
	break;
    default:
	break;
    }

    PUTBACK;
}

XS(XS_attributes__guess_stash)
{
    dVAR;
    dXSARGS;
    SV *rv, *sv;
    dXSTARG;
    PERL_UNUSED_ARG(cv);

    if (items != 1) {
usage:
	Perl_croak(aTHX_
		   "Usage: attributes::_guess_stash $reference");
    }

    rv = ST(0);
    ST(0) = TARG;
    if (!(SvOK(rv) && SvROK(rv)))
	goto usage;
    sv = SvRV(rv);

    if (SvOBJECT(sv))
	sv_setpvn(TARG, HvNAME_get(SvSTASH(sv)), HvNAMELEN_get(SvSTASH(sv)));
#if 0	/* this was probably a bad idea */
    else if (SvPADMY(sv))
	sv_setsv(TARG, &PL_sv_no);	/* unblessed lexical */
#endif
    else {
	const HV *stash = NULL;
	switch (SvTYPE(sv)) {
	case SVt_PVCV:
	    if (CvGV(sv) && isGV(CvGV(sv)) && GvSTASH(CvGV(sv)))
		stash = GvSTASH(CvGV(sv));
	    break;
	case SVt_PVGV:
	    if (GvGP(sv) && GvESTASH((GV*)sv))
		stash = GvESTASH((GV*)sv);
	    break;
	default:
	    break;
	}
	if (stash)
	    sv_setpvn(TARG, HvNAME_get(stash), HvNAMELEN_get(stash));
    }

    SvSETMAGIC(TARG);
    XSRETURN(1);
}

XS(XS_attributes_reftype)
{
    dVAR;
    dXSARGS;
    SV *rv, *sv;
    dXSTARG;
    PERL_UNUSED_ARG(cv);

    if (items != 1) {
usage:
	Perl_croak(aTHX_
		   "Usage: attributes::reftype $reference");
    }

    rv = ST(0);
    ST(0) = TARG;
    SvGETMAGIC(rv);
    if (!(SvOK(rv) && SvROK(rv)))
	goto usage;
    sv = SvRV(rv);
    sv_setpv(TARG, sv_reftype(sv, 0));
    SvSETMAGIC(TARG);

    XSRETURN(1);
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
