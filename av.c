/*    av.c
 *
 *    Copyright (C) 1991, 1992, 1993, 1994, 1995, 1996, 1997, 1998, 1999,
 *    2000, 2001, 2002, 2003, 2004, 2005, 2006, by Larry Wall and others
 *
 *    You may distribute under the terms of either the GNU General Public
 *    License or the Artistic License, as specified in the README file.
 *
 */

/*
 * "...for the Entwives desired order, and plenty, and peace (by which they
 * meant that things should remain where they had set them)." --Treebeard
 */

/*
=head1 Array Manipulation Functions
*/

#include "EXTERN.h"
#define PERL_IN_AV_C
#include "perl.h"

void
Perl_av_reify(pTHX_ AV *av)
{
    dVAR;
    I32 key;

    PERL_ARGS_ASSERT_AV_REIFY;
    assert(SvTYPE(av) == SVt_PVAV);

    if (AvREAL(av))
	return;
    key = AvMAX(av) + 1;
    while (key > AvFILLp(av) + 1)
	AvARRAY(av)[--key] = &PL_sv_undef;
    while (key) {
	SV * const sv = AvARRAY(av)[--key];
	assert(sv);
	if (sv != &PL_sv_undef)
	    SvREFCNT_inc_void_NN(sv);
    }
    key = AvARRAY(av) - AvALLOC(av);
    while (key)
	AvALLOC(av)[--key] = &PL_sv_undef;
    AvREIFY_off(av);
    AvREAL_on(av);
}

/*
=for apidoc av_extend

Pre-extend an array.  The C<key> is the index to which the array should be
extended.

=cut
*/

void
Perl_av_extend(pTHX_ AV *av, I32 key)
{
    dVAR;

    PERL_ARGS_ASSERT_AV_EXTEND;
    assert(SvTYPE(av) == SVt_PVAV);

    if (key > AvMAX(av)) {
	SV** ary;
	I32 tmp;
	I32 newmax;

	if (AvALLOC(av) != AvARRAY(av)) {
	    ary = AvALLOC(av) + AvFILLp(av) + 1;
	    tmp = AvARRAY(av) - AvALLOC(av);
	    Move(AvARRAY(av), AvALLOC(av), AvFILLp(av)+1, SV*);
	    AvMAX(av) += tmp;
	    AvARRAY(av) = AvALLOC(av);
	    if (AvREAL(av)) {
		while (tmp)
		    ary[--tmp] = &PL_sv_undef;
	    }
	    if (key > AvMAX(av) - 10) {
		newmax = key + AvMAX(av);
		goto resize;
	    }
	}
	else {
#ifdef PERL_MALLOC_WRAP
	    static const char oom_array_extend[] =
	      "Out of memory during array extend"; /* Duplicated in pp_hot.c */
#endif

	    if (AvALLOC(av)) {
#if !defined(STRANGE_MALLOC) && !defined(MYMALLOC)
		MEM_SIZE bytes;
		IV itmp;
#endif

#ifdef Perl_safesysmalloc_size
		/* Whilst it would be quite possible to move this logic around
		   (as I did in the SV code), so as to set AvMAX(av) early,
		   based on calling Perl_safesysmalloc_size() immediately after
		   allocation, I'm not convinced that it is a great idea here.
		   In an array we have to loop round setting everything to
		   &PL_sv_undef, which means writing to memory, potentially lots
		   of it, whereas for the SV buffer case we don't touch the
		   "bonus" memory. So there there is no cost in telling the
		   world about it, whereas here we have to do work before we can
		   tell the world about it, and that work involves writing to
		   memory that might never be read. So, I feel, better to keep
		   the current lazy system of only writing to it if our caller
		   has a need for more space. NWC  */
		newmax = Perl_safesysmalloc_size((void*)AvALLOC(av)) /
		    sizeof(SV*) - 1;

		if (key <= newmax) 
		    goto resized;
#endif 
		newmax = key + AvMAX(av) / 5;
	      resize:
		MEM_WRAP_CHECK_1(newmax+1, SV*, oom_array_extend);
#if defined(STRANGE_MALLOC) || defined(MYMALLOC)
		Renew(AvALLOC(av),newmax+1, SV*);
#else
		bytes = (newmax + 1) * sizeof(SV*);
#define MALLOC_OVERHEAD 16
		itmp = MALLOC_OVERHEAD;
		while ((MEM_SIZE)(itmp - MALLOC_OVERHEAD) < bytes)
		    itmp += itmp;
		itmp -= MALLOC_OVERHEAD;
		itmp /= sizeof(SV*);
		assert(itmp > newmax);
		newmax = itmp - 1;
		assert(newmax >= AvMAX(av));
		Newx(ary, newmax+1, SV*);
		Copy(AvALLOC(av), ary, AvMAX(av)+1, SV*);
		if (AvMAX(av) > 64)
		    offer_nice_chunk(AvALLOC(av), (AvMAX(av)+1) * sizeof(SV*));
		else
		    Safefree(AvALLOC(av));
		AvALLOC(av) = ary;
#endif
#ifdef Perl_safesysmalloc_size
	      resized:
#endif
		ary = AvALLOC(av) + AvMAX(av) + 1;
		tmp = newmax - AvMAX(av);
		if (av == PL_curstack) {	/* Oops, grew stack (via av_store()?) */
		    PL_stack_sp = AvALLOC(av) + (PL_stack_sp - PL_stack_base);
		    PL_stack_base = AvALLOC(av);
		    PL_stack_max = PL_stack_base + newmax;
		}
	    }
	    else {
		newmax = key < 3 ? 3 : key;
		MEM_WRAP_CHECK_1(newmax+1, SV*, oom_array_extend);
		Newx(AvALLOC(av), newmax+1, SV*);
		ary = AvALLOC(av) + 1;
		tmp = newmax;
		AvALLOC(av)[0] = &PL_sv_undef;	/* For the stacks */
	    }
	    if (AvREAL(av)) {
		while (tmp)
		    ary[--tmp] = &PL_sv_undef;
	    }
	    
	    AvARRAY(av) = AvALLOC(av);
	    AvMAX(av) = newmax;
	}
    }
}

