#define PERL_NO_GET_CONTEXT
#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"

/* PL_maxo shouldn't differ from MAXO but leave room anyway (see BOOT:)	*/
#define OP_MASK_BUF_SIZE (MAXO + 100)

/* XXX op_named_bits and opset_all are never freed */
#define MY_CXT_KEY "Opcode::_guts" XS_VERSION

typedef struct {
    HV *	x_op_named_bits;	/* cache shared for whole process */
    SV *	x_opset_all;		/* mask with all bits set	*/
    IV		x_opset_len;		/* length of opmasks in bytes	*/
    int		x_opcode_debug;
} my_cxt_t;

START_MY_CXT

#define op_named_bits		(MY_CXT.x_op_named_bits)
#define opset_all		(MY_CXT.x_opset_all)
#define opset_len		(MY_CXT.x_opset_len)
#define opcode_debug		(MY_CXT.x_opcode_debug)

static SV  *new_opset (pTHX_ SV *old_opset);
static int  verify_opset (pTHX_ SV *opset, int fatal);
static void set_opset_bits (pTHX_ char *bitmap, SV *bitspec, int on, const char *opname);
static void put_op_bitspec (pTHX_ const char *optag,  STRLEN len, SV *opset);
static SV  *get_op_bitspec (pTHX_ const char *opname, STRLEN len, int fatal);


/* Initialise our private op_named_bits HV.
 * It is first loaded with the name and number of each perl operator.
 * Then the builtin tags :none and :all are added.
 * Opcode.pm loads the standard optags from __DATA__
 * XXX leak-alert: data allocated here is never freed, call this
 *     at most once
 */

static void
op_names_init(pTHX)
{
    int i;
    STRLEN len;
    char **op_names;
    char *bitmap;
    dMY_CXT;

    op_named_bits = newHV();
    op_names = get_op_names();
    for(i=0; i < PL_maxo; ++i) {
	SV * const sv = newSViv(i);
	SvREADONLY_on(sv);
	hv_store(op_named_bits, op_names[i], strlen(op_names[i]), sv, 0);
    }

    put_op_bitspec(aTHX_ ":none",0, sv_2mortal(new_opset(aTHX_ Nullsv)));

    opset_all = new_opset(aTHX_ Nullsv);
    bitmap = SvPV(opset_all, len);
    i = len-1; /* deal with last byte specially, see below */
    while(i-- > 0)
	bitmap[i] = (char)0xFF;
    /* Take care to set the right number of bits in the last byte */
    bitmap[len-1] = (PL_maxo & 0x07) ? ~(0xFF << (PL_maxo & 0x07)) : 0xFF;
    put_op_bitspec(aTHX_ ":all",0, opset_all); /* don't mortalise */
}


/* Store a new tag definition. Always a mask.
 * The tag must not already be defined.
 * SV *mask is copied not referenced.
 */

static void
put_op_bitspec(pTHX_ const char *optag, STRLEN len, SV *mask)
{
    SV **svp;
    dMY_CXT;

    verify_opset(aTHX_ mask,1);
    if (!len)
	len = strlen(optag);
    svp = hv_fetch(op_named_bits, optag, len, 1);
    if (SvOK(*svp))
	croak("Opcode tag \"%s\" already defined", optag);
    sv_setsv(*svp, mask);
    SvREADONLY_on(*svp);
}



/* Fetch a 'bits' entry for an opname or optag (IV/PV).
 * Note that we return the actual entry for speed.
 * Always sv_mortalcopy() if returing it to user code.
 */

static SV *
get_op_bitspec(pTHX_ const char *opname, STRLEN len, int fatal)
{
    SV **svp;
    dMY_CXT;

    if (!len)
	len = strlen(opname);
    svp = hv_fetch(op_named_bits, opname, len, 0);
    if (!svp || !SvOK(*svp)) {
	if (!fatal)
	    return Nullsv;
	if (*opname == ':')
	    croak("Unknown operator tag \"%s\"", opname);
	if (*opname == '!')	/* XXX here later, or elsewhere? */
	    croak("Can't negate operators here (\"%s\")", opname);
	if (isALPHA(*opname))
	    croak("Unknown operator name \"%s\"", opname);
	croak("Unknown operator prefix \"%s\"", opname);
    }
    return *svp;
}



