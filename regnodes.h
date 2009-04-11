/* -*- buffer-read-only: t -*-
   !!!!!!!   DO NOT EDIT THIS FILE   !!!!!!!
   This file is built by regcomp.pl from regcomp.sym.
   Any changes made here will be lost!
*/

/* Regops and State definitions */

#define REGNODE_MAX           	74
#define REGMATCH_STATE_MAX    	114

#define	END                   	0	/* 0000 End of program. */
#define	SUCCEED               	1	/* 0x01 Return from a subroutine, basically. */
#define	BOL                   	2	/* 0x02 Match "" at beginning of line. */
#define	MBOL                  	3	/* 0x03 Same, assuming multiline. */
#define	SBOL                  	4	/* 0x04 Same, assuming singleline. */
#define	EOS                   	5	/* 0x05 Match "" at end of string. */
#define	EOL                   	6	/* 0x06 Match "" at end of line. */
#define	MEOL                  	7	/* 0x07 Same, assuming multiline. */
#define	SEOL                  	8	/* 0x08 Same, assuming singleline. */
#define	BOUND                 	9	/* 0x09 Match "" at any word boundary */
#define	BOUNDL                	10	/* 0x0a Match "" at any word boundary */
#define	NBOUND                	11	/* 0x0b Match "" at any word non-boundary */
#define	NBOUNDL               	12	/* 0x0c Match "" at any word non-boundary */
#define	GPOS                  	13	/* 0x0d Matches where last m//g left off. */
#define	REG_ANY               	14	/* 0x0e Match any one character (except newline). */
#define	REG_ANYU              	15	/* 0x0f Match any one unicode character (except newline). */
#define	SANY                  	16	/* 0x10 Match any one character. */
#define	CANY                  	17	/* 0x11 Match any one byte. */
#define	ANYOF                 	18	/* 0x12 Match character in (or not in) this class. */
#define	ANYOFU                	19	/* 0x13 Match unicode character in (or not in) this class. */
#define	CLUMP                 	20	/* 0x14 Match any combining character sequence */
#define	BRANCH                	21	/* 0x15 Match this alternative, or the next... */
#define	BACK                  	22	/* 0x16 Match "", "next" ptr points backward. */
#define	EXACT                 	23	/* 0x17 Match this string (preceded by length). */
#define	NOTHING               	24	/* 0x18 Match empty string. */
#define	TAIL                  	25	/* 0x19 Match empty string. Can jump here from outside. */
#define	STAR                  	26	/* 0x1a Match this (simple) thing 0 or more times. */
#define	PLUS                  	27	/* 0x1b Match this (simple) thing 1 or more times. */
#define	CURLY                 	28	/* 0x1c Match this simple thing {n,m} times. */
#define	CURLYN                	29	/* 0x1d Capture next-after-this simple thing */
#define	CURLYM                	30	/* 0x1e Capture this medium-complex thing {n,m} times. */
#define	CURLYX                	31	/* 0x1f Match this complex thing {n,m} times. */
#define	WHILEM                	32	/* 0x20 Do curly processing and see if rest matches. */
#define	OPEN                  	33	/* 0x21 Mark this point in input as start of */
#define	CLOSE                 	34	/* 0x22 Analogous to OPEN. */
#define	REF                   	35	/* 0x23 Match some already matched string */
#define	REFF                  	36	/* 0x24 Match already matched string, folded */
#define	REFFL                 	37	/* 0x25 Match already matched string, folded in loc. */
#define	IFMATCH               	38	/* 0x26 Succeeds if the following matches. */
#define	UNLESSM               	39	/* 0x27 Fails if the following matches. */
#define	SUSPEND               	40	/* 0x28 "Independent" sub-RE. */
#define	IFTHEN                	41	/* 0x29 Switch, should be preceeded by switcher . */
#define	GROUPP                	42	/* 0x2a Whether the group matched. */
#define	LONGJMP               	43	/* 0x2b Jump far away. */
#define	BRANCHJ               	44	/* 0x2c BRANCH with long offset. */
#define	EVAL                  	45	/* 0x2d Execute some Perl code. */
#define	MINMOD                	46	/* 0x2e Next operator is not greedy. */
#define	LOGICAL               	47	/* 0x2f Next opcode should set the flag only. */
#define	RENUM                 	48	/* 0x30 Group with independently numbered parens. */
#define	TRIE                  	49	/* 0x31 Match many EXACT(FL?)? at once. flags==type */
#define	TRIEC                 	50	/* 0x32 Same as TRIE, but with embedded charclass data */
#define	AHOCORASICK           	51	/* 0x33 Aho Corasick stclass. flags==type */
#define	AHOCORASICKC          	52	/* 0x34 Same as AHOCORASICK, but with embedded charclass data */
#define	GOSUB                 	53	/* 0x35 recurse to paren arg1 at (signed) ofs arg2 */
#define	GOSTART               	54	/* 0x36 recurse to start of pattern */
#define	NREF                  	55	/* 0x37 Match some already matched string */
#define	NREFF                 	56	/* 0x38 Match already matched string, folded */
#define	NREFFL                	57	/* 0x39 Match already matched string, folded in loc. */
#define	NGROUPP               	58	/* 0x3a Whether the group matched. */
#define	INSUBP                	59	/* 0x3b Whether we are in a specific recurse. */
#define	DEFINEP               	60	/* 0x3c Never execute directly. */
#define	ENDLIKE               	61	/* 0x3d Used only for the type field of verbs */
#define	OPFAIL                	62	/* 0x3e Same as (?!) */
#define	ACCEPT                	63	/* 0x3f Accepts the current matched string. */
#define	VERB                  	64	/* 0x40    no-sv 1	Used only for the type field of verbs */
#define	PRUNE                 	65	/* 0x41 Pattern fails at this startpoint if no-backtracking through this */
#define	MARKPOINT             	66	/* 0x42 Push the current location for rollback by cut. */
#define	SKIP                  	67	/* 0x43 On failure skip forward (to the mark) before retrying */
#define	COMMIT                	68	/* 0x44 Pattern fails outright if backtracking through this */
#define	CUTGROUP              	69	/* 0x45 On failure go to the next alternation in the group */
#define	KEEPS                 	70	/* 0x46 $& begins here. */
#define	LNBREAK               	71	/* 0x47 generic newline pattern */
#define	FOLDCHAR              	72	/* 0x48 codepoint with tricky case folding properties. */
#define	OPTIMIZED             	73	/* 0x49 Placeholder for dump. */
#define	PSEUDO                	74	/* 0x4a Pseudo opcode for internal use. */
	/* ------------ States ------------- */