/*
=for apidoc av_fetch

Returns the SV at the specified index in the array.  The C<key> is the
index.  If C<lval> is set then the fetch will be part of a store.  Check
that the return value is non-null before dereferencing it to a C<SV*>.

See L<perlguts/"Understanding the Magic of Tied Hashes and Arrays"> for
more information on how to use this function on tied arrays. 

=cut
*/

SV**
Perl_av_fetch(pTHX_ register AV *av, I32 key, I32 lval)
{
    dVAR;

    PERL_ARGS_ASSERT_AV_FETCH;
    assert(SvTYPE(av) == SVt_PVAV);

    if (key < 0) {
	key += AvFILL(av) + 1;
	if (key < 0)
	    return NULL;
    }

    if (key > AvFILLp(av)) {
	if (!lval)
	    return NULL;
	return av_store(av,key,newSV(0));
    }
    if (lval && AvARRAY(av)[key] == &PL_sv_undef) {
    emptyness:
	return av_store(av,key,newSV(0));
    }
    else if (AvREIFY(av)
	     && (!AvARRAY(av)[key]	/* eg. @_ could have freed elts */
		 || SvIS_FREED(AvARRAY(av)[key]))) {
	AvARRAY(av)[key] = &PL_sv_undef;	/* 1/2 reify */
	goto emptyness;
    }
    return &AvARRAY(av)[key];
}

/*
=for apidoc av_store

Stores an SV in an array.  The array index is specified as C<key>.  The
return value will be NULL if the operation failed or if the value did not
need to be actually stored within the array (as in the case of tied
arrays). Otherwise it can be dereferenced to get the original C<SV*>.  Note
that the caller is responsible for suitably incrementing the reference
count of C<val> before the call, and decrementing it if the function
returned NULL.

See L<perlguts/"Understanding the Magic of Tied Hashes and Arrays"> for
more information on how to use this function on tied arrays.

=cut
*/

