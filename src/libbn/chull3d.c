/*
 * Ken Clarkson wrote this.  Copyright (c) 1995 by AT&T..
 * Permission to use, copy, modify, and distribute this software for any
 * purpose without fee is hereby granted, provided that this entire notice
 * is included in all copies of any software which is or includes a copy
 * or modification of this software and in all copies of the supporting
 * documentation for such software.
 * THIS SOFTWARE IS BEING PROVIDED "AS IS", WITHOUT ANY EXPRESS OR IMPLIED
 * WARRANTY.  IN PARTICULAR, NEITHER THE AUTHORS NOR AT&T MAKE ANY
 * REPRESENTATION OR WARRANTY OF ANY KIND CONCERNING THE MERCHANTABILITY
 * OF THIS SOFTWARE OR ITS FITNESS FOR ANY PARTICULAR PURPOSE.
 */

#include "common.h"

#include <assert.h>
#include <float.h>
#include <locale.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>

/*#include "chull3d.h"*/
#include "bu/file.h"
#include "bu/log.h"
#include "bu/getopt.h"
#include "bu/malloc.h"
#include "bn.h"

#define MAXBLOCKS 10000
#define max_blocks 10000
#define Nobj 10000
#define MAXPOINTS 10000
#define BLOCKSIZE 100000

typedef double Coord;
typedef Coord* point;
typedef point site;
typedef Coord* normalp;
typedef Coord site_struct;

typedef struct basis_s {
    struct basis_s *next; /* free list */
    int ref_count;      /* storage management */
    int lscale;    /* the log base 2 of total scaling of vector */
    Coord sqa, sqb; /* sums of squared norms of a part and b part */
    Coord vecs[1]; /* the actual vectors, extended by malloc'ing bigger */
} basis_s;

typedef struct neighbor {
    site vert; /* vertex of simplex */
    struct simplex *simp; /* neighbor sharing all vertices but vert */
    basis_s *basis; /* derived vectors */
} neighbor;

typedef struct simplex {
    struct simplex *next;       /* free list */
    long visit;         /* number of last site visiting this simplex */
    short mark;
    basis_s* normal;    /* normal vector pointing inward */
    neighbor peak;              /* if null, remaining vertices give facet */
    neighbor neigh[1];  /* neighbors of simplex */
} simplex;


typedef struct fg_node fg;
typedef struct tree_node Tree;
struct tree_node {
    Tree *left, *right;
    site key;
    int size;   /* maintained to be the number of nodes rooted here */
    fg *fgs;
    Tree *next; /* freelist */
};

typedef struct fg_node {
    Tree *facets;
    double dist, vol;   /* of Voronoi face dual to this */
    fg *next;           /* freelist */
    short mark;
    int ref_count;
} fg_node;


typedef site gsitef(void *);
typedef long site_n(void *, site);
struct chull3d_data {
    size_t simplex_size;
    simplex *simplex_list;
    size_t basis_s_size;
    basis_s *basis_s_list;
    size_t Tree_size;
    Tree *Tree_list;
    size_t fg_size;
    fg *fg_list;
    int pdim;   /* point dimension */
    point *site_blocks;
    int num_blocks;
    short check_overshoot_f;
    simplex *ch_root;
    double Huge;
    int basis_vec_size;
    int exact_bits;
    float b_err_min, b_err_min_sq;

    short vd;
    basis_s *tt_basis;
    basis_s *infinity_basis;
    Coord *hull_infinity;
    int *B;
    int tot;
    int totinf;
    int bigt;
    gsitef *get_site;
    site_n *site_num;
    fg *faces_gr_t;
    FILE *FG_OUT;
    FILE *DFILE;
    int p_fg_x_depth;
    /*double fg_hist[100][100], fg_hist_bad[100][100],fg_hist_far[100][100];*/
    double **fg_hist;
    double **fg_hist_bad;
    double **fg_hist_far;
    /*double mult_up = 1.0;*/
    double mult_up;
    /*Coord mins[MAXDIM] = {DBL_MAX,DBL_MAX,DBL_MAX,DBL_MAX,DBL_MAX,DBL_MAX,DBL_MAX,DBL_MAX};
     *       Coord maxs[MAXDIM] = {-DBL_MAX,-DBL_MAX,-DBL_MAX,-DBL_MAX,-DBL_MAX,-DBL_MAX,-DBL_MAX,-DBL_MAX};*/
    Coord *mins;
    Coord *maxs;
    unsigned short X[3];
    site p;         /* the current site */
    long pnum;
    int rdim;   /* region dimension: (max) number of sites specifying region */
    int cdim;   /* number of sites currently specifying region */
    int site_size; /* size of malloc needed for a site */
    int point_size;  /* size of malloc needed for a point */
    short *mi;
    short *mo;
    char *tmpfilenam;
    int *face_cnt;
    int *face_array;
    int *vert_cnt;
    point_t *vert_array;
};


#define NEARZERO(d)     ((d) < FLT_EPSILON && (d) > -FLT_EPSILON)
#define SWAP(X,a,b) {X t; t = a; a = b; b = t;}

#define check_overshoot(check_overshoot_f, x)                                                      \
{if (CHECK_OVERSHOOT && check_overshoot_f && ((x)>9e15))                \
        warning(-20, overshot exact arithmetic)}                        \


#define DELIFT 0


#define VA(x) ((x)->vecs+cdata->rdim)
#define VB(x) ((x)->vecs)

#define two_to(x) ( ((x)<20) ? 1<<(x) : ldexp(1,(x)) )


#define trans(z,p,q) {int i; for (i=0;i<cdata->pdim;i++) z[i+cdata->rdim] = z[i] = p[i] - q[i];}
#define lift(cdata,z,s) {if (cdata->vd) z[2*(cdata->rdim)-1] =z[(cdata->rdim)-1]= ldexp(Vec_dot_pdim(cdata,z,z), -DELIFT);}
/*not scaling lift to 2^-DELIFT */


#define swap_points(a,b) {point t; t=a; a=b; b=t;}


/* This is the comparison.                                       */
/* Returns <0 if i<j, =0 if i=j, and >0 if i>j                   */
#define compare(cdata, i,j) (cdata->site_num((void *)cdata, i)-cdata->site_num((void *)cdata, j))

/* This macro returns the size of a node.  Unlike "x->size",     */
/* it works even if x=NULL.  The test could be avoided by using  */
/* a special version of NULL which was a real node with size 0.  */
#define node_size(x) ((x) ? ((x)->size) : 0 )


#define snkey(cdata, x) cdata->site_num((void *)cdata, (x)->vert)

#define push(x) *(st+tms++) = x;
#define pop(x)  x = *(st + --tms);


