/*    vms.c
 *
 *    VMS-specific routines for perl5
 *
 *    Copyright (C) 1993, 1994, 1995, 1996, 1997, 1998, 1999, 2000, 2001,
 *    2002, 2003, 2004, 2005, 2006, 2007 by Charles Bailey and others.
 *
 *    You may distribute under the terms of either the GNU General Public
 *    License or the Artistic License, as specified in the README file.
 *
 *    Please see Changes*.* or the Perl Repository Browser for revision history.
 */

/*
 *               Yet small as was their hunted band
 *               still fell and fearless was each hand,
 *               and strong deeds they wrought yet oft,
 *               and loved the woods, whose ways more soft
 *               them seemed than thralls of that black throne
 *               to live and languish in halls of stone.
 *
 *                           The Lay of Leithian, 135-40
 */
 
#include <acedef.h>
#include <acldef.h>
#include <armdef.h>
#include <atrdef.h>
#include <chpdef.h>
#include <clidef.h>
#include <climsgdef.h>
#include <dcdef.h>
#include <descrip.h>
#include <devdef.h>
#include <dvidef.h>
#include <fibdef.h>
#include <float.h>
#include <fscndef.h>
#include <iodef.h>
#include <jpidef.h>
#include <kgbdef.h>
#include <libclidef.h>
#include <libdef.h>
#include <lib$routines.h>
#include <lnmdef.h>
#include <msgdef.h>
#include <ossdef.h>
#if __CRTL_VER >= 70301000 && !defined(__VAX)
#include <ppropdef.h>
#endif
#include <prvdef.h>
#include <psldef.h>
#include <rms.h>
#include <shrdef.h>
#include <ssdef.h>
#include <starlet.h>
#include <strdef.h>
#include <str$routines.h>
#include <syidef.h>
#include <uaidef.h>
#include <uicdef.h>
#include <stsdef.h>
#include <rmsdef.h>
#include <smgdef.h>
#if __CRTL_VER >= 70000000 /* FIXME to earliest version */
#include <efndef.h>
#define NO_EFN EFN$C_ENF
#else
#define NO_EFN 0;
#endif

#if  __CRTL_VER < 70301000 && __CRTL_VER >= 70300000
int   decc$feature_get_index(const char *name);
char* decc$feature_get_name(int index);
int   decc$feature_get_value(int index, int mode);
int   decc$feature_set_value(int index, int mode, int value);
#else
#include <unixlib.h>
#endif

#pragma member_alignment save
#pragma nomember_alignment longword
struct item_list_3 {
	unsigned short len;
	unsigned short code;
	void * bufadr;
	unsigned short * retadr;
};
#pragma member_alignment restore

/* More specific prototype than in starlet_c.h makes programming errors
   more visible.
 */
#ifdef sys$getdviw
#undef sys$getdviw
int sys$getdviw
       (unsigned long efn,
	unsigned short chan,
	const struct dsc$descriptor_s * devnam,
	const struct item_list_3 * itmlst,
	void * iosb,
	void * (astadr)(unsigned long),
	void * astprm,
	void * nullarg);
#endif

#ifdef sys$get_security
#undef sys$get_security
int sys$get_security
       (const struct dsc$descriptor_s * clsnam,
	const struct dsc$descriptor_s * objnam,
	const unsigned int *objhan,
	unsigned int flags,
	const struct item_list_3 * itmlst,
	unsigned int * contxt,
	const unsigned int * acmode);
#endif

#ifdef sys$set_security
#undef sys$set_security
int sys$set_security
       (const struct dsc$descriptor_s * clsnam,
	const struct dsc$descriptor_s * objnam,
	const unsigned int *objhan,
	unsigned int flags,
	const struct item_list_3 * itmlst,
	unsigned int * contxt,
	const unsigned int * acmode);
#endif

#ifdef lib$find_image_symbol
#undef lib$find_image_symbol
int lib$find_image_symbol
       (const struct dsc$descriptor_s * imgname,
	const struct dsc$descriptor_s * symname,
	void * symval,
	const struct dsc$descriptor_s * defspec,
	unsigned long flag);
#endif

#ifdef lib$rename_file
#undef lib$rename_file
int lib$rename_file
       (const struct dsc$descriptor_s * old_file_dsc,
	const struct dsc$descriptor_s * new_file_dsc,
	const struct dsc$descriptor_s * default_file_dsc,
	const struct dsc$descriptor_s * related_file_dsc,
	const unsigned long * flags,
	void * (success)(const struct dsc$descriptor_s * old_dsc,
			 const struct dsc$descriptor_s * new_dsc,
			 const void *),
	void * (error)(const struct dsc$descriptor_s * old_dsc,
		       const struct dsc$descriptor_s * new_dsc,
		       const int * rms_sts,
		       const int * rms_stv,
		       const int * error_src,
		       const void * usr_arg),
	int (confirm)(const struct dsc$descriptor_s * old_dsc,
		      const struct dsc$descriptor_s * new_dsc,
		      const void * old_fab,
		      const void * usr_arg),
	void * user_arg,
	struct dsc$descriptor_s * old_result_name_dsc,
	struct dsc$descriptor_s * new_result_name_dsc,
	unsigned long * file_scan_context);
#endif

#if __CRTL_VER >= 70300000 && !defined(__VAX)

static int set_feature_default(const char *name, int value)
{
    int status;
    int index;

    index = decc$feature_get_index(name);

    status = decc$feature_set_value(index, 1, value);
    if (index == -1 || (status == -1)) {
      return -1;
    }

    status = decc$feature_get_value(index, 1);
    if (status != value) {
      return -1;
    }

return 0;
}
#endif

/* Older versions of ssdef.h don't have these */
#ifndef SS$_INVFILFOROP
#  define SS$_INVFILFOROP 3930
#endif
#ifndef SS$_NOSUCHOBJECT
#  define SS$_NOSUCHOBJECT 2696
#endif

/* We implement I/O here, so we will be mixing PerlIO and stdio calls. */
#define PERLIO_NOT_STDIO 0 

/* Don't replace system definitions of vfork, getenv, lstat, and stat, 
 * code below needs to get to the underlying CRTL routines. */
#define DONT_MASK_RTL_CALLS
#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"
/* Anticipating future expansion in lexical warnings . . . */
#ifndef WARN_INTERNAL
#  define WARN_INTERNAL WARN_MISC
#endif

#ifdef VMS_LONGNAME_SUPPORT
#include <libfildef.h>
#endif

#if defined(__VMS_VER) && __VMS_VER >= 70000000 && __DECC_VER >= 50200000
#  define RTL_USES_UTC 1
#endif

/* Routine to create a decterm for use with the Perl debugger */
/* No headers, this information was found in the Programming Concepts Manual */

static int (*decw_term_port)
   (const struct dsc$descriptor_s * display,
    const struct dsc$descriptor_s * setup_file,
    const struct dsc$descriptor_s * customization,
    struct dsc$descriptor_s * result_device_name,
    unsigned short * result_device_name_length,
    void * controller,
    void * char_buffer,
    void * char_change_buffer) = 0;

/* gcc's header files don't #define direct access macros
 * corresponding to VAXC's variant structs */
#ifdef __GNUC__
#  define uic$v_format uic$r_uic_form.uic$v_format
#  define uic$v_group uic$r_uic_form.uic$v_group
#  define uic$v_member uic$r_uic_form.uic$v_member
#  define prv$v_bypass  prv$r_prvdef_bits0.prv$v_bypass
#  define prv$v_grpprv  prv$r_prvdef_bits0.prv$v_grpprv
#  define prv$v_readall prv$r_prvdef_bits0.prv$v_readall
#  define prv$v_sysprv  prv$r_prvdef_bits0.prv$v_sysprv
#endif

#if defined(NEED_AN_H_ERRNO)
dEXT int h_errno;
#endif

#ifdef __DECC
#pragma message disable pragma
#pragma member_alignment save
#pragma nomember_alignment longword
#pragma message save
#pragma message disable misalgndmem
#endif
struct itmlst_3 {
  unsigned short int buflen;
  unsigned short int itmcode;
  void *bufadr;
  unsigned short int *retlen;
};

struct filescan_itmlst_2 {
    unsigned short length;
    unsigned short itmcode;
    char * component;
};

struct vs_str_st {
    unsigned short length;
    char str[65536];
};

#ifdef __DECC
#pragma message restore
#pragma member_alignment restore
#endif

#define do_fileify_dirspec(a,b,c,d)	mp_do_fileify_dirspec(aTHX_ a,b,c,d)
#define do_pathify_dirspec(a,b,c,d)	mp_do_pathify_dirspec(aTHX_ a,b,c,d)
#define do_tovmsspec(a,b,c,d)		mp_do_tovmsspec(aTHX_ a,b,c,0,d)
#define do_tovmspath(a,b,c,d)		mp_do_tovmspath(aTHX_ a,b,c,d)
#define do_rmsexpand(a,b,c,d,e,f,g)	mp_do_rmsexpand(aTHX_ a,b,c,d,e,f,g)
#define do_vms_realpath(a,b,c)		mp_do_vms_realpath(aTHX_ a,b,c)
#define do_vms_realname(a,b,c)		mp_do_vms_realname(aTHX_ a,b,c)
#define do_tounixspec(a,b,c,d)		mp_do_tounixspec(aTHX_ a,b,c,d)
#define do_tounixpath(a,b,c,d)		mp_do_tounixpath(aTHX_ a,b,c,d)
#define do_vms_case_tolerant(a)		mp_do_vms_case_tolerant(a)
#define expand_wild_cards(a,b,c,d)	mp_expand_wild_cards(aTHX_ a,b,c,d)
#define getredirection(a,b)		mp_getredirection(aTHX_ a,b)

static char *mp_do_tovmspath(pTHX_ const char *path, char *buf, int ts, int *);
static char *mp_do_tounixpath(pTHX_ const char *path, char *buf, int ts, int *);
static char *mp_do_tounixspec(pTHX_ const char *, char *, int, int *);
static char *mp_do_pathify_dirspec(pTHX_ const char *dir,char *buf, int ts, int *);

/* see system service docs for $TRNLNM -- NOT the same as LNM$_MAX_INDEX */
#define PERL_LNM_MAX_ALLOWED_INDEX 127

/* OpenVMS User's Guide says at least 9 iterative translations will be performed,
 * depending on the facility.  SHOW LOGICAL does 10, so we'll imitate that for
 * the Perl facility.
 */
#define PERL_LNM_MAX_ITER 10

  /* New values with 7.3-2*/ /* why does max DCL have 4 byte subtracted from it? */
#if __CRTL_VER >= 70302000 && !defined(__VAX)
#define MAX_DCL_SYMBOL		(8192)
#define MAX_DCL_LINE_LENGTH	(4096 - 4)
#else
#define MAX_DCL_SYMBOL		(1024)
#define MAX_DCL_LINE_LENGTH	(1024 - 4)
#endif

static char *__mystrtolower(char *str)
{
  if (str) for (; *str; ++str) *str= tolower(*str);
  return str;
}

static struct dsc$descriptor_s fildevdsc = 
  { 12, DSC$K_DTYPE_T, DSC$K_CLASS_S, "LNM$FILE_DEV" };
static struct dsc$descriptor_s crtlenvdsc = 
  { 8, DSC$K_DTYPE_T, DSC$K_CLASS_S, "CRTL_ENV" };
static struct dsc$descriptor_s *fildev[] = { &fildevdsc, NULL };
static struct dsc$descriptor_s *defenv[] = { &fildevdsc, &crtlenvdsc, NULL };
static struct dsc$descriptor_s **env_tables = defenv;
static bool will_taint = FALSE;  /* tainting active, but no PL_curinterp yet */

/* True if we shouldn't treat barewords as logicals during directory */
/* munching */ 
static int no_translate_barewords;

#ifndef RTL_USES_UTC
static int tz_updated = 1;
#endif

/* DECC Features that may need to affect how Perl interprets
 * displays filename information
 */
static int decc_disable_to_vms_logname_translation = 1;
static int decc_disable_posix_root = 1;
int decc_efs_case_preserve = 0;
static int decc_efs_charset = 0;
static int decc_filename_unix_no_version = 0;
static int decc_filename_unix_only = 0;
int decc_filename_unix_report = 0;
int decc_posix_compliant_pathnames = 0;
int decc_readdir_dropdotnotype = 0;
static int vms_process_case_tolerant = 1;
int vms_vtf7_filenames = 0;
int gnv_unix_shell = 0;
static int vms_unlink_all_versions = 0;

/* bug workarounds if needed */
int decc_bug_readdir_efs1 = 0;
int decc_bug_devnull = 1;
int decc_bug_fgetname = 0;
int decc_dir_barename = 0;

static int vms_debug_on_exception = 0;

/* Is this a UNIX file specification?
 *   No longer a simple check with EFS file specs
 *   For now, not a full check, but need to
 *   handle POSIX ^UP^ specifications
 *   Fixing to handle ^/ cases would require
 *   changes to many other conversion routines.
 */

static int is_unix_filespec(const char *path)
{
int ret_val;
const char * pch1;

    ret_val = 0;
    if (strncmp(path,"\"^UP^",5) != 0) {
	pch1 = strchr(path, '/');
	if (pch1 != NULL)
	    ret_val = 1;
	else {

	    /* If the user wants UNIX files, "." needs to be treated as in UNIX */
	    if (decc_filename_unix_report || decc_filename_unix_only) {
	    if (strcmp(path,".") == 0)
		ret_val = 1;
	    }
	}
    }
    return ret_val;
}

/* This routine converts a UCS-2 character to be VTF-7 encoded.
 */

static void ucs2_to_vtf7
   (char *outspec,
    unsigned long ucs2_char,
    int * output_cnt)
{
unsigned char * ucs_ptr;
int hex;

    ucs_ptr = (unsigned char *)&ucs2_char;

    outspec[0] = '^';
    outspec[1] = 'U';
    hex = (ucs_ptr[1] >> 4) & 0xf;
    if (hex < 0xA)
	outspec[2] = hex + '0';
    else
	outspec[2] = (hex - 9) + 'A';
    hex = ucs_ptr[1] & 0xF;
    if (hex < 0xA)
	outspec[3] = hex + '0';
    else {
	outspec[3] = (hex - 9) + 'A';
    }
    hex = (ucs_ptr[0] >> 4) & 0xf;
    if (hex < 0xA)
	outspec[4] = hex + '0';
    else
	outspec[4] = (hex - 9) + 'A';
    hex = ucs_ptr[1] & 0xF;
    if (hex < 0xA)
	outspec[5] = hex + '0';
    else {
	outspec[5] = (hex - 9) + 'A';
    }
    *output_cnt = 6;
}


/* This handles the conversion of a UNIX extended character set to a ^
 * escaped VMS character.
 * in a UNIX file specification.
 *
 * The output count variable contains the number of characters added
 * to the output string.
 *
 * The return value is the number of characters read from the input string
 */
static int copy_expand_unix_filename_escape
  (char *outspec, const char *inspec, int *output_cnt, const int * utf8_fl)
{
int count;
int scnt;
int utf8_flag;

    utf8_flag = 0;
    if (utf8_fl)
      utf8_flag = *utf8_fl;

    count = 0;
    *output_cnt = 0;
    if (*inspec >= 0x80) {
	if (utf8_fl && vms_vtf7_filenames) {
	unsigned long ucs_char;

	    ucs_char = 0;

	    if ((*inspec & 0xE0) == 0xC0) {
		/* 2 byte Unicode */
		ucs_char = ((inspec[0] & 0x1F) << 6) + (inspec[1] & 0x3f);
		if (ucs_char >= 0x80) {
		    ucs2_to_vtf7(outspec, ucs_char, output_cnt);
		    return 2;
		}
	    } else if ((*inspec & 0xF0) == 0xE0) {
		/* 3 byte Unicode */
		ucs_char = ((inspec[0] & 0xF) << 12) + 
		   ((inspec[1] & 0x3f) << 6) +
		   (inspec[2] & 0x3f);
		if (ucs_char >= 0x800) {
		    ucs2_to_vtf7(outspec, ucs_char, output_cnt);
		    return 3;
		}

#if 0 /* I do not see longer sequences supported by OpenVMS */
      /* Maybe some one can fix this later */
	    } else if ((*inspec & 0xF8) == 0xF0) {
		/* 4 byte Unicode */
		/* UCS-4 to UCS-2 */
	    } else if ((*inspec & 0xFC) == 0xF8) {
		/* 5 byte Unicode */
		/* UCS-4 to UCS-2 */
	    } else if ((*inspec & 0xFE) == 0xFC) {
		/* 6 byte Unicode */
		/* UCS-4 to UCS-2 */
#endif
	    }
	}

	/* High bit set, but not a Unicode character! */

	/* Non printing DECMCS or ISO Latin-1 character? */
	if (*inspec <= 0x9F) {
	int hex;
	    outspec[0] = '^';
	    outspec++;
	    hex = (*inspec >> 4) & 0xF;
	    if (hex < 0xA)
		outspec[1] = hex + '0';
	    else {
		outspec[1] = (hex - 9) + 'A';
	    }
	    hex = *inspec & 0xF;
	    if (hex < 0xA)
		outspec[2] = hex + '0';
	    else {
		outspec[2] = (hex - 9) + 'A';
	    }
	    *output_cnt = 3;
	    return 1;
	} else if (*inspec == 0xA0) {
	    outspec[0] = '^';
	    outspec[1] = 'A';
	    outspec[2] = '0';
	    *output_cnt = 3;
	    return 1;
	} else if (*inspec == 0xFF) {
	    outspec[0] = '^';
	    outspec[1] = 'F';
	    outspec[2] = 'F';
	    *output_cnt = 3;
	    return 1;
	}
	*outspec = *inspec;
	*output_cnt = 1;
	return 1;
    }

    /* Is this a macro that needs to be passed through?
     * Macros start with $( and an alpha character, followed
     * by a string of alpha numeric characters ending with a )
     * If this does not match, then encode it as ODS-5.
     */
    if ((inspec[0] == '$') && (inspec[1] == '(')) {
    int tcnt;

	if (isalnum(inspec[2]) || (inspec[2] == '.') || (inspec[2] == '_')) {
	    tcnt = 3;
	    outspec[0] = inspec[0];
	    outspec[1] = inspec[1];
	    outspec[2] = inspec[2];

	    while(isalnum(inspec[tcnt]) ||
		  (inspec[2] == '.') || (inspec[2] == '_')) {
		outspec[tcnt] = inspec[tcnt];
		tcnt++;
	    }
	    if (inspec[tcnt] == ')') {
		outspec[tcnt] = inspec[tcnt];
		tcnt++;
		*output_cnt = tcnt;
		return tcnt;
	    }
	}
    }

    switch (*inspec) {
    case 0x7f:
	outspec[0] = '^';
	outspec[1] = '7';
	outspec[2] = 'F';
	*output_cnt = 3;
	return 1;
	break;
    case '?':
	if (decc_efs_charset == 0)
	  outspec[0] = '%';
	else
	  outspec[0] = '?';
	*output_cnt = 1;
	return 1;
	break;
    case '.':
    case '~':
    case '!':
    case '#':
    case '&':
    case '\'':
    case '`':
    case '(':
    case ')':
    case '+':
    case '@':
    case '{':
    case '}':
    case ',':
    case ';':
    case '[':
    case ']':
    case '%':
    case '^':
        /* Don't escape again if following character is 
         * already something we escape.
         */
        if (strchr(".~!#&\'`()+@{},;[]%^=_", *(inspec+1))) {
	    *outspec = *inspec;
	    *output_cnt = 1;
	    return 1;
	    break;
        }
        /* But otherwise fall through and escape it. */
    case '=':
	/* Assume that this is to be escaped */
	outspec[0] = '^';
	outspec[1] = *inspec;
	*output_cnt = 2;
	return 1;
	break;
    case ' ': /* space */
	/* Assume that this is to be escaped */
	outspec[0] = '^';
	outspec[1] = '_';
	*output_cnt = 2;
	return 1;
	break;
    default:
	*outspec = *inspec;
	*output_cnt = 1;
	return 1;
	break;
    }
}


/* This handles the expansion of a '^' prefix to the proper character
 * in a UNIX file specification.
 *
 * The output count variable contains the number of characters added
 * to the output string.
 *
 * The return value is the number of characters read from the input
 * string
 */
static int copy_expand_vms_filename_escape
  (char *outspec, const char *inspec, int *output_cnt)
{
int count;
int scnt;

    count = 0;
    *output_cnt = 0;
    if (*inspec == '^') {
	inspec++;
	switch (*inspec) {
        /* Spaces and non-trailing dots should just be passed through, 
         * but eat the escape character.
         */
	case '.':
	    *outspec = *inspec;
	    count += 2;
	    (*output_cnt)++;
	    break;
	case '_': /* space */
	    *outspec = ' ';
	    count += 2;
	    (*output_cnt)++;
	    break;
	case '^':
            /* Hmm.  Better leave the escape escaped. */
            outspec[0] = '^';
            outspec[1] = '^';
	    count += 2;
	    (*output_cnt) += 2;
	    break;
	case 'U': /* Unicode - FIX-ME this is wrong. */
	    inspec++;
	    count++;
	    scnt = strspn(inspec, "0123456789ABCDEFabcdef");
	    if (scnt == 4) {
		unsigned int c1, c2;
		scnt = sscanf(inspec, "%2x%2x", &c1, &c2);
		outspec[0] == c1 & 0xff;
		outspec[1] == c2 & 0xff;
		if (scnt > 1) {
		    (*output_cnt) += 2;
		    count += 4;
		}
	    }
	    else {
		/* Error - do best we can to continue */
		*outspec = 'U';
		outspec++;
		(*output_cnt++);
		*outspec = *inspec;
		count++;
		(*output_cnt++);
	    }
	    break;
	default:
	    scnt = strspn(inspec, "0123456789ABCDEFabcdef");
	    if (scnt == 2) {
		/* Hex encoded */
		unsigned int c1;
		scnt = sscanf(inspec, "%2x", &c1);
		outspec[0] = c1 & 0xff;
		if (scnt > 0) {
		    (*output_cnt++);
		    count += 2;
	        }
	    }
	    else {
		*outspec = *inspec;
		count++;
		(*output_cnt++);
	    }
	}
    }
    else {
	*outspec = *inspec;
	count++;
	(*output_cnt)++;
    }
    return count;
}

#ifdef sys$filescan
#undef sys$filescan
int sys$filescan
   (const struct dsc$descriptor_s * srcstr,
    struct filescan_itmlst_2 * valuelist,
    unsigned long * fldflags,
    struct dsc$descriptor_s *auxout,
    unsigned short * retlen);
#endif

/* vms_split_path - Verify that the input file specification is a
 * VMS format file specification, and provide pointers to the components of
 * it.  With EFS format filenames, this is virtually the only way to
 * parse a VMS path specification into components.
 *
 * If the sum of the components do not add up to the length of the
 * string, then the passed file specification is probably a UNIX style
 * path.
 */
static int vms_split_path
   (const char * path,
    char * * volume,
    int * vol_len,
    char * * root,
    int * root_len,
    char * * dir,
    int * dir_len,
    char * * name,
    int * name_len,
    char * * ext,
    int * ext_len,
    char * * version,
    int * ver_len)
{
struct dsc$descriptor path_desc;
int status;
unsigned long flags;
int ret_stat;
struct filescan_itmlst_2 item_list[9];
const int filespec = 0;
const int nodespec = 1;
const int devspec = 2;
const int rootspec = 3;
const int dirspec = 4;
const int namespec = 5;
const int typespec = 6;
const int verspec = 7;

    /* Assume the worst for an easy exit */
    ret_stat = -1;
    *volume = NULL;
    *vol_len = 0;
    *root = NULL;
    *root_len = 0;
    *dir = NULL;
    *dir_len;
    *name = NULL;
    *name_len = 0;
    *ext = NULL;
    *ext_len = 0;
    *version = NULL;
    *ver_len = 0;

    path_desc.dsc$a_pointer = (char *)path; /* cast ok */
    path_desc.dsc$w_length = strlen(path);
    path_desc.dsc$b_dtype = DSC$K_DTYPE_T;
    path_desc.dsc$b_class = DSC$K_CLASS_S;

    /* Get the total length, if it is shorter than the string passed
     * then this was probably not a VMS formatted file specification
     */
    item_list[filespec].itmcode = FSCN$_FILESPEC;
    item_list[filespec].length = 0;
    item_list[filespec].component = NULL;

    /* If the node is present, then it gets considered as part of the
     * volume name to hopefully make things simple.
     */
    item_list[nodespec].itmcode = FSCN$_NODE;
    item_list[nodespec].length = 0;
    item_list[nodespec].component = NULL;

    item_list[devspec].itmcode = FSCN$_DEVICE;
    item_list[devspec].length = 0;
    item_list[devspec].component = NULL;

    /* root is a special case,  adding it to either the directory or
     * the device components will probalby complicate things for the
     * callers of this routine, so leave it separate.
     */
    item_list[rootspec].itmcode = FSCN$_ROOT;
    item_list[rootspec].length = 0;
    item_list[rootspec].component = NULL;

    item_list[dirspec].itmcode = FSCN$_DIRECTORY;
    item_list[dirspec].length = 0;
    item_list[dirspec].component = NULL;

    item_list[namespec].itmcode = FSCN$_NAME;
    item_list[namespec].length = 0;
    item_list[namespec].component = NULL;

    item_list[typespec].itmcode = FSCN$_TYPE;
    item_list[typespec].length = 0;
    item_list[typespec].component = NULL;

    item_list[verspec].itmcode = FSCN$_VERSION;
    item_list[verspec].length = 0;
    item_list[verspec].component = NULL;

    item_list[8].itmcode = 0;
    item_list[8].length = 0;
    item_list[8].component = NULL;

    status = sys$filescan
       ((const struct dsc$descriptor_s *)&path_desc, item_list,
	&flags, NULL, NULL);
    _ckvmssts_noperl(status); /* All failure status values indicate a coding error */

    /* If we parsed it successfully these two lengths should be the same */
    if (path_desc.dsc$w_length != item_list[filespec].length)
	return ret_stat;

    /* If we got here, then it is a VMS file specification */
    ret_stat = 0;

    /* set the volume name */
    if (item_list[nodespec].length > 0) {
	*volume = item_list[nodespec].component;
	*vol_len = item_list[nodespec].length + item_list[devspec].length;
    }
    else {
	*volume = item_list[devspec].component;
	*vol_len = item_list[devspec].length;
    }

    *root = item_list[rootspec].component;
    *root_len = item_list[rootspec].length;

    *dir = item_list[dirspec].component;
    *dir_len = item_list[dirspec].length;

    /* Now fun with versions and EFS file specifications
     * The parser can not tell the difference when a "." is a version
     * delimiter or a part of the file specification.
     */
    if ((decc_efs_charset) && 
	(item_list[verspec].length > 0) &&
	(item_list[verspec].component[0] == '.')) {
	*name = item_list[namespec].component;
	*name_len = item_list[namespec].length + item_list[typespec].length;
	*ext = item_list[verspec].component;
	*ext_len = item_list[verspec].length;
	*version = NULL;
	*ver_len = 0;
    }
    else {
	*name = item_list[namespec].component;
	*name_len = item_list[namespec].length;
	*ext = item_list[typespec].component;
	*ext_len = item_list[typespec].length;
	*version = item_list[verspec].component;
	*ver_len = item_list[verspec].length;
    }
    return ret_stat;
}


/* my_maxidx
 * Routine to retrieve the maximum equivalence index for an input
 * logical name.  Some calls to this routine have no knowledge if
 * the variable is a logical or not.  So on error we return a max
 * index of zero.
 */
/*{{{int my_maxidx(const char *lnm) */
static int
my_maxidx(const char *lnm)
{
    int status;
    int midx;
    int attr = LNM$M_CASE_BLIND;
    struct dsc$descriptor lnmdsc;
    struct itmlst_3 itlst[2] = {{sizeof(midx), LNM$_MAX_INDEX, &midx, 0},
                                {0, 0, 0, 0}};

    lnmdsc.dsc$w_length = strlen(lnm);
    lnmdsc.dsc$b_dtype = DSC$K_DTYPE_T;
    lnmdsc.dsc$b_class = DSC$K_CLASS_S;
    lnmdsc.dsc$a_pointer = (char *) lnm; /* Cast ok for read only parameter */

    status = sys$trnlnm(&attr, &fildevdsc, &lnmdsc, 0, itlst);
    if ((status & 1) == 0)
       midx = 0;

    return (midx);
}
/*}}}*/

/*{{{int vmstrnenv(const char *lnm, char *eqv, unsigned long int idx, struct dsc$descriptor_s **tabvec, unsigned long int flags) */
int
Perl_vmstrnenv(const char *lnm, char *eqv, unsigned long int idx,
  struct dsc$descriptor_s **tabvec, unsigned long int flags)
{
    const char *cp1;
    char uplnm[LNM$C_NAMLENGTH+1], *cp2;
    unsigned short int eqvlen, curtab, ivlnm = 0, ivsym = 0, ivenv = 0, secure;
    unsigned long int retsts, attr = LNM$M_CASE_BLIND;
    int midx;
    unsigned char acmode;
    struct dsc$descriptor_s lnmdsc = {0,DSC$K_DTYPE_T,DSC$K_CLASS_S,0},
                            tmpdsc = {6,DSC$K_DTYPE_T,DSC$K_CLASS_S,0};
    struct itmlst_3 lnmlst[3] = {{sizeof idx, LNM$_INDEX, &idx, 0},
                                 {LNM$C_NAMLENGTH, LNM$_STRING, eqv, &eqvlen},
                                 {0, 0, 0, 0}};
    $DESCRIPTOR(crtlenv,"CRTL_ENV");  $DESCRIPTOR(clisym,"CLISYM");
#if defined(PERL_IMPLICIT_CONTEXT)
    pTHX = NULL;
    if (PL_curinterp) {
      aTHX = PERL_GET_INTERP;
    } else {
      aTHX = NULL;
    }
#endif

    if (!lnm || !eqv || ((idx != 0) && ((idx-1) > PERL_LNM_MAX_ALLOWED_INDEX))) {
      set_errno(EINVAL); set_vaxc_errno(SS$_BADPARAM); return 0;
    }
    for (cp1 = lnm, cp2 = uplnm; *cp1; cp1++, cp2++) {
      *cp2 = _toupper(*cp1);
      if (cp1 - lnm > LNM$C_NAMLENGTH) {
        set_errno(EINVAL); set_vaxc_errno(SS$_IVLOGNAM);
        return 0;
      }
    }
    lnmdsc.dsc$w_length = cp1 - lnm;
    lnmdsc.dsc$a_pointer = uplnm;
    uplnm[lnmdsc.dsc$w_length] = '\0';
    secure = flags & PERL__TRNENV_SECURE;
    acmode = secure ? PSL$C_EXEC : PSL$C_USER;
    if (!tabvec || !*tabvec) tabvec = env_tables;

    for (curtab = 0; tabvec[curtab]; curtab++) {
      if (!str$case_blind_compare(tabvec[curtab],&crtlenv)) {
        if (!ivenv && !secure) {
          char *eq, *end;
          int i;
          if (!environ) {
            ivenv = 1; 
            Perl_warn(aTHX_ "Can't read CRTL environ\n");
            continue;
          }
          retsts = SS$_NOLOGNAM;
          for (i = 0; environ[i]; i++) { 
            if ((eq = strchr(environ[i],'=')) && 
                lnmdsc.dsc$w_length == (eq - environ[i]) &&
                !strncmp(environ[i],uplnm,eq - environ[i])) {
              eq++;
              for (eqvlen = 0; eq[eqvlen]; eqvlen++) eqv[eqvlen] = eq[eqvlen];
              if (!eqvlen) continue;
              retsts = SS$_NORMAL;
              break;
            }
          }
          if (retsts != SS$_NOLOGNAM) break;
        }
      }
      else if ((tmpdsc.dsc$a_pointer = tabvec[curtab]->dsc$a_pointer) &&
               !str$case_blind_compare(&tmpdsc,&clisym)) {
        if (!ivsym && !secure) {
          unsigned short int deflen = LNM$C_NAMLENGTH;
          struct dsc$descriptor_d eqvdsc = {0,DSC$K_DTYPE_T,DSC$K_CLASS_D,0};
          /* dynamic dsc to accomodate possible long value */
          _ckvmssts(lib$sget1_dd(&deflen,&eqvdsc));
          retsts = lib$get_symbol(&lnmdsc,&eqvdsc,&eqvlen,0);
          if (retsts & 1) { 
            if (eqvlen > MAX_DCL_SYMBOL) {
              set_errno(EVMSERR); set_vaxc_errno(LIB$_STRTRU);
              eqvlen = MAX_DCL_SYMBOL;
	      /* Special hack--we might be called before the interpreter's */
	      /* fully initialized, in which case either thr or PL_curcop */
	      /* might be bogus. We have to check, since ckWARN needs them */
	      /* both to be valid if running threaded */
		if (ckWARN(WARN_MISC)) {
		  Perl_warner(aTHX_ packWARN(WARN_MISC),"Value of CLI symbol \"%s\" too long",lnm);
		}
            }
            strncpy(eqv,eqvdsc.dsc$a_pointer,eqvlen);
          }
          _ckvmssts(lib$sfree1_dd(&eqvdsc));
          if (retsts == LIB$_INVSYMNAM) { ivsym = 1; continue; }
          if (retsts == LIB$_NOSUCHSYM) continue;
          break;
        }
      }
      else if (!ivlnm) {
        if ( (idx == 0) && (flags & PERL__TRNENV_JOIN_SEARCHLIST) ) {
          midx = my_maxidx(lnm);
          for (idx = 0, cp2 = eqv; idx <= midx; idx++) {
            lnmlst[1].bufadr = cp2;
            eqvlen = 0;
            retsts = sys$trnlnm(&attr,tabvec[curtab],&lnmdsc,&acmode,lnmlst);
            if (retsts == SS$_IVLOGNAM) { ivlnm = 1; break; }
            if (retsts == SS$_NOLOGNAM) break;
            /* PPFs have a prefix */
            if (
#if INTSIZE == 4
                 *((int *)uplnm) == *((int *)"SYS$")                    &&
#endif
                 eqvlen >= 4 && eqv[0] == 0x1b && eqv[1] == 0x00        &&
                 ( (uplnm[4] == 'O' && !strcmp(uplnm,"SYS$OUTPUT"))  ||
                   (uplnm[4] == 'I' && !strcmp(uplnm,"SYS$INPUT"))   ||
                   (uplnm[4] == 'E' && !strcmp(uplnm,"SYS$ERROR"))   ||
                   (uplnm[4] == 'C' && !strcmp(uplnm,"SYS$COMMAND")) )  ) {
              memmove(eqv,eqv+4,eqvlen-4);
              eqvlen -= 4;
            }
            cp2 += eqvlen;
            *cp2 = '\0';
          }
          if ((retsts == SS$_IVLOGNAM) ||
              (retsts == SS$_NOLOGNAM)) { continue; }
        }
        else {
          retsts = sys$trnlnm(&attr,tabvec[curtab],&lnmdsc,&acmode,lnmlst);
          if (retsts == SS$_IVLOGNAM) { ivlnm = 1; continue; }
          if (retsts == SS$_NOLOGNAM) continue;
          eqv[eqvlen] = '\0';
        }
        eqvlen = strlen(eqv);
        break;
      }
    }
    if (retsts & 1) { eqv[eqvlen] = '\0'; return eqvlen; }
    else if (retsts == LIB$_NOSUCHSYM || retsts == LIB$_INVSYMNAM ||
             retsts == SS$_IVLOGNAM   || retsts == SS$_IVLOGTAB   ||
             retsts == SS$_NOLOGNAM) {
      set_errno(EINVAL);  set_vaxc_errno(retsts);
    }
    else _ckvmssts(retsts);
    return 0;
}  /* end of vmstrnenv */
/*}}}*/

/*{{{ int my_trnlnm(const char *lnm, char *eqv, unsigned long int idx)*/
/* Define as a function so we can access statics. */
int Perl_my_trnlnm(pTHX_ const char *lnm, char *eqv, unsigned long int idx)
{
  return vmstrnenv(lnm,eqv,idx,fildev,                                   
#ifdef SECURE_INTERNAL_GETENV
                   (PL_curinterp ? PL_tainting : will_taint) ? PERL__TRNENV_SECURE : 0
#else
                   0
#endif
                                                                              );
}
/*}}}*/

/* my_getenv
 * Note: Uses Perl temp to store result so char * can be returned to
 * caller; this pointer will be invalidated at next Perl statement
 * transition.
 * We define this as a function rather than a macro in terms of my_getenv_len()
 * so that it'll work when PL_curinterp is undefined (and we therefore can't
 * allocate SVs).
 */
/*{{{ char *my_getenv(const char *lnm, bool sys)*/
char *
Perl_my_getenv(pTHX_ const char *lnm, bool sys)
{
    const char *cp1;
    static char *__my_getenv_eqv = NULL;
    char uplnm[LNM$C_NAMLENGTH+1], *cp2, *eqv;
    unsigned long int idx = 0;
    int trnsuccess, success, secure, saverr, savvmserr;
    int midx, flags;
    SV *tmpsv;

    midx = my_maxidx(lnm) + 1;

    if (PL_curinterp) {  /* Perl interpreter running -- may be threaded */
      /* Set up a temporary buffer for the return value; Perl will
       * clean it up at the next statement transition */
      tmpsv = sv_2mortal(newSVpv("",(LNM$C_NAMLENGTH*midx)+1));
      if (!tmpsv) return NULL;
      eqv = SvPVX(tmpsv);
    }
    else {
      /* Assume no interpreter ==> single thread */
      if (__my_getenv_eqv != NULL) {
        Renew(__my_getenv_eqv,LNM$C_NAMLENGTH*midx+1,char);
      }
      else {
        Newx(__my_getenv_eqv,LNM$C_NAMLENGTH*midx+1,char);
      }
      eqv = __my_getenv_eqv;  
    }

    for (cp1 = lnm, cp2 = eqv; *cp1; cp1++,cp2++) *cp2 = _toupper(*cp1);
    if (cp1 - lnm == 7 && !strncmp(eqv,"DEFAULT",7)) {
      int len;
      getcwd(eqv,LNM$C_NAMLENGTH);

      len = strlen(eqv);

      /* Get rid of "000000/ in rooted filespecs */
      if (len > 7) {
        char * zeros;
	zeros = strstr(eqv, "/000000/");
	if (zeros != NULL) {
	  int mlen;
	  mlen = len - (zeros - eqv) - 7;
	  memmove(zeros, &zeros[7], mlen);
	  len = len - 7;
	  eqv[len] = '\0';
	}
      }
      return eqv;
    }
    else {
      /* Impose security constraints only if tainting */
      if (sys) {
        /* Impose security constraints only if tainting */
        secure = PL_curinterp ? PL_tainting : will_taint;
        saverr = errno;  savvmserr = vaxc$errno;
      }
      else {
        secure = 0;
      }

      flags = 
#ifdef SECURE_INTERNAL_GETENV
              secure ? PERL__TRNENV_SECURE : 0
#else
              0
#endif
      ;

      /* For the getenv interface we combine all the equivalence names
       * of a search list logical into one value to acquire a maximum
       * value length of 255*128 (assuming %ENV is using logicals).
       */
      flags |= PERL__TRNENV_JOIN_SEARCHLIST;

      /* If the name contains a semicolon-delimited index, parse it
       * off and make sure we only retrieve the equivalence name for 
       * that index.  */
      if ((cp2 = strchr(lnm,';')) != NULL) {
        strcpy(uplnm,lnm);
        uplnm[cp2-lnm] = '\0';
        idx = strtoul(cp2+1,NULL,0);
        lnm = uplnm;
        flags &= ~PERL__TRNENV_JOIN_SEARCHLIST;
      }

      success = vmstrnenv(lnm,eqv,idx,secure ? fildev : NULL,flags);

      /* Discard NOLOGNAM on internal calls since we're often looking
       * for an optional name, and this "error" often shows up as the
       * (bogus) exit status for a die() call later on.  */
      if (sys && vaxc$errno == SS$_NOLOGNAM) SETERRNO(saverr,savvmserr);
      return success ? eqv : NULL;
    }

}  /* end of my_getenv() */
/*}}}*/


/*{{{ SV *my_getenv_len(const char *lnm, bool sys)*/
char *
Perl_my_getenv_len(pTHX_ const char *lnm, unsigned long *len, bool sys)
{
    const char *cp1;
    char *buf, *cp2;
    unsigned long idx = 0;
    int midx, flags;
    static char *__my_getenv_len_eqv = NULL;
    int secure, saverr, savvmserr;
    SV *tmpsv;
    
    midx = my_maxidx(lnm) + 1;

    if (PL_curinterp) {  /* Perl interpreter running -- may be threaded */
      /* Set up a temporary buffer for the return value; Perl will
       * clean it up at the next statement transition */
      tmpsv = sv_2mortal(newSVpv("",(LNM$C_NAMLENGTH*midx)+1));
      if (!tmpsv) return NULL;
      buf = SvPVX(tmpsv);
    }
    else {
      /* Assume no interpreter ==> single thread */
      if (__my_getenv_len_eqv != NULL) {
        Renew(__my_getenv_len_eqv,LNM$C_NAMLENGTH*midx+1,char);
      }
      else {
        Newx(__my_getenv_len_eqv,LNM$C_NAMLENGTH*midx+1,char);
      }
      buf = __my_getenv_len_eqv;  
    }

    for (cp1 = lnm, cp2 = buf; *cp1; cp1++,cp2++) *cp2 = _toupper(*cp1);
    if (cp1 - lnm == 7 && !strncmp(buf,"DEFAULT",7)) {
    char * zeros;

      getcwd(buf,LNM$C_NAMLENGTH);
      *len = strlen(buf);

      /* Get rid of "000000/ in rooted filespecs */
      if (*len > 7) {
      zeros = strstr(buf, "/000000/");
      if (zeros != NULL) {
	int mlen;
	mlen = *len - (zeros - buf) - 7;
	memmove(zeros, &zeros[7], mlen);
	*len = *len - 7;
	buf[*len] = '\0';
	}
      }
      return buf;
    }
    else {
      if (sys) {
        /* Impose security constraints only if tainting */
        secure = PL_curinterp ? PL_tainting : will_taint;
        saverr = errno;  savvmserr = vaxc$errno;
      }
      else {
        secure = 0;
      }

      flags = 
#ifdef SECURE_INTERNAL_GETENV
              secure ? PERL__TRNENV_SECURE : 0
#else
              0
#endif
      ;

      flags |= PERL__TRNENV_JOIN_SEARCHLIST;

      if ((cp2 = strchr(lnm,';')) != NULL) {
        strcpy(buf,lnm);
        buf[cp2-lnm] = '\0';
        idx = strtoul(cp2+1,NULL,0);
        lnm = buf;
        flags &= ~PERL__TRNENV_JOIN_SEARCHLIST;
      }

      *len = vmstrnenv(lnm,buf,idx,secure ? fildev : NULL,flags);

      /* Get rid of "000000/ in rooted filespecs */
      if (*len > 7) {
      char * zeros;
	zeros = strstr(buf, "/000000/");
	if (zeros != NULL) {
	  int mlen;
	  mlen = *len - (zeros - buf) - 7;
	  memmove(zeros, &zeros[7], mlen);
	  *len = *len - 7;
	  buf[*len] = '\0';
	}
      }

      /* Discard NOLOGNAM on internal calls since we're often looking
       * for an optional name, and this "error" often shows up as the
       * (bogus) exit status for a die() call later on.  */
      if (sys && vaxc$errno == SS$_NOLOGNAM) SETERRNO(saverr,savvmserr);
      return *len ? buf : NULL;
    }

}  /* end of my_getenv_len() */
/*}}}*/

static void create_mbx(pTHX_ unsigned short int *, struct dsc$descriptor_s *);

static void riseandshine(unsigned long int dummy) { sys$wake(0,0); }

/*{{{ void prime_env_iter() */
void
prime_env_iter(void)
/* Fill the %ENV associative array with all logical names we can
 * find, in preparation for iterating over it.
 */
{
  static int primed = 0;
  HV *seenhv = NULL, *envhv;
  SV *sv = NULL;
  char cmd[LNM$C_NAMLENGTH+24], mbxnam[LNM$C_NAMLENGTH], *buf = NULL;
  unsigned short int chan;
#ifndef CLI$M_TRUSTED
#  define CLI$M_TRUSTED 0x40  /* Missing from VAXC headers */
#endif
  unsigned long int defflags = CLI$M_NOWAIT | CLI$M_NOKEYPAD | CLI$M_TRUSTED;
  unsigned long int mbxbufsiz, flags, retsts, subpid = 0, substs = 0, wakect = 0;
  long int i;
  bool have_sym = FALSE, have_lnm = FALSE;
  struct dsc$descriptor_s tmpdsc = {6,DSC$K_DTYPE_T,DSC$K_CLASS_S,0};
  $DESCRIPTOR(cmddsc,cmd);    $DESCRIPTOR(nldsc,"_NLA0:");
  $DESCRIPTOR(clidsc,"DCL");  $DESCRIPTOR(clitabdsc,"DCLTABLES");
  $DESCRIPTOR(crtlenv,"CRTL_ENV");  $DESCRIPTOR(clisym,"CLISYM");
  $DESCRIPTOR(local,"_LOCAL"); $DESCRIPTOR(mbxdsc,mbxnam); 
#if defined(PERL_IMPLICIT_CONTEXT)
  pTHX;
#endif

#if defined(PERL_IMPLICIT_CONTEXT)
    /* We jump through these hoops because we can be called at */
    /* platform-specific initialization time, which is before anything is */
    /* set up--we can't even do a plain dTHX since that relies on the */
    /* interpreter structure to be initialized */
    if (PL_curinterp) {
      aTHX = PERL_GET_INTERP;
    } else {
      aTHX = NULL;
    }
#endif

  if (primed || !PL_envhv) return;
  MUTEX_LOCK(&primenv_mutex);
  if (primed) { MUTEX_UNLOCK(&primenv_mutex); return; }
  envhv = PL_envhv;
  /* Perform a dummy fetch as an lval to insure that the hash table is
   * set up.  Otherwise, the hv_store() will turn into a nullop. */
  (void) hv_fetch(envhv,"DEFAULT",7,TRUE);

  for (i = 0; env_tables[i]; i++) {
     if (!have_sym && (tmpdsc.dsc$a_pointer = env_tables[i]->dsc$a_pointer) &&
         !str$case_blind_compare(&tmpdsc,&clisym)) have_sym = 1;
     if (!have_lnm && str$case_blind_compare(env_tables[i],&crtlenv)) have_lnm = 1;
  }
  if (have_sym || have_lnm) {
    long int syiitm = SYI$_MAXBUF, dviitm = DVI$_DEVNAM;
    _ckvmssts(lib$getsyi(&syiitm, &mbxbufsiz, 0, 0, 0, 0));
    _ckvmssts(sys$crembx(0,&chan,mbxbufsiz,mbxbufsiz,0xff0f,0,0));
    _ckvmssts(lib$getdvi(&dviitm, &chan, NULL, NULL, &mbxdsc, &mbxdsc.dsc$w_length));
  }

  for (i--; i >= 0; i--) {
    if (!str$case_blind_compare(env_tables[i],&crtlenv)) {
      char *start;
      int j;
      for (j = 0; environ[j]; j++) { 
        if (!(start = strchr(environ[j],'='))) {
          if (ckWARN(WARN_INTERNAL)) 
            Perl_warner(aTHX_ packWARN(WARN_INTERNAL),"Ill-formed CRTL environ value \"%s\"\n",environ[j]);
        }
        else {
          start++;
          sv = newSVpv(start,0);
          (void) hv_store(envhv,environ[j],start - environ[j] - 1,sv,0);
        }
      }
      continue;
    }
    else if ((tmpdsc.dsc$a_pointer = env_tables[i]->dsc$a_pointer) &&
             !str$case_blind_compare(&tmpdsc,&clisym)) {
      strcpy(cmd,"Show Symbol/Global *");
      cmddsc.dsc$w_length = 20;
      if (env_tables[i]->dsc$w_length == 12 &&
          (tmpdsc.dsc$a_pointer = env_tables[i]->dsc$a_pointer + 6) &&
          !str$case_blind_compare(&tmpdsc,&local)) strcpy(cmd+12,"Local  *");
      flags = defflags | CLI$M_NOLOGNAM;
    }
    else {
      strcpy(cmd,"Show Logical *");
      if (str$case_blind_compare(env_tables[i],&fildevdsc)) {
        strcat(cmd," /Table=");
        strncat(cmd,env_tables[i]->dsc$a_pointer,env_tables[i]->dsc$w_length);
        cmddsc.dsc$w_length = strlen(cmd);
      }
      else cmddsc.dsc$w_length = 14;  /* N.B. We test this below */
      flags = defflags | CLI$M_NOCLISYM;
    }
    
    /* Create a new subprocess to execute each command, to exclude the
     * remote possibility that someone could subvert a mbx or file used
     * to write multiple commands to a single subprocess.
     */
    do {
      retsts = lib$spawn(&cmddsc,&nldsc,&mbxdsc,&flags,0,&subpid,&substs,
                         0,&riseandshine,0,0,&clidsc,&clitabdsc);
      flags &= ~CLI$M_TRUSTED; /* Just in case we hit a really old version */
      defflags &= ~CLI$M_TRUSTED;
    } while (retsts == LIB$_INVARG && (flags | CLI$M_TRUSTED));
    _ckvmssts(retsts);
    if (!buf) Newx(buf,mbxbufsiz + 1,char);
    if (seenhv) SvREFCNT_dec(seenhv);
    seenhv = newHV();
    while (1) {
      char *cp1, *cp2, *key;
      unsigned long int sts, iosb[2], retlen, keylen;
      register U32 hash;

      sts = sys$qiow(0,chan,IO$_READVBLK,iosb,0,0,buf,mbxbufsiz,0,0,0,0);
      if (sts & 1) sts = iosb[0] & 0xffff;
      if (sts == SS$_ENDOFFILE) {
        int wakect = 0;
        while (substs == 0) { sys$hiber(); wakect++;}
        if (wakect > 1) sys$wake(0,0);  /* Stole someone else's wake */
        _ckvmssts(substs);
        break;
      }
      _ckvmssts(sts);
      retlen = iosb[0] >> 16;      
      if (!retlen) continue;  /* blank line */
      buf[retlen] = '\0';
      if (iosb[1] != subpid) {
        if (iosb[1]) {
          Perl_croak(aTHX_ "Unknown process %x sent message to prime_env_iter: %s",buf);
        }
        continue;
      }
      if (sts == SS$_BUFFEROVF && ckWARN(WARN_INTERNAL))
        Perl_warner(aTHX_ packWARN(WARN_INTERNAL),"Buffer overflow in prime_env_iter: %s",buf);

      for (cp1 = buf; *cp1 && isspace(*cp1); cp1++) ;
      if (*cp1 == '(' || /* Logical name table name */
          *cp1 == '='    /* Next eqv of searchlist  */) continue;
      if (*cp1 == '"') cp1++;
      for (cp2 = cp1; *cp2 && *cp2 != '"' && *cp2 != ' '; cp2++) ;
      key = cp1;  keylen = cp2 - cp1;
      if (keylen && hv_exists(seenhv,key,keylen)) continue;
      while (*cp2 && *cp2 != '=') cp2++;
      while (*cp2 && *cp2 == '=') cp2++;
      while (*cp2 && *cp2 == ' ') cp2++;
      if (*cp2 == '"') {  /* String translation; may embed "" */
        for (cp1 = buf + retlen; *cp1 != '"'; cp1--) ;
        cp2++;  cp1--; /* Skip "" surrounding translation */
      }
      else {  /* Numeric translation */
        for (cp1 = cp2; *cp1 && *cp1 != ' '; cp1++) ;
        cp1--;  /* stop on last non-space char */
      }
      if ((!keylen || (cp1 - cp2 < -1)) && ckWARN(WARN_INTERNAL)) {
        Perl_warner(aTHX_ packWARN(WARN_INTERNAL),"Ill-formed message in prime_env_iter: |%s|",buf);
        continue;
      }
      PERL_HASH(hash,key,keylen);

      if (cp1 == cp2 && *cp2 == '.') {
        /* A single dot usually means an unprintable character, such as a null
         * to indicate a zero-length value.  Get the actual value to make sure.
         */
        char lnm[LNM$C_NAMLENGTH+1];
        char eqv[MAX_DCL_SYMBOL+1];
        int trnlen;
        strncpy(lnm, key, keylen);
        trnlen = vmstrnenv(lnm, eqv, 0, fildev, 0);
        sv = newSVpvn(eqv, strlen(eqv));
      }
      else {
        sv = newSVpvn(cp2,cp1 - cp2 + 1);
      }

      hv_store(envhv,key,keylen,sv,hash);
      hv_store(seenhv,key,keylen,&PL_sv_yes,hash);
    }
    if (cmddsc.dsc$w_length == 14) { /* We just read LNM$FILE_DEV */
      /* get the PPFs for this process, not the subprocess */
      const char *ppfs[] = {"SYS$COMMAND", "SYS$INPUT", "SYS$OUTPUT", "SYS$ERROR", NULL};
      char eqv[LNM$C_NAMLENGTH+1];
      int trnlen, i;
      for (i = 0; ppfs[i]; i++) {
        trnlen = vmstrnenv(ppfs[i],eqv,0,fildev,0);
        sv = newSVpv(eqv,trnlen);
        hv_store(envhv,ppfs[i],strlen(ppfs[i]),sv,0);
      }
    }
  }
  primed = 1;
  if (have_sym || have_lnm) _ckvmssts(sys$dassgn(chan));
  if (buf) Safefree(buf);
  if (seenhv) SvREFCNT_dec(seenhv);
  MUTEX_UNLOCK(&primenv_mutex);
  return;

}  /* end of prime_env_iter */
/*}}}*/


/*{{{ int  vmssetenv(const char *lnm, const char *eqv)*/
/* Define or delete an element in the same "environment" as
 * vmstrnenv().  If an element is to be deleted, it's removed from
 * the first place it's found.  If it's to be set, it's set in the
 * place designated by the first element of the table vector.
 * Like setenv() returns 0 for success, non-zero on error.
 */
int
Perl_vmssetenv(pTHX_ const char *lnm, const char *eqv, struct dsc$descriptor_s **tabvec)
{
    const char *cp1;
    char uplnm[LNM$C_NAMLENGTH], *cp2, *c;
    unsigned short int curtab, ivlnm = 0, ivsym = 0, ivenv = 0;
    int nseg = 0, j;
    unsigned long int retsts, usermode = PSL$C_USER;
    struct itmlst_3 *ile, *ilist;
    struct dsc$descriptor_s lnmdsc = {0,DSC$K_DTYPE_T,DSC$K_CLASS_S,uplnm},
                            eqvdsc = {0,DSC$K_DTYPE_T,DSC$K_CLASS_S,0},
                            tmpdsc = {6,DSC$K_DTYPE_T,DSC$K_CLASS_S,0};
    $DESCRIPTOR(crtlenv,"CRTL_ENV");  $DESCRIPTOR(clisym,"CLISYM");
    $DESCRIPTOR(local,"_LOCAL");

    if (!lnm) {
        set_errno(EINVAL); set_vaxc_errno(SS$_IVLOGNAM);
        return SS$_IVLOGNAM;
    }

    for (cp1 = lnm, cp2 = uplnm; *cp1; cp1++, cp2++) {
      *cp2 = _toupper(*cp1);
      if (cp1 - lnm > LNM$C_NAMLENGTH) {
        set_errno(EINVAL); set_vaxc_errno(SS$_IVLOGNAM);
        return SS$_IVLOGNAM;
      }
    }
    lnmdsc.dsc$w_length = cp1 - lnm;
    if (!tabvec || !*tabvec) tabvec = env_tables;

    if (!eqv) {  /* we're deleting n element */
      for (curtab = 0; tabvec[curtab]; curtab++) {
        if (!ivenv && !str$case_blind_compare(tabvec[curtab],&crtlenv)) {
        int i;
          for (i = 0; environ[i]; i++) { /* If it's an environ elt, reset */
            if ((cp1 = strchr(environ[i],'=')) && 
                lnmdsc.dsc$w_length == (cp1 - environ[i]) &&
                !strncmp(environ[i],lnm,cp1 - environ[i])) {
#ifdef HAS_SETENV
              return setenv(lnm,"",1) ? vaxc$errno : 0;
            }
          }
          ivenv = 1; retsts = SS$_NOLOGNAM;
#else
              if (ckWARN(WARN_INTERNAL))
                Perl_warner(aTHX_ packWARN(WARN_INTERNAL),"This Perl can't reset CRTL environ elements (%s)",lnm);
              ivenv = 1; retsts = SS$_NOSUCHPGM;
              break;
            }
          }
#endif
        }
        else if ((tmpdsc.dsc$a_pointer = tabvec[curtab]->dsc$a_pointer) &&
                 !str$case_blind_compare(&tmpdsc,&clisym)) {
          unsigned int symtype;
          if (tabvec[curtab]->dsc$w_length == 12 &&
              (tmpdsc.dsc$a_pointer = tabvec[curtab]->dsc$a_pointer + 6) &&
              !str$case_blind_compare(&tmpdsc,&local)) 
            symtype = LIB$K_CLI_LOCAL_SYM;
          else symtype = LIB$K_CLI_GLOBAL_SYM;
          retsts = lib$delete_symbol(&lnmdsc,&symtype);
          if (retsts == LIB$_INVSYMNAM) { ivsym = 1; continue; }
          if (retsts == LIB$_NOSUCHSYM) continue;
          break;
        }
        else if (!ivlnm) {
          retsts = sys$dellnm(tabvec[curtab],&lnmdsc,&usermode); /* try user mode first */
          if (retsts == SS$_IVLOGNAM) { ivlnm = 1; continue; }
          if (retsts != SS$_NOLOGNAM && retsts != SS$_NOLOGTAB) break;
          retsts = lib$delete_logical(&lnmdsc,tabvec[curtab]); /* then supervisor mode */
          if (retsts != SS$_NOLOGNAM && retsts != SS$_NOLOGTAB) break;
        }
      }
    }
    else {  /* we're defining a value */
      if (!ivenv && !str$case_blind_compare(tabvec[0],&crtlenv)) {
#ifdef HAS_SETENV
        return setenv(lnm,eqv,1) ? vaxc$errno : 0;
#else
        if (ckWARN(WARN_INTERNAL))
          Perl_warner(aTHX_ packWARN(WARN_INTERNAL),"This Perl can't set CRTL environ elements (%s=%s)",lnm,eqv);
        retsts = SS$_NOSUCHPGM;
#endif
      }
      else {
        eqvdsc.dsc$a_pointer = (char *) eqv; /* cast ok to readonly parameter */
        eqvdsc.dsc$w_length  = strlen(eqv);
        if ((tmpdsc.dsc$a_pointer = tabvec[0]->dsc$a_pointer) &&
            !str$case_blind_compare(&tmpdsc,&clisym)) {
          unsigned int symtype;
          if (tabvec[0]->dsc$w_length == 12 &&
              (tmpdsc.dsc$a_pointer = tabvec[0]->dsc$a_pointer + 6) &&
               !str$case_blind_compare(&tmpdsc,&local)) 
            symtype = LIB$K_CLI_LOCAL_SYM;
          else symtype = LIB$K_CLI_GLOBAL_SYM;
          retsts = lib$set_symbol(&lnmdsc,&eqvdsc,&symtype);
        }
        else {
          if (!*eqv) eqvdsc.dsc$w_length = 1;
	  if (eqvdsc.dsc$w_length > LNM$C_NAMLENGTH) {

            nseg = (eqvdsc.dsc$w_length + LNM$C_NAMLENGTH - 1) / LNM$C_NAMLENGTH;
            if (nseg > PERL_LNM_MAX_ALLOWED_INDEX + 1) {
	      Perl_warner(aTHX_ packWARN(WARN_MISC),"Value of logical \"%s\" too long. Truncating to %i bytes",
                          lnm, LNM$C_NAMLENGTH * (PERL_LNM_MAX_ALLOWED_INDEX+1));
              eqvdsc.dsc$w_length = LNM$C_NAMLENGTH * (PERL_LNM_MAX_ALLOWED_INDEX+1);
              nseg = PERL_LNM_MAX_ALLOWED_INDEX + 1;
	    }

            Newx(ilist,nseg+1,struct itmlst_3);
            ile = ilist;
            if (!ile) {
	      set_errno(ENOMEM); set_vaxc_errno(SS$_INSFMEM);
              return SS$_INSFMEM;
	    }
            memset(ilist, 0, (sizeof(struct itmlst_3) * (nseg+1)));

            for (j = 0, c = eqvdsc.dsc$a_pointer; j < nseg; j++, ile++, c += LNM$C_NAMLENGTH) {
              ile->itmcode = LNM$_STRING;
              ile->bufadr = c;
              if ((j+1) == nseg) {
                ile->buflen = strlen(c);
                /* in case we are truncating one that's too long */
                if (ile->buflen > LNM$C_NAMLENGTH) ile->buflen = LNM$C_NAMLENGTH;
              }
              else {
                ile->buflen = LNM$C_NAMLENGTH;
              }
            }

            retsts = lib$set_logical(&lnmdsc,0,tabvec[0],0,ilist);
            Safefree (ilist);
	  }
          else {
            retsts = lib$set_logical(&lnmdsc,&eqvdsc,tabvec[0],0,0);
	  }
        }
      }
    }
    if (!(retsts & 1)) {
      switch (retsts) {
        case LIB$_AMBSYMDEF: case LIB$_INSCLIMEM:
        case SS$_NOLOGTAB: case SS$_TOOMANYLNAM: case SS$_IVLOGTAB:
          set_errno(EVMSERR); break;
        case LIB$_INVARG: case LIB$_INVSYMNAM: case SS$_IVLOGNAM: 
        case LIB$_NOSUCHSYM: case SS$_NOLOGNAM:
          set_errno(EINVAL); break;
        case SS$_NOPRIV:
          set_errno(EACCES); break;
        default:
          _ckvmssts(retsts);
          set_errno(EVMSERR);
       }
       set_vaxc_errno(retsts);
       return (int) retsts || 44; /* retsts should never be 0, but just in case */
    }
    else {
      /* We reset error values on success because Perl does an hv_fetch()
       * before each hv_store(), and if the thing we're setting didn't
       * previously exist, we've got a leftover error message.  (Of course,
       * this fails in the face of
       *    $foo = $ENV{nonexistent}; $ENV{existent} = 'foo';
       * in that the error reported in $! isn't spurious, 
       * but it's right more often than not.)
       */
      set_errno(0); set_vaxc_errno(retsts);
      return 0;
    }

}  /* end of vmssetenv() */
/*}}}*/

/*{{{ void  my_setenv(const char *lnm, const char *eqv)*/
/* This has to be a function since there's a prototype for it in proto.h */
void
Perl_my_setenv(pTHX_ const char *lnm, const char *eqv)
{
    if (lnm && *lnm) {
      int len = strlen(lnm);
      if  (len == 7) {
        char uplnm[8];
        int i;
        for (i = 0; lnm[i]; i++) uplnm[i] = _toupper(lnm[i]);
        if (!strcmp(uplnm,"DEFAULT")) {
          if (eqv && *eqv) my_chdir(eqv);
          return;
        }
    } 
#ifndef RTL_USES_UTC
    if (len == 6 || len == 2) {
      char uplnm[7];
      int i;
      for (i = 0; lnm[i]; i++) uplnm[i] = _toupper(lnm[i]);
      uplnm[len] = '\0';
      if (!strcmp(uplnm,"UCX$TZ")) tz_updated = 1;
      if (!strcmp(uplnm,"TZ")) tz_updated = 1;
    }
#endif
  }
  (void) vmssetenv(lnm,eqv,NULL);
}
/*}}}*/

/*{{{static void vmssetuserlnm(char *name, char *eqv); */
/*  vmssetuserlnm
 *  sets a user-mode logical in the process logical name table
 *  used for redirection of sys$error
 */
void
Perl_vmssetuserlnm(pTHX_ const char *name, const char *eqv)
{
    $DESCRIPTOR(d_tab, "LNM$PROCESS");
    struct dsc$descriptor_d d_name = {0,DSC$K_DTYPE_T,DSC$K_CLASS_D,0};
    unsigned long int iss, attr = LNM$M_CONFINE;
    unsigned char acmode = PSL$C_USER;
    struct itmlst_3 lnmlst[2] = {{0, LNM$_STRING, 0, 0},
                                 {0, 0, 0, 0}};
    d_name.dsc$a_pointer = (char *)name; /* Cast OK for read only parameter */
    d_name.dsc$w_length = strlen(name);

    lnmlst[0].buflen = strlen(eqv);
    lnmlst[0].bufadr = (char *)eqv; /* Cast OK for read only parameter */

    iss = sys$crelnm(&attr,&d_tab,&d_name,&acmode,lnmlst);
    if (!(iss&1)) lib$signal(iss);
}
/*}}}*/


/*{{{ char *my_crypt(const char *textpasswd, const char *usrname)*/
/* my_crypt - VMS password hashing
 * my_crypt() provides an interface compatible with the Unix crypt()
 * C library function, and uses sys$hash_password() to perform VMS
 * password hashing.  The quadword hashed password value is returned
 * as a NUL-terminated 8 character string.  my_crypt() does not change
 * the case of its string arguments; in order to match the behavior
 * of LOGINOUT et al., alphabetic characters in both arguments must
 *  be upcased by the caller.
 *
 * - fix me to call ACM services when available
 */
char *
Perl_my_crypt(pTHX_ const char *textpasswd, const char *usrname)
{
#   ifndef UAI$C_PREFERRED_ALGORITHM
#     define UAI$C_PREFERRED_ALGORITHM 127
#   endif
    unsigned char alg = UAI$C_PREFERRED_ALGORITHM;
    unsigned short int salt = 0;
    unsigned long int sts;
    struct const_dsc {
        unsigned short int dsc$w_length;
        unsigned char      dsc$b_type;
        unsigned char      dsc$b_class;
        const char *       dsc$a_pointer;
    }  usrdsc = {0, DSC$K_DTYPE_T, DSC$K_CLASS_S, 0},
       txtdsc = {0, DSC$K_DTYPE_T, DSC$K_CLASS_S, 0};
    struct itmlst_3 uailst[3] = {
        { sizeof alg,  UAI$_ENCRYPT, &alg, 0},
        { sizeof salt, UAI$_SALT,    &salt, 0},
        { 0,           0,            NULL,  NULL}};
    static char hash[9];

    usrdsc.dsc$w_length = strlen(usrname);
    usrdsc.dsc$a_pointer = usrname;
    if (!((sts = sys$getuai(0, 0, &usrdsc, uailst, 0, 0, 0)) & 1)) {
      switch (sts) {
        case SS$_NOGRPPRV: case SS$_NOSYSPRV:
          set_errno(EACCES);
          break;
        case RMS$_RNF:
          set_errno(ESRCH);  /* There isn't a Unix no-such-user error */
          break;
        default:
          set_errno(EVMSERR);
      }
      set_vaxc_errno(sts);
      if (sts != RMS$_RNF) return NULL;
    }

    txtdsc.dsc$w_length = strlen(textpasswd);
    txtdsc.dsc$a_pointer = textpasswd;
    if (!((sts = sys$hash_password(&txtdsc, alg, salt, &usrdsc, &hash)) & 1)) {
      set_errno(EVMSERR);  set_vaxc_errno(sts);  return NULL;
    }

    return (char *) hash;

}  /* end of my_crypt() */
/*}}}*/


static char *mp_do_rmsexpand(pTHX_ const char *, char *, int, const char *, unsigned, int *, int *);
static char *mp_do_fileify_dirspec(pTHX_ const char *, char *, int, int *);
static char *mp_do_tovmsspec(pTHX_ const char *, char *, int, int, int *);

/* fixup barenames that are directories for internal use.
 * There have been problems with the consistent handling of UNIX
 * style directory names when routines are presented with a name that
 * has no directory delimitors at all.  So this routine will eventually
 * fix the issue.
 */
static char * fixup_bare_dirnames(const char * name)
{
  if (decc_disable_to_vms_logname_translation) {
/* fix me */
  }
  return NULL;
}

/* 8.3, remove() is now broken on symbolic links */
static int rms_erase(const char * vmsname);


/* mp_do_kill_file
 * A little hack to get around a bug in some implemenation of remove()
 * that do not know how to delete a directory
 *
 * Delete any file to which user has control access, regardless of whether
 * delete access is explicitly allowed.
 * Limitations: User must have write access to parent directory.
 *              Does not block signals or ASTs; if interrupted in midstream
 *              may leave file with an altered ACL.
 * HANDLE WITH CARE!
 */
/*{{{int mp_do_kill_file(const char *name, int dirflag)*/
static int
mp_do_kill_file(pTHX_ const char *name, int dirflag)
{
    char *vmsname;
    char *rslt;
    unsigned long int jpicode = JPI$_UIC, type = ACL$C_FILE;
    unsigned long int cxt = 0, aclsts, fndsts, rmsts = -1;
    struct dsc$descriptor_s fildsc = {0, DSC$K_DTYPE_T, DSC$K_CLASS_S, 0};
    struct myacedef {
      unsigned char myace$b_length;
      unsigned char myace$b_type;
      unsigned short int myace$w_flags;
      unsigned long int myace$l_access;
      unsigned long int myace$l_ident;
    } newace = { sizeof(struct myacedef), ACE$C_KEYID, 0,
                 ACE$M_READ | ACE$M_WRITE | ACE$M_DELETE | ACE$M_CONTROL, 0},
      oldace = { sizeof(struct myacedef), ACE$C_KEYID, 0, 0, 0};
     struct itmlst_3
       findlst[3] = {{sizeof oldace, ACL$C_FNDACLENT, &oldace, 0},
                     {sizeof oldace, ACL$C_READACE,   &oldace, 0},{0,0,0,0}},
       addlst[2] = {{sizeof newace, ACL$C_ADDACLENT, &newace, 0},{0,0,0,0}},
       dellst[2] = {{sizeof newace, ACL$C_DELACLENT, &newace, 0},{0,0,0,0}},
       lcklst[2] = {{sizeof newace, ACL$C_WLOCK_ACL, &newace, 0},{0,0,0,0}},
       ulklst[2] = {{sizeof newace, ACL$C_UNLOCK_ACL, &newace, 0},{0,0,0,0}};

    /* Expand the input spec using RMS, since the CRTL remove() and
     * system services won't do this by themselves, so we may miss
     * a file "hiding" behind a logical name or search list. */
    vmsname = PerlMem_malloc(NAM$C_MAXRSS+1);
    if (vmsname == NULL) _ckvmssts(SS$_INSFMEM);

    rslt = do_rmsexpand(name,
			vmsname,
			0,
			NULL,
			PERL_RMSEXPAND_M_VMS | PERL_RMSEXPAND_M_SYMLINK,
			NULL,
			NULL);
    if (rslt == NULL) {
        PerlMem_free(vmsname);
	return -1;
      }

    /* Erase the file */
    rmsts = rms_erase(vmsname);

    /* Did it succeed */
    if ($VMS_STATUS_SUCCESS(rmsts)) {
	PerlMem_free(vmsname);
	return 0;
      }

    /* If not, can changing protections help? */
    if (rmsts != RMS$_PRV) {
      set_vaxc_errno(rmsts);
      PerlMem_free(vmsname);
      return -1;
    }

    /* No, so we get our own UIC to use as a rights identifier,
     * and the insert an ACE at the head of the ACL which allows us
     * to delete the file.
     */
    _ckvmssts(lib$getjpi(&jpicode,0,0,&(oldace.myace$l_ident),0,0));
    fildsc.dsc$w_length = strlen(vmsname);
    fildsc.dsc$a_pointer = vmsname;
    cxt = 0;
    newace.myace$l_ident = oldace.myace$l_ident;
    rmsts = -1;
    if (!((aclsts = sys$change_acl(0,&type,&fildsc,lcklst,0,0,0)) & 1)) {
      switch (aclsts) {
        case RMS$_FNF: case RMS$_DNF: case SS$_NOSUCHOBJECT:
          set_errno(ENOENT); break;
        case RMS$_DIR:
          set_errno(ENOTDIR); break;
        case RMS$_DEV:
          set_errno(ENODEV); break;
        case RMS$_SYN: case SS$_INVFILFOROP:
          set_errno(EINVAL); break;
        case RMS$_PRV:
          set_errno(EACCES); break;
        default:
          _ckvmssts(aclsts);
      }
      set_vaxc_errno(aclsts);
      PerlMem_free(vmsname);
      return -1;
    }
    /* Grab any existing ACEs with this identifier in case we fail */
    aclsts = fndsts = sys$change_acl(0,&type,&fildsc,findlst,0,0,&cxt);
    if ( fndsts & 1 || fndsts == SS$_ACLEMPTY || fndsts == SS$_NOENTRY
                    || fndsts == SS$_NOMOREACE ) {
      /* Add the new ACE . . . */
      if (!((aclsts = sys$change_acl(0,&type,&fildsc,addlst,0,0,0)) & 1))
        goto yourroom;

      rmsts = rms_erase(vmsname);
      if ($VMS_STATUS_SUCCESS(rmsts)) {
	rmsts = 0;
	}
	else {
	rmsts = -1;
        /* We blew it - dir with files in it, no write priv for
         * parent directory, etc.  Put things back the way they were. */
        if (!((aclsts = sys$change_acl(0,&type,&fildsc,dellst,0,0,0)) & 1))
          goto yourroom;
        if (fndsts & 1) {
          addlst[0].bufadr = &oldace;
          if (!((aclsts = sys$change_acl(0,&type,&fildsc,addlst,0,0,&cxt)) & 1))
            goto yourroom;
        }
      }
    }

    yourroom:
    fndsts = sys$change_acl(0,&type,&fildsc,ulklst,0,0,0);
    /* We just deleted it, so of course it's not there.  Some versions of
     * VMS seem to return success on the unlock operation anyhow (after all
     * the unlock is successful), but others don't.
     */
    if (fndsts == RMS$_FNF || fndsts == SS$_NOSUCHOBJECT) fndsts = SS$_NORMAL;
    if (aclsts & 1) aclsts = fndsts;
    if (!(aclsts & 1)) {
      set_errno(EVMSERR);
      set_vaxc_errno(aclsts);
    }

    PerlMem_free(vmsname);
    return rmsts;

}  /* end of kill_file() */
/*}}}*/


/*{{{int do_rmdir(char *name)*/
int
Perl_do_rmdir(pTHX_ const char *name)
{
    char * dirfile;
    int retval;
    Stat_t st;

    dirfile = PerlMem_malloc(VMS_MAXRSS + 1);
    if (dirfile == NULL)
	_ckvmssts(SS$_INSFMEM);

    /* Force to a directory specification */
    if (do_fileify_dirspec(name, dirfile, 0, NULL) == NULL) {
	PerlMem_free(dirfile);
	return -1;
    }
    if (Perl_flex_lstat(aTHX_ dirfile, &st) || !S_ISDIR(st.st_mode)) {
	errno = ENOTDIR;
	retval = -1;
    }
    else
	retval = mp_do_kill_file(aTHX_ dirfile, 1);

    PerlMem_free(dirfile);
    return retval;

}  /* end of do_rmdir */
/*}}}*/

/* kill_file
 * Delete any file to which user has control access, regardless of whether
 * delete access is explicitly allowed.
 * Limitations: User must have write access to parent directory.
 *              Does not block signals or ASTs; if interrupted in midstream
 *              may leave file with an altered ACL.
 * HANDLE WITH CARE!
 */
/*{{{int kill_file(char *name)*/
int
Perl_kill_file(pTHX_ const char *name)
{
    char rspec[NAM$C_MAXRSS+1];
    char *tspec;
    Stat_t st;
    int rmsts;

   /* Remove() is allowed to delete directories, according to the X/Open
    * specifications.
    * This may need special handling to work with the ACL hacks.
     */
   if ((flex_lstat(name, &st) == 0) && S_ISDIR(st.st_mode)) {
	rmsts = Perl_do_rmdir(aTHX_ name);
	return rmsts;
    }

   rmsts = mp_do_kill_file(aTHX_ name, 0);

    return rmsts;

}  /* end of kill_file() */
/*}}}*/


/*{{{int my_mkdir(char *,Mode_t)*/
int
Perl_my_mkdir(pTHX_ const char *dir, Mode_t mode)
{
  STRLEN dirlen = strlen(dir);

  /* zero length string sometimes gives ACCVIO */
  if (dirlen == 0) return -1;

  /* CRTL mkdir() doesn't tolerate trailing /, since that implies
   * null file name/type.  However, it's commonplace under Unix,
   * so we'll allow it for a gain in portability.
   */
  if (dir[dirlen-1] == '/') {
    char *newdir = savepvn(dir,dirlen-1);
    int ret = mkdir(newdir,mode);
    Safefree(newdir);
    return ret;
  }
  else return mkdir(dir,mode);
}  /* end of my_mkdir */
/*}}}*/

/*{{{int my_chdir(char *)*/
int
Perl_my_chdir(pTHX_ const char *dir)
{
  STRLEN dirlen = strlen(dir);

  /* zero length string sometimes gives ACCVIO */
  if (dirlen == 0) return -1;
  const char *dir1;

  /* Perl is passing the output of the DCL SHOW DEFAULT with leading spaces.
   * This does not work if DECC$EFS_CHARSET is active.  Hack it here
   * so that existing scripts do not need to be changed.
   */
  dir1 = dir;
  while ((dirlen > 0) && (*dir1 == ' ')) {
    dir1++;
    dirlen--;
  }

  /* some versions of CRTL chdir() doesn't tolerate trailing /, since
   * that implies
   * null file name/type.  However, it's commonplace under Unix,
   * so we'll allow it for a gain in portability.
   *
   * - Preview- '/' will be valid soon on VMS
   */
  if ((dirlen > 1) && (dir1[dirlen-1] == '/')) {
    char *newdir = savepvn(dir1,dirlen-1);
    int ret = chdir(newdir);
    Safefree(newdir);
    return ret;
  }
  else return chdir(dir1);
}  /* end of my_chdir */
/*}}}*/


/*{{{int my_chmod(char *, mode_t)*/
int
Perl_my_chmod(pTHX_ const char *file_spec, mode_t mode)
{
  STRLEN speclen = strlen(file_spec);

  /* zero length string sometimes gives ACCVIO */
  if (speclen == 0) return -1;

  /* some versions of CRTL chmod() doesn't tolerate trailing /, since
   * that implies null file name/type.  However, it's commonplace under Unix,
   * so we'll allow it for a gain in portability.
   *
   * Tests are showing that chmod() on VMS 8.3 is only accepting directories
   * in VMS file.dir notation.
   */
  if ((speclen > 1) && (file_spec[speclen-1] == '/')) {
    char *vms_src, *vms_dir, *rslt;
    int ret = -1;
    errno = EIO;

    /* First convert this to a VMS format specification */
    vms_src = PerlMem_malloc(VMS_MAXRSS);
    if (vms_src == NULL)
	_ckvmssts(SS$_INSFMEM);

    rslt = do_tovmsspec(file_spec, vms_src, 0, NULL);
    if (rslt == NULL) {
	/* If we fail, then not a file specification */
	PerlMem_free(vms_src);
	errno = EIO;
	return -1;
    }

    /* Now make it a directory spec so chmod is happy */
    vms_dir = PerlMem_malloc(VMS_MAXRSS + 1);
    if (vms_dir == NULL)
	_ckvmssts(SS$_INSFMEM);
    rslt = do_fileify_dirspec(vms_src, vms_dir, 0, NULL);
    PerlMem_free(vms_src);

    /* Now do it */
    if (rslt != NULL) {
	ret = chmod(vms_dir, mode);
    } else {
	errno = EIO;
    }
    PerlMem_free(vms_dir);
    return ret;
  }
  else return chmod(file_spec, mode);
}  /* end of my_chmod */
/*}}}*/


/*{{{FILE *my_tmpfile()*/
FILE *
my_tmpfile(void)
{
  FILE *fp;
  char *cp;

  if ((fp = tmpfile())) return fp;

  cp = PerlMem_malloc(L_tmpnam+24);
  if (cp == NULL) _ckvmssts_noperl(SS$_INSFMEM);

  if (decc_filename_unix_only == 0)
    strcpy(cp,"Sys$Scratch:");
  else
    strcpy(cp,"/tmp/");
  tmpnam(cp+strlen(cp));
  strcat(cp,".Perltmp");
  fp = fopen(cp,"w+","fop=dlt");
  PerlMem_free(cp);
  return fp;
}
/*}}}*/


#ifndef HOMEGROWN_POSIX_SIGNALS
/*
 * The C RTL's sigaction fails to check for invalid signal numbers so we 
 * help it out a bit.  The docs are correct, but the actual routine doesn't
 * do what the docs say it will.
 */
/*{{{int Perl_my_sigaction (pTHX_ int, const struct sigaction*, struct sigaction*);*/
int
Perl_my_sigaction (pTHX_ int sig, const struct sigaction* act, 
                   struct sigaction* oact)
{
  if (sig == SIGKILL || sig == SIGSTOP || sig == SIGCONT) {
	SETERRNO(EINVAL, SS$_INVARG);
	return -1;
  }
  return sigaction(sig, act, oact);
}
/*}}}*/
#endif

#ifdef KILL_BY_SIGPRC
#include <errnodef.h>

/* We implement our own kill() using the undocumented system service
   sys$sigprc for one of two reasons:

   1.) If the kill() in an older CRTL uses sys$forcex, causing the
   target process to do a sys$exit, which usually can't be handled 
   gracefully...certainly not by Perl and the %SIG{} mechanism.

   2.) If the kill() in the CRTL can't be called from a signal
   handler without disappearing into the ether, i.e., the signal
   it purportedly sends is never trapped. Still true as of VMS 7.3.

   sys$sigprc has the same parameters as sys$forcex, but throws an exception
   in the target process rather than calling sys$exit.

   Note that distinguishing SIGSEGV from SIGBUS requires an extra arg
   on the ACCVIO condition, which sys$sigprc (and sys$forcex) don't
   provide.  On VMS 7.0+ this is taken care of by doing sys$sigprc
   with condition codes C$_SIG0+nsig*8, catching the exception on the 
   target process and resignaling with appropriate arguments.

   But we don't have that VMS 7.0+ exception handler, so if you
   Perl_my_kill(.., SIGSEGV) it will show up as a SIGBUS.  Oh well.

   Also note that SIGTERM is listed in the docs as being "unimplemented",
   yet always seems to be signaled with a VMS condition code of 4 (and
   correctly handled for that code).  So we hardwire it in.

   Unlike the VMS 7.0+ CRTL kill() function, we actually check the signal
   number to see if it's valid.  So Perl_my_kill(pid,0) returns -1 rather
   than signalling with an unrecognized (and unhandled by CRTL) code.
*/

#define _MY_SIG_MAX 28

static unsigned int
Perl_sig_to_vmscondition_int(int sig)
{
    static unsigned int sig_code[_MY_SIG_MAX+1] = 
    {
        0,                  /*  0 ZERO     */
        SS$_HANGUP,         /*  1 SIGHUP   */
        SS$_CONTROLC,       /*  2 SIGINT   */
        SS$_CONTROLY,       /*  3 SIGQUIT  */
        SS$_RADRMOD,        /*  4 SIGILL   */
        SS$_BREAK,          /*  5 SIGTRAP  */
        SS$_OPCCUS,         /*  6 SIGABRT  */
        SS$_COMPAT,         /*  7 SIGEMT   */
#ifdef __VAX                      
        SS$_FLTOVF,         /*  8 SIGFPE VAX */
#else                             
        SS$_HPARITH,        /*  8 SIGFPE AXP */
#endif                            
        SS$_ABORT,          /*  9 SIGKILL  */
        SS$_ACCVIO,         /* 10 SIGBUS   */
        SS$_ACCVIO,         /* 11 SIGSEGV  */
        SS$_BADPARAM,       /* 12 SIGSYS   */
        SS$_NOMBX,          /* 13 SIGPIPE  */
        SS$_ASTFLT,         /* 14 SIGALRM  */
        4,                  /* 15 SIGTERM  */
        0,                  /* 16 SIGUSR1  */
        0,                  /* 17 SIGUSR2  */
        0,                  /* 18 */
        0,                  /* 19 */
        0,                  /* 20 SIGCHLD  */
        0,                  /* 21 SIGCONT  */
        0,                  /* 22 SIGSTOP  */
        0,                  /* 23 SIGTSTP  */
        0,                  /* 24 SIGTTIN  */
        0,                  /* 25 SIGTTOU  */
        0,                  /* 26 */
        0,                  /* 27 */
        0                   /* 28 SIGWINCH  */
    };

#if __VMS_VER >= 60200000
    static int initted = 0;
    if (!initted) {
        initted = 1;
        sig_code[16] = C$_SIGUSR1;
        sig_code[17] = C$_SIGUSR2;
#if __CRTL_VER >= 70000000
        sig_code[20] = C$_SIGCHLD;
#endif
#if __CRTL_VER >= 70300000
        sig_code[28] = C$_SIGWINCH;
#endif
    }
#endif

    if (sig < _SIG_MIN) return 0;
    if (sig > _MY_SIG_MAX) return 0;
    return sig_code[sig];
}

unsigned int
Perl_sig_to_vmscondition(int sig)
{
#ifdef SS$_DEBUG
    if (vms_debug_on_exception != 0)
	lib$signal(SS$_DEBUG);
#endif
    return Perl_sig_to_vmscondition_int(sig);
}


int
Perl_my_kill(int pid, int sig)
{
    dTHX;
    int iss;
    unsigned int code;
    int sys$sigprc(unsigned int *pidadr,
                     struct dsc$descriptor_s *prcname,
                     unsigned int code);

     /* sig 0 means validate the PID */
    /*------------------------------*/
    if (sig == 0) {
	const unsigned long int jpicode = JPI$_PID;
	pid_t ret_pid;
	int status;
        status = lib$getjpi(&jpicode, &pid, NULL, &ret_pid, NULL, NULL);
	if ($VMS_STATUS_SUCCESS(status))
	   return 0;
	switch (status) {
        case SS$_NOSUCHNODE:
        case SS$_UNREACHABLE:
	case SS$_NONEXPR:
	   errno = ESRCH;
	   break;
	case SS$_NOPRIV:
	   errno = EPERM;
	   break;
	default:
	   errno = EVMSERR;
	}
	vaxc$errno=status;
	return -1;
    }

    code = Perl_sig_to_vmscondition_int(sig);

    if (!code) {
	SETERRNO(EINVAL, SS$_BADPARAM);
        return -1;
    }

    /* Fixme: Per official UNIX specification: If pid = 0, or negative then
     * signals are to be sent to multiple processes.
     *  pid = 0 - all processes in group except ones that the system exempts
     *  pid = -1 - all processes except ones that the system exempts
     *  pid = -n - all processes in group (abs(n)) except ... 
     * For now, just report as not supported.
     */

    if (pid <= 0) {
	SETERRNO(ENOTSUP, SS$_UNSUPPORTED);
        return -1;
    }

    iss = sys$sigprc((unsigned int *)&pid,0,code);
    if (iss&1) return 0;

    switch (iss) {
      case SS$_NOPRIV:
        set_errno(EPERM);  break;
      case SS$_NONEXPR:  
      case SS$_NOSUCHNODE:
      case SS$_UNREACHABLE:
        set_errno(ESRCH);  break;
      case SS$_INSFMEM:
        set_errno(ENOMEM); break;
      default:
        _ckvmssts(iss);
        set_errno(EVMSERR);
    } 
    set_vaxc_errno(iss);
 
    return -1;
}
#endif

/* Routine to convert a VMS status code to a UNIX status code.
** More tricky than it appears because of conflicting conventions with
** existing code.
**
** VMS status codes are a bit mask, with the least significant bit set for
** success.
**
** Special UNIX status of EVMSERR indicates that no translation is currently
** available, and programs should check the VMS status code.
**
** Programs compiled with _POSIX_EXIT have a special encoding that requires
** decoding.
*/

#ifndef C_FACILITY_NO
#define C_FACILITY_NO 0x350000
#endif
#ifndef DCL_IVVERB
#define DCL_IVVERB 0x38090
#endif

int Perl_vms_status_to_unix(int vms_status, int child_flag)
{
int facility;
int fac_sp;
int msg_no;
int msg_status;
int unix_status;

  /* Assume the best or the worst */
  if (vms_status & STS$M_SUCCESS)
    unix_status = 0;
  else
    unix_status = EVMSERR;

  msg_status = vms_status & ~STS$M_CONTROL;

  facility = vms_status & STS$M_FAC_NO;
  fac_sp = vms_status & STS$M_FAC_SP;
  msg_no = vms_status & (STS$M_MSG_NO | STS$M_SEVERITY);

  if (((facility == 0) || (fac_sp == 0))  && (child_flag == 0)) {
    switch(msg_no) {
    case SS$_NORMAL:
	unix_status = 0;
	break;
    case SS$_ACCVIO:
	unix_status = EFAULT;
	break;
    case SS$_DEVOFFLINE:
	unix_status = EBUSY;
	break;
    case SS$_CLEARED:
	unix_status = ENOTCONN;
	break;
    case SS$_IVCHAN:
    case SS$_IVLOGNAM:
    case SS$_BADPARAM:
    case SS$_IVLOGTAB:
    case SS$_NOLOGNAM:
    case SS$_NOLOGTAB:
    case SS$_INVFILFOROP:
    case SS$_INVARG:
    case SS$_NOSUCHID:
    case SS$_IVIDENT:
	unix_status = EINVAL;
	break;
    case SS$_UNSUPPORTED:
	unix_status = ENOTSUP;
	break;
    case SS$_FILACCERR:
    case SS$_NOGRPPRV:
    case SS$_NOSYSPRV:
	unix_status = EACCES;
	break;
    case SS$_DEVICEFULL:
	unix_status = ENOSPC;
	break;
    case SS$_NOSUCHDEV:
	unix_status = ENODEV;
	break;
    case SS$_NOSUCHFILE:
    case SS$_NOSUCHOBJECT:
	unix_status = ENOENT;
	break;
    case SS$_ABORT:				    /* Fatal case */
    case ((SS$_ABORT & STS$M_COND_ID) | STS$K_ERROR): /* Error case */
    case ((SS$_ABORT & STS$M_COND_ID) | STS$K_WARNING): /* Warning case */
	unix_status = EINTR;
	break;
    case SS$_BUFFEROVF:
	unix_status = E2BIG;
	break;
    case SS$_INSFMEM:
	unix_status = ENOMEM;
	break;
    case SS$_NOPRIV:
	unix_status = EPERM;
	break;
    case SS$_NOSUCHNODE:
    case SS$_UNREACHABLE:
	unix_status = ESRCH;
	break;
    case SS$_NONEXPR:
	unix_status = ECHILD;
	break;
    default:
	if ((facility == 0) && (msg_no < 8)) {
	  /* These are not real VMS status codes so assume that they are
          ** already UNIX status codes
	  */
	  unix_status = msg_no;
	  break;
	}
    }
  }
  else {
    /* Translate a POSIX exit code to a UNIX exit code */
    if ((facility == C_FACILITY_NO) && ((msg_no & 0xA000) == 0xA000))  {
	unix_status = (msg_no & 0x07F8) >> 3;
    }
    else {

	 /* Documented traditional behavior for handling VMS child exits */
	/*--------------------------------------------------------------*/
	if (child_flag != 0) {

	     /* Success / Informational return 0 */
	    /*----------------------------------*/
	    if (msg_no & STS$K_SUCCESS)
		return 0;

	     /* Warning returns 1 */
	    /*-------------------*/
	    if ((msg_no & (STS$K_ERROR | STS$K_SEVERE)) == 0)
	    	return 1;

	     /* Everything else pass through the severity bits */
	    /*------------------------------------------------*/
	    return (msg_no & STS$M_SEVERITY);
	}

	 /* Normal VMS status to ERRNO mapping attempt */
	/*--------------------------------------------*/
	switch(msg_status) {
	/* case RMS$_EOF: */ /* End of File */
	case RMS$_FNF:	/* File Not Found */
	case RMS$_DNF:	/* Dir Not Found */
		unix_status = ENOENT;
		break;
	case RMS$_RNF:	/* Record Not Found */
		unix_status = ESRCH;
		break;
	case RMS$_DIR:
		unix_status = ENOTDIR;
		break;
	case RMS$_DEV:
		unix_status = ENODEV;
		break;
	case RMS$_IFI:
	case RMS$_FAC:
	case RMS$_ISI:
		unix_status = EBADF;
		break;
	case RMS$_FEX:
		unix_status = EEXIST;
		break;
	case RMS$_SYN:
	case RMS$_FNM:
	case LIB$_INVSTRDES:
	case LIB$_INVARG:
	case LIB$_NOSUCHSYM:
	case LIB$_INVSYMNAM:
	case DCL_IVVERB:
		unix_status = EINVAL;
		break;
	case CLI$_BUFOVF:
	case RMS$_RTB:
	case CLI$_TKNOVF:
	case CLI$_RSLOVF:
		unix_status = E2BIG;
		break;
	case RMS$_PRV:	/* No privilege */
	case RMS$_ACC:	/* ACP file access failed */
	case RMS$_WLK:	/* Device write locked */
		unix_status = EACCES;
		break;
	/* case RMS$_NMF: */  /* No more files */
	}
    }
  }

  return unix_status;
} 

/* Try to guess at what VMS error status should go with a UNIX errno
 * value.  This is hard to do as there could be many possible VMS
 * error statuses that caused the errno value to be set.
 */

int Perl_unix_status_to_vms(int unix_status)
{
int test_unix_status;

     /* Trivial cases first */
    /*---------------------*/
    if (unix_status == EVMSERR)
	return vaxc$errno;

     /* Is vaxc$errno sane? */
    /*---------------------*/
    test_unix_status = Perl_vms_status_to_unix(vaxc$errno, 0);
    if (test_unix_status == unix_status)
	return vaxc$errno;

     /* If way out of range, must be VMS code already */
    /*-----------------------------------------------*/
    if (unix_status > EVMSERR)
	return unix_status;

     /* If out of range, punt */
    /*-----------------------*/
    if (unix_status > __ERRNO_MAX)
	return SS$_ABORT;


     /* Ok, now we have to do it the hard way. */
    /*----------------------------------------*/
    switch(unix_status) {
    case 0:	return SS$_NORMAL;
    case EPERM: return SS$_NOPRIV;
    case ENOENT: return SS$_NOSUCHOBJECT;
    case ESRCH: return SS$_UNREACHABLE;
    case EINTR: return SS$_ABORT;
    /* case EIO: */
    /* case ENXIO:  */
    case E2BIG: return SS$_BUFFEROVF;
    /* case ENOEXEC */
    case EBADF: return RMS$_IFI;
    case ECHILD: return SS$_NONEXPR;
    /* case EAGAIN */
    case ENOMEM: return SS$_INSFMEM;
    case EACCES: return SS$_FILACCERR;
    case EFAULT: return SS$_ACCVIO;
    /* case ENOTBLK */
    case EBUSY: return SS$_DEVOFFLINE;
    case EEXIST: return RMS$_FEX;
    /* case EXDEV */
    case ENODEV: return SS$_NOSUCHDEV;
    case ENOTDIR: return RMS$_DIR;
    /* case EISDIR */
    case EINVAL: return SS$_INVARG;
    /* case ENFILE */
    /* case EMFILE */
    /* case ENOTTY */
    /* case ETXTBSY */
    /* case EFBIG */
    case ENOSPC: return SS$_DEVICEFULL;
    case ESPIPE: return LIB$_INVARG;
    /* case EROFS: */
    /* case EMLINK: */
    /* case EPIPE: */
    /* case EDOM */
    case ERANGE: return LIB$_INVARG;
    /* case EWOULDBLOCK */
    /* case EINPROGRESS */
    /* case EALREADY */
    /* case ENOTSOCK */
    /* case EDESTADDRREQ */
    /* case EMSGSIZE */
    /* case EPROTOTYPE */
    /* case ENOPROTOOPT */
    /* case EPROTONOSUPPORT */
    /* case ESOCKTNOSUPPORT */
    /* case EOPNOTSUPP */
    /* case EPFNOSUPPORT */
    /* case EAFNOSUPPORT */
    /* case EADDRINUSE */
    /* case EADDRNOTAVAIL */
    /* case ENETDOWN */
    /* case ENETUNREACH */
    /* case ENETRESET */
    /* case ECONNABORTED */
    /* case ECONNRESET */
    /* case ENOBUFS */
    /* case EISCONN */
    case ENOTCONN: return SS$_CLEARED;
    /* case ESHUTDOWN */
    /* case ETOOMANYREFS */
    /* case ETIMEDOUT */
    /* case ECONNREFUSED */
    /* case ELOOP */
    /* case ENAMETOOLONG */
    /* case EHOSTDOWN */
    /* case EHOSTUNREACH */
    /* case ENOTEMPTY */
    /* case EPROCLIM */
    /* case EUSERS  */
    /* case EDQUOT  */
    /* case ENOMSG  */
    /* case EIDRM */
    /* case EALIGN */
    /* case ESTALE */
    /* case EREMOTE */
    /* case ENOLCK */
    /* case ENOSYS */
    /* case EFTYPE */
    /* case ECANCELED */
    /* case EFAIL */
    /* case EINPROG */
    case ENOTSUP:
	return SS$_UNSUPPORTED;
    /* case EDEADLK */
    /* case ENWAIT */
    /* case EILSEQ */
    /* case EBADCAT */
    /* case EBADMSG */
    /* case EABANDONED */
    default:
	return SS$_ABORT; /* punt */
    }

  return SS$_ABORT; /* Should not get here */
} 


/* default piping mailbox size */
#define PERL_BUFSIZ        512


static void
create_mbx(pTHX_ unsigned short int *chan, struct dsc$descriptor_s *namdsc)
{
  unsigned long int mbxbufsiz;
  static unsigned long int syssize = 0;
  unsigned long int dviitm = DVI$_DEVNAM;
  char csize[LNM$C_NAMLENGTH+1];
  int sts;

  if (!syssize) {
    unsigned long syiitm = SYI$_MAXBUF;
    /*
     * Get the SYSGEN parameter MAXBUF
     *
     * If the logical 'PERL_MBX_SIZE' is defined
     * use the value of the logical instead of PERL_BUFSIZ, but 
     * keep the size between 128 and MAXBUF.
     *
     */
    _ckvmssts(lib$getsyi(&syiitm, &syssize, 0, 0, 0, 0));
  }

  if (vmstrnenv("PERL_MBX_SIZE", csize, 0, fildev, 0)) {
      mbxbufsiz = atoi(csize);
  } else {
      mbxbufsiz = PERL_BUFSIZ;
  }
  if (mbxbufsiz < 128) mbxbufsiz = 128;
  if (mbxbufsiz > syssize) mbxbufsiz = syssize;

  _ckvmssts(sts = sys$crembx(0,chan,mbxbufsiz,mbxbufsiz,0,0,0));

  _ckvmssts(sts = lib$getdvi(&dviitm, chan, NULL, NULL, namdsc, &namdsc->dsc$w_length));
  namdsc->dsc$a_pointer[namdsc->dsc$w_length] = '\0';

}  /* end of create_mbx() */


/*{{{  my_popen and my_pclose*/

typedef struct _iosb           IOSB;
typedef struct _iosb*         pIOSB;
typedef struct _pipe           Pipe;
typedef struct _pipe*         pPipe;
typedef struct pipe_details    Info;
typedef struct pipe_details*  pInfo;
typedef struct _srqp            RQE;
typedef struct _srqp*          pRQE;
typedef struct _tochildbuf      CBuf;
typedef struct _tochildbuf*    pCBuf;

struct _iosb {
    unsigned short status;
    unsigned short count;
    unsigned long  dvispec;
};

#pragma member_alignment save
#pragma nomember_alignment quadword
struct _srqp {          /* VMS self-relative queue entry */
    unsigned long qptr[2];
};
#pragma member_alignment restore
static RQE  RQE_ZERO = {0,0};

struct _tochildbuf {
    RQE             q;
    int             eof;
    unsigned short  size;
    char            *buf;
};

struct _pipe {
    RQE            free;
    RQE            wait;
    int            fd_out;
    unsigned short chan_in;
    unsigned short chan_out;
    char          *buf;
    unsigned int   bufsize;
    IOSB           iosb;
    IOSB           iosb2;
    int           *pipe_done;
    int            retry;
    int            type;
    int            shut_on_empty;
    int            need_wake;
    pPipe         *home;
    pInfo          info;
    pCBuf          curr;
    pCBuf          curr2;
#if defined(PERL_IMPLICIT_CONTEXT)
    void	    *thx;	    /* Either a thread or an interpreter */
                                    /* pointer, depending on how we're built */
#endif
};


struct pipe_details
{
    pInfo           next;
    PerlIO *fp;  /* file pointer to pipe mailbox */
    int useFILE; /* using stdio, not perlio */
    int pid;   /* PID of subprocess */
    int mode;  /* == 'r' if pipe open for reading */
    int done;  /* subprocess has completed */
    int waiting; /* waiting for completion/closure */
    int             closing;        /* my_pclose is closing this pipe */
    unsigned long   completion;     /* termination status of subprocess */
    pPipe           in;             /* pipe in to sub */
    pPipe           out;            /* pipe out of sub */
    pPipe           err;            /* pipe of sub's sys$error */
    int             in_done;        /* true when in pipe finished */
    int             out_done;
    int             err_done;
    unsigned short  xchan;	    /* channel to debug xterm */
    unsigned short  xchan_valid;    /* channel is assigned */
};

struct exit_control_block
{
    struct exit_control_block *flink;
    unsigned long int	(*exit_routine)();
    unsigned long int arg_count;
    unsigned long int *status_address;
    unsigned long int exit_status;
}; 

typedef struct _closed_pipes    Xpipe;
typedef struct _closed_pipes*  pXpipe;

struct _closed_pipes {
    int             pid;            /* PID of subprocess */
    unsigned long   completion;     /* termination status of subprocess */
};
#define NKEEPCLOSED 50
static Xpipe closed_list[NKEEPCLOSED];
static int   closed_index = 0;
static int   closed_num = 0;

#define RETRY_DELAY     "0 ::0.20"
#define MAX_RETRY              50

static int pipe_ef = 0;          /* first call to safe_popen inits these*/
static unsigned long mypid;
static unsigned long delaytime[2];

static pInfo open_pipes = NULL;
static $DESCRIPTOR(nl_desc, "NL:");

#define PIPE_COMPLETION_WAIT    30  /* seconds, for EOF/FORCEX wait */



static unsigned long int
pipe_exit_routine(pTHX)
{
    pInfo info;
    unsigned long int retsts = SS$_NORMAL, abort = SS$_TIMEOUT;
    int sts, did_stuff, need_eof, j;

   /* 
    * Flush any pending i/o, but since we are in process run-down, be
    * careful about referencing PerlIO structures that may already have
    * been deallocated.  We may not even have an interpreter anymore.
    */
    info = open_pipes;
    while (info) {
        if (info->fp) {
           if (!info->useFILE
             && PL_perlio_fd_refcnt) 
               PerlIO_flush(info->fp);
           else 
               fflush((FILE *)info->fp);
        }
        info = info->next;
    }

    /* 
     next we try sending an EOF...ignore if doesn't work, make sure we
     don't hang
    */
    did_stuff = 0;
    info = open_pipes;

    while (info) {
      int need_eof;
      _ckvmssts_noperl(sys$setast(0));
      if (info->in && !info->in->shut_on_empty) {
        _ckvmssts_noperl(sys$qio(0,info->in->chan_in,IO$_WRITEOF,0,0,0,
                          0, 0, 0, 0, 0, 0));
        info->waiting = 1;
        did_stuff = 1;
      }
      _ckvmssts_noperl(sys$setast(1));
      info = info->next;
    }

    /* wait for EOF to have effect, up to ~ 30 sec [default] */

    for (j = 0; did_stuff && j < PIPE_COMPLETION_WAIT; j++) {
        int nwait = 0;

        info = open_pipes;
        while (info) {
          _ckvmssts_noperl(sys$setast(0));
          if (info->waiting && info->done) 
                info->waiting = 0;
          nwait += info->waiting;
          _ckvmssts_noperl(sys$setast(1));
          info = info->next;
        }
        if (!nwait) break;
        sleep(1);  
    }

    did_stuff = 0;
    info = open_pipes;
    while (info) {
      _ckvmssts_noperl(sys$setast(0));
      if (!info->done) { /* Tap them gently on the shoulder . . .*/
        sts = sys$forcex(&info->pid,0,&abort);
        if (!(sts&1) && sts != SS$_NONEXPR) _ckvmssts_noperl(sts); 
        did_stuff = 1;
      }
      _ckvmssts_noperl(sys$setast(1));
      info = info->next;
    }

    /* again, wait for effect */

    for (j = 0; did_stuff && j < PIPE_COMPLETION_WAIT; j++) {
        int nwait = 0;

        info = open_pipes;
        while (info) {
          _ckvmssts_noperl(sys$setast(0));
          if (info->waiting && info->done) 
                info->waiting = 0;
          nwait += info->waiting;
          _ckvmssts_noperl(sys$setast(1));
          info = info->next;
        }
        if (!nwait) break;
        sleep(1);  
    }

    info = open_pipes;
    while (info) {
      _ckvmssts_noperl(sys$setast(0));
      if (!info->done) {  /* We tried to be nice . . . */
        sts = sys$delprc(&info->pid,0);
        if (!(sts&1) && sts != SS$_NONEXPR) _ckvmssts_noperl(sts); 
        info->done = 1;  /* sys$delprc is as done as we're going to get. */
      }
      _ckvmssts_noperl(sys$setast(1));
      info = info->next;
    }

    while(open_pipes) {
      if ((sts = my_pclose(open_pipes->fp)) == -1) retsts = vaxc$errno;
      else if (!(sts & 1)) retsts = sts;
    }
    return retsts;
}

static struct exit_control_block pipe_exitblock = 
       {(struct exit_control_block *) 0,
        pipe_exit_routine, 0, &pipe_exitblock.exit_status, 0};

static void pipe_mbxtofd_ast(pPipe p);
static void pipe_tochild1_ast(pPipe p);
static void pipe_tochild2_ast(pPipe p);

static void
popen_completion_ast(pInfo info)
{
  pInfo i = open_pipes;
  int iss;
  int sts;
  pXpipe x;

  info->completion &= 0x0FFFFFFF; /* strip off "control" field */
  closed_list[closed_index].pid = info->pid;
  closed_list[closed_index].completion = info->completion;
  closed_index++;
  if (closed_index == NKEEPCLOSED) 
    closed_index = 0;
  closed_num++;

  while (i) {
    if (i == info) break;
    i = i->next;
  }
  if (!i) return;       /* unlinked, probably freed too */

  info->done = TRUE;

/*
    Writing to subprocess ...
            if my_pclose'd: EOF already sent, should shutdown chan_in part of pipe

            chan_out may be waiting for "done" flag, or hung waiting
            for i/o completion to child...cancel the i/o.  This will
            put it into "snarf mode" (done but no EOF yet) that discards
            input.

    Output from subprocess (stdout, stderr) needs to be flushed and
    shut down.   We try sending an EOF, but if the mbx is full the pipe
    routine should still catch the "shut_on_empty" flag, telling it to
    use immediate-style reads so that "mbx empty" -> EOF.


*/
  if (info->in && !info->in_done) {               /* only for mode=w */
        if (info->in->shut_on_empty && info->in->need_wake) {
            info->in->need_wake = FALSE;
            _ckvmssts_noperl(sys$dclast(pipe_tochild2_ast,info->in,0));
        } else {
            _ckvmssts_noperl(sys$cancel(info->in->chan_out));
        }
  }

  if (info->out && !info->out_done) {             /* were we also piping output? */
      info->out->shut_on_empty = TRUE;
      iss = sys$qio(0,info->out->chan_in,IO$_WRITEOF|IO$M_NORSWAIT, 0, 0, 0, 0, 0, 0, 0, 0, 0);
      if (iss == SS$_MBFULL) iss = SS$_NORMAL;
      _ckvmssts_noperl(iss);
  }

  if (info->err && !info->err_done) {        /* we were piping stderr */
        info->err->shut_on_empty = TRUE;
        iss = sys$qio(0,info->err->chan_in,IO$_WRITEOF|IO$M_NORSWAIT, 0, 0, 0, 0, 0, 0, 0, 0, 0);
        if (iss == SS$_MBFULL) iss = SS$_NORMAL;
        _ckvmssts_noperl(iss);
  }
  _ckvmssts_noperl(sys$setef(pipe_ef));

}

static unsigned long int setup_cmddsc(pTHX_ const char *cmd, int check_img, int *suggest_quote, struct dsc$descriptor_s **pvmscmd);
static void vms_execfree(struct dsc$descriptor_s *vmscmd);

/*
    we actually differ from vmstrnenv since we use this to
    get the RMS IFI to check if SYS$OUTPUT and SYS$ERROR *really*
    are pointing to the same thing
*/

static unsigned short
popen_translate(pTHX_ char *logical, char *result)
{
    int iss;
    $DESCRIPTOR(d_table,"LNM$PROCESS_TABLE");
    $DESCRIPTOR(d_log,"");
    struct _il3 {
        unsigned short length;
        unsigned short code;
        char *         buffer_addr;
        unsigned short *retlenaddr;
    } itmlst[2];
    unsigned short l, ifi;

    d_log.dsc$a_pointer = logical;
    d_log.dsc$w_length  = strlen(logical);

    itmlst[0].code = LNM$_STRING;
    itmlst[0].length = 255;
    itmlst[0].buffer_addr = result;
    itmlst[0].retlenaddr = &l;

    itmlst[1].code = 0;
    itmlst[1].length = 0;
    itmlst[1].buffer_addr = 0;
    itmlst[1].retlenaddr = 0;

    iss = sys$trnlnm(0, &d_table, &d_log, 0, itmlst);
    if (iss == SS$_NOLOGNAM) {
        iss = SS$_NORMAL;
        l = 0;
    }
    if (!(iss&1)) lib$signal(iss);
    result[l] = '\0';
/*
    logicals for PPFs have a 4 byte prefix  ESC+NUL+(RMS IFI)
    strip it off and return the ifi, if any
*/
    ifi  = 0;
    if (result[0] == 0x1b && result[1] == 0x00) {
        memmove(&ifi,result+2,2);
        strcpy(result,result+4);
    }
    return ifi;     /* this is the RMS internal file id */
}

static void pipe_infromchild_ast(pPipe p);

/*
    I'm using LIB$(GET|FREE)_VM here so that we can allocate and deallocate
    inside an AST routine without worrying about reentrancy and which Perl
    memory allocator is being used.

    We read data and queue up the buffers, then spit them out one at a
    time to the output mailbox when the output mailbox is ready for one.

*/
#define INITIAL_TOCHILDQUEUE  2

static pPipe
pipe_tochild_setup(pTHX_ char *rmbx, char *wmbx)
{
    pPipe p;
    pCBuf b;
    char mbx1[64], mbx2[64];
    struct dsc$descriptor_s d_mbx1 = {sizeof mbx1, DSC$K_DTYPE_T,
                                      DSC$K_CLASS_S, mbx1},
                            d_mbx2 = {sizeof mbx2, DSC$K_DTYPE_T,
                                      DSC$K_CLASS_S, mbx2};
    unsigned int dviitm = DVI$_DEVBUFSIZ;
    int j, n;

    n = sizeof(Pipe);
    _ckvmssts(lib$get_vm(&n, &p));

    create_mbx(aTHX_ &p->chan_in , &d_mbx1);
    create_mbx(aTHX_ &p->chan_out, &d_mbx2);
    _ckvmssts(lib$getdvi(&dviitm, &p->chan_in, 0, &p->bufsize));

    p->buf           = 0;
    p->shut_on_empty = FALSE;
    p->need_wake     = FALSE;
    p->type          = 0;
    p->retry         = 0;
    p->iosb.status   = SS$_NORMAL;
    p->iosb2.status  = SS$_NORMAL;
    p->free          = RQE_ZERO;
    p->wait          = RQE_ZERO;
    p->curr          = 0;
    p->curr2         = 0;
    p->info          = 0;
#ifdef PERL_IMPLICIT_CONTEXT
    p->thx	     = aTHX;
#endif

    n = sizeof(CBuf) + p->bufsize;

    for (j = 0; j < INITIAL_TOCHILDQUEUE; j++) {
        _ckvmssts(lib$get_vm(&n, &b));
        b->buf = (char *) b + sizeof(CBuf);
        _ckvmssts(lib$insqhi(b, &p->free));
    }

    pipe_tochild2_ast(p);
    pipe_tochild1_ast(p);
    strcpy(wmbx, mbx1);
    strcpy(rmbx, mbx2);
    return p;
}

/*  reads the MBX Perl is writing, and queues */

static void
pipe_tochild1_ast(pPipe p)
{
    pCBuf b = p->curr;
    int iss = p->iosb.status;
    int eof = (iss == SS$_ENDOFFILE);
    int sts;
#ifdef PERL_IMPLICIT_CONTEXT
    pTHX = p->thx;
#endif

    if (p->retry) {
        if (eof) {
            p->shut_on_empty = TRUE;
            b->eof     = TRUE;
            _ckvmssts(sys$dassgn(p->chan_in));
        } else  {
            _ckvmssts(iss);
        }

        b->eof  = eof;
        b->size = p->iosb.count;
        _ckvmssts(sts = lib$insqhi(b, &p->wait));
        if (p->need_wake) {
            p->need_wake = FALSE;
            _ckvmssts(sys$dclast(pipe_tochild2_ast,p,0));
        }
    } else {
        p->retry = 1;   /* initial call */
    }

    if (eof) {                  /* flush the free queue, return when done */
        int n = sizeof(CBuf) + p->bufsize;
        while (1) {
            iss = lib$remqti(&p->free, &b);
            if (iss == LIB$_QUEWASEMP) return;
            _ckvmssts(iss);
            _ckvmssts(lib$free_vm(&n, &b));
        }
    }

    iss = lib$remqti(&p->free, &b);
    if (iss == LIB$_QUEWASEMP) {
        int n = sizeof(CBuf) + p->bufsize;
        _ckvmssts(lib$get_vm(&n, &b));
        b->buf = (char *) b + sizeof(CBuf);
    } else {
       _ckvmssts(iss);
    }

    p->curr = b;
    iss = sys$qio(0,p->chan_in,
             IO$_READVBLK|(p->shut_on_empty ? IO$M_NOWAIT : 0),
             &p->iosb,
             pipe_tochild1_ast, p, b->buf, p->bufsize, 0, 0, 0, 0);
    if (iss == SS$_ENDOFFILE && p->shut_on_empty) iss = SS$_NORMAL;
    _ckvmssts(iss);
}


/* writes queued buffers to output, waits for each to complete before
   doing the next */

static void
pipe_tochild2_ast(pPipe p)
{
    pCBuf b = p->curr2;
    int iss = p->iosb2.status;
    int n = sizeof(CBuf) + p->bufsize;
    int done = (p->info && p->info->done) ||
              iss == SS$_CANCEL || iss == SS$_ABORT;
#if defined(PERL_IMPLICIT_CONTEXT)
    pTHX = p->thx;
#endif

    do {
        if (p->type) {         /* type=1 has old buffer, dispose */
            if (p->shut_on_empty) {
                _ckvmssts(lib$free_vm(&n, &b));
            } else {
                _ckvmssts(lib$insqhi(b, &p->free));
            }
            p->type = 0;
        }

        iss = lib$remqti(&p->wait, &b);
        if (iss == LIB$_QUEWASEMP) {
            if (p->shut_on_empty) {
                if (done) {
                    _ckvmssts(sys$dassgn(p->chan_out));
                    *p->pipe_done = TRUE;
                    _ckvmssts(sys$setef(pipe_ef));
                } else {
                    _ckvmssts(sys$qio(0,p->chan_out,IO$_WRITEOF,
                        &p->iosb2, pipe_tochild2_ast, p, 0, 0, 0, 0, 0, 0));
                }
                return;
            }
            p->need_wake = TRUE;
            return;
        }
        _ckvmssts(iss);
        p->type = 1;
    } while (done);


    p->curr2 = b;
    if (b->eof) {
        _ckvmssts(sys$qio(0,p->chan_out,IO$_WRITEOF,
            &p->iosb2, pipe_tochild2_ast, p, 0, 0, 0, 0, 0, 0));
    } else {
        _ckvmssts(sys$qio(0,p->chan_out,IO$_WRITEVBLK,
            &p->iosb2, pipe_tochild2_ast, p, b->buf, b->size, 0, 0, 0, 0));
    }

    return;

}


static pPipe
pipe_infromchild_setup(pTHX_ char *rmbx, char *wmbx)
{
    pPipe p;
    char mbx1[64], mbx2[64];
    struct dsc$descriptor_s d_mbx1 = {sizeof mbx1, DSC$K_DTYPE_T,
                                      DSC$K_CLASS_S, mbx1},
                            d_mbx2 = {sizeof mbx2, DSC$K_DTYPE_T,
                                      DSC$K_CLASS_S, mbx2};
    unsigned int dviitm = DVI$_DEVBUFSIZ;

    int n = sizeof(Pipe);
    _ckvmssts(lib$get_vm(&n, &p));
    create_mbx(aTHX_ &p->chan_in , &d_mbx1);
    create_mbx(aTHX_ &p->chan_out, &d_mbx2);

    _ckvmssts(lib$getdvi(&dviitm, &p->chan_in, 0, &p->bufsize));
    n = p->bufsize * sizeof(char);
    _ckvmssts(lib$get_vm(&n, &p->buf));
    p->shut_on_empty = FALSE;
    p->info   = 0;
    p->type   = 0;
    p->iosb.status = SS$_NORMAL;
#if defined(PERL_IMPLICIT_CONTEXT)
    p->thx = aTHX;
#endif
    pipe_infromchild_ast(p);

    strcpy(wmbx, mbx1);
    strcpy(rmbx, mbx2);
    return p;
}

static void
pipe_infromchild_ast(pPipe p)
{
    int iss = p->iosb.status;
    int eof = (iss == SS$_ENDOFFILE);
    int myeof = (eof && (p->iosb.dvispec == mypid || p->iosb.dvispec == 0));
    int kideof = (eof && (p->iosb.dvispec == p->info->pid));
#if defined(PERL_IMPLICIT_CONTEXT)
    pTHX = p->thx;
#endif

    if (p->info && p->info->closing && p->chan_out)  {           /* output shutdown */
        _ckvmssts(sys$dassgn(p->chan_out));
        p->chan_out = 0;
    }

    /* read completed:
            input shutdown if EOF from self (done or shut_on_empty)
            output shutdown if closing flag set (my_pclose)
            send data/eof from child or eof from self
            otherwise, re-read (snarf of data from child)
    */

    if (p->type == 1) {
        p->type = 0;
        if (myeof && p->chan_in) {                  /* input shutdown */
            _ckvmssts(sys$dassgn(p->chan_in));
            p->chan_in = 0;
        }

        if (p->chan_out) {
            if (myeof || kideof) {      /* pass EOF to parent */
                _ckvmssts(sys$qio(0,p->chan_out,IO$_WRITEOF, &p->iosb,
                              pipe_infromchild_ast, p,
                              0, 0, 0, 0, 0, 0));
                return;
            } else if (eof) {       /* eat EOF --- fall through to read*/

            } else {                /* transmit data */
                _ckvmssts(sys$qio(0,p->chan_out,IO$_WRITEVBLK,&p->iosb,
                              pipe_infromchild_ast,p,
                              p->buf, p->iosb.count, 0, 0, 0, 0));
                return;
            }
        }
    }

    /*  everything shut? flag as done */

    if (!p->chan_in && !p->chan_out) {
        *p->pipe_done = TRUE;
        _ckvmssts(sys$setef(pipe_ef));
        return;
    }

    /* write completed (or read, if snarfing from child)
            if still have input active,
               queue read...immediate mode if shut_on_empty so we get EOF if empty
            otherwise,
               check if Perl reading, generate EOFs as needed
    */

    if (p->type == 0) {
        p->type = 1;
        if (p->chan_in) {
            iss = sys$qio(0,p->chan_in,IO$_READVBLK|(p->shut_on_empty ? IO$M_NOW : 0),&p->iosb,
                          pipe_infromchild_ast,p,
                          p->buf, p->bufsize, 0, 0, 0, 0);
            if (p->shut_on_empty && iss == SS$_ENDOFFILE) iss = SS$_NORMAL;
            _ckvmssts(iss);
        } else {           /* send EOFs for extra reads */
            p->iosb.status = SS$_ENDOFFILE;
            p->iosb.dvispec = 0;
            _ckvmssts(sys$qio(0,p->chan_out,IO$_SETMODE|IO$M_READATTN,
                      0, 0, 0,
                      pipe_infromchild_ast, p, 0, 0, 0, 0));
        }
    }
}

static pPipe
pipe_mbxtofd_setup(pTHX_ int fd, char *out)
{
    pPipe p;
    char mbx[64];
    unsigned long dviitm = DVI$_DEVBUFSIZ;
    struct stat s;
    struct dsc$descriptor_s d_mbx = {sizeof mbx, DSC$K_DTYPE_T,
                                      DSC$K_CLASS_S, mbx};
    int n = sizeof(Pipe);

    /* things like terminals and mbx's don't need this filter */
    if (fd && fstat(fd,&s) == 0) {
        unsigned long dviitm = DVI$_DEVCHAR, devchar;
	char device[65];
	unsigned short dev_len;
	struct dsc$descriptor_s d_dev;
	char * cptr;
	struct item_list_3 items[3];
	int status;
	unsigned short dvi_iosb[4];

	cptr = getname(fd, out, 1);
	if (cptr == NULL) _ckvmssts(SS$_NOSUCHDEV);
	d_dev.dsc$a_pointer = out;
	d_dev.dsc$w_length = strlen(out);
	d_dev.dsc$b_dtype = DSC$K_DTYPE_T;
	d_dev.dsc$b_class = DSC$K_CLASS_S;

	items[0].len = 4;
	items[0].code = DVI$_DEVCHAR;
	items[0].bufadr = &devchar;
	items[0].retadr = NULL;
	items[1].len = 64;
	items[1].code = DVI$_FULLDEVNAM;
	items[1].bufadr = device;
	items[1].retadr = &dev_len;
	items[2].len = 0;
	items[2].code = 0;

	status = sys$getdviw
	        (NO_EFN, 0, &d_dev, items, dvi_iosb, NULL, NULL, NULL);
	_ckvmssts(status);
	if ($VMS_STATUS_SUCCESS(dvi_iosb[0])) {
	    device[dev_len] = 0;

	    if (!(devchar & DEV$M_DIR)) {
		strcpy(out, device);
		return 0;
	    }
	}
    }

    _ckvmssts(lib$get_vm(&n, &p));
    p->fd_out = dup(fd);
    create_mbx(aTHX_ &p->chan_in, &d_mbx);
    _ckvmssts(lib$getdvi(&dviitm, &p->chan_in, 0, &p->bufsize));
    n = (p->bufsize+1) * sizeof(char);
    _ckvmssts(lib$get_vm(&n, &p->buf));
    p->shut_on_empty = FALSE;
    p->retry = 0;
    p->info  = 0;
    strcpy(out, mbx);

    _ckvmssts(sys$qio(0, p->chan_in, IO$_READVBLK, &p->iosb,
                  pipe_mbxtofd_ast, p,
                  p->buf, p->bufsize, 0, 0, 0, 0));

    return p;
}

static void
pipe_mbxtofd_ast(pPipe p)
{
    int iss = p->iosb.status;
    int done = p->info->done;
    int iss2;
    int eof = (iss == SS$_ENDOFFILE);
    int myeof = eof && ((p->iosb.dvispec == mypid)||(p->iosb.dvispec == 0));
    int err = !(iss&1) && !eof;
#if defined(PERL_IMPLICIT_CONTEXT)
    pTHX = p->thx;
#endif

    if (done && myeof) {               /* end piping */
        close(p->fd_out);
        sys$dassgn(p->chan_in);
        *p->pipe_done = TRUE;
        _ckvmssts(sys$setef(pipe_ef));
        return;
    }

    if (!err && !eof) {             /* good data to send to file */
        p->buf[p->iosb.count] = '\n';
        iss2 = write(p->fd_out, p->buf, p->iosb.count+1);
        if (iss2 < 0) {
            p->retry++;
            if (p->retry < MAX_RETRY) {
                _ckvmssts(sys$setimr(0,delaytime,pipe_mbxtofd_ast,p));
                return;
            }
        }
        p->retry = 0;
    } else if (err) {
        _ckvmssts(iss);
    }


    iss = sys$qio(0, p->chan_in, IO$_READVBLK|(p->shut_on_empty ? IO$M_NOW : 0), &p->iosb,
          pipe_mbxtofd_ast, p,
          p->buf, p->bufsize, 0, 0, 0, 0);
    if (p->shut_on_empty && (iss == SS$_ENDOFFILE)) iss = SS$_NORMAL;
    _ckvmssts(iss);
}


typedef struct _pipeloc     PLOC;
typedef struct _pipeloc*   pPLOC;

struct _pipeloc {
    pPLOC   next;
    char    dir[NAM$C_MAXRSS+1];
};
static pPLOC  head_PLOC = 0;

void
free_pipelocs(pTHX_ void *head)
{
    pPLOC p, pnext;
    pPLOC *pHead = (pPLOC *)head;

    p = *pHead;
    while (p) {
        pnext = p->next;
        PerlMem_free(p);
        p = pnext;
    }
    *pHead = 0;
}

static void
store_pipelocs(pTHX)
{
    int    i;
    pPLOC  p;
    AV    *av = 0;
    SV    *dirsv;
    GV    *gv;
    char  *dir, *x;
    char  *unixdir;
    char  temp[NAM$C_MAXRSS+1];
    STRLEN n_a;

    if (head_PLOC)  
        free_pipelocs(aTHX_ &head_PLOC);

/*  the . directory from @INC comes last */

    p = (pPLOC) PerlMem_malloc(sizeof(PLOC));
    if (p == NULL) _ckvmssts(SS$_INSFMEM);
    p->next = head_PLOC;
    head_PLOC = p;
    strcpy(p->dir,"./");

/*  get the directory from $^X */

    unixdir = PerlMem_malloc(VMS_MAXRSS);
    if (unixdir == NULL) _ckvmssts(SS$_INSFMEM);

#ifdef PERL_IMPLICIT_CONTEXT
    if (aTHX && PL_origargv && PL_origargv[0]) {    /* maybe nul if embedded Perl */
#else
    if (PL_origargv && PL_origargv[0]) {    /* maybe nul if embedded Perl */
#endif
        strcpy(temp, PL_origargv[0]);
        x = strrchr(temp,']');
	if (x == NULL) {
	x = strrchr(temp,'>');
	  if (x == NULL) {
	    /* It could be a UNIX path */
	    x = strrchr(temp,'/');
	  }
	}
	if (x)
	  x[1] = '\0';
	else {
	  /* Got a bare name, so use default directory */
	  temp[0] = '.';
	  temp[1] = '\0';
	}

        if ((tounixpath_utf8(temp, unixdir, NULL)) != NULL) {
            p = (pPLOC) PerlMem_malloc(sizeof(PLOC));
	    if (p == NULL) _ckvmssts(SS$_INSFMEM);
            p->next = head_PLOC;
            head_PLOC = p;
            strncpy(p->dir,unixdir,sizeof(p->dir)-1);
            p->dir[NAM$C_MAXRSS] = '\0';
	}
    }

/*  reverse order of @INC entries, skip "." since entered above */

#ifdef PERL_IMPLICIT_CONTEXT
    if (aTHX)
#endif
    av = PL_includepathav;

    for (i = 0; av && i <= AvFILL(av); i++) {
        dirsv = *av_fetch(av,i,TRUE);

        if (SvROK(dirsv)) continue;
        dir = SvPVx(dirsv,n_a);
        if (strcmp(dir,".") == 0) continue;
        if ((tounixpath_utf8(dir, unixdir, NULL)) == NULL)
            continue;

        p = (pPLOC) PerlMem_malloc(sizeof(PLOC));
        p->next = head_PLOC;
        head_PLOC = p;
        strncpy(p->dir,unixdir,sizeof(p->dir)-1);
        p->dir[NAM$C_MAXRSS] = '\0';
    }

/* most likely spot (ARCHLIB) put first in the list */

#ifdef ARCHLIB_EXP
    if ((tounixpath_utf8(ARCHLIB_EXP, unixdir, NULL)) != NULL) {
        p = (pPLOC) PerlMem_malloc(sizeof(PLOC));
	if (p == NULL) _ckvmssts(SS$_INSFMEM);
        p->next = head_PLOC;
        head_PLOC = p;
        strncpy(p->dir,unixdir,sizeof(p->dir)-1);
        p->dir[NAM$C_MAXRSS] = '\0';
    }
#endif
    PerlMem_free(unixdir);
}

static I32
Perl_cando_by_name_int
   (pTHX_ I32 bit, bool effective, const char *fname, int opts);
#if !defined(PERL_IMPLICIT_CONTEXT)
#define cando_by_name_int		Perl_cando_by_name_int
#else
#define cando_by_name_int(a,b,c,d)	Perl_cando_by_name_int(aTHX_ a,b,c,d)
#endif

static char *
find_vmspipe(pTHX)
{
    static int   vmspipe_file_status = 0;
    static char  vmspipe_file[NAM$C_MAXRSS+1];

    /* already found? Check and use ... need read+execute permission */

    if (vmspipe_file_status == 1) {
        if (cando_by_name_int(S_IRUSR, 0, vmspipe_file, PERL_RMSEXPAND_M_VMS_IN)
         && cando_by_name_int
	   (S_IXUSR, 0, vmspipe_file, PERL_RMSEXPAND_M_VMS_IN)) {
            return vmspipe_file;
        }
        vmspipe_file_status = 0;
    }

    /* scan through stored @INC, $^X */

    if (vmspipe_file_status == 0) {
        char file[NAM$C_MAXRSS+1];
        pPLOC  p = head_PLOC;

        while (p) {
	    char * exp_res;
	    int dirlen;
            strcpy(file, p->dir);
	    dirlen = strlen(file);
            strncat(file, "vmspipe.com",NAM$C_MAXRSS - dirlen);
            file[NAM$C_MAXRSS] = '\0';
            p = p->next;

            exp_res = do_rmsexpand
		(file, vmspipe_file, 0, NULL, PERL_RMSEXPAND_M_VMS, NULL, NULL);
            if (!exp_res) continue;

            if (cando_by_name_int
		(S_IRUSR, 0, vmspipe_file, PERL_RMSEXPAND_M_VMS_IN)
             && cando_by_name_int
		   (S_IXUSR, 0, vmspipe_file, PERL_RMSEXPAND_M_VMS_IN)) {
                vmspipe_file_status = 1;
                return vmspipe_file;
            }
        }
        vmspipe_file_status = -1;   /* failed, use tempfiles */
    }

    return 0;
}

static FILE *
vmspipe_tempfile(pTHX)
{
    char file[NAM$C_MAXRSS+1];
    FILE *fp;
    static int index = 0;
    Stat_t s0, s1;
    int cmp_result;

    /* create a tempfile */

    /* we can't go from   W, shr=get to  R, shr=get without
       an intermediate vulnerable state, so don't bother trying...

       and lib$spawn doesn't shr=put, so have to close the write

       So... match up the creation date/time and the FID to
       make sure we're dealing with the same file

    */

    index++;
    if (!decc_filename_unix_only) {
      sprintf(file,"sys$scratch:perlpipe_%08.8x_%d.com",mypid,index);
      fp = fopen(file,"w");
      if (!fp) {
        sprintf(file,"sys$login:perlpipe_%08.8x_%d.com",mypid,index);
        fp = fopen(file,"w");
        if (!fp) {
            sprintf(file,"sys$disk:[]perlpipe_%08.8x_%d.com",mypid,index);
            fp = fopen(file,"w");
	}
      }
     }
     else {
      sprintf(file,"/tmp/perlpipe_%08.8x_%d.com",mypid,index);
      fp = fopen(file,"w");
      if (!fp) {
	sprintf(file,"/sys$login/perlpipe_%08.8x_%d.com",mypid,index);
	fp = fopen(file,"w");
	if (!fp) {
	  sprintf(file,"./perlpipe_%08.8x_%d.com",mypid,index);
	  fp = fopen(file,"w");
	}
      }
    }
    if (!fp) return 0;  /* we're hosed */

    fprintf(fp,"$! 'f$verify(0)'\n");
    fprintf(fp,"$!  ---  protect against nonstandard definitions ---\n");
    fprintf(fp,"$ perl_cfile  = f$environment(\"procedure\")\n");
    fprintf(fp,"$ perl_define = \"define/nolog\"\n");
    fprintf(fp,"$ perl_on     = \"set noon\"\n");
    fprintf(fp,"$ perl_exit   = \"exit\"\n");
    fprintf(fp,"$ perl_del    = \"delete\"\n");
    fprintf(fp,"$ pif         = \"if\"\n");
    fprintf(fp,"$!  --- define i/o redirection (sys$output set by lib$spawn)\n");
    fprintf(fp,"$ pif perl_popen_in  .nes. \"\" then perl_define/user/name_attributes=confine sys$input  'perl_popen_in'\n");
    fprintf(fp,"$ pif perl_popen_err .nes. \"\" then perl_define/user/name_attributes=confine sys$error  'perl_popen_err'\n");
    fprintf(fp,"$ pif perl_popen_out .nes. \"\" then perl_define      sys$output 'perl_popen_out'\n");
    fprintf(fp,"$!  --- build command line to get max possible length\n");
    fprintf(fp,"$c=perl_popen_cmd0\n"); 
    fprintf(fp,"$c=c+perl_popen_cmd1\n"); 
    fprintf(fp,"$c=c+perl_popen_cmd2\n"); 
    fprintf(fp,"$x=perl_popen_cmd3\n"); 
    fprintf(fp,"$c=c+x\n"); 
    fprintf(fp,"$ perl_on\n");
    fprintf(fp,"$ 'c'\n");
    fprintf(fp,"$ perl_status = $STATUS\n");
    fprintf(fp,"$ perl_del  'perl_cfile'\n");
    fprintf(fp,"$ perl_exit 'perl_status'\n");
    fsync(fileno(fp));

    fgetname(fp, file, 1);
    fstat(fileno(fp), (struct stat *)&s0);
    fclose(fp);

    if (decc_filename_unix_only)
	do_tounixspec(file, file, 0, NULL);
    fp = fopen(file,"r","shr=get");
    if (!fp) return 0;
    fstat(fileno(fp), (struct stat *)&s1);

    cmp_result = VMS_INO_T_COMPARE(s0.crtl_stat.st_ino, s1.crtl_stat.st_ino);
    if ((cmp_result != 0) && (s0.st_ctime != s1.st_ctime))  {
        fclose(fp);
        return 0;
    }

    return fp;
}


static int vms_is_syscommand_xterm(void)
{
    const static struct dsc$descriptor_s syscommand_dsc = 
      { 11, DSC$K_DTYPE_T, DSC$K_CLASS_S, "SYS$COMMAND" };

    const static struct dsc$descriptor_s decwdisplay_dsc = 
      { 12, DSC$K_DTYPE_T, DSC$K_CLASS_S, "DECW$DISPLAY" };

    struct item_list_3 items[2];
    unsigned short dvi_iosb[4];
    unsigned long devchar;
    unsigned long devclass;
    int status;

    /* Very simple check to guess if sys$command is a decterm? */
    /* First see if the DECW$DISPLAY: device exists */
    items[0].len = 4;
    items[0].code = DVI$_DEVCHAR;
    items[0].bufadr = &devchar;
    items[0].retadr = NULL;
    items[1].len = 0;
    items[1].code = 0;

    status = sys$getdviw
	(NO_EFN, 0, &decwdisplay_dsc, items, dvi_iosb, NULL, NULL, NULL);

    if ($VMS_STATUS_SUCCESS(status)) {
        status = dvi_iosb[0];
    }

    if (!$VMS_STATUS_SUCCESS(status)) {
        SETERRNO(EVMSERR, status);
	return -1;
    }

    /* If it does, then for now assume that we are on a workstation */
    /* Now verify that SYS$COMMAND is a terminal */
    /* for creating the debugger DECTerm */

    items[0].len = 4;
    items[0].code = DVI$_DEVCLASS;
    items[0].bufadr = &devclass;
    items[0].retadr = NULL;
    items[1].len = 0;
    items[1].code = 0;

    status = sys$getdviw
	(NO_EFN, 0, &syscommand_dsc, items, dvi_iosb, NULL, NULL, NULL);

    if ($VMS_STATUS_SUCCESS(status)) {
        status = dvi_iosb[0];
    }

    if (!$VMS_STATUS_SUCCESS(status)) {
        SETERRNO(EVMSERR, status);
	return -1;
    }
    else {
	if (devclass == DC$_TERM) {
	    return 0;
	}
    }
    return -1;
}

/* If we are on a DECTerm, we can pretend to fork xterms when requested */
static PerlIO * create_forked_xterm(pTHX_ const char *cmd, const char *mode)
{
    int status;
    int ret_stat;
    char * ret_char;
    char device_name[65];
    unsigned short device_name_len;
    struct dsc$descriptor_s customization_dsc;
    struct dsc$descriptor_s device_name_dsc;
    const char * cptr;
    char * tptr;
    char customization[200];
    char title[40];
    pInfo info = NULL;
    char mbx1[64];
    unsigned short p_chan;
    int n;
    unsigned short iosb[4];
    struct item_list_3 items[2];
    const char * cust_str =
        "DECW$TERMINAL.iconName:\tPerl Dbg\nDECW$TERMINAL.title:\t%40s\n";
    struct dsc$descriptor_s d_mbx1 = {sizeof mbx1, DSC$K_DTYPE_T,
                                          DSC$K_CLASS_S, mbx1};

     /* LIB$FIND_IMAGE_SIGNAL needs a handler */
    /*---------------------------------------*/
    VAXC$ESTABLISH((__vms_handler)lib$sig_to_ret);


    /* Make sure that this is from the Perl debugger */
    ret_char = strstr(cmd," xterm ");
    if (ret_char == NULL)
	return NULL;
    cptr = ret_char + 7;
    ret_char = strstr(cmd,"tty");
    if (ret_char == NULL)
	return NULL;
    ret_char = strstr(cmd,"sleep");
    if (ret_char == NULL)
	return NULL;

    if (decw_term_port == 0) {
	$DESCRIPTOR(filename1_dsc, "DECW$TERMINALSHR12");
	$DESCRIPTOR(filename2_dsc, "DECW$TERMINALSHR");
	$DESCRIPTOR(decw_term_port_dsc, "DECW$TERM_PORT");

       status = lib$find_image_symbol
			       (&filename1_dsc,
				&decw_term_port_dsc,
				(void *)&decw_term_port,
				NULL,
				0);

	/* Try again with the other image name */
	if (!$VMS_STATUS_SUCCESS(status)) {

           status = lib$find_image_symbol
			       (&filename2_dsc,
				&decw_term_port_dsc,
				(void *)&decw_term_port,
				NULL,
				0);

	}

    }


    /* No decw$term_port, give it up */
    if (!$VMS_STATUS_SUCCESS(status))
	return NULL;

    /* Are we on a workstation? */
    /* to do: capture the rows / columns and pass their properties */
    ret_stat = vms_is_syscommand_xterm();
    if (ret_stat < 0)
	return NULL;

    /* Make the title: */
    ret_char = strstr(cptr,"-title");
    if (ret_char != NULL) {
	while ((*cptr != 0) && (*cptr != '\"')) {
	    cptr++;
	}
	if (*cptr == '\"')
	    cptr++;
	n = 0;
	while ((*cptr != 0) && (*cptr != '\"')) {
	    title[n] = *cptr;
	    n++;
	    if (n == 39) {
		title[39] == 0;
		break;
	    }
	    cptr++;
	}
	title[n] = 0;
    }
    else {
	    /* Default title */
	    strcpy(title,"Perl Debug DECTerm");
    }
    sprintf(customization, cust_str, title);

    customization_dsc.dsc$a_pointer = customization;
    customization_dsc.dsc$w_length = strlen(customization);
    customization_dsc.dsc$b_dtype = DSC$K_DTYPE_T;
    customization_dsc.dsc$b_class = DSC$K_CLASS_S;

    device_name_dsc.dsc$a_pointer = device_name;
    device_name_dsc.dsc$w_length = sizeof device_name -1;
    device_name_dsc.dsc$b_dtype = DSC$K_DTYPE_T;
    device_name_dsc.dsc$b_class = DSC$K_CLASS_S;

    device_name_len = 0;

    /* Try to create the window */
     status = (*decw_term_port)
       (NULL,
	NULL,
	&customization_dsc,
	&device_name_dsc,
	&device_name_len,
	NULL,
	NULL,
	NULL);
    if (!$VMS_STATUS_SUCCESS(status)) {
        SETERRNO(EVMSERR, status);
	return NULL;
    }

    device_name[device_name_len] = '\0';

    /* Need to set this up to look like a pipe for cleanup */
    n = sizeof(Info);
    status = lib$get_vm(&n, &info);
    if (!$VMS_STATUS_SUCCESS(status)) {
        SETERRNO(ENOMEM, status);
        return NULL;
    }

    info->mode = *mode;
    info->done = FALSE;
    info->completion = 0;
    info->closing    = FALSE;
    info->in         = 0;
    info->out        = 0;
    info->err        = 0;
    info->fp         = NULL;
    info->useFILE    = 0;
    info->waiting    = 0;
    info->in_done    = TRUE;
    info->out_done   = TRUE;
    info->err_done   = TRUE;

    /* Assign a channel on this so that it will persist, and not login */
    /* We stash this channel in the info structure for reference. */
    /* The created xterm self destructs when the last channel is removed */
    /* and it appears that perl5db.pl (perl debugger) does this routinely */
    /* So leave this assigned. */
    device_name_dsc.dsc$w_length = device_name_len;
    status = sys$assign(&device_name_dsc,&info->xchan,0,0);
    if (!$VMS_STATUS_SUCCESS(status)) {
        SETERRNO(EVMSERR, status);
	return NULL;
    }
    info->xchan_valid = 1;

    /* Now create a mailbox to be read by the application */

    create_mbx(aTHX_ &p_chan, &d_mbx1);

    /* write the name of the created terminal to the mailbox */
    status = sys$qiow(NO_EFN, p_chan, IO$_WRITEVBLK|IO$M_NOW,
            iosb, NULL, NULL, device_name, device_name_len, 0, 0, 0, 0);

    if (!$VMS_STATUS_SUCCESS(status)) {
        SETERRNO(EVMSERR, status);
	return NULL;
    }

    info->fp  = PerlIO_open(mbx1, mode);

    /* Done with this channel */
    sys$dassgn(p_chan);

    /* If any errors, then clean up */
    if (!info->fp) {
       	n = sizeof(Info);
	_ckvmssts(lib$free_vm(&n, &info));
	return NULL;
        }

    /* All done */
    return info->fp;
}

static PerlIO *
safe_popen(pTHX_ const char *cmd, const char *in_mode, int *psts)
{
    static int handler_set_up = FALSE;
    unsigned long int sts, flags = CLI$M_NOWAIT;
    /* The use of a GLOBAL table (as was done previously) rendered
     * Perl's qx() or `` unusable from a C<$ SET SYMBOL/SCOPE=NOGLOBAL> DCL
     * environment.  Hence we've switched to LOCAL symbol table.
     */
    unsigned int table = LIB$K_CLI_LOCAL_SYM;
    int j, wait = 0, n;
    char *p, mode[10], symbol[MAX_DCL_SYMBOL+1], *vmspipe;
    char *in, *out, *err, mbx[512];
    FILE *tpipe = 0;
    char tfilebuf[NAM$C_MAXRSS+1];
    pInfo info = NULL;
    char cmd_sym_name[20];
    struct dsc$descriptor_s d_symbol= {0, DSC$K_DTYPE_T,
                                      DSC$K_CLASS_S, symbol};
    struct dsc$descriptor_s vmspipedsc = {0, DSC$K_DTYPE_T,
                                      DSC$K_CLASS_S, 0};
    struct dsc$descriptor_s d_sym_cmd = {0, DSC$K_DTYPE_T,
                                      DSC$K_CLASS_S, cmd_sym_name};
    struct dsc$descriptor_s *vmscmd;
    $DESCRIPTOR(d_sym_in ,"PERL_POPEN_IN");
    $DESCRIPTOR(d_sym_out,"PERL_POPEN_OUT");
    $DESCRIPTOR(d_sym_err,"PERL_POPEN_ERR");

    /* Check here for Xterm create request.  This means looking for
     * "3>&1 xterm\b" and "\btty 1>&3\b$" in the command, and that it
     *  is possible to create an xterm.
     */
    if (*in_mode == 'r') {
        PerlIO * xterm_fd;

	xterm_fd = create_forked_xterm(aTHX_ cmd, in_mode);
	if (xterm_fd != NULL)
	    return xterm_fd;
    }

    if (!head_PLOC) store_pipelocs(aTHX);   /* at least TRY to use a static vmspipe file */

    /* once-per-program initialization...
       note that the SETAST calls and the dual test of pipe_ef
       makes sure that only the FIRST thread through here does
       the initialization...all other threads wait until it's
       done.

       Yeah, uglier than a pthread call, it's got all the stuff inline
       rather than in a separate routine.
    */

    if (!pipe_ef) {
        _ckvmssts(sys$setast(0));
        if (!pipe_ef) {
            unsigned long int pidcode = JPI$_PID;
            $DESCRIPTOR(d_delay, RETRY_DELAY);
            _ckvmssts(lib$get_ef(&pipe_ef));
            _ckvmssts(lib$getjpi(&pidcode,0,0,&mypid,0,0));
            _ckvmssts(sys$bintim(&d_delay, delaytime));
        }
        if (!handler_set_up) {
          _ckvmssts(sys$dclexh(&pipe_exitblock));
          handler_set_up = TRUE;
        }
        _ckvmssts(sys$setast(1));
    }

    /* see if we can find a VMSPIPE.COM */

    tfilebuf[0] = '@';
    vmspipe = find_vmspipe(aTHX);
    if (vmspipe) {
        strcpy(tfilebuf+1,vmspipe);
    } else {        /* uh, oh...we're in tempfile hell */
        tpipe = vmspipe_tempfile(aTHX);
        if (!tpipe) {       /* a fish popular in Boston */
            if (ckWARN(WARN_PIPE)) {
                Perl_warner(aTHX_ packWARN(WARN_PIPE),"unable to find VMSPIPE.COM for i/o piping");
            }
        return NULL;
        }
        fgetname(tpipe,tfilebuf+1,1);
    }
    vmspipedsc.dsc$a_pointer = tfilebuf;
    vmspipedsc.dsc$w_length  = strlen(tfilebuf);

    sts = setup_cmddsc(aTHX_ cmd,0,0,&vmscmd);
    if (!(sts & 1)) { 
      switch (sts) {
        case RMS$_FNF:  case RMS$_DNF:
          set_errno(ENOENT); break;
        case RMS$_DIR:
          set_errno(ENOTDIR); break;
        case RMS$_DEV:
          set_errno(ENODEV); break;
        case RMS$_PRV:
          set_errno(EACCES); break;
        case RMS$_SYN:
          set_errno(EINVAL); break;
        case CLI$_BUFOVF: case RMS$_RTB: case CLI$_TKNOVF: case CLI$_RSLOVF:
          set_errno(E2BIG); break;
        case LIB$_INVARG: case LIB$_INVSTRDES: case SS$_ACCVIO: /* shouldn't happen */
          _ckvmssts(sts); /* fall through */
        default:  /* SS$_DUPLNAM, SS$_CLI, resource exhaustion, etc. */
          set_errno(EVMSERR); 
      }
      set_vaxc_errno(sts);
      if (*in_mode != 'n' && ckWARN(WARN_PIPE)) {
        Perl_warner(aTHX_ packWARN(WARN_PIPE),"Can't pipe \"%*s\": %s", strlen(cmd), cmd, Strerror(errno));
      }
      *psts = sts;
      return NULL; 
    }
    n = sizeof(Info);
    _ckvmssts(lib$get_vm(&n, &info));
        
    strcpy(mode,in_mode);
    info->mode = *mode;
    info->done = FALSE;
    info->completion = 0;
    info->closing    = FALSE;
    info->in         = 0;
    info->out        = 0;
    info->err        = 0;
    info->fp         = NULL;
    info->useFILE    = 0;
    info->waiting    = 0;
    info->in_done    = TRUE;
    info->out_done   = TRUE;
    info->err_done   = TRUE;
    info->xchan      = 0;
    info->xchan_valid = 0;

    in = PerlMem_malloc(VMS_MAXRSS);
    if (in == NULL) _ckvmssts(SS$_INSFMEM);
    out = PerlMem_malloc(VMS_MAXRSS);
    if (out == NULL) _ckvmssts(SS$_INSFMEM);
    err = PerlMem_malloc(VMS_MAXRSS);
    if (err == NULL) _ckvmssts(SS$_INSFMEM);

    in[0] = out[0] = err[0] = '\0';

    if ((p = strchr(mode,'F')) != NULL) {   /* F -> use FILE* */
        info->useFILE = 1;
        strcpy(p,p+1);
    }
    if ((p = strchr(mode,'W')) != NULL) {   /* W -> wait for completion */
        wait = 1;
        strcpy(p,p+1);
    }

    if (*mode == 'r') {             /* piping from subroutine */

        info->out = pipe_infromchild_setup(aTHX_ mbx,out);
        if (info->out) {
            info->out->pipe_done = &info->out_done;
            info->out_done = FALSE;
            info->out->info = info;
        }
        if (!info->useFILE) {
	    info->fp  = PerlIO_open(mbx, mode);
        } else {
            info->fp = (PerlIO *) freopen(mbx, mode, stdin);
            Perl_vmssetuserlnm(aTHX_ "SYS$INPUT",mbx);
        }

        if (!info->fp && info->out) {
            sys$cancel(info->out->chan_out);
        
            while (!info->out_done) {
                int done;
                _ckvmssts(sys$setast(0));
                done = info->out_done;
                if (!done) _ckvmssts(sys$clref(pipe_ef));
                _ckvmssts(sys$setast(1));
                if (!done) _ckvmssts(sys$waitfr(pipe_ef));
            }

            if (info->out->buf) {
                n = info->out->bufsize * sizeof(char);
                _ckvmssts(lib$free_vm(&n, &info->out->buf));
            }
            n = sizeof(Pipe);
            _ckvmssts(lib$free_vm(&n, &info->out));
            n = sizeof(Info);
            _ckvmssts(lib$free_vm(&n, &info));
            *psts = RMS$_FNF;
            return NULL;
        }

        info->err = pipe_mbxtofd_setup(aTHX_ fileno(stderr), err);
        if (info->err) {
            info->err->pipe_done = &info->err_done;
            info->err_done = FALSE;
            info->err->info = info;
        }

    } else if (*mode == 'w') {      /* piping to subroutine */

        info->out = pipe_mbxtofd_setup(aTHX_ fileno(stdout), out);
        if (info->out) {
            info->out->pipe_done = &info->out_done;
            info->out_done = FALSE;
            info->out->info = info;
        }

        info->err = pipe_mbxtofd_setup(aTHX_ fileno(stderr), err);
        if (info->err) {
            info->err->pipe_done = &info->err_done;
            info->err_done = FALSE;
            info->err->info = info;
        }

        info->in = pipe_tochild_setup(aTHX_ in,mbx);
        if (!info->useFILE) {
	    info->fp  = PerlIO_open(mbx, mode);
        } else {
            info->fp = (PerlIO *) freopen(mbx, mode, stdout);
            Perl_vmssetuserlnm(aTHX_ "SYS$OUTPUT",mbx);
        }

        if (info->in) {
            info->in->pipe_done = &info->in_done;
            info->in_done = FALSE;
            info->in->info = info;
        }

        /* error cleanup */
        if (!info->fp && info->in) {
            info->done = TRUE;
            _ckvmssts(sys$qiow(0,info->in->chan_in, IO$_WRITEOF, 0,
                              0, 0, 0, 0, 0, 0, 0, 0));

            while (!info->in_done) {
                int done;
                _ckvmssts(sys$setast(0));
                done = info->in_done;
                if (!done) _ckvmssts(sys$clref(pipe_ef));
                _ckvmssts(sys$setast(1));
                if (!done) _ckvmssts(sys$waitfr(pipe_ef));
            }

            if (info->in->buf) {
                n = info->in->bufsize * sizeof(char);
                _ckvmssts(lib$free_vm(&n, &info->in->buf));
            }
            n = sizeof(Pipe);
            _ckvmssts(lib$free_vm(&n, &info->in));
            n = sizeof(Info);
            _ckvmssts(lib$free_vm(&n, &info));
            *psts = RMS$_FNF;
            return NULL;
        }
        

    } else if (*mode == 'n') {       /* separate subprocess, no Perl i/o */
        info->out = pipe_mbxtofd_setup(aTHX_ fileno(stdout), out);
        if (info->out) {
            info->out->pipe_done = &info->out_done;
            info->out_done = FALSE;
            info->out->info = info;
        }

        info->err = pipe_mbxtofd_setup(aTHX_ fileno(stderr), err);
        if (info->err) {
            info->err->pipe_done = &info->err_done;
            info->err_done = FALSE;
            info->err->info = info;
        }
    }

    symbol[MAX_DCL_SYMBOL] = '\0';

    strncpy(symbol, in, MAX_DCL_SYMBOL);
    d_symbol.dsc$w_length = strlen(symbol);
    _ckvmssts(lib$set_symbol(&d_sym_in, &d_symbol, &table));

    strncpy(symbol, err, MAX_DCL_SYMBOL);
    d_symbol.dsc$w_length = strlen(symbol);
    _ckvmssts(lib$set_symbol(&d_sym_err, &d_symbol, &table));

    strncpy(symbol, out, MAX_DCL_SYMBOL);
    d_symbol.dsc$w_length = strlen(symbol);
    _ckvmssts(lib$set_symbol(&d_sym_out, &d_symbol, &table));

    /* Done with the names for the pipes */
    PerlMem_free(err);
    PerlMem_free(out);
    PerlMem_free(in);

    p = vmscmd->dsc$a_pointer;
    while (*p == ' ' || *p == '\t') p++;        /* remove leading whitespace */
    if (*p == '$') p++;                         /* remove leading $ */
    while (*p == ' ' || *p == '\t') p++;

    for (j = 0; j < 4; j++) {
        sprintf(cmd_sym_name,"PERL_POPEN_CMD%d",j);
        d_sym_cmd.dsc$w_length = strlen(cmd_sym_name);

    strncpy(symbol, p, MAX_DCL_SYMBOL);
    d_symbol.dsc$w_length = strlen(symbol);
    _ckvmssts(lib$set_symbol(&d_sym_cmd, &d_symbol, &table));

        if (strlen(p) > MAX_DCL_SYMBOL) {
            p += MAX_DCL_SYMBOL;
        } else {
            p += strlen(p);
        }
    }
    _ckvmssts(sys$setast(0));
    info->next=open_pipes;  /* prepend to list */
    open_pipes=info;
    _ckvmssts(sys$setast(1));
    /* Omit arg 2 (input file) so the child will get the parent's SYS$INPUT
     * and SYS$COMMAND.  vmspipe.com will redefine SYS$INPUT, but we'll still
     * have SYS$COMMAND if we need it.
     */
    _ckvmssts(lib$spawn(&vmspipedsc, 0, &nl_desc, &flags,
                      0, &info->pid, &info->completion,
                      0, popen_completion_ast,info,0,0,0));

    /* if we were using a tempfile, close it now */

    if (tpipe) fclose(tpipe);

    /* once the subprocess is spawned, it has copied the symbols and
       we can get rid of ours */

    for (j = 0; j < 4; j++) {
        sprintf(cmd_sym_name,"PERL_POPEN_CMD%d",j);
        d_sym_cmd.dsc$w_length = strlen(cmd_sym_name);
    _ckvmssts(lib$delete_symbol(&d_sym_cmd, &table));
    }
    _ckvmssts(lib$delete_symbol(&d_sym_in,  &table));
    _ckvmssts(lib$delete_symbol(&d_sym_err, &table));
    _ckvmssts(lib$delete_symbol(&d_sym_out, &table));
    vms_execfree(vmscmd);
        
#ifdef PERL_IMPLICIT_CONTEXT
    if (aTHX) 
#endif
    PL_forkprocess = info->pid;

    if (wait) {
         int done = 0;
         while (!done) {
             _ckvmssts(sys$setast(0));
             done = info->done;
             if (!done) _ckvmssts(sys$clref(pipe_ef));
             _ckvmssts(sys$setast(1));
             if (!done) _ckvmssts(sys$waitfr(pipe_ef));
         }
        *psts = info->completion;
/* Caller thinks it is open and tries to close it. */
/* This causes some problems, as it changes the error status */
/*        my_pclose(info->fp); */
    } else { 
        *psts = info->pid;
    }
    return info->fp;
}  /* end of safe_popen */


/*{{{  PerlIO *my_popen(char *cmd, char *mode)*/
PerlIO *
Perl_my_popen(pTHX_ const char *cmd, const char *mode)
{
    int sts;
    PERL_FLUSHALL_FOR_CHILD;
    return safe_popen(aTHX_ cmd,mode,&sts);
}

/*}}}*/

/*{{{  I32 my_pclose(PerlIO *fp)*/
I32 Perl_my_pclose(pTHX_ PerlIO *fp)
{
    pInfo info, last = NULL;
    unsigned long int retsts;
    int done, iss, n;
    int status;
    
    for (info = open_pipes; info != NULL; last = info, info = info->next)
        if (info->fp == fp) break;

    if (info == NULL) {  /* no such pipe open */
      set_errno(ECHILD); /* quoth POSIX */
      set_vaxc_errno(SS$_NONEXPR);
      return -1;
    }

    /* If we were writing to a subprocess, insure that someone reading from
     * the mailbox gets an EOF.  It looks like a simple fclose() doesn't
     * produce an EOF record in the mailbox.
     *
     *  well, at least sometimes it *does*, so we have to watch out for
     *  the first EOF closing the pipe (and DASSGN'ing the channel)...
     */
     if (info->fp) {
        if (!info->useFILE
          && PL_perlio_fd_refcnt) 
            PerlIO_flush(info->fp);
        else 
            fflush((FILE *)info->fp);
    }

    _ckvmssts(sys$setast(0));
     info->closing = TRUE;
     done = info->done && info->in_done && info->out_done && info->err_done;
     /* hanging on write to Perl's input? cancel it */
     if (info->mode == 'r' && info->out && !info->out_done) {
        if (info->out->chan_out) {
            _ckvmssts(sys$cancel(info->out->chan_out));
            if (!info->out->chan_in) {   /* EOF generation, need AST */
                _ckvmssts(sys$dclast(pipe_infromchild_ast,info->out,0));
            }
        }
     }
     if (info->in && !info->in_done && !info->in->shut_on_empty)  /* EOF if hasn't had one yet */
         _ckvmssts(sys$qio(0,info->in->chan_in,IO$_WRITEOF,0,0,0,
                           0, 0, 0, 0, 0, 0));
    _ckvmssts(sys$setast(1));
    if (info->fp) {
     if (!info->useFILE
         && PL_perlio_fd_refcnt) 
        PerlIO_close(info->fp);
     else 
        fclose((FILE *)info->fp);
    }
     /*
        we have to wait until subprocess completes, but ALSO wait until all
        the i/o completes...otherwise we'll be freeing the "info" structure
        that the i/o ASTs could still be using...
     */

     while (!done) {
         _ckvmssts(sys$setast(0));
         done = info->done && info->in_done && info->out_done && info->err_done;
         if (!done) _ckvmssts(sys$clref(pipe_ef));
         _ckvmssts(sys$setast(1));
         if (!done) _ckvmssts(sys$waitfr(pipe_ef));
     }
     retsts = info->completion;

    /* remove from list of open pipes */
    _ckvmssts(sys$setast(0));
    if (last) last->next = info->next;
    else open_pipes = info->next;
    _ckvmssts(sys$setast(1));

    /* free buffers and structures */

    if (info->in) {
        if (info->in->buf) {
            n = info->in->bufsize * sizeof(char);
            _ckvmssts(lib$free_vm(&n, &info->in->buf));
        }
        n = sizeof(Pipe);
        _ckvmssts(lib$free_vm(&n, &info->in));
    }
    if (info->out) {
        if (info->out->buf) {
            n = info->out->bufsize * sizeof(char);
            _ckvmssts(lib$free_vm(&n, &info->out->buf));
        }
        n = sizeof(Pipe);
        _ckvmssts(lib$free_vm(&n, &info->out));
    }
    if (info->err) {
        if (info->err->buf) {
            n = info->err->bufsize * sizeof(char);
            _ckvmssts(lib$free_vm(&n, &info->err->buf));
        }
        n = sizeof(Pipe);
        _ckvmssts(lib$free_vm(&n, &info->err));
    }
    n = sizeof(Info);
    _ckvmssts(lib$free_vm(&n, &info));

    return retsts;

}  /* end of my_pclose() */

#if defined(__CRTL_VER) && __CRTL_VER >= 70200000
  /* Roll our own prototype because we want this regardless of whether
   * _VMS_WAIT is defined.
   */
  __pid_t __vms_waitpid( __pid_t __pid, int *__stat_loc, int __options );
#endif
/* sort-of waitpid; special handling of pipe clean-up for subprocesses 
   created with popen(); otherwise partially emulate waitpid() unless 
   we have a suitable one from the CRTL that came with VMS 7.2 and later.
   Also check processes not considered by the CRTL waitpid().
 */
/*{{{Pid_t my_waitpid(Pid_t pid, int *statusp, int flags)*/
Pid_t
Perl_my_waitpid(pTHX_ Pid_t pid, int *statusp, int flags)
{
    pInfo info;
    int done;
    int sts;
    int j;
    
    if (statusp) *statusp = 0;
    
    for (info = open_pipes; info != NULL; info = info->next)
        if (info->pid == pid) break;

    if (info != NULL) {  /* we know about this child */
      while (!info->done) {
          _ckvmssts(sys$setast(0));
          done = info->done;
          if (!done) _ckvmssts(sys$clref(pipe_ef));
          _ckvmssts(sys$setast(1));
          if (!done) _ckvmssts(sys$waitfr(pipe_ef));
      }

      if (statusp) *statusp = info->completion;
      return pid;
    }

    /* child that already terminated? */

    for (j = 0; j < NKEEPCLOSED && j < closed_num; j++) {
        if (closed_list[j].pid == pid) {
            if (statusp) *statusp = closed_list[j].completion;
            return pid;
        }
    }

    /* fall through if this child is not one of our own pipe children */

#if defined(__CRTL_VER) && __CRTL_VER >= 70200000

      /* waitpid() became available in the CRTL as of VMS 7.0, but only
       * in 7.2 did we get a version that fills in the VMS completion
       * status as Perl has always tried to do.
       */

      sts = __vms_waitpid( pid, statusp, flags );

      if ( sts == 0 || !(sts == -1 && errno == ECHILD) ) 
         return sts;

      /* If the real waitpid tells us the child does not exist, we 
       * fall through here to implement waiting for a child that 
       * was created by some means other than exec() (say, spawned
       * from DCL) or to wait for a process that is not a subprocess 
       * of the current process.
       */

#endif /* defined(__CRTL_VER) && __CRTL_VER >= 70200000 */

    {
      $DESCRIPTOR(intdsc,"0 00:00:01");
      unsigned long int ownercode = JPI$_OWNER, ownerpid;
      unsigned long int pidcode = JPI$_PID, mypid;
      unsigned long int interval[2];
      unsigned int jpi_iosb[2];
      struct itmlst_3 jpilist[2] = { 
          {sizeof(ownerpid),        JPI$_OWNER, &ownerpid,        0},
          {                      0,         0,                 0, 0} 
      };

      if (pid <= 0) {
        /* Sorry folks, we don't presently implement rooting around for 
           the first child we can find, and we definitely don't want to
           pass a pid of -1 to $getjpi, where it is a wildcard operation.
         */
        set_errno(ENOTSUP); 
        return -1;
      }

      /* Get the owner of the child so I can warn if it's not mine. If the 
       * process doesn't exist or I don't have the privs to look at it, 
       * I can go home early.
       */
      sts = sys$getjpiw(0,&pid,NULL,&jpilist,&jpi_iosb,NULL,NULL);
      if (sts & 1) sts = jpi_iosb[0];
      if (!(sts & 1)) {
        switch (sts) {
            case SS$_NONEXPR:
                set_errno(ECHILD);
                break;
            case SS$_NOPRIV:
                set_errno(EACCES);
                break;
            default:
                _ckvmssts(sts);
        }
        set_vaxc_errno(sts);
        return -1;
      }

      if (ckWARN(WARN_EXEC)) {
        /* remind folks they are asking for non-standard waitpid behavior */
        _ckvmssts(lib$getjpi(&pidcode,0,0,&mypid,0,0));
        if (ownerpid != mypid)
          Perl_warner(aTHX_ packWARN(WARN_EXEC),
                      "waitpid: process %x is not a child of process %x",
                      pid,mypid);
      }

      /* simply check on it once a second until it's not there anymore. */

      _ckvmssts(sys$bintim(&intdsc,interval));
      while ((sts=lib$getjpi(&ownercode,&pid,0,&ownerpid,0,0)) & 1) {
            _ckvmssts(sys$schdwk(0,0,interval,0));
            _ckvmssts(sys$hiber());
      }
      if (sts == SS$_NONEXPR) sts = SS$_NORMAL;

      _ckvmssts(sts);
      return pid;
    }
}  /* end of waitpid() */
/*}}}*/
/*}}}*/
/*}}}*/

/*{{{ char *my_gconvert(double val, int ndig, int trail, char *buf) */
char *
my_gconvert(double val, int ndig, int trail, char *buf)
{
  static char __gcvtbuf[DBL_DIG+1];
  char *loc;

  loc = buf ? buf : __gcvtbuf;

#ifndef __DECC  /* VAXCRTL gcvt uses E format for numbers < 1 */
  if (val < 1) {
    sprintf(loc,"%.*g",ndig,val);
    return loc;
  }
#endif

  if (val) {
    if (!buf && ndig > DBL_DIG) ndig = DBL_DIG;
    return gcvt(val,ndig,loc);
  }
  else {
    loc[0] = '0'; loc[1] = '\0';
    return loc;
  }

}
/*}}}*/

#if defined(__VAX) || !defined(NAML$C_MAXRSS)
static int rms_free_search_context(struct FAB * fab)
{
struct NAM * nam;

    nam = fab->fab$l_nam;
    nam->nam$b_nop |= NAM$M_SYNCHK;
    nam->nam$l_rlf = NULL;
    fab->fab$b_dns = 0;
    return sys$parse(fab, NULL, NULL);
}

#define rms_setup_nam(nam) struct NAM nam = cc$rms_nam
#define rms_clear_nam_nop(nam) nam.nam$b_nop = 0;
#define rms_set_nam_nop(nam, opt) nam.nam$b_nop |= (opt)
#define rms_set_nam_fnb(nam, opt) nam.nam$l_fnb |= (opt)
#define rms_is_nam_fnb(nam, opt) (nam.nam$l_fnb & (opt))
#define rms_nam_esll(nam) nam.nam$b_esl
#define rms_nam_esl(nam) nam.nam$b_esl
#define rms_nam_name(nam) nam.nam$l_name
#define rms_nam_namel(nam) nam.nam$l_name
#define rms_nam_type(nam) nam.nam$l_type
#define rms_nam_typel(nam) nam.nam$l_type
#define rms_nam_ver(nam) nam.nam$l_ver
#define rms_nam_verl(nam) nam.nam$l_ver
#define rms_nam_rsll(nam) nam.nam$b_rsl
#define rms_nam_rsl(nam) nam.nam$b_rsl
#define rms_bind_fab_nam(fab, nam) fab.fab$l_nam = &nam
#define rms_set_fna(fab, nam, name, size) \
	{ fab.fab$b_fns = size; fab.fab$l_fna = name; }
#define rms_get_fna(fab, nam) fab.fab$l_fna
#define rms_set_dna(fab, nam, name, size) \
	{ fab.fab$b_dns = size; fab.fab$l_dna = name; }
#define rms_nam_dns(fab, nam) fab.fab$b_dns
#define rms_set_esa(nam, name, size) \
	{ nam.nam$b_ess = size; nam.nam$l_esa = name; }
#define rms_set_esal(nam, s_name, s_size, l_name, l_size) \
	{ nam.nam$l_esa = s_name; nam.nam$b_ess = s_size;}
#define rms_set_rsa(nam, name, size) \
	{ nam.nam$l_rsa = name; nam.nam$b_rss = size; }
#define rms_set_rsal(nam, s_name, s_size, l_name, l_size) \
	{ nam.nam$l_rsa = s_name; nam.nam$b_rss = s_size; }
#define rms_nam_name_type_l_size(nam) \
	(nam.nam$b_name + nam.nam$b_type)
#else
static int rms_free_search_context(struct FAB * fab)
{
struct NAML * nam;

    nam = fab->fab$l_naml;
    nam->naml$b_nop |= NAM$M_SYNCHK;
    nam->naml$l_rlf = NULL;
    nam->naml$l_long_defname_size = 0;

    fab->fab$b_dns = 0;
    return sys$parse(fab, NULL, NULL);
}

#define rms_setup_nam(nam) struct NAML nam = cc$rms_naml
#define rms_clear_nam_nop(nam) nam.naml$b_nop = 0;
#define rms_set_nam_nop(nam, opt) nam.naml$b_nop |= (opt)
#define rms_set_nam_fnb(nam, opt) nam.naml$l_fnb |= (opt)
#define rms_is_nam_fnb(nam, opt) (nam.naml$l_fnb & (opt))
#define rms_nam_esll(nam) nam.naml$l_long_expand_size
#define rms_nam_esl(nam) nam.naml$b_esl
#define rms_nam_name(nam) nam.naml$l_name
#define rms_nam_namel(nam) nam.naml$l_long_name
#define rms_nam_type(nam) nam.naml$l_type
#define rms_nam_typel(nam) nam.naml$l_long_type
#define rms_nam_ver(nam) nam.naml$l_ver
#define rms_nam_verl(nam) nam.naml$l_long_ver
#define rms_nam_rsll(nam) nam.naml$l_long_result_size
#define rms_nam_rsl(nam) nam.naml$b_rsl
#define rms_bind_fab_nam(fab, nam) fab.fab$l_naml = &nam
#define rms_set_fna(fab, nam, name, size) \
	{ fab.fab$b_fns = 0; fab.fab$l_fna = (char *) -1; \
	nam.naml$l_long_filename_size = size; \
	nam.naml$l_long_filename = name;}
#define rms_get_fna(fab, nam) nam.naml$l_long_filename
#define rms_set_dna(fab, nam, name, size) \
	{ fab.fab$b_dns = 0; fab.fab$l_dna = (char *) -1; \
	nam.naml$l_long_defname_size = size; \
	nam.naml$l_long_defname = name; }
#define rms_nam_dns(fab, nam) nam.naml$l_long_defname_size
#define rms_set_esa(nam, name, size) \
	{ nam.naml$b_ess = 0; nam.naml$l_esa = (char *) -1; \
	nam.naml$l_long_expand_alloc = size; \
	nam.naml$l_long_expand = name; }
#define rms_set_esal(nam, s_name, s_size, l_name, l_size) \
	{ nam.naml$l_esa = s_name; nam.naml$b_ess = s_size; \
	nam.naml$l_long_expand = l_name; \
	nam.naml$l_long_expand_alloc = l_size; }
#define rms_set_rsa(nam, name, size) \
	{ nam.naml$l_rsa = NULL; nam.naml$b_rss = 0; \
	nam.naml$l_long_result = name; \
	nam.naml$l_long_result_alloc = size; }
#define rms_set_rsal(nam, s_name, s_size, l_name, l_size) \
	{ nam.naml$l_rsa = s_name; nam.naml$b_rss = s_size; \
	nam.naml$l_long_result = l_name; \
	nam.naml$l_long_result_alloc = l_size; }
#define rms_nam_name_type_l_size(nam) \
	(nam.naml$l_long_name_size + nam.naml$l_long_type_size)
#endif


/* rms_erase
 * The CRTL for 8.3 and later can create symbolic links in any mode,
 * however in 8.3 the unlink/remove/delete routines will only properly handle
 * them if one of the PCP modes is active.
 */
static int rms_erase(const char * vmsname)
{
  int status;
  struct FAB myfab = cc$rms_fab;
  rms_setup_nam(mynam);

  rms_set_fna(myfab, mynam, (char *)vmsname, strlen(vmsname)); /* cast ok */
  rms_bind_fab_nam(myfab, mynam);

  /* Are we removing all versions? */
  if (vms_unlink_all_versions == 1) {
    const char * defspec = ";*";
    rms_set_dna(myfab, mynam, (char *)defspec, strlen(defspec)); /* cast ok */
  }

#ifdef NAML$M_OPEN_SPECIAL
  rms_set_nam_nop(mynam, NAML$M_OPEN_SPECIAL);
#endif

  status = sys$erase(&myfab, 0, 0);

  return status;
}


static int
vms_rename_with_acl(pTHX_ const struct dsc$descriptor_s * vms_src_dsc,
		    const struct dsc$descriptor_s * vms_dst_dsc,
		    unsigned long flags)
{
    /*  VMS and UNIX handle file permissions differently and the
     * the same ACL trick may be needed for renaming files,
     * especially if they are directories.
     */

   /* todo: get kill_file and rename to share common code */
   /* I can not find online documentation for $change_acl
    * it appears to be replaced by $set_security some time ago */

const unsigned int access_mode = 0;
$DESCRIPTOR(obj_file_dsc,"FILE");
char *vmsname;
char *rslt;
unsigned long int jpicode = JPI$_UIC, type = ACL$C_FILE;
int aclsts, fndsts, rnsts = -1;
unsigned int ctx = 0;
struct dsc$descriptor_s fildsc = {0, DSC$K_DTYPE_T, DSC$K_CLASS_S, 0};
struct dsc$descriptor_s * clean_dsc;

struct myacedef {
    unsigned char myace$b_length;
    unsigned char myace$b_type;
    unsigned short int myace$w_flags;
    unsigned long int myace$l_access;
    unsigned long int myace$l_ident;
} newace = { sizeof(struct myacedef), ACE$C_KEYID, 0,
	     ACE$M_READ | ACE$M_WRITE | ACE$M_DELETE | ACE$M_CONTROL,
	     0},
	     oldace = { sizeof(struct myacedef), ACE$C_KEYID, 0, 0, 0};

struct item_list_3
	findlst[3] = {{sizeof oldace, OSS$_ACL_FIND_ENTRY, &oldace, 0},
		      {sizeof oldace, OSS$_ACL_READ_ENTRY, &oldace, 0},
		      {0,0,0,0}},
	addlst[2] = {{sizeof newace, OSS$_ACL_ADD_ENTRY, &newace, 0},{0,0,0,0}},
	dellst[2] = {{sizeof newace, OSS$_ACL_DELETE_ENTRY, &newace, 0},
		     {0,0,0,0}};


    /* Expand the input spec using RMS, since we do not want to put
     * ACLs on the target of a symbolic link */
    vmsname = PerlMem_malloc(NAM$C_MAXRSS+1);
    if (vmsname == NULL)
	return SS$_INSFMEM;

    rslt = do_rmsexpand(vms_src_dsc->dsc$a_pointer,
			vmsname,
			0,
			NULL,
			PERL_RMSEXPAND_M_VMS | PERL_RMSEXPAND_M_SYMLINK,
			NULL,
			NULL);
    if (rslt == NULL) {
	PerlMem_free(vmsname);
	return SS$_INSFMEM;
    }

    /* So we get our own UIC to use as a rights identifier,
     * and the insert an ACE at the head of the ACL which allows us
     * to delete the file.
     */
    _ckvmssts(lib$getjpi(&jpicode,0,0,&(oldace.myace$l_ident),0,0));

    fildsc.dsc$w_length = strlen(vmsname);
    fildsc.dsc$a_pointer = vmsname;
    ctx = 0;
    newace.myace$l_ident = oldace.myace$l_ident;
    rnsts = SS$_ABORT;

    /* Grab any existing ACEs with this identifier in case we fail */
    clean_dsc = &fildsc;
    aclsts = fndsts = sys$get_security(&obj_file_dsc,
			       &fildsc,
			       NULL,
			       OSS$M_WLOCK,
			       findlst,
			       &ctx,
			       &access_mode);

    if ($VMS_STATUS_SUCCESS(fndsts)  || (fndsts == SS$_ACLEMPTY)) {
	/* Add the new ACE . . . */

	/* if the sys$get_security succeeded, then ctx is valid, and the
	 * object/file descriptors will be ignored.  But otherwise they
	 * are needed
	 */
	aclsts = sys$set_security(&obj_file_dsc, &fildsc, NULL,
				  OSS$M_RELCTX, addlst, &ctx, &access_mode);
	if (!$VMS_STATUS_SUCCESS(aclsts) && (aclsts != SS$_NOCLASS)) {
	    set_errno(EVMSERR);
	    set_vaxc_errno(aclsts);
	    PerlMem_free(vmsname);
	    return aclsts;
	}

	rnsts = lib$rename_file(vms_src_dsc, vms_dst_dsc,
				NULL, NULL,
				&flags,
				NULL, NULL, NULL, NULL, NULL, NULL, NULL);

	if ($VMS_STATUS_SUCCESS(rnsts)) {
	    clean_dsc = (struct dsc$descriptor_s *)vms_dst_dsc;
	}

	/* Put things back the way they were. */
	ctx = 0;
	aclsts = sys$get_security(&obj_file_dsc,
				  clean_dsc,
				  NULL,
				  OSS$M_WLOCK,
				  findlst,
				  &ctx,
				  &access_mode);

	if ($VMS_STATUS_SUCCESS(aclsts)) {
	int sec_flags;

	    sec_flags = 0;
	    if (!$VMS_STATUS_SUCCESS(fndsts))
		sec_flags = OSS$M_RELCTX;

	    /* Get rid of the new ACE */
	    aclsts = sys$set_security(NULL, NULL, NULL,
				  sec_flags, dellst, &ctx, &access_mode);

	    /* If there was an old ACE, put it back */
	    if ($VMS_STATUS_SUCCESS(aclsts) && $VMS_STATUS_SUCCESS(fndsts)) {
		addlst[0].bufadr = &oldace;
		aclsts = sys$set_security(NULL, NULL, NULL,
				      OSS$M_RELCTX, addlst, &ctx, &access_mode);
		if (!$VMS_STATUS_SUCCESS(aclsts) && (aclsts != SS$_NOCLASS)) {
		    set_errno(EVMSERR);
		    set_vaxc_errno(aclsts);
		    rnsts = aclsts;
		}
	    } else {
	    int aclsts2;

		/* Try to clear the lock on the ACL list */
		aclsts2 = sys$set_security(NULL, NULL, NULL,
				      OSS$M_RELCTX, NULL, &ctx, &access_mode);

		/* Rename errors are most important */
		if (!$VMS_STATUS_SUCCESS(rnsts))
		    aclsts = rnsts;
		set_errno(EVMSERR);
		set_vaxc_errno(aclsts);
		rnsts = aclsts;
	    }
	}
	else {
	    if (aclsts != SS$_ACLEMPTY)
		rnsts = aclsts;
	}
    }
    else
	rnsts = fndsts;

    PerlMem_free(vmsname);
    return rnsts;
}


/*{{{int rename(const char *, const char * */
/* Not exactly what X/Open says to do, but doing it absolutely right
 * and efficiently would require a lot more work.  This should be close
 * enough to pass all but the most strict X/Open compliance test.
 */
int
Perl_rename(pTHX_ const char *src, const char * dst)
{
int retval;
int pre_delete = 0;
int src_sts;
int dst_sts;
Stat_t src_st;
Stat_t dst_st;

    /* Validate the source file */
    src_sts = flex_lstat(src, &src_st);
    if (src_sts != 0) {

	/* No source file or other problem */
	return src_sts;
    }

    dst_sts = flex_lstat(dst, &dst_st);
    if (dst_sts == 0) {

	if (dst_st.st_dev != src_st.st_dev) {
	    /* Must be on the same device */
	    errno = EXDEV;
	    return -1;
	}

	/* VMS_INO_T_COMPARE is true if the inodes are different
	 * to match the output of memcmp
	 */

	if (!VMS_INO_T_COMPARE(src_st.st_ino, dst_st.st_ino)) {
	    /* That was easy, the files are the same! */
	    return 0;
	}

	if (S_ISDIR(src_st.st_mode) && !S_ISDIR(dst_st.st_mode)) {
	    /* If source is a directory, so must be dest */
		errno = EISDIR;
		return -1;
	}

    }


    if ((dst_sts == 0) &&
	(vms_unlink_all_versions || S_ISDIR(dst_st.st_mode))) {

	/* We have issues here if vms_unlink_all_versions is set
	 * If the destination exists, and is not a directory, then
	 * we must delete in advance.
	 *
	 * If the src is a directory, then we must always pre-delete
	 * the destination.
	 *
	 * If we successfully delete the dst in advance, and the rename fails
	 * X/Open requires that errno be EIO.
	 *
	 */

	if (!S_ISDIR(dst_st.st_mode) || S_ISDIR(src_st.st_mode)) {
	    int d_sts;
	    d_sts = mp_do_kill_file(aTHX_ dst, S_ISDIR(dst_st.st_mode));
	    if (d_sts != 0)
		return d_sts;

	    /* We killed the destination, so only errno now is EIO */
	    pre_delete = 1;
	}
    }

    /* Originally the idea was to call the CRTL rename() and only
     * try the lib$rename_file if it failed.
     * It turns out that there are too many variants in what the
     * the CRTL rename might do, so only use lib$rename_file
     */
    retval = -1;

    {
	/* Is the source and dest both in VMS format */
	/* if the source is a directory, then need to fileify */
	/*  and dest must be a directory or non-existant. */

	char * vms_src;
	char * vms_dst;
	int sts;
	char * ret_str;
	unsigned long flags;
	struct dsc$descriptor_s old_file_dsc;
	struct dsc$descriptor_s new_file_dsc;

	/* We need to modify the src and dst depending
	 * on if one or more of them are directories.
	 */

	vms_src = PerlMem_malloc(VMS_MAXRSS);
	if (vms_src == NULL)
	    _ckvmssts(SS$_INSFMEM);

	/* Source is always a VMS format file */
	ret_str = do_tovmsspec(src, vms_src, 0, NULL);
	if (ret_str == NULL) {
	    PerlMem_free(vms_src);
	    errno = EIO;
	    return -1;
	}

	vms_dst = PerlMem_malloc(VMS_MAXRSS);
	if (vms_dst == NULL)
	    _ckvmssts(SS$_INSFMEM);

	if (S_ISDIR(src_st.st_mode)) {
	char * ret_str;
	char * vms_dir_file;

	    vms_dir_file = PerlMem_malloc(VMS_MAXRSS);
	    if (vms_dir_file == NULL)
		_ckvmssts(SS$_INSFMEM);

	    /* The source must be a file specification */
	    ret_str = do_fileify_dirspec(vms_src, vms_dir_file, 0, NULL);
	    if (ret_str == NULL) {
		PerlMem_free(vms_src);
		PerlMem_free(vms_dst);
		PerlMem_free(vms_dir_file);
		errno = EIO;
		return -1;
	    }
	    PerlMem_free(vms_src);
	    vms_src = vms_dir_file;

	    /* If the dest is a directory, we must remove it
	    if (dst_sts == 0) {
		int d_sts;
		d_sts = mp_do_kill_file(aTHX_ dst, 1);
		if (d_sts != 0) {
		    PerlMem_free(vms_src);
		    PerlMem_free(vms_dst);
		    errno = EIO;
		    return sts;
		}

		pre_delete = 1;
	    }

	   /* The dest must be a VMS file specification */
	   ret_str = do_tovmsspec(dst, vms_dst, 0, NULL);
	   if (ret_str == NULL) {
		PerlMem_free(vms_src);
		PerlMem_free(vms_dst);
		errno = EIO;
		return -1;
	   }

	    /* The source must be a file specification */
	    vms_dir_file = PerlMem_malloc(VMS_MAXRSS);
	    if (vms_dir_file == NULL)
		_ckvmssts(SS$_INSFMEM);

	    ret_str = do_fileify_dirspec(vms_dst, vms_dir_file, 0, NULL);
	    if (ret_str == NULL) {
		PerlMem_free(vms_src);
		PerlMem_free(vms_dst);
		PerlMem_free(vms_dir_file);
		errno = EIO;
		return -1;
	    }
	    PerlMem_free(vms_dst);
	    vms_dst = vms_dir_file;

	} else {
	    /* File to file or file to new dir */

	    if ((dst_sts == 0) && S_ISDIR(dst_st.st_mode)) {
		/* VMS pathify a dir target */
		ret_str = do_tovmspath(dst, vms_dst, 0, NULL);
		if (ret_str == NULL) {
		    PerlMem_free(vms_src);
		    PerlMem_free(vms_dst);
		    errno = EIO;
		    return -1;
		}
	    } else {

		/* fileify a target VMS file specification */
		ret_str = do_tovmsspec(dst, vms_dst, 0, NULL);
		if (ret_str == NULL) {
		    PerlMem_free(vms_src);
		    PerlMem_free(vms_dst);
		    errno = EIO;
		    return -1;
		}
	    }
	}

	old_file_dsc.dsc$a_pointer = vms_src;
	old_file_dsc.dsc$w_length = strlen(vms_src);
	old_file_dsc.dsc$b_dtype = DSC$K_DTYPE_T;
	old_file_dsc.dsc$b_class = DSC$K_CLASS_S;

	new_file_dsc.dsc$a_pointer = vms_dst;
	new_file_dsc.dsc$w_length = strlen(vms_dst);
	new_file_dsc.dsc$b_dtype = DSC$K_DTYPE_T;
	new_file_dsc.dsc$b_class = DSC$K_CLASS_S;

	flags = 0;
#if !defined(__VAX) && defined(NAML$C_MAXRSS)
	flags |= 2; /* LIB$M_FIL_LONG_NAMES */
#endif

	sts = lib$rename_file(&old_file_dsc,
			      &new_file_dsc,
			      NULL, NULL,
			      &flags,
			      NULL, NULL, NULL, NULL, NULL, NULL, NULL);
	if (!$VMS_STATUS_SUCCESS(sts)) {

	   /* We could have failed because VMS style permissions do not
	    * permit renames that UNIX will allow.  Just like the hack
	    * in for kill_file.
	    */
	   sts = vms_rename_with_acl(aTHX_ &old_file_dsc, &new_file_dsc, flags);
	}

	PerlMem_free(vms_src);
	PerlMem_free(vms_dst);
	if (!$VMS_STATUS_SUCCESS(sts)) {
	    errno = EIO;
	    return -1;
	}
	retval = 0;
    }

    if (vms_unlink_all_versions) {
	/* Now get rid of any previous versions of the source file that
	 * might still exist
	 */
	int save_errno;
	save_errno = errno;
	src_sts = mp_do_kill_file(aTHX_ src, S_ISDIR(src_st.st_mode));
	errno = save_errno;
    }

    /* We deleted the destination, so must force the error to be EIO */
    if ((retval != 0) && (pre_delete != 0))
	errno = EIO;

    return retval;
}
/*}}}*/


/*{{{char *do_rmsexpand(char *fspec, char *out, int ts, char *def, unsigned opts)*/
/* Shortcut for common case of simple calls to $PARSE and $SEARCH
 * to expand file specification.  Allows for a single default file
 * specification and a simple mask of options.  If outbuf is non-NULL,
 * it must point to a buffer at least NAM$C_MAXRSS bytes long, into which
 * the resultant file specification is placed.  If outbuf is NULL, the
 * resultant file specification is placed into a static buffer.
 * The third argument, if non-NULL, is taken to be a default file
 * specification string.  The fourth argument is unused at present.
 * rmesexpand() returns the address of the resultant string if
 * successful, and NULL on error.
 *
 * New functionality for previously unused opts value:
 *  PERL_RMSEXPAND_M_VMS - Force output file specification to VMS format.
 *  PERL_RMSEXPAND_M_LONG - Want output in long formst
 *  PERL_RMSEXPAND_M_VMS_IN - Input is already in VMS, so do not vmsify
 *  PERL_RMSEXPAND_M_SYMLINK - Use symbolic link, not target
 */
static char *mp_do_tounixspec(pTHX_ const char *, char *, int, int *);

static char *
mp_do_rmsexpand
   (pTHX_ const char *filespec,
    char *outbuf,
    int ts,
    const char *defspec,
    unsigned opts,
    int * fs_utf8,
    int * dfs_utf8)
{
  static char __rmsexpand_retbuf[VMS_MAXRSS];
  char * vmsfspec, *tmpfspec;
  char * esa, *cp, *out = NULL;
  char * tbuf;
  char * esal = NULL;
  char * outbufl;
  struct FAB myfab = cc$rms_fab;
  rms_setup_nam(mynam);
  STRLEN speclen;
  unsigned long int retsts, trimver, trimtype, haslower = 0, isunix = 0;
  int sts;

  /* temp hack until UTF8 is actually implemented */
  if (fs_utf8 != NULL)
    *fs_utf8 = 0;

  if (!filespec || !*filespec) {
    set_vaxc_errno(LIB$_INVARG); set_errno(EINVAL);
    return NULL;
  }
  if (!outbuf) {
    if (ts) out = Newx(outbuf,VMS_MAXRSS,char);
    else    outbuf = __rmsexpand_retbuf;
  }

  vmsfspec = NULL;
  tmpfspec = NULL;
  outbufl = NULL;

  isunix = 0;
  if ((opts & PERL_RMSEXPAND_M_VMS_IN) == 0) {
    isunix = is_unix_filespec(filespec);
    if (isunix) {
      vmsfspec = PerlMem_malloc(VMS_MAXRSS);
      if (vmsfspec == NULL) _ckvmssts(SS$_INSFMEM);
      if (do_tovmsspec(filespec,vmsfspec,0,fs_utf8) == NULL) {
	PerlMem_free(vmsfspec);
	if (out)
	   Safefree(out);
	return NULL;
      }
      filespec = vmsfspec;

      /* Unless we are forcing to VMS format, a UNIX input means
       * UNIX output, and that requires long names to be used
       */
#if !defined(__VAX) && defined(NAML$C_MAXRSS)
      if ((opts & PERL_RMSEXPAND_M_VMS) == 0)
	opts |= PERL_RMSEXPAND_M_LONG;
      else
#endif
	isunix = 0;
      }
    }

  rms_set_fna(myfab, mynam, (char *)filespec, strlen(filespec)); /* cast ok */
  rms_bind_fab_nam(myfab, mynam);

  if (defspec && *defspec) {
    int t_isunix;
    t_isunix = is_unix_filespec(defspec);
    if (t_isunix) {
      tmpfspec = PerlMem_malloc(VMS_MAXRSS);
      if (tmpfspec == NULL) _ckvmssts(SS$_INSFMEM);
      if (do_tovmsspec(defspec,tmpfspec,0,dfs_utf8) == NULL) {
	PerlMem_free(tmpfspec);
	if (vmsfspec != NULL)
	    PerlMem_free(vmsfspec);
	if (out)
	   Safefree(out);
	return NULL;
      }
      defspec = tmpfspec;
    }
    rms_set_dna(myfab, mynam, (char *)defspec, strlen(defspec)); /* cast ok */
  }

  esa = PerlMem_malloc(NAM$C_MAXRSS + 1);
  if (esa == NULL) _ckvmssts(SS$_INSFMEM);
#if !defined(__VAX) && defined(NAML$C_MAXRSS)
  esal = PerlMem_malloc(VMS_MAXRSS);
  if (esal == NULL) _ckvmssts(SS$_INSFMEM);
#endif
  rms_set_esal(mynam, esa, NAM$C_MAXRSS, esal, VMS_MAXRSS-1);

  /* If a NAML block is used RMS always writes to the long and short
   * addresses unless you suppress the short name.
   */
#if !defined(__VAX) && defined(NAML$C_MAXRSS)
  outbufl = PerlMem_malloc(VMS_MAXRSS);
  if (outbufl == NULL) _ckvmssts(SS$_INSFMEM);
#endif
   rms_set_rsal(mynam, outbuf, NAM$C_MAXRSS, outbufl, (VMS_MAXRSS - 1));

#ifdef NAM$M_NO_SHORT_UPCASE
  if (decc_efs_case_preserve)
    rms_set_nam_nop(mynam, NAM$M_NO_SHORT_UPCASE);
#endif

   /* We may not want to follow symbolic links */
#ifdef NAML$M_OPEN_SPECIAL
  if ((opts & PERL_RMSEXPAND_M_SYMLINK) != 0)
    rms_set_nam_nop(mynam, NAML$M_OPEN_SPECIAL);
#endif

  /* First attempt to parse as an existing file */
  retsts = sys$parse(&myfab,0,0);
  if (!(retsts & STS$K_SUCCESS)) {

    /* Could not find the file, try as syntax only if error is not fatal */
    rms_set_nam_nop(mynam, NAM$M_SYNCHK);
    if (retsts == RMS$_DNF || retsts == RMS$_DIR || retsts == RMS$_DEV) {
      retsts = sys$parse(&myfab,0,0);
      if (retsts & STS$K_SUCCESS) goto expanded;
    }  

     /* Still could not parse the file specification */
    /*----------------------------------------------*/
    sts = rms_free_search_context(&myfab); /* Free search context */
    if (out) Safefree(out);
    if (tmpfspec != NULL)
	PerlMem_free(tmpfspec);
    if (vmsfspec != NULL)
	PerlMem_free(vmsfspec);
    if (outbufl != NULL)
	PerlMem_free(outbufl);
    PerlMem_free(esa);
    if (esal != NULL) 
	PerlMem_free(esal);
    set_vaxc_errno(retsts);
    if      (retsts == RMS$_PRV) set_errno(EACCES);
    else if (retsts == RMS$_DEV) set_errno(ENODEV);
    else if (retsts == RMS$_DIR) set_errno(ENOTDIR);
    else                         set_errno(EVMSERR);
    return NULL;
  }
  retsts = sys$search(&myfab,0,0);
  if (!(retsts & STS$K_SUCCESS) && retsts != RMS$_FNF) {
    sts = rms_free_search_context(&myfab); /* Free search context */
    if (out) Safefree(out);
    if (tmpfspec != NULL)
	PerlMem_free(tmpfspec);
    if (vmsfspec != NULL)
	PerlMem_free(vmsfspec);
    if (outbufl != NULL)
	PerlMem_free(outbufl);
    PerlMem_free(esa);
    if (esal != NULL) 
	PerlMem_free(esal);
    set_vaxc_errno(retsts);
    if      (retsts == RMS$_PRV) set_errno(EACCES);
    else                         set_errno(EVMSERR);
    return NULL;
  }

  /* If the input filespec contained any lowercase characters,
   * downcase the result for compatibility with Unix-minded code. */
  expanded:
  if (!decc_efs_case_preserve) {
    for (tbuf = rms_get_fna(myfab, mynam); *tbuf; tbuf++)
      if (islower(*tbuf)) { haslower = 1; break; }
  }

   /* Is a long or a short name expected */
  /*------------------------------------*/
  if ((opts & PERL_RMSEXPAND_M_LONG) != 0) {
    if (rms_nam_rsll(mynam)) {
	tbuf = outbufl;
	speclen = rms_nam_rsll(mynam);
    }
    else {
	tbuf = esal; /* Not esa */
	speclen = rms_nam_esll(mynam);
    }
  }
  else {
    if (rms_nam_rsl(mynam)) {
	tbuf = outbuf;
	speclen = rms_nam_rsl(mynam);
    }
    else {
	tbuf = esa; /* Not esal */
	speclen = rms_nam_esl(mynam);
    }
  }
  tbuf[speclen] = '\0';

  /* Trim off null fields added by $PARSE
   * If type > 1 char, must have been specified in original or default spec
   * (not true for version; $SEARCH may have added version of existing file).
   */
  trimver  = !rms_is_nam_fnb(mynam, NAM$M_EXP_VER);
  if ((opts & PERL_RMSEXPAND_M_LONG) != 0) {
    trimtype = !rms_is_nam_fnb(mynam, NAM$M_EXP_TYPE) &&
             ((rms_nam_verl(mynam) - rms_nam_typel(mynam)) == 1);
  }
  else {
    trimtype = !rms_is_nam_fnb(mynam, NAM$M_EXP_TYPE) &&
             ((rms_nam_ver(mynam) - rms_nam_type(mynam)) == 1);
  }
  if (trimver || trimtype) {
    if (defspec && *defspec) {
      char *defesal = NULL;
      char *defesa = NULL;
      defesa = PerlMem_malloc(VMS_MAXRSS + 1);
      if (defesa != NULL) {
#if !defined(__VAX) && defined(NAML$C_MAXRSS)
        defesal = PerlMem_malloc(VMS_MAXRSS + 1);
        if (defesal == NULL) _ckvmssts(SS$_INSFMEM);
#endif
	struct FAB deffab = cc$rms_fab;
	rms_setup_nam(defnam);
     
	rms_bind_fab_nam(deffab, defnam);

	/* Cast ok */ 
	rms_set_fna
	    (deffab, defnam, (char *)defspec, rms_nam_dns(myfab, mynam)); 

	/* RMS needs the esa/esal as a work area if wildcards are involved */
	rms_set_esal(defnam, defesa, NAM$C_MAXRSS, defesal, VMS_MAXRSS - 1);

	rms_clear_nam_nop(defnam);
	rms_set_nam_nop(defnam, NAM$M_SYNCHK);
#ifdef NAM$M_NO_SHORT_UPCASE
	if (decc_efs_case_preserve)
	  rms_set_nam_nop(defnam, NAM$M_NO_SHORT_UPCASE);
#endif
#ifdef NAML$M_OPEN_SPECIAL
	if ((opts & PERL_RMSEXPAND_M_SYMLINK) != 0)
	  rms_set_nam_nop(mynam, NAML$M_OPEN_SPECIAL);
#endif
	if (sys$parse(&deffab,0,0) & STS$K_SUCCESS) {
	  if (trimver) {
	     trimver  = !rms_is_nam_fnb(defnam, NAM$M_EXP_VER);
	  }
	  if (trimtype) {
	    trimtype = !rms_is_nam_fnb(defnam, NAM$M_EXP_TYPE); 
	  }
	}
	if (defesal != NULL)
	    PerlMem_free(defesal);
	PerlMem_free(defesa);
      }
    }
    if (trimver) {
      if ((opts & PERL_RMSEXPAND_M_LONG) != 0) {
	if (*(rms_nam_verl(mynam)) != '\"')
	  speclen = rms_nam_verl(mynam) - tbuf;
      }
      else {
	if (*(rms_nam_ver(mynam)) != '\"')
	  speclen = rms_nam_ver(mynam) - tbuf;
      }
    }
    if (trimtype) {
      /* If we didn't already trim version, copy down */
      if ((opts & PERL_RMSEXPAND_M_LONG) != 0) {
	if (speclen > rms_nam_verl(mynam) - tbuf)
	  memmove
	   (rms_nam_typel(mynam),
	    rms_nam_verl(mynam),
	    speclen - (rms_nam_verl(mynam) - tbuf));
	  speclen -= rms_nam_verl(mynam) - rms_nam_typel(mynam);
      }
      else {
	if (speclen > rms_nam_ver(mynam) - tbuf)
	  memmove
	   (rms_nam_type(mynam),
	    rms_nam_ver(mynam),
	    speclen - (rms_nam_ver(mynam) - tbuf));
	  speclen -= rms_nam_ver(mynam) - rms_nam_type(mynam);
      }
    }
  }

   /* Done with these copies of the input files */
  /*-------------------------------------------*/
  if (vmsfspec != NULL)
	PerlMem_free(vmsfspec);
  if (tmpfspec != NULL)
	PerlMem_free(tmpfspec);

  /* If we just had a directory spec on input, $PARSE "helpfully"
   * adds an empty name and type for us */
#if !defined(__VAX) && defined(NAML$C_MAXRSS)
  if ((opts & PERL_RMSEXPAND_M_LONG) != 0) {
    if (rms_nam_namel(mynam) == rms_nam_typel(mynam) &&
	rms_nam_verl(mynam)  == rms_nam_typel(mynam) + 1 &&
	!(rms_is_nam_fnb(mynam, NAM$M_EXP_NAME)))
      speclen = rms_nam_namel(mynam) - tbuf;
  }
  else
#endif
  {
    if (rms_nam_name(mynam) == rms_nam_type(mynam) &&
	rms_nam_ver(mynam)  == rms_nam_ver(mynam) + 1 &&
	!(rms_is_nam_fnb(mynam, NAM$M_EXP_NAME)))
      speclen = rms_nam_name(mynam) - tbuf;
  }

  /* Posix format specifications must have matching quotes */
  if (speclen < (VMS_MAXRSS - 1)) {
    if (decc_posix_compliant_pathnames && (tbuf[0] == '\"')) {
      if ((speclen > 1) && (tbuf[speclen-1] != '\"')) {
        tbuf[speclen] = '\"';
        speclen++;
      }
    }
  }
  tbuf[speclen] = '\0';
  if (haslower && !decc_efs_case_preserve) __mystrtolower(tbuf);

  /* Have we been working with an expanded, but not resultant, spec? */
  /* Also, convert back to Unix syntax if necessary. */
  {
  int rsl;

#if !defined(__VAX) && defined(NAML$C_MAXRSS)
    if ((opts & PERL_RMSEXPAND_M_LONG) != 0) {
      rsl = rms_nam_rsll(mynam);
    } else
#endif
    {
      rsl = rms_nam_rsl(mynam);
    }
    if (!rsl) {
      if (isunix) {
        if (do_tounixspec(tbuf, outbuf ,0 , fs_utf8) == NULL) {
	  if (out) Safefree(out);
	  if (esal != NULL)
	    PerlMem_free(esal);
	  PerlMem_free(esa);
	  if (outbufl != NULL)
	    PerlMem_free(outbufl);
	  return NULL;
        }
      }
      else strcpy(outbuf, tbuf);
    }
    else if (isunix) {
      tmpfspec = PerlMem_malloc(VMS_MAXRSS);
      if (tmpfspec == NULL) _ckvmssts(SS$_INSFMEM);
      if (do_tounixspec(tbuf,tmpfspec,0,fs_utf8) == NULL) {
	if (out) Safefree(out);
	PerlMem_free(esa);
	if (esal != NULL)
	    PerlMem_free(esal);
	PerlMem_free(tmpfspec);
	if (outbufl != NULL)
	    PerlMem_free(outbufl);
	return NULL;
      }
      strcpy(outbuf,tmpfspec);
      PerlMem_free(tmpfspec);
    }
  }
  rms_set_rsal(mynam, NULL, 0, NULL, 0);
  sts = rms_free_search_context(&myfab); /* Free search context */
  PerlMem_free(esa);
  if (esal != NULL)
     PerlMem_free(esal);
  if (outbufl != NULL)
     PerlMem_free(outbufl);
  return outbuf;
}
/*}}}*/
/* External entry points */
char *Perl_rmsexpand(pTHX_ const char *spec, char *buf, const char *def, unsigned opt)
{ return do_rmsexpand(spec,buf,0,def,opt,NULL,NULL); }
char *Perl_rmsexpand_ts(pTHX_ const char *spec, char *buf, const char *def, unsigned opt)
{ return do_rmsexpand(spec,buf,1,def,opt,NULL,NULL); }
char *Perl_rmsexpand_utf8
  (pTHX_ const char *spec, char *buf, const char *def,
   unsigned opt, int * fs_utf8, int * dfs_utf8)
{ return do_rmsexpand(spec,buf,0,def,opt, fs_utf8, dfs_utf8); }
char *Perl_rmsexpand_utf8_ts
  (pTHX_ const char *spec, char *buf, const char *def,
   unsigned opt, int * fs_utf8, int * dfs_utf8)
{ return do_rmsexpand(spec,buf,1,def,opt, fs_utf8, dfs_utf8); }


/*
** The following routines are provided to make life easier when
** converting among VMS-style and Unix-style directory specifications.
** All will take input specifications in either VMS or Unix syntax. On
** failure, all return NULL.  If successful, the routines listed below
** return a pointer to a buffer containing the appropriately
** reformatted spec (and, therefore, subsequent calls to that routine
** will clobber the result), while the routines of the same names with
** a _ts suffix appended will return a pointer to a mallocd string
** containing the appropriately reformatted spec.
** In all cases, only explicit syntax is altered; no check is made that
** the resulting string is valid or that the directory in question
** actually exists.
**
**   fileify_dirspec() - convert a directory spec into the name of the
**     directory file (i.e. what you can stat() to see if it's a dir).
**     The style (VMS or Unix) of the result is the same as the style
**     of the parameter passed in.
**   pathify_dirspec() - convert a directory spec into a path (i.e.
**     what you prepend to a filename to indicate what directory it's in).
**     The style (VMS or Unix) of the result is the same as the style
**     of the parameter passed in.
**   tounixpath() - convert a directory spec into a Unix-style path.
**   tovmspath() - convert a directory spec into a VMS-style path.
**   tounixspec() - convert any file spec into a Unix-style file spec.
**   tovmsspec() - convert any file spec into a VMS-style spec.
**   xxxxx_utf8() - Variants that support UTF8 encoding of Unix-Style file spec.
**
** Copyright 1996 by Charles Bailey  <bailey@newman.upenn.edu>
** Permission is given to distribute this code as part of the Perl
** standard distribution under the terms of the GNU General Public
** License or the Perl Artistic License.  Copies of each may be
** found in the Perl standard distribution.
 */

/*{{{ char *fileify_dirspec[_ts](char *dir, char *buf, int * utf8_fl)*/
static char *mp_do_fileify_dirspec(pTHX_ const char *dir,char *buf,int ts, int *utf8_fl)
{
    static char __fileify_retbuf[VMS_MAXRSS];
    unsigned long int dirlen, retlen, addmfd = 0, hasfilename = 0;
    char *retspec, *cp1, *cp2, *lastdir;
    char *trndir, *vmsdir;
    unsigned short int trnlnm_iter_count;
    int sts;
    if (utf8_fl != NULL)
	*utf8_fl = 0;

    if (!dir || !*dir) {
      set_errno(EINVAL); set_vaxc_errno(SS$_BADPARAM); return NULL;
    }
    dirlen = strlen(dir);
    while (dirlen && dir[dirlen-1] == '/') --dirlen;
    if (!dirlen) { /* We had Unixish '/' -- substitute top of current tree */
      if (!decc_posix_compliant_pathnames && decc_disable_posix_root) {
        dir = "/sys$disk";
        dirlen = 9;
      }
      else
	dirlen = 1;
    }
    if (dirlen > (VMS_MAXRSS - 1)) {
      set_errno(ENAMETOOLONG); set_vaxc_errno(RMS$_SYN);
      return NULL;
    }
    trndir = PerlMem_malloc(VMS_MAXRSS + 1);
    if (trndir == NULL) _ckvmssts(SS$_INSFMEM);
    if (!strpbrk(dir+1,"/]>:")  &&
	(!decc_posix_compliant_pathnames && decc_disable_posix_root)) {
      strcpy(trndir,*dir == '/' ? dir + 1: dir);
      trnlnm_iter_count = 0;
      while (!strpbrk(trndir,"/]>:") && my_trnlnm(trndir,trndir,0)) {
        trnlnm_iter_count++; 
        if (trnlnm_iter_count >= PERL_LNM_MAX_ITER) break;
      }
      dirlen = strlen(trndir);
    }
    else {
      strncpy(trndir,dir,dirlen);
      trndir[dirlen] = '\0';
    }

    /* At this point we are done with *dir and use *trndir which is a
     * copy that can be modified.  *dir must not be modified.
     */

    /* If we were handed a rooted logical name or spec, treat it like a
     * simple directory, so that
     *    $ Define myroot dev:[dir.]
     *    ... do_fileify_dirspec("myroot",buf,1) ...
     * does something useful.
     */
    if (dirlen >= 2 && !strcmp(trndir+dirlen-2,".]")) {
      trndir[--dirlen] = '\0';
      trndir[dirlen-1] = ']';
    }
    if (dirlen >= 2 && !strcmp(trndir+dirlen-2,".>")) {
      trndir[--dirlen] = '\0';
      trndir[dirlen-1] = '>';
    }

    if ((cp1 = strrchr(trndir,']')) != NULL || (cp1 = strrchr(trndir,'>')) != NULL) {
      /* If we've got an explicit filename, we can just shuffle the string. */
      if (*(cp1+1)) hasfilename = 1;
      /* Similarly, we can just back up a level if we've got multiple levels
         of explicit directories in a VMS spec which ends with directories. */
      else {
        for (cp2 = cp1; cp2 > trndir; cp2--) {
	  if (*cp2 == '.') {
	    if ((cp2 - 1 > trndir) && (*(cp2 - 1) != '^')) {
/* fix-me, can not scan EFS file specs backward like this */
              *cp2 = *cp1; *cp1 = '\0';
              hasfilename = 1;
	      break;
	    }
          }
          if (*cp2 == '[' || *cp2 == '<') break;
        }
      }
    }

    vmsdir = PerlMem_malloc(VMS_MAXRSS + 1);
    if (vmsdir == NULL) _ckvmssts(SS$_INSFMEM);
    cp1 = strpbrk(trndir,"]:>");
    if (hasfilename || !cp1) { /* Unix-style path or filename */
      if (trndir[0] == '.') {
        if (trndir[1] == '\0' || (trndir[1] == '/' && trndir[2] == '\0')) {
	  PerlMem_free(trndir);
	  PerlMem_free(vmsdir);
          return do_fileify_dirspec("[]",buf,ts,NULL);
	}
        else if (trndir[1] == '.' &&
               (trndir[2] == '\0' || (trndir[2] == '/' && trndir[3] == '\0'))) {
	  PerlMem_free(trndir);
	  PerlMem_free(vmsdir);
          return do_fileify_dirspec("[-]",buf,ts,NULL);
	}
      }
      if (dirlen && trndir[dirlen-1] == '/') {    /* path ends with '/'; just add .dir;1 */
        dirlen -= 1;                 /* to last element */
        lastdir = strrchr(trndir,'/');
      }
      else if ((cp1 = strstr(trndir,"/.")) != NULL) {
        /* If we have "/." or "/..", VMSify it and let the VMS code
         * below expand it, rather than repeating the code to handle
         * relative components of a filespec here */
        do {
          if (*(cp1+2) == '.') cp1++;
          if (*(cp1+2) == '/' || *(cp1+2) == '\0') {
	    char * ret_chr;
            if (do_tovmsspec(trndir,vmsdir,0,NULL) == NULL) {
		PerlMem_free(trndir);
		PerlMem_free(vmsdir);
		return NULL;
	    }
            if (strchr(vmsdir,'/') != NULL) {
              /* If do_tovmsspec() returned it, it must have VMS syntax
               * delimiters in it, so it's a mixed VMS/Unix spec.  We take
               * the time to check this here only so we avoid a recursion
               * loop; otherwise, gigo.
               */
	      PerlMem_free(trndir);
	      PerlMem_free(vmsdir);
              set_errno(EINVAL);  set_vaxc_errno(RMS$_SYN);
	      return NULL;
            }
            if (do_fileify_dirspec(vmsdir,trndir,0,NULL) == NULL) {
		PerlMem_free(trndir);
		PerlMem_free(vmsdir);
		return NULL;
	    }
	    ret_chr = do_tounixspec(trndir,buf,ts,NULL);
	    PerlMem_free(trndir);
	    PerlMem_free(vmsdir);
            return ret_chr;
          }
          cp1++;
        } while ((cp1 = strstr(cp1,"/.")) != NULL);
        lastdir = strrchr(trndir,'/');
      }
      else if (dirlen >= 7 && !strcmp(&trndir[dirlen-7],"/000000")) {
	char * ret_chr;
        /* Ditto for specs that end in an MFD -- let the VMS code
         * figure out whether it's a real device or a rooted logical. */

        /* This should not happen any more.  Allowing the fake /000000
         * in a UNIX pathname causes all sorts of problems when trying
         * to run in UNIX emulation.  So the VMS to UNIX conversions
         * now remove the fake /000000 directories.
         */

        trndir[dirlen] = '/'; trndir[dirlen+1] = '\0';
        if (do_tovmsspec(trndir,vmsdir,0,NULL) == NULL) {
	    PerlMem_free(trndir);
	    PerlMem_free(vmsdir);
	    return NULL;
	}
        if (do_fileify_dirspec(vmsdir,trndir,0,NULL) == NULL) {
	    PerlMem_free(trndir);
	    PerlMem_free(vmsdir);
	    return NULL;
	}
	ret_chr = do_tounixspec(trndir,buf,ts,NULL);
	PerlMem_free(trndir);
	PerlMem_free(vmsdir);
        return ret_chr;
      }
      else {

        if ( !(lastdir = cp1 = strrchr(trndir,'/')) &&
             !(lastdir = cp1 = strrchr(trndir,']')) &&
             !(lastdir = cp1 = strrchr(trndir,'>'))) cp1 = trndir;
        if ((cp2 = strchr(cp1,'.'))) {  /* look for explicit type */
          int ver; char *cp3;

	  /* For EFS or ODS-5 look for the last dot */
	  if (decc_efs_charset) {
	      cp2 = strrchr(cp1,'.');
	  }
	  if (vms_process_case_tolerant) {
              if (!*(cp2+1) || toupper(*(cp2+1)) != 'D' ||  /* Wrong type. */
                  !*(cp2+2) || toupper(*(cp2+2)) != 'I' ||  /* Bzzt. */
                  !*(cp2+3) || toupper(*(cp2+3)) != 'R' ||
                  (*(cp2+4) && ((*(cp2+4) != ';' && *(cp2+4) != '.')  ||
                  (*(cp2+5) && ((ver = strtol(cp2+5,&cp3,10)) != 1 &&
                            (ver || *cp3)))))) {
		  PerlMem_free(trndir);
		  PerlMem_free(vmsdir);
                  set_errno(ENOTDIR);
                  set_vaxc_errno(RMS$_DIR);
                  return NULL;
	      }
	  }
	  else {
              if (!*(cp2+1) || *(cp2+1) != 'D' ||  /* Wrong type. */
                  !*(cp2+2) || *(cp2+2) != 'I' ||  /* Bzzt. */
                  !*(cp2+3) || *(cp2+3) != 'R' ||
                  (*(cp2+4) && ((*(cp2+4) != ';' && *(cp2+4) != '.')  ||
                  (*(cp2+5) && ((ver = strtol(cp2+5,&cp3,10)) != 1 &&
                            (ver || *cp3)))))) {
		 PerlMem_free(trndir);
		 PerlMem_free(vmsdir);
                 set_errno(ENOTDIR);
                 set_vaxc_errno(RMS$_DIR);
                 return NULL;
	      }
          }
          dirlen = cp2 - trndir;
        }
      }

      retlen = dirlen + 6;
      if (buf) retspec = buf;
      else if (ts) Newx(retspec,retlen+1,char);
      else retspec = __fileify_retbuf;
      memcpy(retspec,trndir,dirlen);
      retspec[dirlen] = '\0';

      /* We've picked up everything up to the directory file name.
         Now just add the type and version, and we're set. */
      if ((!decc_efs_case_preserve) && vms_process_case_tolerant)
	strcat(retspec,".dir;1");
      else
	strcat(retspec,".DIR;1");
      PerlMem_free(trndir);
      PerlMem_free(vmsdir);
      return retspec;
    }
    else {  /* VMS-style directory spec */

      char *esa, *esal, term, *cp;
      char *my_esa;
      int my_esa_len;
      unsigned long int sts, cmplen, haslower = 0;
      unsigned int nam_fnb;
      char * nam_type;
      struct FAB dirfab = cc$rms_fab;
      rms_setup_nam(savnam);
      rms_setup_nam(dirnam);

      esa = PerlMem_malloc(NAM$C_MAXRSS + 1);
      if (esa == NULL) _ckvmssts(SS$_INSFMEM);
      esal = NULL;
#if !defined(__VAX) && defined(NAML$C_MAXRSS)
      esal = PerlMem_malloc(VMS_MAXRSS);
      if (esal == NULL) _ckvmssts(SS$_INSFMEM);
#endif
      rms_set_fna(dirfab, dirnam, trndir, strlen(trndir));
      rms_bind_fab_nam(dirfab, dirnam);
      rms_set_dna(dirfab, dirnam, ".DIR;1", 6);
      rms_set_esal(dirnam, esa, NAM$C_MAXRSS, esal, (VMS_MAXRSS - 1));
#ifdef NAM$M_NO_SHORT_UPCASE
      if (decc_efs_case_preserve)
	rms_set_nam_nop(dirnam, NAM$M_NO_SHORT_UPCASE);
#endif

      for (cp = trndir; *cp; cp++)
        if (islower(*cp)) { haslower = 1; break; }
      if (!((sts = sys$parse(&dirfab)) & STS$K_SUCCESS)) {
        if ((dirfab.fab$l_sts == RMS$_DIR) || (dirfab.fab$l_sts == RMS$_DNF)) {
	  rms_set_nam_nop(dirnam, NAM$M_SYNCHK);
          sts = sys$parse(&dirfab) & STS$K_SUCCESS;
        }
        if (!sts) {
	  PerlMem_free(esa);
	  if (esal != NULL)
	      PerlMem_free(esal);
	  PerlMem_free(trndir);
	  PerlMem_free(vmsdir);
          set_errno(EVMSERR);
          set_vaxc_errno(dirfab.fab$l_sts);
          return NULL;
        }
      }
      else {
        savnam = dirnam;
	/* Does the file really exist? */
        if (sys$search(&dirfab)& STS$K_SUCCESS) { 
          /* Yes; fake the fnb bits so we'll check type below */
	rms_set_nam_fnb(dirnam, (NAM$M_EXP_TYPE | NAM$M_EXP_VER));
        }
        else { /* No; just work with potential name */
          if (dirfab.fab$l_sts == RMS$_FNF) dirnam = savnam;
          else { 
	    int fab_sts;
	    fab_sts = dirfab.fab$l_sts;
	    sts = rms_free_search_context(&dirfab);
	    PerlMem_free(esa);
	    if (esal != NULL)
		PerlMem_free(esal);
	    PerlMem_free(trndir);
	    PerlMem_free(vmsdir);
            set_errno(EVMSERR);  set_vaxc_errno(fab_sts);
            return NULL;
          }
        }
      }

      /* Make sure we are using the right buffer */
      if (esal != NULL) {
	my_esa = esal;
	my_esa_len = rms_nam_esll(dirnam);
      } else {
	my_esa = esa;
        my_esa_len = rms_nam_esl(dirnam);
      }
      my_esa[my_esa_len] = '\0';
      if (!rms_is_nam_fnb(dirnam, (NAM$M_EXP_DEV | NAM$M_EXP_DIR))) {
        cp1 = strchr(my_esa,']');
        if (!cp1) cp1 = strchr(my_esa,'>');
        if (cp1) {  /* Should always be true */
          my_esa_len -= cp1 - my_esa - 1;
          memmove(my_esa, cp1 + 1, my_esa_len);
        }
      }
      if (rms_is_nam_fnb(dirnam, NAM$M_EXP_TYPE)) {  /* Was type specified? */
        /* Yep; check version while we're at it, if it's there. */
        cmplen = rms_is_nam_fnb(dirnam, NAM$M_EXP_VER) ? 6 : 4;
        if (strncmp(rms_nam_typel(dirnam), ".DIR;1", cmplen)) { 
          /* Something other than .DIR[;1].  Bzzt. */
	  sts = rms_free_search_context(&dirfab);
	  PerlMem_free(esa);
	  if (esal != NULL)
	     PerlMem_free(esal);
	  PerlMem_free(trndir);
	  PerlMem_free(vmsdir);
          set_errno(ENOTDIR);
          set_vaxc_errno(RMS$_DIR);
          return NULL;
        }
      }

      if (rms_is_nam_fnb(dirnam, NAM$M_EXP_NAME)) {
        /* They provided at least the name; we added the type, if necessary, */
        if (buf) retspec = buf;                            /* in sys$parse() */
        else if (ts) Newx(retspec, my_esa_len + 1, char);
        else retspec = __fileify_retbuf;
        strcpy(retspec,my_esa);
	sts = rms_free_search_context(&dirfab);
	PerlMem_free(trndir);
	PerlMem_free(esa);
	if (esal != NULL)
	    PerlMem_free(esal);
	PerlMem_free(vmsdir);
        return retspec;
      }
      if ((cp1 = strstr(esa,".][000000]")) != NULL) {
        for (cp2 = cp1 + 9; *cp2; cp1++,cp2++) *cp1 = *cp2;
        *cp1 = '\0';
        my_esa_len -= 9;
      }
      if ((cp1 = strrchr(my_esa,']')) == NULL) cp1 = strrchr(my_esa,'>');
      if (cp1 == NULL) { /* should never happen */
	sts = rms_free_search_context(&dirfab);
	PerlMem_free(trndir);
	PerlMem_free(esa);
	if (esal != NULL)
	    PerlMem_free(esal);
	PerlMem_free(vmsdir);
        return NULL;
      }
      term = *cp1;
      *cp1 = '\0';
      retlen = strlen(my_esa);
      cp1 = strrchr(my_esa,'.');
      /* ODS-5 directory specifications can have extra "." in them. */
      /* Fix-me, can not scan EFS file specifications backwards */
      while (cp1 != NULL) {
        if ((cp1-1 == my_esa) || (*(cp1-1) != '^'))
	  break;
	else {
	   cp1--;
	   while ((cp1 > my_esa) && (*cp1 != '.'))
	     cp1--;
	}
	if (cp1 == my_esa)
	  cp1 = NULL;
      }

      if ((cp1) != NULL) {
        /* There's more than one directory in the path.  Just roll back. */
        *cp1 = term;
        if (buf) retspec = buf;
        else if (ts) Newx(retspec,retlen+7,char);
        else retspec = __fileify_retbuf;
        strcpy(retspec,my_esa);
      }
      else {
        if (rms_is_nam_fnb(dirnam, NAM$M_ROOT_DIR)) {
          /* Go back and expand rooted logical name */
          rms_set_nam_nop(dirnam, NAM$M_SYNCHK | NAM$M_NOCONCEAL);
#ifdef NAM$M_NO_SHORT_UPCASE
	  if (decc_efs_case_preserve)
	    rms_set_nam_nop(dirnam, NAM$M_NO_SHORT_UPCASE);
#endif
          if (!(sys$parse(&dirfab) & STS$K_SUCCESS)) {
	    sts = rms_free_search_context(&dirfab);
	    PerlMem_free(esa);
	    if (esal != NULL)
		PerlMem_free(esal);
	    PerlMem_free(trndir);
	    PerlMem_free(vmsdir);
            set_errno(EVMSERR);
            set_vaxc_errno(dirfab.fab$l_sts);
            return NULL;
          }

	  /* This changes the length of the string of course */
	  if (esal != NULL) {
	      my_esa_len = rms_nam_esll(dirnam);
	  } else {
	      my_esa_len = rms_nam_esl(dirnam);
	  }

          retlen = my_esa_len - 9; /* esa - '][' - '].DIR;1' */
          if (buf) retspec = buf;
          else if (ts) Newx(retspec,retlen+16,char);
          else retspec = __fileify_retbuf;
          cp1 = strstr(my_esa,"][");
          if (!cp1) cp1 = strstr(my_esa,"]<");
          dirlen = cp1 - my_esa;
          memcpy(retspec,my_esa,dirlen);
          if (!strncmp(cp1+2,"000000]",7)) {
            retspec[dirlen-1] = '\0';
	    /* fix-me Not full ODS-5, just extra dots in directories for now */
	    cp1 = retspec + dirlen - 1;
	    while (cp1 > retspec)
	    {
	      if (*cp1 == '[')
		break;
	      if (*cp1 == '.') {
		if (*(cp1-1) != '^')
		  break;
	      }
	      cp1--;
	    }
            if (*cp1 == '.') *cp1 = ']';
            else {
              memmove(cp1+8,cp1+1,retspec+dirlen-cp1);
              memmove(cp1+1,"000000]",7);
            }
          }
          else {
            memmove(retspec+dirlen,cp1+2,retlen-dirlen);
            retspec[retlen] = '\0';
            /* Convert last '.' to ']' */
            cp1 = retspec+retlen-1;
	    while (*cp != '[') {
	      cp1--;
	      if (*cp1 == '.') {
		/* Do not trip on extra dots in ODS-5 directories */
		if ((cp1 == retspec) || (*(cp1-1) != '^'))
		break;
	      }
	    }
            if (*cp1 == '.') *cp1 = ']';
            else {
              memmove(cp1+8,cp1+1,retspec+dirlen-cp1);
              memmove(cp1+1,"000000]",7);
            }
          }
        }
        else {  /* This is a top-level dir.  Add the MFD to the path. */
          if (buf) retspec = buf;
          else if (ts) Newx(retspec,retlen+16,char);
          else retspec = __fileify_retbuf;
          cp1 = my_esa;
          cp2 = retspec;
          while ((*cp1 != ':')  && (*cp1 != '\0')) *(cp2++) = *(cp1++);
          strcpy(cp2,":[000000]");
          cp1 += 2;
          strcpy(cp2+9,cp1);
        }
      }
      sts = rms_free_search_context(&dirfab);
      /* We've set up the string up through the filename.  Add the
         type and version, and we're done. */
      strcat(retspec,".DIR;1");

      /* $PARSE may have upcased filespec, so convert output to lower
       * case if input contained any lowercase characters. */
      if (haslower && !decc_efs_case_preserve) __mystrtolower(retspec);
      PerlMem_free(trndir);
      PerlMem_free(esa);
      if (esal != NULL)
	PerlMem_free(esal);
      PerlMem_free(vmsdir);
      return retspec;
    }
}  /* end of do_fileify_dirspec() */
/*}}}*/
/* External entry points */
char *Perl_fileify_dirspec(pTHX_ const char *dir, char *buf)
{ return do_fileify_dirspec(dir,buf,0,NULL); }
char *Perl_fileify_dirspec_ts(pTHX_ const char *dir, char *buf)
{ return do_fileify_dirspec(dir,buf,1,NULL); }
char *Perl_fileify_dirspec_utf8(pTHX_ const char *dir, char *buf, int * utf8_fl)
{ return do_fileify_dirspec(dir,buf,0,utf8_fl); }
char *Perl_fileify_dirspec_utf8_ts(pTHX_ const char *dir, char *buf, int * utf8_fl)
{ return do_fileify_dirspec(dir,buf,1,utf8_fl); }

/*{{{ char *pathify_dirspec[_ts](char *path, char *buf)*/
static char *mp_do_pathify_dirspec(pTHX_ const char *dir,char *buf, int ts, int * utf8_fl)
{
    static char __pathify_retbuf[VMS_MAXRSS];
    unsigned long int retlen;
    char *retpath, *cp1, *cp2, *trndir;
    unsigned short int trnlnm_iter_count;
    STRLEN trnlen;
    int sts;
    if (utf8_fl != NULL)
	*utf8_fl = 0;

    if (!dir || !*dir) {
      set_errno(EINVAL); set_vaxc_errno(SS$_BADPARAM); return NULL;
    }

    trndir = PerlMem_malloc(VMS_MAXRSS);
    if (trndir == NULL) _ckvmssts(SS$_INSFMEM);
    if (*dir) strcpy(trndir,dir);
    else getcwd(trndir,VMS_MAXRSS - 1);

    trnlnm_iter_count = 0;
    while (!strpbrk(trndir,"/]:>") && !no_translate_barewords
	   && my_trnlnm(trndir,trndir,0)) {
      trnlnm_iter_count++; 
      if (trnlnm_iter_count >= PERL_LNM_MAX_ITER) break;
      trnlen = strlen(trndir);

      /* Trap simple rooted lnms, and return lnm:[000000] */
      if (!strcmp(trndir+trnlen-2,".]")) {
        if (buf) retpath = buf;
        else if (ts) Newx(retpath,strlen(dir)+10,char);
        else retpath = __pathify_retbuf;
        strcpy(retpath,dir);
        strcat(retpath,":[000000]");
	PerlMem_free(trndir);
        return retpath;
      }
    }

    /* At this point we do not work with *dir, but the copy in
     * *trndir that is modifiable.
     */

    if (!strpbrk(trndir,"]:>")) { /* Unix-style path or plain name */
      if (*trndir == '.' && (*(trndir+1) == '\0' ||
                          (*(trndir+1) == '.' && *(trndir+2) == '\0')))
        retlen = 2 + (*(trndir+1) != '\0');
      else {
        if ( !(cp1 = strrchr(trndir,'/')) &&
             !(cp1 = strrchr(trndir,']')) &&
             !(cp1 = strrchr(trndir,'>')) ) cp1 = trndir;
        if ((cp2 = strchr(cp1,'.')) != NULL &&
            (*(cp2-1) != '/' ||                /* Trailing '.', '..', */
             !(*(cp2+1) == '\0' ||             /* or '...' are dirs.  */
              (*(cp2+1) == '.' && *(cp2+2) == '\0') ||
              (*(cp2+1) == '.' && *(cp2+2) == '.' && *(cp2+3) == '\0')))) {
          int ver; char *cp3;

	  /* For EFS or ODS-5 look for the last dot */
	  if (decc_efs_charset) {
	    cp2 = strrchr(cp1,'.');
	  }
	  if (vms_process_case_tolerant) {
              if (!*(cp2+1) || toupper(*(cp2+1)) != 'D' ||  /* Wrong type. */
                  !*(cp2+2) || toupper(*(cp2+2)) != 'I' ||  /* Bzzt. */
                  !*(cp2+3) || toupper(*(cp2+3)) != 'R' ||
                  (*(cp2+4) && ((*(cp2+4) != ';' && *(cp2+4) != '.')  ||
                  (*(cp2+5) && ((ver = strtol(cp2+5,&cp3,10)) != 1 &&
                            (ver || *cp3)))))) {
		PerlMem_free(trndir);
                set_errno(ENOTDIR);
                set_vaxc_errno(RMS$_DIR);
                return NULL;
              }
	  }
	  else {
              if (!*(cp2+1) || *(cp2+1) != 'D' ||  /* Wrong type. */
                  !*(cp2+2) || *(cp2+2) != 'I' ||  /* Bzzt. */
                  !*(cp2+3) || *(cp2+3) != 'R' ||
                  (*(cp2+4) && ((*(cp2+4) != ';' && *(cp2+4) != '.')  ||
                  (*(cp2+5) && ((ver = strtol(cp2+5,&cp3,10)) != 1 &&
                            (ver || *cp3)))))) {
		PerlMem_free(trndir);
                set_errno(ENOTDIR);
                set_vaxc_errno(RMS$_DIR);
                return NULL;
              }
	  }
          retlen = cp2 - trndir + 1;
        }
        else {  /* No file type present.  Treat the filename as a directory. */
          retlen = strlen(trndir) + 1;
        }
      }
      if (buf) retpath = buf;
      else if (ts) Newx(retpath,retlen+1,char);
      else retpath = __pathify_retbuf;
      strncpy(retpath, trndir, retlen-1);
      if (retpath[retlen-2] != '/') { /* If the path doesn't already end */
        retpath[retlen-1] = '/';      /* with '/', add it. */
        retpath[retlen] = '\0';
      }
      else retpath[retlen-1] = '\0';
    }
    else {  /* VMS-style directory spec */
      char *esa, *esal, *cp;
      char *my_esa;
      int my_esa_len;
      unsigned long int sts, cmplen, haslower;
      struct FAB dirfab = cc$rms_fab;
      int dirlen;
      rms_setup_nam(savnam);
      rms_setup_nam(dirnam);

      /* If we've got an explicit filename, we can just shuffle the string. */
      if ( ( (cp1 = strrchr(trndir,']')) != NULL ||
             (cp1 = strrchr(trndir,'>')) != NULL     ) && *(cp1+1)) {
        if ((cp2 = strchr(cp1,'.')) != NULL) {
          int ver; char *cp3;
	  if (vms_process_case_tolerant) {
              if (!*(cp2+1) || toupper(*(cp2+1)) != 'D' ||  /* Wrong type. */
                  !*(cp2+2) || toupper(*(cp2+2)) != 'I' ||  /* Bzzt. */
                  !*(cp2+3) || toupper(*(cp2+3)) != 'R' ||
                  (*(cp2+4) && ((*(cp2+4) != ';' && *(cp2+4) != '.')  ||
                  (*(cp2+5) && ((ver = strtol(cp2+5,&cp3,10)) != 1 &&
                            (ver || *cp3)))))) {
	       PerlMem_free(trndir);
               set_errno(ENOTDIR);
               set_vaxc_errno(RMS$_DIR);
               return NULL;
             }
	  }
	  else {
              if (!*(cp2+1) || *(cp2+1) != 'D' ||  /* Wrong type. */
                  !*(cp2+2) || *(cp2+2) != 'I' ||  /* Bzzt. */
                  !*(cp2+3) || *(cp2+3) != 'R' ||
                  (*(cp2+4) && ((*(cp2+4) != ';' && *(cp2+4) != '.')  ||
                  (*(cp2+5) && ((ver = strtol(cp2+5,&cp3,10)) != 1 &&
                            (ver || *cp3)))))) {
	       PerlMem_free(trndir);
               set_errno(ENOTDIR);
               set_vaxc_errno(RMS$_DIR);
               return NULL;
             }
	  }
        }
        else {  /* No file type, so just draw name into directory part */
          for (cp2 = cp1; *cp2; cp2++) ;
        }
        *cp2 = *cp1;
        *(cp2+1) = '\0';  /* OK; trndir is guaranteed to be long enough */
        *cp1 = '.';
        /* We've now got a VMS 'path'; fall through */
      }

      dirlen = strlen(trndir);
      if (trndir[dirlen-1] == ']' ||
          trndir[dirlen-1] == '>' ||
          trndir[dirlen-1] == ':') { /* It's already a VMS 'path' */
        if (buf) retpath = buf;
        else if (ts) Newx(retpath,strlen(trndir)+1,char);
        else retpath = __pathify_retbuf;
        strcpy(retpath,trndir);
	PerlMem_free(trndir);
        return retpath;
      }
      rms_set_fna(dirfab, dirnam, trndir, dirlen);
      esa = PerlMem_malloc(VMS_MAXRSS);
      if (esa == NULL) _ckvmssts(SS$_INSFMEM);
      esal = NULL;
#if !defined(__VAX) && defined(NAML$C_MAXRSS)
      esal = PerlMem_malloc(VMS_MAXRSS);
      if (esal == NULL) _ckvmssts(SS$_INSFMEM);
#endif
      rms_set_dna(dirfab, dirnam, ".DIR;1", 6);
      rms_bind_fab_nam(dirfab, dirnam);
      rms_set_esal(dirnam, esa, NAM$C_MAXRSS, esal, VMS_MAXRSS-1);
#ifdef NAM$M_NO_SHORT_UPCASE
      if (decc_efs_case_preserve)
	  rms_set_nam_nop(dirnam, NAM$M_NO_SHORT_UPCASE);
#endif

      for (cp = trndir; *cp; cp++)
        if (islower(*cp)) { haslower = 1; break; }

      if (!(sts = (sys$parse(&dirfab)& STS$K_SUCCESS))) {
        if ((dirfab.fab$l_sts == RMS$_DIR) || (dirfab.fab$l_sts == RMS$_DNF)) {
	  rms_set_nam_nop(dirnam, NAM$M_SYNCHK);
          sts = sys$parse(&dirfab) & STS$K_SUCCESS;
        }
        if (!sts) {
	  PerlMem_free(trndir);
	  PerlMem_free(esa);
	  if (esal != NULL)
	    PerlMem_free(esal);
          set_errno(EVMSERR);
          set_vaxc_errno(dirfab.fab$l_sts);
          return NULL;
        }
      }
      else {
        savnam = dirnam;
	/* Does the file really exist? */
        if (!(sys$search(&dirfab)&STS$K_SUCCESS)) {
          if (dirfab.fab$l_sts != RMS$_FNF) {
	    int sts1;
	    sts1 = rms_free_search_context(&dirfab);
	    PerlMem_free(trndir);
	    PerlMem_free(esa);
	    if (esal != NULL)
		PerlMem_free(esal);
            set_errno(EVMSERR);
            set_vaxc_errno(dirfab.fab$l_sts);
            return NULL;
          }
          dirnam = savnam; /* No; just work with potential name */
        }
      }
      if (rms_is_nam_fnb(dirnam, NAM$M_EXP_TYPE)) {  /* Was type specified? */
        /* Yep; check version while we're at it, if it's there. */
        cmplen = rms_is_nam_fnb(dirnam, NAM$M_EXP_VER) ? 6 : 4;
        if (strncmp(rms_nam_typel(dirnam),".DIR;1",cmplen)) {
	  int sts2;
          /* Something other than .DIR[;1].  Bzzt. */
	  sts2 = rms_free_search_context(&dirfab);
	  PerlMem_free(trndir);
	  PerlMem_free(esa);
	  if (esal != NULL)
	     PerlMem_free(esal);
          set_errno(ENOTDIR);
          set_vaxc_errno(RMS$_DIR);
          return NULL;
        }
      }
      /* Make sure we are using the right buffer */
      if (esal != NULL) {
	/* We only need one, clean up the other */
	my_esa = esal;
	my_esa_len = rms_nam_esll(dirnam);
      } else {
	my_esa = esa;
        my_esa_len = rms_nam_esl(dirnam);
      }

      /* Null terminate the buffer */
      my_esa[my_esa_len] = '\0';

      /* OK, the type was fine.  Now pull any file name into the
         directory path. */
      if ((cp1 = strrchr(my_esa,']'))) *(rms_nam_typel(dirnam)) = ']';
      else {
        cp1 = strrchr(my_esa,'>');
        *(rms_nam_typel(dirnam)) = '>';
      }
      *cp1 = '.';
      *(rms_nam_typel(dirnam) + 1) = '\0';
      retlen = (rms_nam_typel(dirnam)) - my_esa + 2;
      if (buf) retpath = buf;
      else if (ts) Newx(retpath,retlen,char);
      else retpath = __pathify_retbuf;
      strcpy(retpath,my_esa);
      PerlMem_free(esa);
      if (esal != NULL)
	  PerlMem_free(esal);
      sts = rms_free_search_context(&dirfab);
      /* $PARSE may have upcased filespec, so convert output to lower
       * case if input contained any lowercase characters. */
      if (haslower && !decc_efs_case_preserve) __mystrtolower(retpath);
    }

    PerlMem_free(trndir);
    return retpath;
}  /* end of do_pathify_dirspec() */
/*}}}*/
/* External entry points */
char *Perl_pathify_dirspec(pTHX_ const char *dir, char *buf)
{ return do_pathify_dirspec(dir,buf,0,NULL); }
char *Perl_pathify_dirspec_ts(pTHX_ const char *dir, char *buf)
{ return do_pathify_dirspec(dir,buf,1,NULL); }
char *Perl_pathify_dirspec_utf8(pTHX_ const char *dir, char *buf, int *utf8_fl)
{ return do_pathify_dirspec(dir,buf,0,utf8_fl); }
char *Perl_pathify_dirspec_utf8_ts(pTHX_ const char *dir, char *buf, int *utf8_fl)
{ return do_pathify_dirspec(dir,buf,1,utf8_fl); }

/*{{{ char *tounixspec[_ts](char *spec, char *buf, int *)*/
static char *mp_do_tounixspec(pTHX_ const char *spec, char *buf, int ts, int * utf8_fl)
{
  static char __tounixspec_retbuf[VMS_MAXRSS];
  char *dirend, *rslt, *cp1, *cp3, *tmp;
  const char *cp2;
  int devlen, dirlen, retlen = VMS_MAXRSS;
  int expand = 1; /* guarantee room for leading and trailing slashes */
  unsigned short int trnlnm_iter_count;
  int cmp_rslt;
  if (utf8_fl != NULL)
    *utf8_fl = 0;

  if (spec == NULL) return NULL;
  if (strlen(spec) > (VMS_MAXRSS-1)) return NULL;
  if (buf) rslt = buf;
  else if (ts) {
    Newx(rslt, VMS_MAXRSS, char);
  }
  else rslt = __tounixspec_retbuf;

  /* New VMS specific format needs translation
   * glob passes filenames with trailing '\n' and expects this preserved.
   */
  if (decc_posix_compliant_pathnames) {
    if (strncmp(spec, "\"^UP^", 5) == 0) {
      char * uspec;
      char *tunix;
      int tunix_len;
      int nl_flag;

      tunix = PerlMem_malloc(VMS_MAXRSS);
      if (tunix == NULL) _ckvmssts(SS$_INSFMEM);
      strcpy(tunix, spec);
      tunix_len = strlen(tunix);
      nl_flag = 0;
      if (tunix[tunix_len - 1] == '\n') {
	tunix[tunix_len - 1] = '\"';
	tunix[tunix_len] = '\0';
	tunix_len--;
	nl_flag = 1;
      }
      uspec = decc$translate_vms(tunix);
      PerlMem_free(tunix);
      if ((int)uspec > 0) {
	strcpy(rslt,uspec);
	if (nl_flag) {
	  strcat(rslt,"\n");
	}
	else {
	  /* If we can not translate it, makemaker wants as-is */
	  strcpy(rslt, spec);
	}
	return rslt;
      }
    }
  }

  cmp_rslt = 0; /* Presume VMS */
  cp1 = strchr(spec, '/');
  if (cp1 == NULL)
    cmp_rslt = 0;

    /* Look for EFS ^/ */
    if (decc_efs_charset) {
      while (cp1 != NULL) {
	cp2 = cp1 - 1;
	if (*cp2 != '^') {
	  /* Found illegal VMS, assume UNIX */
	  cmp_rslt = 1;
	  break;
	}
      cp1++;
      cp1 = strchr(cp1, '/');
    }
  }

  /* Look for "." and ".." */
  if (decc_filename_unix_report) {
    if (spec[0] == '.') {
      if ((spec[1] == '\0') || (spec[1] == '\n')) {
	cmp_rslt = 1;
      }
      else {
	if ((spec[1] == '.') && ((spec[2] == '\0') || (spec[2] == '\n'))) {
	  cmp_rslt = 1;
	}
      }
    }
  }
  /* This is already UNIX or at least nothing VMS understands */
  if (cmp_rslt) {
    strcpy(rslt,spec);
    return rslt;
  }

  cp1 = rslt;
  cp2 = spec;
  dirend = strrchr(spec,']');
  if (dirend == NULL) dirend = strrchr(spec,'>');
  if (dirend == NULL) dirend = strchr(spec,':');
  if (dirend == NULL) {
    strcpy(rslt,spec);
    return rslt;
  }

  /* Special case 1 - sys$posix_root = / */
#if __CRTL_VER >= 70000000
  if (!decc_disable_posix_root) {
    if (strncasecmp(spec, "SYS$POSIX_ROOT:", 15) == 0) {
      *cp1 = '/';
      cp1++;
      cp2 = cp2 + 15;
      }
  }
#endif

  /* Special case 2 - Convert NLA0: to /dev/null */
#if __CRTL_VER < 70000000
  cmp_rslt = strncmp(spec,"NLA0:", 5);
  if (cmp_rslt != 0)
     cmp_rslt = strncmp(spec,"nla0:", 5);
#else
  cmp_rslt = strncasecmp(spec,"NLA0:", 5);
#endif
  if (cmp_rslt == 0) {
    strcpy(rslt, "/dev/null");
    cp1 = cp1 + 9;
    cp2 = cp2 + 5;
    if (spec[6] != '\0') {
      cp1[9] == '/';
      cp1++;
      cp2++;
    }
  }

   /* Also handle special case "SYS$SCRATCH:" */
#if __CRTL_VER < 70000000
  cmp_rslt = strncmp(spec,"SYS$SCRATCH:", 12);
  if (cmp_rslt != 0)
     cmp_rslt = strncmp(spec,"sys$scratch:", 12);
#else
  cmp_rslt = strncasecmp(spec,"SYS$SCRATCH:", 12);
#endif
  tmp = PerlMem_malloc(VMS_MAXRSS);
  if (tmp == NULL) _ckvmssts(SS$_INSFMEM);
  if (cmp_rslt == 0) {
  int islnm;

    islnm = my_trnlnm(tmp, "TMP", 0);
    if (!islnm) {
      strcpy(rslt, "/tmp");
      cp1 = cp1 + 4;
      cp2 = cp2 + 12;
      if (spec[12] != '\0') {
	cp1[4] == '/';
	cp1++;
	cp2++;
      }
    }
  }

  if (*cp2 != '[' && *cp2 != '<') {
    *(cp1++) = '/';
  }
  else {  /* the VMS spec begins with directories */
    cp2++;
    if (*cp2 == ']' || *cp2 == '>') {
      *(cp1++) = '.'; *(cp1++) = '/'; *(cp1++) = '\0';
      PerlMem_free(tmp);
      return rslt;
    }
    else if ( *cp2 != '^' && *cp2 != '.' && *cp2 != '-') { /* add the implied device */
      if (getcwd(tmp, VMS_MAXRSS-1 ,1) == NULL) {
        if (ts) Safefree(rslt);
	PerlMem_free(tmp);
        return NULL;
      }
      trnlnm_iter_count = 0;
      do {
        cp3 = tmp;
        while (*cp3 != ':' && *cp3) cp3++;
        *(cp3++) = '\0';
        if (strchr(cp3,']') != NULL) break;
        trnlnm_iter_count++; 
        if (trnlnm_iter_count >= PERL_LNM_MAX_ITER+1) break;
      } while (vmstrnenv(tmp,tmp,0,fildev,0));
      if (ts && !buf &&
          ((devlen = strlen(tmp)) + (dirlen = strlen(cp2)) + 1 > retlen)) {
        retlen = devlen + dirlen;
        Renew(rslt,retlen+1+2*expand,char);
        cp1 = rslt;
      }
      cp3 = tmp;
      *(cp1++) = '/';
      while (*cp3) {
        *(cp1++) = *(cp3++);
        if (cp1 - rslt > (VMS_MAXRSS - 1) && !ts && !buf) {
	    PerlMem_free(tmp);
	    return NULL; /* No room */
	}
      }
      *(cp1++) = '/';
    }
    if ((*cp2 == '^')) {
	/* EFS file escape, pass the next character as is */
	/* Fix me: HEX encoding for Unicode not implemented */
	cp2++;
    }
    else if ( *cp2 == '.') {
      if (*(cp2+1) == '.' && *(cp2+2) == '.') {
        *(cp1++) = '.'; *(cp1++) = '.'; *(cp1++) = '.'; *(cp1++) = '/';
        cp2 += 3;
      }
      else cp2++;
    }
  }
  PerlMem_free(tmp);
  for (; cp2 <= dirend; cp2++) {
    if ((*cp2 == '^')) {
	/* EFS file escape, pass the next character as is */
	/* Fix me: HEX encoding for Unicode not implemented */
	*(cp1++) = *(++cp2);
        /* An escaped dot stays as is -- don't convert to slash */
        if (*cp2 == '.') cp2++;
    }
    if (*cp2 == ':') {
      *(cp1++) = '/';
      if (*(cp2+1) == '[') cp2++;
    }
    else if (*cp2 == ']' || *cp2 == '>') {
      if (*(cp1-1) != '/') *(cp1++) = '/'; /* Don't double after ellipsis */
    }
    else if ((*cp2 == '.') && (*cp2-1 != '^')) {
      *(cp1++) = '/';
      if (*(cp2+1) == ']' || *(cp2+1) == '>') {
        while (*(cp2+1) == ']' || *(cp2+1) == '>' ||
               *(cp2+1) == '[' || *(cp2+1) == '<') cp2++;
        if (!strncmp(cp2,"[000000",7) && (*(cp2+7) == ']' ||
            *(cp2+7) == '>' || *(cp2+7) == '.')) cp2 += 7;
      }
      else if ( *(cp2+1) == '.' && *(cp2+2) == '.') {
        *(cp1++) = '.'; *(cp1++) = '.'; *(cp1++) = '.'; *(cp1++) ='/';
        cp2 += 2;
      }
    }
    else if (*cp2 == '-') {
      if (*(cp2-1) == '[' || *(cp2-1) == '<' || *(cp2-1) == '.') {
        while (*cp2 == '-') {
          cp2++;
          *(cp1++) = '.'; *(cp1++) = '.'; *(cp1++) = '/';
        }
        if (*cp2 != '.' && *cp2 != ']' && *cp2 != '>') { /* we don't allow */
          if (ts) Safefree(rslt);                        /* filespecs like */
          set_errno(EINVAL); set_vaxc_errno(RMS$_SYN);   /* [fred.--foo.bar] */
          return NULL;
        }
      }
      else *(cp1++) = *cp2;
    }
    else *(cp1++) = *cp2;
  }
  while (*cp2) {
    if ((*cp2 == '^') && (*(cp2+1) == '.')) cp2++;  /* '^.' --> '.' */
    *(cp1++) = *(cp2++);
  }
  *cp1 = '\0';

  /* This still leaves /000000/ when working with a
   * VMS device root or concealed root.
   */
  {
  int ulen;
  char * zeros;

      ulen = strlen(rslt);

      /* Get rid of "000000/ in rooted filespecs */
      if (ulen > 7) {
	zeros = strstr(rslt, "/000000/");
	if (zeros != NULL) {
	  int mlen;
	  mlen = ulen - (zeros - rslt) - 7;
	  memmove(zeros, &zeros[7], mlen);
	  ulen = ulen - 7;
	  rslt[ulen] = '\0';
	}
      }
  }

  return rslt;

}  /* end of do_tounixspec() */
/*}}}*/
/* External entry points */
char *Perl_tounixspec(pTHX_ const char *spec, char *buf)
  { return do_tounixspec(spec,buf,0, NULL); }
char *Perl_tounixspec_ts(pTHX_ const char *spec, char *buf)
  { return do_tounixspec(spec,buf,1, NULL); }
char *Perl_tounixspec_utf8(pTHX_ const char *spec, char *buf, int * utf8_fl)
  { return do_tounixspec(spec,buf,0, utf8_fl); }
char *Perl_tounixspec_utf8_ts(pTHX_ const char *spec, char *buf, int * utf8_fl)
  { return do_tounixspec(spec,buf,1, utf8_fl); }

#if __CRTL_VER >= 70200000 && !defined(__VAX)

/*
 This procedure is used to identify if a path is based in either
 the old SYS$POSIX_ROOT: or the new 8.3 RMS based POSIX root, and
 it returns the OpenVMS format directory for it.

 It is expecting specifications of only '/' or '/xxxx/'

 If a posix root does not exist, or 'xxxx' is not a directory
 in the posix root, it returns a failure.

 FIX-ME: xxxx could be in UTF-8 and needs to be returned in VTF-7.

 It is used only internally by posix_to_vmsspec_hardway().
 */

static int posix_root_to_vms
  (char *vmspath, int vmspath_len,
   const char *unixpath,
   const int * utf8_fl)
{
int sts;
struct FAB myfab = cc$rms_fab;
rms_setup_nam(mynam);
struct dsc$descriptor_s dvidsc = {0, DSC$K_DTYPE_T, DSC$K_CLASS_S, 0};
struct dsc$descriptor_s specdsc = {0, DSC$K_DTYPE_T, DSC$K_CLASS_S, 0};
char * esa, * esal, * rsa, * rsal;
char *vms_delim;
int dir_flag;
int unixlen;

    dir_flag = 0;
    vmspath[0] = '\0';
    unixlen = strlen(unixpath);
    if (unixlen == 0) {
      return RMS$_FNF;
    }

#if __CRTL_VER >= 80200000
  /* If not a posix spec already, convert it */
  if (decc_posix_compliant_pathnames) {
    if (strncmp(unixpath,"\"^UP^",5) != 0) {
      sprintf(vmspath,"\"^UP^%s\"",unixpath);
    }
    else {
      /* This is already a VMS specification, no conversion */
      unixlen--;
      strncpy(vmspath,unixpath, vmspath_len);
    }
  }
  else
#endif
  {     
  int path_len;
  int i,j;

     /* Check to see if this is under the POSIX root */
     if (decc_disable_posix_root) {
	return RMS$_FNF;
     }

     /* Skip leading / */
     if (unixpath[0] == '/') {
	unixpath++;
	unixlen--;
     }


     strcpy(vmspath,"SYS$POSIX_ROOT:");

     /* If this is only the / , or blank, then... */
     if (unixpath[0] == '\0') {
	/* by definition, this is the answer */
	return SS$_NORMAL;
     }

     /* Need to look up a directory */
     vmspath[15] = '[';
     vmspath[16] = '\0';

     /* Copy and add '^' escape characters as needed */
     j = 16;
     i = 0;
     while (unixpath[i] != 0) {
     int k;

	j += copy_expand_unix_filename_escape
	    (&vmspath[j], &unixpath[i], &k, utf8_fl);
	i += k;
     }

     path_len = strlen(vmspath);
     if (vmspath[path_len - 1] == '/')
	path_len--;
     vmspath[path_len] = ']';
     path_len++;
     vmspath[path_len] = '\0';
	
  }
  vmspath[vmspath_len] = 0;
  if (unixpath[unixlen - 1] == '/')
  dir_flag = 1;
  esal = PerlMem_malloc(VMS_MAXRSS);
  if (esal == NULL) _ckvmssts_noperl(SS$_INSFMEM);
  esa = PerlMem_malloc(NAM$C_MAXRSS + 1);
  if (esa == NULL) _ckvmssts_noperl(SS$_INSFMEM);
  rsal = PerlMem_malloc(VMS_MAXRSS);
  if (rsal == NULL) _ckvmssts_noperl(SS$_INSFMEM);
  rsa = PerlMem_malloc(NAM$C_MAXRSS + 1);
  if (rsa == NULL) _ckvmssts_noperl(SS$_INSFMEM);
  rms_set_fna(myfab, mynam, (char *) vmspath, strlen(vmspath)); /* cast ok */
  rms_bind_fab_nam(myfab, mynam);
  rms_set_esal(mynam, esa, NAM$C_MAXRSS, esal, VMS_MAXRSS - 1);
  rms_set_rsal(mynam, rsa, NAM$C_MAXRSS, rsal, VMS_MAXRSS - 1);
  if (decc_efs_case_preserve)
    mynam.naml$b_nop |= NAM$M_NO_SHORT_UPCASE;
#ifdef NAML$M_OPEN_SPECIAL
  mynam.naml$l_input_flags |= NAML$M_OPEN_SPECIAL;
#endif

  /* Set up the remaining naml fields */
  sts = sys$parse(&myfab);

  /* It failed! Try again as a UNIX filespec */
  if (!(sts & 1)) {
    PerlMem_free(esal);
    PerlMem_free(esa);
    PerlMem_free(rsal);
    PerlMem_free(rsa);
    return sts;
  }

   /* get the Device ID and the FID */
   sts = sys$search(&myfab);

   /* These are no longer needed */
   PerlMem_free(esa);
   PerlMem_free(rsal);
   PerlMem_free(rsa);

   /* on any failure, returned the POSIX ^UP^ filespec */
   if (!(sts & 1)) {
      PerlMem_free(esal);
      return sts;
   }
   specdsc.dsc$a_pointer = vmspath;
   specdsc.dsc$w_length = vmspath_len;
 
   dvidsc.dsc$a_pointer = &mynam.naml$t_dvi[1];
   dvidsc.dsc$w_length = mynam.naml$t_dvi[0];
   sts = lib$fid_to_name
      (&dvidsc, mynam.naml$w_fid, &specdsc, &specdsc.dsc$w_length);

  /* on any failure, returned the POSIX ^UP^ filespec */
  if (!(sts & 1)) {
     /* This can happen if user does not have permission to read directories */
     if (strncmp(unixpath,"\"^UP^",5) != 0)
       sprintf(vmspath,"\"^UP^%s\"",unixpath);
     else
       strcpy(vmspath, unixpath);
  }
  else {
    vmspath[specdsc.dsc$w_length] = 0;

    /* Are we expecting a directory? */
    if (dir_flag != 0) {
    int i;
    char *eptr;

      eptr = NULL;

      i = specdsc.dsc$w_length - 1;
      while (i > 0) {
      int zercnt;
	zercnt = 0;
	/* Version must be '1' */
	if (vmspath[i--] != '1')
	  break;
	/* Version delimiter is one of ".;" */
	if ((vmspath[i] != '.') && (vmspath[i] != ';'))
	  break;
	i--;
	if (vmspath[i--] != 'R')
	  break;
	if (vmspath[i--] != 'I')
	  break;
	if (vmspath[i--] != 'D')
	  break;
	if (vmspath[i--] != '.')
	  break;
	eptr = &vmspath[i+1];
 	while (i > 0) {
	  if ((vmspath[i] == ']') || (vmspath[i] == '>')) {
	    if (vmspath[i-1] != '^') {
	      if (zercnt != 6) {
  		*eptr = vmspath[i];
		eptr[1] = '\0';
		vmspath[i] = '.';
  		break;
	      }
	      else {
 		/* Get rid of 6 imaginary zero directory filename */
  		vmspath[i+1] = '\0';
 	      }
	    }
	  }
	  if (vmspath[i] == '0')
	    zercnt++;
	  else
	    zercnt = 10;
	  i--;
	}
	break;
      }
    }
  }
  PerlMem_free(esal);
  return sts;
}

/* /dev/mumble needs to be handled special.
   /dev/null becomes NLA0:, And there is the potential for other stuff
   like /dev/tty which may need to be mapped to something.
*/

static int 
slash_dev_special_to_vms
   (const char * unixptr,
    char * vmspath,
    int vmspath_len)
{
char * nextslash;
int len;
int cmp;
int islnm;

    unixptr += 4;
    nextslash = strchr(unixptr, '/');
    len = strlen(unixptr);
    if (nextslash != NULL)
	len = nextslash - unixptr;
    cmp = strncmp("null", unixptr, 5);
    if (cmp == 0) {
	if (vmspath_len >= 6) {
	    strcpy(vmspath, "_NLA0:");
	    return SS$_NORMAL;
	}
    }
}


/* The built in routines do not understand perl's special needs, so
    doing a manual conversion from UNIX to VMS

    If the utf8_fl is not null and points to a non-zero value, then
    treat 8 bit characters as UTF-8.

    The sequence starting with '$(' and ending with ')' will be passed
    through with out interpretation instead of being escaped.

  */
static int posix_to_vmsspec_hardway
  (char *vmspath, int vmspath_len,
   const char *unixpath,
   int dir_flag,
   int * utf8_fl) {

char *esa;
const char *unixptr;
const char *unixend;
char *vmsptr;
const char *lastslash;
const char *lastdot;
int unixlen;
int vmslen;
int dir_start;
int dir_dot;
int quoted;
char * v_spec, * r_spec, * d_spec, * n_spec, * e_spec, * vs_spec;
int sts, v_len, r_len, d_len, n_len, e_len, vs_len;

  if (utf8_fl != NULL)
    *utf8_fl = 0;

  unixptr = unixpath;
  dir_dot = 0;

  /* Ignore leading "/" characters */
  while((unixptr[0] == '/') && (unixptr[1] == '/')) {
    unixptr++;
  }
  unixlen = strlen(unixptr);

  /* Do nothing with blank paths */
  if (unixlen == 0) {
    vmspath[0] = '\0';
    return SS$_NORMAL;
  }

  quoted = 0;
  /* This could have a "^UP^ on the front */
  if (strncmp(unixptr,"\"^UP^",5) == 0) {
    quoted = 1;
    unixptr+= 5;
    unixlen-= 5;
  }

  lastslash = strrchr(unixptr,'/');
  lastdot = strrchr(unixptr,'.');
  unixend = strrchr(unixptr,'\"');
  if (!quoted || !((unixend != NULL) && (unixend[1] == '\0'))) {
    unixend = unixptr + unixlen;
  }

  /* last dot is last dot or past end of string */
  if (lastdot == NULL)
    lastdot = unixptr + unixlen;

  /* if no directories, set last slash to beginning of string */
  if (lastslash == NULL) {
    lastslash = unixptr;
  }
  else {
    /* Watch out for trailing "." after last slash, still a directory */
    if ((lastslash[1] == '.') && (lastslash[2] == '\0')) {
      lastslash = unixptr + unixlen;
    }

    /* Watch out for traiing ".." after last slash, still a directory */
    if ((lastslash[1] == '.')&&(lastslash[2] == '.')&&(lastslash[3] == '\0')) {
      lastslash = unixptr + unixlen;
    }

    /* dots in directories are aways escaped */
    if (lastdot < lastslash)
      lastdot = unixptr + unixlen;
  }

  /* if (unixptr < lastslash) then we are in a directory */

  dir_start = 0;

  vmsptr = vmspath;
  vmslen = 0;

  /* Start with the UNIX path */
  if (*unixptr != '/') {
    /* relative paths */

    /* If allowing logical names on relative pathnames, then handle here */
    if ((unixptr[0] != '.') && !decc_disable_to_vms_logname_translation &&
	!decc_posix_compliant_pathnames) {
    char * nextslash;
    int seg_len;
    char * trn;
    int islnm;

	/* Find the next slash */
	nextslash = strchr(unixptr,'/');

	esa = PerlMem_malloc(vmspath_len);
	if (esa == NULL) _ckvmssts_noperl(SS$_INSFMEM);

	trn = PerlMem_malloc(VMS_MAXRSS);
	if (trn == NULL) _ckvmssts_noperl(SS$_INSFMEM);

	if (nextslash != NULL) {

	    seg_len = nextslash - unixptr;
	    strncpy(esa, unixptr, seg_len);
	    esa[seg_len] = 0;
	}
	else {
	    strcpy(esa, unixptr);
	    seg_len = strlen(unixptr);
	}
	/* trnlnm(section) */
	islnm = vmstrnenv(esa, trn, 0, fildev, 0);

	if (islnm) {
	    /* Now fix up the directory */

	    /* Split up the path to find the components */
	    sts = vms_split_path
		  (trn,
		   &v_spec,
		   &v_len,
		   &r_spec,
		   &r_len,
		   &d_spec,
		   &d_len,
		   &n_spec,
		   &n_len,
		   &e_spec,
		   &e_len,
		   &vs_spec,
		   &vs_len);

	    while (sts == 0) {
	    char * strt;
	    int cmp;

		/* A logical name must be a directory  or the full
		   specification.  It is only a full specification if
		   it is the only component */
		if ((unixptr[seg_len] == '\0') ||
		    (unixptr[seg_len+1] == '\0')) {

		    /* Is a directory being required? */
		    if (((n_len + e_len) != 0) && (dir_flag !=0)) {
			/* Not a logical name */
			break;
		    }


		    if ((unixptr[seg_len] == '/') || (dir_flag != 0)) {
			/* This must be a directory */
			if (((n_len + e_len) == 0)&&(seg_len <= vmspath_len)) {
			    strcpy(vmsptr, esa);
			    vmslen=strlen(vmsptr);
			    vmsptr[vmslen] = ':';
			    vmslen++;
			    vmsptr[vmslen] = '\0';
			    return SS$_NORMAL;
			}
		    }

		}


		/* must be dev/directory - ignore version */
		if ((n_len + e_len) != 0)
		    break;

		/* transfer the volume */
		if (v_len > 0 && ((v_len + vmslen) < vmspath_len)) {
		    strncpy(vmsptr, v_spec, v_len);
		    vmsptr += v_len;
		    vmsptr[0] = '\0';
		    vmslen += v_len;
		}

		/* unroot the rooted directory */
		if ((r_len > 0) && ((r_len + d_len + vmslen) < vmspath_len)) {
		    r_spec[0] = '[';
		    r_spec[r_len - 1] = ']';

		    /* This should not be there, but nothing is perfect */
		    if (r_len > 9) {
			cmp = strcmp(&r_spec[1], "000000.");
			if (cmp == 0) {
			    r_spec += 7;
			    r_spec[7] = '[';
			    r_len -= 7;
			    if (r_len == 2)
				r_len = 0;
			}
		    }
		    if (r_len > 0) {
			strncpy(vmsptr, r_spec, r_len);
			vmsptr += r_len;
			vmslen += r_len;
			vmsptr[0] = '\0';
		    }
		}
		/* Bring over the directory. */
		if ((d_len > 0) &&
		    ((d_len + vmslen) < vmspath_len)) {
		    d_spec[0] = '[';
		    d_spec[d_len - 1] = ']';
		    if (d_len > 9) {
			cmp = strcmp(&d_spec[1], "000000.");
			if (cmp == 0) {
			    d_spec += 7;
			    d_spec[7] = '[';
			    d_len -= 7;
			    if (d_len == 2)
				d_len = 0;
			}
		    }

		    if (r_len > 0) {
			/* Remove the redundant root */
			if (r_len > 0) {
			    /* remove the ][ */
			    vmsptr--;
			    vmslen--;
			    d_spec++;
			    d_len--;
			}
			strncpy(vmsptr, d_spec, d_len);
			    vmsptr += d_len;
			    vmslen += d_len;
			    vmsptr[0] = '\0';
		    }
		}
		break;
	    }
	}

	PerlMem_free(esa);
	PerlMem_free(trn);
    }

    if (lastslash > unixptr) {
    int dotdir_seen;

      /* skip leading ./ */
      dotdir_seen = 0;
      while ((unixptr[0] == '.') && (unixptr[1] == '/')) {
	dotdir_seen = 1;
	unixptr++;
	unixptr++;
      }

      /* Are we still in a directory? */
      if (unixptr <= lastslash) {
 	*vmsptr++ = '[';
 	vmslen = 1;
 	dir_start = 1;
 
 	/* if not backing up, then it is relative forward. */
 	if (!((*unixptr == '.') && (unixptr[1] == '.') &&
 	      ((unixptr[2] == '/') || (&unixptr[2] == unixend)))) {
 	  *vmsptr++ = '.';
 	  vmslen++;
 	  dir_dot = 1;
 	  }
       }
       else {
	 if (dotdir_seen) {
	   /* Perl wants an empty directory here to tell the difference
	    * between a DCL commmand and a filename
	    */
	  *vmsptr++ = '[';
	  *vmsptr++ = ']';
	  vmslen = 2;
 	}
      }
    }
    else {
      /* Handle two special files . and .. */
      if (unixptr[0] == '.') {
        if (&unixptr[1] == unixend) {
	  *vmsptr++ = '[';
	  *vmsptr++ = ']';
	  vmslen += 2;
	  *vmsptr++ = '\0';
	  return SS$_NORMAL;
	}
        if ((unixptr[1] == '.') && (&unixptr[2] == unixend)) {
	  *vmsptr++ = '[';
	  *vmsptr++ = '-';
	  *vmsptr++ = ']';
	  vmslen += 3;
	  *vmsptr++ = '\0';
	  return SS$_NORMAL;
	}
      }
    }
  }
  else {	/* Absolute PATH handling */
  int sts;
  char * nextslash;
  int seg_len;
    /* Need to find out where root is */

    /* In theory, this procedure should never get an absolute POSIX pathname
     * that can not be found on the POSIX root.
     * In practice, that can not be relied on, and things will show up
     * here that are a VMS device name or concealed logical name instead.
     * So to make things work, this procedure must be tolerant.
     */
    esa = PerlMem_malloc(vmspath_len);
    if (esa == NULL) _ckvmssts_noperl(SS$_INSFMEM);

    sts = SS$_NORMAL;
    nextslash = strchr(&unixptr[1],'/');
    seg_len = 0;
    if (nextslash != NULL) {
    int cmp;
      seg_len = nextslash - &unixptr[1];
      strncpy(vmspath, unixptr, seg_len + 1);
      vmspath[seg_len+1] = 0;
      cmp = 1;
      if (seg_len == 3) {
	cmp = strncmp(vmspath, "dev", 4);
	if (cmp == 0) {
	    sts = slash_dev_special_to_vms(unixptr, vmspath, vmspath_len);
	    if (sts = SS$_NORMAL)
		return SS$_NORMAL;
	}
      }
      sts = posix_root_to_vms(esa, vmspath_len, vmspath, utf8_fl);
    }

    if ($VMS_STATUS_SUCCESS(sts)) {
      /* This is verified to be a real path */

      sts = posix_root_to_vms(esa, vmspath_len, "/", NULL);
      if ($VMS_STATUS_SUCCESS(sts)) {
	strcpy(vmspath, esa);
	vmslen = strlen(vmspath);
	vmsptr = vmspath + vmslen;
	unixptr++;
	if (unixptr < lastslash) {
	char * rptr;
	  vmsptr--;
	  *vmsptr++ = '.';
	  dir_start = 1;
	  dir_dot = 1;
	  if (vmslen > 7) {
	  int cmp;
	    rptr = vmsptr - 7;
	    cmp = strcmp(rptr,"000000.");
	    if (cmp == 0) {
	      vmslen -= 7;
	      vmsptr -= 7;
	      vmsptr[1] = '\0';
	    } /* removing 6 zeros */
	  } /* vmslen < 7, no 6 zeros possible */
	} /* Not in a directory */
      } /* Posix root found */
      else {
	/* No posix root, fall back to default directory */
	strcpy(vmspath, "SYS$DISK:[");
	vmsptr = &vmspath[10];
	vmslen = 10;
	if (unixptr > lastslash) {
	   *vmsptr = ']';
	   vmsptr++;
	   vmslen++;
	}
	else {
	   dir_start = 1;
	}
      }
    } /* end of verified real path handling */
    else {
    int add_6zero;
    int islnm;

      /* Ok, we have a device or a concealed root that is not in POSIX
       * or we have garbage.  Make the best of it.
       */

      /* Posix to VMS destroyed this, so copy it again */
      strncpy(vmspath, &unixptr[1], seg_len);
      vmspath[seg_len] = 0;
      vmslen = seg_len;
      vmsptr = &vmsptr[vmslen];
      islnm = 0;

      /* Now do we need to add the fake 6 zero directory to it? */
      add_6zero = 1;
      if ((*lastslash == '/') && (nextslash < lastslash)) {
	/* No there is another directory */
	add_6zero = 0;
      }
      else {
      int trnend;
      int cmp;

	/* now we have foo:bar or foo:[000000]bar to decide from */
	islnm = vmstrnenv(vmspath, esa, 0, fildev, 0);

        if (!islnm && !decc_posix_compliant_pathnames) {

	    cmp = strncmp("bin", vmspath, 4);
	    if (cmp == 0) {
	        /* bin => SYS$SYSTEM: */
		islnm = vmstrnenv("SYS$SYSTEM:", esa, 0, fildev, 0);
	    }
	    else {
	        /* tmp => SYS$SCRATCH: */
	        cmp = strncmp("tmp", vmspath, 4);
		if (cmp == 0) {
		    islnm = vmstrnenv("SYS$SCRATCH:", esa, 0, fildev, 0);
		}
	    }
	}

        trnend = islnm ? islnm - 1 : 0;

	/* if this was a logical name, ']' or '>' must be present */
	/* if not a logical name, then assume a device and hope. */
	islnm =  trnend ? (esa[trnend] == ']' || esa[trnend] == '>') : 0;

	/* if log name and trailing '.' then rooted - treat as device */
	add_6zero = islnm ? (esa[trnend-1] == '.') : 0;

	/* Fix me, if not a logical name, a device lookup should be
         * done to see if the device is file structured.  If the device
         * is not file structured, the 6 zeros should not be put on.
         *
         * As it is, perl is occasionally looking for dev:[000000]tty.
	 * which looks a little strange.
	 *
	 * Not that easy to detect as "/dev" may be file structured with
	 * special device files.
         */

	if ((add_6zero == 0) && (*nextslash == '/') &&
	    (&nextslash[1] == unixend)) {
	  /* No real directory present */
	  add_6zero = 1;
	}
      }

      /* Put the device delimiter on */
      *vmsptr++ = ':';
      vmslen++;
      unixptr = nextslash;
      unixptr++;

      /* Start directory if needed */
      if (!islnm || add_6zero) {
	*vmsptr++ = '[';
	vmslen++;
	dir_start = 1;
      }

      /* add fake 000000] if needed */
      if (add_6zero) {
	*vmsptr++ = '0';
	*vmsptr++ = '0';
	*vmsptr++ = '0';
	*vmsptr++ = '0';
	*vmsptr++ = '0';
	*vmsptr++ = '0';
	*vmsptr++ = ']';
	vmslen += 7;
	dir_start = 0;
      }

    } /* non-POSIX translation */
    PerlMem_free(esa);
  } /* End of relative/absolute path handling */

  while ((unixptr <= unixend) && (vmslen < vmspath_len)){
  int dash_flag;
  int in_cnt;
  int out_cnt;

    dash_flag = 0;

    if (dir_start != 0) {

      /* First characters in a directory are handled special */
      while ((*unixptr == '/') ||
	     ((*unixptr == '.') &&
	      ((unixptr[1]=='.') || (unixptr[1]=='/') ||
		(&unixptr[1]==unixend)))) {
      int loop_flag;

	loop_flag = 0;

        /* Skip redundant / in specification */
        while ((*unixptr == '/') && (dir_start != 0)) {
	  loop_flag = 1;
	  unixptr++;
	  if (unixptr == lastslash)
	    break;
	}
	if (unixptr == lastslash)
	  break;

        /* Skip redundant ./ characters */
	while ((*unixptr == '.') &&
	       ((unixptr[1] == '/')||(&unixptr[1] == unixend))) {
	  loop_flag = 1;
	  unixptr++;
	  if (unixptr == lastslash)
	    break;
	  if (*unixptr == '/')
	    unixptr++;
	}
	if (unixptr == lastslash)
	  break;

	/* Skip redundant ../ characters */
	while ((*unixptr == '.') && (unixptr[1] == '.') &&
	     ((unixptr[2] == '/') || (&unixptr[2] == unixend))) {
	  /* Set the backing up flag */
	  loop_flag = 1;
	  dir_dot = 0;
	  dash_flag = 1;
	  *vmsptr++ = '-';
	  vmslen++;
	  unixptr++; /* first . */
	  unixptr++; /* second . */
	  if (unixptr == lastslash)
	    break;
	  if (*unixptr == '/') /* The slash */
	    unixptr++;
	}
	if (unixptr == lastslash)
	  break;

	/* To do: Perl expects /.../ to be translated to [...] on VMS */
  	/* Not needed when VMS is pretending to be UNIX. */

	/* Is this loop stuck because of too many dots? */
	if (loop_flag == 0) {
	  /* Exit the loop and pass the rest through */
	  break;
	}
      }

      /* Are we done with directories yet? */
      if (unixptr >= lastslash) {

	/* Watch out for trailing dots */
	if (dir_dot != 0) {
	    vmslen --;
	    vmsptr--;
	}
	*vmsptr++ = ']';
	vmslen++;
	dash_flag = 0;
	dir_start = 0;
	if (*unixptr == '/')
	  unixptr++;
      }
      else {
	/* Have we stopped backing up? */
	if (dash_flag) {
	  *vmsptr++ = '.';
	  vmslen++;
	  dash_flag = 0;
	  /* dir_start continues to be = 1 */
	}
	if (*unixptr == '-') {
	  *vmsptr++ = '^';
	  *vmsptr++ = *unixptr++;
	  vmslen += 2;
	  dir_start = 0;

	  /* Now are we done with directories yet? */
	  if (unixptr >= lastslash) {

	    /* Watch out for trailing dots */
	    if (dir_dot != 0) {
	      vmslen --;
	      vmsptr--;
	    }

	    *vmsptr++ = ']';
	    vmslen++;
	    dash_flag = 0;
	    dir_start = 0;
	  }
	}
      }
    }

    /* All done? */
    if (unixptr >= unixend)
      break;

    /* Normal characters - More EFS work probably needed */
    dir_start = 0;
    dir_dot = 0;

    switch(*unixptr) {
    case '/':
	/* remove multiple / */
	while (unixptr[1] == '/') {
	   unixptr++;
	}
	if (unixptr == lastslash) {
	  /* Watch out for trailing dots */
	  if (dir_dot != 0) {
	    vmslen --;
	    vmsptr--;
	  }
	  *vmsptr++ = ']';
	}
	else {
	  dir_start = 1;
	  *vmsptr++ = '.';
	  dir_dot = 1;

	  /* To do: Perl expects /.../ to be translated to [...] on VMS */
 	  /* Not needed when VMS is pretending to be UNIX. */

	}
	dash_flag = 0;
	if (unixptr != unixend)
	  unixptr++;
	vmslen++;
	break;
    case '.':
	if ((unixptr < lastdot) || (unixptr < lastslash) ||
	    (&unixptr[1] == unixend)) {
	  *vmsptr++ = '^';
	  *vmsptr++ = '.';
	  vmslen += 2;
	  unixptr++;

	  /* trailing dot ==> '^..' on VMS */
	  if (unixptr == unixend) {
	    *vmsptr++ = '.';
	    vmslen++;
	    unixptr++;
	  }
	  break;
	}

	*vmsptr++ = *unixptr++;
	vmslen ++;
	break;
    case '"':
	if (quoted && (&unixptr[1] == unixend)) {
	    unixptr++;
	    break;
	}
	in_cnt = copy_expand_unix_filename_escape
		(vmsptr, unixptr, &out_cnt, utf8_fl);
	vmsptr += out_cnt;
	unixptr += in_cnt;
	break;
    case '~':
    case ';':
    case '\\':
    case '?':
    case ' ':
    default:
	in_cnt = copy_expand_unix_filename_escape
		(vmsptr, unixptr, &out_cnt, utf8_fl);
	vmsptr += out_cnt;
	unixptr += in_cnt;
	break;
    }
  }

  /* Make sure directory is closed */
  if (unixptr == lastslash) {
    char *vmsptr2;
    vmsptr2 = vmsptr - 1;

    if (*vmsptr2 != ']') {
      *vmsptr2--;

      /* directories do not end in a dot bracket */
      if (*vmsptr2 == '.') {
	vmsptr2--;

	/* ^. is allowed */
        if (*vmsptr2 != '^') {
	  vmsptr--; /* back up over the dot */
 	}
      }
      *vmsptr++ = ']';
    }
  }
  else {
    char *vmsptr2;
    /* Add a trailing dot if a file with no extension */
    vmsptr2 = vmsptr - 1;
    if ((vmslen > 1) &&
	(*vmsptr2 != ']') && (*vmsptr2 != '*') && (*vmsptr2 != '%') &&
 	(*vmsptr2 != ')') && (*lastdot != '.')) {
	*vmsptr++ = '.';
        vmslen++;
    }
  }

  *vmsptr = '\0';
  return SS$_NORMAL;
}
#endif

 /* Eventual routine to convert a UTF-8 specification to VTF-7. */
static char * utf8_to_vtf7(char * rslt, const char * path, int *utf8_fl)
{
char * result;
int utf8_flag;

   /* If a UTF8 flag is being passed, honor it */
   utf8_flag = 0;
   if (utf8_fl != NULL) {
     utf8_flag = *utf8_fl;
    *utf8_fl = 0;
   }

   if (utf8_flag) {
     /* If there is a possibility of UTF8, then if any UTF8 characters
        are present, then they must be converted to VTF-7
      */
     result = strcpy(rslt, path); /* FIX-ME */
   }
   else
     result = strcpy(rslt, path);

   return result;
}


/*{{{ char *tovmsspec[_ts](char *path, char *buf, int * utf8_flag)*/
static char *mp_do_tovmsspec
   (pTHX_ const char *path, char *buf, int ts, int dir_flag, int * utf8_flag) {
  static char __tovmsspec_retbuf[VMS_MAXRSS];
  char *rslt, *dirend;
  char *lastdot;
  char *vms_delim;
  register char *cp1;
  const char *cp2;
  unsigned long int infront = 0, hasdir = 1;
  int rslt_len;
  int no_type_seen;
  char * v_spec, * r_spec, * d_spec, * n_spec, * e_spec, * vs_spec;
  int sts, v_len, r_len, d_len, n_len, e_len, vs_len;

  if (path == NULL) return NULL;
  rslt_len = VMS_MAXRSS-1;
  if (buf) rslt = buf;
  else if (ts) Newx(rslt, VMS_MAXRSS, char);
  else rslt = __tovmsspec_retbuf;

  /* '.' and '..' are "[]" and "[-]" for a quick check */
  if (path[0] == '.') {
    if (path[1] == '\0') {
      strcpy(rslt,"[]");
      if (utf8_flag != NULL)
	*utf8_flag = 0;
      return rslt;
    }
    else {
      if (path[1] == '.' && path[2] == '\0') {
	strcpy(rslt,"[-]");
	if (utf8_flag != NULL)
	   *utf8_flag = 0;
	return rslt;
      }
    }
  }

   /* Posix specifications are now a native VMS format */
  /*--------------------------------------------------*/
#if __CRTL_VER >= 80200000 && !defined(__VAX)
  if (decc_posix_compliant_pathnames) {
    if (strncmp(path,"\"^UP^",5) == 0) {
      posix_to_vmsspec_hardway(rslt, rslt_len, path, dir_flag, utf8_flag);
      return rslt;
    }
  }
#endif

  /* This is really the only way to see if this is already in VMS format */
  sts = vms_split_path
       (path,
	&v_spec,
	&v_len,
	&r_spec,
	&r_len,
	&d_spec,
	&d_len,
	&n_spec,
	&n_len,
	&e_spec,
	&e_len,
	&vs_spec,
	&vs_len);
  if (sts == 0) {
    /* FIX-ME - If dir_flag is non-zero, then this is a mp_do_vmspath()
       replacement, because the above parse just took care of most of
       what is needed to do vmspath when the specification is already
       in VMS format.

       And if it is not already, it is easier to do the conversion as
       part of this routine than to call this routine and then work on
       the result.
     */

    /* If VMS punctuation was found, it is already VMS format */
    if ((v_len != 0) || (r_len != 0) || (d_len != 0) || (vs_len != 0)) {
      if (utf8_flag != NULL)
	*utf8_flag = 0;
      strcpy(rslt, path);
      return rslt;
    }
    /* Now, what to do with trailing "." cases where there is no
       extension?  If this is a UNIX specification, and EFS characters
       are enabled, then the trailing "." should be converted to a "^.".
       But if this was already a VMS specification, then it should be
       left alone.

       So in the case of ambiguity, leave the specification alone.
     */


    /* If there is a possibility of UTF8, then if any UTF8 characters
        are present, then they must be converted to VTF-7
     */
    if (utf8_flag != NULL)
      *utf8_flag = 0;
    strcpy(rslt, path);
    return rslt;
  }

  dirend = strrchr(path,'/');

  if (dirend == NULL) {
     /* If we get here with no UNIX directory delimiters, then this is
        not a complete file specification, either garbage a UNIX glob
	specification that can not be converted to a VMS wildcard, or
	it a UNIX shell macro.  MakeMaker wants these passed through AS-IS,
	so apparently other programs expect this also.

	utf8 flag setting needs to be preserved.
      */
      strcpy(rslt, path);
      return rslt;
  }

/* If POSIX mode active, handle the conversion */
#if __CRTL_VER >= 80200000 && !defined(__VAX)
  if (decc_efs_charset) {
    posix_to_vmsspec_hardway(rslt, rslt_len, path, dir_flag, utf8_flag);
    return rslt;
  }
#endif

  if (*(dirend+1) == '.') {  /* do we have trailing "/." or "/.." or "/..."? */
    if (!*(dirend+2)) dirend +=2;
    if (*(dirend+2) == '.' && !*(dirend+3)) dirend += 3;
    if (decc_efs_charset == 0) {
      if (*(dirend+2) == '.' && *(dirend+3) == '.' && !*(dirend+4)) dirend += 4;
    }
  }

  cp1 = rslt;
  cp2 = path;
  lastdot = strrchr(cp2,'.');
  if (*cp2 == '/') {
    char *trndev;
    int islnm, rooted;
    STRLEN trnend;

    while (*(cp2+1) == '/') cp2++;  /* Skip multiple /s */
    if (!*(cp2+1)) {
      if (decc_disable_posix_root) {
	strcpy(rslt,"sys$disk:[000000]");
      }
      else {
	strcpy(rslt,"sys$posix_root:[000000]");
      }
      if (utf8_flag != NULL)
	*utf8_flag = 0;
      return rslt;
    }
    while (*(++cp2) != '/' && *cp2) *(cp1++) = *cp2;
    *cp1 = '\0';
    trndev = PerlMem_malloc(VMS_MAXRSS);
    if (trndev == NULL) _ckvmssts(SS$_INSFMEM);
    islnm =  my_trnlnm(rslt,trndev,0);

     /* DECC special handling */
    if (!islnm) {
      if (strcmp(rslt,"bin") == 0) {
	strcpy(rslt,"sys$system");
	cp1 = rslt + 10;
	*cp1 = 0;
	islnm =  my_trnlnm(rslt,trndev,0);
      }
      else if (strcmp(rslt,"tmp") == 0) {
	strcpy(rslt,"sys$scratch");
	cp1 = rslt + 11;
	*cp1 = 0;
	islnm =  my_trnlnm(rslt,trndev,0);
      }
      else if (!decc_disable_posix_root) {
        strcpy(rslt, "sys$posix_root");
	cp1 = rslt + 13;
	*cp1 = 0;
	cp2 = path;
        while (*(cp2+1) == '/') cp2++;  /* Skip multiple /s */
	islnm =  my_trnlnm(rslt,trndev,0);
      }
      else if (strcmp(rslt,"dev") == 0) {
	if (strncmp(cp2,"/null", 5) == 0) {
	  if ((cp2[5] == 0) || (cp2[5] == '/')) {
	    strcpy(rslt,"NLA0");
	    cp1 = rslt + 4;
	    *cp1 = 0;
	    cp2 = cp2 + 5;
	    islnm =  my_trnlnm(rslt,trndev,0);
	  }
	}
      }
    }

    trnend = islnm ? strlen(trndev) - 1 : 0;
    islnm =  trnend ? (trndev[trnend] == ']' || trndev[trnend] == '>') : 0;
    rooted = islnm ? (trndev[trnend-1] == '.') : 0;
    /* If the first element of the path is a logical name, determine
     * whether it has to be translated so we can add more directories. */
    if (!islnm || rooted) {
      *(cp1++) = ':';
      *(cp1++) = '[';
      if (cp2 == dirend) while (infront++ < 6) *(cp1++) = '0';
      else cp2++;
    }
    else {
      if (cp2 != dirend) {
        strcpy(rslt,trndev);
        cp1 = rslt + trnend;
	if (*cp2 != 0) {
          *(cp1++) = '.';
          cp2++;
        }
      }
      else {
	if (decc_disable_posix_root) {
	  *(cp1++) = ':';
	  hasdir = 0;
	}
      }
    }
    PerlMem_free(trndev);
  }
  else {
    *(cp1++) = '[';
    if (*cp2 == '.') {
      if (*(cp2+1) == '/' || *(cp2+1) == '\0') {
        cp2 += 2;         /* skip over "./" - it's redundant */
        *(cp1++) = '.';   /* but it does indicate a relative dirspec */
      }
      else if (*(cp2+1) == '.' && (*(cp2+2) == '/' || *(cp2+2) == '\0')) {
        *(cp1++) = '-';                                 /* "../" --> "-" */
        cp2 += 3;
      }
      else if (*(cp2+1) == '.' && *(cp2+2) == '.' &&
               (*(cp2+3) == '/' || *(cp2+3) == '\0')) {
        *(cp1++) = '.'; *(cp1++) = '.'; *(cp1++) = '.'; /* ".../" --> "..." */
        if (!*(cp2+4)) *(cp1++) = '.'; /* Simulate trailing '/' for later */
        cp2 += 4;
      }
      else if ((cp2 != lastdot) || (lastdot < dirend)) {
	/* Escape the extra dots in EFS file specifications */
	*(cp1++) = '^';
      }
      if (cp2 > dirend) cp2 = dirend;
    }
    else *(cp1++) = '.';
  }
  for (; cp2 < dirend; cp2++) {
    if (*cp2 == '/') {
      if (*(cp2-1) == '/') continue;
      if (*(cp1-1) != '.') *(cp1++) = '.';
      infront = 0;
    }
    else if (!infront && *cp2 == '.') {
      if (cp2+1 == dirend || *(cp2+1) == '\0') { cp2++; break; }
      else if (*(cp2+1) == '/') cp2++;   /* skip over "./" - it's redundant */
      else if (*(cp2+1) == '.' && (*(cp2+2) == '/' || *(cp2+2) == '\0')) {
        if (*(cp1-1) == '-' || *(cp1-1) == '[') *(cp1++) = '-'; /* handle "../" */
        else if (*(cp1-2) == '[') *(cp1-1) = '-';
        else {  /* back up over previous directory name */
          cp1--;
          while (*(cp1-1) != '.' && *(cp1-1) != '[') cp1--;
          if (*(cp1-1) == '[') {
            memcpy(cp1,"000000.",7);
            cp1 += 7;
          }
        }
        cp2 += 2;
        if (cp2 == dirend) break;
      }
      else if ( *(cp2+1) == '.' && *(cp2+2) == '.' &&
                (*(cp2+3) == '/' || *(cp2+3) == '\0') ) {
        if (*(cp1-1) != '.') *(cp1++) = '.'; /* May already have 1 from '/' */
        *(cp1++) = '.'; *(cp1++) = '.'; /* ".../" --> "..." */
        if (!*(cp2+3)) { 
          *(cp1++) = '.';  /* Simulate trailing '/' */
          cp2 += 2;  /* for loop will incr this to == dirend */
        }
        else cp2 += 3;  /* Trailing '/' was there, so skip it, too */
      }
      else {
        if (decc_efs_charset == 0)
	  *(cp1++) = '_';  /* fix up syntax - '.' in name not allowed */
	else {
	  *(cp1++) = '^';  /* fix up syntax - '.' in name is allowed */
	  *(cp1++) = '.';
	}
      }
    }
    else {
      if (!infront && *(cp1-1) == '-')  *(cp1++) = '.';
      if (*cp2 == '.') {
        if (decc_efs_charset == 0)
	  *(cp1++) = '_';
	else {
	  *(cp1++) = '^';
	  *(cp1++) = '.';
	}
      }
      else                  *(cp1++) =  *cp2;
      infront = 1;
    }
  }
  if (*(cp1-1) == '.') cp1--; /* Unix spec ending in '/' ==> trailing '.' */
  if (hasdir) *(cp1++) = ']';
  if (*cp2) cp2++;  /* check in case we ended with trailing '..' */
  /* fixme for ODS5 */
  no_type_seen = 0;
  if (cp2 > lastdot)
    no_type_seen = 1;
  while (*cp2) {
    switch(*cp2) {
    case '?':
        if (decc_efs_charset == 0)
	  *(cp1++) = '%';
	else
	  *(cp1++) = '?';
	cp2++;
    case ' ':
	*(cp1)++ = '^';
	*(cp1)++ = '_';
	cp2++;
	break;
    case '.':
	if (((cp2 < lastdot) || (cp2[1] == '\0')) &&
	    decc_readdir_dropdotnotype) {
	  *(cp1)++ = '^';
	  *(cp1)++ = '.';
	  cp2++;

	  /* trailing dot ==> '^..' on VMS */
	  if (*cp2 == '\0') {
	    *(cp1++) = '.';
	    no_type_seen = 0;
	  }
	}
	else {
	  *(cp1++) = *(cp2++);
	  no_type_seen = 0;
	}
	break;
    case '$':
	 /* This could be a macro to be passed through */
	*(cp1++) = *(cp2++);
	if (*cp2 == '(') {
	const char * save_cp2;
	char * save_cp1;
	int is_macro;

	    /* paranoid check */
	    save_cp2 = cp2;
	    save_cp1 = cp1;
	    is_macro = 0;

	    /* Test through */
	    *(cp1++) = *(cp2++);
	    if (isalnum(*cp2) || (*cp2 == '.') || (*cp2 == '_')) {
		*(cp1++) = *(cp2++);
		while (isalnum(*cp2) || (*cp2 == '.') || (*cp2 == '_')) {
		    *(cp1++) = *(cp2++);
		}
		if (*cp2 == ')') {
		    *(cp1++) = *(cp2++);
		    is_macro = 1;
		}
	    }
	    if (is_macro == 0) {
		/* Not really a macro - never mind */
		cp2 = save_cp2;
		cp1 = save_cp1;
	    }
	}
	break;
    case '\"':
    case '~':
    case '`':
    case '!':
    case '#':
    case '%':
    case '^':
        /* Don't escape again if following character is 
         * already something we escape.
         */
        if (strchr("\"~`!#%^&()=+\'@[]{}:\\|<>_.", *(cp2+1))) {
	    *(cp1++) = *(cp2++);
	    break;
        }
        /* But otherwise fall through and escape it. */
    case '&':
    case '(':
    case ')':
    case '=':
    case '+':
    case '\'':
    case '@':
    case '[':
    case ']':
    case '{':
    case '}':
    case ':':
    case '\\':
    case '|':
    case '<':
    case '>':
	*(cp1++) = '^';
	*(cp1++) = *(cp2++);
	break;
    case ';':
	/* FIXME: This needs fixing as Perl is putting ".dir;" on UNIX filespecs
	 * which is wrong.  UNIX notation should be ".dir." unless
	 * the DECC$FILENAME_UNIX_NO_VERSION is enabled.
	 * changing this behavior could break more things at this time.
	 * efs character set effectively does not allow "." to be a version
	 * delimiter as a further complication about changing this.
	 */
	if (decc_filename_unix_report != 0) {
	  *(cp1++) = '^';
	}
	*(cp1++) = *(cp2++);
	break;
    default:
	*(cp1++) = *(cp2++);
    }
  }
  if ((no_type_seen == 1) && decc_readdir_dropdotnotype) {
  char *lcp1;
    lcp1 = cp1;
    lcp1--;
     /* Fix me for "^]", but that requires making sure that you do
      * not back up past the start of the filename
      */
    if ((*lcp1 != ']') && (*lcp1 != '*') && (*lcp1 != '%'))
      *cp1++ = '.';
  }
  *cp1 = '\0';

  if (utf8_flag != NULL)
    *utf8_flag = 0;
  return rslt;

}  /* end of do_tovmsspec() */
/*}}}*/
/* External entry points */
char *Perl_tovmsspec(pTHX_ const char *path, char *buf)
  { return do_tovmsspec(path,buf,0,NULL); }
char *Perl_tovmsspec_ts(pTHX_ const char *path, char *buf)
  { return do_tovmsspec(path,buf,1,NULL); }
char *Perl_tovmsspec_utf8(pTHX_ const char *path, char *buf, int * utf8_fl)
  { return do_tovmsspec(path,buf,0,utf8_fl); }
char *Perl_tovmsspec_utf8_ts(pTHX_ const char *path, char *buf, int * utf8_fl)
  { return do_tovmsspec(path,buf,1,utf8_fl); }

/*{{{ char *tovmspath[_ts](char *path, char *buf, const int *)*/
static char *mp_do_tovmspath(pTHX_ const char *path, char *buf, int ts, int * utf8_fl) {
  static char __tovmspath_retbuf[VMS_MAXRSS];
  int vmslen;
  char *pathified, *vmsified, *cp;

  if (path == NULL) return NULL;
  pathified = PerlMem_malloc(VMS_MAXRSS);
  if (pathified == NULL) _ckvmssts(SS$_INSFMEM);
  if (do_pathify_dirspec(path,pathified,0,NULL) == NULL) {
    PerlMem_free(pathified);
    return NULL;
  }

  vmsified = NULL;
  if (buf == NULL)
     Newx(vmsified, VMS_MAXRSS, char);
  if (do_tovmsspec(pathified, buf ? buf : vmsified, 0, NULL) == NULL) {
    PerlMem_free(pathified);
    if (vmsified) Safefree(vmsified);
    return NULL;
  }
  PerlMem_free(pathified);
  if (buf) {
    return buf;
  }
  else if (ts) {
    vmslen = strlen(vmsified);
    Newx(cp,vmslen+1,char);
    memcpy(cp,vmsified,vmslen);
    cp[vmslen] = '\0';
    Safefree(vmsified);
    return cp;
  }
  else {
    strcpy(__tovmspath_retbuf,vmsified);
    Safefree(vmsified);
    return __tovmspath_retbuf;
  }

}  /* end of do_tovmspath() */
/*}}}*/
/* External entry points */
char *Perl_tovmspath(pTHX_ const char *path, char *buf)
  { return do_tovmspath(path,buf,0, NULL); }
char *Perl_tovmspath_ts(pTHX_ const char *path, char *buf)
  { return do_tovmspath(path,buf,1, NULL); }
char *Perl_tovmspath_utf8(pTHX_ const char *path, char *buf, int *utf8_fl) 
  { return do_tovmspath(path,buf,0,utf8_fl); }
char *Perl_tovmspath_utf8_ts(pTHX_ const char *path, char *buf, int *utf8_fl)
  { return do_tovmspath(path,buf,1,utf8_fl); }


/*{{{ char *tounixpath[_ts](char *path, char *buf, int * utf8_fl)*/
static char *mp_do_tounixpath(pTHX_ const char *path, char *buf, int ts, int * utf8_fl) {
  static char __tounixpath_retbuf[VMS_MAXRSS];
  int unixlen;
  char *pathified, *unixified, *cp;

  if (path == NULL) return NULL;
  pathified = PerlMem_malloc(VMS_MAXRSS);
  if (pathified == NULL) _ckvmssts(SS$_INSFMEM);
  if (do_pathify_dirspec(path,pathified,0,NULL) == NULL) {
    PerlMem_free(pathified);
    return NULL;
  }

  unixified = NULL;
  if (buf == NULL) {
      Newx(unixified, VMS_MAXRSS, char);
  }
  if (do_tounixspec(pathified,buf ? buf : unixified,0,NULL) == NULL) {
    PerlMem_free(pathified);
    if (unixified) Safefree(unixified);
    return NULL;
  }
  PerlMem_free(pathified);
  if (buf) {
    return buf;
  }
  else if (ts) {
    unixlen = strlen(unixified);
    Newx(cp,unixlen+1,char);
    memcpy(cp,unixified,unixlen);
    cp[unixlen] = '\0';
    Safefree(unixified);
    return cp;
  }
  else {
    strcpy(__tounixpath_retbuf,unixified);
    Safefree(unixified);
    return __tounixpath_retbuf;
  }

}  /* end of do_tounixpath() */
/*}}}*/
/* External entry points */
char *Perl_tounixpath(pTHX_ const char *path, char *buf)
  { return do_tounixpath(path,buf,0,NULL); }
char *Perl_tounixpath_ts(pTHX_ const char *path, char *buf)
  { return do_tounixpath(path,buf,1,NULL); }
char *Perl_tounixpath_utf8(pTHX_ const char *path, char *buf, int * utf8_fl)
  { return do_tounixpath(path,buf,0,utf8_fl); }
char *Perl_tounixpath_utf8_ts(pTHX_ const char *path, char *buf, int * utf8_fl)
  { return do_tounixpath(path,buf,1,utf8_fl); }

/*
 * @(#)argproc.c 2.2 94/08/16	Mark Pizzolato (mark AT infocomm DOT com)
 *
 *****************************************************************************
 *                                                                           *
 *  Copyright (C) 1989-1994, 2007 by                                         *
 *  Mark Pizzolato - INFO COMM, Danville, California  (510) 837-5600         *
 *                                                                           *
 *  Permission is hereby granted for the reproduction of this software       *
 *  on condition that this copyright notice is included in source            *
 *  distributions of the software.  The code may be modified and             *
 *  distributed under the same terms as Perl itself.                         *
 *                                                                           *
 *  27-Aug-1994 Modified for inclusion in perl5                              *
 *              by Charles Bailey  (bailey AT newman DOT upenn DOT edu)      *
 *****************************************************************************
 */

/*
 * getredirection() is intended to aid in porting C programs
 * to VMS (Vax-11 C).  The native VMS environment does not support 
 * '>' and '<' I/O redirection, or command line wild card expansion, 
 * or a command line pipe mechanism using the '|' AND background 
 * command execution '&'.  All of these capabilities are provided to any
 * C program which calls this procedure as the first thing in the 
 * main program.
 * The piping mechanism will probably work with almost any 'filter' type
 * of program.  With suitable modification, it may useful for other
 * portability problems as well.
 *
 * Author:  Mark Pizzolato	(mark AT infocomm DOT com)
 */
struct list_item
    {
    struct list_item *next;
    char *value;
    };

static void add_item(struct list_item **head,
		     struct list_item **tail,
		     char *value,
		     int *count);

static void mp_expand_wild_cards(pTHX_ char *item,
				struct list_item **head,
				struct list_item **tail,
				int *count);

static int background_process(pTHX_ int argc, char **argv);

static void pipe_and_fork(pTHX_ char **cmargv);

/*{{{ void getredirection(int *ac, char ***av)*/
static void
mp_getredirection(pTHX_ int *ac, char ***av)
/*
 * Process vms redirection arg's.  Exit if any error is seen.
 * If getredirection() processes an argument, it is erased
 * from the vector.  getredirection() returns a new argc and argv value.
 * In the event that a background command is requested (by a trailing "&"),
 * this routine creates a background subprocess, and simply exits the program.
 *
 * Warning: do not try to simplify the code for vms.  The code
 * presupposes that getredirection() is called before any data is
 * read from stdin or written to stdout.
 *
 * Normal usage is as follows:
 *
 *	main(argc, argv)
 *	int		argc;
 *    	char		*argv[];
 *	{
 *		getredirection(&argc, &argv);
 *	}
 */
{
    int			argc = *ac;	/* Argument Count	  */
    char		**argv = *av;	/* Argument Vector	  */
    char		*ap;   		/* Argument pointer	  */
    int	       		j;		/* argv[] index		  */
    int			item_count = 0;	/* Count of Items in List */
    struct list_item 	*list_head = 0;	/* First Item in List	    */
    struct list_item	*list_tail;	/* Last Item in List	    */
    char 		*in = NULL;	/* Input File Name	    */
    char 		*out = NULL;	/* Output File Name	    */
    char 		*outmode = "w";	/* Mode to Open Output File */
    char 		*err = NULL;	/* Error File Name	    */
    char 		*errmode = "w";	/* Mode to Open Error File  */
    int			cmargc = 0;    	/* Piped Command Arg Count  */
    char		**cmargv = NULL;/* Piped Command Arg Vector */

    /*
     * First handle the case where the last thing on the line ends with
     * a '&'.  This indicates the desire for the command to be run in a
     * subprocess, so we satisfy that desire.
     */
    ap = argv[argc-1];
    if (0 == strcmp("&", ap))
       exit(background_process(aTHX_ --argc, argv));
    if (*ap && '&' == ap[strlen(ap)-1])
	{
	ap[strlen(ap)-1] = '\0';
       exit(background_process(aTHX_ argc, argv));
	}
    /*
     * Now we handle the general redirection cases that involve '>', '>>',
     * '<', and pipes '|'.
     */
    for (j = 0; j < argc; ++j)
	{
	if (0 == strcmp("<", argv[j]))
	    {
	    if (j+1 >= argc)
		{
		fprintf(stderr,"No input file after < on command line");
		exit(LIB$_WRONUMARG);
		}
	    in = argv[++j];
	    continue;
	    }
	if ('<' == *(ap = argv[j]))
	    {
	    in = 1 + ap;
	    continue;
	    }
	if (0 == strcmp(">", ap))
	    {
	    if (j+1 >= argc)
		{
		fprintf(stderr,"No output file after > on command line");
		exit(LIB$_WRONUMARG);
		}
	    out = argv[++j];
	    continue;
	    }
	if ('>' == *ap)
	    {
	    if ('>' == ap[1])
		{
		outmode = "a";
		if ('\0' == ap[2])
		    out = argv[++j];
		else
		    out = 2 + ap;
		}
	    else
		out = 1 + ap;
	    if (j >= argc)
		{
		fprintf(stderr,"No output file after > or >> on command line");
		exit(LIB$_WRONUMARG);
		}
	    continue;
	    }
	if (('2' == *ap) && ('>' == ap[1]))
	    {
	    if ('>' == ap[2])
		{
		errmode = "a";
		if ('\0' == ap[3])
		    err = argv[++j];
		else
		    err = 3 + ap;
		}
	    else
		if ('\0' == ap[2])
		    err = argv[++j];
		else
		    err = 2 + ap;
	    if (j >= argc)
		{
		fprintf(stderr,"No output file after 2> or 2>> on command line");
		exit(LIB$_WRONUMARG);
		}
	    continue;
	    }
	if (0 == strcmp("|", argv[j]))
	    {
	    if (j+1 >= argc)
		{
		fprintf(stderr,"No command into which to pipe on command line");
		exit(LIB$_WRONUMARG);
		}
	    cmargc = argc-(j+1);
	    cmargv = &argv[j+1];
	    argc = j;
	    continue;
	    }
	if ('|' == *(ap = argv[j]))
	    {
	    ++argv[j];
	    cmargc = argc-j;
	    cmargv = &argv[j];
	    argc = j;
	    continue;
	    }
	expand_wild_cards(ap, &list_head, &list_tail, &item_count);
	}
    /*
     * Allocate and fill in the new argument vector, Some Unix's terminate
     * the list with an extra null pointer.
     */
    argv = (char **) PerlMem_malloc((item_count+1) * sizeof(char *));
    if (argv == NULL) _ckvmssts_noperl(SS$_INSFMEM);
    *av = argv;
    for (j = 0; j < item_count; ++j, list_head = list_head->next)
	argv[j] = list_head->value;
    *ac = item_count;
    if (cmargv != NULL)
	{
	if (out != NULL)
	    {
	    fprintf(stderr,"'|' and '>' may not both be specified on command line");
	    exit(LIB$_INVARGORD);
	    }
	pipe_and_fork(aTHX_ cmargv);
	}
	
    /* Check for input from a pipe (mailbox) */

    if (in == NULL && 1 == isapipe(0))
	{
	char mbxname[L_tmpnam];
	long int bufsize;
	long int dvi_item = DVI$_DEVBUFSIZ;
	$DESCRIPTOR(mbxnam, "");
	$DESCRIPTOR(mbxdevnam, "");

	/* Input from a pipe, reopen it in binary mode to disable	*/
	/* carriage control processing.	 				*/

	fgetname(stdin, mbxname);
	mbxnam.dsc$a_pointer = mbxname;
	mbxnam.dsc$w_length = strlen(mbxnam.dsc$a_pointer);	
	lib$getdvi(&dvi_item, 0, &mbxnam, &bufsize, 0, 0);
	mbxdevnam.dsc$a_pointer = mbxname;
	mbxdevnam.dsc$w_length = sizeof(mbxname);
	dvi_item = DVI$_DEVNAM;
	lib$getdvi(&dvi_item, 0, &mbxnam, 0, &mbxdevnam, &mbxdevnam.dsc$w_length);
	mbxdevnam.dsc$a_pointer[mbxdevnam.dsc$w_length] = '\0';
	set_errno(0);
	set_vaxc_errno(1);
	freopen(mbxname, "rb", stdin);
	if (errno != 0)
	    {
	    fprintf(stderr,"Can't reopen input pipe (name: %s) in binary mode",mbxname);
	    exit(vaxc$errno);
	    }
	}
    if ((in != NULL) && (NULL == freopen(in, "r", stdin, "mbc=32", "mbf=2")))
	{
	fprintf(stderr,"Can't open input file %s as stdin",in);
	exit(vaxc$errno);
	}
    if ((out != NULL) && (NULL == freopen(out, outmode, stdout, "mbc=32", "mbf=2")))
	{	
	fprintf(stderr,"Can't open output file %s as stdout",out);
	exit(vaxc$errno);
	}
	if (out != NULL) Perl_vmssetuserlnm(aTHX_ "SYS$OUTPUT",out);

    if (err != NULL) {
        if (strcmp(err,"&1") == 0) {
            dup2(fileno(stdout), fileno(stderr));
            Perl_vmssetuserlnm(aTHX_ "SYS$ERROR","SYS$OUTPUT");
        } else {
	FILE *tmperr;
	if (NULL == (tmperr = fopen(err, errmode, "mbc=32", "mbf=2")))
	    {
	    fprintf(stderr,"Can't open error file %s as stderr",err);
	    exit(vaxc$errno);
	    }
	    fclose(tmperr);
           if (NULL == freopen(err, "a", stderr, "mbc=32", "mbf=2"))
		{
		exit(vaxc$errno);
		}
	    Perl_vmssetuserlnm(aTHX_ "SYS$ERROR",err);
	}
        }
#ifdef ARGPROC_DEBUG
    PerlIO_printf(Perl_debug_log, "Arglist:\n");
    for (j = 0; j < *ac;  ++j)
	PerlIO_printf(Perl_debug_log, "argv[%d] = '%s'\n", j, argv[j]);
#endif
   /* Clear errors we may have hit expanding wildcards, so they don't
      show up in Perl's $! later */
   set_errno(0); set_vaxc_errno(1);
}  /* end of getredirection() */
/*}}}*/

static void add_item(struct list_item **head,
		     struct list_item **tail,
		     char *value,
		     int *count)
{
    if (*head == 0)
	{
	*head = (struct list_item *) PerlMem_malloc(sizeof(struct list_item));
	if (head == NULL) _ckvmssts_noperl(SS$_INSFMEM);
	*tail = *head;
	}
    else {
	(*tail)->next = (struct list_item *) PerlMem_malloc(sizeof(struct list_item));
	if ((*tail)->next == NULL) _ckvmssts_noperl(SS$_INSFMEM);
	*tail = (*tail)->next;
	}
    (*tail)->value = value;
    ++(*count);
}

static void mp_expand_wild_cards(pTHX_ char *item,
			      struct list_item **head,
			      struct list_item **tail,
			      int *count)
{
int expcount = 0;
unsigned long int context = 0;
int isunix = 0;
int item_len = 0;
char *had_version;
char *had_device;
int had_directory;
char *devdir,*cp;
char *vmsspec;
$DESCRIPTOR(filespec, "");
$DESCRIPTOR(defaultspec, "SYS$DISK:[]");
$DESCRIPTOR(resultspec, "");
unsigned long int lff_flags = 0;
int sts;
int rms_sts;

#ifdef VMS_LONGNAME_SUPPORT
    lff_flags = LIB$M_FIL_LONG_NAMES;
#endif

    for (cp = item; *cp; cp++) {
	if (*cp == '*' || *cp == '%' || isspace(*cp)) break;
	if (*cp == '.' && *(cp-1) == '.' && *(cp-2) =='.') break;
    }
    if (!*cp || isspace(*cp))
	{
	add_item(head, tail, item, count);
	return;
	}
    else
        {
     /* "double quoted" wild card expressions pass as is */
     /* From DCL that means using e.g.:                  */
     /* perl program """perl.*"""                        */
     item_len = strlen(item);
     if ( '"' == *item && '"' == item[item_len-1] )
       {
       item++;
       item[item_len-2] = '\0';
       add_item(head, tail, item, count);
       return;
       }
     }
    resultspec.dsc$b_dtype = DSC$K_DTYPE_T;
    resultspec.dsc$b_class = DSC$K_CLASS_D;
    resultspec.dsc$a_pointer = NULL;
    vmsspec = PerlMem_malloc(VMS_MAXRSS);
    if (vmsspec == NULL) _ckvmssts_noperl(SS$_INSFMEM);
    if ((isunix = (int) strchr(item,'/')) != (int) NULL)
      filespec.dsc$a_pointer = do_tovmsspec(item,vmsspec,0,NULL);
    if (!isunix || !filespec.dsc$a_pointer)
      filespec.dsc$a_pointer = item;
    filespec.dsc$w_length = strlen(filespec.dsc$a_pointer);
    /*
     * Only return version specs, if the caller specified a version
     */
    had_version = strchr(item, ';');
    /*
     * Only return device and directory specs, if the caller specifed either.
     */
    had_device = strchr(item, ':');
    had_directory = (isunix || NULL != strchr(item, '[')) || (NULL != strchr(item, '<'));
    
    while ($VMS_STATUS_SUCCESS(sts = lib$find_file
				 (&filespec, &resultspec, &context,
    				  &defaultspec, 0, &rms_sts, &lff_flags)))
	{
	char *string;
	char *c;

	string = PerlMem_malloc(resultspec.dsc$w_length+1);
        if (string == NULL) _ckvmssts_noperl(SS$_INSFMEM);
	strncpy(string, resultspec.dsc$a_pointer, resultspec.dsc$w_length);
	string[resultspec.dsc$w_length] = '\0';
	if (NULL == had_version)
	    *(strrchr(string, ';')) = '\0';
	if ((!had_directory) && (had_device == NULL))
	    {
	    if (NULL == (devdir = strrchr(string, ']')))
		devdir = strrchr(string, '>');
	    strcpy(string, devdir + 1);
	    }
	/*
	 * Be consistent with what the C RTL has already done to the rest of
	 * the argv items and lowercase all of these names.
	 */
	if (!decc_efs_case_preserve) {
	    for (c = string; *c; ++c)
	    if (isupper(*c))
		*c = tolower(*c);
	}
	if (isunix) trim_unixpath(string,item,1);
	add_item(head, tail, string, count);
	++expcount;
    }
    PerlMem_free(vmsspec);
    if (sts != RMS$_NMF)
	{
	set_vaxc_errno(sts);
	switch (sts)
	    {
	    case RMS$_FNF: case RMS$_DNF:
		set_errno(ENOENT); break;
	    case RMS$_DIR:
		set_errno(ENOTDIR); break;
	    case RMS$_DEV:
		set_errno(ENODEV); break;
	    case RMS$_FNM: case RMS$_SYN:
		set_errno(EINVAL); break;
	    case RMS$_PRV:
		set_errno(EACCES); break;
	    default:
		_ckvmssts_noperl(sts);
	    }
	}
    if (expcount == 0)
	add_item(head, tail, item, count);
    _ckvmssts_noperl(lib$sfree1_dd(&resultspec));
    _ckvmssts_noperl(lib$find_file_end(&context));
}

static int child_st[2];/* Event Flag set when child process completes	*/

static unsigned short child_chan;/* I/O Channel for Pipe Mailbox		*/

static unsigned long int exit_handler(int *status)
{
short iosb[4];

    if (0 == child_st[0])
	{
#ifdef ARGPROC_DEBUG
	PerlIO_printf(Perl_debug_log, "Waiting for Child Process to Finish . . .\n");
#endif
	fflush(stdout);	    /* Have to flush pipe for binary data to	*/
			    /* terminate properly -- <tp@mccall.com>	*/
	sys$qiow(0, child_chan, IO$_WRITEOF, iosb, 0, 0, 0, 0, 0, 0, 0, 0);
	sys$dassgn(child_chan);
	fclose(stdout);
	sys$synch(0, child_st);
	}
    return(1);
}

static void sig_child(int chan)
{
#ifdef ARGPROC_DEBUG
    PerlIO_printf(Perl_debug_log, "Child Completion AST\n");
#endif
    if (child_st[0] == 0)
	child_st[0] = 1;
}

static struct exit_control_block exit_block =
    {
    0,
    exit_handler,
    1,
    &exit_block.exit_status,
    0
    };

static void 
pipe_and_fork(pTHX_ char **cmargv)
{
    PerlIO *fp;
    struct dsc$descriptor_s *vmscmd;
    char subcmd[2*MAX_DCL_LINE_LENGTH], *p, *q;
    int sts, j, l, ismcr, quote, tquote = 0;

    sts = setup_cmddsc(aTHX_ cmargv[0],0,&quote,&vmscmd);
    vms_execfree(vmscmd);

    j = l = 0;
    p = subcmd;
    q = cmargv[0];
    ismcr = q && toupper(*q) == 'M'     && toupper(*(q+1)) == 'C' 
              && toupper(*(q+2)) == 'R' && !*(q+3);

    while (q && l < MAX_DCL_LINE_LENGTH) {
        if (!*q) {
            if (j > 0 && quote) {
                *p++ = '"';
                l++;
            }
            q = cmargv[++j];
            if (q) {
                if (ismcr && j > 1) quote = 1;
                tquote =  (strchr(q,' ')) != NULL || *q == '\0';
                *p++ = ' ';
                l++;
                if (quote || tquote) {
                    *p++ = '"';
                    l++;
                }
	    }
        } else {
            if ((quote||tquote) && *q == '"') {
                *p++ = '"';
                l++;
	    }
            *p++ = *q++;
            l++;
        }
    }
    *p = '\0';

    fp = safe_popen(aTHX_ subcmd,"wbF",&sts);
    if (fp == NULL) {
        PerlIO_printf(Perl_debug_log,"Can't open output pipe (status %d)",sts);
    }
}

static int background_process(pTHX_ int argc, char **argv)
{
char command[MAX_DCL_SYMBOL + 1] = "$";
$DESCRIPTOR(value, "");
static $DESCRIPTOR(cmd, "BACKGROUND$COMMAND");
static $DESCRIPTOR(null, "NLA0:");
static $DESCRIPTOR(pidsymbol, "SHELL_BACKGROUND_PID");
char pidstring[80];
$DESCRIPTOR(pidstr, "");
int pid;
unsigned long int flags = 17, one = 1, retsts;
int len;

    strcat(command, argv[0]);
    len = strlen(command);
    while (--argc && (len < MAX_DCL_SYMBOL))
	{
	strcat(command, " \"");
	strcat(command, *(++argv));
	strcat(command, "\"");
	len = strlen(command);
	}
    value.dsc$a_pointer = command;
    value.dsc$w_length = strlen(value.dsc$a_pointer);
    _ckvmssts_noperl(lib$set_symbol(&cmd, &value));
    retsts = lib$spawn(&cmd, &null, 0, &flags, 0, &pid);
    if (retsts == 0x38250) { /* DCL-W-NOTIFY - We must be BATCH, so retry */
	_ckvmssts_noperl(lib$spawn(&cmd, &null, 0, &one, 0, &pid));
    }
    else {
	_ckvmssts_noperl(retsts);
    }
#ifdef ARGPROC_DEBUG
    PerlIO_printf(Perl_debug_log, "%s\n", command);
#endif
    sprintf(pidstring, "%08X", pid);
    PerlIO_printf(Perl_debug_log, "%s\n", pidstring);
    pidstr.dsc$a_pointer = pidstring;
    pidstr.dsc$w_length = strlen(pidstr.dsc$a_pointer);
    lib$set_symbol(&pidsymbol, &pidstr);
    return(SS$_NORMAL);
}
/*}}}*/
/***** End of code taken from Mark Pizzolato's argproc.c package *****/


/* OS-specific initialization at image activation (not thread startup) */
/* Older VAXC header files lack these constants */
#ifndef JPI$_RIGHTS_SIZE
#  define JPI$_RIGHTS_SIZE 817
#endif
#ifndef KGB$M_SUBSYSTEM
#  define KGB$M_SUBSYSTEM 0x8
#endif
 
/* Avoid Newx() in vms_image_init as thread context has not been initialized. */

/*{{{void vms_image_init(int *, char ***)*/
void
vms_image_init(int *argcp, char ***argvp)
{
  char eqv[LNM$C_NAMLENGTH+1] = "";
  unsigned int len, tabct = 8, tabidx = 0;
  unsigned long int *mask, iosb[2], i, rlst[128], rsz;
  unsigned long int iprv[(sizeof(union prvdef) + sizeof(unsigned long int) - 1) / sizeof(unsigned long int)];
  unsigned short int dummy, rlen;
  struct dsc$descriptor_s **tabvec;
#if defined(PERL_IMPLICIT_CONTEXT)
  pTHX = NULL;
#endif
  struct itmlst_3 jpilist[4] = { {sizeof iprv,    JPI$_IMAGPRIV, iprv, &dummy},
                                 {sizeof rlst,  JPI$_RIGHTSLIST, rlst,  &rlen},
                                 { sizeof rsz, JPI$_RIGHTS_SIZE, &rsz, &dummy},
                                 {          0,                0,    0,      0} };

#ifdef KILL_BY_SIGPRC
    Perl_csighandler_init();
#endif

  _ckvmssts_noperl(sys$getjpiw(0,NULL,NULL,jpilist,iosb,NULL,NULL));
  _ckvmssts_noperl(iosb[0]);
  for (i = 0; i < sizeof iprv / sizeof(unsigned long int); i++) {
    if (iprv[i]) {           /* Running image installed with privs? */
      _ckvmssts_noperl(sys$setprv(0,iprv,0,NULL));       /* Turn 'em off. */
      will_taint = TRUE;
      break;
    }
  }
  /* Rights identifiers might trigger tainting as well. */
  if (!will_taint && (rlen || rsz)) {
    while (rlen < rsz) {
      /* We didn't get all the identifiers on the first pass.  Allocate a
       * buffer much larger than $GETJPI wants (rsz is size in bytes that
       * were needed to hold all identifiers at time of last call; we'll
       * allocate that many unsigned long ints), and go back and get 'em.
       * If it gave us less than it wanted to despite ample buffer space, 
       * something's broken.  Is your system missing a system identifier?
       */
      if (rsz <= jpilist[1].buflen) { 
         /* Perl_croak accvios when used this early in startup. */
         fprintf(stderr, "vms_image_init: $getjpiw refuses to store RIGHTSLIST of %u bytes in buffer of %u bytes.\n%s", 
                         rsz, (unsigned long) jpilist[1].buflen,
                         "Check your rights database for corruption.\n");
         exit(SS$_ABORT);
      }
      if (jpilist[1].bufadr != rlst) PerlMem_free(jpilist[1].bufadr);
      jpilist[1].bufadr = mask = (unsigned long int *) PerlMem_malloc(rsz * sizeof(unsigned long int));
      if (mask == NULL) _ckvmssts_noperl(SS$_INSFMEM);
      jpilist[1].buflen = rsz * sizeof(unsigned long int);
      _ckvmssts_noperl(sys$getjpiw(0,NULL,NULL,&jpilist[1],iosb,NULL,NULL));
      _ckvmssts_noperl(iosb[0]);
    }
    mask = jpilist[1].bufadr;
    /* Check attribute flags for each identifier (2nd longword); protected
     * subsystem identifiers trigger tainting.
     */
    for (i = 1; i < (rlen + sizeof(unsigned long int) - 1) / sizeof(unsigned long int); i += 2) {
      if (mask[i] & KGB$M_SUBSYSTEM) {
        will_taint = TRUE;
        break;
      }
    }
    if (mask != rlst) PerlMem_free(mask);
  }

  /* When Perl is in decc_filename_unix_report mode and is run from a concealed
   * logical, some versions of the CRTL will add a phanthom /000000/
   * directory.  This needs to be removed.
   */
  if (decc_filename_unix_report) {
  char * zeros;
  int ulen;
    ulen = strlen(argvp[0][0]);
    if (ulen > 7) {
      zeros = strstr(argvp[0][0], "/000000/");
      if (zeros != NULL) {
	int mlen;
	mlen = ulen - (zeros - argvp[0][0]) - 7;
	memmove(zeros, &zeros[7], mlen);
	ulen = ulen - 7;
	argvp[0][0][ulen] = '\0';
      }
    }
    /* It also may have a trailing dot that needs to be removed otherwise
     * it will be converted to VMS mode incorrectly.
     */
    ulen--;
    if ((argvp[0][0][ulen] == '.') && (decc_readdir_dropdotnotype))
      argvp[0][0][ulen] = '\0';
  }

  /* We need to use this hack to tell Perl it should run with tainting,
   * since its tainting flag may be part of the PL_curinterp struct, which
   * hasn't been allocated when vms_image_init() is called.
   */
  if (will_taint) {
    char **newargv, **oldargv;
    oldargv = *argvp;
    newargv = (char **) PerlMem_malloc(((*argcp)+2) * sizeof(char *));
    if (newargv == NULL) _ckvmssts_noperl(SS$_INSFMEM);
    newargv[0] = oldargv[0];
    newargv[1] = PerlMem_malloc(3 * sizeof(char));
    if (newargv[1] == NULL) _ckvmssts_noperl(SS$_INSFMEM);
    strcpy(newargv[1], "-T");
    Copy(&oldargv[1],&newargv[2],(*argcp)-1,char **);
    (*argcp)++;
    newargv[*argcp] = NULL;
    /* We orphan the old argv, since we don't know where it's come from,
     * so we don't know how to free it.
     */
    *argvp = newargv;
  }
  else {  /* Did user explicitly request tainting? */
    int i;
    char *cp, **av = *argvp;
    for (i = 1; i < *argcp; i++) {
      if (*av[i] != '-') break;
      for (cp = av[i]+1; *cp; cp++) {
        if (*cp == 'T') { will_taint = 1; break; }
        else if ( (*cp == 'd' || *cp == 'V') && *(cp+1) == ':' ||
                  strchr("DFIiMmx",*cp)) break;
      }
      if (will_taint) break;
    }
  }

  for (tabidx = 0;
       len = my_trnlnm("PERL_ENV_TABLES",eqv,tabidx);
       tabidx++) {
    if (!tabidx) {
      tabvec = (struct dsc$descriptor_s **)
	    PerlMem_malloc(tabct * sizeof(struct dsc$descriptor_s *));
      if (tabvec == NULL) _ckvmssts_noperl(SS$_INSFMEM);
    }
    else if (tabidx >= tabct) {
      tabct += 8;
      tabvec = (struct dsc$descriptor_s **) PerlMem_realloc(tabvec, tabct * sizeof(struct dsc$descriptor_s *));
      if (tabvec == NULL) _ckvmssts_noperl(SS$_INSFMEM);
    }
    tabvec[tabidx] = (struct dsc$descriptor_s *) PerlMem_malloc(sizeof(struct dsc$descriptor_s));
    if (tabvec[tabidx] == NULL) _ckvmssts_noperl(SS$_INSFMEM);
    tabvec[tabidx]->dsc$w_length  = 0;
    tabvec[tabidx]->dsc$b_dtype   = DSC$K_DTYPE_T;
    tabvec[tabidx]->dsc$b_class   = DSC$K_CLASS_D;
    tabvec[tabidx]->dsc$a_pointer = NULL;
    _ckvmssts_noperl(lib$scopy_r_dx(&len,eqv,tabvec[tabidx]));
  }
  if (tabidx) { tabvec[tabidx] = NULL; env_tables = tabvec; }

  getredirection(argcp,argvp);
  return;
}
/*}}}*/


/* trim_unixpath()
 * Trim Unix-style prefix off filespec, so it looks like what a shell
 * glob expansion would return (i.e. from specified prefix on, not
 * full path).  Note that returned filespec is Unix-style, regardless
 * of whether input filespec was VMS-style or Unix-style.
 *
 * fspec is filespec to be trimmed, and wildspec is wildcard spec used to
 * determine prefix (both may be in VMS or Unix syntax).  opts is a bit
 * vector of options; at present, only bit 0 is used, and if set tells
 * trim unixpath to try the current default directory as a prefix when
 * presented with a possibly ambiguous ... wildcard.
 *
 * Returns !=0 on success, with trimmed filespec replacing contents of
 * fspec, and 0 on failure, with contents of fpsec unchanged.
 */
/*{{{int trim_unixpath(char *fspec, char *wildspec, int opts)*/
int
Perl_trim_unixpath(pTHX_ char *fspec, const char *wildspec, int opts)
{
  char *unixified, *unixwild,
       *template, *base, *end, *cp1, *cp2;
  register int tmplen, reslen = 0, dirs = 0;

  unixwild = PerlMem_malloc(VMS_MAXRSS);
  if (unixwild == NULL) _ckvmssts(SS$_INSFMEM);
  if (!wildspec || !fspec) return 0;
  template = unixwild;
  if (strpbrk(wildspec,"]>:") != NULL) {
    if (do_tounixspec(wildspec,unixwild,0,NULL) == NULL) {
        PerlMem_free(unixwild);
	return 0;
    }
  }
  else {
    strncpy(unixwild, wildspec, VMS_MAXRSS-1);
    unixwild[VMS_MAXRSS-1] = 0;
  }
  unixified = PerlMem_malloc(VMS_MAXRSS);
  if (unixified == NULL) _ckvmssts(SS$_INSFMEM);
  if (strpbrk(fspec,"]>:") != NULL) {
    if (do_tounixspec(fspec,unixified,0,NULL) == NULL) {
        PerlMem_free(unixwild);
        PerlMem_free(unixified);
	return 0;
    }
    else base = unixified;
    /* reslen != 0 ==> we had to unixify resultant filespec, so we must
     * check to see that final result fits into (isn't longer than) fspec */
    reslen = strlen(fspec);
  }
  else base = fspec;

  /* No prefix or absolute path on wildcard, so nothing to remove */
  if (!*template || *template == '/') {
    PerlMem_free(unixwild);
    if (base == fspec) {
        PerlMem_free(unixified);
	return 1;
    }
    tmplen = strlen(unixified);
    if (tmplen > reslen) {
        PerlMem_free(unixified);
	return 0;  /* not enough space */
    }
    /* Copy unixified resultant, including trailing NUL */
    memmove(fspec,unixified,tmplen+1);
    PerlMem_free(unixified);
    return 1;
  }

  for (end = base; *end; end++) ;  /* Find end of resultant filespec */
  if ((cp1 = strstr(template,".../")) == NULL) { /* No ...; just count elts */
    for (cp1 = template; *cp1; cp1++) if (*cp1 == '/') dirs++;
    for (cp1 = end ;cp1 >= base; cp1--)
      if ((*cp1 == '/') && !dirs--) /* postdec so we get front of rel path */
        { cp1++; break; }
    if (cp1 != fspec) memmove(fspec,cp1, end - cp1 + 1);
    PerlMem_free(unixified);
    PerlMem_free(unixwild);
    return 1;
  }
  else {
    char *tpl, *lcres;
    char *front, *nextell, *lcend, *lcfront, *ellipsis = cp1;
    int ells = 1, totells, segdirs, match;
    struct dsc$descriptor_s wilddsc = {0, DSC$K_DTYPE_T, DSC$K_CLASS_S, NULL},
                            resdsc =  {0, DSC$K_DTYPE_T, DSC$K_CLASS_S, 0};

    while ((cp1 = strstr(ellipsis+4,".../")) != NULL) {ellipsis = cp1; ells++;}
    totells = ells;
    for (cp1 = ellipsis+4; *cp1; cp1++) if (*cp1 == '/') dirs++;
    tpl = PerlMem_malloc(VMS_MAXRSS);
    if (tpl == NULL) _ckvmssts(SS$_INSFMEM);
    if (ellipsis == template && opts & 1) {
      /* Template begins with an ellipsis.  Since we can't tell how many
       * directory names at the front of the resultant to keep for an
       * arbitrary starting point, we arbitrarily choose the current
       * default directory as a starting point.  If it's there as a prefix,
       * clip it off.  If not, fall through and act as if the leading
       * ellipsis weren't there (i.e. return shortest possible path that
       * could match template).
       */
      if (getcwd(tpl, (VMS_MAXRSS - 1),0) == NULL) {
	  PerlMem_free(tpl);
	  PerlMem_free(unixified);
	  PerlMem_free(unixwild);
	  return 0;
      }
      if (!decc_efs_case_preserve) {
 	for (cp1 = tpl, cp2 = base; *cp1 && *cp2; cp1++,cp2++)
	  if (_tolower(*cp1) != _tolower(*cp2)) break;
      }
      segdirs = dirs - totells;  /* Min # of dirs we must have left */
      for (front = cp2+1; *front; front++) if (*front == '/') segdirs--;
      if (*cp1 == '\0' && *cp2 == '/' && segdirs < 1) {
        memmove(fspec,cp2+1,end - cp2);
	PerlMem_free(tpl);
	PerlMem_free(unixified);
	PerlMem_free(unixwild);
        return 1;
      }
    }
    /* First off, back up over constant elements at end of path */
    if (dirs) {
      for (front = end ; front >= base; front--)
         if (*front == '/' && !dirs--) { front++; break; }
    }
    lcres = PerlMem_malloc(VMS_MAXRSS);
    if (lcres == NULL) _ckvmssts(SS$_INSFMEM);
    for (cp1=template,cp2=lcres; *cp1 && cp2 <= lcres + (VMS_MAXRSS - 1);
         cp1++,cp2++) {
	    if (!decc_efs_case_preserve) {
		*cp2 = _tolower(*cp1);  /* Make lc copy for match */
	    }
	    else {
		*cp2 = *cp1;
	    }
    }
    if (cp1 != '\0') {
	PerlMem_free(tpl);
	PerlMem_free(unixified);
	PerlMem_free(unixwild);
	PerlMem_free(lcres);
	return 0;  /* Path too long. */
    }
    lcend = cp2;
    *cp2 = '\0';  /* Pick up with memcpy later */
    lcfront = lcres + (front - base);
    /* Now skip over each ellipsis and try to match the path in front of it. */
    while (ells--) {
      for (cp1 = ellipsis - 2; cp1 >= template; cp1--)
        if (*(cp1)   == '.' && *(cp1+1) == '.' &&
            *(cp1+2) == '.' && *(cp1+3) == '/'    ) break;
      if (cp1 < template) break; /* template started with an ellipsis */
      if (cp1 + 4 == ellipsis) { /* Consecutive ellipses */
        ellipsis = cp1; continue;
      }
      wilddsc.dsc$a_pointer = tpl;
      wilddsc.dsc$w_length = resdsc.dsc$w_length = ellipsis - 1 - cp1;
      nextell = cp1;
      for (segdirs = 0, cp2 = tpl;
           cp1 <= ellipsis - 1 && cp2 <= tpl + (VMS_MAXRSS-1);
           cp1++, cp2++) {
         if (*cp1 == '?') *cp2 = '%'; /* Substitute VMS' wildcard for Unix' */
         else {
	    if (!decc_efs_case_preserve) {
	      *cp2 = _tolower(*cp1);  /* else lowercase for match */
	    }
	    else {
	      *cp2 = *cp1;  /* else preserve case for match */
	    }
	 }
         if (*cp2 == '/') segdirs++;
      }
      if (cp1 != ellipsis - 1) {
	  PerlMem_free(tpl);
	  PerlMem_free(unixified);
	  PerlMem_free(unixwild);
	  PerlMem_free(lcres);
	  return 0; /* Path too long */
      }
      /* Back up at least as many dirs as in template before matching */
      for (cp1 = lcfront - 1; segdirs && cp1 >= lcres; cp1--)
        if (*cp1 == '/' && !segdirs--) { cp1++; break; }
      for (match = 0; cp1 > lcres;) {
        resdsc.dsc$a_pointer = cp1;
        if (str$match_wild(&wilddsc,&resdsc) == STR$_MATCH) { 
          match++;
          if (match == 1) lcfront = cp1;
        }
        for ( ; cp1 >= lcres; cp1--) if (*cp1 == '/') { cp1++; break; }
      }
      if (!match) {
	PerlMem_free(tpl);
	PerlMem_free(unixified);
	PerlMem_free(unixwild);
	PerlMem_free(lcres);
	return 0;  /* Can't find prefix ??? */
      }
      if (match > 1 && opts & 1) {
        /* This ... wildcard could cover more than one set of dirs (i.e.
         * a set of similar dir names is repeated).  If the template
         * contains more than 1 ..., upstream elements could resolve the
         * ambiguity, but it's not worth a full backtracking setup here.
         * As a quick heuristic, clip off the current default directory
         * if it's present to find the trimmed spec, else use the
         * shortest string that this ... could cover.
         */
        char def[NAM$C_MAXRSS+1], *st;

        if (getcwd(def, sizeof def,0) == NULL) {
	    Safefree(unixified);
	    Safefree(unixwild);
	    Safefree(lcres);
	    Safefree(tpl);
	    return 0;
	}
	if (!decc_efs_case_preserve) {
	  for (cp1 = def, cp2 = base; *cp1 && *cp2; cp1++,cp2++)
	    if (_tolower(*cp1) != _tolower(*cp2)) break;
	}
        segdirs = dirs - totells;  /* Min # of dirs we must have left */
        for (st = cp2+1; *st; st++) if (*st == '/') segdirs--;
        if (*cp1 == '\0' && *cp2 == '/') {
          memmove(fspec,cp2+1,end - cp2);
	  PerlMem_free(tpl);
	  PerlMem_free(unixified);
	  PerlMem_free(unixwild);
	  PerlMem_free(lcres);
          return 1;
        }
        /* Nope -- stick with lcfront from above and keep going. */
      }
    }
    memmove(fspec,base + (lcfront - lcres), lcend - lcfront + 1);
    PerlMem_free(tpl);
    PerlMem_free(unixified);
    PerlMem_free(unixwild);
    PerlMem_free(lcres);
    return 1;
    ellipsis = nextell;
  }

}  /* end of trim_unixpath() */
/*}}}*/


/*
 *  VMS readdir() routines.
 *  Written by Rich $alz, <rsalz@bbn.com> in August, 1990.
 *
 *  21-Jul-1994  Charles Bailey  bailey@newman.upenn.edu
 *  Minor modifications to original routines.
 */

/* readdir may have been redefined by reentr.h, so make sure we get
 * the local version for what we do here.
 */
#ifdef readdir
# undef readdir
#endif
#if !defined(PERL_IMPLICIT_CONTEXT)
# define readdir Perl_readdir
#else
# define readdir(a) Perl_readdir(aTHX_ a)
#endif

    /* Number of elements in vms_versions array */
#define VERSIZE(e)	(sizeof e->vms_versions / sizeof e->vms_versions[0])

/*
 *  Open a directory, return a handle for later use.
 */
/*{{{ DIR *opendir(char*name) */
DIR *
Perl_opendir(pTHX_ const char *name)
{
    DIR *dd;
    char *dir;
    Stat_t sb;

    Newx(dir, VMS_MAXRSS, char);
    if (do_tovmspath(name,dir,0,NULL) == NULL) {
      Safefree(dir);
      return NULL;
    }
    /* Check access before stat; otherwise stat does not
     * accurately report whether it's a directory.
     */
    if (!cando_by_name_int(S_IRUSR,0,dir,PERL_RMSEXPAND_M_VMS_IN)) {
      /* cando_by_name has already set errno */
      Safefree(dir);
      return NULL;
    }
    if (flex_stat(dir,&sb) == -1) return NULL;
    if (!S_ISDIR(sb.st_mode)) {
      Safefree(dir);
      set_errno(ENOTDIR);  set_vaxc_errno(RMS$_DIR);
      return NULL;
    }
    /* Get memory for the handle, and the pattern. */
    Newx(dd,1,DIR);
    Newx(dd->pattern,strlen(dir)+sizeof "*.*" + 1,char);

    /* Fill in the fields; mainly playing with the descriptor. */
    sprintf(dd->pattern, "%s*.*",dir);
    Safefree(dir);
    dd->context = 0;
    dd->count = 0;
    dd->flags = 0;
    /* By saying we always want the result of readdir() in unix format, we 
     * are really saying we want all the escapes removed.  Otherwise the caller,
     * having no way to know whether it's already in VMS format, might send it
     * through tovmsspec again, thus double escaping.
     */
    dd->flags = PERL_VMSDIR_M_UNIXSPECS;
    dd->pat.dsc$a_pointer = dd->pattern;
    dd->pat.dsc$w_length = strlen(dd->pattern);
    dd->pat.dsc$b_dtype = DSC$K_DTYPE_T;
    dd->pat.dsc$b_class = DSC$K_CLASS_S;
    dd->mutex = NULL;

    return dd;
}  /* end of opendir() */
/*}}}*/

/*
 *  Set the flag to indicate we want versions or not.
 */
/*{{{ void vmsreaddirversions(DIR *dd, int flag)*/
void
vmsreaddirversions(DIR *dd, int flag)
{
    if (flag)
	dd->flags |= PERL_VMSDIR_M_VERSIONS;
    else
	dd->flags &= ~PERL_VMSDIR_M_VERSIONS;
}
/*}}}*/

/*
 *  Free up an opened directory.
 */
/*{{{ void closedir(DIR *dd)*/
void
Perl_closedir(DIR *dd)
{
    int sts;

    sts = lib$find_file_end(&dd->context);
    Safefree(dd->pattern);
    Safefree(dd);
}
/*}}}*/

/*
 *  Collect all the version numbers for the current file.
 */
static void
collectversions(pTHX_ DIR *dd)
{
    struct dsc$descriptor_s	pat;
    struct dsc$descriptor_s	res;
    struct dirent *e;
    char *p, *text, *buff;
    int i;
    unsigned long context, tmpsts;

    /* Convenient shorthand. */
    e = &dd->entry;

    /* Add the version wildcard, ignoring the "*.*" put on before */
    i = strlen(dd->pattern);
    Newx(text,i + e->d_namlen + 3,char);
    strcpy(text, dd->pattern);
    sprintf(&text[i - 3], "%s;*", e->d_name);

    /* Set up the pattern descriptor. */
    pat.dsc$a_pointer = text;
    pat.dsc$w_length = i + e->d_namlen - 1;
    pat.dsc$b_dtype = DSC$K_DTYPE_T;
    pat.dsc$b_class = DSC$K_CLASS_S;

    /* Set up result descriptor. */
    Newx(buff, VMS_MAXRSS, char);
    res.dsc$a_pointer = buff;
    res.dsc$w_length = VMS_MAXRSS - 1;
    res.dsc$b_dtype = DSC$K_DTYPE_T;
    res.dsc$b_class = DSC$K_CLASS_S;

    /* Read files, collecting versions. */
    for (context = 0, e->vms_verscount = 0;
         e->vms_verscount < VERSIZE(e);
         e->vms_verscount++) {
	unsigned long rsts;
	unsigned long flags = 0;

#ifdef VMS_LONGNAME_SUPPORT
	flags = LIB$M_FIL_LONG_NAMES;
#endif
	tmpsts = lib$find_file(&pat, &res, &context, NULL, NULL, &rsts, &flags);
	if (tmpsts == RMS$_NMF || context == 0) break;
	_ckvmssts(tmpsts);
	buff[VMS_MAXRSS - 1] = '\0';
	if ((p = strchr(buff, ';')))
	    e->vms_versions[e->vms_verscount] = atoi(p + 1);
	else
	    e->vms_versions[e->vms_verscount] = -1;
    }

    _ckvmssts(lib$find_file_end(&context));
    Safefree(text);
    Safefree(buff);

}  /* end of collectversions() */

/*
 *  Read the next entry from the directory.
 */
/*{{{ struct dirent *readdir(DIR *dd)*/
struct dirent *
Perl_readdir(pTHX_ DIR *dd)
{
    struct dsc$descriptor_s	res;
    char *p, *buff;
    unsigned long int tmpsts;
    unsigned long rsts;
    unsigned long flags = 0;
    char * v_spec, * r_spec, * d_spec, * n_spec, * e_spec, * vs_spec;
    int sts, v_len, r_len, d_len, n_len, e_len, vs_len;

    /* Set up result descriptor, and get next file. */
    Newx(buff, VMS_MAXRSS, char);
    res.dsc$a_pointer = buff;
    res.dsc$w_length = VMS_MAXRSS - 1;
    res.dsc$b_dtype = DSC$K_DTYPE_T;
    res.dsc$b_class = DSC$K_CLASS_S;

#ifdef VMS_LONGNAME_SUPPORT
    flags = LIB$M_FIL_LONG_NAMES;
#endif

    tmpsts = lib$find_file
	(&dd->pat, &res, &dd->context, NULL, NULL, &rsts, &flags);
    if ( tmpsts == RMS$_NMF || dd->context == 0) return NULL;  /* None left. */
    if (!(tmpsts & 1)) {
      set_vaxc_errno(tmpsts);
      switch (tmpsts) {
        case RMS$_PRV:
          set_errno(EACCES); break;
        case RMS$_DEV:
          set_errno(ENODEV); break;
        case RMS$_DIR:
          set_errno(ENOTDIR); break;
        case RMS$_FNF: case RMS$_DNF:
          set_errno(ENOENT); break;
        default:
          set_errno(EVMSERR);
      }
      Safefree(buff);
      return NULL;
    }
    dd->count++;
    /* Force the buffer to end with a NUL, and downcase name to match C convention. */
    buff[res.dsc$w_length] = '\0';
    p = buff + res.dsc$w_length;
    while (--p >= buff) if (!isspace(*p)) break;  
    *p = '\0';
    if (!decc_efs_case_preserve) {
      for (p = buff; *p; p++) *p = _tolower(*p);
    }

    /* Skip any directory component and just copy the name. */
    sts = vms_split_path
       (buff,
	&v_spec,
	&v_len,
	&r_spec,
	&r_len,
	&d_spec,
	&d_len,
	&n_spec,
	&n_len,
	&e_spec,
	&e_len,
	&vs_spec,
	&vs_len);

    /* Drop NULL extensions on UNIX file specification */
    if ((dd->flags & PERL_VMSDIR_M_UNIXSPECS &&
	(e_len == 1) && decc_readdir_dropdotnotype)) {
	e_len = 0;
	e_spec[0] = '\0';
    }

    strncpy(dd->entry.d_name, n_spec, n_len + e_len);
    dd->entry.d_name[n_len + e_len] = '\0';
    dd->entry.d_namlen = strlen(dd->entry.d_name);

    /* Convert the filename to UNIX format if needed */
    if (dd->flags & PERL_VMSDIR_M_UNIXSPECS) {

	/* Translate the encoded characters. */
	/* Fixme: Unicode handling could result in embedded 0 characters */
	if (strchr(dd->entry.d_name, '^') != NULL) {
	    char new_name[256];
	    char * q;
	    p = dd->entry.d_name;
	    q = new_name;
	    while (*p != 0) {
		int inchars_read, outchars_added;
		inchars_read = copy_expand_vms_filename_escape(q, p, &outchars_added);
		p += inchars_read;
		q += outchars_added;
		/* fix-me */
		/* if outchars_added > 1, then this is a wide file specification */
		/* Wide file specifications need to be passed in Perl */
		/* counted strings apparently with a Unicode flag */
	    }
	    *q = 0;
	    strcpy(dd->entry.d_name, new_name);
	    dd->entry.d_namlen = strlen(dd->entry.d_name);
	}
    }

    dd->entry.vms_verscount = 0;
    if (dd->flags & PERL_VMSDIR_M_VERSIONS) collectversions(aTHX_ dd);
    Safefree(buff);
    return &dd->entry;

}  /* end of readdir() */
/*}}}*/

/*
 *  Read the next entry from the directory -- thread-safe version.
 */
/*{{{ int readdir_r(DIR *dd, struct dirent *entry, struct dirent **result)*/
int
Perl_readdir_r(pTHX_ DIR *dd, struct dirent *entry, struct dirent **result)
{
    int retval;

    MUTEX_LOCK( (perl_mutex *) dd->mutex );

    entry = readdir(dd);
    *result = entry;
    retval = ( *result == NULL ? errno : 0 );

    MUTEX_UNLOCK( (perl_mutex *) dd->mutex );

    return retval;

}  /* end of readdir_r() */
/*}}}*/

/*
 *  Return something that can be used in a seekdir later.
 */
/*{{{ long telldir(DIR *dd)*/
long
Perl_telldir(DIR *dd)
{
    return dd->count;
}
/*}}}*/

/*
 *  Return to a spot where we used to be.  Brute force.
 */
/*{{{ void seekdir(DIR *dd,long count)*/
void
Perl_seekdir(pTHX_ DIR *dd, long count)
{
    int old_flags;

    /* If we haven't done anything yet... */
    if (dd->count == 0)
	return;

    /* Remember some state, and clear it. */
    old_flags = dd->flags;
    dd->flags &= ~PERL_VMSDIR_M_VERSIONS;
    _ckvmssts(lib$find_file_end(&dd->context));
    dd->context = 0;

    /* The increment is in readdir(). */
    for (dd->count = 0; dd->count < count; )
	readdir(dd);

    dd->flags = old_flags;

}  /* end of seekdir() */
/*}}}*/

/* VMS subprocess management
 *
 * my_vfork() - just a vfork(), after setting a flag to record that
 * the current script is trying a Unix-style fork/exec.
 *
 * vms_do_aexec() and vms_do_exec() are called in response to the
 * perl 'exec' function.  If this follows a vfork call, then they
 * call out the regular perl routines in doio.c which do an
 * execvp (for those who really want to try this under VMS).
 * Otherwise, they do exactly what the perl docs say exec should
 * do - terminate the current script and invoke a new command
 * (See below for notes on command syntax.)
 *
 * do_aspawn() and do_spawn() implement the VMS side of the perl
 * 'system' function.
 *
 * Note on command arguments to perl 'exec' and 'system': When handled
 * in 'VMSish fashion' (i.e. not after a call to vfork) The args
 * are concatenated to form a DCL command string.  If the first non-numeric
 * arg begins with '$' (i.e. the perl script had "\$ Type" or some such),
 * the command string is handed off to DCL directly.  Otherwise,
 * the first token of the command is taken as the filespec of an image
 * to run.  The filespec is expanded using a default type of '.EXE' and
 * the process defaults for device, directory, etc., and if found, the resultant
 * filespec is invoked using the DCL verb 'MCR', and passed the rest of
 * the command string as parameters.  This is perhaps a bit complicated,
 * but I hope it will form a happy medium between what VMS folks expect
 * from lib$spawn and what Unix folks expect from exec.
 */

static int vfork_called;

/*{{{int my_vfork()*/
int
my_vfork()
{
  vfork_called++;
  return vfork();
}
/*}}}*/


static void
vms_execfree(struct dsc$descriptor_s *vmscmd) 
{
  if (vmscmd) {
      if (vmscmd->dsc$a_pointer) {
          PerlMem_free(vmscmd->dsc$a_pointer);
      }
      PerlMem_free(vmscmd);
  }
}

static char *
setup_argstr(pTHX_ SV *really, SV **mark, SV **sp)
{
  char *junk, *tmps = NULL;
  register size_t cmdlen = 0;
  size_t rlen;
  register SV **idx;
  STRLEN n_a;

  idx = mark;
  if (really) {
    tmps = SvPV(really,rlen);
    if (*tmps) {
      cmdlen += rlen + 1;
      idx++;
    }
  }
  
  for (idx++; idx <= sp; idx++) {
    if (*idx) {
      junk = SvPVx(*idx,rlen);
      cmdlen += rlen ? rlen + 1 : 0;
    }
  }
  Newx(PL_Cmd, cmdlen+1, char);

  if (tmps && *tmps) {
    strcpy(PL_Cmd,tmps);
    mark++;
  }
  else *PL_Cmd = '\0';
  while (++mark <= sp) {
    if (*mark) {
      char *s = SvPVx(*mark,n_a);
      if (!*s) continue;
      if (*PL_Cmd) strcat(PL_Cmd," ");
      strcat(PL_Cmd,s);
    }
  }
  return PL_Cmd;

}  /* end of setup_argstr() */


static unsigned long int
setup_cmddsc(pTHX_ const char *incmd, int check_img, int *suggest_quote,
                   struct dsc$descriptor_s **pvmscmd)
{
  char vmsspec[NAM$C_MAXRSS+1], resspec[NAM$C_MAXRSS+1];
  char image_name[NAM$C_MAXRSS+1];
  char image_argv[NAM$C_MAXRSS+1];
  $DESCRIPTOR(defdsc,".EXE");
  $DESCRIPTOR(defdsc2,".");
  $DESCRIPTOR(resdsc,resspec);
  struct dsc$descriptor_s *vmscmd;
  struct dsc$descriptor_s imgdsc = {0, DSC$K_DTYPE_T, DSC$K_CLASS_S, 0};
  unsigned long int cxt = 0, flags = 1, retsts = SS$_NORMAL;
  register char *s, *rest, *cp, *wordbreak;
  char * cmd;
  int cmdlen;
  register int isdcl;

  vmscmd = PerlMem_malloc(sizeof(struct dsc$descriptor_s));
  if (vmscmd == NULL) _ckvmssts(SS$_INSFMEM);

  /* Make a copy for modification */
  cmdlen = strlen(incmd);
  cmd = PerlMem_malloc(cmdlen+1);
  if (cmd == NULL) _ckvmssts(SS$_INSFMEM);
  strncpy(cmd, incmd, cmdlen);
  cmd[cmdlen] = 0;
  image_name[0] = 0;
  image_argv[0] = 0;

  vmscmd->dsc$a_pointer = NULL;
  vmscmd->dsc$b_dtype  = DSC$K_DTYPE_T;
  vmscmd->dsc$b_class  = DSC$K_CLASS_S;
  vmscmd->dsc$w_length = 0;
  if (pvmscmd) *pvmscmd = vmscmd;

  if (suggest_quote) *suggest_quote = 0;

  if (strlen(cmd) > MAX_DCL_LINE_LENGTH) {
    PerlMem_free(cmd);
    return CLI$_BUFOVF;                /* continuation lines currently unsupported */
  }

  s = cmd;

  while (*s && isspace(*s)) s++;

  if (*s == '@' || *s == '$') {
    vmsspec[0] = *s;  rest = s + 1;
    for (cp = &vmsspec[1]; *rest && isspace(*rest); rest++,cp++) *cp = *rest;
  }
  else { cp = vmsspec; rest = s; }
  if (*rest == '.' || *rest == '/') {
    char *cp2;
    for (cp2 = resspec;
         *rest && !isspace(*rest) && cp2 - resspec < sizeof resspec;
         rest++, cp2++) *cp2 = *rest;
    *cp2 = '\0';
    if (do_tovmsspec(resspec,cp,0,NULL)) { 
      s = vmsspec;
      if (*rest) {
        for (cp2 = vmsspec + strlen(vmsspec);
             *rest && cp2 - vmsspec < sizeof vmsspec;
             rest++, cp2++) *cp2 = *rest;
        *cp2 = '\0';
      }
    }
  }
  /* Intuit whether verb (first word of cmd) is a DCL command:
   *   - if first nonspace char is '@', it's a DCL indirection
   * otherwise
   *   - if verb contains a filespec separator, it's not a DCL command
   *   - if it doesn't, caller tells us whether to default to a DCL
   *     command, or to a local image unless told it's DCL (by leading '$')
   */
  if (*s == '@') {
      isdcl = 1;
      if (suggest_quote) *suggest_quote = 1;
  } else {
    register char *filespec = strpbrk(s,":<[.;");
    rest = wordbreak = strpbrk(s," \"\t/");
    if (!wordbreak) wordbreak = s + strlen(s);
    if (*s == '$') check_img = 0;
    if (filespec && (filespec < wordbreak)) isdcl = 0;
    else isdcl = !check_img;
  }

  if (!isdcl) {
    int rsts;
    imgdsc.dsc$a_pointer = s;
    imgdsc.dsc$w_length = wordbreak - s;
    retsts = lib$find_file(&imgdsc,&resdsc,&cxt,&defdsc,0,&rsts,&flags);
    if (!(retsts&1)) {
        _ckvmssts(lib$find_file_end(&cxt));
        retsts = lib$find_file(&imgdsc,&resdsc,&cxt,&defdsc2,0,&rsts,&flags);
      if (!(retsts & 1) && *s == '$') {
        _ckvmssts(lib$find_file_end(&cxt));
	imgdsc.dsc$a_pointer++; imgdsc.dsc$w_length--;
	retsts = lib$find_file(&imgdsc,&resdsc,&cxt,&defdsc,0,&rsts,&flags);
	if (!(retsts&1)) {
	  _ckvmssts(lib$find_file_end(&cxt));
          retsts = lib$find_file(&imgdsc,&resdsc,&cxt,&defdsc2,0,&rsts,&flags);
        }
      }
    }
    _ckvmssts(lib$find_file_end(&cxt));

    if (retsts & 1) {
      FILE *fp;
      s = resspec;
      while (*s && !isspace(*s)) s++;
      *s = '\0';

      /* check that it's really not DCL with no file extension */
      fp = fopen(resspec,"r","ctx=bin","ctx=rec","shr=get");
      if (fp) {
        char b[256] = {0,0,0,0};
        read(fileno(fp), b, 256);
        isdcl = isprint(b[0]) && isprint(b[1]) && isprint(b[2]) && isprint(b[3]);
	if (isdcl) {
	  int shebang_len;

	  /* Check for script */
	  shebang_len = 0;
	  if ((b[0] == '#') && (b[1] == '!'))
	     shebang_len = 2;
#ifdef ALTERNATE_SHEBANG
	  else {
	    shebang_len = strlen(ALTERNATE_SHEBANG);
	    if (strncmp(b, ALTERNATE_SHEBANG, shebang_len) == 0) {
	      char * perlstr;
		perlstr = strstr("perl",b);
		if (perlstr == NULL)
		  shebang_len = 0;
	    }
	    else
	      shebang_len = 0;
	  }
#endif

	  if (shebang_len > 0) {
	  int i;
	  int j;
	  char tmpspec[NAM$C_MAXRSS + 1];

	    i = shebang_len;
	     /* Image is following after white space */
	    /*--------------------------------------*/
	    while (isprint(b[i]) && isspace(b[i]))
		i++;

	    j = 0;
	    while (isprint(b[i]) && !isspace(b[i])) {
		tmpspec[j++] = b[i++];
		if (j >= NAM$C_MAXRSS)
		   break;
	    }
	    tmpspec[j] = '\0';

	     /* There may be some default parameters to the image */
	    /*---------------------------------------------------*/
	    j = 0;
	    while (isprint(b[i])) {
		image_argv[j++] = b[i++];
		if (j >= NAM$C_MAXRSS)
		   break;
	    }
	    while ((j > 0) && !isprint(image_argv[j-1]))
		j--;
	    image_argv[j] = 0;

	    /* It will need to be converted to VMS format and validated */
	    if (tmpspec[0] != '\0') {
	      char * iname;

	       /* Try to find the exact program requested to be run */
	      /*---------------------------------------------------*/
	      iname = do_rmsexpand
		 (tmpspec, image_name, 0, ".exe",
		  PERL_RMSEXPAND_M_VMS, NULL, NULL);
	      if (iname != NULL) {
		if (cando_by_name_int
			(S_IXUSR,0,image_name,PERL_RMSEXPAND_M_VMS_IN)) {
		  /* MCR prefix needed */
		  isdcl = 0;
		}
		else {
		   /* Try again with a null type */
		  /*----------------------------*/
		  iname = do_rmsexpand
		    (tmpspec, image_name, 0, ".",
		     PERL_RMSEXPAND_M_VMS, NULL, NULL);
		  if (iname != NULL) {
		    if (cando_by_name_int
			 (S_IXUSR,0,image_name, PERL_RMSEXPAND_M_VMS_IN)) {
		      /* MCR prefix needed */
		      isdcl = 0;
		    }
		  }
		}

		 /* Did we find the image to run the script? */
		/*------------------------------------------*/
		if (isdcl) {
		  char *tchr;

		   /* Assume DCL or foreign command exists */
		  /*--------------------------------------*/
		  tchr = strrchr(tmpspec, '/');
		  if (tchr != NULL) {
		    tchr++;
		  }
		  else {
		    tchr = tmpspec;
		  }
		  strcpy(image_name, tchr);
		}
	      }
	    }
	  }
	}
        fclose(fp);
      }
      if (check_img && isdcl) return RMS$_FNF;

      if (cando_by_name(S_IXUSR,0,resspec)) {
        vmscmd->dsc$a_pointer = PerlMem_malloc(MAX_DCL_LINE_LENGTH);
	if (vmscmd->dsc$a_pointer == NULL) _ckvmssts(SS$_INSFMEM);
        if (!isdcl) {
            strcpy(vmscmd->dsc$a_pointer,"$ MCR ");
	    if (image_name[0] != 0) {
		strcat(vmscmd->dsc$a_pointer, image_name);
		strcat(vmscmd->dsc$a_pointer, " ");
	    }
	} else if (image_name[0] != 0) {
	    strcpy(vmscmd->dsc$a_pointer, image_name);
	    strcat(vmscmd->dsc$a_pointer, " ");
        } else {
            strcpy(vmscmd->dsc$a_pointer,"@");
        }
        if (suggest_quote) *suggest_quote = 1;

	/* If there is an image name, use original command */
	if (image_name[0] == 0)
	    strcat(vmscmd->dsc$a_pointer,resspec);
	else {
	    rest = cmd;
	    while (*rest && isspace(*rest)) rest++;
	}

	if (image_argv[0] != 0) {
	  strcat(vmscmd->dsc$a_pointer,image_argv);
	  strcat(vmscmd->dsc$a_pointer, " ");
	}
        if (rest) {
	   int rest_len;
	   int vmscmd_len;

	   rest_len = strlen(rest);
	   vmscmd_len = strlen(vmscmd->dsc$a_pointer);
	   if ((rest_len + vmscmd_len) < MAX_DCL_LINE_LENGTH)
	      strcat(vmscmd->dsc$a_pointer,rest);
	   else
	     retsts = CLI$_BUFOVF;
	}
        vmscmd->dsc$w_length = strlen(vmscmd->dsc$a_pointer);
        PerlMem_free(cmd);
        return (vmscmd->dsc$w_length > MAX_DCL_LINE_LENGTH ? CLI$_BUFOVF : retsts);
      }
      else
	retsts = RMS$_PRV;
    }
  }
  /* It's either a DCL command or we couldn't find a suitable image */
  vmscmd->dsc$w_length = strlen(cmd);

  vmscmd->dsc$a_pointer = PerlMem_malloc(vmscmd->dsc$w_length + 1);
  strncpy(vmscmd->dsc$a_pointer,cmd,vmscmd->dsc$w_length);
  vmscmd->dsc$a_pointer[vmscmd->dsc$w_length] = 0;

  PerlMem_free(cmd);

  /* check if it's a symbol (for quoting purposes) */
  if (suggest_quote && !*suggest_quote) { 
    int iss;     
    char equiv[LNM$C_NAMLENGTH];
    struct dsc$descriptor_s eqvdsc = {sizeof(equiv), DSC$K_DTYPE_T, DSC$K_CLASS_S, 0};
    eqvdsc.dsc$a_pointer = equiv;

    iss = lib$get_symbol(vmscmd,&eqvdsc);
    if (iss&1 && (*equiv == '$' || *equiv == '@')) *suggest_quote = 1;
  }
  if (!(retsts & 1)) {
    /* just hand off status values likely to be due to user error */
    if (retsts == RMS$_FNF || retsts == RMS$_DNF || retsts == RMS$_PRV ||
        retsts == RMS$_DEV || retsts == RMS$_DIR || retsts == RMS$_SYN ||
       (retsts & STS$M_CODE) == (SHR$_NOWILD & STS$M_CODE)) return retsts;
    else { _ckvmssts(retsts); }
  }

  return (vmscmd->dsc$w_length > MAX_DCL_LINE_LENGTH ? CLI$_BUFOVF : retsts);

}  /* end of setup_cmddsc() */


/* {{{ bool vms_do_aexec(SV *really,SV **mark,SV **sp) */
bool
Perl_vms_do_aexec(pTHX_ SV *really,SV **mark,SV **sp)
{
bool exec_sts;
char * cmd;

  if (sp > mark) {
    if (vfork_called) {           /* this follows a vfork - act Unixish */
      vfork_called--;
      if (vfork_called < 0) {
        Perl_warn(aTHX_ "Internal inconsistency in tracking vforks");
        vfork_called = 0;
      }
      else return do_aexec(really,mark,sp);
    }
                                           /* no vfork - act VMSish */
    cmd = setup_argstr(aTHX_ really,mark,sp);
    exec_sts = vms_do_exec(cmd);
    Safefree(cmd);  /* Clean up from setup_argstr() */
    return exec_sts;
  }

  return FALSE;
}  /* end of vms_do_aexec() */
/*}}}*/

/* {{{bool vms_do_exec(char *cmd) */
bool
Perl_vms_do_exec(pTHX_ const char *cmd)
{
  struct dsc$descriptor_s *vmscmd;

  if (vfork_called) {             /* this follows a vfork - act Unixish */
    vfork_called--;
    if (vfork_called < 0) {
      Perl_warn(aTHX_ "Internal inconsistency in tracking vforks");
      vfork_called = 0;
    }
    else return do_exec(cmd);
  }

  {                               /* no vfork - act VMSish */
    unsigned long int retsts;

    if ((retsts = setup_cmddsc(aTHX_ cmd,1,0,&vmscmd)) & 1)
      retsts = lib$do_command(vmscmd);

    switch (retsts) {
      case RMS$_FNF: case RMS$_DNF:
        set_errno(ENOENT); break;
      case RMS$_DIR:
        set_errno(ENOTDIR); break;
      case RMS$_DEV:
        set_errno(ENODEV); break;
      case RMS$_PRV:
        set_errno(EACCES); break;
      case RMS$_SYN:
        set_errno(EINVAL); break;
      case CLI$_BUFOVF: case RMS$_RTB: case CLI$_TKNOVF: case CLI$_RSLOVF:
        set_errno(E2BIG); break;
      case LIB$_INVARG: case LIB$_INVSTRDES: case SS$_ACCVIO: /* shouldn't happen */
        _ckvmssts(retsts); /* fall through */
      default:  /* SS$_DUPLNAM, SS$_CLI, resource exhaustion, etc. */
        set_errno(EVMSERR); 
    }
    set_vaxc_errno(retsts);
    if (ckWARN(WARN_EXEC)) {
      Perl_warner(aTHX_ packWARN(WARN_EXEC),"Can't exec \"%*s\": %s",
             vmscmd->dsc$w_length, vmscmd->dsc$a_pointer, Strerror(errno));
    }
    vms_execfree(vmscmd);
  }

  return FALSE;

}  /* end of vms_do_exec() */
/*}}}*/

int do_spawn2(pTHX_ const char *, int);

int
Perl_do_aspawn(pTHX_ SV* really, SV** mark, SV** sp)
{
unsigned long int sts;
char * cmd;
int flags = 0;

  if (sp > mark) {

    /* We'll copy the (undocumented?) Win32 behavior and allow a 
     * numeric first argument.  But the only value we'll support
     * through do_aspawn is a value of 1, which means spawn without
     * waiting for completion -- other values are ignored.
     */
    if (SvNIOKp(*(mark+1)) && !SvPOKp(*(mark+1))) {
	++mark;
	flags = SvIV(*mark);
    }

    if (flags && flags == 1)     /* the Win32 P_NOWAIT value */
        flags = CLI$M_NOWAIT;
    else
        flags = 0;

    cmd = setup_argstr(aTHX_ really, mark, sp);
    sts = do_spawn2(aTHX_ cmd, flags);
    /* pp_sys will clean up cmd */
    return sts;
  }
  return SS$_ABORT;
}  /* end of do_aspawn() */
/*}}}*/


/* {{{int do_spawn(char* cmd) */
int
Perl_do_spawn(pTHX_ char* cmd)
{
    PERL_ARGS_ASSERT_DO_SPAWN;

    return do_spawn2(aTHX_ cmd, 0);
}
/*}}}*/

/* {{{int do_spawn_nowait(char* cmd) */
int
Perl_do_spawn_nowait(pTHX_ char* cmd)
{
    PERL_ARGS_ASSERT_DO_SPAWN_NOWAIT;

    return do_spawn2(aTHX_ cmd, CLI$M_NOWAIT);
}
/*}}}*/

/* {{{int do_spawn2(char *cmd) */
int
do_spawn2(pTHX_ const char *cmd, int flags)
{
  unsigned long int sts, substs;

  /* The caller of this routine expects to Safefree(PL_Cmd) */
  Newx(PL_Cmd,10,char);

  if (!cmd || !*cmd) {
    sts = lib$spawn(0,0,0,&flags,0,0,&substs,0,0,0,0,0,0);
    if (!(sts & 1)) {
      switch (sts) {
        case RMS$_FNF:  case RMS$_DNF:
          set_errno(ENOENT); break;
        case RMS$_DIR:
          set_errno(ENOTDIR); break;
        case RMS$_DEV:
          set_errno(ENODEV); break;
        case RMS$_PRV:
          set_errno(EACCES); break;
        case RMS$_SYN:
          set_errno(EINVAL); break;
        case CLI$_BUFOVF: case RMS$_RTB: case CLI$_TKNOVF: case CLI$_RSLOVF:
          set_errno(E2BIG); break;
        case LIB$_INVARG: case LIB$_INVSTRDES: case SS$_ACCVIO: /* shouldn't happen */
          _ckvmssts(sts); /* fall through */
        default:  /* SS$_DUPLNAM, SS$_CLI, resource exhaustion, etc. */
          set_errno(EVMSERR);
      }
      set_vaxc_errno(sts);
      if (ckWARN(WARN_EXEC)) {
        Perl_warner(aTHX_ packWARN(WARN_EXEC),"Can't spawn: %s",
		    Strerror(errno));
      }
    }
    sts = substs;
  }
  else {
    char mode[3];
    PerlIO * fp;
    if (flags & CLI$M_NOWAIT)
        strcpy(mode, "n");
    else
        strcpy(mode, "nW");
    
    fp = safe_popen(aTHX_ cmd, mode, (int *)&sts);
    if (fp != NULL)
      my_pclose(fp);
    /* sts will be the pid in the nowait case */
  }
  return sts;
}  /* end of do_spawn2() */
/*}}}*/


static unsigned int *sockflags, sockflagsize;

/*
 * Shim fdopen to identify sockets for my_fwrite later, since the stdio
 * routines found in some versions of the CRTL can't deal with sockets.
 * We don't shim the other file open routines since a socket isn't
 * likely to be opened by a name.
 */
/*{{{ FILE *my_fdopen(int fd, const char *mode)*/
FILE *my_fdopen(int fd, const char *mode)
{
  FILE *fp = fdopen(fd, mode);

  if (fp) {
    unsigned int fdoff = fd / sizeof(unsigned int);
    Stat_t sbuf; /* native stat; we don't need flex_stat */
    if (!sockflagsize || fdoff > sockflagsize) {
      if (sockflags) Renew(     sockflags,fdoff+2,unsigned int);
      else           Newx  (sockflags,fdoff+2,unsigned int);
      memset(sockflags+sockflagsize,0,fdoff + 2 - sockflagsize);
      sockflagsize = fdoff + 2;
    }
    if (fstat(fd, (struct stat *)&sbuf) == 0 && S_ISSOCK(sbuf.st_mode))
      sockflags[fdoff] |= 1 << (fd % sizeof(unsigned int));
  }
  return fp;

}
/*}}}*/


/*
 * Clear the corresponding bit when the (possibly) socket stream is closed.
 * There still a small hole: we miss an implicit close which might occur
 * via freopen().  >> Todo
 */
/*{{{ int my_fclose(FILE *fp)*/
int my_fclose(FILE *fp) {
  if (fp) {
    unsigned int fd = fileno(fp);
    unsigned int fdoff = fd / sizeof(unsigned int);

    if (sockflagsize && fdoff <= sockflagsize)
      sockflags[fdoff] &= ~(1 << fd % sizeof(unsigned int));
  }
  return fclose(fp);
}
/*}}}*/


/* 
 * A simple fwrite replacement which outputs itmsz*nitm chars without
 * introducing record boundaries every itmsz chars.
 * We are using fputs, which depends on a terminating null.  We may
 * well be writing binary data, so we need to accommodate not only
 * data with nulls sprinkled in the middle but also data with no null 
 * byte at the end.
 */
/*{{{ int my_fwrite(const void *src, size_t itmsz, size_t nitm, FILE *dest)*/
int
my_fwrite(const void *src, size_t itmsz, size_t nitm, FILE *dest)
{
  register char *cp, *end, *cpd, *data;
  register unsigned int fd = fileno(dest);
  register unsigned int fdoff = fd / sizeof(unsigned int);
  int retval;
  int bufsize = itmsz * nitm + 1;

  if (fdoff < sockflagsize &&
      (sockflags[fdoff] | 1 << (fd % sizeof(unsigned int)))) {
    if (write(fd, src, itmsz * nitm) == EOF) return EOF;
    return nitm;
  }

  _ckvmssts_noperl(lib$get_vm(&bufsize, &data));
  memcpy( data, src, itmsz*nitm );
  data[itmsz*nitm] = '\0';

  end = data + itmsz * nitm;
  retval = (int) nitm; /* on success return # items written */

  cpd = data;
  while (cpd <= end) {
    for (cp = cpd; cp <= end; cp++) if (!*cp) break;
    if (fputs(cpd,dest) == EOF) { retval = EOF; break; }
    if (cp < end)
      if (fputc('\0',dest) == EOF) { retval = EOF; break; }
    cpd = cp + 1;
  }

  if (data) _ckvmssts_noperl(lib$free_vm(&bufsize, &data));
  return retval;

}  /* end of my_fwrite() */
/*}}}*/

/*{{{ int my_flush(FILE *fp)*/
int
Perl_my_flush(pTHX_ FILE *fp)
{
    int res;
    if ((res = fflush(fp)) == 0 && fp) {
#ifdef VMS_DO_SOCKETS
	Stat_t s;
	if (fstat(fileno(fp), (stat_t *)&s) == 0 && !S_ISSOCK(s.st_mode))
#endif
	    res = fsync(fileno(fp));
    }
/*
 * If the flush succeeded but set end-of-file, we need to clear
 * the error because our caller may check ferror().  BTW, this 
 * probably means we just flushed an empty file.
 */
    if (res == 0 && vaxc$errno == RMS$_EOF) clearerr(fp);

    return res;
}
/*}}}*/

/*
 * Here are replacements for the following Unix routines in the VMS environment:
 *      getpwuid    Get information for a particular UIC or UID
 *      getpwnam    Get information for a named user
 *      getpwent    Get information for each user in the rights database
 *      setpwent    Reset search to the start of the rights database
 *      endpwent    Finish searching for users in the rights database
 *
 * getpwuid, getpwnam, and getpwent return a pointer to the passwd structure
 * (defined in pwd.h), which contains the following fields:-
 *      struct passwd {
 *              char        *pw_name;    Username (in lower case)
 *              char        *pw_passwd;  Hashed password
 *              unsigned int pw_uid;     UIC
 *              unsigned int pw_gid;     UIC group  number
 *              char        *pw_unixdir; Default device/directory (VMS-style)
 *              char        *pw_gecos;   Owner name
 *              char        *pw_dir;     Default device/directory (Unix-style)
 *              char        *pw_shell;   Default CLI name (eg. DCL)
 *      };
 * If the specified user does not exist, getpwuid and getpwnam return NULL.
 *
 * pw_uid is the full UIC (eg. what's returned by stat() in st_uid).
 * not the UIC member number (eg. what's returned by getuid()),
 * getpwuid() can accept either as input (if uid is specified, the caller's
 * UIC group is used), though it won't recognise gid=0.
 *
 * Note that in VMS it is necessary to have GRPPRV or SYSPRV to return
 * information about other users in your group or in other groups, respectively.
 * If the required privilege is not available, then these routines fill only
 * the pw_name, pw_uid, and pw_gid fields (the others point to an empty
 * string).
 *
 * By Tim Adye (T.J.Adye@rl.ac.uk), 10th February 1995.
 */

/* sizes of various UAF record fields */
#define UAI$S_USERNAME 12
#define UAI$S_IDENT    31
#define UAI$S_OWNER    31
#define UAI$S_DEFDEV   31
#define UAI$S_DEFDIR   63
#define UAI$S_DEFCLI   31
#define UAI$S_PWD       8

#define valid_uic(uic) ((uic).uic$v_format == UIC$K_UIC_FORMAT  && \
                        (uic).uic$v_member != UIC$K_WILD_MEMBER && \
                        (uic).uic$v_group  != UIC$K_WILD_GROUP)

static char __empty[]= "";
static struct passwd __passwd_empty=
    {(char *) __empty, (char *) __empty, 0, 0,
     (char *) __empty, (char *) __empty, (char *) __empty, (char *) __empty};
static int contxt= 0;
static struct passwd __pwdcache;
static char __pw_namecache[UAI$S_IDENT+1];

/*
 * This routine does most of the work extracting the user information.
 */
static int fillpasswd (pTHX_ const char *name, struct passwd *pwd)
{
    static struct {
        unsigned char length;
        char pw_gecos[UAI$S_OWNER+1];
    } owner;
    static union uicdef uic;
    static struct {
        unsigned char length;
        char pw_dir[UAI$S_DEFDEV+UAI$S_DEFDIR+1];
    } defdev;
    static struct {
        unsigned char length;
        char unixdir[UAI$_DEFDEV+UAI$S_DEFDIR+1];
    } defdir;
    static struct {
        unsigned char length;
        char pw_shell[UAI$S_DEFCLI+1];
    } defcli;
    static char pw_passwd[UAI$S_PWD+1];

    static unsigned short lowner, luic, ldefdev, ldefdir, ldefcli, lpwd;
    struct dsc$descriptor_s name_desc;
    unsigned long int sts;

    static struct itmlst_3 itmlst[]= {
        {UAI$S_OWNER+1,    UAI$_OWNER,  &owner,    &lowner},
        {sizeof(uic),      UAI$_UIC,    &uic,      &luic},
        {UAI$S_DEFDEV+1,   UAI$_DEFDEV, &defdev,   &ldefdev},
        {UAI$S_DEFDIR+1,   UAI$_DEFDIR, &defdir,   &ldefdir},
        {UAI$S_DEFCLI+1,   UAI$_DEFCLI, &defcli,   &ldefcli},
        {UAI$S_PWD,        UAI$_PWD,    pw_passwd, &lpwd},
        {0,                0,           NULL,    NULL}};

    name_desc.dsc$w_length=  strlen(name);
    name_desc.dsc$b_dtype=   DSC$K_DTYPE_T;
    name_desc.dsc$b_class=   DSC$K_CLASS_S;
    name_desc.dsc$a_pointer= (char *) name; /* read only pointer */

/*  Note that sys$getuai returns many fields as counted strings. */
    sts= sys$getuai(0, 0, &name_desc, &itmlst, 0, 0, 0);
    if (sts == SS$_NOSYSPRV || sts == SS$_NOGRPPRV || sts == RMS$_RNF) {
      set_vaxc_errno(sts); set_errno(sts == RMS$_RNF ? EINVAL : EACCES);
    }
    else { _ckvmssts(sts); }
    if (!(sts & 1)) return 0;  /* out here in case _ckvmssts() doesn't abort */

    if ((int) owner.length  < lowner)  lowner=  (int) owner.length;
    if ((int) defdev.length < ldefdev) ldefdev= (int) defdev.length;
    if ((int) defdir.length < ldefdir) ldefdir= (int) defdir.length;
    if ((int) defcli.length < ldefcli) ldefcli= (int) defcli.length;
    memcpy(&defdev.pw_dir[ldefdev], &defdir.unixdir[0], ldefdir);
    owner.pw_gecos[lowner]=            '\0';
    defdev.pw_dir[ldefdev+ldefdir]= '\0';
    defcli.pw_shell[ldefcli]=          '\0';
    if (valid_uic(uic)) {
        pwd->pw_uid= uic.uic$l_uic;
        pwd->pw_gid= uic.uic$v_group;
    }
    else
      Perl_warn(aTHX_ "getpwnam returned invalid UIC %#o for user \"%s\"");
    pwd->pw_passwd=  pw_passwd;
    pwd->pw_gecos=   owner.pw_gecos;
    pwd->pw_dir=     defdev.pw_dir;
    pwd->pw_unixdir= do_tounixpath(defdev.pw_dir, defdir.unixdir,1,NULL);
    pwd->pw_shell=   defcli.pw_shell;
    if (pwd->pw_unixdir && pwd->pw_unixdir[0]) {
        int ldir;
        ldir= strlen(pwd->pw_unixdir) - 1;
        if (pwd->pw_unixdir[ldir]=='/') pwd->pw_unixdir[ldir]= '\0';
    }
    else
        strcpy(pwd->pw_unixdir, pwd->pw_dir);
    if (!decc_efs_case_preserve)
        __mystrtolower(pwd->pw_unixdir);
    return 1;
}

/*
 * Get information for a named user.
*/
/*{{{struct passwd *getpwnam(char *name)*/
struct passwd *Perl_my_getpwnam(pTHX_ const char *name)
{
    struct dsc$descriptor_s name_desc;
    union uicdef uic;
    unsigned long int status, sts;
                                  
    __pwdcache = __passwd_empty;
    if (!fillpasswd(aTHX_ name, &__pwdcache)) {
      /* We still may be able to determine pw_uid and pw_gid */
      name_desc.dsc$w_length=  strlen(name);
      name_desc.dsc$b_dtype=   DSC$K_DTYPE_T;
      name_desc.dsc$b_class=   DSC$K_CLASS_S;
      name_desc.dsc$a_pointer= (char *) name;
      if ((sts = sys$asctoid(&name_desc, &uic, 0)) == SS$_NORMAL) {
        __pwdcache.pw_uid= uic.uic$l_uic;
        __pwdcache.pw_gid= uic.uic$v_group;
      }
      else {
        if (sts == SS$_NOSUCHID || sts == SS$_IVIDENT || sts == RMS$_PRV) {
          set_vaxc_errno(sts);
          set_errno(sts == RMS$_PRV ? EACCES : EINVAL);
          return NULL;
        }
        else { _ckvmssts(sts); }
      }
    }
    strncpy(__pw_namecache, name, sizeof(__pw_namecache));
    __pw_namecache[sizeof __pw_namecache - 1] = '\0';
    __pwdcache.pw_name= __pw_namecache;
    return &__pwdcache;
}  /* end of my_getpwnam() */
/*}}}*/

/*
 * Get information for a particular UIC or UID.
 * Called by my_getpwent with uid=-1 to list all users.
*/
/*{{{struct passwd *my_getpwuid(Uid_t uid)*/
struct passwd *Perl_my_getpwuid(pTHX_ Uid_t uid)
{
    const $DESCRIPTOR(name_desc,__pw_namecache);
    unsigned short lname;
    union uicdef uic;
    unsigned long int status;

    if (uid == (unsigned int) -1) {
      do {
        status = sys$idtoasc(-1, &lname, &name_desc, &uic, 0, &contxt);
        if (status == SS$_NOSUCHID || status == RMS$_PRV) {
          set_vaxc_errno(status);
          set_errno(status == RMS$_PRV ? EACCES : EINVAL);
          my_endpwent();
          return NULL;
        }
        else { _ckvmssts(status); }
      } while (!valid_uic (uic));
    }
    else {
      uic.uic$l_uic= uid;
      if (!uic.uic$v_group)
        uic.uic$v_group= PerlProc_getgid();
      if (valid_uic(uic))
        status = sys$idtoasc(uic.uic$l_uic, &lname, &name_desc, 0, 0, 0);
      else status = SS$_IVIDENT;
      if (status == SS$_IVIDENT || status == SS$_NOSUCHID ||
          status == RMS$_PRV) {
        set_vaxc_errno(status); set_errno(status == RMS$_PRV ? EACCES : EINVAL);
        return NULL;
      }
      else { _ckvmssts(status); }
    }
    __pw_namecache[lname]= '\0';
    __mystrtolower(__pw_namecache);

    __pwdcache = __passwd_empty;
    __pwdcache.pw_name = __pw_namecache;

/*  Fill in the uid and gid in case fillpasswd can't (eg. no privilege).
    The identifier's value is usually the UIC, but it doesn't have to be,
    so if we can, we let fillpasswd update this. */
    __pwdcache.pw_uid =  uic.uic$l_uic;
    __pwdcache.pw_gid =  uic.uic$v_group;

    fillpasswd(aTHX_ __pw_namecache, &__pwdcache);
    return &__pwdcache;

}  /* end of my_getpwuid() */
/*}}}*/

/*
 * Get information for next user.
*/
/*{{{struct passwd *my_getpwent()*/
struct passwd *Perl_my_getpwent(pTHX)
{
    return (my_getpwuid((unsigned int) -1));
}
/*}}}*/

/*
 * Finish searching rights database for users.
*/
/*{{{void my_endpwent()*/
void Perl_my_endpwent(pTHX)
{
    if (contxt) {
      _ckvmssts(sys$finish_rdb(&contxt));
      contxt= 0;
    }
}
/*}}}*/

#ifdef HOMEGROWN_POSIX_SIGNALS
  /* Signal handling routines, pulled into the core from POSIX.xs.
   *
   * We need these for threads, so they've been rolled into the core,
   * rather than left in POSIX.xs.
   *
   * (DRS, Oct 23, 1997)
   */

  /* sigset_t is atomic under VMS, so these routines are easy */
/*{{{int my_sigemptyset(sigset_t *) */
int my_sigemptyset(sigset_t *set) {
    if (!set) { SETERRNO(EFAULT,SS$_ACCVIO); return -1; }
    *set = 0; return 0;
}
/*}}}*/


/*{{{int my_sigfillset(sigset_t *)*/
int my_sigfillset(sigset_t *set) {
    int i;
    if (!set) { SETERRNO(EFAULT,SS$_ACCVIO); return -1; }
    for (i = 0; i < NSIG; i++) *set |= (1 << i);
    return 0;
}
/*}}}*/


/*{{{int my_sigaddset(sigset_t *set, int sig)*/
int my_sigaddset(sigset_t *set, int sig) {
    if (!set) { SETERRNO(EFAULT,SS$_ACCVIO); return -1; }
    if (sig > NSIG) { SETERRNO(EINVAL,LIB$_INVARG); return -1; }
    *set |= (1 << (sig - 1));
    return 0;
}
/*}}}*/


/*{{{int my_sigdelset(sigset_t *set, int sig)*/
int my_sigdelset(sigset_t *set, int sig) {
    if (!set) { SETERRNO(EFAULT,SS$_ACCVIO); return -1; }
    if (sig > NSIG) { SETERRNO(EINVAL,LIB$_INVARG); return -1; }
    *set &= ~(1 << (sig - 1));
    return 0;
}
/*}}}*/


/*{{{int my_sigismember(sigset_t *set, int sig)*/
int my_sigismember(sigset_t *set, int sig) {
    if (!set) { SETERRNO(EFAULT,SS$_ACCVIO); return -1; }
    if (sig > NSIG) { SETERRNO(EINVAL,LIB$_INVARG); return -1; }
    return *set & (1 << (sig - 1));
}
/*}}}*/


/*{{{int my_sigprocmask(int how, sigset_t *set, sigset_t *oset)*/
int my_sigprocmask(int how, sigset_t *set, sigset_t *oset) {
    sigset_t tempmask;

    /* If set and oset are both null, then things are badly wrong. Bail out. */
    if ((oset == NULL) && (set == NULL)) {
      set_errno(EFAULT); set_vaxc_errno(SS$_ACCVIO);
      return -1;
    }

    /* If set's null, then we're just handling a fetch. */
    if (set == NULL) {
        tempmask = sigblock(0);
    }
    else {
      switch (how) {
      case SIG_SETMASK:
        tempmask = sigsetmask(*set);
        break;
      case SIG_BLOCK:
        tempmask = sigblock(*set);
        break;
      case SIG_UNBLOCK:
        tempmask = sigblock(0);
        sigsetmask(*oset & ~tempmask);
        break;
      default:
        set_errno(EINVAL); set_vaxc_errno(LIB$_INVARG);
        return -1;
      }
    }

    /* Did they pass us an oset? If so, stick our holding mask into it */
    if (oset)
      *oset = tempmask;
  
    return 0;
}
/*}}}*/
#endif  /* HOMEGROWN_POSIX_SIGNALS */


/* Used for UTC calculation in my_gmtime(), my_localtime(), my_time(),
 * my_utime(), and flex_stat(), all of which operate on UTC unless
 * VMSISH_TIMES is true.
 */
/* method used to handle UTC conversions:
 *   1 == CRTL gmtime();  2 == SYS$TIMEZONE_DIFFERENTIAL;  3 == no correction
 */
static int gmtime_emulation_type;
/* number of secs to add to UTC POSIX-style time to get local time */
static long int utc_offset_secs;

/* We #defined 'gmtime', 'localtime', and 'time' as 'my_gmtime' etc.
 * in vmsish.h.  #undef them here so we can call the CRTL routines
 * directly.
 */
#undef gmtime
#undef localtime
#undef time


/*
 * DEC C previous to 6.0 corrupts the behavior of the /prefix
 * qualifier with the extern prefix pragma.  This provisional
 * hack circumvents this prefix pragma problem in previous 
 * precompilers.
 */
#if defined(__VMS_VER) && __VMS_VER >= 70000000 
#  if defined(VMS_WE_ARE_CASE_SENSITIVE) && (__DECC_VER < 60000000)
#    pragma __extern_prefix save
#    pragma __extern_prefix ""  /* set to empty to prevent prefixing */
#    define gmtime decc$__utctz_gmtime
#    define localtime decc$__utctz_localtime
#    define time decc$__utc_time
#    pragma __extern_prefix restore

     struct tm *gmtime(), *localtime();   

#  endif
#endif


static time_t toutc_dst(time_t loc) {
  struct tm *rsltmp;

  if ((rsltmp = localtime(&loc)) == NULL) return -1;
  loc -= utc_offset_secs;
  if (rsltmp->tm_isdst) loc -= 3600;
  return loc;
}
#define _toutc(secs)  ((secs) == (time_t) -1 ? (time_t) -1 : \
       ((gmtime_emulation_type || my_time(NULL)), \
       (gmtime_emulation_type == 1 ? toutc_dst(secs) : \
       ((secs) - utc_offset_secs))))

static time_t toloc_dst(time_t utc) {
  struct tm *rsltmp;

  utc += utc_offset_secs;
  if ((rsltmp = localtime(&utc)) == NULL) return -1;
  if (rsltmp->tm_isdst) utc += 3600;
  return utc;
}
#define _toloc(secs)  ((secs) == (time_t) -1 ? (time_t) -1 : \
       ((gmtime_emulation_type || my_time(NULL)), \
       (gmtime_emulation_type == 1 ? toloc_dst(secs) : \
       ((secs) + utc_offset_secs))))

#ifndef RTL_USES_UTC
/*
  
    ucx$tz = "EST5EDT4,M4.1.0,M10.5.0"  typical 
        DST starts on 1st sun of april      at 02:00  std time
            ends on last sun of october     at 02:00  dst time
    see the UCX management command reference, SET CONFIG TIMEZONE
    for formatting info.

    No, it's not as general as it should be, but then again, NOTHING
    will handle UK times in a sensible way. 
*/


/* 
    parse the DST start/end info:
    (Jddd|ddd|Mmon.nth.dow)[/hh:mm:ss]
*/

static char *
tz_parse_startend(char *s, struct tm *w, int *past)
{
    int dinm[] = {31,28,31,30,31,30,31,31,30,31,30,31};
    int ly, dozjd, d, m, n, hour, min, sec, j, k;
    time_t g;

    if (!s)    return 0;
    if (!w) return 0;
    if (!past) return 0;

    ly = 0;
    if (w->tm_year % 4        == 0) ly = 1;
    if (w->tm_year % 100      == 0) ly = 0;
    if (w->tm_year+1900 % 400 == 0) ly = 1;
    if (ly) dinm[1]++;

    dozjd = isdigit(*s);
    if (*s == 'J' || *s == 'j' || dozjd) {
        if (!dozjd && !isdigit(*++s)) return 0;
        d = *s++ - '0';
        if (isdigit(*s)) {
            d = d*10 + *s++ - '0';
            if (isdigit(*s)) {
                d = d*10 + *s++ - '0';
            }
        }
        if (d == 0) return 0;
        if (d > 366) return 0;
        d--;
        if (!dozjd && d > 58 && ly) d++;  /* after 28 feb */
        g = d * 86400;
        dozjd = 1;
    } else if (*s == 'M' || *s == 'm') {
        if (!isdigit(*++s)) return 0;
        m = *s++ - '0';
        if (isdigit(*s)) m = 10*m + *s++ - '0';
        if (*s != '.') return 0;
        if (!isdigit(*++s)) return 0;
        n = *s++ - '0';
        if (n < 1 || n > 5) return 0;
        if (*s != '.') return 0;
        if (!isdigit(*++s)) return 0;
        d = *s++ - '0';
        if (d > 6) return 0;
    }

    if (*s == '/') {
        if (!isdigit(*++s)) return 0;
        hour = *s++ - '0';
        if (isdigit(*s)) hour = 10*hour + *s++ - '0';
        if (*s == ':') {
            if (!isdigit(*++s)) return 0;
            min = *s++ - '0';
            if (isdigit(*s)) min = 10*min + *s++ - '0';
            if (*s == ':') {
                if (!isdigit(*++s)) return 0;
                sec = *s++ - '0';
                if (isdigit(*s)) sec = 10*sec + *s++ - '0';
            }
        }
    } else {
        hour = 2;
        min = 0;
        sec = 0;
    }

    if (dozjd) {
        if (w->tm_yday < d) goto before;
        if (w->tm_yday > d) goto after;
    } else {
        if (w->tm_mon+1 < m) goto before;
        if (w->tm_mon+1 > m) goto after;

        j = (42 + w->tm_wday - w->tm_mday)%7;   /*dow of mday 0 */
        k = d - j; /* mday of first d */
        if (k <= 0) k += 7;
        k += 7 * ((n>4?4:n)-1);  /* mday of n'th d */
        if (n == 5 && k+7 <= dinm[w->tm_mon]) k += 7;
        if (w->tm_mday < k) goto before;
        if (w->tm_mday > k) goto after;
    }

    if (w->tm_hour < hour) goto before;
    if (w->tm_hour > hour) goto after;
    if (w->tm_min  < min)  goto before;
    if (w->tm_min  > min)  goto after;
    if (w->tm_sec  < sec)  goto before;
    goto after;

before:
    *past = 0;
    return s;
after:
    *past = 1;
    return s;
}




/*  parse the offset:   (+|-)hh[:mm[:ss]]  */

static char *
tz_parse_offset(char *s, int *offset)
{
    int hour = 0, min = 0, sec = 0;
    int neg = 0;
    if (!s) return 0;
    if (!offset) return 0;

    if (*s == '-') {neg++; s++;}
    if (*s == '+') s++;
    if (!isdigit(*s)) return 0;
    hour = *s++ - '0';
    if (isdigit(*s)) hour = hour*10+(*s++ - '0');
    if (hour > 24) return 0;
    if (*s == ':') {
        if (!isdigit(*++s)) return 0;
        min = *s++ - '0';
        if (isdigit(*s)) min = min*10 + (*s++ - '0');
        if (min > 59) return 0;
        if (*s == ':') {
            if (!isdigit(*++s)) return 0;
            sec = *s++ - '0';
            if (isdigit(*s)) sec = sec*10 + (*s++ - '0');
            if (sec > 59) return 0;
        }
    }

    *offset = (hour*60+min)*60 + sec;
    if (neg) *offset = -*offset;
    return s;
}

/*
    input time is w, whatever type of time the CRTL localtime() uses.
    sets dst, the zone, and the gmtoff (seconds)

    caches the value of TZ and UCX$TZ env variables; note that 
    my_setenv looks for these and sets a flag if they're changed
    for efficiency. 

    We have to watch out for the "australian" case (dst starts in
    october, ends in april)...flagged by "reverse" and checked by
    scanning through the months of the previous year.

*/

static int
tz_parse(pTHX_ time_t *w, int *dst, char *zone, int *gmtoff)
{
    time_t when;
    struct tm *w2;
    char *s,*s2;
    char *dstzone, *tz, *s_start, *s_end;
    int std_off, dst_off, isdst;
    int y, dststart, dstend;
    static char envtz[1025];  /* longer than any logical, symbol, ... */
    static char ucxtz[1025];
    static char reversed = 0;

    if (!w) return 0;

    if (tz_updated) {
        tz_updated = 0;
        reversed = -1;  /* flag need to check  */
        envtz[0] = ucxtz[0] = '\0';
        tz = my_getenv("TZ",0);
        if (tz) strcpy(envtz, tz);
        tz = my_getenv("UCX$TZ",0);
        if (tz) strcpy(ucxtz, tz);
        if (!envtz[0] && !ucxtz[0]) return 0;  /* we give up */
    }
    tz = envtz;
    if (!*tz) tz = ucxtz;

    s = tz;
    while (isalpha(*s)) s++;
    s = tz_parse_offset(s, &std_off);
    if (!s) return 0;
    if (!*s) {                  /* no DST, hurray we're done! */
        isdst = 0;
        goto done;
    }

    dstzone = s;
    while (isalpha(*s)) s++;
    s2 = tz_parse_offset(s, &dst_off);
    if (s2) {
        s = s2;
    } else {
        dst_off = std_off - 3600;
    }

    if (!*s) {      /* default dst start/end?? */
        if (tz != ucxtz) {          /* if TZ tells zone only, UCX$TZ tells rule */
            s = strchr(ucxtz,',');
        }
        if (!s || !*s) s = ",M4.1.0,M10.5.0";   /* we know we do dst, default rule */
    }
    if (*s != ',') return 0;

    when = *w;
    when = _toutc(when);      /* convert to utc */
    when = when - std_off;    /* convert to pseudolocal time*/

    w2 = localtime(&when);
    y = w2->tm_year;
    s_start = s+1;
    s = tz_parse_startend(s_start,w2,&dststart);
    if (!s) return 0;
    if (*s != ',') return 0;

    when = *w;
    when = _toutc(when);      /* convert to utc */
    when = when - dst_off;    /* convert to pseudolocal time*/
    w2 = localtime(&when);
    if (w2->tm_year != y) {   /* spans a year, just check one time */
        when += dst_off - std_off;
        w2 = localtime(&when);
    }
    s_end = s+1;
    s = tz_parse_startend(s_end,w2,&dstend);
    if (!s) return 0;

    if (reversed == -1) {  /* need to check if start later than end */
        int j, ds, de;

        when = *w;
        if (when < 2*365*86400) {
            when += 2*365*86400;
        } else {
            when -= 365*86400;
        }
        w2 =localtime(&when);
        when = when + (15 - w2->tm_yday) * 86400;   /* jan 15 */

        for (j = 0; j < 12; j++) {
            w2 =localtime(&when);
            tz_parse_startend(s_start,w2,&ds);
            tz_parse_startend(s_end,w2,&de);
            if (ds != de) break;
            when += 30*86400;
        }
        reversed = 0;
        if (de && !ds) reversed = 1;
    }

    isdst = dststart && !dstend;
    if (reversed) isdst = dststart  || !dstend;

done:
    if (dst)    *dst = isdst;
    if (gmtoff) *gmtoff = isdst ? dst_off : std_off;
    if (isdst)  tz = dstzone;
    if (zone) {
        while(isalpha(*tz))  *zone++ = *tz++;
        *zone = '\0';
    }
    return 1;
}

#endif /* !RTL_USES_UTC */

/* my_time(), my_localtime(), my_gmtime()
 * By default traffic in UTC time values, using CRTL gmtime() or
 * SYS$TIMEZONE_DIFFERENTIAL to determine offset from local time zone.
 * Note: We need to use these functions even when the CRTL has working
 * UTC support, since they also handle C<use vmsish qw(times);>
 *
 * Contributed by Chuck Lane  <lane@duphy4.physics.drexel.edu>
 * Modified by Charles Bailey <bailey@newman.upenn.edu>
 */

/*{{{time_t my_time(time_t *timep)*/
time_t Perl_my_time(pTHX_ time_t *timep)
{
  time_t when;
  struct tm *tm_p;

  if (gmtime_emulation_type == 0) {
    int dstnow;
    time_t base = 15 * 86400; /* 15jan71; to avoid month/year ends between    */
                              /* results of calls to gmtime() and localtime() */
                              /* for same &base */

    gmtime_emulation_type++;
    if ((tm_p = gmtime(&base)) == NULL) { /* CRTL gmtime() is a fake */
      char off[LNM$C_NAMLENGTH+1];;

      gmtime_emulation_type++;
      if (!vmstrnenv("SYS$TIMEZONE_DIFFERENTIAL",off,0,fildev,0)) {
        gmtime_emulation_type++;
        utc_offset_secs = 0;
        Perl_warn(aTHX_ "no UTC offset information; assuming local time is UTC");
      }
      else { utc_offset_secs = atol(off); }
    }
    else { /* We've got a working gmtime() */
      struct tm gmt, local;

      gmt = *tm_p;
      tm_p = localtime(&base);
      local = *tm_p;
      utc_offset_secs  = (local.tm_mday - gmt.tm_mday) * 86400;
      utc_offset_secs += (local.tm_hour - gmt.tm_hour) * 3600;
      utc_offset_secs += (local.tm_min  - gmt.tm_min)  * 60;
      utc_offset_secs += (local.tm_sec  - gmt.tm_sec);
    }
  }

  when = time(NULL);
# ifdef VMSISH_TIME
# ifdef RTL_USES_UTC
  if (VMSISH_TIME) when = _toloc(when);
# else
  if (!VMSISH_TIME) when = _toutc(when);
# endif
# endif
  if (timep != NULL) *timep = when;
  return when;

}  /* end of my_time() */
/*}}}*/


/*{{{struct tm *my_gmtime(const time_t *timep)*/
struct tm *
Perl_my_gmtime(pTHX_ const time_t *timep)
{
  char *p;
  time_t when;
  struct tm *rsltmp;

  if (timep == NULL) {
    set_errno(EINVAL); set_vaxc_errno(LIB$_INVARG);
    return NULL;
  }
  if (*timep == 0) gmtime_emulation_type = 0;  /* possibly reset TZ */

  when = *timep;
# ifdef VMSISH_TIME
  if (VMSISH_TIME) when = _toutc(when); /* Input was local time */
#  endif
# ifdef RTL_USES_UTC  /* this implies that the CRTL has a working gmtime() */
  return gmtime(&when);
# else
  /* CRTL localtime() wants local time as input, so does no tz correction */
  rsltmp = localtime(&when);
  if (rsltmp) rsltmp->tm_isdst = 0;  /* We already took DST into account */
  return rsltmp;
#endif
}  /* end of my_gmtime() */
/*}}}*/


/*{{{struct tm *my_localtime(const time_t *timep)*/
struct tm *
Perl_my_localtime(pTHX_ const time_t *timep)
{
  time_t when, whenutc;
  struct tm *rsltmp;
  int dst, offset;

  if (timep == NULL) {
    set_errno(EINVAL); set_vaxc_errno(LIB$_INVARG);
    return NULL;
  }
  if (*timep == 0) gmtime_emulation_type = 0;  /* possibly reset TZ */
  if (gmtime_emulation_type == 0) my_time(NULL); /* Init UTC */

  when = *timep;
# ifdef RTL_USES_UTC
# ifdef VMSISH_TIME
  if (VMSISH_TIME) when = _toutc(when);
# endif
  /* CRTL localtime() wants UTC as input, does tz correction itself */
  return localtime(&when);
  
# else /* !RTL_USES_UTC */
  whenutc = when;
# ifdef VMSISH_TIME
  if (!VMSISH_TIME) when = _toloc(whenutc);  /*  input was UTC */
  if (VMSISH_TIME) whenutc = _toutc(when);   /*  input was truelocal */
# endif
  dst = -1;
#ifndef RTL_USES_UTC
  if (tz_parse(aTHX_ &when, &dst, 0, &offset)) {   /* truelocal determines DST*/
      when = whenutc - offset;                   /* pseudolocal time*/
  }
# endif
  /* CRTL localtime() wants local time as input, so does no tz correction */
  rsltmp = localtime(&when);
  if (rsltmp && gmtime_emulation_type != 1) rsltmp->tm_isdst = dst;
  return rsltmp;
# endif

} /*  end of my_localtime() */
/*}}}*/

/* Reset definitions for later calls */
#define gmtime(t)    my_gmtime(t)
#define localtime(t) my_localtime(t)
#define time(t)      my_time(t)


/* my_utime - update modification/access time of a file
 *
 * VMS 7.3 and later implementation
 * Only the UTC translation is home-grown. The rest is handled by the
 * CRTL utime(), which will take into account the relevant feature
 * logicals and ODS-5 volume characteristics for true access times.
 *
 * pre VMS 7.3 implementation:
 * The calling sequence is identical to POSIX utime(), but under
 * VMS with ODS-2, only the modification time is changed; ODS-2 does
 * not maintain access times.  Restrictions differ from the POSIX
 * definition in that the time can be changed as long as the
 * caller has permission to execute the necessary IO$_MODIFY $QIO;
 * no separate checks are made to insure that the caller is the
 * owner of the file or has special privs enabled.
 * Code here is based on Joe Meadows' FILE utility.
 *
 */

/* Adjustment from Unix epoch (01-JAN-1970 00:00:00.00)
 *              to VMS epoch  (01-JAN-1858 00:00:00.00)
 * in 100 ns intervals.
 */
static const long int utime_baseadjust[2] = { 0x4beb4000, 0x7c9567 };

/*{{{int my_utime(const char *path, const struct utimbuf *utimes)*/
int Perl_my_utime(pTHX_ const char *file, const struct utimbuf *utimes)
{
#if __CRTL_VER >= 70300000
  struct utimbuf utc_utimes, *utc_utimesp;

  if (utimes != NULL) {
    utc_utimes.actime = utimes->actime;
    utc_utimes.modtime = utimes->modtime;
# ifdef VMSISH_TIME
    /* If input was local; convert to UTC for sys svc */
    if (VMSISH_TIME) {
      utc_utimes.actime = _toutc(utimes->actime);
      utc_utimes.modtime = _toutc(utimes->modtime);
    }
# endif
    utc_utimesp = &utc_utimes;
  }
  else {
    utc_utimesp = NULL;
  }

  return utime(file, utc_utimesp);

#else /* __CRTL_VER < 70300000 */

  register int i;
  int sts;
  long int bintime[2], len = 2, lowbit, unixtime,
           secscale = 10000000; /* seconds --> 100 ns intervals */
  unsigned long int chan, iosb[2], retsts;
  char vmsspec[NAM$C_MAXRSS+1], rsa[NAM$C_MAXRSS], esa[NAM$C_MAXRSS];
  struct FAB myfab = cc$rms_fab;
  struct NAM mynam = cc$rms_nam;
#if defined (__DECC) && defined (__VAX)
  /* VAX DEC C atrdef.h has unsigned type for pointer member atr$l_addr,
   * at least through VMS V6.1, which causes a type-conversion warning.
   */
#  pragma message save
#  pragma message disable cvtdiftypes
#endif
  struct atrdef myatr[2] = {{sizeof bintime, ATR$C_REVDATE, bintime}, {0,0,0}};
  struct fibdef myfib;
#if defined (__DECC) && defined (__VAX)
  /* This should be right after the declaration of myatr, but due
   * to a bug in VAX DEC C, this takes effect a statement early.
   */
#  pragma message restore
#endif
  /* cast ok for read only parameter */
  struct dsc$descriptor fibdsc = {sizeof(myfib), DSC$K_DTYPE_Z, DSC$K_CLASS_S,(char *) &myfib},
                        devdsc = {0,DSC$K_DTYPE_T, DSC$K_CLASS_S,0},
                        fnmdsc = {0,DSC$K_DTYPE_T, DSC$K_CLASS_S,0};
	
  if (file == NULL || *file == '\0') {
    SETERRNO(ENOENT, LIB$_INVARG);
    return -1;
  }

  /* Convert to VMS format ensuring that it will fit in 255 characters */
  if (do_rmsexpand(file, vmsspec, 0, NULL, PERL_RMSEXPAND_M_VMS, NULL, NULL) == NULL) {
      SETERRNO(ENOENT, LIB$_INVARG);
      return -1;
  }
  if (utimes != NULL) {
    /* Convert Unix time    (seconds since 01-JAN-1970 00:00:00.00)
     * to VMS quadword time (100 nsec intervals since 01-JAN-1858 00:00:00.00).
     * Since time_t is unsigned long int, and lib$emul takes a signed long int
     * as input, we force the sign bit to be clear by shifting unixtime right
     * one bit, then multiplying by an extra factor of 2 in lib$emul().
     */
    lowbit = (utimes->modtime & 1) ? secscale : 0;
    unixtime = (long int) utimes->modtime;
#   ifdef VMSISH_TIME
    /* If input was UTC; convert to local for sys svc */
    if (!VMSISH_TIME) unixtime = _toloc(unixtime);
#   endif
    unixtime >>= 1;  secscale <<= 1;
    retsts = lib$emul(&secscale, &unixtime, &lowbit, bintime);
    if (!(retsts & 1)) {
      SETERRNO(EVMSERR, retsts);
      return -1;
    }
    retsts = lib$addx(bintime,utime_baseadjust,bintime,&len);
    if (!(retsts & 1)) {
      SETERRNO(EVMSERR, retsts);
      return -1;
    }
  }
  else {
    /* Just get the current time in VMS format directly */
    retsts = sys$gettim(bintime);
    if (!(retsts & 1)) {
      SETERRNO(EVMSERR, retsts);
      return -1;
    }
  }

  myfab.fab$l_fna = vmsspec;
  myfab.fab$b_fns = (unsigned char) strlen(vmsspec);
  myfab.fab$l_nam = &mynam;
  mynam.nam$l_esa = esa;
  mynam.nam$b_ess = (unsigned char) sizeof esa;
  mynam.nam$l_rsa = rsa;
  mynam.nam$b_rss = (unsigned char) sizeof rsa;
  if (decc_efs_case_preserve)
      mynam.nam$b_nop |= NAM$M_NO_SHORT_UPCASE;

  /* Look for the file to be affected, letting RMS parse the file
   * specification for us as well.  I have set errno using only
   * values documented in the utime() man page for VMS POSIX.
   */
  retsts = sys$parse(&myfab,0,0);
  if (!(retsts & 1)) {
    set_vaxc_errno(retsts);
    if      (retsts == RMS$_PRV) set_errno(EACCES);
    else if (retsts == RMS$_DIR) set_errno(ENOTDIR);
    else                         set_errno(EVMSERR);
    return -1;
  }
  retsts = sys$search(&myfab,0,0);
  if (!(retsts & 1)) {
    mynam.nam$b_nop |= NAM$M_SYNCHK;  mynam.nam$l_rlf = NULL;
    myfab.fab$b_dns = 0;  sts = sys$parse(&myfab,0,0);
    set_vaxc_errno(retsts);
    if      (retsts == RMS$_PRV) set_errno(EACCES);
    else if (retsts == RMS$_FNF) set_errno(ENOENT);
    else                         set_errno(EVMSERR);
    return -1;
  }

  devdsc.dsc$w_length = mynam.nam$b_dev;
  /* cast ok for read only parameter */
  devdsc.dsc$a_pointer = (char *) mynam.nam$l_dev;

  retsts = sys$assign(&devdsc,&chan,0,0);
  if (!(retsts & 1)) {
    mynam.nam$b_nop |= NAM$M_SYNCHK;  mynam.nam$l_rlf = NULL;
    myfab.fab$b_dns = 0;  sts = sys$parse(&myfab,0,0);
    set_vaxc_errno(retsts);
    if      (retsts == SS$_IVDEVNAM)   set_errno(ENOTDIR);
    else if (retsts == SS$_NOPRIV)     set_errno(EACCES);
    else if (retsts == SS$_NOSUCHDEV)  set_errno(ENOTDIR);
    else                               set_errno(EVMSERR);
    return -1;
  }

  fnmdsc.dsc$a_pointer = mynam.nam$l_name;
  fnmdsc.dsc$w_length = mynam.nam$b_name + mynam.nam$b_type + mynam.nam$b_ver;

  memset((void *) &myfib, 0, sizeof myfib);
#if defined(__DECC) || defined(__DECCXX)
  for (i=0;i<3;i++) myfib.fib$w_fid[i] = mynam.nam$w_fid[i];
  for (i=0;i<3;i++) myfib.fib$w_did[i] = mynam.nam$w_did[i];
  /* This prevents the revision time of the file being reset to the current
   * time as a result of our IO$_MODIFY $QIO. */
  myfib.fib$l_acctl = FIB$M_NORECORD;
#else
  for (i=0;i<3;i++) myfib.fib$r_fid_overlay.fib$w_fid[i] = mynam.nam$w_fid[i];
  for (i=0;i<3;i++) myfib.fib$r_did_overlay.fib$w_did[i] = mynam.nam$w_did[i];
  myfib.fib$r_acctl_overlay.fib$l_acctl = FIB$M_NORECORD;
#endif
  retsts = sys$qiow(0,chan,IO$_MODIFY,iosb,0,0,&fibdsc,&fnmdsc,0,0,myatr,0);
  mynam.nam$b_nop |= NAM$M_SYNCHK;  mynam.nam$l_rlf = NULL;
  myfab.fab$b_dns = 0;  sts = sys$parse(&myfab,0,0);
  _ckvmssts(sys$dassgn(chan));
  if (retsts & 1) retsts = iosb[0];
  if (!(retsts & 1)) {
    set_vaxc_errno(retsts);
    if (retsts == SS$_NOPRIV) set_errno(EACCES);
    else                      set_errno(EVMSERR);
    return -1;
  }

  return 0;

#endif /* #if __CRTL_VER >= 70300000 */

}  /* end of my_utime() */
/*}}}*/

/*
 * flex_stat, flex_lstat, flex_fstat
 * basic stat, but gets it right when asked to stat
 * a Unix-style path ending in a directory name (e.g. dir1/dir2/dir3)
 */

#ifndef _USE_STD_STAT
/* encode_dev packs a VMS device name string into an integer to allow
 * simple comparisons. This can be used, for example, to check whether two
 * files are located on the same device, by comparing their encoded device
 * names. Even a string comparison would not do, because stat() reuses the
 * device name buffer for each call; so without encode_dev, it would be
 * necessary to save the buffer and use strcmp (this would mean a number of
 * changes to the standard Perl code, to say nothing of what a Perl script
 * would have to do.
 *
 * The device lock id, if it exists, should be unique (unless perhaps compared
 * with lock ids transferred from other nodes). We have a lock id if the disk is
 * mounted cluster-wide, which is when we tend to get long (host-qualified)
 * device names. Thus we use the lock id in preference, and only if that isn't
 * available, do we try to pack the device name into an integer (flagged by
 * the sign bit (LOCKID_MASK) being set).
 *
 * Note that encode_dev cannot guarantee an 1-to-1 correspondence twixt device
 * name and its encoded form, but it seems very unlikely that we will find
 * two files on different disks that share the same encoded device names,
 * and even more remote that they will share the same file id (if the test
 * is to check for the same file).
 *
 * A better method might be to use sys$device_scan on the first call, and to
 * search for the device, returning an index into the cached array.
 * The number returned would be more intelligible.
 * This is probably not worth it, and anyway would take quite a bit longer
 * on the first call.
 */
#define LOCKID_MASK 0x80000000     /* Use 0 to force device name use only */
static mydev_t encode_dev (pTHX_ const char *dev)
{
  int i;
  unsigned long int f;
  mydev_t enc;
  char c;
  const char *q;

  if (!dev || !dev[0]) return 0;

#if LOCKID_MASK
  {
    struct dsc$descriptor_s dev_desc;
    unsigned long int status, lockid = 0, item = DVI$_LOCKID;

    /* For cluster-mounted disks, the disk lock identifier is unique, so we
       can try that first. */
    dev_desc.dsc$w_length =  strlen (dev);
    dev_desc.dsc$b_dtype =   DSC$K_DTYPE_T;
    dev_desc.dsc$b_class =   DSC$K_CLASS_S;
    dev_desc.dsc$a_pointer = (char *) dev;  /* Read only parameter */
    status = lib$getdvi(&item, 0, &dev_desc, &lockid, 0, 0);
    if (!$VMS_STATUS_SUCCESS(status)) {
      switch (status) {
        case SS$_NOSUCHDEV: 
          SETERRNO(ENODEV, status);
          return 0;
        default: 
          _ckvmssts(status);
      }
    }
    if (lockid) return (lockid & ~LOCKID_MASK);
  }
#endif

  /* Otherwise we try to encode the device name */
  enc = 0;
  f = 1;
  i = 0;
  for (q = dev + strlen(dev); q--; q >= dev) {
    if (*q == ':')
	break;
    if (isdigit (*q))
      c= (*q) - '0';
    else if (isalpha (toupper (*q)))
      c= toupper (*q) - 'A' + (char)10;
    else
      continue; /* Skip '$'s */
    i++;
    if (i>6) break;     /* 36^7 is too large to fit in an unsigned long int */
    if (i>1) f *= 36;
    enc += f * (unsigned long int) c;
  }
  return (enc | LOCKID_MASK);  /* May have already overflowed into bit 31 */

}  /* end of encode_dev() */
#define VMS_DEVICE_ENCODE(device_no, devname, new_dev_no) \
	device_no = encode_dev(aTHX_ devname)
#else
#define VMS_DEVICE_ENCODE(device_no, devname, new_dev_no) \
	device_no = new_dev_no
#endif

static int
is_null_device(name)
    const char *name;
{
  if (decc_bug_devnull != 0) {
    if (strncmp("/dev/null", name, 9) == 0)
      return 1;
  }
    /* The VMS null device is named "_NLA0:", usually abbreviated as "NL:".
       The underscore prefix, controller letter, and unit number are
       independently optional; for our purposes, the colon punctuation
       is not.  The colon can be trailed by optional directory and/or
       filename, but two consecutive colons indicates a nodename rather
       than a device.  [pr]  */
  if (*name == '_') ++name;
  if (tolower(*name++) != 'n') return 0;
  if (tolower(*name++) != 'l') return 0;
  if (tolower(*name) == 'a') ++name;
  if (*name == '0') ++name;
  return (*name++ == ':') && (*name != ':');
}


static I32
Perl_cando_by_name_int
   (pTHX_ I32 bit, bool effective, const char *fname, int opts)
{
  char usrname[L_cuserid];
  struct dsc$descriptor_s usrdsc =
         {0, DSC$K_DTYPE_T, DSC$K_CLASS_S, usrname};
  char *vmsname = NULL, *fileified = NULL;
  unsigned long int objtyp = ACL$C_FILE, access, retsts, privused, iosb[2], flags;
  unsigned short int retlen, trnlnm_iter_count;
  struct dsc$descriptor_s namdsc = {0, DSC$K_DTYPE_T, DSC$K_CLASS_S, 0};
  union prvdef curprv;
  struct itmlst_3 armlst[4] = {{sizeof access, CHP$_ACCESS, &access, &retlen},
         {sizeof privused, CHP$_PRIVUSED, &privused, &retlen},
         {sizeof flags, CHP$_FLAGS, &flags, &retlen},{0,0,0,0}};
  struct itmlst_3 jpilst[3] = {{sizeof curprv, JPI$_CURPRIV, &curprv, &retlen},
         {sizeof usrname, JPI$_USERNAME, &usrname, &usrdsc.dsc$w_length},
         {0,0,0,0}};
  struct itmlst_3 usrprolst[2] = {{sizeof curprv, CHP$_PRIV, &curprv, &retlen},
         {0,0,0,0}};
  struct dsc$descriptor_s usrprodsc = {0, DSC$K_DTYPE_T, DSC$K_CLASS_S, 0};
  Stat_t st;
  static int profile_context = -1;

  if (!fname || !*fname) return FALSE;

  /* Make sure we expand logical names, since sys$check_access doesn't */
  fileified = PerlMem_malloc(VMS_MAXRSS);
  if (fileified == NULL) _ckvmssts(SS$_INSFMEM);
  if (!strpbrk(fname,"/]>:")) {
      strcpy(fileified,fname);
      trnlnm_iter_count = 0;
      while (!strpbrk(fileified,"/]>:") && my_trnlnm(fileified,fileified,0)) {
        trnlnm_iter_count++; 
        if (trnlnm_iter_count >= PERL_LNM_MAX_ITER) break;
      }
      fname = fileified;
  }

  vmsname = PerlMem_malloc(VMS_MAXRSS);
  if (vmsname == NULL) _ckvmssts(SS$_INSFMEM);
  if ( !(opts & PERL_RMSEXPAND_M_VMS_IN) ) {
    /* Don't know if already in VMS format, so make sure */
    if (!do_rmsexpand(fname, vmsname, 0, NULL, PERL_RMSEXPAND_M_VMS, NULL, NULL)) {
      PerlMem_free(fileified);
      PerlMem_free(vmsname);
      return FALSE;
    }
  }
  else {
    strcpy(vmsname,fname);
  }

  /* sys$check_access needs a file spec, not a directory spec.
   * Don't use flex_stat here, as that depends on thread context
   * having been initialized, and we may get here during startup.
   */

  retlen = namdsc.dsc$w_length = strlen(vmsname);
  if (vmsname[retlen-1] == ']' 
      || vmsname[retlen-1] == '>' 
      || vmsname[retlen-1] == ':'
      || (!stat(vmsname, (stat_t *)&st) && S_ISDIR(st.st_mode))) {

      if (!do_fileify_dirspec(vmsname,fileified,1,NULL)) {
        PerlMem_free(fileified);
        PerlMem_free(vmsname);
        return FALSE;
      }
      fname = fileified;
  }
  else {
      fname = vmsname;
  }

  retlen = namdsc.dsc$w_length = strlen(fname);
  namdsc.dsc$a_pointer = (char *)fname;

  switch (bit) {
    case S_IXUSR: case S_IXGRP: case S_IXOTH:
      access = ARM$M_EXECUTE;
      flags = CHP$M_READ;
      break;
    case S_IRUSR: case S_IRGRP: case S_IROTH:
      access = ARM$M_READ;
      flags = CHP$M_READ | CHP$M_USEREADALL;
      break;
    case S_IWUSR: case S_IWGRP: case S_IWOTH:
      access = ARM$M_WRITE;
      flags = CHP$M_READ | CHP$M_WRITE;
      break;
    case S_IDUSR: case S_IDGRP: case S_IDOTH:
      access = ARM$M_DELETE;
      flags = CHP$M_READ | CHP$M_WRITE;
      break;
    default:
      if (fileified != NULL)
	PerlMem_free(fileified);
      if (vmsname != NULL)
	PerlMem_free(vmsname);
      return FALSE;
  }

  /* Before we call $check_access, create a user profile with the current
   * process privs since otherwise it just uses the default privs from the
   * UAF and might give false positives or negatives.  This only works on
   * VMS versions v6.0 and later since that's when sys$create_user_profile
   * became available.
   */

  /* get current process privs and username */
  _ckvmssts(sys$getjpiw(0,0,0,jpilst,iosb,0,0));
  _ckvmssts(iosb[0]);

#if defined(__VMS_VER) && __VMS_VER >= 60000000

  /* find out the space required for the profile */
  _ckvmssts(sys$create_user_profile(&usrdsc,&usrprolst,0,0,
                                    &usrprodsc.dsc$w_length,&profile_context));

  /* allocate space for the profile and get it filled in */
  usrprodsc.dsc$a_pointer = PerlMem_malloc(usrprodsc.dsc$w_length);
  if (usrprodsc.dsc$a_pointer == NULL) _ckvmssts(SS$_INSFMEM);
  _ckvmssts(sys$create_user_profile(&usrdsc,&usrprolst,0,usrprodsc.dsc$a_pointer,
                                    &usrprodsc.dsc$w_length,&profile_context));

  /* use the profile to check access to the file; free profile & analyze results */
  retsts = sys$check_access(&objtyp,&namdsc,0,armlst,&profile_context,0,0,&usrprodsc);
  PerlMem_free(usrprodsc.dsc$a_pointer);
  if (retsts == SS$_NOCALLPRIV) retsts = SS$_NOPRIV; /* not really 3rd party */

#else

  retsts = sys$check_access(&objtyp,&namdsc,&usrdsc,armlst);

#endif

  if (retsts == SS$_NOPRIV      || retsts == SS$_NOSUCHOBJECT ||
      retsts == SS$_INVFILFOROP || retsts == RMS$_FNF || retsts == RMS$_SYN ||
      retsts == RMS$_DIR        || retsts == RMS$_DEV || retsts == RMS$_DNF) {
    set_vaxc_errno(retsts);
    if (retsts == SS$_NOPRIV) set_errno(EACCES);
    else if (retsts == SS$_INVFILFOROP) set_errno(EINVAL);
    else set_errno(ENOENT);
    if (fileified != NULL)
      PerlMem_free(fileified);
    if (vmsname != NULL)
      PerlMem_free(vmsname);
    return FALSE;
  }
  if (retsts == SS$_NORMAL || retsts == SS$_ACCONFLICT) {
    if (fileified != NULL)
      PerlMem_free(fileified);
    if (vmsname != NULL)
      PerlMem_free(vmsname);
    return TRUE;
  }
  _ckvmssts(retsts);

  if (fileified != NULL)
    PerlMem_free(fileified);
  if (vmsname != NULL)
    PerlMem_free(vmsname);
  return FALSE;  /* Should never get here */

}

/* Do the permissions allow some operation?  Assumes PL_statcache already set. */
/* Do this via $Check_Access on VMS, since the CRTL stat() returns only a
 * subset of the applicable information.
 */
bool
Perl_cando(pTHX_ Mode_t bit, bool effective, const Stat_t *statbufp)
{
  return cando_by_name_int
	(bit, effective, statbufp->st_devnam, PERL_RMSEXPAND_M_VMS_IN);
}  /* end of cando() */
/*}}}*/


/*{{{I32 cando_by_name(I32 bit, bool effective, char *fname)*/
I32
Perl_cando_by_name(pTHX_ I32 bit, bool effective, const char *fname)
{
   return cando_by_name_int(bit, effective, fname, 0);

}  /* end of cando_by_name() */
/*}}}*/


/*{{{ int flex_fstat(int fd, Stat_t *statbuf)*/
int
Perl_flex_fstat(pTHX_ int fd, Stat_t *statbufp)
{
  if (!fstat(fd,(stat_t *) statbufp)) {
    char *cptr;
    char *vms_filename;
    vms_filename = PerlMem_malloc(VMS_MAXRSS);
    if (vms_filename == NULL) _ckvmssts(SS$_INSFMEM);

    /* Save name for cando by name in VMS format */
    cptr = getname(fd, vms_filename, 1);

    /* This should not happen, but just in case */
    if (cptr == NULL) {
	statbufp->st_devnam[0] = 0;
    }
    else {
	/* Make sure that the saved name fits in 255 characters */
	cptr = do_rmsexpand
		       (vms_filename,
			statbufp->st_devnam, 
			0,
			NULL,
			PERL_RMSEXPAND_M_VMS | PERL_RMSEXPAND_M_VMS_IN,
			NULL,
			NULL);
	if (cptr == NULL)
	    statbufp->st_devnam[0] = 0;
    }
    PerlMem_free(vms_filename);

    VMS_INO_T_COPY(statbufp->st_ino, statbufp->crtl_stat.st_ino);
    VMS_DEVICE_ENCODE
	(statbufp->st_dev, statbufp->st_devnam, statbufp->crtl_stat.st_dev);

#   ifdef RTL_USES_UTC
#   ifdef VMSISH_TIME
    if (VMSISH_TIME) {
      statbufp->st_mtime = _toloc(statbufp->st_mtime);
      statbufp->st_atime = _toloc(statbufp->st_atime);
      statbufp->st_ctime = _toloc(statbufp->st_ctime);
    }
#   endif
#   else
#   ifdef VMSISH_TIME
    if (!VMSISH_TIME) { /* Return UTC instead of local time */
#   else
    if (1) {
#   endif
      statbufp->st_mtime = _toutc(statbufp->st_mtime);
      statbufp->st_atime = _toutc(statbufp->st_atime);
      statbufp->st_ctime = _toutc(statbufp->st_ctime);
    }
#endif
    return 0;
  }
  return -1;

}  /* end of flex_fstat() */
/*}}}*/

#if !defined(__VAX) && __CRTL_VER >= 80200000
#ifdef lstat
#undef lstat
#endif
#else
#ifdef lstat
#undef lstat
#endif
#define lstat(_x, _y) stat(_x, _y)
#endif

#define flex_stat_int(a,b,c)		Perl_flex_stat_int(aTHX_ a,b,c)

static int
Perl_flex_stat_int(pTHX_ const char *fspec, Stat_t *statbufp, int lstat_flag)
{
    char fileified[VMS_MAXRSS];
    char temp_fspec[VMS_MAXRSS];
    char *save_spec;
    int retval = -1;
    int saved_errno, saved_vaxc_errno;

    if (!fspec) return retval;
    saved_errno = errno; saved_vaxc_errno = vaxc$errno;
    strcpy(temp_fspec, fspec);

    if (decc_bug_devnull != 0) {
      if (is_null_device(temp_fspec)) { /* Fake a stat() for the null device */
	memset(statbufp,0,sizeof *statbufp);
        VMS_DEVICE_ENCODE(statbufp->st_dev, "_NLA0:", 0);
	statbufp->st_mode = S_IFBLK | S_IREAD | S_IWRITE | S_IEXEC;
	statbufp->st_uid = 0x00010001;
	statbufp->st_gid = 0x0001;
	time((time_t *)&statbufp->st_mtime);
	statbufp->st_atime = statbufp->st_ctime = statbufp->st_mtime;
	return 0;
      }
    }

    /* Try for a directory name first.  If fspec contains a filename without
     * a type (e.g. sea:[wine.dark]water), and both sea:[wine.dark]water.dir
     * and sea:[wine.dark]water. exist, we prefer the directory here.
     * Similarly, sea:[wine.dark] returns the result for sea:[wine]dark.dir,
     * not sea:[wine.dark]., if the latter exists.  If the intended target is
     * the file with null type, specify this by calling flex_stat() with
     * a '.' at the end of fspec.
     *
     * If we are in Posix filespec mode, accept the filename as is.
     */


#if __CRTL_VER >= 70300000 && !defined(__VAX)
  /* The CRTL stat() falls down hard on multi-dot filenames in unix format unless
   * DECC$EFS_CHARSET is in effect, so temporarily enable it if it isn't already.
   */
  if (!decc_efs_charset)
    decc$feature_set_value(decc$feature_get_index("DECC$EFS_CHARSET"),1,1); 
#endif

#if __CRTL_VER >= 80200000 && !defined(__VAX)
  if (decc_posix_compliant_pathnames == 0) {
#endif
    if (do_fileify_dirspec(temp_fspec,fileified,0,NULL) != NULL) {
      if (lstat_flag == 0)
	retval = stat(fileified,(stat_t *) statbufp);
      else
	retval = lstat(fileified,(stat_t *) statbufp);
      save_spec = fileified;
    }
    if (retval) {
      if (lstat_flag == 0)
	retval = stat(temp_fspec,(stat_t *) statbufp);
      else
	retval = lstat(temp_fspec,(stat_t *) statbufp);
      save_spec = temp_fspec;
    }
/*
 * In debugging, on 8.3 Alpha, I found a case where stat was returning a
 * file not found error for a directory named foo:[bar.t] or /foo/bar/t
 * and lstat was working correctly for the same file.
 * The only syntax that was working for stat was "foo:[bar]t.dir".
 *
 * Other directories with the same syntax worked fine.
 * So work around the problem when it shows up here.
 */
    if (retval) {
        int save_errno = errno;
	if (do_tovmsspec(fspec, temp_fspec, 0, NULL) != NULL) {
	    if (do_fileify_dirspec(temp_fspec, fileified, 0, NULL) != NULL) {
		retval = stat(fileified, (stat_t *) statbufp);
		save_spec = fileified;
	    }
	}
	/* Restore the errno value if third stat does not succeed */
	if (retval != 0)
	    errno = save_errno;
    }
#if __CRTL_VER >= 80200000 && !defined(__VAX)
  } else {
    if (lstat_flag == 0)
      retval = stat(temp_fspec,(stat_t *) statbufp);
    else
      retval = lstat(temp_fspec,(stat_t *) statbufp);
      save_spec = temp_fspec;
  }
#endif

#if __CRTL_VER >= 70300000 && !defined(__VAX)
  /* As you were... */
  if (!decc_efs_charset)
    decc$feature_set_value(decc$feature_get_index("DECC$EFS_CHARSET"),1,0); 
#endif

    if (!retval) {
    char * cptr;
    int rmsex_flags = PERL_RMSEXPAND_M_VMS;

      /* If this is an lstat, do not follow the link */
      if (lstat_flag)
	rmsex_flags |= PERL_RMSEXPAND_M_SYMLINK;

      cptr = do_rmsexpand
       (save_spec, statbufp->st_devnam, 0, NULL, rmsex_flags, NULL, NULL);
      if (cptr == NULL)
	statbufp->st_devnam[0] = 0;

      VMS_INO_T_COPY(statbufp->st_ino, statbufp->crtl_stat.st_ino);
      VMS_DEVICE_ENCODE
	(statbufp->st_dev, statbufp->st_devnam, statbufp->crtl_stat.st_dev);
#     ifdef RTL_USES_UTC
#     ifdef VMSISH_TIME
      if (VMSISH_TIME) {
        statbufp->st_mtime = _toloc(statbufp->st_mtime);
        statbufp->st_atime = _toloc(statbufp->st_atime);
        statbufp->st_ctime = _toloc(statbufp->st_ctime);
      }
#     endif
#     else
#     ifdef VMSISH_TIME
      if (!VMSISH_TIME) { /* Return UTC instead of local time */
#     else
      if (1) {
#     endif
        statbufp->st_mtime = _toutc(statbufp->st_mtime);
        statbufp->st_atime = _toutc(statbufp->st_atime);
        statbufp->st_ctime = _toutc(statbufp->st_ctime);
      }
#     endif
    }
    /* If we were successful, leave errno where we found it */
    if (retval == 0) { errno = saved_errno; vaxc$errno = saved_vaxc_errno; }
    return retval;

}  /* end of flex_stat_int() */


/*{{{ int flex_stat(const char *fspec, Stat_t *statbufp)*/
int
Perl_flex_stat(pTHX_ const char *fspec, Stat_t *statbufp)
{
   return flex_stat_int(fspec, statbufp, 0);
}
/*}}}*/

/*{{{ int flex_lstat(const char *fspec, Stat_t *statbufp)*/
int
Perl_flex_lstat(pTHX_ const char *fspec, Stat_t *statbufp)
{
   return flex_stat_int(fspec, statbufp, 1);
}
/*}}}*/


/*{{{char *my_getlogin()*/
/* VMS cuserid == Unix getlogin, except calling sequence */
char *
my_getlogin(void)
{
    static char user[L_cuserid];
    return cuserid(user);
}
/*}}}*/


/*  rmscopy - copy a file using VMS RMS routines
 *
 *  Copies contents and attributes of spec_in to spec_out, except owner
 *  and protection information.  Name and type of spec_in are used as
 *  defaults for spec_out.  The third parameter specifies whether rmscopy()
 *  should try to propagate timestamps from the input file to the output file.
 *  If it is less than 0, no timestamps are preserved.  If it is 0, then
 *  rmscopy() will behave similarly to the DCL COPY command: timestamps are
 *  propagated to the output file at creation iff the output file specification
 *  did not contain an explicit name or type, and the revision date is always
 *  updated at the end of the copy operation.  If it is greater than 0, then
 *  it is interpreted as a bitmask, in which bit 0 indicates that timestamps
 *  other than the revision date should be propagated, and bit 1 indicates
 *  that the revision date should be propagated.
 *
 *  Returns 1 on success; returns 0 and sets errno and vaxc$errno on failure.
 *
 *  Copyright 1996 by Charles Bailey <bailey@newman.upenn.edu>.
 *  Incorporates, with permission, some code from EZCOPY by Tim Adye
 *  <T.J.Adye@rl.ac.uk>.  Permission is given to distribute this code
 * as part of the Perl standard distribution under the terms of the
 * GNU General Public License or the Perl Artistic License.  Copies
 * of each may be found in the Perl standard distribution.
 */ /* FIXME */
/*{{{int rmscopy(char *src, char *dst, int preserve_dates)*/
int
Perl_rmscopy(pTHX_ const char *spec_in, const char *spec_out, int preserve_dates)
{
    char *vmsin, * vmsout, *esa, *esal, *esa_out, *esal_out,
         *rsa, *rsal, *rsa_out, *rsal_out, *ubf;
    unsigned long int i, sts, sts2;
    int dna_len;
    struct FAB fab_in, fab_out;
    struct RAB rab_in, rab_out;
    rms_setup_nam(nam);
    rms_setup_nam(nam_out);
    struct XABDAT xabdat;
    struct XABFHC xabfhc;
    struct XABRDT xabrdt;
    struct XABSUM xabsum;

    vmsin = PerlMem_malloc(VMS_MAXRSS);
    if (vmsin == NULL) _ckvmssts(SS$_INSFMEM);
    vmsout = PerlMem_malloc(VMS_MAXRSS);
    if (vmsout == NULL) _ckvmssts(SS$_INSFMEM);
    if (!spec_in  || !*spec_in  || !do_tovmsspec(spec_in,vmsin,1,NULL) ||
        !spec_out || !*spec_out || !do_tovmsspec(spec_out,vmsout,1,NULL)) {
      PerlMem_free(vmsin);
      PerlMem_free(vmsout);
      set_errno(EINVAL); set_vaxc_errno(LIB$_INVARG);
      return 0;
    }

    esa = PerlMem_malloc(VMS_MAXRSS);
    if (esa == NULL) _ckvmssts(SS$_INSFMEM);
    esal = NULL;
#if !defined(__VAX) && defined(NAML$C_MAXRSS)
    esal = PerlMem_malloc(VMS_MAXRSS);
    if (esal == NULL) _ckvmssts(SS$_INSFMEM);
#endif
    fab_in = cc$rms_fab;
    rms_set_fna(fab_in, nam, vmsin, strlen(vmsin));
    fab_in.fab$b_shr = FAB$M_SHRPUT | FAB$M_UPI;
    fab_in.fab$b_fac = FAB$M_BIO | FAB$M_GET;
    fab_in.fab$l_fop = FAB$M_SQO;
    rms_bind_fab_nam(fab_in, nam);
    fab_in.fab$l_xab = (void *) &xabdat;

    rsa = PerlMem_malloc(VMS_MAXRSS);
    if (rsa == NULL) _ckvmssts(SS$_INSFMEM);
    rsal = NULL;
#if !defined(__VAX) && defined(NAML$C_MAXRSS)
    rsal = PerlMem_malloc(VMS_MAXRSS);
    if (rsal == NULL) _ckvmssts(SS$_INSFMEM);
#endif
    rms_set_rsal(nam, rsa, NAM$C_MAXRSS, rsal, (VMS_MAXRSS - 1));
    rms_set_esal(nam, esa, NAM$C_MAXRSS, esal, (VMS_MAXRSS - 1));
    rms_nam_esl(nam) = 0;
    rms_nam_rsl(nam) = 0;
    rms_nam_esll(nam) = 0;
    rms_nam_rsll(nam) = 0;
#ifdef NAM$M_NO_SHORT_UPCASE
    if (decc_efs_case_preserve)
	rms_set_nam_nop(nam, NAM$M_NO_SHORT_UPCASE);
#endif

    xabdat = cc$rms_xabdat;        /* To get creation date */
    xabdat.xab$l_nxt = (void *) &xabfhc;

    xabfhc = cc$rms_xabfhc;        /* To get record length */
    xabfhc.xab$l_nxt = (void *) &xabsum;

    xabsum = cc$rms_xabsum;        /* To get key and area information */

    if (!((sts = sys$open(&fab_in)) & 1)) {
      PerlMem_free(vmsin);
      PerlMem_free(vmsout);
      PerlMem_free(esa);
      if (esal != NULL)
	PerlMem_free(esal);
      PerlMem_free(rsa);
      if (rsal != NULL)
	PerlMem_free(rsal);
      set_vaxc_errno(sts);
      switch (sts) {
        case RMS$_FNF: case RMS$_DNF:
          set_errno(ENOENT); break;
        case RMS$_DIR:
          set_errno(ENOTDIR); break;
        case RMS$_DEV:
          set_errno(ENODEV); break;
        case RMS$_SYN:
          set_errno(EINVAL); break;
        case RMS$_PRV:
          set_errno(EACCES); break;
        default:
          set_errno(EVMSERR);
      }
      return 0;
    }

    nam_out = nam;
    fab_out = fab_in;
    fab_out.fab$w_ifi = 0;
    fab_out.fab$b_fac = FAB$M_BIO | FAB$M_PUT;
    fab_out.fab$b_shr = FAB$M_SHRGET | FAB$M_UPI;
    fab_out.fab$l_fop = FAB$M_SQO;
    rms_bind_fab_nam(fab_out, nam_out);
    rms_set_fna(fab_out, nam_out, vmsout, strlen(vmsout));
    dna_len = rms_nam_namel(nam) ? rms_nam_name_type_l_size(nam) : 0;
    rms_set_dna(fab_out, nam_out, rms_nam_namel(nam), dna_len);
    esa_out = PerlMem_malloc(NAM$C_MAXRSS + 1);
    if (esa_out == NULL) _ckvmssts(SS$_INSFMEM);
    rsa_out = PerlMem_malloc(NAM$C_MAXRSS + 1);
    if (rsa_out == NULL) _ckvmssts(SS$_INSFMEM);
    esal_out = NULL;
    rsal_out = NULL;
#if !defined(__VAX) && defined(NAML$C_MAXRSS)
    esal_out = PerlMem_malloc(VMS_MAXRSS);
    if (esal_out == NULL) _ckvmssts(SS$_INSFMEM);
    rsal_out = PerlMem_malloc(VMS_MAXRSS);
    if (rsal_out == NULL) _ckvmssts(SS$_INSFMEM);
#endif
    rms_set_rsal(nam_out, rsa_out, NAM$C_MAXRSS, rsal_out, (VMS_MAXRSS - 1));
    rms_set_esal(nam_out, esa_out, NAM$C_MAXRSS, esal_out, (VMS_MAXRSS - 1));

    if (preserve_dates == 0) {  /* Act like DCL COPY */
      rms_set_nam_nop(nam_out, NAM$M_SYNCHK);
      fab_out.fab$l_xab = NULL;  /* Don't disturb data from input file */
      if (!((sts = sys$parse(&fab_out)) & STS$K_SUCCESS)) {
	PerlMem_free(vmsin);
	PerlMem_free(vmsout);
	PerlMem_free(esa);
	if (esal != NULL)
	    PerlMem_free(esal);
	PerlMem_free(rsa);
	if (rsal != NULL)
	    PerlMem_free(rsal);
	PerlMem_free(esa_out);
	if (esal_out != NULL)
	    PerlMem_free(esal_out);
	PerlMem_free(rsa_out);
	if (rsal_out != NULL)
	    PerlMem_free(rsal_out);
        set_errno(sts == RMS$_SYN ? EINVAL : EVMSERR);
        set_vaxc_errno(sts);
        return 0;
      }
      fab_out.fab$l_xab = (void *) &xabdat;
      if (rms_is_nam_fnb(nam, NAM$M_EXP_NAME | NAM$M_EXP_TYPE))
	preserve_dates = 1;
    }
    if (preserve_dates < 0)   /* Clear all bits; we'll use it as a */
      preserve_dates =0;      /* bitmask from this point forward   */

    if (!(preserve_dates & 1)) fab_out.fab$l_xab = (void *) &xabfhc;
    if (!((sts = sys$create(&fab_out)) & STS$K_SUCCESS)) {
      PerlMem_free(vmsin);
      PerlMem_free(vmsout);
      PerlMem_free(esa);
      if (esal != NULL)
	  PerlMem_free(esal);
      PerlMem_free(rsa);
      if (rsal != NULL)
	  PerlMem_free(rsal);
      PerlMem_free(esa_out);
      if (esal_out != NULL)
	  PerlMem_free(esal_out);
      PerlMem_free(rsa_out);
      if (rsal_out != NULL)
	  PerlMem_free(rsal_out);
      set_vaxc_errno(sts);
      switch (sts) {
        case RMS$_DNF:
          set_errno(ENOENT); break;
        case RMS$_DIR:
          set_errno(ENOTDIR); break;
        case RMS$_DEV:
          set_errno(ENODEV); break;
        case RMS$_SYN:
          set_errno(EINVAL); break;
        case RMS$_PRV:
          set_errno(EACCES); break;
        default:
          set_errno(EVMSERR);
      }
      return 0;
    }
    fab_out.fab$l_fop |= FAB$M_DLT;  /* in case we have to bail out */
    if (preserve_dates & 2) {
      /* sys$close() will process xabrdt, not xabdat */
      xabrdt = cc$rms_xabrdt;
#ifndef __GNUC__
      xabrdt.xab$q_rdt = xabdat.xab$q_rdt;
#else
      /* gcc doesn't like the assignment, since its prototype for xab$q_rdt
       * is unsigned long[2], while DECC & VAXC use a struct */
      memcpy(xabrdt.xab$q_rdt,xabdat.xab$q_rdt,sizeof xabrdt.xab$q_rdt);
#endif
      fab_out.fab$l_xab = (void *) &xabrdt;
    }

    ubf = PerlMem_malloc(32256);
    if (ubf == NULL) _ckvmssts(SS$_INSFMEM);
    rab_in = cc$rms_rab;
    rab_in.rab$l_fab = &fab_in;
    rab_in.rab$l_rop = RAB$M_BIO;
    rab_in.rab$l_ubf = ubf;
    rab_in.rab$w_usz = 32256;
    if (!((sts = sys$connect(&rab_in)) & 1)) {
      sys$close(&fab_in); sys$close(&fab_out);
      PerlMem_free(vmsin);
      PerlMem_free(vmsout);
      PerlMem_free(ubf);
      PerlMem_free(esa);
      if (esal != NULL)
	  PerlMem_free(esal);
      PerlMem_free(rsa);
      if (rsal != NULL)
	  PerlMem_free(rsal);
      PerlMem_free(esa_out);
      if (esal_out != NULL)
	  PerlMem_free(esal_out);
      PerlMem_free(rsa_out);
      if (rsal_out != NULL)
	  PerlMem_free(rsal_out);
      set_errno(EVMSERR); set_vaxc_errno(sts);
      return 0;
    }

    rab_out = cc$rms_rab;
    rab_out.rab$l_fab = &fab_out;
    rab_out.rab$l_rbf = ubf;
    if (!((sts = sys$connect(&rab_out)) & 1)) {
      sys$close(&fab_in); sys$close(&fab_out);
      PerlMem_free(vmsin);
      PerlMem_free(vmsout);
      PerlMem_free(ubf);
      PerlMem_free(esa);
      if (esal != NULL)
	  PerlMem_free(esal);
      PerlMem_free(rsa);
      if (rsal != NULL)
	  PerlMem_free(rsal);
      PerlMem_free(esa_out);
      if (esal_out != NULL)
	  PerlMem_free(esal_out);
      PerlMem_free(rsa_out);
      if (rsal_out != NULL)
	  PerlMem_free(rsal_out);
      set_errno(EVMSERR); set_vaxc_errno(sts);
      return 0;
    }

    while ((sts = sys$read(&rab_in))) {  /* always true  */
      if (sts == RMS$_EOF) break;
      rab_out.rab$w_rsz = rab_in.rab$w_rsz;
      if (!(sts & 1) || !((sts = sys$write(&rab_out)) & 1)) {
        sys$close(&fab_in); sys$close(&fab_out);
	PerlMem_free(vmsin);
	PerlMem_free(vmsout);
	PerlMem_free(ubf);
	PerlMem_free(esa);
	if (esal != NULL)
	    PerlMem_free(esal);
	PerlMem_free(rsa);
	if (rsal != NULL)
	    PerlMem_free(rsal);
	PerlMem_free(esa_out);
 	if (esal_out != NULL)
	    PerlMem_free(esal_out);
	PerlMem_free(rsa_out);
 	if (rsal_out != NULL)
	    PerlMem_free(rsal_out);
        set_errno(EVMSERR); set_vaxc_errno(sts);
        return 0;
      }
    }


    fab_out.fab$l_fop &= ~FAB$M_DLT;  /* We got this far; keep the output */
    sys$close(&fab_in);  sys$close(&fab_out);
    sts = (fab_in.fab$l_sts & 1) ? fab_out.fab$l_sts : fab_in.fab$l_sts;

    PerlMem_free(vmsin);
    PerlMem_free(vmsout);
    PerlMem_free(ubf);
    PerlMem_free(esa);
    if (esal != NULL)
	PerlMem_free(esal);
    PerlMem_free(rsa);
    if (rsal != NULL)
	PerlMem_free(rsal);
    PerlMem_free(esa_out);
    if (esal_out != NULL)
	PerlMem_free(esal_out);
    PerlMem_free(rsa_out);
    if (rsal_out != NULL)
	PerlMem_free(rsal_out);

    if (!(sts & 1)) {
      set_errno(EVMSERR); set_vaxc_errno(sts);
      return 0;
    }

    return 1;

}  /* end of rmscopy() */
/*}}}*/


/***  The following glue provides 'hooks' to make some of the routines
 * from this file available from Perl.  These routines are sufficiently
 * basic, and are required sufficiently early in the build process,
 * that's it's nice to have them available to miniperl as well as the
 * full Perl, so they're set up here instead of in an extension.  The
 * Perl code which handles importation of these names into a given
 * package lives in [.VMS]Filespec.pm in @INC.
 */

void
rmsexpand_fromperl(pTHX_ CV *cv)
{
  dXSARGS;
  char *fspec, *defspec = NULL, *rslt;
  STRLEN n_a;
  int fs_utf8, dfs_utf8;

  fs_utf8 = 0;
  dfs_utf8 = 0;
  if (!items || items > 2)
    Perl_croak(aTHX_ "Usage: VMS::Filespec::rmsexpand(spec[,defspec])");
  fspec = SvPV(ST(0),n_a);
  fs_utf8 = SvUTF8(ST(0));
  if (!fspec || !*fspec) XSRETURN_UNDEF;
  if (items == 2) {
    defspec = SvPV(ST(1),n_a);
    dfs_utf8 = SvUTF8(ST(1));
  }
  rslt = do_rmsexpand(fspec,NULL,1,defspec,0,&fs_utf8,&dfs_utf8);
  ST(0) = sv_newmortal();
  if (rslt != NULL) {
    sv_usepvn(ST(0),rslt,strlen(rslt));
    if (fs_utf8) {
	SvUTF8_on(ST(0));
    }
  }
  XSRETURN(1);
}

void
vmsify_fromperl(pTHX_ CV *cv)
{
  dXSARGS;
  char *vmsified;
  STRLEN n_a;
  int utf8_fl;

  if (items != 1) Perl_croak(aTHX_ "Usage: VMS::Filespec::vmsify(spec)");
  utf8_fl = SvUTF8(ST(0));
  vmsified = do_tovmsspec(SvPV(ST(0),n_a),NULL,1,&utf8_fl);
  ST(0) = sv_newmortal();
  if (vmsified != NULL) {
    sv_usepvn(ST(0),vmsified,strlen(vmsified));
    if (utf8_fl) {
	SvUTF8_on(ST(0));
    }
  }
  XSRETURN(1);
}

void
unixify_fromperl(pTHX_ CV *cv)
{
  dXSARGS;
  char *unixified;
  STRLEN n_a;
  int utf8_fl;

  if (items != 1) Perl_croak(aTHX_ "Usage: VMS::Filespec::unixify(spec)");
  utf8_fl = SvUTF8(ST(0));
  unixified = do_tounixspec(SvPV(ST(0),n_a),NULL,1,&utf8_fl);
  ST(0) = sv_newmortal();
  if (unixified != NULL) {
    sv_usepvn(ST(0),unixified,strlen(unixified));
    if (utf8_fl) {
	SvUTF8_on(ST(0));
    }
  }
  XSRETURN(1);
}

void
fileify_fromperl(pTHX_ CV *cv)
{
  dXSARGS;
  char *fileified;
  STRLEN n_a;
  int utf8_fl;

  if (items != 1) Perl_croak(aTHX_ "Usage: VMS::Filespec::fileify(spec)");
  utf8_fl = SvUTF8(ST(0));
  fileified = do_fileify_dirspec(SvPV(ST(0),n_a),NULL,1,&utf8_fl);
  ST(0) = sv_newmortal();
  if (fileified != NULL) {
    sv_usepvn(ST(0),fileified,strlen(fileified));
    if (utf8_fl) {
	SvUTF8_on(ST(0));
    }
  }
  XSRETURN(1);
}

void
pathify_fromperl(pTHX_ CV *cv)
{
  dXSARGS;
  char *pathified;
  STRLEN n_a;
  int utf8_fl;

  if (items != 1) Perl_croak(aTHX_ "Usage: VMS::Filespec::pathify(spec)");
  utf8_fl = SvUTF8(ST(0));
  pathified = do_pathify_dirspec(SvPV(ST(0),n_a),NULL,1,&utf8_fl);
  ST(0) = sv_newmortal();
  if (pathified != NULL) {
    sv_usepvn(ST(0),pathified,strlen(pathified));
    if (utf8_fl) {
	SvUTF8_on(ST(0));
    }
  }
  XSRETURN(1);
}

void
vmspath_fromperl(pTHX_ CV *cv)
{
  dXSARGS;
  char *vmspath;
  STRLEN n_a;
  int utf8_fl;

  if (items != 1) Perl_croak(aTHX_ "Usage: VMS::Filespec::vmspath(spec)");
  utf8_fl = SvUTF8(ST(0));
  vmspath = do_tovmspath(SvPV(ST(0),n_a),NULL,1,&utf8_fl);
  ST(0) = sv_newmortal();
  if (vmspath != NULL) {
    sv_usepvn(ST(0),vmspath,strlen(vmspath));
    if (utf8_fl) {
	SvUTF8_on(ST(0));
    }
  }
  XSRETURN(1);
}

void
unixpath_fromperl(pTHX_ CV *cv)
{
  dXSARGS;
  char *unixpath;
  STRLEN n_a;
  int utf8_fl;

  if (items != 1) Perl_croak(aTHX_ "Usage: VMS::Filespec::unixpath(spec)");
  utf8_fl = SvUTF8(ST(0));
  unixpath = do_tounixpath(SvPV(ST(0),n_a),NULL,1,&utf8_fl);
  ST(0) = sv_newmortal();
  if (unixpath != NULL) {
    sv_usepvn(ST(0),unixpath,strlen(unixpath));
    if (utf8_fl) {
	SvUTF8_on(ST(0));
    }
  }
  XSRETURN(1);
}

void
candelete_fromperl(pTHX_ CV *cv)
{
  dXSARGS;
  char *fspec, *fsp;
  SV *mysv;
  IO *io;
  STRLEN n_a;

  if (items != 1) Perl_croak(aTHX_ "Usage: VMS::Filespec::candelete(spec)");

  mysv = SvROK(ST(0)) ? SvRV(ST(0)) : ST(0);
  Newx(fspec, VMS_MAXRSS, char);
  if (fspec == NULL) _ckvmssts(SS$_INSFMEM);
  if (SvTYPE(mysv) == SVt_PVGV) {
    if (!(io = GvIOp(mysv)) || !PerlIO_getname(IoIFP(io),fspec)) {
      set_errno(EINVAL); set_vaxc_errno(LIB$_INVARG);
      ST(0) = &PL_sv_no;
      Safefree(fspec);
      XSRETURN(1);
    }
    fsp = fspec;
  }
  else {
    if (mysv != ST(0) || !(fsp = SvPV(mysv,n_a)) || !*fsp) {
      set_errno(EINVAL); set_vaxc_errno(LIB$_INVARG);
      ST(0) = &PL_sv_no;
      Safefree(fspec);
      XSRETURN(1);
    }
  }

  ST(0) = boolSV(cando_by_name(S_IDUSR,0,fsp));
  Safefree(fspec);
  XSRETURN(1);
}

void
rmscopy_fromperl(pTHX_ CV *cv)
{
  dXSARGS;
  char *inspec, *outspec, *inp, *outp;
  int date_flag;
  struct dsc$descriptor indsc  = { 0, DSC$K_DTYPE_T, DSC$K_CLASS_S, 0},
                        outdsc = { 0, DSC$K_DTYPE_T, DSC$K_CLASS_S, 0};
  unsigned long int sts;
  SV *mysv;
  IO *io;
  STRLEN n_a;

  if (items < 2 || items > 3)
    Perl_croak(aTHX_ "Usage: File::Copy::rmscopy(from,to[,date_flag])");

  mysv = SvROK(ST(0)) ? SvRV(ST(0)) : ST(0);
  Newx(inspec, VMS_MAXRSS, char);
  if (SvTYPE(mysv) == SVt_PVGV) {
    if (!(io = GvIOp(mysv)) || !PerlIO_getname(IoIFP(io),inspec)) {
      set_errno(EINVAL); set_vaxc_errno(LIB$_INVARG);
      ST(0) = &PL_sv_no;
      Safefree(inspec);
      XSRETURN(1);
    }
    inp = inspec;
  }
  else {
    if (mysv != ST(0) || !(inp = SvPV(mysv,n_a)) || !*inp) {
      set_errno(EINVAL); set_vaxc_errno(LIB$_INVARG);
      ST(0) = &PL_sv_no;
      Safefree(inspec);
      XSRETURN(1);
    }
  }
  mysv = SvROK(ST(1)) ? SvRV(ST(1)) : ST(1);
  Newx(outspec, VMS_MAXRSS, char);
  if (SvTYPE(mysv) == SVt_PVGV) {
    if (!(io = GvIOp(mysv)) || !PerlIO_getname(IoIFP(io),outspec)) {
      set_errno(EINVAL); set_vaxc_errno(LIB$_INVARG);
      ST(0) = &PL_sv_no;
      Safefree(inspec);
      Safefree(outspec);
      XSRETURN(1);
    }
    outp = outspec;
  }
  else {
    if (mysv != ST(1) || !(outp = SvPV(mysv,n_a)) || !*outp) {
      set_errno(EINVAL); set_vaxc_errno(LIB$_INVARG);
      ST(0) = &PL_sv_no;
      Safefree(inspec);
      Safefree(outspec);
      XSRETURN(1);
    }
  }
  date_flag = (items == 3) ? SvIV(ST(2)) : 0;

  ST(0) = boolSV(rmscopy(inp,outp,date_flag));
  Safefree(inspec);
  Safefree(outspec);
  XSRETURN(1);
}

/* The mod2fname is limited to shorter filenames by design, so it should
 * not be modified to support longer EFS pathnames
 */
void
mod2fname(pTHX_ CV *cv)
{
  dXSARGS;
  char ultimate_name[NAM$C_MAXRSS+1], work_name[NAM$C_MAXRSS*8 + 1],
       workbuff[NAM$C_MAXRSS*1 + 1];
  int total_namelen = 3, counter, num_entries;
  /* ODS-5 ups this, but we want to be consistent, so... */
  int max_name_len = 39;
  AV *in_array = (AV *)SvRV(ST(0));

  num_entries = av_len(in_array);

  /* All the names start with PL_. */
  strcpy(ultimate_name, "PL_");

  /* Clean up our working buffer */
  Zero(work_name, sizeof(work_name), char);

  /* Run through the entries and build up a working name */
  for(counter = 0; counter <= num_entries; counter++) {
    /* If it's not the first name then tack on a __ */
    if (counter) {
      strcat(work_name, "__");
    }
    strcat(work_name, SvPV_nolen(*av_fetch(in_array, counter, FALSE)));
  }

  /* Check to see if we actually have to bother...*/
  if (strlen(work_name) + 3 <= max_name_len) {
    strcat(ultimate_name, work_name);
  } else {
    /* It's too darned big, so we need to go strip. We use the same */
    /* algorithm as xsubpp does. First, strip out doubled __ */
    char *source, *dest, last;
    dest = workbuff;
    last = 0;
    for (source = work_name; *source; source++) {
      if (last == *source && last == '_') {
	continue;
      }
      *dest++ = *source;
      last = *source;
    }
    /* Go put it back */
    strcpy(work_name, workbuff);
    /* Is it still too big? */
    if (strlen(work_name) + 3 > max_name_len) {
      /* Strip duplicate letters */
      last = 0;
      dest = workbuff;
      for (source = work_name; *source; source++) {
	if (last == toupper(*source)) {
	continue;
	}
	*dest++ = *source;
	last = toupper(*source);
      }
      strcpy(work_name, workbuff);
    }

    /* Is it *still* too big? */
    if (strlen(work_name) + 3 > max_name_len) {
      /* Too bad, we truncate */
      work_name[max_name_len - 2] = 0;
    }
    strcat(ultimate_name, work_name);
  }

  /* Okay, return it */
  ST(0) = sv_2mortal(newSVpv(ultimate_name, 0));
  XSRETURN(1);
}

void
hushexit_fromperl(pTHX_ CV *cv)
{
    dXSARGS;

    if (items > 0) {
        VMSISH_HUSHED = SvTRUE(ST(0));
    }
    ST(0) = boolSV(VMSISH_HUSHED);
    XSRETURN(1);
}


PerlIO * 
Perl_vms_start_glob
   (pTHX_ SV *tmpglob,
    IO *io)
{
    PerlIO *fp;
    struct vs_str_st *rslt;
    char *vmsspec;
    char *rstr;
    char *begin, *cp;
    $DESCRIPTOR(dfltdsc,"SYS$DISK:[]*.*;");
    PerlIO *tmpfp;
    STRLEN i;
    struct dsc$descriptor_s wilddsc = {0, DSC$K_DTYPE_T, DSC$K_CLASS_S, 0};
    struct dsc$descriptor_vs rsdsc;
    unsigned long int cxt = 0, sts = 0, ok = 1, hasdir = 0;
    unsigned long hasver = 0, isunix = 0;
    unsigned long int lff_flags = 0;
    int rms_sts;

#ifdef VMS_LONGNAME_SUPPORT
    lff_flags = LIB$M_FIL_LONG_NAMES;
#endif
    /* The Newx macro will not allow me to assign a smaller array
     * to the rslt pointer, so we will assign it to the begin char pointer
     * and then copy the value into the rslt pointer.
     */
    Newx(begin, VMS_MAXRSS + sizeof(unsigned short int), char);
    rslt = (struct vs_str_st *)begin;
    rslt->length = 0;
    rstr = &rslt->str[0];
    rsdsc.dsc$a_pointer = (char *) rslt; /* cast required */
    rsdsc.dsc$w_maxstrlen = VMS_MAXRSS + sizeof(unsigned short int);
    rsdsc.dsc$b_dtype = DSC$K_DTYPE_VT;
    rsdsc.dsc$b_class = DSC$K_CLASS_VS;

    Newx(vmsspec, VMS_MAXRSS, char);

	/* We could find out if there's an explicit dev/dir or version
	   by peeking into lib$find_file's internal context at
	   ((struct NAM *)((struct FAB *)cxt)->fab$l_nam)->nam$l_fnb
	   but that's unsupported, so I don't want to do it now and
	   have it bite someone in the future. */
	/* Fix-me: vms_split_path() is the only way to do this, the
	   existing method will fail with many legal EFS or UNIX specifications
	 */

    cp = SvPV(tmpglob,i);

    for (; i; i--) {
	if (cp[i] == ';') hasver = 1;
	if (cp[i] == '.') {
	    if (sts) hasver = 1;
	    else sts = 1;
	}
	if (cp[i] == '/') {
	    hasdir = isunix = 1;
	    break;
	}
	if (cp[i] == ']' || cp[i] == '>' || cp[i] == ':') {
	    hasdir = 1;
	    break;
	}
    }
    if ((tmpfp = PerlIO_tmpfile()) != NULL) {
	int found = 0;
	Stat_t st;
	int stat_sts;
	stat_sts = PerlLIO_stat(SvPVX_const(tmpglob),&st);
	if (!stat_sts && S_ISDIR(st.st_mode)) {
	    wilddsc.dsc$a_pointer = tovmspath_utf8(SvPVX(tmpglob),vmsspec,NULL);
	    ok = (wilddsc.dsc$a_pointer != NULL);
	    /* maybe passed 'foo' rather than '[.foo]', thus not detected above */
	    hasdir = 1; 
	}
	else {
	    wilddsc.dsc$a_pointer = tovmsspec_utf8(SvPVX(tmpglob),vmsspec,NULL);
	    ok = (wilddsc.dsc$a_pointer != NULL);
	}
	if (ok)
	    wilddsc.dsc$w_length = strlen(wilddsc.dsc$a_pointer);

	/* If not extended character set, replace ? with % */
	/* With extended character set, ? is a wildcard single character */
	if (!decc_efs_case_preserve) {
	    for (cp=wilddsc.dsc$a_pointer; ok && cp && *cp; cp++)
	        if (*cp == '?') *cp = '%';
	}
	sts = SS$_NORMAL;
	while (ok && $VMS_STATUS_SUCCESS(sts)) {
	 char * v_spec, * r_spec, * d_spec, * n_spec, * e_spec, * vs_spec;
	 int v_sts, v_len, r_len, d_len, n_len, e_len, vs_len;

	    sts = lib$find_file(&wilddsc,&rsdsc,&cxt,
				&dfltdsc,NULL,&rms_sts,&lff_flags);
	    if (!$VMS_STATUS_SUCCESS(sts))
		break;

	    found++;

	    /* with varying string, 1st word of buffer contains result length */
	    rstr[rslt->length] = '\0';

	     /* Find where all the components are */
	     v_sts = vms_split_path
		       (rstr,
			&v_spec,
			&v_len,
			&r_spec,
			&r_len,
			&d_spec,
			&d_len,
			&n_spec,
			&n_len,
			&e_spec,
			&e_len,
			&vs_spec,
			&vs_len);

	    /* If no version on input, truncate the version on output */
	    if (!hasver && (vs_len > 0)) {
		*vs_spec = '\0';
		vs_len = 0;

		/* No version & a null extension on UNIX handling */
		if (isunix && (e_len == 1) && decc_readdir_dropdotnotype) {
		    e_len = 0;
		    *e_spec = '\0';
		}
	    }

	    if (!decc_efs_case_preserve) {
	        for (cp = rstr; *cp; cp++) *cp = _tolower(*cp);
	    }

	    if (hasdir) {
		if (isunix) trim_unixpath(rstr,SvPVX(tmpglob),1);
		begin = rstr;
	    }
	    else {
		/* Start with the name */
		begin = n_spec;
	    }
	    strcat(begin,"\n");
	    ok = (PerlIO_puts(tmpfp,begin) != EOF);
	}
	if (cxt) (void)lib$find_file_end(&cxt);

	if (!found) {
	    /* Be POSIXish: return the input pattern when no matches */
	    strcpy(rstr,SvPVX(tmpglob));
	    strcat(rstr,"\n");
	    ok = (PerlIO_puts(tmpfp,rstr) != EOF);
	}

	if (ok && sts != RMS$_NMF &&
	    sts != RMS$_DNF && sts != RMS_FNF) ok = 0;
	if (!ok) {
	    if (!(sts & 1)) {
		SETERRNO((sts == RMS$_SYN ? EINVAL : EVMSERR),sts);
	    }
	    PerlIO_close(tmpfp);
	    fp = NULL;
	}
	else {
	    PerlIO_rewind(tmpfp);
	    IoTYPE(io) = IoTYPE_RDONLY;
	    IoIFP(io) = fp = tmpfp;
	}
    }
    Safefree(vmsspec);
    Safefree(rslt);
    return fp;
}


static char *
mp_do_vms_realpath(pTHX_ const char *filespec, char * rslt_spec,
		   int *utf8_fl);

void
unixrealpath_fromperl(pTHX_ CV *cv)
{
    dXSARGS;
    char *fspec, *rslt_spec, *rslt;
    STRLEN n_a;

    if (!items || items != 1)
	Perl_croak(aTHX_ "Usage: VMS::Filespec::unixrealpath(spec)");

    fspec = SvPV(ST(0),n_a);
    if (!fspec || !*fspec) XSRETURN_UNDEF;

    Newx(rslt_spec, VMS_MAXRSS + 1, char);
    rslt = do_vms_realpath(fspec, rslt_spec, NULL);

    ST(0) = sv_newmortal();
    if (rslt != NULL)
	sv_usepvn(ST(0),rslt,strlen(rslt));
    else
	Safefree(rslt_spec);
	XSRETURN(1);
}

static char *
mp_do_vms_realname(pTHX_ const char *filespec, char * rslt_spec,
		   int *utf8_fl);

void
vmsrealpath_fromperl(pTHX_ CV *cv)
{
    dXSARGS;
    char *fspec, *rslt_spec, *rslt;
    STRLEN n_a;

    if (!items || items != 1)
	Perl_croak(aTHX_ "Usage: VMS::Filespec::vmsrealpath(spec)");

    fspec = SvPV(ST(0),n_a);
    if (!fspec || !*fspec) XSRETURN_UNDEF;

    Newx(rslt_spec, VMS_MAXRSS + 1, char);
    rslt = do_vms_realname(fspec, rslt_spec, NULL);

    ST(0) = sv_newmortal();
    if (rslt != NULL)
	sv_usepvn(ST(0),rslt,strlen(rslt));
    else
	Safefree(rslt_spec);
	XSRETURN(1);
}

#ifdef HAS_SYMLINK
/*
 * A thin wrapper around decc$symlink to make sure we follow the 
 * standard and do not create a symlink with a zero-length name.
 */
/*{{{ int my_symlink(const char *path1, const char *path2)*/
int my_symlink(const char *path1, const char *path2) {
  if (!path2 || !*path2) {
    SETERRNO(ENOENT, SS$_NOSUCHFILE);
    return -1;
  }
  return symlink(path1, path2);
}
/*}}}*/

#endif /* HAS_SYMLINK */

int do_vms_case_tolerant(void);

void
case_tolerant_process_fromperl(pTHX_ CV *cv)
{
  dXSARGS;
  ST(0) = boolSV(do_vms_case_tolerant());
  XSRETURN(1);
}

void  
Perl_sys_intern_clear(pTHX)
{
}

void  
Perl_sys_intern_init(pTHX)
{
    unsigned int ix = RAND_MAX;
    double x;

    VMSISH_HUSHED = 0;

    /* fix me later to track running under GNV */
    /* this allows some limited testing */
    MY_POSIX_EXIT = decc_filename_unix_report;

    x = (float)ix;
    MY_INV_RAND_MAX = 1./x;
}

void
init_os_extras(void)
{
  dTHX;
  char* file = __FILE__;
  if (decc_disable_to_vms_logname_translation) {
    no_translate_barewords = TRUE;
  } else {
    no_translate_barewords = FALSE;
  }

  newXSproto("VMS::Filespec::rmsexpand",rmsexpand_fromperl,file,"$;$");
  newXSproto("VMS::Filespec::vmsify",vmsify_fromperl,file,"$");
  newXSproto("VMS::Filespec::unixify",unixify_fromperl,file,"$");
  newXSproto("VMS::Filespec::pathify",pathify_fromperl,file,"$");
  newXSproto("VMS::Filespec::fileify",fileify_fromperl,file,"$");
  newXSproto("VMS::Filespec::vmspath",vmspath_fromperl,file,"$");
  newXSproto("VMS::Filespec::unixpath",unixpath_fromperl,file,"$");
  newXSproto("VMS::Filespec::candelete",candelete_fromperl,file,"$");
  newXSproto("DynaLoader::mod2fname", mod2fname, file, "$");
  newXS("File::Copy::rmscopy",rmscopy_fromperl,file);
  newXSproto("vmsish::hushed",hushexit_fromperl,file,";$");
  newXSproto("VMS::Filespec::unixrealpath",unixrealpath_fromperl,file,"$;$");
  newXSproto("VMS::Filespec::vmsrealpath",vmsrealpath_fromperl,file,"$;$");
  newXSproto("VMS::Filespec::case_tolerant_process",
      case_tolerant_process_fromperl,file,"");

  store_pipelocs(aTHX);         /* will redo any earlier attempts */

  return;
}
  
#if __CRTL_VER == 80200000
/* This missed getting in to the DECC SDK for 8.2 */
char *realpath(const char *file_name, char * resolved_name, ...);
#endif

/*{{{char *do_vms_realpath(const char *file_name, char *resolved_name)*/
/* wrapper for the realpath() function added with 8.2 RMS SYMLINK SDK.
 * The perl fallback routine to provide realpath() is not as efficient
 * on OpenVMS.
 */

/* Hack, use old stat() as fastest way of getting ino_t and device */
int decc$stat(const char *name, void * statbuf);


/* Realpath is fragile.  In 8.3 it does not work if the feature
 * DECC$POSIX_COMPLIANT_PATHNAMES is not enabled, even though symbolic
 * links are implemented in RMS, not the CRTL. It also can fail if the 
 * user does not have read/execute access to some of the directories.
 * So in order for Do What I Mean mode to work, if realpath() fails,
 * fall back to looking up the filename by the device name and FID.
 */

int vms_fid_to_name(char * outname, int outlen, const char * name)
{
struct statbuf_t {
    char	   * st_dev;
    unsigned short st_ino[3];
    unsigned short padw;
    unsigned long  padl[30];  /* plenty of room */
} statbuf;
int sts;
struct dsc$descriptor_s dvidsc = {0, DSC$K_DTYPE_T, DSC$K_CLASS_S, 0};
struct dsc$descriptor_s specdsc = {0, DSC$K_DTYPE_T, DSC$K_CLASS_S, 0};

    sts = decc$stat(name, &statbuf);
    if (sts == 0) {

	dvidsc.dsc$a_pointer=statbuf.st_dev;
       dvidsc.dsc$w_length=strlen(statbuf.st_dev);

	specdsc.dsc$a_pointer = outname;
	specdsc.dsc$w_length = outlen-1;

       sts = lib$fid_to_name
	    (&dvidsc, statbuf.st_ino, &specdsc, &specdsc.dsc$w_length);
       if ($VMS_STATUS_SUCCESS(sts)) {
	    outname[specdsc.dsc$w_length] = 0;
	    return 0;
	}
    }
    return sts;
}



static char *
mp_do_vms_realpath(pTHX_ const char *filespec, char *outbuf,
		   int *utf8_fl)
{
    char * rslt = NULL;

#ifdef HAS_SYMLINK
    if (decc_posix_compliant_pathnames > 0 ) {
	/* realpath currently only works if posix compliant pathnames are
	 * enabled.  It may start working when they are not, but in that
	 * case we still want the fallback behavior for backwards compatibility
	 */
        rslt = realpath(filespec, outbuf);
    }
#endif

    if (rslt == NULL) {
        char * vms_spec;
        char * v_spec, * r_spec, * d_spec, * n_spec, * e_spec, * vs_spec;
        int sts, v_len, r_len, d_len, n_len, e_len, vs_len;
        int file_len;

	/* Fall back to fid_to_name */

        Newx(vms_spec, VMS_MAXRSS + 1, char);

	sts = vms_fid_to_name(vms_spec, VMS_MAXRSS + 1, filespec);
	if (sts == 0) {


	    /* Now need to trim the version off */
	    sts = vms_split_path
		  (vms_spec,
		   &v_spec,
		   &v_len,
		   &r_spec,
		   &r_len,
		   &d_spec,
		   &d_len,
		   &n_spec,
		   &n_len,
		   &e_spec,
		   &e_len,
		   &vs_spec,
		   &vs_len);


		if (sts == 0) {
	            int haslower = 0;
	            const char *cp;

	            /* Trim off the version */
	            int file_len = v_len + r_len + d_len + n_len + e_len;
	            vms_spec[file_len] = 0;

	            /* The result is expected to be in UNIX format */
		    rslt = do_tounixspec(vms_spec, outbuf, 0, utf8_fl);

                    /* Downcase if input had any lower case letters and 
	             * case preservation is not in effect. 
	             */
	            if (!decc_efs_case_preserve) {
	                for (cp = filespec; *cp; cp++)
	                    if (islower(*cp)) { haslower = 1; break; }

	                if (haslower) __mystrtolower(rslt);
	            }
	        }
	}

        Safefree(vms_spec);
    }
    return rslt;
}

static char *
mp_do_vms_realname(pTHX_ const char *filespec, char *outbuf,
		   int *utf8_fl)
{
    char * v_spec, * r_spec, * d_spec, * n_spec, * e_spec, * vs_spec;
    int sts, v_len, r_len, d_len, n_len, e_len, vs_len;
    int file_len;

    /* Fall back to fid_to_name */

    sts = vms_fid_to_name(outbuf, VMS_MAXRSS + 1, filespec);
    if (sts != 0) {
	return NULL;
    }
    else {


	/* Now need to trim the version off */
	sts = vms_split_path
		  (outbuf,
		   &v_spec,
		   &v_len,
		   &r_spec,
		   &r_len,
		   &d_spec,
		   &d_len,
		   &n_spec,
		   &n_len,
		   &e_spec,
		   &e_len,
		   &vs_spec,
		   &vs_len);


	if (sts == 0) {
	    int haslower = 0;
	    const char *cp;

	    /* Trim off the version */
	    int file_len = v_len + r_len + d_len + n_len + e_len;
	    outbuf[file_len] = 0;

	    /* Downcase if input had any lower case letters and 
	     * case preservation is not in effect. 
	     */
	    if (!decc_efs_case_preserve) {
	        for (cp = filespec; *cp; cp++)
	            if (islower(*cp)) { haslower = 1; break; }

	        if (haslower) __mystrtolower(outbuf);
	    }
	}
    }
    return outbuf;
}


/*}}}*/
/* External entry points */
char *Perl_vms_realpath(pTHX_ const char *filespec, char *outbuf, int *utf8_fl)
{ return do_vms_realpath(filespec, outbuf, utf8_fl); }

char *Perl_vms_realname(pTHX_ const char *filespec, char *outbuf, int *utf8_fl)
{ return do_vms_realname(filespec, outbuf, utf8_fl); }

/* case_tolerant */

/*{{{int do_vms_case_tolerant(void)*/
/* OpenVMS provides a case sensitive implementation of ODS-5 and this is
 * controlled by a process setting.
 */
int do_vms_case_tolerant(void)
{
    return vms_process_case_tolerant;
}
/*}}}*/
/* External entry points */
#if __CRTL_VER >= 70301000 && !defined(__VAX)
int Perl_vms_case_tolerant(void)
{ return do_vms_case_tolerant(); }
#else
int Perl_vms_case_tolerant(void)
{ return vms_process_case_tolerant; }
#endif


 /* Start of DECC RTL Feature handling */

static int sys_trnlnm
   (const char * logname,
    char * value,
    int value_len)
{
    const $DESCRIPTOR(table_dsc, "LNM$FILE_DEV");
    const unsigned long attr = LNM$M_CASE_BLIND;
    struct dsc$descriptor_s name_dsc;
    int status;
    unsigned short result;
    struct itmlst_3 itlst[2] = {{value_len, LNM$_STRING, value, &result},
                                {0, 0, 0, 0}};

    name_dsc.dsc$w_length = strlen(logname);
    name_dsc.dsc$a_pointer = (char *)logname;
    name_dsc.dsc$b_dtype = DSC$K_DTYPE_T;
    name_dsc.dsc$b_class = DSC$K_CLASS_S;

    status = sys$trnlnm(&attr, &table_dsc, &name_dsc, 0, itlst);

    if ($VMS_STATUS_SUCCESS(status)) {

	 /* Null terminate and return the string */
	/*--------------------------------------*/
	value[result] = 0;
    }

    return status;
}

static int sys_crelnm
   (const char * logname,
    const char * value)
{
    int ret_val;
    const char * proc_table = "LNM$PROCESS_TABLE";
    struct dsc$descriptor_s proc_table_dsc;
    struct dsc$descriptor_s logname_dsc;
    struct itmlst_3 item_list[2];

    proc_table_dsc.dsc$a_pointer = (char *) proc_table;
    proc_table_dsc.dsc$w_length = strlen(proc_table);
    proc_table_dsc.dsc$b_dtype = DSC$K_DTYPE_T;
    proc_table_dsc.dsc$b_class = DSC$K_CLASS_S;

    logname_dsc.dsc$a_pointer = (char *) logname;
    logname_dsc.dsc$w_length = strlen(logname);
    logname_dsc.dsc$b_dtype = DSC$K_DTYPE_T;
    logname_dsc.dsc$b_class = DSC$K_CLASS_S;

    item_list[0].buflen = strlen(value);
    item_list[0].itmcode = LNM$_STRING;
    item_list[0].bufadr = (char *)value;
    item_list[0].retlen = NULL;

    item_list[1].buflen = 0;
    item_list[1].itmcode = 0;

    ret_val = sys$crelnm
		       (NULL,
			(const struct dsc$descriptor_s *)&proc_table_dsc,
			(const struct dsc$descriptor_s *)&logname_dsc,
			NULL,
			(const struct item_list_3 *) item_list);

    return ret_val;
}

/* C RTL Feature settings */

static int set_features
   (int (* init_coroutine)(int *, int *, void *),  /* Needs casts if used */
    int (* cli_routine)(void),	/* Not documented */
    void *image_info)		/* Not documented */
{
    int status;
    int s;
    int dflt;
    char* str;
    char val_str[10];
#if defined(JPI$_CASE_LOOKUP_PERM) && !defined(__VAX)
    const unsigned long int jpicode1 = JPI$_CASE_LOOKUP_PERM;
    const unsigned long int jpicode2 = JPI$_CASE_LOOKUP_IMAGE;
    unsigned long case_perm;
    unsigned long case_image;
#endif

    /* Allow an exception to bring Perl into the VMS debugger */
    vms_debug_on_exception = 0;
    status = sys_trnlnm("PERL_VMS_EXCEPTION_DEBUG", val_str, sizeof(val_str));
    if ($VMS_STATUS_SUCCESS(status)) {
       if ((val_str[0] == 'E') || (val_str[0] == '1') || (val_str[0] == 'T'))
	 vms_debug_on_exception = 1;
       else
	 vms_debug_on_exception = 0;
    }

    /* Create VTF-7 filenames from Unicode instead of UTF-8 */
    vms_vtf7_filenames = 0;
    status = sys_trnlnm("PERL_VMS_VTF7_FILENAMES", val_str, sizeof(val_str));
    if ($VMS_STATUS_SUCCESS(status)) {
       if ((val_str[0] == 'E') || (val_str[0] == '1') || (val_str[0] == 'T'))
	 vms_vtf7_filenames = 1;
       else
	 vms_vtf7_filenames = 0;
    }


    /* unlink all versions on unlink() or rename() */
    vms_unlink_all_versions = 0;
    status = sys_trnlnm
	("PERL_VMS_UNLINK_ALL_VERSIONS", val_str, sizeof(val_str));
    if ($VMS_STATUS_SUCCESS(status)) {
       if ((val_str[0] == 'E') || (val_str[0] == '1') || (val_str[0] == 'T'))
	 vms_unlink_all_versions = 1;
       else
	 vms_unlink_all_versions = 0;
    }

    /* Dectect running under GNV Bash or other UNIX like shell */
#if __CRTL_VER >= 70300000 && !defined(__VAX)
    gnv_unix_shell = 0;
    status = sys_trnlnm("GNV$UNIX_SHELL", val_str, sizeof(val_str));
    if ($VMS_STATUS_SUCCESS(status)) {
       if ((val_str[0] == 'E') || (val_str[0] == '1') || (val_str[0] == 'T')) {
	 gnv_unix_shell = 1;
	 set_feature_default("DECC$EFS_CASE_PRESERVE", 1);
	 set_feature_default("DECC$EFS_CHARSET", 1);
	 set_feature_default("DECC$FILENAME_UNIX_NO_VERSION", 1);
	 set_feature_default("DECC$FILENAME_UNIX_REPORT", 1);
	 set_feature_default("DECC$READDIR_DROPDOTNOTYPE", 1);
	 set_feature_default("DECC$DISABLE_POSIX_ROOT", 0);
	 vms_unlink_all_versions = 1;
       }
       else
	 gnv_unix_shell = 0;
    }
#endif

    /* hacks to see if known bugs are still present for testing */

    /* Readdir is returning filenames in VMS syntax always */
    decc_bug_readdir_efs1 = 1;
    status = sys_trnlnm("DECC_BUG_READDIR_EFS1", val_str, sizeof(val_str));
    if ($VMS_STATUS_SUCCESS(status)) {
       if ((val_str[0] == 'E') || (val_str[0] == '1') || (val_str[0] == 'T'))
         decc_bug_readdir_efs1 = 1;
       else
	 decc_bug_readdir_efs1 = 0;
    }

    /* PCP mode requires creating /dev/null special device file */
    decc_bug_devnull = 0;
    status = sys_trnlnm("DECC_BUG_DEVNULL", val_str, sizeof(val_str));
    if ($VMS_STATUS_SUCCESS(status)) {
       if ((val_str[0] == 'E') || (val_str[0] == '1') || (val_str[0] == 'T'))
          decc_bug_devnull = 1;
       else
	  decc_bug_devnull = 0;
    }

    /* fgetname returning a VMS name in UNIX mode */
    decc_bug_fgetname = 1;
    status = sys_trnlnm("DECC_BUG_FGETNAME", val_str, sizeof(val_str));
    if ($VMS_STATUS_SUCCESS(status)) {
      if ((val_str[0] == 'E') || (val_str[0] == '1') || (val_str[0] == 'T'))
	decc_bug_fgetname = 1;
      else
	decc_bug_fgetname = 0;
    }

    /* UNIX directory names with no paths are broken in a lot of places */
    decc_dir_barename = 1;
    status = sys_trnlnm("DECC_DIR_BARENAME", val_str, sizeof(val_str));
    if ($VMS_STATUS_SUCCESS(status)) {
      if ((val_str[0] == 'E') || (val_str[0] == '1') || (val_str[0] == 'T'))
	decc_dir_barename = 1;
      else
	decc_dir_barename = 0;
    }

#if __CRTL_VER >= 70300000 && !defined(__VAX)
    s = decc$feature_get_index("DECC$DISABLE_TO_VMS_LOGNAME_TRANSLATION");
    if (s >= 0) {
	decc_disable_to_vms_logname_translation = decc$feature_get_value(s, 1);
	if (decc_disable_to_vms_logname_translation < 0)
	    decc_disable_to_vms_logname_translation = 0;
    }

    s = decc$feature_get_index("DECC$EFS_CASE_PRESERVE");
    if (s >= 0) {
	decc_efs_case_preserve = decc$feature_get_value(s, 1);
	if (decc_efs_case_preserve < 0)
	    decc_efs_case_preserve = 0;
    }

    s = decc$feature_get_index("DECC$EFS_CHARSET");
    if (s >= 0) {
	decc_efs_charset = decc$feature_get_value(s, 1);
	if (decc_efs_charset < 0)
	    decc_efs_charset = 0;
    }

    s = decc$feature_get_index("DECC$FILENAME_UNIX_REPORT");
    if (s >= 0) {
	decc_filename_unix_report = decc$feature_get_value(s, 1);
	if (decc_filename_unix_report > 0)
	    decc_filename_unix_report = 1;
	else
	    decc_filename_unix_report = 0;
    }

    s = decc$feature_get_index("DECC$FILENAME_UNIX_ONLY");
    if (s >= 0) {
	decc_filename_unix_only = decc$feature_get_value(s, 1);
	if (decc_filename_unix_only > 0) {
	    decc_filename_unix_only = 1;
	}
	else {
	    decc_filename_unix_only = 0;
	}
    }

    s = decc$feature_get_index("DECC$FILENAME_UNIX_NO_VERSION");
    if (s >= 0) {
	decc_filename_unix_no_version = decc$feature_get_value(s, 1);
	if (decc_filename_unix_no_version < 0)
	    decc_filename_unix_no_version = 0;
    }

    s = decc$feature_get_index("DECC$READDIR_DROPDOTNOTYPE");
    if (s >= 0) {
	decc_readdir_dropdotnotype = decc$feature_get_value(s, 1);
	if (decc_readdir_dropdotnotype < 0)
	    decc_readdir_dropdotnotype = 0;
    }

    status = sys_trnlnm("SYS$POSIX_ROOT", val_str, sizeof(val_str));
    if ($VMS_STATUS_SUCCESS(status)) {
	s = decc$feature_get_index("DECC$DISABLE_POSIX_ROOT");
	if (s >= 0) {
	    dflt = decc$feature_get_value(s, 4);
	    if (dflt > 0) {
		decc_disable_posix_root = decc$feature_get_value(s, 1);
		if (decc_disable_posix_root <= 0) {
		    decc$feature_set_value(s, 1, 1);
		    decc_disable_posix_root = 1;
		}
	    }
	    else {
		/* Traditionally Perl assumes this is off */
		decc_disable_posix_root = 1;
		decc$feature_set_value(s, 1, 1);
	    }
	}
    }

#if __CRTL_VER >= 80200000
    s = decc$feature_get_index("DECC$POSIX_COMPLIANT_PATHNAMES");
    if (s >= 0) {
	decc_posix_compliant_pathnames = decc$feature_get_value(s, 1);
	if (decc_posix_compliant_pathnames < 0)
	    decc_posix_compliant_pathnames = 0;
	if (decc_posix_compliant_pathnames > 4)
	    decc_posix_compliant_pathnames = 0;
    }

#endif
#else
    status = sys_trnlnm
	("DECC$DISABLE_TO_VMS_LOGNAME_TRANSLATION", val_str, sizeof(val_str));
    if ($VMS_STATUS_SUCCESS(status)) {
	val_str[0] = _toupper(val_str[0]);
	if ((val_str[0] == 'E') || (val_str[0] == '1') || (val_str[0] == 'T')) {
	   decc_disable_to_vms_logname_translation = 1;
	}
    }

#ifndef __VAX
    status = sys_trnlnm("DECC$EFS_CASE_PRESERVE", val_str, sizeof(val_str));
    if ($VMS_STATUS_SUCCESS(status)) {
	val_str[0] = _toupper(val_str[0]);
	if ((val_str[0] == 'E') || (val_str[0] == '1') || (val_str[0] == 'T')) {
	   decc_efs_case_preserve = 1;
	}
    }
#endif

    status = sys_trnlnm("DECC$FILENAME_UNIX_REPORT", val_str, sizeof(val_str));
    if ($VMS_STATUS_SUCCESS(status)) {
	val_str[0] = _toupper(val_str[0]);
	if ((val_str[0] == 'E') || (val_str[0] == '1') || (val_str[0] == 'T')) {
	   decc_filename_unix_report = 1;
	}
    }
    status = sys_trnlnm("DECC$FILENAME_UNIX_ONLY", val_str, sizeof(val_str));
    if ($VMS_STATUS_SUCCESS(status)) {
	val_str[0] = _toupper(val_str[0]);
	if ((val_str[0] == 'E') || (val_str[0] == '1') || (val_str[0] == 'T')) {
	   decc_filename_unix_only = 1;
	   decc_filename_unix_report = 1;
	}
    }
    status = sys_trnlnm("DECC$FILENAME_UNIX_NO_VERSION", val_str, sizeof(val_str));
    if ($VMS_STATUS_SUCCESS(status)) {
	val_str[0] = _toupper(val_str[0]);
	if ((val_str[0] == 'E') || (val_str[0] == '1') || (val_str[0] == 'T')) {
	   decc_filename_unix_no_version = 1;
	}
    }
    status = sys_trnlnm("DECC$READDIR_DROPDOTNOTYPE", val_str, sizeof(val_str));
    if ($VMS_STATUS_SUCCESS(status)) {
	val_str[0] = _toupper(val_str[0]);
	if ((val_str[0] == 'E') || (val_str[0] == '1') || (val_str[0] == 'T')) {
	   decc_readdir_dropdotnotype = 1;
	}
    }
#endif

#if defined(JPI$_CASE_LOOKUP_PERM) && !defined(__VAX)

     /* Report true case tolerance */
    /*----------------------------*/
    status = lib$getjpi(&jpicode1, 0, 0, &case_perm, 0, 0);
    if (!$VMS_STATUS_SUCCESS(status))
	case_perm = PPROP$K_CASE_BLIND;
    status = lib$getjpi(&jpicode2, 0, 0, &case_image, 0, 0);
    if (!$VMS_STATUS_SUCCESS(status))
	case_image = PPROP$K_CASE_BLIND;
    if ((case_perm == PPROP$K_CASE_SENSITIVE) ||
	(case_image == PPROP$K_CASE_SENSITIVE))
	vms_process_case_tolerant = 0;

#endif


    /* CRTL can be initialized past this point, but not before. */
/*    DECC$CRTL_INIT(); */

    return SS$_NORMAL;
}

#ifdef __DECC
#pragma nostandard
#pragma extern_model save
#pragma extern_model strict_refdef "LIB$INITIALIZ" nowrt
	const __align (LONGWORD) int spare[8] = {0};

/* .psect LIB$INITIALIZE, NOPIC, USR, CON, REL, GBL, NOSHR, NOEXE, RD, NOWRT, LONG */
#if __DECC_VER >= 60560002
#pragma extern_model strict_refdef "LIB$INITIALIZE" nopic, con, rel, gbl, noshr, noexe, nowrt, long
#else
#pragma extern_model strict_refdef "LIB$INITIALIZE" nopic, con, gbl, noshr, nowrt, long
#endif
#endif /* __DECC */

const long vms_cc_features = (const long)set_features;

/*
** Force a reference to LIB$INITIALIZE to ensure it
** exists in the image.
*/
int lib$initialize(void);
#ifdef __DECC
#pragma extern_model strict_refdef
#endif
    int lib_init_ref = (int) lib$initialize;

#ifdef __DECC
#pragma extern_model restore
#pragma standard
#endif

/*  End of vms.c */