SV**
Perl_av_store(pTHX_ register AV *av, I32 key, SV *val)
{
    dVAR;
    SV** ary;

    PERL_ARGS_ASSERT_AV_STORE;
    assert(SvTYPE(av) == SVt_PVAV);

    /* S_regclass relies on being able to pass in a NULL sv
       (unicode_alternate may be NULL).
    */

    if (!val)
	val = &PL_sv_undef;

    if (key < 0) {
	key += AvFILL(av) + 1;
	if (key < 0)
	    return NULL;
    }

    if (SvREADONLY(av) && key >= AvFILL(av))
	Perl_croak(aTHX_ PL_no_modify);

    if (!AvREAL(av) && AvREIFY(av))
	av_reify(av);
    if (key > AvMAX(av))
	av_extend(av,key);
    ary = AvARRAY(av);
    if (AvFILLp(av) < key) {
	if (!AvREAL(av)) {
	    if (av == PL_curstack && key > PL_stack_sp - PL_stack_base)
		PL_stack_sp = PL_stack_base + key;	/* XPUSH in disguise */
	    do {
		ary[++AvFILLp(av)] = &PL_sv_undef;
	    } while (AvFILLp(av) < key);
	}
	AvFILLp(av) = key;
    }
    else if (AvREAL(av))
	SvREFCNT_dec(ary[key]);
    ary[key] = val;
    if (SvSMAGICAL(av)) {
	const MAGIC* const mg = SvMAGIC(av);
	if (val != &PL_sv_undef) {
	    SV* ref = newRV_inc(mg->mg_obj);
	    sv_rvweaken(ref);
	    sv_magic(val, ref, toLOWER(mg->mg_type), 0, key);
	    SvREFCNT_dec(ref);
	}
	if (PL_delaymagic && mg->mg_type == PERL_MAGIC_isa)
	    PL_delaymagic |= DM_ARRAY;
	else
	   mg_set((SV*)av);
    }
    return &ary[key];
}

/*
=for apidoc av_make

Creates a new AV and populates it with a list of SVs.  The SVs are copied
into the array, so they may be freed after the call to av_make.  The new AV
will have a reference count of 1.

=cut
*/

AV *
Perl_av_make(pTHX_ register I32 size, register SV **strp)
{
    register AV * const av = (AV*)newSV_type(SVt_PVAV);
    /* sv_upgrade does AvREAL_only()  */
    PERL_ARGS_ASSERT_AV_MAKE;
    assert(SvTYPE(av) == SVt_PVAV);

    if (size) {		/* "defined" was returning undef for size==0 anyway. */
        register SV** ary;
        register I32 i;
	Newx(ary,size,SV*);
	AvALLOC(av) = ary;
	AvARRAY(av) = ary;
	AvFILLp(av) = AvMAX(av) = size - 1;
	for (i = 0; i < size; i++) {
	    assert (*strp);
	    ary[i] = newSV(0);
	    sv_setsv(ary[i], *strp);
	    strp++;
	}
    }
    return av;
}

/*
=for apidoc av_clear

Clears an array, making it empty.  Does not free the memory used by the
array itself.

=cut
*/

void
Perl_av_clear(pTHX_ register AV *av)
{
    dVAR;
    I32 extra;

    PERL_ARGS_ASSERT_AV_CLEAR;
    assert(SvTYPE(av) == SVt_PVAV);

#ifdef DEBUGGING
    if (SvREFCNT(av) == 0 && ckWARN_d(WARN_DEBUGGING)) {
	Perl_warner(aTHX_ packWARN(WARN_DEBUGGING), "Attempt to clear deleted array");
    }
#endif

    if (SvREADONLY(av))
	Perl_croak(aTHX_ PL_no_modify);

    /* Give any tie a chance to cleanup first */
    if (SvRMAGICAL(av)) {
	const MAGIC* const mg = SvMAGIC(av);
	if (PL_delaymagic && mg && mg->mg_type == PERL_MAGIC_isa)
	    PL_delaymagic |= DM_ARRAY;
        else
	    mg_clear((SV*)av); 
    }

    if (AvMAX(av) < 0)
	return;

    if (AvREAL(av)) {
	SV** const ary = AvARRAY(av);
	I32 index = AvFILLp(av) + 1;
	while (index) {
	    SV * const sv = ary[--index];
	    /* undef the slot before freeing the value, because a
	     * destructor might try to modify this array */
	    ary[index] = &PL_sv_undef;
	    SvREFCNT_dec(sv);
	}
    }
    extra = AvARRAY(av) - AvALLOC(av);
    if (extra) {
	AvMAX(av) += extra;
	AvARRAY(av) = AvALLOC(av);
    }
    AvFILLp(av) = -1;

}

/*
=for apidoc av_undef

Undefines the array.  Frees the memory used by the array itself.

=cut
*/

void
Perl_av_undef(pTHX_ register AV *av)
{
    PERL_ARGS_ASSERT_AV_UNDEF;
    assert(SvTYPE(av) == SVt_PVAV);

    if (AvREAL(av)) {
	register I32 key = AvFILLp(av) + 1;
	while (key)
	    SvREFCNT_dec(AvARRAY(av)[--key]);
    }

    Safefree(AvALLOC(av));
    AvALLOC(av) = NULL;
    AvARRAY(av) = NULL;
    AvMAX(av) = AvFILLp(av) = -1;

    if(SvRMAGICAL(av)) mg_clear((SV*)av);
}