static SV *
new_opset(pTHX_ SV *old_opset)
{
    SV *opset;
    dMY_CXT;

    if (old_opset) {
	verify_opset(aTHX_ old_opset,1);
	opset = newSVsv(old_opset);
    }
    else {
	opset = newSV(opset_len);
	Zero(SvPVX_const(opset), opset_len + 1, char);
	SvCUR_set(opset, opset_len);
	(void)SvPOK_only(opset);
    }
    /* not mortalised here */
    return opset;
}


static int
verify_opset(pTHX_ SV *opset, int fatal)
{
    const char *err = NULL;
    dMY_CXT;

    if      (!SvOK(opset))              err = "undefined";
    else if (!SvPOK(opset))             err = "wrong type";
    else if (SvCUR(opset) != (STRLEN)opset_len) err = "wrong size";
    if (err && fatal) {
	croak("Invalid opset: %s", err);
    }
    return !err;
}


static void
set_opset_bits(pTHX_ char *bitmap, SV *bitspec, int on, const char *opname)
{
    dMY_CXT;

    if (SvIOK(bitspec)) {
	const int myopcode = SvIV(bitspec);
	const int offset = myopcode >> 3;
	const int bit    = myopcode & 0x07;
	if (myopcode >= PL_maxo || myopcode < 0)
	    croak("panic: opcode \"%s\" value %d is invalid", opname, myopcode);
	if (opcode_debug >= 2)
	    warn("set_opset_bits bit %2d (off=%d, bit=%d) %s %s\n",
			myopcode, offset, bit, opname, (on)?"on":"off");
	if (on)
	    bitmap[offset] |= 1 << bit;
	else
	    bitmap[offset] &= ~(1 << bit);
    }
    else if (SvPOK(bitspec) && SvCUR(bitspec) == (STRLEN)opset_len) {

	STRLEN len;
	const char * const specbits = SvPV(bitspec, len);
	if (opcode_debug >= 2)
	    warn("set_opset_bits opset %s %s\n", opname, (on)?"on":"off");
	if (on) 
	    while(len-- > 0) bitmap[len] |=  specbits[len];
	else
	    while(len-- > 0) bitmap[len] &= ~specbits[len];
    }
    else
	croak("panic: invalid bitspec for \"%s\" (type %u)",
		opname, (unsigned)SvTYPE(bitspec));
}


static void
opmask_add(pTHX_ SV *opset)	/* THE ONLY FUNCTION TO EDIT PL_op_mask ITSELF	*/
{
    int i,j;
    char *bitmask;
    STRLEN len;
    int myopcode = 0;
    dMY_CXT;

    verify_opset(aTHX_ opset,1);		/* croaks on bad opset	*/

    if (!PL_op_mask)		/* caller must ensure PL_op_mask exists	*/
	croak("Can't add to uninitialised PL_op_mask");

    /* OPCODES ALREADY MASKED ARE NEVER UNMASKED. See opmask_addlocal()	*/

    bitmask = SvPV(opset, len);
    for (i=0; i < opset_len; i++) {
	const U16 bits = bitmask[i];
	if (!bits) {	/* optimise for sparse masks */
	    myopcode += 8;
	    continue;
	}
	for (j=0; j < 8 && myopcode < PL_maxo; )
	    PL_op_mask[myopcode++] |= bits & (1 << j++);
    }
}

static void
opmask_addlocal(pTHX_ SV *opset, char *op_mask_buf) /* Localise PL_op_mask then opmask_add() */
{
    char *orig_op_mask = PL_op_mask;
    dMY_CXT;

    SAVEVPTR(PL_op_mask);
    /* XXX casting to an ordinary function ptr from a member function ptr
     * is disallowed by Borland
     */
    if (opcode_debug >= 2)
	SAVEDESTRUCTOR((void(*)(void*))Perl_warn,"PL_op_mask restored");
    PL_op_mask = &op_mask_buf[0];
    if (orig_op_mask)
	Copy(orig_op_mask, PL_op_mask, PL_maxo, char);
    else
	Zero(PL_op_mask, PL_maxo, char);
    opmask_add(aTHX_ opset);
}



MODULE = Opcode	PACKAGE = Opcode

PROTOTYPES: ENABLE

BOOT:
{
    MY_CXT_INIT;
    assert(PL_maxo < OP_MASK_BUF_SIZE);
    opset_len = (PL_maxo + 7) / 8;
    if (opcode_debug >= 1)
	warn("opset_len %ld\n", (long)opset_len);
    op_names_init(aTHX);
}