#define	TRIE_next             	(REGNODE_MAX + 1)	/* state for TRIE */
#define	TRIE_next_fail        	(REGNODE_MAX + 2)	/* state for TRIE */
#define	EVAL_AB               	(REGNODE_MAX + 3)	/* state for EVAL */
#define	EVAL_AB_fail          	(REGNODE_MAX + 4)	/* state for EVAL */
#define	CURLYX_end            	(REGNODE_MAX + 5)	/* state for CURLYX */
#define	CURLYX_end_fail       	(REGNODE_MAX + 6)	/* state for CURLYX */
#define	WHILEM_A_pre          	(REGNODE_MAX + 7)	/* state for WHILEM */
#define	WHILEM_A_pre_fail     	(REGNODE_MAX + 8)	/* state for WHILEM */
#define	WHILEM_A_min          	(REGNODE_MAX + 9)	/* state for WHILEM */
#define	WHILEM_A_min_fail     	(REGNODE_MAX + 10)	/* state for WHILEM */
#define	WHILEM_A_max          	(REGNODE_MAX + 11)	/* state for WHILEM */
#define	WHILEM_A_max_fail     	(REGNODE_MAX + 12)	/* state for WHILEM */
#define	WHILEM_B_min          	(REGNODE_MAX + 13)	/* state for WHILEM */
#define	WHILEM_B_min_fail     	(REGNODE_MAX + 14)	/* state for WHILEM */
#define	WHILEM_B_max          	(REGNODE_MAX + 15)	/* state for WHILEM */
#define	WHILEM_B_max_fail     	(REGNODE_MAX + 16)	/* state for WHILEM */
#define	BRANCH_next           	(REGNODE_MAX + 17)	/* state for BRANCH */
#define	BRANCH_next_fail      	(REGNODE_MAX + 18)	/* state for BRANCH */
#define	CURLYM_A              	(REGNODE_MAX + 19)	/* state for CURLYM */
#define	CURLYM_A_fail         	(REGNODE_MAX + 20)	/* state for CURLYM */
#define	CURLYM_B              	(REGNODE_MAX + 21)	/* state for CURLYM */
#define	CURLYM_B_fail         	(REGNODE_MAX + 22)	/* state for CURLYM */
#define	IFMATCH_A             	(REGNODE_MAX + 23)	/* state for IFMATCH */
#define	IFMATCH_A_fail        	(REGNODE_MAX + 24)	/* state for IFMATCH */
#define	CURLY_B_min_known     	(REGNODE_MAX + 25)	/* state for CURLY */
#define	CURLY_B_min_known_fail	(REGNODE_MAX + 26)	/* state for CURLY */
#define	CURLY_B_min           	(REGNODE_MAX + 27)	/* state for CURLY */
#define	CURLY_B_min_fail      	(REGNODE_MAX + 28)	/* state for CURLY */
#define	CURLY_B_max           	(REGNODE_MAX + 29)	/* state for CURLY */
#define	CURLY_B_max_fail      	(REGNODE_MAX + 30)	/* state for CURLY */
#define	COMMIT_next           	(REGNODE_MAX + 31)	/* state for COMMIT */
#define	COMMIT_next_fail      	(REGNODE_MAX + 32)	/* state for COMMIT */
#define	MARKPOINT_next        	(REGNODE_MAX + 33)	/* state for MARKPOINT */
#define	MARKPOINT_next_fail   	(REGNODE_MAX + 34)	/* state for MARKPOINT */
#define	SKIP_next             	(REGNODE_MAX + 35)	/* state for SKIP */
#define	SKIP_next_fail        	(REGNODE_MAX + 36)	/* state for SKIP */
#define	CUTGROUP_next         	(REGNODE_MAX + 37)	/* state for CUTGROUP */
#define	CUTGROUP_next_fail    	(REGNODE_MAX + 38)	/* state for CUTGROUP */
#define	KEEPS_next            	(REGNODE_MAX + 39)	/* state for KEEPS */
#define	KEEPS_next_fail       	(REGNODE_MAX + 40)	/* state for KEEPS */

