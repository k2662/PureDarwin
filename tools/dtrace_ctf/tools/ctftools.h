/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License (the "License").
 * You may not use this file except in compliance with the License.
 *
 * You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
 * or http://www.opensolaris.org/os/licensing.
 * See the License for the specific language governing permissions
 * and limitations under the License.
 *
 * When distributing Covered Code, include this CDDL HEADER in each
 * file and include the License file at usr/src/OPENSOLARIS.LICENSE.
 * If applicable, add the following below this CDDL HEADER, with the
 * fields enclosed by brackets "[]" replaced with your own identifying
 * information: Portions Copyright [yyyy] [name of copyright owner]
 *
 * CDDL HEADER END
 */
/*
 * Copyright 2006 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

#ifndef _CTFTOOLS_H
#define	_CTFTOOLS_H

/*
 * Functions and data structures used in the manipulation of stabs and CTF data
 */

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <libelf.h>
#include <gelf.h>
#include <pthread.h>

#ifdef __cplusplus
extern "C" {
#endif

#include "array.h"
#include "alist.h"
#include "list.h"
#include "hash.h"
#include "atom.h"

#ifndef DEBUG_LEVEL
#define	DEBUG_LEVEL 0
#endif

#ifndef DEBUG_STREAM
#define	DEBUG_STREAM stderr
#endif

#ifndef MAX
#define	MAX(a, b) 		((a) < (b) ? (b) : (a))
#endif

#ifndef MIN
#define	MIN(a, b) 		((a) > (b) ? (b) : (a))
#endif

#define	TRUE	1
#define	FALSE	0

#define	CTF_ELF_SCN_NAME	".SUNW_ctf"

#define	CTF_LABEL_LASTIDX	-1

#define	CTF_DEFAULT_LABEL	"*** No Label Provided ***"

/*
 * Default hash sizes
 */
#define	TDATA_LAYOUT_HASH_SIZE	8191	/* A tdesc hash based on layout */
#define	TDATA_ID_HASH_SIZE	997	/* A tdesc hash based on type id */
#define	IIDESC_HASH_SIZE	8191	/* Hash of iidesc's */

/*
 * The default function argument array size.  We'll realloc the array larger
 * if we need to, but we want a default value that will allow us to avoid
 * reallocation in the common case.
 */
#define	FUNCARG_DEF	5

extern const char *progname;
extern int debug_level;
extern const char *curhdr;

#define	N_GSYM	0x20	/* global symbol: name,,0,type,0 */
#define	N_FUN	0x24	/* procedure: name,,0,linenumber,0 */
#define	N_STSYM	0x26	/* static symbol: name,,0,type,0 or section relative */
#define	N_LCSYM	0x28	/* .lcomm symbol: name,,0,type,0 or section relative */
#define	N_ROSYM	0x2c	/* ro_data: name,,0,type,0 or section relative */
#define	N_OPT	0x3c	/* compiler options */
#define	N_RSYM	0x40	/* register sym: name,,0,type,register */
#define	N_SO	0x64	/* source file name: name,,0,0,0 */
#define	N_LSYM	0x80	/* local sym: name,,0,type,offset */
#define	N_SOL	0x84	/* #included file name: name,,0,0,0 */
#define	N_PSYM	0xa0	/* parameter: name,,0,type,offset */
#define	N_LBRAC	0xc0	/* left bracket: 0,,0,nesting level,function relative */
#define	N_RBRAC	0xe0	/* right bracket: 0,,0,nesting level,func relative */
#define	N_BINCL 0x82	/* header file: name,,0,0,0 */
#define	N_EINCL 0xa2	/* end of include file */

/*
 * Nodes in the type tree
 *
 * Each node consists of a single tdesc_t, with one of several auxiliary
 * structures linked in via the `data' union.
 */

/* The type of tdesc_t node */
typedef enum stabtype {
	STABTYPE_FIRST, /* do not use */
	INTRINSIC,
	POINTER,
	ARRAY,
	FUNCTION,
	STRUCT,
	UNION,
	ENUM,
	FORWARD,
	TYPEDEF,
	TYPEDEF_UNRES,
	VOLATILE,
	CONST,
	RESTRICT,
	PTRAUTH,
	STABTYPE_LAST /* do not use */
} stabtype_t;

typedef struct tdesc tdesc_t;

/* Auxiliary structure for array tdesc_t */
typedef struct ardef {
	tdesc_t	*ad_contents;
	tdesc_t *ad_idxtype;
	uint_t	ad_nelems;
} ardef_t;

/* Auxiliary structure for structure/union tdesc_t */
typedef struct mlist {
	int	ml_offset;	/* Offset from start of structure (in bits) */
	int	ml_size;	/* Member size (in bits) */
	atom_t	*ml_name;	/* Member name */
	struct	tdesc *ml_type;	/* Member type */
	struct	mlist *ml_next;	/* Next member */
} mlist_t;

/* Auxiliary structure for enum tdesc_t */
typedef struct elist {
	atom_t	*el_name;
	int	el_number;
	struct elist *el_next;
} elist_t;

/* Auxiliary structure for intrinsics (integers and reals) */
typedef enum {
	INTR_INT,
	INTR_REAL
} intrtype_t;

typedef struct intr {
	intrtype_t	intr_type;
	int		intr_signed;
	union {
			char _iformat;
			int _fformat;
	} _u;
	int		intr_offset;
	int		intr_nbits;
} intr_t;

#define	intr_iformat _u._iformat
#define	intr_fformat _u._fformat

typedef struct fndef {
	struct tdesc *fn_ret;
	uint_t fn_nargs;
	uint_t fn_vargs;
	tdesc_t *fn_args[];
} fndef_t;

typedef struct ptrauth {
	tdesc_t *pta_type;
	uint8_t pta_key;
	uint16_t pta_discriminator;
	bool pta_discriminated;
} ptrauth_t;

typedef int32_t tid_t;

/*
 * The tdesc_t (Type DESCription) is the basic node type used in the stabs data
 * structure.  Each data node gets a tdesc structure.  Each node is linked into
 * a directed graph (think of it as a tree with multiple roots and multiple
 * leaves), with the root nodes at the top, and intrinsics at the bottom.  The
 * root nodes, which are pointed to by iidesc nodes, correspond to the types,
 * globals, and statics defined by the stabs.
 */
struct tdesc {
	atom_t	*t_name;
	tdesc_t *t_next;	/* Name hash next pointer */

	tid_t t_id;
	tdesc_t *t_hash;	/* ID hash next pointer */

	stabtype_t t_type;
	int	t_size;	/* Size in bytes of object represented by this node */

	union {
		intr_t	*intr;		/* int, real */
		tdesc_t *tdesc;		/* ptr, typedef, vol, const, restr */
		ardef_t *ardef;		/* array */
		mlist_t *members;	/* struct, union */
		elist_t *emem;		/* enum */
		fndef_t *fndef;		/* function - first is return type */
		ptrauth_t *ptrauth;	/* ptrauth */
	} t_data;

	int t_flags;
	int t_vgen;	/* Visitation generation (see traverse.c) */
	int t_emark;	/* Equality mark (see equiv_cb() in merge.c) */
};

#define	t_intr		t_data.intr
#define	t_tdesc		t_data.tdesc
#define	t_ardef		t_data.ardef
#define	t_members	t_data.members
#define	t_emem		t_data.emem
#define	t_fndef		t_data.fndef
#define	t_ptrauth	t_data.ptrauth

#define	TDESC_F_ISROOT		0x1	/* Has an iidesc_t (see below) */
#define	TDESC_F_GLOBAL		0x2
#define	TDESC_F_RESOLVED	0x4

/*
 * iidesc_t (Interesting Item DESCription) nodes point to tdesc_t nodes that
 * correspond to "interesting" stabs.  A stab is interesting if it defines a
 * global or static variable, a global or static function, or a data type.
 */
typedef enum iitype {
	II_NOT = 0,
	II_GFUN,	/* Global function */
	II_SFUN,	/* Static function */
	II_GVAR,	/* Global variable */
	II_SVAR,	/* Static variable */
	II_PSYM,	/* Function argument */
	II_SOU,		/* Struct or union */
	II_TYPE		/* Type (typedef) */
} iitype_t;

typedef struct iidesc {
	iitype_t	ii_type;
	atom_t		*ii_name;
	tdesc_t 	*ii_dtype;
	atom_t		*ii_owner;	/* File that defined this node */
	int		ii_flags;

	/* Function arguments (if any) */
	int		ii_nargs;
	tdesc_t 	**ii_args;
	int		ii_vargs;	/* Function uses varargs */
} iidesc_t;

#define	IIDESC_F_USED	0x1	/* Write this iidesc out */

/*
 * labelent_t nodes identify labels and corresponding type ranges associated
 * with them.  The label in a given labelent_t is associated with types with
 * ids <= le_idx.
 */
typedef struct labelent {
	atom_t *le_name;
	int le_idx;
} labelent_t;

/*
 * The tdata_t (Type DATA) structure contains or references all type data for
 * a given file or, during merging, several files.
 */
typedef struct tdata {
	int	td_curemark;	/* Equality mark (see merge.c) */
	int	td_curvgen;	/* Visitation generation (see traverse.c) */
	int	td_nextid;	/* The ID for the next tdesc_t created */
	hash_t	*td_iihash;	/* The iidesc_t nodes for this file */

	hash_t	*td_layouthash;	/* The tdesc nodes, hashed by structure */
	hash_t	*td_idhash;	/* The tdesc nodes, hashed by type id */

	atom_t	*td_parlabel;	/* Top label uniq'd against in parent */
	atom_t	*td_parname;	/* Basename of parent */
	array_t	*td_labels;	/* Labels and their type ranges */

	pthread_mutex_t td_mergelock;

	int	td_ref;
} tdata_t;

/*
 * By design, the iidesc hash is heterogeneous.  The CTF emitter, on the
 * other hand, needs to be able to access the elements of the list by type,
 * and in a specific sorted order.  An iiburst holds these elements in that
 * order.  (A burster is a machine that separates carbon-copy forms)
 */
typedef struct iiburst {
	int iib_nfuncs;
	int iib_curfunc;
	iidesc_t **iib_funcs;

	int iib_nobjts;
	int iib_curobjt;
	iidesc_t **iib_objts;

	array_t *iib_types;
	int iib_maxtypeid;

	tdata_t *iib_td;
	struct tdtrav_data *iib_tdtd; /* tdtrav_data_t */
} iiburst_t;

typedef struct ctf_buf ctf_buf_t;

typedef struct symit_data symit_data_t;

/*
 * The workhorse structure of tdata_t merging.  Holds all lists of nodes to be
 * processed during various phases of the merge algorithm.
 */
typedef struct merge_cb_data {
	tdata_t *md_parent;
	tdata_t *md_tgt;
	alist_t *md_ta;		/* Type Association */
	alist_t *md_fdida;	/* Forward -> Definition ID Association */
	array_t	*md_iitba;	/* iidesc_t nodes To Be Added to the parent */
	hash_t	*md_tdtba;	/* tdesc_t nodes To Be Added to the parent */
	array_t	*md_tdtbr;	/* tdesc_t nodes To Be Remapped */
	int md_flags;
} merge_cb_data_t;


/* ctf.c */
caddr_t ctf_gen(iiburst_t *, size_t *, int);
tdata_t *ctf_load(char *, caddr_t, size_t, symit_data_t *, char *);

/* iidesc.c */
iidesc_t *iidesc_new(atom_t *);
int iidesc_hash(int, void *);
void iter_iidescs_by_name(tdata_t *, const char *,
    int (*)(iidesc_t *, void *), void *);
iidesc_t *iidesc_dup(iidesc_t *);
iidesc_t *iidesc_dup_rename(iidesc_t *, char const *, char const *);
void iidesc_add(hash_t *, iidesc_t *);
void iidesc_free(iidesc_t *, void *);
int iidesc_count_type(void *, void *);
void iidesc_stats(hash_t *);
int iidesc_dump(iidesc_t *);

/* input.c */
typedef enum source_types {
	SOURCE_NONE 	= 0,
	SOURCE_UNKNOWN	= 1,
	SOURCE_C	= 2,
	SOURCE_S	= 4
} source_types_t;

source_types_t built_source_types(Elf *, const char *);
int count_files(char **, int);
int read_ctf(char **, int, char *, int (*)(tdata_t *, char *, void *),
    void *, int);
int read_ctf_save_cb(tdata_t *, char *, void *);
symit_data_t *symit_new(Elf *, const char *);
void symit_reset(symit_data_t *);
char *symit_curfile(symit_data_t *);
GElf_Sym *symit_next(symit_data_t *, int);
char *symit_name(symit_data_t *);
void symit_free(symit_data_t *);

/* merge.c */
void merge_cb_data_destroy(merge_cb_data_t *);
void merge_into_master(merge_cb_data_t *, tdata_t *, tdata_t *, tdata_t *, int);
void ctfmerge_prepare(int nielems);
int ctfmerge_add_td(tdata_t *td, const char *name);
tdata_t *ctfmerge_done(void);

/* output.c */
#define	CTF_FUZZY_MATCH	0x01 /* match local symbols to global CTF */
#define	CTF_USE_DYNSYM	0x02 /* use .dynsym not .symtab */
#define	CTF_COMPRESS	0x04 /* compress CTF output */
#define	CTF_KEEP_STABS	0x08 /* keep .stabs sections */
#define	CTF_MINIMIZE	0x10 /* create minimal symbols & CTF output file */
#if defined(__APPLE__)
#define CTF_RAW_OUTPUT	(1<<30) /* output raw CTF data only, no segment/section data, and do not copy original file */
#endif /* __APPLE__ */

void write_ctf(tdata_t *, const char *, const char *, int);

/* dwarf.c */
int dw_read(Elf *, const char *, const char *, int, tdata_t **);

/* tdata.c */
tdata_t *tdata_new(void);
void tdata_free(tdata_t *);
void tdata_build_hashes(tdata_t *td);
const char *tdesc_name(tdesc_t *);
int tdesc_idhash(int, void *);
int tdesc_idcmp(void *, void *);
int tdesc_namehash(int, void *);
int tdesc_namecmp(void *, void *);
int tdesc_layouthash(int, void *);
int tdesc_layoutcmp(void *, void *);
void tdesc_free(tdesc_t *);
int	tdata_label_iter(tdata_t *, int (*)(labelent_t *, void *), void *);
void tdata_label_add(tdata_t *, char *, int);
labelent_t *tdata_label_top(tdata_t *);
void tdata_label_free(tdata_t *);
void tdata_merge(tdata_t *, tdata_t *);
void tdata_label_newmax(tdata_t *, int);

/* util.c */
int streq(const char *, const char *);
int findelfsecidx(Elf *, const char *, const char *);
size_t elf_ptrsz(Elf *);
char *mktmpname(const char *, const char *);
void terminate(const char *, ...);
void aborterr(char *, ...);
void set_terminate_cleanup(void (*)());
void elfterminate(const char *, const char *, ...);
void warning(char *, ...);
void vadebug(int, char *, va_list);
void debug(int, char *, ...);

#ifdef __cplusplus
}
#endif

#endif /* _CTFTOOLS_H */