void
_safe_pkg_prep(Package)
    const char *Package
PPCODE:
    HV *hv; 
    ENTER;
   
    hv = gv_stashpv(Package, GV_ADDWARN); /* should exist already	*/

    if (strNE(HvNAME_get(hv),"main")) {
        /* make it think it's in main:: */
	hv_name_set(hv, "main", 4, 0);
        hv_store(hv,"_",1,(SV *)PL_defgv,0);  /* connect _ to global */
        SvREFCNT_inc((SV *)PL_defgv);  /* want to keep _ around! */
    }
    LEAVE;





void
_safe_call_sv(Package, mask, codesv)
    char *	Package
    SV *	mask
    SV *	codesv
PPCODE:
    char op_mask_buf[OP_MASK_BUF_SIZE];
    GV *gv;
    HV *dummy_hv;
    SV *res;

    ENTER;

    opmask_addlocal(aTHX_ mask, op_mask_buf);

    SAVESPTR(PL_endav);
    AVcpSTEAL(PL_endav, newAV()); /* ignore END blocks for now	*/

    SAVESPTR(PL_defstash);		/* save current default stash	*/
    /* the assignment to global defstash changes our sense of 'main'	*/
    HVcpREPLACE(PL_defstash, gv_stashpv(Package, GV_ADDWARN)); /* should exist already	*/

    SAVESPTR(PL_curstash);
    HVcpREPLACE(PL_curstash, PL_defstash);

    /* make "main::" the default stash */
    gv = gv_fetchpv("main::", GV_ADDWARN, SVt_PVHV);
    HVcpREPLACE(PL_curstash, GvHV(gv));

    /* %INC must be clean for use/require in compartment */
    SAVESPTR(PL_includedhv);
    HVcpSTEAL(PL_includedhv, newHV());

    /* Invalidate ISA and method caches */
    ++PL_sub_generation;
    hv_clear(PL_stashcache);

    PUSHMARK(SP);
    res = perl_call_sv(codesv, GIMME|G_EVAL|G_KEEPERR); /* use callers context */
    SPAGAIN; /* for the PUTBACK added by xsubpp */
    XPUSHs(res);
    LEAVE;


int
verify_opset(opset, fatal = 0)
    SV *opset
    int fatal
CODE:
    RETVAL = verify_opset(aTHX_ opset,fatal);
OUTPUT:
    RETVAL

void
invert_opset(opset)
    SV *opset
CODE:
    {
    char *bitmap;
    dMY_CXT;
    STRLEN len = opset_len;

    opset = sv_2mortal(new_opset(aTHX_ opset));	/* verify and clone opset */
    bitmap = SvPVX_mutable(opset);
    while(len-- > 0)
	bitmap[len] = ~bitmap[len];
    /* take care of extra bits beyond PL_maxo in last byte	*/
    if (PL_maxo & 07)
	bitmap[opset_len-1] &= ~(0xFF << (PL_maxo & 0x07));
    }
    ST(0) = opset;


void
opset_to_ops(opset, desc = 0)
    SV *opset
    int	desc
PPCODE:
    {
    STRLEN len;
    int i, j, myopcode;
    const char * const bitmap = SvPV(opset, len);
    char **names = (desc) ? get_op_descs() : get_op_names();
    AV *av;
    dMY_CXT;

    av = newAV();
    mXPUSHs((SV*)av);

    verify_opset(aTHX_ opset,1);
    for (myopcode=0, i=0; i < opset_len; i++) {
	const U16 bits = bitmap[i];
	for (j=0; j < 8 && myopcode < PL_maxo; j++, myopcode++) {
	    if ( bits & (1 << j) )
		av_push(av, newSVpv(names[myopcode], 0));
	}
    }
    }


void
opset(...)
CODE:
    int i;
    SV *bitspec;
    STRLEN len, on;

    SV * const opset = sv_2mortal(new_opset(aTHX_ Nullsv));
    char * const bitmap = SvPVX_mutable(opset);
    for (i = 0; i < items; i++) {
	const char *opname;
	on = 1;
	if (verify_opset(aTHX_ ST(i),0)) {
	    opname = "(opset)";
	    bitspec = ST(i);
	}
	else {
	    opname = SvPV(ST(i), len);
	    if (*opname == '!') { on=0; ++opname;--len; }
	    bitspec = get_op_bitspec(aTHX_ opname, len, 1);
	}
	set_opset_bits(aTHX_ bitmap, bitspec, on, opname);
    }
    ST(0) = opset;