/* PL_regkind[] What type of regop or state is this. */

#ifndef DOINIT
EXTCONST U8 PL_regkind[];
#else
EXTCONST U8 PL_regkind[] = {
	END,      	/* END                    */
	END,      	/* SUCCEED                */
	BOL,      	/* BOL                    */
	BOL,      	/* MBOL                   */
	BOL,      	/* SBOL                   */
	EOL,      	/* EOS                    */
	EOL,      	/* EOL                    */
	EOL,      	/* MEOL                   */
	EOL,      	/* SEOL                   */
	BOUND,    	/* BOUND                  */
	BOUND,    	/* BOUNDL                 */
	NBOUND,   	/* NBOUND                 */
	NBOUND,   	/* NBOUNDL                */
	GPOS,     	/* GPOS                   */
	REG_ANY,  	/* REG_ANY                */
	REG_ANY,  	/* REG_ANYU               */
	REG_ANY,  	/* SANY                   */
	REG_ANY,  	/* CANY                   */
	ANYOF,    	/* ANYOF                  */
	ANYOF,    	/* ANYOFU                 */
	CLUMP,    	/* CLUMP                  */
	BRANCH,   	/* BRANCH                 */
	BACK,     	/* BACK                   */
	EXACT,    	/* EXACT                  */
	NOTHING,  	/* NOTHING                */
	NOTHING,  	/* TAIL                   */
	STAR,     	/* STAR                   */
	PLUS,     	/* PLUS                   */
	CURLY,    	/* CURLY                  */
	CURLY,    	/* CURLYN                 */
	CURLY,    	/* CURLYM                 */
	CURLY,    	/* CURLYX                 */
	WHILEM,   	/* WHILEM                 */
	OPEN,     	/* OPEN                   */
	CLOSE,    	/* CLOSE                  */
	REF,      	/* REF                    */
	REF,      	/* REFF                   */
	REF,      	/* REFFL                  */
	BRANCHJ,  	/* IFMATCH                */
	BRANCHJ,  	/* UNLESSM                */
	BRANCHJ,  	/* SUSPEND                */
	BRANCHJ,  	/* IFTHEN                 */
	GROUPP,   	/* GROUPP                 */
	LONGJMP,  	/* LONGJMP                */
	BRANCHJ,  	/* BRANCHJ                */
	EVAL,     	/* EVAL                   */
	MINMOD,   	/* MINMOD                 */
	LOGICAL,  	/* LOGICAL                */
	BRANCHJ,  	/* RENUM                  */
	TRIE,     	/* TRIE                   */
	TRIE,     	/* TRIEC                  */
	TRIE,     	/* AHOCORASICK            */
	TRIE,     	/* AHOCORASICKC           */
	GOSUB,    	/* GOSUB                  */
	GOSTART,  	/* GOSTART                */
	REF,      	/* NREF                   */
	REF,      	/* NREFF                  */
	REF,      	/* NREFFL                 */
	NGROUPP,  	/* NGROUPP                */
	INSUBP,   	/* INSUBP                 */
	DEFINEP,  	/* DEFINEP                */
	ENDLIKE,  	/* ENDLIKE                */
	ENDLIKE,  	/* OPFAIL                 */
	ENDLIKE,  	/* ACCEPT                 */
	VERB,     	/* VERB                   */
	VERB,     	/* PRUNE                  */
	VERB,     	/* MARKPOINT              */
	VERB,     	/* SKIP                   */
	VERB,     	/* COMMIT                 */
	VERB,     	/* CUTGROUP               */
	KEEPS,    	/* KEEPS                  */
	LNBREAK,  	/* LNBREAK                */
	FOLDCHAR, 	/* FOLDCHAR               */
	NOTHING,  	/* OPTIMIZED              */
	PSEUDO,   	/* PSEUDO                 */
	/* ------------ States ------------- */
	TRIE,     	/* TRIE_next              */
	TRIE,     	/* TRIE_next_fail         */
	EVAL,     	/* EVAL_AB                */
	EVAL,     	/* EVAL_AB_fail           */
	CURLYX,   	/* CURLYX_end             */
	CURLYX,   	/* CURLYX_end_fail        */
	WHILEM,   	/* WHILEM_A_pre           */
	WHILEM,   	/* WHILEM_A_pre_fail      */
	WHILEM,   	/* WHILEM_A_min           */
	WHILEM,   	/* WHILEM_A_min_fail      */
	WHILEM,   	/* WHILEM_A_max           */
	WHILEM,   	/* WHILEM_A_max_fail      */
	WHILEM,   	/* WHILEM_B_min           */
	WHILEM,   	/* WHILEM_B_min_fail      */
	WHILEM,   	/* WHILEM_B_max           */
	WHILEM,   	/* WHILEM_B_max_fail      */
	BRANCH,   	/* BRANCH_next            */
	BRANCH,   	/* BRANCH_next_fail       */
	CURLYM,   	/* CURLYM_A               */
	CURLYM,   	/* CURLYM_A_fail          */
	CURLYM,   	/* CURLYM_B               */
	CURLYM,   	/* CURLYM_B_fail          */
	IFMATCH,  	/* IFMATCH_A              */
	IFMATCH,  	/* IFMATCH_A_fail         */
	CURLY,    	/* CURLY_B_min_known      */
	CURLY,    	/* CURLY_B_min_known_fail */
	CURLY,    	/* CURLY_B_min            */
	CURLY,    	/* CURLY_B_min_fail       */
	CURLY,    	/* CURLY_B_max            */
	CURLY,    	/* CURLY_B_max_fail       */
	COMMIT,   	/* COMMIT_next            */
	COMMIT,   	/* COMMIT_next_fail       */
	MARKPOINT,	/* MARKPOINT_next         */
	MARKPOINT,	/* MARKPOINT_next_fail    */
	SKIP,     	/* SKIP_next              */
	SKIP,     	/* SKIP_next_fail         */
	CUTGROUP, 	/* CUTGROUP_next          */
	CUTGROUP, 	/* CUTGROUP_next_fail     */
	KEEPS,    	/* KEEPS_next             */
	KEEPS,    	/* KEEPS_next_fail        */
};
#endif