#define lookup(cdata, a,b,what,whatt)                                          \
{                                                                       \
        int i;                                                          \
        neighbor *x;                                                    \
        for (i=0, x = a->neigh; (x->what != b) && (i<(cdata->cdim)) ; i++, x++); \
        if (i<(cdata->cdim))                                                     \
        return x;                                               \
        else {                                                          \
	            fprintf(cdata->DFILE,"adjacency failure,op_" #what ":\n");     \
	            DEBTR(-10)                                              \
	            print_simplex_f(cdata, a, cdata->DFILE, &print_neighbor_full);        \
	            print_##whatt(cdata,b, cdata->DFILE);                                \
	            fprintf(cdata->DFILE,"---------------------\n");               \
	            print_triang(cdata, a,cdata->DFILE, &print_neighbor_full);            \
	            exit(1);                                                \
	            return 0;                                               \
	        }                                                               \
}


#define INCP(X,p,k) ((X*) ( (char*)p + (k) * X##_size)) /* portability? */


#define NEWL(cdata, list, X, p)                                               \
{                                                               \
        p = list ? list : new_block_##X(cdata, 1);             \
        assert(p);                                              \
        list = p->next;                                     \
}                                                               \


#define NEWLRC(cdata, list, X, p)                                             \
{                                                               \
        p = list ? list : new_block_##X(cdata, 1);             \
        assert(p);                                              \
        list = p->next;                                     \
        p->ref_count = 1;                                       \
}                                                               \


#define FREEL(cdata, X,p)                                              \
{                                                               \
        memset((p),0,cdata->X##_size);                                 \
        (p)->next = cdata->X##_list;                                   \
        cdata->X##_list = p;                                           \
}                                                               \


#define dec_ref(cdata, X,v)    {if ((v) && --(v)->ref_count == 0) FREEL(cdata, X,(v));}
#define inc_ref(X,v)    {if (v) v->ref_count++;}
#define NULLIFY(cdata, X,v)    {dec_ref(cdata, X,v); v = NULL;}

#define mod_refs(op,s)                                  \
{                                                       \
        int i;                                          \
        neighbor *mrsn;                                 \
        \
        for (i=-1,mrsn=s->neigh-1;i<(cdata->cdim);i++,mrsn++)    \
        op##_ref(basis_s, mrsn->basis);         \
}

#define free_simp(cdata, s)                            \
{       mod_refs(dec,s);                        \
        FREEL(cdata, basis_s,s->normal);               \
        FREEL(cdata, simplex, s);                      \
}                                               \


#define copy_simp(cdata, new, s, simplex_list, simplex_size)             \
{       NEWL(cdata, simplex_list, simplex, new);                      \
        memcpy(new,s,simplex_size);             \
        mod_refs(inc,s);                        \
}                                               \


#define MAXDIM 8
/*#define MAXBLOCKS 1000*/
#define DEBUG -7
#define CHECK_OVERSHOOT 1





#define DEBS(qq)  {if (DEBUG>qq) {
#define EDEBS }}
#define DEBOUT cdata->DFILE
#define DEB(ll,mes)  DEBS(ll) fprintf(DEBOUT,#mes "\n");fflush(DEBOUT); EDEBS
#define DEBEXP(ll,exp) DEBS(ll) fprintf(DEBOUT,#exp "=%G\n", (double) exp); fflush(DEBOUT); EDEBS
#define DEBTR(ll) DEBS(ll) fprintf(DEBOUT, __FILE__ " line %d \n" ,__LINE__);fflush(DEBOUT); EDEBS
#define warning(lev, x)                                         \
{static int messcount;                                  \
        if (++messcount<=10) {DEB(lev,x) DEBTR(lev)}        \
        if (messcount==10) DEB(lev, consider yourself warned) \
}                                                       \


#define SBCHECK(s)




simplex *new_block_simplex(struct chull3d_data *cdata, int make_blocks) {
    int i;
    static simplex *simplex_block_table[max_blocks];
    simplex *xlm, *xbt;
    static int num_simplex_blocks;
    if (make_blocks && cdata) {
	((num_simplex_blocks<max_blocks) ? (void) (0) : __assert_fail ("num_simplex_blocks<max_blocks", "chull3d.c", 31, __PRETTY_FUNCTION__));
	xbt = simplex_block_table[num_simplex_blocks++] = (simplex*)malloc(max_blocks * cdata->simplex_size);
	memset(xbt,0,max_blocks * cdata->simplex_size);
	((xbt) ? (void) (0) : __assert_fail ("xbt", "chull3d.c", 31, __PRETTY_FUNCTION__));
	xlm = ((simplex*) ( (char*)xbt + (max_blocks) * cdata->simplex_size));
	for (i=0; i<max_blocks; i++) {
	    xlm = ((simplex*) ( (char*)xlm + ((-1)) * cdata->simplex_size));
	    xlm->next = cdata->simplex_list;
	    cdata->simplex_list = xlm;
	}
	return cdata->simplex_list;
    };
    for (i=0; i<num_simplex_blocks; i++) free(simplex_block_table[i]);
    num_simplex_blocks = 0;
    if (cdata) cdata->simplex_list = 0;
    return 0;
}

void free_simplex_storage(struct chull3d_data *cdata) {
    new_block_simplex(cdata, 0);
}

basis_s *new_block_basis_s(struct chull3d_data *cdata, int make_blocks) {
    int i;
    static basis_s *basis_s_block_table[max_blocks];
    basis_s *xlm, *xbt;
    static int num_basis_s_blocks;
    if (make_blocks && cdata) {
	((num_basis_s_blocks<max_blocks) ? (void) (0) : __assert_fail ("num_basis_s_blocks<max_blocks", "chull3d.c", 32, __PRETTY_FUNCTION__));
	xbt = basis_s_block_table[num_basis_s_blocks++] = (basis_s*)malloc(max_blocks * cdata->basis_s_size);
	memset(xbt,0,max_blocks * cdata->basis_s_size);
	((xbt) ? (void) (0) : __assert_fail ("xbt", "chull3d.c", 32, __PRETTY_FUNCTION__));
	xlm = ((basis_s*) ( (char*)xbt + (max_blocks) * cdata->basis_s_size));
	for (i=0; i<max_blocks; i++) {
	    xlm = ((basis_s*) ( (char*)xlm + ((-1)) * cdata->basis_s_size));
	    xlm->next = cdata->basis_s_list;
	    cdata->basis_s_list = xlm;
	}
	return cdata->basis_s_list;
    };
    for (i=0; i<num_basis_s_blocks; i++) free(basis_s_block_table[i]);
    num_basis_s_blocks = 0;
    if (cdata) cdata->basis_s_list = 0;
    return 0;
}
void free_basis_s_storage(struct chull3d_data *cdata) {
    new_block_basis_s(cdata, 0);
}


Tree *new_block_Tree(struct chull3d_data *cdata, int make_blocks) {
    int i;
    static Tree *Tree_block_table[max_blocks];
    Tree *xlm, *xbt;
    static int num_Tree_blocks;
    if (make_blocks && cdata) {
	((num_Tree_blocks<max_blocks) ? (void) (0) : __assert_fail ("num_Tree_blocks<max_blocks", "chull3d.c", 33, __PRETTY_FUNCTION__));
	xbt = Tree_block_table[num_Tree_blocks++] = (Tree*)malloc(max_blocks * cdata->Tree_size);
	memset(xbt,0,max_blocks * cdata->Tree_size);
	((xbt) ? (void) (0) : __assert_fail ("xbt", "chull3d.c", 33, __PRETTY_FUNCTION__));
	xlm = ((Tree*) ( (char*)xbt + (max_blocks) * cdata->Tree_size));
	for (i=0; i<max_blocks; i++) {
	    xlm = ((Tree*) ( (char*)xlm + ((-1)) * cdata->Tree_size));
	    xlm->next = cdata->Tree_list;
	    cdata->Tree_list = xlm;
	}
	return cdata->Tree_list;
    };
    for (i=0; i<num_Tree_blocks; i++) free(Tree_block_table[i]);
    num_Tree_blocks = 0;
    if (cdata) cdata->Tree_list = 0;
    return 0;
}
void free_Tree_storage(struct chull3d_data *cdata) {
    new_block_Tree(cdata, 0);
}

fg *new_block_fg(struct chull3d_data *cdata, int make_blocks) {
    int i;
    static fg *fg_block_table[max_blocks];
    fg *xlm, *xbt;
    static int num_fg_blocks;
    if (make_blocks && cdata) {
	((num_fg_blocks<max_blocks) ? (void) (0) : __assert_fail ("num_fg_blocks<max_blocks", "chull3d.c", 34, __PRETTY_FUNCTION__));
	xbt = fg_block_table[num_fg_blocks++] = (fg*)malloc(max_blocks * cdata->fg_size);
	memset(xbt,0,max_blocks * cdata->fg_size);
	((xbt) ? (void) (0) : __assert_fail ("xbt", "chull3d.c", 34, __PRETTY_FUNCTION__));
	xlm = ((fg*) ( (char*)xbt + (max_blocks) * cdata->fg_size));
	for (i=0; i<max_blocks; i++) {
	    xlm = ((fg*) ( (char*)xlm + ((-1)) * cdata->fg_size));
	    xlm->next = cdata->fg_list;
	    cdata->fg_list = xlm;
	}
	return cdata->fg_list;
    };
    for (i=0; i<num_fg_blocks; i++) free(fg_block_table[i]);
    num_fg_blocks = 0;
    if (cdata) cdata->fg_list = 0;
    return 0;
}
void free_fg_storage(struct chull3d_data *cdata) {
    new_block_fg(cdata, 0);
}



/* io.c : input-output */

void chull3d_panic(struct chull3d_data *cdata, char *fmt, ...) {
    va_list args;

    va_start(args, fmt);
    vfprintf(cdata->DFILE, fmt, args);
    fflush(cdata->DFILE);
    va_end(args);

    exit(1);
}




/* ch.c : numerical functions for hull computation */

static Coord Vec_dot(struct chull3d_data *cdata, point x, point y) {
    int i;
    Coord sum = 0;
    for (i=0;i<cdata->rdim;i++) sum += x[i] * y[i];
    return sum;
}

static Coord Vec_dot_pdim(struct chull3d_data *cdata, point x, point y) {
    int i;
    Coord sum = 0;
    for (i=0;i<cdata->pdim;i++) sum += x[i] * y[i];
    /*	check_overshoot(cdata->check_overshoot_f, sum); */
    return sum;
}

static Coord Norm2(struct chull3d_data *cdata, point x) {
    int i;
    Coord sum = 0;
    for (i=0;i<cdata->rdim;i++) sum += x[i] * x[i];
    return sum;
}

static void Ax_plus_y(struct chull3d_data *cdata, Coord a, point x, point y) {
    int i;
    for (i=0;i<cdata->rdim;i++) {
	*y++ += a * *x++;
    }
}

static void Ax_plus_y_test(struct chull3d_data *cdata, Coord a, point x, point y) {
    int i;
    for (i=0;i<cdata->rdim;i++) {
	check_overshoot(cdata->check_overshoot_f, *y + a * *x);
	*y++ += a * *x++;
    }
}

static void Vec_scale_test(struct chull3d_data *cdata, int n, Coord a, Coord *x)
{
    register Coord *xx = x,
	     *xend = xx + n	;

    check_overshoot(cdata->check_overshoot_f, a);

    while (xx!=xend) {
	*xx *= a;
	check_overshoot(cdata->check_overshoot_f, *xx);
	xx++;
    }
}


static void print_simplex_num(FILE *F, simplex *s) {
    fprintf(F, "simplex ");
    if(!s) fprintf(F, "NULL ");
    else fprintf(F, "%p  ", (void*)s);
}

void print_neighbor_snum(struct chull3d_data *cdata, FILE* F, neighbor *n){

    assert(cdata->site_num!=NULL);
    if (n->vert)
	fprintf(F, "%ld ", (*(cdata->site_num))((void *)cdata, n->vert));
    else
	fprintf(F, "NULL vert ");
    fflush(stdout);
}

void print_point(FILE *F, int dim, point p) {
    int j;
    if (!p) {
	fprintf(F, "NULL");
	return;
    }
    for (j=0;j<dim;j++) fprintf(F, "%g  ", *p++);
}

void print_point_int(FILE *F, int dim, point p) {
    int j;
    if (!p) {
	fprintf(F, "NULL");
	return;
    }
    for (j=0;j<dim;j++) fprintf(F, "%.20g  ", *p++);
}

void print_basis(struct chull3d_data *cdata, FILE *F, basis_s* b) {
    if (!b) {fprintf(F, "NULL basis ");fflush(stdout);return;}
    if (b->lscale<0) {fprintf(F, "\nbasis computed");return;}
    fprintf(F, "\n%p  %d \n b=",(void*)b,b->lscale);
    print_point(F, cdata->rdim, b->vecs);
    fprintf(F, "\n a= ");
    print_point_int(F, cdata->rdim, b->vecs+cdata->rdim); fprintf(F, "   ");
    fflush(F);
}

void print_neighbor_full(struct chull3d_data *cdata, FILE *F, neighbor *n){

    if (!n) {fprintf(F, "null neighbor\n");return;}

    print_simplex_num(F, n->simp);
    print_neighbor_snum(cdata,F, n);fprintf(F, ":  ");fflush(F);
    if (n->vert) {
	/*		if (n->basis && n->basis->lscale <0) fprintf(F, "trans ");*/
	/* else */ print_point(F, cdata->pdim,n->vert);
	fflush(F);
    }
    print_basis(cdata, F, n->basis);
    fflush(F);
    fprintf(F, "\n");
}


typedef void print_neighbor_f(struct chull3d_data *, FILE*, neighbor*);

void *print_facet(struct chull3d_data *cdata, FILE *F, simplex *s, print_neighbor_f *pnfin) {
    int i;
    neighbor *sn = s->neigh;

    /*	fprintf(F, "%d ", s->mark);*/
    for (i=0; i<(cdata->cdim);i++,sn++) (*pnfin)(cdata, F, sn);
    fprintf(F, "\n");
    fflush(F);
    return NULL;
}




void *print_simplex_f(struct chull3d_data *cdata, simplex *s, FILE *F, print_neighbor_f *pnfin){

    static print_neighbor_f *pnf;

    if (pnfin) {pnf=pnfin; if (!s) return NULL;}

    print_simplex_num(F, s);
    fprintf(F, "\n");
    if (!s) return NULL;
    fprintf(F, "normal ="); print_basis(cdata, F, s->normal); fprintf(F, "\n");
    fprintf(F, "peak ="); (*pnf)(cdata,F, &(s->peak));
    fprintf (F, "facet =\n");fflush(F);
    return print_facet(cdata, F, s, pnf);
}

void *print_simplex(struct chull3d_data *cdata, simplex *s, void *Fin) {
    static FILE *F;

    if (Fin) {F=(FILE*)Fin; if (!s) return NULL;}

    return print_simplex_f(cdata, s, F, 0);

}

/* amount by which to scale up vector, for reduce_inner */
static double sc(struct chull3d_data *cdata, basis_s *v,simplex *s, int k, int j) {
    double		labound;
    static int	lscale;
    static double	max_scale,
			ldetbound,
			Sb;

    if (j<10) {
	labound = logb(v->sqa)/2;
	max_scale = cdata->exact_bits - labound - 0.66*(k-2)-1  -DELIFT;
	if (max_scale<1) {
	    warning(-10, overshot exact arithmetic);
	    max_scale = 1;

	}

	if (j==0) {
	    int	i;
	    neighbor *sni;
	    basis_s *snib;

	    ldetbound = DELIFT;

	    Sb = 0;
	    for (i=k-1,sni=s->neigh+k-1;i>0;i--,sni--) {
		snib = sni->basis;
		Sb += snib->sqb;
		ldetbound += logb(snib->sqb)/2 + 1;
		ldetbound -= snib->lscale;
	    }
	}
    }
    if (ldetbound - v->lscale + logb(v->sqb)/2 + 1 < 0) {
	DEBS(-2)
	    DEBTR(-2) DEBEXP(-2, ldetbound)
	    print_simplex_f(cdata, s, cdata->DFILE, &print_neighbor_full);
	print_basis(cdata, cdata->DFILE,v);
	EDEBS
	    return 0;
    } else {
	lscale = (int)floor(logb(2*Sb/(v->sqb + v->sqa*cdata->b_err_min)))/2;
	if (lscale > max_scale) {
	    lscale = (int)floor(max_scale);
	} else if (lscale<0) lscale = 0;
	v->lscale += lscale;
	return two_to(lscale);
    }
}


static int reduce_inner(struct chull3d_data *cdata, basis_s *v, simplex *s, int k) {

    point	va = VA(v),
		vb = VB(v);
    int	i,j;
    double	dd;
    double	scale;
    basis_s	*snibv;
    neighbor *sni;
    static int failcount;

    v->sqa = v->sqb = Norm2(cdata, vb);
    if (k<=1) {
	memcpy(vb,va,cdata->basis_vec_size);
	return 1;
    }
    for (j=0;j<250;j++) {

	memcpy(vb,va,cdata->basis_vec_size);
	for (i=k-1,sni=s->neigh+k-1;i>0;i--,sni--) {
	    snibv = sni->basis;
	    dd = -Vec_dot(cdata, VB(snibv),vb)/ snibv->sqb;
	    Ax_plus_y(cdata, dd, VA(snibv), vb);
	}
	v->sqb = Norm2(cdata, vb);
	v->sqa = Norm2(cdata, va);

	if (2*v->sqb >= v->sqa) {cdata->B[j]++; return 1;}

	Vec_scale_test(cdata, cdata->rdim,scale = sc(cdata,v,s,k,j),va);

	for (i=k-1,sni=s->neigh+k-1;i>0;i--,sni--) {
	    snibv = sni->basis;
	    dd = -Vec_dot(cdata, VB(snibv),va)/snibv->sqb;
	    dd = floor(dd+0.5);
	    Ax_plus_y_test(cdata, dd, VA(snibv), va);
	}
    }
    if (failcount++<10) {
	DEB(-8, reduce_inner failed on:)
	    DEBTR(-8) print_basis(cdata, cdata->DFILE, v);
	print_simplex_f(cdata, s, cdata->DFILE, &print_neighbor_full);
    }
    return 0;
}

static int reduce(struct chull3d_data *cdata, basis_s **v, point p, simplex *s, int k) {

    point	z;
    point	tt = s->neigh[0].vert;

    if (!*v) NEWLRC(cdata, cdata->basis_s_list,basis_s,(*v))
    else (*v)->lscale = 0;
    z = VB(*v);
    if (cdata->vd) {
	if (p==cdata->hull_infinity) memcpy(*v,cdata->infinity_basis,cdata->basis_s_size);
	else {trans(z,p,tt); lift(cdata,z,s);}
    } else trans(z,p,tt);
    return reduce_inner(cdata,*v,s,k);
}

void get_basis_sede(struct chull3d_data *cdata, simplex *s) {

    int	k=1;
    neighbor *sn = s->neigh+1,
	     *sn0 = s->neigh;

    if (cdata->vd && sn0->vert == cdata->hull_infinity && (cdata->cdim) >1) {
	SWAP(neighbor, *sn0, *sn );
	NULLIFY(cdata, basis_s,sn0->basis);
	sn0->basis = cdata->tt_basis;
	cdata->tt_basis->ref_count++;
    } else {
	if (!sn0->basis) {
	    sn0->basis = cdata->tt_basis;
	    cdata->tt_basis->ref_count++;
	} else while (k < (cdata->cdim) && sn->basis) {k++;sn++;}
    }
    while (k<(cdata->cdim)) {
	NULLIFY(cdata, basis_s,sn->basis);
	reduce(cdata, &sn->basis,sn->vert,s,k);
	k++; sn++;
    }
}


int out_of_flat(struct chull3d_data *cdata, simplex *root, point p) {

    static neighbor p_neigh={0,0,0};

    if (!p_neigh.basis) p_neigh.basis = (basis_s*) malloc(cdata->basis_s_size);

    p_neigh.vert = p;
    (cdata->cdim)++;
    root->neigh[(cdata->cdim)-1].vert = root->peak.vert;
    NULLIFY(cdata, basis_s,root->neigh[(cdata->cdim)-1].basis);
    get_basis_sede(cdata, root);
    if (cdata->vd && root->neigh[0].vert == cdata->hull_infinity) return 1;
    reduce(cdata, &p_neigh.basis,p,root,(cdata->cdim));
    if (p_neigh.basis->sqa != 0) return 1;
    (cdata->cdim)--;
    return 0;
}


static double cosangle_sq(struct chull3d_data *cdata, basis_s* v,basis_s* w)  {
    double dd;
    point	vv=v->vecs,
		wv=w->vecs;
    dd = Vec_dot(cdata, vv,wv);
    return dd*dd/Norm2(cdata, vv)/Norm2(cdata, wv);
}


int check_perps(struct chull3d_data *cdata, simplex *s) {

    static basis_s *b = NULL;
    point 	z,y;
    point	tt;
    double	dd;
    int i,j;

    for (i=1; i<(cdata->cdim); i++) if (NEARZERO(s->neigh[i].basis->sqb)) return 0;
    if (!b) b = (basis_s*)malloc(cdata->basis_s_size);
    else b->lscale = 0;
    z = VB(b);
    tt = s->neigh[0].vert;
    for (i=1;i<(cdata->cdim);i++) {
	y = s->neigh[i].vert;
	if (cdata->vd && y==cdata->hull_infinity) memcpy(b, cdata->infinity_basis, cdata->basis_s_size);
	else {trans(z,y,tt); lift(cdata,z,s);}
	if (s->normal && cosangle_sq(cdata,b,s->normal)>cdata->b_err_min_sq) {DEBS(0)
	    DEB(0,bad normal) DEBEXP(0,i) DEBEXP(0,dd)
		print_simplex_f(cdata, s, cdata->DFILE, &print_neighbor_full);
	    EDEBS
		return 0;
	}
	for (j=i+1;j<(cdata->cdim);j++) {
	    if (cosangle_sq(cdata,b,s->neigh[j].basis)>cdata->b_err_min_sq) {
		DEBS(0)
		    DEB(0,bad basis)DEBEXP(0,i) DEBEXP(0,j) DEBEXP(0,dd)
		    DEBTR(-8) print_basis(cdata, cdata->DFILE, b);
		print_simplex_f(cdata, s, cdata->DFILE, &print_neighbor_full);
		EDEBS
		    return 0;
	    }
	}
    }
    return 1;
}

int sees(struct chull3d_data *, site, simplex *);

void get_normal_sede(struct chull3d_data *cdata, simplex *s) {

    neighbor *rn;
    int i,j;

    get_basis_sede(cdata, s);
    if (cdata->rdim==3 && (cdata->cdim)==3) {
	point	c,
		a = VB(s->neigh[1].basis),
		b = VB(s->neigh[2].basis);
	NEWLRC(cdata, cdata->basis_s_list, basis_s,s->normal);
	c = VB(s->normal);
	c[0] = a[1]*b[2] - a[2]*b[1];
	c[1] = a[2]*b[0] - a[0]*b[2];
	c[2] = a[0]*b[1] - a[1]*b[0];
	s->normal->sqb = Norm2(cdata, c);
	for (i=(cdata->cdim)+1,rn = cdata->ch_root->neigh+(cdata->cdim)-1; i; i--, rn--) {
	    for (j = 0; j<(cdata->cdim) && rn->vert != s->neigh[j].vert;j++);
	    if (j<(cdata->cdim)) continue;
	    if (rn->vert==cdata->hull_infinity) {
		if (c[2] > -cdata->b_err_min) continue;
	    } else  if (!sees(cdata, rn->vert,s)) continue;
	    c[0] = -c[0]; c[1] = -c[1]; c[2] = -c[2];
	    break;
	}
	DEBS(-1) if (!check_perps(cdata,s)) exit(1); EDEBS
	    return;
    }

    for (i=(cdata->cdim)+1,rn = cdata->ch_root->neigh+(cdata->cdim)-1; i; i--, rn--) {
	for (j = 0; j<(cdata->cdim) && rn->vert != s->neigh[j].vert;j++);
	if (j<(cdata->cdim)) continue;
	reduce(cdata, &s->normal,rn->vert,s,(cdata->cdim));
	if (s->normal->sqb != 0) break;
    }

    DEBS(-1) if (!check_perps(cdata,s)) {DEBTR(-1) exit(1);} EDEBS

}


void get_normal(struct chull3d_data *cdata, simplex *s) {get_normal_sede(cdata,s); return;}


void print_site(struct chull3d_data *cdata, site p, FILE* F)
{print_point(F, cdata->pdim,p);fprintf(F, "\n");}

int sees(struct chull3d_data *cdata, site p, simplex *s) {

    static basis_s *b = NULL;
    point	tt,zz;
    double	dd,dds;
    int i;


    if (!b) b = (basis_s*)malloc(cdata->basis_s_size);
    else b->lscale = 0;
    zz = VB(b);
    if ((cdata->cdim)==0) return 0;
    if (!s->normal) {
	get_normal_sede(cdata, s);
	for (i=0;i<(cdata->cdim);i++) NULLIFY(cdata, basis_s,s->neigh[i].basis);
    }
    tt = s->neigh[0].vert;
    if (cdata->vd) {
	if (p==cdata->hull_infinity) memcpy(b,cdata->infinity_basis,cdata->basis_s_size);
	else {trans(zz,p,tt); lift(cdata,zz,s);}
    } else trans(zz,p,tt);
    for (i=0;i<3;i++) {
	dd = Vec_dot(cdata, zz,s->normal->vecs);
	if (dd == 0.0) {
	    DEBS(-7) DEB(-6,degeneracy:); DEBEXP(-6,cdata->site_num((void *)cdata, p));
	    print_site(cdata, p, cdata->DFILE); print_simplex_f(cdata, s, cdata->DFILE, &print_neighbor_full); EDEBS
		return 0;
	}
	dds = dd*dd/s->normal->sqb/Norm2(cdata, zz);
	if (dds > cdata->b_err_min_sq) return (dd<0);
	get_basis_sede(cdata, s);
	reduce_inner(cdata,b,s,(cdata->cdim));
    }
    DEBS(-7) if (i==3) {
	DEB(-6, looped too much in sees);
	DEBEXP(-6,dd) DEBEXP(-6,dds) DEBEXP(-6,cdata->site_num((void *)cdata, p));
	print_simplex_f(cdata, s, cdata->DFILE, &print_neighbor_full); exit(1);}
	EDEBS
	    return 0;
}


static double radsq(struct chull3d_data *cdata, simplex *s) {

    point n;
    neighbor *sn;
    int i;

    /* square of ratio of circumcircle radius to
       max edge length for Delaunay tetrahedra */

    for (i=0,sn=s->neigh;i<(cdata->cdim);i++,sn++)
	if (sn->vert == cdata->hull_infinity) return cdata->Huge;

    if (!s->normal) get_normal_sede(cdata, s);

    /* compute circumradius */
    n = s->normal->vecs;

    if (NEARZERO(n[cdata->rdim-1])) {return cdata->Huge;}

    return Vec_dot_pdim(cdata,n,n)/4/n[cdata->rdim-1]/n[cdata->rdim-1];
}


static void *zero_marks(struct chull3d_data *cdata, simplex * s, void *dum) { s->mark = 0; return NULL; }

static void *one_marks(struct chull3d_data *cdata, simplex * s, void *dum) {s->mark = 1; return NULL;}

int alph_test(struct chull3d_data *cdata, simplex *s, int i, void *alphap) {
    /*returns 1 if not an alpha-facet */

    simplex *si;
    double	rs,rsi,rsfi;
    neighbor *scn, *sin;
    int k, nsees, ssees;
    static double alpha;

    if (alphap) {alpha=*(double*)alphap; if (!s) return 1;}
    if (i==-1) return 0;

    si = s->neigh[i].simp;
    scn = s->neigh+(cdata->cdim)-1;
    sin = s->neigh+i;
    nsees = 0;

    for (k=0;k<(cdata->cdim);k++) if (s->neigh[k].vert==cdata->hull_infinity && k!=i) return 1;
    rs = radsq(cdata, s);
    rsi = radsq(cdata, si);

    if (rs < alpha &&  rsi < alpha) return 1;

    swap_points(scn->vert,sin->vert);
    NULLIFY(cdata, basis_s, s->neigh[i].basis);
    (cdata->cdim)--;
    get_basis_sede(cdata, s);
    reduce(cdata, &s->normal,cdata->hull_infinity,s,(cdata->cdim));
    rsfi = radsq(cdata, s);

    for (k=0;k<(cdata->cdim);k++) if (si->neigh[k].simp==s) break;

    ssees = sees(cdata, scn->vert,s);
    if (!ssees) nsees = sees(cdata, si->neigh[k].vert,s);
    swap_points(scn->vert,sin->vert);
    (cdata->cdim)++;
    NULLIFY(cdata, basis_s, s->normal);
    NULLIFY(cdata, basis_s, s->neigh[i].basis);

    if (ssees) return alpha<rs;
    if (nsees) return alpha<rsi;

    assert(rsfi<=rs+FLT_EPSILON && rsfi<=rsi+FLT_EPSILON);

    return alpha<=rsfi;
}


static void *conv_facetv(struct chull3d_data *cdata, simplex *s, void *dum) {
    int i;
    for (i=0;i<(cdata->cdim);i++) if (s->neigh[i].vert==cdata->hull_infinity) {return s;}
    return NULL;
}


static void *mark_points(struct chull3d_data *cdata, simplex *s, void *dum) {
    int i, snum;
    neighbor *sn;

    for  (i=0,sn=s->neigh;i<(cdata->cdim);i++,sn++) {
	if (sn->vert==cdata->hull_infinity) continue;
	snum = cdata->site_num((void *)cdata, sn->vert);
	if (s->mark) cdata->mo[snum] = 1;
	else cdata->mi[snum] = 1;
    }
    return NULL;
}






static void vols(struct chull3d_data *cdata, fg *f, Tree *t, basis_s* n, int depth) {

    static simplex *s;
    static neighbor *sn;
    int tdim = (cdata->cdim);
    basis_s *nn = 0;
    int signum;
    point nnv;
    double sqq;


    if (!t) {return;}

    if (!s) {NEWL(cdata, cdata->simplex_list,simplex,s); sn = s->neigh;}
    (cdata->cdim) = depth;
    s->normal = n;
    if (depth>1 && sees(cdata, t->key,s)) signum = -1; else signum = 1;
    (cdata->cdim) = tdim;

    if (t->fgs->dist == 0) {
	sn[depth-1].vert = t->key;
	NULLIFY(cdata, basis_s,sn[depth-1].basis);
	(cdata->cdim) = depth; get_basis_sede(cdata, s); (cdata->cdim) = tdim;
	reduce(cdata, &nn, cdata->hull_infinity, s, depth);
	nnv = nn->vecs;
	if (t->key==cdata->hull_infinity || f->dist==cdata->Huge || NEARZERO(nnv[cdata->rdim-1]))
	    t->fgs->dist = cdata->Huge;
	else
	    t->fgs->dist = Vec_dot_pdim(cdata,nnv,nnv)
		/4/nnv[cdata->rdim-1]/nnv[cdata->rdim-1];
	if (!t->fgs->facets) t->fgs->vol = 1;
	else vols(cdata, t->fgs, t->fgs->facets, nn, depth+1);
    }

    assert(f->dist!=cdata->Huge || t->fgs->dist==cdata->Huge);
    if (t->fgs->dist==cdata->Huge || t->fgs->vol==cdata->Huge) f->vol = cdata->Huge;
    else {
	sqq = t->fgs->dist - f->dist;
	if (NEARZERO(sqq)) f->vol = 0;
	else f->vol += signum
	    *sqrt(sqq)
		*t->fgs->vol
		/((cdata->cdim)-depth+1);
    }
    vols(cdata, f, t->left, n, depth);
    vols(cdata, f, t->right, n, depth);

    return;
}




void free_hull_storage(struct chull3d_data *cdata) {
    free_basis_s_storage(cdata);
    free_simplex_storage(cdata);
    free_Tree_storage(cdata);
    free_fg_storage(cdata);
}

/* splay tree code */

/*
   Fri Oct 21 21:15:01 EDT 1994
   style changes, removed Sedgewickized...
   Ken Clarkson

*/
/*
   An implementation of top-down splaying with sizes
   D. Sleator <sleator@cs.cmu.edu>, January 1994.

   This extends top-down-splay.c to maintain a size field in each node.
   This is the number of nodes in the subtree rooted there.  This makes
   it possible to efficiently compute the rank of a key.  (The rank is
   the number of nodes to the left of the given key.)  It it also
   possible to quickly find the node of a given rank.  Both of these
   operations are illustrated in the code below.  The remainder of this
   introduction is taken from top-down-splay.c.

   "Splay trees", or "self-adjusting search trees" are a simple and
   efficient data structure for storing an ordered set.  The data
   structure consists of a binary tree, with no additional fields.  It
   allows searching, insertion, deletion, deletemin, deletemax,
   splitting, joining, and many other operations, all with amortized
   logarithmic performance.  Since the trees adapt to the sequence of
   requests, their performance on real access patterns is typically even
   better.  Splay trees are described in a number of texts and papers
   [1,2,3,4].

   The code here is adapted from simple top-down splay, at the bottom of
   page 669 of [2].  It can be obtained via anonymous ftp from
   spade.pc.cs.cmu.edu in directory /usr/sleator/public.

   The chief modification here is that the splay operation works even if the
   item being splayed is not in the tree, and even if the tree root of the
   tree is NULL.  So the line:

   t = splay(cdata, i, t);

   causes it to search for item with key i in the tree rooted at t.  If it's
   there, it is splayed to the root.  If it isn't there, then the node put
   at the root is the last one before NULL that would have been reached in a
   normal binary search for i.  (It's a neighbor of i in the tree.)  This
   allows many other operations to be easily implemented, as shown below.

   [1] "Data Structures and Their Algorithms", Lewis and Denenberg,
   Harper Collins, 1991, pp 243-251.
   [2] "Self-adjusting Binary Search Trees" Sleator and Tarjan,
   JACM Volume 32, No 3, July 1985, pp 652-686.
   [3] "Data Structure and Algorithm Analysis", Mark Weiss,
   Benjamin Cummins, 1992, pp 119-130.
   [4] "Data Structures, Algorithms, and Performance", Derick Wood,
   Addison-Wesley, 1993, pp 367-375

   Adding the following notice, which appears in the version of this file at
http://www.cs.cmu.edu/afs/cs.cmu.edu/user/sleator/public/splaying/ but did
not originally appear in the hull version:

The following code was written by Daniel Sleator, and is released
in the public domain.

The reworking for hull, insofar as copyright protection applies, would thus
be under the same license as hull.
*/


/* Splay using the key i (which may or may not be in the tree.) */
/* The starting root is t, and the tree used is defined by rat  */
/* size fields are maintained */
Tree * splay(struct chull3d_data *cdata, site i, Tree *t)
{
    Tree N, *l, *r, *y;
    int comp, root_size, l_size, r_size;

    if (!t) return t;
    N.left = N.right = NULL;
    l = r = &N;
    root_size = node_size(t);
    l_size = r_size = 0;

    for (;;) {
	comp = compare(cdata, i, t->key);
	if (comp < 0) {
	    if (!t->left) break;
	    if (compare(cdata, i, t->left->key) < 0) {
		y = t->left;                           /* rotate right */
		t->left = y->right;
		y->right = t;
		t->size = node_size(t->left) + node_size(t->right) + 1;
		t = y;
		if (!t->left) break;
	    }
	    r->left = t;                               /* link right */
	    r = t;
	    t = t->left;
	    r_size += 1+node_size(r->right);
	} else if (comp > 0) {
	    if (!t->right) break;
	    if (compare(cdata, i, t->right->key) > 0) {
		y = t->right;                          /* rotate left */
		t->right = y->left;
		y->left = t;
		t->size = node_size(t->left) + node_size(t->right) + 1;
		t = y;
		if (!t->right) break;
	    }
	    l->right = t;                              /* link left */
	    l = t;
	    t = t->right;
	    l_size += 1+node_size(l->left);
	} else break;
    }
    l_size += node_size(t->left);  /* Now l_size and r_size are the sizes of */
    r_size += node_size(t->right); /* the left and right trees we just built.*/
    t->size = l_size + r_size + 1;

    l->right = r->left = NULL;

    /* The following two loops correct the size fields of the right path  */
    /* from the left child of the root and the right path from the left   */
    /* child of the root.                                                 */
    for (y = N.right; y != NULL; y = y->right) {
	y->size = l_size;
	l_size -= 1+node_size(y->left);
    }
    for (y = N.left; y != NULL; y = y->left) {
	y->size = r_size;
	r_size -= 1+node_size(y->right);
    }

    l->right = t->left;                                /* assemble */
    r->left = t->right;
    t->left = N.right;
    t->right = N.left;

    return t;
}

/* Insert key i into the tree t, if it is not already there. */
/* Return a pointer to the resulting tree.                   */
Tree * insert(struct chull3d_data *cdata, site i, Tree * t) {
    Tree * new;

    if (t != NULL) {
	t = splay(cdata, i,t);
	if (compare(cdata, i, t->key)==0) {
	    return t;  /* it's already there */
	}
    }
    NEWL(cdata, cdata->Tree_list, Tree,new)
	if (!t) {
	    new->left = new->right = NULL;
	} else if (compare(cdata, i, t->key) < 0) {
	    new->left = t->left;
	    new->right = t;
	    t->left = NULL;
	    t->size = 1+node_size(t->right);
	} else {
	    new->right = t->right;
	    new->left = t;
	    t->right = NULL;
	    t->size = 1+node_size(t->left);
	}
    new->key = i;
    new->size = 1 + node_size(new->left) + node_size(new->right);
    return new;
}

/* Deletes i from the tree if it's there.               */
/* Return a pointer to the resulting tree.              */
Tree * delete(struct chull3d_data *cdata, site i, Tree *t) {
    Tree * x;
    int tsize;

    if (!t) return NULL;
    tsize = t->size;
    t = splay(cdata, i,t);
    if (compare(cdata, i, t->key) == 0) {               /* found it */
	if (!t->left) {
	    x = t->right;
	} else {
	    x = splay(cdata, i, t->left);
	    x->right = t->right;
	}
	FREEL(cdata, Tree,t);
	if (x) x->size = tsize-1;
	return x;
    } else {
	return t;                         /* It wasn't there */
    }
}

/* Returns a pointer to the node in the tree with the given rank.  */
/* Returns NULL if there is no such node.                          */
/* Does not change the tree.  To guarantee logarithmic behavior,   */
/* the node found here should be splayed to the root.              */
Tree *find_rank(int r, Tree *t) {
    int lsize;
    if ((r < 0) || (r >= node_size(t))) return NULL;
    for (;;) {
	lsize = node_size(t->left);
	if (r < lsize) {
	    t = t->left;
	} else if (r > lsize) {
	    r = r - lsize -1;
	    t = t->right;
	} else {
	    return t;
	}
    }
}

void printtree_flat_inner(Tree * t) {
    if (!t) return;

    printtree_flat_inner(t->right);
    printf("%p ", t->key);fflush(stdout);
    printtree_flat_inner(t->left);
}

void printtree_flat(Tree * t) {
    if (!t) {
	printf("<empty tree>");
	return;
    }
    printtree_flat_inner(t);
}


void printtree(Tree * t, int d) {
    int i;
    if (!t) return;

    printtree(t->right, d+1);
    for (i=0; i<d; i++) printf("  ");
    printf("%p(%d)\n", t->key, t->size);fflush(stdout);
    printtree(t->left, d+1);
}


fg *find_fg(struct chull3d_data *cdata, simplex *s,int q) {

    fg *f;
    neighbor *si, *sn = s->neigh;
    Tree *t;

    if (q==0) return cdata->faces_gr_t;
    if (!cdata->faces_gr_t) NEWLRC(cdata, cdata->fg_list, fg, cdata->faces_gr_t);
    f = cdata->faces_gr_t;
    for (si=sn; si<sn+(cdata->cdim); si++) if (q & (1<<(si-sn))) {
	t = f->facets = insert(cdata, si->vert,f->facets);
	if (!t->fgs) NEWLRC(cdata, cdata->fg_list, fg, (t->fgs))
	    f = t->fgs;
    }
    return f;
}

void *add_to_fg(struct chull3d_data *cdata, simplex *s, void *dum) {

    neighbor t, *si, *sj, *sn = s->neigh;
    fg *fq;
    int q,m,Q=1<<(cdata->cdim);
    /* sort neigh by site number */
    for (si=sn+2;si<sn+(cdata->cdim);si++)
	for (sj=si; sj>sn+1 && snkey(cdata, sj-1) > snkey(cdata, sj); sj--)
	{t=*(sj-1); *(sj-1) = *sj; *sj = t;}

    NULLIFY(cdata, basis_s,s->normal);
    NULLIFY(cdata, basis_s,s->neigh[0].basis);

    /* insert subsets */
    for (q=1; q<Q; q++) find_fg(cdata, s,q);

    /* include all superset relations */
    for (q=1; q<Q; q++) {
	fq = find_fg(cdata,s,q);
	assert(fq);
	for (m=1,si=sn;si<sn+(cdata->cdim);si++,m<<=1) if (!(q&m)) {
	    fq->facets = insert(cdata, si->vert,fq->facets);
	    fq->facets->fgs = find_fg(cdata,s, q|m);
	}
    }
    return NULL;
}

void visit_fg_i(struct chull3d_data *cdata, void (*v_fg)(struct chull3d_data *,Tree *, int, int),
	Tree *t, int depth, int vn, int boundary) {
    int	boundaryc = boundary;

    if (!t) return;

    assert(t->fgs);
    if (t->fgs->mark!=vn) {
	t->fgs->mark = vn;
	if (t->key!=cdata->hull_infinity && !cdata->mo[cdata->site_num((void *)cdata, t->key)]) boundaryc = 0;
	v_fg(cdata, t,depth, boundaryc);
	visit_fg_i(cdata, v_fg, t->fgs->facets,depth+1, vn, boundaryc);
    }
    visit_fg_i(cdata, v_fg, t->left,depth,vn, boundary);
    visit_fg_i(cdata, v_fg, t->right,depth,vn,boundary);
}

void visit_fg(struct chull3d_data *cdata, fg *faces_gr, void (*v_fg)(struct chull3d_data *, Tree *, int, int)) {
    static int fg_vn;
    visit_fg_i(cdata, v_fg, faces_gr->facets, 0, ++fg_vn, 1);
}

int visit_fg_i_far(struct chull3d_data *cdata, void (*v_fg)(struct chull3d_data *, Tree *, int),
	Tree *t, int depth, int vn) {
    int nb = 0;

    if (!t) return 0;

    assert(t->fgs);
    if (t->fgs->mark!=vn) {
	t->fgs->mark = vn;
	nb = (t->key==cdata->hull_infinity) || cdata->mo[cdata->site_num((void *)cdata, t->key)];
	if (!nb && !visit_fg_i_far(cdata, v_fg, t->fgs->facets,depth+1,vn))
	    v_fg(cdata, t,depth);
    }
    nb = visit_fg_i_far(cdata, v_fg, t->left,depth,vn) || nb;
    nb = visit_fg_i_far(cdata, v_fg, t->right,depth,vn) || nb;
    return nb;
}

void visit_fg_far(struct chull3d_data *cdata, fg *faces_gr, void (*v_fg)(struct chull3d_data *, Tree *, int)) {
    static int fg_vn;
    visit_fg_i_far(cdata, v_fg,faces_gr->facets, 0, --fg_vn);
}

void p_fg(struct chull3d_data *cdata, Tree* t, int depth, int bad) {
    static int fa[MAXDIM];
    int i;
    static double mults[MAXDIM];

    if (mults[0]==0) {
	mults[cdata->pdim] = 1;
	for (i=cdata->pdim-1; i>=0; i--) mults[i] = cdata->mult_up*mults[i+1];
    }

    fa[depth] = cdata->site_num((void *)cdata, t->key);
    for (i=0;i<=depth;i++)
	fprintf(cdata->FG_OUT, "%d ", fa[i]);
    fprintf(cdata->FG_OUT, "	%G\n", t->fgs->vol/mults[depth]);
}

void print_fg(struct chull3d_data *cdata, fg *faces_gr, FILE *F) {cdata->FG_OUT=F; visit_fg(cdata,faces_gr, p_fg);}

void find_volumes(struct chull3d_data *cdata, fg *faces_gr, FILE *F) {
    if (!faces_gr) return;
    vols(cdata, faces_gr, faces_gr->facets, 0, 1);
    print_fg(cdata,faces_gr, F);
}



void p_fg_x(struct chull3d_data *cdata, Tree*t, int depth, int bad) {

    static int fa[MAXDIM];
    static point fp[MAXDIM];
    int i;

    fa[depth] = cdata->site_num((void *)cdata, t->key);
    fp[depth] = t->key;

    if (depth==cdata->p_fg_x_depth) for (i=0;i<=depth;i++)
	fprintf(cdata->FG_OUT, "%d%s", fa[i], (i==depth) ? "\n" : " ");
}

void print_fg_alt(struct chull3d_data *cdata, fg *faces_gr, FILE *F, int fd) {
    cdata->FG_OUT=F;
    if (!faces_gr) return;
    cdata->p_fg_x_depth = fd;
    visit_fg(cdata, faces_gr, p_fg_x);
    fclose(cdata->FG_OUT);
}





void h_fg(struct chull3d_data *cdata, Tree *t, int depth, int bad) {
    if (!t->fgs->facets) return;
    if (bad) {
	cdata->fg_hist_bad[depth][t->fgs->facets->size]++;
	return;
    }
    cdata->fg_hist[depth][t->fgs->facets->size]++;
}

void h_fg_far(struct chull3d_data *cdata, Tree* t, int depth) {
    if (t->fgs->facets) cdata->fg_hist_far[depth][t->fgs->facets->size]++;
}


/* visit all simplices with facets of the current hull */
typedef void* visit_func(struct chull3d_data *, simplex *, void *);

static void *facet_test(struct chull3d_data *cdata, simplex *s, void *dummy) {return (!s->peak.vert) ? s : NULL;}
static int hullt(struct chull3d_data *cdata, simplex *s, int i, void *dummy) {return i>-1;}





FILE* efopen(struct chull3d_data *cdata, char *file, char *mode) {
    FILE* fp;
    if ((fp = fopen(file, mode))) return fp;
    fprintf(cdata->DFILE, "couldn't open file %s mode %s\n",file,mode);
    exit(1);
    return NULL;
}


#ifndef WIN32
FILE* epopen(char *com, char *mode) {
    FILE* fp;
    if ((fp = popen(com, mode))) return fp;
    fprintf(stderr, "couldn't open stream %s mode %s\n",com,mode);
    exit(1);
    return 0;
}
#endif

/* hull.c : "combinatorial" functions for hull computation */

typedef int test_func(struct chull3d_data *, simplex *, int, void *);

void *visit_triang_gen(struct chull3d_data *cdata, simplex *s, visit_func *visit, test_func *test) {
    /*
     * starting at s, visit simplices t such that test(s,i,0) is true,
     * and t is the i'th neighbor of s;
     * apply visit function to all visited simplices;
     * when visit returns nonNULL, exit and return its value
     */
    neighbor *sn;
    void *v;
    simplex *t;
    int i;
    long tms = 0;
    static long vnum = -1;
    static long ss = 2000;
    static simplex **st;

    vnum--;
    if (!st) st=(simplex**)malloc((ss+MAXDIM+1)*sizeof(simplex*));
    if (s) push(s);
    while (tms) {

	if (tms>ss) {DEBEXP(-1,tms);
	    st=(simplex**)realloc(st,
		    ((ss+=ss)+MAXDIM+1)*sizeof(simplex*));
	}
	pop(t);
	if (!t || t->visit == vnum) continue;
	t->visit = vnum;
	if ((v=(*visit)(cdata,t,0))) {return v;}
	for (i=-1,sn = t->neigh-1;i<(cdata->cdim);i++,sn++)
	    if ((sn->simp->visit != vnum) && sn->simp && test(cdata,t,i,0))
		push(sn->simp);
    }
    return NULL;
}

static int truet(struct chull3d_data *cdata, simplex *s, int i, void *dum) {return 1;}

void *visit_triang(struct chull3d_data *cdata, simplex *root, visit_func *visit)
    /* visit the whole triangulation */
{return visit_triang_gen(cdata, root, visit, truet);}


void *visit_hull(struct chull3d_data *cdata, simplex *root, visit_func *visit){
    return visit_triang_gen(cdata, visit_triang(cdata, root, &facet_test), visit, hullt);}


fg *build_fg(struct chull3d_data *cdata, simplex *root) {
    cdata->faces_gr_t= 0;
    visit_hull(cdata, root, add_to_fg);
    return cdata->faces_gr_t;
}




void print_triang(struct chull3d_data *cdata, simplex *root, FILE *F, print_neighbor_f *pnf) {
    print_simplex(cdata, 0,F);
    print_simplex_f(cdata, 0,0,pnf);
    visit_triang(cdata, root, print_simplex);
}


void *check_simplex(struct chull3d_data *cdata, simplex *s, void *dum){

    int i,j,k,l;
    neighbor *sn, *snn, *sn2;
    simplex *sns;
    site vn;

    for (i=-1,sn=s->neigh-1;i<(cdata->cdim);i++,sn++) {
	sns = sn->simp;
	if (!sns) {
	    fprintf(cdata->DFILE, "check_triang; bad simplex\n");
	    print_simplex_f(cdata, s, cdata->DFILE, &print_neighbor_full); fprintf(cdata->DFILE, "site_num(p)=%ld\n",cdata->site_num((void *)cdata,cdata->p));
	    return s;
	}
	if (!s->peak.vert && sns->peak.vert && i!=-1) {
	    fprintf(cdata->DFILE, "huh?\n");
	    print_simplex_f(cdata, s, cdata->DFILE, &print_neighbor_full);
	    print_simplex_f(cdata, sns, cdata->DFILE, &print_neighbor_full);
	    exit(1);
	}
	for (j=-1,snn=sns->neigh-1; j<(cdata->cdim) && snn->simp!=s; j++,snn++);
	if (j==(cdata->cdim)) {
	    fprintf(cdata->DFILE, "adjacency failure:\n");
	    DEBEXP(-1,cdata->site_num((void *)cdata, cdata->p))
		print_simplex_f(cdata, sns, cdata->DFILE, &print_neighbor_full);
	    print_simplex_f(cdata, s, cdata->DFILE, &print_neighbor_full);
	    exit(1);
	}
	for (k=-1,snn=sns->neigh-1; k<(cdata->cdim); k++,snn++){
	    vn = snn->vert;
	    if (k!=j) {
		for (l=-1,sn2 = s->neigh-1;
			l<(cdata->cdim) && sn2->vert != vn;
			l++,sn2++);
		if (l==(cdata->cdim)) {
		    fprintf(cdata->DFILE, "(cdata->cdim)=%d\n",(cdata->cdim));
		    fprintf(cdata->DFILE, "error: neighboring simplices with incompatible vertices:\n");
		    print_simplex_f(cdata, sns, cdata->DFILE, &print_neighbor_full);
		    print_simplex_f(cdata, s, cdata->DFILE, &print_neighbor_full);
		    exit(1);
		}
	    }
	}
    }
    return NULL;
}

static int p_neight(struct chull3d_data *cdata, simplex *s, int i, void *dum) {return s->neigh[i].vert !=cdata->p;}

void check_triang(struct chull3d_data *cdata, simplex *root){visit_triang(cdata, root, &check_simplex);}






/* outfuncs: given a list of points, output in a given format */


void vlist_out(struct chull3d_data *cdata, point *v, int vdim, FILE *Fin, int amble) {

    static FILE *F;
    int j;

    if (Fin) {F=Fin; if (!v) return;}

    for (j=0;j<vdim;j++) fprintf(F, "%ld ", (cdata->site_num)((void *)cdata, v[j]));
    fprintf(F,"\n");

    return;
}


void cpr_out(struct chull3d_data *cdata, point *v, int vdim, FILE *Fin, int amble) {

    static FILE *F;
    int i;

    if (Fin) {F=Fin; if (!v) return;}

    for (i=0;i<vdim;i++) if (v[i]==cdata->hull_infinity) return;

    fprintf(F, "t %G %G %G %G %G %G %G %G %G 3 128\n",
	    v[0][0]/cdata->mult_up,v[0][1]/cdata->mult_up,v[0][2]/cdata->mult_up,
	    v[1][0]/cdata->mult_up,v[1][1]/cdata->mult_up,v[1][2]/cdata->mult_up,
	    v[2][0]/cdata->mult_up,v[2][1]/cdata->mult_up,v[2][2]/cdata->mult_up
	   );
}


void off_out(struct chull3d_data *cdata, point *v, int vdim, FILE *Fin, int amble) {

    static FILE *F, *G;
    static FILE *OFFFILE;
    static char offfilenam[MAXPATHLEN];
    char comst[100], buf[200];
    int j,i;

    if (Fin) {F=Fin;}

    if (cdata->pdim!=3) { warning(-10, off apparently for 3d points only); return;}

    if (amble==0) {
	for (i=0;i<vdim;i++) if (v[i]==cdata->hull_infinity) return;
	fprintf(OFFFILE, "%d ", vdim);
	for (j=0;j<vdim;j++) fprintf(OFFFILE, "%ld ", (cdata->site_num)((void *)cdata, v[j]));
	fprintf(OFFFILE,"\n");
    } else if (amble==-1) {
	OFFFILE = bu_temp_file((char *)&offfilenam, MAXPATHLEN);
    } else {
	fclose(OFFFILE);

	fprintf(F, "    OFF\n");

	sprintf(comst, "wc %s", cdata->tmpfilenam);
	G = epopen(comst, "r");
	fscanf(G, "%d", &i);
	fprintf(F, " %d", i);
	pclose(G);

	sprintf(comst, "wc %s", offfilenam);
	G = epopen(comst, "r");
	fscanf(G, "%d", &i);
	fprintf(F, " %d", i);
	pclose(G);

	fprintf (F, " 0\n");


	G = efopen(cdata, cdata->tmpfilenam, "r");
	while (fgets(buf, sizeof(buf), G)) fprintf(F, "%s", buf);
	fclose(G);

	G = efopen(cdata, offfilenam, "r");


	while (fgets(buf, sizeof(buf), G)) fprintf(F, "%s", buf);
	fclose(G);
    }

}



/* vist_funcs for different kinds of output: facets, alpha shapes, etc. */
typedef void out_func(struct chull3d_data *, point *, int, FILE*, int);

void *facets_print(struct chull3d_data *cdata, simplex *s, void *p) {

    static out_func *out_func_here;
    point v[MAXDIM];
    int j;

    if (p) {out_func_here = (out_func*)p; if (!s) return NULL;}

    for (j=0;j<(cdata->cdim);j++) v[j] = s->neigh[j].vert;

    out_func_here(cdata, v,(cdata->cdim),0,0);

    return NULL;
}


void *ridges_print(struct chull3d_data *cdata, simplex *s, void *p) {

    static out_func *out_func_here;
    point v[MAXDIM];
    int j,k,vnum;

    if (p) {out_func_here = (out_func*)p; if (!s) return NULL;}

    for (j=0;j<(cdata->cdim);j++) {
	vnum=0;
	for (k=0;k<(cdata->cdim);k++) {
	    if (k==j) continue;
	    v[vnum++] = (s->neigh[k].vert);
	}
	out_func_here(cdata, v,(cdata->cdim)-1,0,0);
    }
    return NULL;
}



void *afacets_print(struct chull3d_data *cdata, simplex *s, void *p) {

    static out_func *out_func_here;
    point v[MAXDIM];
    int j,k,vnum;

    if (p) {out_func_here = (out_func*)p; if (!s) return NULL;}

    for (j=0;j<(cdata->cdim);j++) { /* check for ashape consistency */
	for (k=0;k<(cdata->cdim);k++) if (s->neigh[j].simp->neigh[k].simp==s) break;
	if (alph_test(cdata, s,j,0)!=alph_test(cdata, s->neigh[j].simp,k,0)) {
	    DEB(-10,alpha-shape not consistent)
		DEBTR(-10)
		print_simplex_f(cdata, s,cdata->DFILE,&print_neighbor_full);
	    print_simplex_f(cdata, s->neigh[j].simp,cdata->DFILE,&print_neighbor_full);
	    fflush(cdata->DFILE);
	    exit(1);
	}
    }
    for (j=0;j<(cdata->cdim);j++) {
	vnum=0;
	if (alph_test(cdata,s,j,0)) continue;
	for (k=0;k<(cdata->cdim);k++) {
	    if (k==j) continue;
	    v[vnum++] = s->neigh[k].vert;
	}
	out_func_here(cdata, v,(cdata->cdim)-1,0,0);
    }
    return NULL;
}

int rand(void);
void srand(unsigned int);

/* TODO -make contingent on test */
#ifdef WIN32
double logb(double x) {
    if (x<=0) return -1e305;
    return log(x)/log(2);
}
#endif



Coord maxdist(int dim, point p1, point p2) {
    Coord	x,y,
		d = 0;
    int i = dim;


    while (i--) {
	x = *p1++;
	y = *p2++;
	d += (x<y) ? y-x : x-y ;
    }

    return d;
}


void check_new_triangs(struct chull3d_data *cdata, simplex *s){visit_triang_gen(cdata, s, check_simplex, p_neight);}

void* visit_outside_ashape(struct chull3d_data *cdata, simplex *root, visit_func visit) {
    return visit_triang_gen(cdata, visit_hull(cdata, root, conv_facetv), visit, alph_test);
}

static int check_ashape(struct chull3d_data *cdata, simplex *root, double alpha) {

    int i;

    for (i=0;i<MAXPOINTS;i++) {cdata->mi[i] = cdata->mo[i] = 0;}

    visit_hull(cdata, root, zero_marks);

    alph_test(cdata, 0,0,&alpha);
    visit_outside_ashape(cdata, root, one_marks);

    visit_hull(cdata, root, mark_points);

    for (i=0;i<MAXPOINTS;i++) if (cdata->mo[i] && !cdata->mi[i]) {return 0;}

    return 1;
}

double find_alpha(struct chull3d_data *cdata, simplex *root) {

    int i;
    float al=0,ah=0,am;

    for (i=0;i<cdata->pdim;i++) ah += (float)((cdata->maxs[i]-cdata->mins[i])*(cdata->maxs[i]-cdata->mins[i]));
    check_ashape(cdata, root,ah);
    for (i=0;i<17;i++) {
	if (check_ashape(cdata, root, am = (al+ah)/2)) ah = am;
	else al = am;
	if ((ah-al)/ah<.5) break;
    }
    return 1.1*ah;
}

void print_hist_fg(struct chull3d_data *cdata, simplex *root, fg *faces_gr, FILE *F) {
    int i,j,k;
    double tot_good[100], tot_bad[100], tot_far[100];
    for (i=0;i<20;i++) {
	tot_good[i] = tot_bad[i] = tot_far[i] = 0;
	for (j=0;j<100;j++) {
	    cdata->fg_hist[i][j]= cdata->fg_hist_bad[i][j]= cdata->fg_hist_far[i][j] = 0;
	}
    }
    if (!root) return;

    find_alpha(cdata, root);

    if (!faces_gr) faces_gr = build_fg(cdata, root);

    visit_fg(cdata, faces_gr, h_fg);
    visit_fg_far(cdata, faces_gr, h_fg_far);

    for (j=0;j<100;j++) for (i=0;i<20;i++) {
	tot_good[i] += cdata->fg_hist[i][j];
	tot_bad[i] += cdata->fg_hist_bad[i][j];
	tot_far[i]  += cdata->fg_hist_far[i][j];
    }

    for (i=19;i>=0 && !tot_good[i] && !tot_bad[i]; i--);
    fprintf(F,"totals	");
    for (k=0;k<=i;k++) {
	if (k==0) fprintf(F, "	");
	else fprintf(F,"			");
	fprintf(F, "%d/%d/%d",
		(int)tot_far[k], (int)tot_good[k], (int)tot_good[k] + (int)tot_bad[k]);
    }


    for (j=0;j<100;j++) {
	for (i=19; i>=0 && !cdata->fg_hist[i][j] && !cdata->fg_hist_bad[i][j]; i--);
	if (i==-1) continue;
	fprintf(F, "\n%d	",j);fflush(F);

	for (k=0;k<=i;k++) {
	    if (k==0) fprintf(F, "	");
	    else fprintf(F,"			");
	    if (cdata->fg_hist[k][j] || cdata->fg_hist_bad[k][j])
		fprintf(F,
			"%2.1f/%2.1f/%2.1f",
			tot_far[k] ? 100*cdata->fg_hist_far[k][j]/tot_far[k]+.05 : 0,
			tot_good[k] ? 100*cdata->fg_hist[k][j]/tot_good[k]+.05 : 0,
			100*(cdata->fg_hist[k][j]+cdata->fg_hist_bad[k][j])/(tot_good[k]+tot_bad[k])+.05
		       );
	}
    }
    fprintf(F, "\n");
}



neighbor *op_simp(struct chull3d_data *cdata, simplex *a, simplex *b) {lookup(cdata, a,b,simp,simplex)}
/* the neighbor entry of a containing b */

neighbor *op_vert(struct chull3d_data *cdata, simplex *a, site b) {lookup(cdata, a,b,vert,site)}
/* the neighbor entry of a containing b */


static void connect(struct chull3d_data *cdata, simplex *s) {
    /* make neighbor connections between newly created simplices incident to p */

    site xf,xb,xfi;
    simplex *sb, *sf, *seen;
    int i;
    neighbor *sn;

    if (!s) return;
    assert(!s->peak.vert
	    && s->peak.simp->peak.vert==cdata->p
	    && !op_vert(cdata,s,cdata->p)->simp->peak.vert);
    if (s->visit==cdata->pnum) return;
    s->visit = cdata->pnum;
    seen = s->peak.simp;
    xfi = op_simp(cdata,seen,s)->vert;
    for (i=0, sn = s->neigh; i<(cdata->cdim); i++,sn++) {
	xb = sn->vert;
	if (cdata->p == xb) continue;
	sb = seen;
	sf = sn->simp;
	xf = xfi;
	if (!sf->peak.vert) {	/* are we done already? */
	    sf = op_vert(cdata,seen,xb)->simp;
	    if (sf->peak.vert) continue;
	} else do {
	    xb = xf;
	    xf = op_simp(cdata,sf,sb)->vert;
	    sb = sf;
	    sf = op_vert(cdata,sb,xb)->simp;
	} while (sf->peak.vert);

	sn->simp = sf;
	op_vert(cdata,sf,xf)->simp = s;

	connect(cdata, sf);
    }

}


static simplex *make_facets(struct chull3d_data *cdata, simplex *seen) {
    /*
     * visit simplices s with sees(p,s), and make a facet for every neighbor
     * of s not seen by p
     */

    simplex *n;
    static simplex *ns;
    neighbor *bn;
    int i;


    if (!seen) return NULL;
    DEBS(-1) assert(sees(cdata, cdata->p,seen) && !seen->peak.vert); EDEBS
	seen->peak.vert = cdata->p;

    for (i=0,bn = seen->neigh; i<(cdata->cdim); i++,bn++) {
	n = bn->simp;
	if (cdata->pnum != n->visit) {
	    n->visit = cdata->pnum;
	    if (sees(cdata, cdata->p,n)) make_facets(cdata, n);
	}
	if (n->peak.vert) continue;
	copy_simp(cdata, ns,seen,cdata->simplex_list,cdata->simplex_size);
	ns->visit = 0;
	ns->peak.vert = 0;
	ns->normal = 0;
	ns->peak.simp = seen;
	NULLIFY(cdata, basis_s,ns->neigh[i].basis);
	ns->neigh[i].vert = cdata->p;
	bn->simp = op_simp(cdata,n,seen)->simp = ns;
    }
    return ns;
}



static simplex *extend_simplices(struct chull3d_data *cdata, simplex *s) {
    /*
     * p lies outside flat containing previous sites;
     * make p a vertex of every current simplex, and create some new simplices
     */

    int	i,
	ocdim=(cdata->cdim)-1;
    simplex *ns;
    neighbor *nsn;

    if (s->visit == cdata->pnum) return s->peak.vert ? s->neigh[ocdim].simp : s;
    s->visit = cdata->pnum;
    s->neigh[ocdim].vert = cdata->p;
    NULLIFY(cdata, basis_s,s->normal);
    NULLIFY(cdata, basis_s,s->neigh[0].basis);
    if (!s->peak.vert) {
	s->neigh[ocdim].simp = extend_simplices(cdata, s->peak.simp);
	return s;
    } else {
	copy_simp(cdata, ns,s,cdata->simplex_list,cdata->simplex_size);
	s->neigh[ocdim].simp = ns;
	ns->peak.vert = NULL;
	ns->peak.simp = s;
	ns->neigh[ocdim] = s->peak;
	inc_ref(basis_s,s->peak.basis);
	for (i=0,nsn=ns->neigh;i<(cdata->cdim);i++,nsn++)
	    nsn->simp = extend_simplices(cdata, nsn->simp);
    }
    return ns;
}


static simplex *search(struct chull3d_data *cdata, simplex *root) {
    /* return a simplex s that corresponds to a facet of the
     * current hull, and sees(p, s) */

    simplex *s;
    static simplex **st;
    static long ss = MAXDIM;
    neighbor *sn;
    int i;
    long tms = 0;

    if (!st) st = (simplex **)malloc((ss+MAXDIM+1)*sizeof(simplex*));
    push(root->peak.simp);
    root->visit = cdata->pnum;
    if (!sees(cdata, cdata->p,root))
	for (i=0,sn=root->neigh;i<(cdata->cdim);i++,sn++) push(sn->simp);
    while (tms) {
	if (tms>ss)
	    st=(simplex**)realloc(st,
		    ((ss+=ss)+MAXDIM+1)*sizeof(simplex*));
	pop(s);
	if (s->visit == cdata->pnum) continue;
	s->visit = cdata->pnum;
	if (!sees(cdata, cdata->p,s)) continue;
	if (!s->peak.vert) return s;
	for (i=0, sn=s->neigh; i<(cdata->cdim); i++,sn++) push(sn->simp);
    }
    return NULL;
}


static point get_another_site(struct chull3d_data *cdata) {

    static int scount =0;
    point pnext;

    if (!(++scount%1000)) {fprintf(cdata->DFILE,"site %d...", scount);}
    pnext = (*(cdata->get_site))((void *)cdata);
    if (!pnext) return NULL;
    cdata->pnum = cdata->site_num((void *)cdata, pnext)+2;
    return pnext;
}



void buildhull(struct chull3d_data *cdata, simplex *root)
{

    while ((cdata->cdim) < cdata->rdim) {
	cdata->p = get_another_site(cdata);
	if (!cdata->p) return;
	if (out_of_flat(cdata, root,cdata->p))
	    extend_simplices(cdata, root);
	else
	    connect(cdata, make_facets(cdata, search(cdata, root)));
    }
    while ((cdata->p = get_another_site(cdata)))
	connect(cdata, make_facets(cdata, search(cdata, root)));
}


simplex *build_convex_hull(struct chull3d_data *cdata, gsitef *get_s, site_n *site_numm, short dim, short vdd) {

    /*
       get_s		returns next site each call;
       hull construction stops when NULL returned;
       site_numm	returns number of site when given site;
       dim		dimension of point set;
       vdd		if (vdd) then return Delaunay triangulation


*/

    simplex *s, *root;

    if (!cdata->Huge) cdata->Huge = DBL_MAX*DBL_MAX;

    (cdata->cdim) = 0;
    cdata->get_site = get_s;
    cdata->site_num = site_numm;
    cdata->pdim = dim;
    cdata->vd = vdd;

    cdata->exact_bits = (int)floor(DBL_MANT_DIG*log(FLT_RADIX)/log(2));
    cdata->b_err_min = (float)(DBL_EPSILON*MAXDIM*(1<<MAXDIM)*MAXDIM*3.01);
    cdata->b_err_min_sq = cdata->b_err_min * cdata->b_err_min;

    assert(cdata->get_site!=NULL); assert(cdata->site_num!=NULL);

    cdata->rdim = cdata->vd ? cdata->pdim+1 : cdata->pdim;
    if (cdata->rdim > MAXDIM)
	chull3d_panic(cdata, "dimension bound MAXDIM exceeded; cdata->rdim=%d; pdim=%d\n",
		cdata->rdim, cdata->pdim);
    /*	fprintf(cdata->DFILE, "cdata->rdim=%d; pdim=%d\n", cdata->rdim, pdim); fflush(cdata->DFILE);*/

    cdata->point_size = cdata->site_size = sizeof(Coord)*cdata->pdim;
    cdata->basis_vec_size = sizeof(Coord)*cdata->rdim;
    cdata->basis_s_size = sizeof(basis_s)+ (2*cdata->rdim-1)*sizeof(Coord);
    cdata->simplex_size = sizeof(simplex) + (cdata->rdim-1)*sizeof(neighbor);
    cdata->Tree_size = sizeof(Tree);
    cdata->fg_size = sizeof(fg);

    root = NULL;
    if (cdata->vd) {
	cdata->p = cdata->hull_infinity;
	NEWLRC(cdata, cdata->basis_s_list, basis_s, cdata->infinity_basis);
	cdata->infinity_basis->vecs[2*cdata->rdim-1]
	    = cdata->infinity_basis->vecs[cdata->rdim-1]
	    = 1;
	cdata->infinity_basis->sqa
	    = cdata->infinity_basis->sqb
	    = 1;
    } else if (!(cdata->p = (*(cdata->get_site))((void *)cdata))) return 0;

    NEWL(cdata, cdata->simplex_list, simplex,root);

    cdata->ch_root = root;

    copy_simp(cdata, s,root,cdata->simplex_list,cdata->simplex_size);
    root->peak.vert = cdata->p;
    root->peak.simp = s;
    s->peak.simp = root;

    buildhull(cdata, root);
    return root;
}


/* hullmain.c */

/* int	getopt(int, char**, char*); */
extern char	*optarg;
extern int optind;
extern int opterr;

FILE *INFILE, *OUTFILE, *TFILE;

static long site_numm(void *data, site p) {

    long i,j;
    struct chull3d_data *cdata = (struct chull3d_data *)data;

    if (cdata->vd && p==cdata->hull_infinity) return -1;
    if (!p) return -2;
    for (i=0; i<cdata->num_blocks; i++)
	if ((j=p-cdata->site_blocks[i])>=0 && j < BLOCKSIZE*cdata->pdim)
	    return j/cdata->pdim+BLOCKSIZE*i;
    return -3;
}


static site new_site(struct chull3d_data *cdata, site p, long j) {

    assert(cdata->num_blocks+1<MAXBLOCKS);
    if (0==(j%BLOCKSIZE)) {
	assert(cdata->num_blocks < MAXBLOCKS);
	return(cdata->site_blocks[cdata->num_blocks++]=(site)malloc(BLOCKSIZE*cdata->site_size));
    } else
	return p+cdata->pdim;
}

site read_next_site(struct chull3d_data *cdata, long j){

    int i=0, k=0;
    static char buf[100], *s;

    if (j!=-1) cdata->p = new_site(cdata, cdata->p,j);
    if (j!=0) while ((s=fgets(buf,sizeof(buf),INFILE))) {
	if (buf[0]=='%') continue;
	for (k=0; buf[k] && isspace(buf[k]); k++);
	if (buf[k]) break;
    }
    if (!s) return 0;
    if (j!=0) fprintf(TFILE, "%s", buf+k);
    if (j!=0) bu_log("TFILE: %s\n", buf+k);
    while (buf[k]) {
	while (buf[k] && isspace(buf[k])) k++;
	if (buf[k] && j!=-1) {
	    if (sscanf(buf+k,"%lf",(cdata->p)+i)==EOF) {
		fprintf(cdata->DFILE, "bad input line: %s\n", buf);
		exit(1);
	    }
	    (cdata->p)[i] = floor(cdata->mult_up*(cdata->p)[i]+0.5);
	    cdata->mins[i] = (cdata->mins[i]<(cdata->p)[i]) ? cdata->mins[i] : (cdata->p)[i];
	    cdata->maxs[i] = (cdata->maxs[i]>(cdata->p)[i]) ? cdata->maxs[i] : (cdata->p)[i];
	}
	if (buf[k]) i++;
	while (buf[k] && !isspace(buf[k])) k++;
    }

    if (!cdata->pdim) cdata->pdim = i;
    if (i!=cdata->pdim) {DEB(-10,inconsistent input);DEBTR(-10); exit(1);}
    return cdata->p;
}

static site (*get_site_n)(struct chull3d_data *cdata, long);
site get_next_site(void *data) {
    static long s_num = 0;
    return (*get_site_n)((struct chull3d_data *)data, s_num++);
}


static void errline(char *s) {fprintf(stderr, s); fprintf(stderr,"\n"); return;}
static void tell_options(void) {

    errline("options:");
    errline( "-m mult  multiply by mult before rounding;");
    errline( "-d compute delaunay triangulation;");
    errline( "-i<name> read input from <name>;");
    errline( "-X<name> chatter to <name>;");
    errline( "-f<fmt> main output in <fmt>:");
    errline("	ps->postscript, mp->metapost, cpr->cpr format, off->OFF format, vn->vertex numbers(default)");
    errline( "-A alpha shape, find suitable alpha");
    errline( "-aa<alpha> alpha shape, alpha=<alpha>;");
    errline( "-af<fmt> output alpha shape in <fmt>;");
    errline( "-oo  opt==o (default) list of simplices;");
    errline( "-oF<name>  prefix of output files is <name>;");
    errline( "-oN  no main output;");
    errline( "-ov volumes of Voronoi facets");
    errline( "-oh incidence histograms");
}


static void echo_command_line(FILE *F, int argc, char **argv) {
    fprintf(F,"%%");
    while (--argc>=0)
	fprintf(F, "%s%s", *argv++, (argc>0) ? " " : "");
    fprintf(F,"\n");
}

char *output_forms[] = {"vn", "cpr", "off"};

out_func *out_funcs[] = {&vlist_out, &cpr_out, &off_out};


static int set_out_func(char *s) {

    int i;

#ifdef WIN32
    if (strcmp(s, "off")==0) {
	errline("Sorry, no OFF output on WIN32");
	return 0;
    }
#endif

    for (i=0;i< sizeof(out_funcs)/(sizeof (out_func*)); i++)
	if (strcmp(s,output_forms[i])==0) return i;
    tell_options();
    return 0;
}

static void make_output(struct chull3d_data *cdata, simplex *root, void *(visit_gen)(struct chull3d_data *, simplex*, visit_func* visit),
	visit_func* visit,
	out_func* out_funcp,
	FILE *F){

    out_funcp(cdata, 0,0,F,-1);
    visit(cdata, 0, out_funcp);
    visit_gen(cdata, root, visit);
    out_funcp(cdata, 0,0,F,1);
    fclose(F);
}


void chull3d_data_init(struct chull3d_data *data)
{
    int i = 0;
    data->mult_up = 1.0;
    data->simplex_list = 0;
    data->basis_s_list = 0;
    data->Tree_list = 0;
    data->fg_list = 0;
    data->check_overshoot_f = 0;
    data->site_blocks = (point *)bu_calloc(MAXBLOCKS, sizeof(point), "site_blocks");
    BU_GET(data->tt_basis, basis_s);
    data->tt_basis->next = 0;
    data->tt_basis->ref_count = 1;
    data->tt_basis->lscale = -1;
    data->tt_basis->sqa = 0;
    data->tt_basis->sqb = 0;
    data->tt_basis->vecs[0] = 0;
    /* point at infinity for Delaunay triang; value not used */
    data->hull_infinity = (Coord *)bu_calloc(10, sizeof(Coord), "hull_infinity");
    data->hull_infinity[0] = 57.2;
    data->B = (int *)bu_calloc(100, sizeof(int), "A");
    data->tot = 0;
    data->totinf = 0;
    data->bigt = 0;
    data->fg_hist = (double **)bu_calloc(100*100, sizeof(double), "fg_hist");
    data->fg_hist_bad = (double **)bu_calloc(100*100, sizeof(double), "cdata->fg_hist_bad");
    data->fg_hist_far = (double **)bu_calloc(100*100, sizeof(double), "cdata->fg_hist_far");
    data->mins = (Coord *)bu_calloc(MAXDIM, sizeof(Coord), "mins");
    for(i = 0; i < MAXDIM; i++) data->mins[i] = DBL_MAX;
    data->maxs = (Coord *)bu_calloc(MAXDIM, sizeof(Coord), "maxs");
    for(i = 0; i < MAXDIM; i++) data->maxs[i] = -DBL_MAX;
    data->mult_up = 1.0;
    data->mi = (short *)bu_calloc(MAXPOINTS, sizeof(short), "mi");
    data->mo = (short *)bu_calloc(MAXPOINTS, sizeof(short), "mo");
    data->tmpfilenam = (char *)bu_calloc(MAXPATHLEN, sizeof(char), "tmpfilenam");
    data->pdim = 3;
    data->mult_up = 1;
    data->vd = 1;  /* we're using the triangulation by default */
}

void chull3d_data_free(struct chull3d_data *data)
{
    bu_free(data->site_blocks, "site_blocks");
    bu_free(data->hull_infinity, "hull_infinity");
    bu_free(data->B, "B");
    bu_free(data->fg_hist, "fg_hist");
    bu_free(data->fg_hist_bad, "fg_hist_bad");
    bu_free(data->fg_hist_far, "fg_hist_far");
    bu_free(data->mins, "mins");
    bu_free(data->maxs, "maxs");
    bu_free(data->mi, "mi");
    bu_free(data->mo, "mo");
    BU_PUT(data->tt_basis, basis_s);
    bu_free(data->tmpfilenam, "tmpfilenam");
}

int
bn_3d_chull(int *faces, int *num_faces, point_t **vertices, int *num_vertices,
            const point_t *input_points_3d, int num_input_pnts)
{
    struct chull3d_data *cdata;
    simplex *root = NULL;

    BU_GET(cdata, struct chull3d_data);
    chull3d_data_init(cdata);
    root = build_convex_hull(cdata, get_next_site, site_numm, cdata->pdim, cdata->vd);

    if (!root) return -1;

    make_output(cdata, root, visit_hull, ridges_print, off_out, stdout);

    free_hull_storage(cdata);
    chull3d_data_free(cdata);
    BU_PUT(cdata, struct chull3d_data);

    return 0;
}


/* TODO - replace this with a top level function using libbn types */
int main(int argc, char **argv) {


    short	output = 1,
		ofn = 0,
		ifn = 0;
    int	option;
    char	ofile[50] = "",
		ifile[50] = "",
		ofilepre[50] = "";
    int	main_out_form=0;
    simplex *root;


    struct chull3d_data *cdata;
    BU_GET(cdata, struct chull3d_data);
    chull3d_data_init(cdata);

    cdata->mult_up = 1;
    cdata->DFILE = stderr;
    cdata->vd = 1;

    while ((option = getopt(argc, argv, "i:f:")) != EOF) {
	switch (option) {
	    case 'i' :
		strcpy(ifile, optarg);
		break;
	    case 'f' :
		main_out_form = set_out_func(optarg);
		break;
	    default :
		      tell_options();
		      exit(1);
	}
    }

    ifn = (strlen(ifile)!=0);
    INFILE = ifn ? efopen(cdata, ifile, "r") : stdin;
    fprintf(cdata->DFILE, "reading from %s\n", ifn ? ifile : "stdin");

    ofn = (strlen(ofile)!=0);

    strcpy(ofilepre, ofn ? ofile : (ifn ? ifile : "hout") );

    if (output) {
	OUTFILE = stdout;
    }

    TFILE = bu_temp_file(cdata->tmpfilenam, MAXPATHLEN);

    read_next_site(cdata, -1);

    cdata->point_size = cdata->site_size = sizeof(Coord)*cdata->pdim;

    get_site_n = read_next_site;

    root = build_convex_hull(cdata, get_next_site, site_numm, cdata->pdim, cdata->vd);

    fclose(TFILE);

    if (output) {
	out_func* mof = out_funcs[main_out_form];
	visit_func *pr = facets_print;
	if (main_out_form==0) echo_command_line(OUTFILE,argc,argv);
	else if (cdata->vd) pr = ridges_print;
	make_output(cdata, root, visit_hull, pr, mof, OUTFILE);
    }

    free_hull_storage(cdata);
    chull3d_data_free(cdata);
    BU_PUT(cdata, struct chull3d_data);

    exit(0);
}


/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */

