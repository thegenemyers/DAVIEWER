#include <stdlib.h>
#include <stdio.h>
#include <math.h>

#include "DB.h"
#include "doter.h"

#undef DEBUG

typedef struct
  { uint32 code;
    int    pos;
  } Tuple;

static uint32 Kmask;

static uint32 Cumber[4];

static Tuple *build_vector(int len, char *seq, int kmer)
{ int    p, km1;
  uint32 u, c, x;
  Tuple *list;

  km1  = kmer-1;
  list = (Tuple *) Malloc(sizeof(Tuple)*len,"Tuple list");

  c = u = 0;
  for (p = 0; p < km1; p++)
    { x = seq[p];
      c = (c << 2) | x;
      u = (u >> 2) | Cumber[x];
    }
  seq += km1;
  len -= km1;
  for (p = 0; p < len; p++)
    { x = seq[p];
      c = ((c << 2) | x) & Kmask;
      u = (u >> 2) | Cumber[x];
      if (u < c)
        { list[p].code = u;
          list[p].pos  = p;
        }
      else
        { list[p].code = c;
          list[p].pos  = p;
        }
    }
  return (list);
}

static void merge(int alen, Tuple *alist, int *blen, Tuple *blist)
{ int    *bplot;
  int     i, j;
  int     al, bl;
  int     al2, bl2;
  int     y;
  uint32  ka, lc, bnull;

  al = alen;
  bl = *blen;

  lc = blist[bl-1].code;
  for (al2 = al-1; al2 >= 0; al2--)
    if (alist[al2].code < lc)
      break;
  al2 += 1;
  for (bl2 = bl-2; bl2 >= 0; bl2--)
    if (blist[bl2].code < lc)
      break;
  bl2 += 1;

  bplot = (int *) blist;
  bnull = bl;

  y = 0;
  i = j = 0;
  while (i < al2)
    { ka = alist[i].code;
      while (blist[j].code < ka)
        j += 1;

      if (blist[j].code == ka)
        { alist[i++].code = y;
          while (alist[i].code == ka)
            alist[i++].code = y;
          bplot[y++] = blist[j++].pos;
          while (blist[j].code == ka)
            bplot[y++] = blist[j++].pos;
          bplot[y++] = -1;
        }
      else
        { alist[i++].code = bnull;
          while (alist[i].code == ka)
            alist[i++].code = bnull;
        }
    }

  if (al2 < al && alist[i].code == lc)
    { alist[i++].code = y;
      while (i < al && alist[i].code == lc)
        alist[i++].code = y;
      while (bl2 < bl)
        bplot[y++] = blist[bl2++].pos;
      bplot[y++] = -1;
    }
  while (i < al)
    alist[i++].code = bnull;

  *blen = y;
}

static void compress(int alen, Tuple *alist, int bnull)
{ int    *aplot;
  int     i, x;

  aplot = (int *) alist;

  for (i = 0; i < alen; i++)
    { x = alist[i].code;
      if (x >= bnull)
        aplot[i] = bnull; 
      else
        aplot[i] = alist[i].code;
    }
}

static int TSORT(const void *l, const void *r)
{ Tuple *x = (Tuple *) l;
  Tuple *y = (Tuple *) r;
  return (x->code - y->code);
}

static int PSORT(const void *l, const void *r)
{ Tuple *x = (Tuple *) l;
  Tuple *y = (Tuple *) r;
  return (x->pos - y->pos);
}

DotList *build_dots(int alen, char *aseq, int blen, char *bseq, int kmer)
{ DotList *dots;
  int      km1, brun;
  Tuple   *lista, *listb;

  km1 = kmer-1;

  if (kmer == 16)
    Kmask = 0xffffffff;
  else
    Kmask = (0x1 << 2*kmer) - 1;

  Cumber[0] = (0x3 << 2*km1);
  Cumber[1] = (0x2 << 2*km1);
  Cumber[2] = (0x1 << 2*km1);
  Cumber[3] = 0;

  dots = (DotList *) Malloc(sizeof(DotList),"Allocating dot data structure");

  lista = build_vector(alen,aseq,kmer);
  listb = build_vector(blen,bseq,kmer);

  qsort(lista,alen-km1,sizeof(Tuple),TSORT);
  qsort(listb,blen-km1,sizeof(Tuple),TSORT);

#ifdef DEBUG
  { int i;

    for (i = 1; i < alen-km1; i++)
      if (lista[i].code < lista[i-1].code)
        printf("Not sorted\n");

    for (i = 1; i < blen-km1; i++)
      if (listb[i].code < listb[i-1].code)
        printf("Not sorted\n");
  }
#endif

  alen -= km1;
  blen -= km1;
  brun  = blen;
  merge(alen,lista,&brun,listb);

  dots->bplot = Realloc(listb,sizeof(int)*brun,"Adjusting hit list\n");
  dots->alen  = alen;
  dots->brun  = brun;

  qsort(lista,alen,sizeof(Tuple),PSORT);

#ifdef DEBUG
  { int i;

    printf("brun = %d\n",brun);
    for (i = 0; i < alen; i++)
      if (lista[i].pos != i)
        printf("  Unassigned pos %d vs %d\n",i,lista[i].pos);
    printf("All good\n"); fflush(stdout);
  }
#endif

  compress(alen,lista,brun-1);

  dots->aplot = Realloc(lista,sizeof(int)*alen,"Adjusting hit list\n");

#ifdef DEBUG
  { int i, j;

    printf("Scan Lines:\n");
    for (i = 0; i < alen; i++)
      { printf(" %5d:",i);
        j = dots->aplot[i];
        while (dots->bplot[j] >= 0)
          { printf(" %5d",dots->bplot[j]);
            j += 1;
          }
        printf("\n");
      }
  }
#endif

  { int i, j;
    int nz, nel;

    printf("Scan Stats:\n");
    nz = nel = 0;
    for (i = 0; i < alen; i++)
      { j = dots->aplot[i];
        if (dots->bplot[j] >= 0)
          { nz += 1;
            while (dots->bplot[j] >= 0)
              { nel += 1;
                j += 1;
              }
          }
      }
    printf("  Non-Zero lines: %d (out of %d)\n",nz,alen);
    printf("  Av/line = %.1f (out of %d)\n",(1.*nel)/blen,blen);
    printf("  Density = %.3f%%\n",((100.*nel)/blen)/alen);
  }

  return (dots);
}

void free_dots(DotList *dots)
{ free(dots->aplot);
  free(dots->bplot);
  free(dots);
}

double scale_plot(DotList *dots, Frame *frame, int rectW, int rectH)
{ int  alen  = dots->alen;
  int *aplot = dots->aplot;
  int *bplot = dots->bplot;

  double vX = frame->x;
  double vY = frame->y;
  double vW = frame->w;
  double vH = frame->h;

  double xa = rectW/vW;
  double xb = -vX*(rectW/vW);

  double ya = rectH/vH;
  double yb = -vY*(rectH/vH);

  double maxim;
  double MATRIX0[rectH], MATRIX1[rectH];
  int    PUSH0[rectH], PUSH1[rectH];

  double *matrix0, *matrix1;
  int    *push0, *push1;
  int     top0, top1;

  int    r0, r1;
  int    s0, s1;
  double r, s, x;
  double rl, sl;
  double rh, sh;
  int    i, ib, ie, p, y;

  maxim = 0.;
  for (i = 0; i < rectH; i++)
    MATRIX0[i] = MATRIX1[i] = 0;
  matrix0 = MATRIX0;
  matrix1 = MATRIX1;
  push0   = PUSH0;
  push1   = PUSH1;
  top0 = top1 = 0;
  
  ib = trunc(-(xb+1.)/xa) + 1;
  ie = ceil((rectW-(xb+1.))/xa);
  if (ib < 0) ib = 0;
  if (ie > alen) ie = alen;
  r = xa*ib+xb;
// printf("lines %d - %d: %.3f pixels (%.3f) / line\n",ib,ie,xa,ya);
  for (i = ib; i < ie; i++)
    { p = aplot[i];
// printf("i,r = %d,%.3f\n",i,r);
      if ((y = bplot[p]) >= 0)
        { r0 = trunc(r);
          r1 = r0+1;
          rl = r1-r;
          rh = 1.-rl; 
          while (y >= 0)
            { s = ya*y+yb; 
// printf("  j,s = %d,%.3f",y,s);
              s0 = trunc(s);
              s1 = s0+1;
              sl = (s1-s);
              sh = 1.-sl;
// printf("       %d-%d vs %d-%d: %.3f-%.3f %.3f-%.3f\n",r0,r1,s0,s1,rl,rh,sl,sh); fflush(stdout);

#define MATRIX(m,s,p,t,v)       \
  x = m[s];                     \
  if (x == 0.)                  \
    p[t++] = s;                 \
  m[s] = x+v;
                
              if (s0 >= 0)
                { if (s1 < rectH)
                    { MATRIX(matrix0,s1,push1,top1,rl*sl);
                      MATRIX(matrix1,s1,push0,top0,rh*sl);
                      MATRIX(matrix0,s0,push1,top1,rl*sh);
                      MATRIX(matrix1,s0,push0,top0,rh*sh);
                    }
                  else if (s0 < rectH)
                    { MATRIX(matrix0,s0,push1,top1,rl*sh);
                      MATRIX(matrix1,s0,push0,top0,rh*sh);
                    }
                }
              else if (s1 >= 0 && s1 < rectH)
                { MATRIX(matrix0,s1,push1,top1,rl*sl);
                  MATRIX(matrix1,s1,push0,top0,rh*sl);
                }
              
              y = bplot[++p];
            }
        }
      if (r + xa > ceil(r))
        { r0 = trunc(r);
          if (r0 >= 0 && r0 < rectW)
            while (top0 > 0)
              { s0 = push0[--top0];
                if (matrix0[s0] > maxim)
                  maxim = matrix0[s0];
                matrix0[s0] = 0.;
              }
          else
            while (top0 > 0)
              { s0 = push0[--top0];
                matrix0[s0] = 0.;
              }
// printf("FLIPPING %g\n",maxim);
          matrix0 = matrix1;
          push0   = push1;
          top0    = top1;
          if (matrix0 == MATRIX0)
            { matrix1 = MATRIX1;
              push1   = PUSH1;
            }
          else
            { matrix1 = MATRIX0;
              push1   = PUSH0;
            }
          top1 = 0;
        }
      r += xa;
    }
  r1 = trunc(r-xa)+1;
  if (r1 >= 0 && r1 < rectW)
    while (top1 > 0)
      { s1 = push1[--top1];
        if (matrix1[s1] > maxim)
          maxim = matrix1[s1];
      }

printf("max = %g\n",maxim);

  return (maxim);
}