/* regarglen[] - How large is the argument part of the node (in regnodes) */

#ifdef REG_COMP_C
static const U8 regarglen[] = {
	0,                                   	/* END          */
	0,                                   	/* SUCCEED      */
	0,                                   	/* BOL          */
	0,                                   	/* MBOL         */
	0,                                   	/* SBOL         */
	0,                                   	/* EOS          */
	0,                                   	/* EOL          */
	0,                                   	/* MEOL         */
	0,                                   	/* SEOL         */
	0,                                   	/* BOUND        */
	0,                                   	/* BOUNDL       */
	0,                                   	/* NBOUND       */
	0,                                   	/* NBOUNDL      */
	0,                                   	/* GPOS         */
	0,                                   	/* REG_ANY      */
	0,                                   	/* REG_ANYU     */
	0,                                   	/* SANY         */
	0,                                   	/* CANY         */
	0,                                   	/* ANYOF        */
	0,                                   	/* ANYOFU       */
	0,                                   	/* CLUMP        */
	0,                                   	/* BRANCH       */
	0,                                   	/* BACK         */
	0,                                   	/* EXACT        */
	0,                                   	/* NOTHING      */
	0,                                   	/* TAIL         */
	0,                                   	/* STAR         */
	0,                                   	/* PLUS         */
	EXTRA_SIZE(struct regnode_2),        	/* CURLY        */
	EXTRA_SIZE(struct regnode_2),        	/* CURLYN       */
	EXTRA_SIZE(struct regnode_2),        	/* CURLYM       */
	EXTRA_SIZE(struct regnode_2),        	/* CURLYX       */
	0,                                   	/* WHILEM       */
	EXTRA_SIZE(struct regnode_1),        	/* OPEN         */
	EXTRA_SIZE(struct regnode_1),        	/* CLOSE        */
	EXTRA_SIZE(struct regnode_1),        	/* REF          */
	EXTRA_SIZE(struct regnode_1),        	/* REFF         */
	EXTRA_SIZE(struct regnode_1),        	/* REFFL        */
	EXTRA_SIZE(struct regnode_1),        	/* IFMATCH      */
	EXTRA_SIZE(struct regnode_1),        	/* UNLESSM      */
	EXTRA_SIZE(struct regnode_1),        	/* SUSPEND      */
	EXTRA_SIZE(struct regnode_1),        	/* IFTHEN       */
	EXTRA_SIZE(struct regnode_1),        	/* GROUPP       */
	EXTRA_SIZE(struct regnode_1),        	/* LONGJMP      */
	EXTRA_SIZE(struct regnode_1),        	/* BRANCHJ      */
	EXTRA_SIZE(struct regnode_1),        	/* EVAL         */
	0,                                   	/* MINMOD       */
	0,                                   	/* LOGICAL      */
	EXTRA_SIZE(struct regnode_1),        	/* RENUM        */
	EXTRA_SIZE(struct regnode_1),        	/* TRIE         */
	EXTRA_SIZE(struct regnode_charclass),	/* TRIEC        */
	EXTRA_SIZE(struct regnode_1),        	/* AHOCORASICK  */
	EXTRA_SIZE(struct regnode_charclass),	/* AHOCORASICKC */
	EXTRA_SIZE(struct regnode_2L),       	/* GOSUB        */
	0,                                   	/* GOSTART      */
	EXTRA_SIZE(struct regnode_1),        	/* NREF         */
	EXTRA_SIZE(struct regnode_1),        	/* NREFF        */
	EXTRA_SIZE(struct regnode_1),        	/* NREFFL       */
	EXTRA_SIZE(struct regnode_1),        	/* NGROUPP      */
	EXTRA_SIZE(struct regnode_1),        	/* INSUBP       */
	EXTRA_SIZE(struct regnode_1),        	/* DEFINEP      */
	0,                                   	/* ENDLIKE      */
	0,                                   	/* OPFAIL       */
	EXTRA_SIZE(struct regnode_1),        	/* ACCEPT       */
	0,                                   	/* VERB         */
	EXTRA_SIZE(struct regnode_1),        	/* PRUNE        */
	EXTRA_SIZE(struct regnode_1),        	/* MARKPOINT    */
	EXTRA_SIZE(struct regnode_1),        	/* SKIP         */
	EXTRA_SIZE(struct regnode_1),        	/* COMMIT       */
	EXTRA_SIZE(struct regnode_1),        	/* CUTGROUP     */
	0,                                   	/* KEEPS        */
	0,                                   	/* LNBREAK      */
	EXTRA_SIZE(struct regnode_1),        	/* FOLDCHAR     */
	0,                                   	/* OPTIMIZED    */
	0,                                   	/* PSEUDO       */
};