#define PERMITING  (ix == 0 || ix == 1)
#define ONLY_THESE (ix == 0 || ix == 2)

void
permit_only(safe, ...)
    SV *safe
ALIAS:
	permit    = 1
	deny_only = 2
	deny      = 3
CODE:
    int i;
    SV *bitspec, *mask;
    char *bitmap;
    STRLEN len;
    dMY_CXT;

    if (!SvROK(safe) || !SvOBJECT(SvRV(safe)) || SvTYPE(SvRV(safe))!=SVt_PVHV)
	croak("Not a Safe object");
    mask = *hv_fetch((HV*)SvRV(safe), "Mask",4, 1);
    if (ONLY_THESE)	/* *_only = new mask, else edit current	*/
	sv_setsv(mask, sv_2mortal(new_opset(aTHX_ PERMITING ? opset_all : Nullsv)));
    else
	verify_opset(aTHX_ mask,1); /* croaks */
    bitmap = SvPVX_mutable(mask);
    for (i = 1; i < items; i++) {
	const char *opname;
	int on = PERMITING ? 0 : 1;		/* deny = mask bit on	*/
	if (verify_opset(aTHX_ ST(i),0)) {	/* it's a valid mask	*/
	    opname = "(opset)";
	    bitspec = ST(i);
	}
	else {				/* it's an opname/optag	*/
	    opname = SvPV(ST(i), len);
	    /* invert if op has ! prefix (only one allowed)	*/
	    if (*opname == '!') { on = !on; ++opname; --len; }
	    bitspec = get_op_bitspec(aTHX_ opname, len, 1); /* croaks */
	}
	set_opset_bits(aTHX_ bitmap, bitspec, on, opname);
    }
    ST(0) = &PL_sv_yes;



void
opdesc(op)
    SV* op;    
PPCODE:
    int i;
    STRLEN len;
    char **op_desc = get_op_descs(); 
    dMY_CXT;

    /* copy args to a scratch area since we may push output values onto	*/
    /* the stack faster than we read values off it if masks are used.	*/
    const char * const opname = SvPV(op, len);
    SV *bitspec = get_op_bitspec(aTHX_ opname, len, 1);
    if (SvIOK(bitspec)) {
        const int myopcode = SvIV(bitspec);
        if (myopcode < 0 || myopcode >= PL_maxo)
            croak("panic: opcode %d (%s) out of range",myopcode,opname);
        mXPUSHs(newSVpv(op_desc[myopcode], 0));
    }
    else
        croak("panic: invalid bitspec for \"%s\" (type %u)",
              opname, (unsigned)SvTYPE(bitspec));


void
define_optag(optagsv, mask)
    SV *optagsv
    SV *mask
CODE:
    STRLEN len;
    const char *optag = SvPV(optagsv, len);

    put_op_bitspec(aTHX_ optag, len, mask); /* croaks */
    ST(0) = &PL_sv_yes;


void
empty_opset()
CODE:
    ST(0) = sv_2mortal(new_opset(aTHX_ Nullsv));

void
full_opset()
CODE:
    dMY_CXT;
    ST(0) = sv_2mortal(new_opset(aTHX_ opset_all));

void
opmask_add(opset)
    SV *opset
PREINIT:
    if (!PL_op_mask)
	Newxz(PL_op_mask, PL_maxo, char);
CODE:
    opmask_add(aTHX_ opset);

void
opcodes()
PPCODE:
    if (GIMME == G_ARRAY) {
	croak("opcodes in list context not yet implemented"); /* XXX */
    }
    else {
	XPUSHs(sv_2mortal(newSViv(PL_maxo)));
    }

void
opmask()
CODE:
    ST(0) = sv_2mortal(new_opset(aTHX_ Nullsv));
    if (PL_op_mask) {
	char * const bitmap = SvPVX_mutable(ST(0));
	int myopcode;
	for(myopcode=0; myopcode < PL_maxo; ++myopcode) {
	    if (PL_op_mask[myopcode])
		bitmap[myopcode >> 3] |= 1 << (myopcode & 0x07);
	}
    }