void
Perl_av_tmprefcnt(pTHX_ AV *av)
{
    PERL_ARGS_ASSERT_AV_TMPREFCNT;

    if (AvREAL(av)) {
	register I32 key = AvFILLp(av) + 1;
	while (key)
	    SvTMPREFCNT_inc(AvARRAY(av)[--key]);
    }
}

/*

=for apidoc av_create_and_push

Push an SV onto the end of the array, creating the array if necessary.
A small internal helper function to remove a commonly duplicated idiom.

=cut
*/

void
Perl_av_create_and_push(pTHX_ AV **const avp, SV *const val)
{
    PERL_ARGS_ASSERT_AV_CREATE_AND_PUSH;

    if (!*avp)
	*avp = newAV();
    av_push(*avp, val);
}

/*
=for apidoc av_push

Pushes an SV onto the end of the array.  The array will grow automatically
to accommodate the addition.

=cut
*/

void
Perl_av_push(pTHX_ register AV *av, SV *val)
{             
    dVAR;

    PERL_ARGS_ASSERT_AV_PUSH;
    assert(SvTYPE(av) == SVt_PVAV);

    if (SvREADONLY(av))
	Perl_croak(aTHX_ PL_no_modify);

    av_store(av,AvFILLp(av)+1,val);
}

/*
=for apidoc av_pop

Pops an SV off the end of the array.  Returns C<&PL_sv_undef> if the array
is empty.

=cut
*/

SV *
Perl_av_pop(pTHX_ register AV *av)
{
    dVAR;
    SV *retval;

    PERL_ARGS_ASSERT_AV_POP;
    assert(SvTYPE(av) == SVt_PVAV);

    if (SvREADONLY(av))
	Perl_croak(aTHX_ PL_no_modify);
    if (AvFILL(av) < 0)
	return &PL_sv_undef;
    retval = AvARRAY(av)[AvFILLp(av)];
    AvARRAY(av)[AvFILLp(av)--] = &PL_sv_undef;
    if (SvSMAGICAL(av))
	mg_set((SV*)av);
    return retval;
}

/*

=for apidoc av_create_and_unshift_one

Unshifts an SV onto the beginning of the array, creating the array if
necessary.
A small internal helper function to remove a commonly duplicated idiom.

=cut
*/

SV **
Perl_av_create_and_unshift_one(pTHX_ AV **const avp, SV *const val)
{
    PERL_ARGS_ASSERT_AV_CREATE_AND_UNSHIFT_ONE;

    if (!*avp)
	*avp = newAV();
    av_unshift(*avp, 1);
    return av_store(*avp, 0, val);
}

/*
=for apidoc av_unshift

Unshift the given number of C<undef> values onto the beginning of the
array.  The array will grow automatically to accommodate the addition.  You
must then use C<av_store> to assign values to these new elements.

=cut
*/

void
Perl_av_unshift(pTHX_ register AV *av, register I32 num)
{
    dVAR;
    register I32 i;

    PERL_ARGS_ASSERT_AV_UNSHIFT;
    assert(SvTYPE(av) == SVt_PVAV);

    if (SvREADONLY(av))
	Perl_croak(aTHX_ PL_no_modify);

    if (num <= 0)
      return;
    if (!AvREAL(av) && AvREIFY(av))
	av_reify(av);
    i = AvARRAY(av) - AvALLOC(av);
    if (i) {
	if (i > num)
	    i = num;
	num -= i;
    
	AvMAX(av) += i;
	AvFILLp(av) += i;
	AvARRAY(av) = AvARRAY(av) - i;
    }
    if (num) {
	register SV **ary;
	const I32 i = AvFILLp(av);
	/* Create extra elements */
	const I32 slide = i > 0 ? i : 0;
	num += slide;
	av_extend(av, i + num);
	AvFILLp(av) += num;
	ary = AvARRAY(av);
	Move(ary, ary + num, i + 1, SV*);
	do {
	    ary[--num] = &PL_sv_undef;
	} while (num);
	/* Make extra elements into a buffer */
	AvMAX(av) -= slide;
	AvFILLp(av) -= slide;
	AvARRAY(av) = AvARRAY(av) + slide;
    }
}

/*
=for apidoc av_shift

Shifts an SV off the beginning of the array.

=cut
*/