/* reg_off_by_arg[] - Which argument holds the offset to the next node */

static const char reg_off_by_arg[] = {
	0,	/* END          */
	0,	/* SUCCEED      */
	0,	/* BOL          */
	0,	/* MBOL         */
	0,	/* SBOL         */
	0,	/* EOS          */
	0,	/* EOL          */
	0,	/* MEOL         */
	0,	/* SEOL         */
	0,	/* BOUND        */
	0,	/* BOUNDL       */
	0,	/* NBOUND       */
	0,	/* NBOUNDL      */
	0,	/* GPOS         */
	0,	/* REG_ANY      */
	0,	/* REG_ANYU     */
	0,	/* SANY         */
	0,	/* CANY         */
	0,	/* ANYOF        */
	0,	/* ANYOFU       */
	0,	/* CLUMP        */
	0,	/* BRANCH       */
	0,	/* BACK         */
	0,	/* EXACT        */
	0,	/* NOTHING      */
	0,	/* TAIL         */
	0,	/* STAR         */
	0,	/* PLUS         */
	0,	/* CURLY        */
	0,	/* CURLYN       */
	0,	/* CURLYM       */
	0,	/* CURLYX       */
	0,	/* WHILEM       */
	0,	/* OPEN         */
	0,	/* CLOSE        */
	0,	/* REF          */
	0,	/* REFF         */
	0,	/* REFFL        */
	2,	/* IFMATCH      */
	2,	/* UNLESSM      */
	1,	/* SUSPEND      */
	1,	/* IFTHEN       */
	0,	/* GROUPP       */
	1,	/* LONGJMP      */
	1,	/* BRANCHJ      */
	0,	/* EVAL         */
	0,	/* MINMOD       */
	0,	/* LOGICAL      */
	1,	/* RENUM        */
	0,	/* TRIE         */
	0,	/* TRIEC        */
	0,	/* AHOCORASICK  */
	0,	/* AHOCORASICKC */
	0,	/* GOSUB        */
	0,	/* GOSTART      */
	0,	/* NREF         */
	0,	/* NREFF        */
	0,	/* NREFFL       */
	0,	/* NGROUPP      */
	0,	/* INSUBP       */
	0,	/* DEFINEP      */
	0,	/* ENDLIKE      */
	0,	/* OPFAIL       */
	0,	/* ACCEPT       */
	0,	/* VERB         */
	0,	/* PRUNE        */
	0,	/* MARKPOINT    */
	0,	/* SKIP         */
	0,	/* COMMIT       */
	0,	/* CUTGROUP     */
	0,	/* KEEPS        */
	0,	/* LNBREAK      */
	0,	/* FOLDCHAR     */
	0,	/* OPTIMIZED    */
	0,	/* PSEUDO       */
};

#endif /* REG_COMP_C */

/* reg_name[] - Opcode/state names in string form, for debugging */

#ifndef DOINIT
EXTCONST char * PL_reg_name[];
#else
EXTCONST char * const PL_reg_name[] = {
	"END",                   	/* 0000 */
	"SUCCEED",               	/* 0x01 */
	"BOL",                   	/* 0x02 */
	"MBOL",                  	/* 0x03 */
	"SBOL",                  	/* 0x04 */
	"EOS",                   	/* 0x05 */
	"EOL",                   	/* 0x06 */
	"MEOL",                  	/* 0x07 */
	"SEOL",                  	/* 0x08 */
	"BOUND",                 	/* 0x09 */
	"BOUNDL",                	/* 0x0a */
	"NBOUND",                	/* 0x0b */
	"NBOUNDL",               	/* 0x0c */
	"GPOS",                  	/* 0x0d */
	"REG_ANY",               	/* 0x0e */
	"REG_ANYU",              	/* 0x0f */
	"SANY",                  	/* 0x10 */
	"CANY",                  	/* 0x11 */
	"ANYOF",                 	/* 0x12 */
	"ANYOFU",                	/* 0x13 */
	"CLUMP",                 	/* 0x14 */
	"BRANCH",                	/* 0x15 */
	"BACK",                  	/* 0x16 */
	"EXACT",                 	/* 0x17 */
	"NOTHING",               	/* 0x18 */
	"TAIL",                  	/* 0x19 */
	"STAR",                  	/* 0x1a */
	"PLUS",                  	/* 0x1b */
	"CURLY",                 	/* 0x1c */
	"CURLYN",                	/* 0x1d */
	"CURLYM",                	/* 0x1e */
	"CURLYX",                	/* 0x1f */
	"WHILEM",                	/* 0x20 */
	"OPEN",                  	/* 0x21 */
	"CLOSE",                 	/* 0x22 */
	"REF",                   	/* 0x23 */
	"REFF",                  	/* 0x24 */
	"REFFL",                 	/* 0x25 */
	"IFMATCH",               	/* 0x26 */
	"UNLESSM",               	/* 0x27 */
	"SUSPEND",               	/* 0x28 */
	"IFTHEN",                	/* 0x29 */
	"GROUPP",                	/* 0x2a */
	"LONGJMP",               	/* 0x2b */
	"BRANCHJ",               	/* 0x2c */
	"EVAL",                  	/* 0x2d */
	"MINMOD",                	/* 0x2e */
	"LOGICAL",               	/* 0x2f */
	"RENUM",                 	/* 0x30 */
	"TRIE",                  	/* 0x31 */
	"TRIEC",                 	/* 0x32 */
	"AHOCORASICK",           	/* 0x33 */
	"AHOCORASICKC",          	/* 0x34 */
	"GOSUB",                 	/* 0x35 */
	"GOSTART",               	/* 0x36 */
	"NREF",                  	/* 0x37 */
	"NREFF",                 	/* 0x38 */
	"NREFFL",                	/* 0x39 */
	"NGROUPP",               	/* 0x3a */
	"INSUBP",                	/* 0x3b */
	"DEFINEP",               	/* 0x3c */
	"ENDLIKE",               	/* 0x3d */
	"OPFAIL",                	/* 0x3e */
	"ACCEPT",                	/* 0x3f */
	"VERB",                  	/* 0x40 */
	"PRUNE",                 	/* 0x41 */
	"MARKPOINT",             	/* 0x42 */
	"SKIP",                  	/* 0x43 */
	"COMMIT",                	/* 0x44 */
	"CUTGROUP",              	/* 0x45 */
	"KEEPS",                 	/* 0x46 */
	"LNBREAK",               	/* 0x47 */
	"FOLDCHAR",              	/* 0x48 */
	"OPTIMIZED",             	/* 0x49 */
	"PSEUDO",                	/* 0x4a */
	/* ------------ States ------------- */
	"TRIE_next",             	/* REGNODE_MAX +0x01 */
	"TRIE_next_fail",        	/* REGNODE_MAX +0x02 */
	"EVAL_AB",               	/* REGNODE_MAX +0x03 */
	"EVAL_AB_fail",          	/* REGNODE_MAX +0x04 */
	"CURLYX_end",            	/* REGNODE_MAX +0x05 */
	"CURLYX_end_fail",       	/* REGNODE_MAX +0x06 */
	"WHILEM_A_pre",          	/* REGNODE_MAX +0x07 */
	"WHILEM_A_pre_fail",     	/* REGNODE_MAX +0x08 */
	"WHILEM_A_min",          	/* REGNODE_MAX +0x09 */
	"WHILEM_A_min_fail",     	/* REGNODE_MAX +0x0a */
	"WHILEM_A_max",          	/* REGNODE_MAX +0x0b */
	"WHILEM_A_max_fail",     	/* REGNODE_MAX +0x0c */
	"WHILEM_B_min",          	/* REGNODE_MAX +0x0d */
	"WHILEM_B_min_fail",     	/* REGNODE_MAX +0x0e */
	"WHILEM_B_max",          	/* REGNODE_MAX +0x0f */
	"WHILEM_B_max_fail",     	/* REGNODE_MAX +0x10 */
	"BRANCH_next",           	/* REGNODE_MAX +0x11 */
	"BRANCH_next_fail",      	/* REGNODE_MAX +0x12 */
	"CURLYM_A",              	/* REGNODE_MAX +0x13 */
	"CURLYM_A_fail",         	/* REGNODE_MAX +0x14 */
	"CURLYM_B",              	/* REGNODE_MAX +0x15 */
	"CURLYM_B_fail",         	/* REGNODE_MAX +0x16 */
	"IFMATCH_A",             	/* REGNODE_MAX +0x17 */
	"IFMATCH_A_fail",        	/* REGNODE_MAX +0x18 */
	"CURLY_B_min_known",     	/* REGNODE_MAX +0x19 */
	"CURLY_B_min_known_fail",	/* REGNODE_MAX +0x1a */
	"CURLY_B_min",           	/* REGNODE_MAX +0x1b */
	"CURLY_B_min_fail",      	/* REGNODE_MAX +0x1c */
	"CURLY_B_max",           	/* REGNODE_MAX +0x1d */
	"CURLY_B_max_fail",      	/* REGNODE_MAX +0x1e */
	"COMMIT_next",           	/* REGNODE_MAX +0x1f */
	"COMMIT_next_fail",      	/* REGNODE_MAX +0x20 */
	"MARKPOINT_next",        	/* REGNODE_MAX +0x21 */
	"MARKPOINT_next_fail",   	/* REGNODE_MAX +0x22 */
	"SKIP_next",             	/* REGNODE_MAX +0x23 */
	"SKIP_next_fail",        	/* REGNODE_MAX +0x24 */
	"CUTGROUP_next",         	/* REGNODE_MAX +0x25 */
	"CUTGROUP_next_fail",    	/* REGNODE_MAX +0x26 */
	"KEEPS_next",            	/* REGNODE_MAX +0x27 */
	"KEEPS_next_fail",       	/* REGNODE_MAX +0x28 */
};
#endif /* DOINIT */

/* PL_reg_extflags_name[] - Opcode/state names in string form, for debugging */

#ifndef DOINIT
EXTCONST char * PL_reg_extflags_name[];
#else
EXTCONST char * const PL_reg_extflags_name[] = {
	/* Bits in extflags defined: 00011110011111111111011100111111 */
	"ANCH_BOL",         /* 0x00000001 */
	"ANCH_MBOL",        /* 0x00000002 */
	"ANCH_SBOL",        /* 0x00000004 */
	"ANCH_GPOS",        /* 0x00000008 */
	"GPOS_SEEN",        /* 0x00000010 */
	"GPOS_FLOAT",       /* 0x00000020 */
	"UNUSED_BIT_6",     /* 0x00000040 */
	"UNUSED_BIT_7",     /* 0x00000080 */
	"SKIPWHITE",        /* 0x00000100 */
	"START_ONLY",       /* 0x00000200 */
	"WHITE",            /* 0x00000400 */
	"UNUSED_BIT_11",    /* 0x00000800 */
	"MULTILINE",        /* 0x00001000 */
	"SINGLELINE",       /* 0x00002000 */
	"FOLD",             /* 0x00004000 */
	"EXTENDED",         /* 0x00008000 */
	"UTF8",             /* 0x00010000 */
	"KEEPCOPY",         /* 0x00020000 */
	"LOOKBEHIND_SEEN",  /* 0x00040000 */
	"EVAL_SEEN",        /* 0x00080000 */
	"CANY_SEEN",        /* 0x00100000 */
	"NOSCAN",           /* 0x00200000 */
	"CHECK_ALL",        /* 0x00400000 */
	"UNUSED_BIT_23",    /* 0x00800000 */
	"UNUSED_BIT_24",    /* 0x01000000 */
	"USE_INTUIT_NOML",  /* 0x02000000 */
	"USE_INTUIT_ML",    /* 0x04000000 */
	"INTUIT_TAIL",      /* 0x08000000 */
	"COPY_DONE",        /* 0x10000000 */
	"UNUSED_BIT_29",    /* 0x20000000 */
	"UNUSED_BIT_30",    /* 0x40000000 */
	"UNUSED_BIT_31",    /* 0x80000000 */
};
#endif /* DOINIT */

/* ex: set ro: */