SV *
Perl_av_shift(pTHX_ register AV *av)
{
    dVAR;
    SV *retval;

    PERL_ARGS_ASSERT_AV_SHIFT;
    assert(SvTYPE(av) == SVt_PVAV);

    if (SvREADONLY(av))
	Perl_croak(aTHX_ PL_no_modify);
    if (AvFILL(av) < 0)
      return &PL_sv_undef;
    retval = *AvARRAY(av);
    if (AvREAL(av))
	*AvARRAY(av) = &PL_sv_undef;
    AvARRAY(av) = AvARRAY(av) + 1;
    AvMAX(av)--;
    AvFILLp(av)--;
    if (SvSMAGICAL(av))
	mg_set((SV*)av);
    return retval;
}

/*
=for apidoc av_len

Returns the highest index in the array.  The number of elements in the
array is C<av_len(av) + 1>.  Returns -1 if the array is empty.

=cut
*/

I32
Perl_av_len(pTHX_ register const AV *av)
{
    PERL_ARGS_ASSERT_AV_LEN;
    assert(SvTYPE(av) == SVt_PVAV);

    return AvFILL(av);
}

/*
=for apidoc av_fill

Set the highest index in the array to the given number, equivalent to
Perl's C<$#array = $fill;>.

The number of elements in the an array will be C<fill + 1> after
av_fill() returns.  If the array was previously shorter then the
additional elements appended are set to C<PL_sv_undef>.  If the array
was longer, then the excess elements are freed.  C<av_fill(av, -1)> is
the same as C<av_clear(av)>.

=cut
*/
void
Perl_av_fill(pTHX_ register AV *av, I32 fill)
{
    dVAR;

    PERL_ARGS_ASSERT_AV_FILL;
    assert(SvTYPE(av) == SVt_PVAV);

    if (fill < 0)
	fill = -1;
    if (fill <= AvMAX(av)) {
	I32 key = AvFILLp(av);
	SV** const ary = AvARRAY(av);

	if (AvREAL(av)) {
	    while (key > fill) {
		SvREFCNT_dec(ary[key]);
		ary[key--] = &PL_sv_undef;
	    }
	}
	else {
	    while (key < fill)
		ary[++key] = &PL_sv_undef;
	}
	    
	AvFILLp(av) = fill;
	if (SvSMAGICAL(av))
	    mg_set((SV*)av);
    }
    else
	(void)av_store(av,fill,&PL_sv_undef);
}

/*
=for apidoc av_delete

Deletes the element indexed by C<key> from the array.  Returns the
deleted element. If C<flags> equals C<G_DISCARD>, the element is freed
and null is returned.

=cut
*/
SV *
Perl_av_delete(pTHX_ AV *av, I32 key, I32 flags)
{
    dVAR;
    SV *sv;

    PERL_ARGS_ASSERT_AV_DELETE;
    assert(SvTYPE(av) == SVt_PVAV);

    if (SvREADONLY(av))
	Perl_croak(aTHX_ PL_no_modify);

    if (key < 0) {
	key += AvFILL(av) + 1;
	if (key < 0)
	    return NULL;
    }

    if (key > AvFILLp(av))
	return NULL;
    else {
	if (!AvREAL(av) && AvREIFY(av))
	    av_reify(av);
	sv = AvARRAY(av)[key];
	if (key == AvFILLp(av)) {
	    AvARRAY(av)[key] = &PL_sv_undef;
	    do {
		AvFILLp(av)--;
	    } while (--key >= 0 && AvARRAY(av)[key] == &PL_sv_undef);
	}
	else
	    AvARRAY(av)[key] = &PL_sv_undef;
	if (SvSMAGICAL(av))
	    mg_set((SV*)av);
    }
    if (flags & G_DISCARD) {
	SvREFCNT_dec(sv);
	sv = NULL;
    }
    else if (AvREAL(av))
	sv = sv_2mortal(sv);
    return sv;
}

/*
=for apidoc av_exists

Returns true if the element indexed by C<key> has been initialized.

This relies on the fact that uninitialized array elements are set to
C<&PL_sv_undef>.

=cut
*/
bool
Perl_av_exists(pTHX_ AV *av, I32 key)
{
    dVAR;
    PERL_ARGS_ASSERT_AV_EXISTS;
    assert(SvTYPE(av) == SVt_PVAV);

    if (key < 0) {
	key += AvFILL(av) + 1;
	if (key < 0)
	    return FALSE;
    }

    if (key <= AvFILLp(av) && AvARRAY(av)[key] != &PL_sv_undef
	&& AvARRAY(av)[key])
    {
	return TRUE;
    }
    else
	return FALSE;
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
