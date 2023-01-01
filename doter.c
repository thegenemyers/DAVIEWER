#include <stdlib.h>
#include <stdio.h>
#include <math.h>

#include "DB.h"
#include "doter.h"

#undef  DEBUG
#undef  DEBUG_FILL

typedef struct
  { uint64 code;
    int    pos;
  } Tuple;

static int _units[] = { 1, 2, 5, 10, 20, 50, 100, 200, 500, 1000, 2000, 5000, 10000, 20000, 50000,
                       100000, 200000, 500000, 1000000, 2000000, 5000000, 10000000, 20000000,
                       50000000, 100000000, 200000000, 500000000, 1000000000 };

static char *_suffix[] = { "", "", "", "", "", "", "", "", "", "K", "K", "K", "K", "K", "K",
                           "K", "K", "K", "M", "M", "M", "M", "M", "M", "M", "M", "M", "G" };

static int _divot[] = { 1, 1, 1, 1, 1, 1, 1, 1, 1, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000,
                       1000, 1000000, 1000000, 1000000, 1000000, 1000000, 1000000, 1000000, 1000000,
                       1000000, 1000000000 };

int   *units  = _units;
char **suffix = _suffix;
int   *divot  = _divot;

int divide_bar(int t)
{ int u;

  for (u = 1; u < 28; u++)
    { if (units[u] > t)
        break;
    }
  u -= 1;
  return (u);
}

static uint64 Kmask;

static double LowLft[256];
static double LowRgt[256];
static double HghLft[256];
static double HghRgt[256];

static uint64 Cumber[4];

static int Tran[128] =
  { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 1, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 1, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

static Tuple *build_vector(int len, char *seq, int kmer, Tuple *list)
{ int    p, km1;
  uint64 u, c, x;

  km1  = kmer-1;

  c = u = 0;
  for (p = 0; p < km1; p++)
    { x = Tran[(int) seq[p]];
      c = (c << 2) | x;
      u = (u >> 2) | Cumber[x];
    }
  seq += km1;
  for (p = 0; p < len; p++)
    { x = Tran[(int) seq[p]];
      c = ((c << 2) | x) & Kmask;
      u = (u >> 2) | Cumber[x];
      if (u < c)
        list[p].code = u;
      else
        list[p].code = c;
      list[p].pos = p;
    }
  return (list);
}

static void merge(int blen, Tuple *blist, int *alen, Tuple *alist)
{ int    *aplot;
  int     i, j;
  int     al, bl;
  int     al2, bl2;
  int     y;
  uint64  kb, lc, anull;

  bl = blen;
  al = *alen;

  lc = alist[al-1].code;
  for (bl2 = bl-1; bl2 >= 0; bl2--)
    if (blist[bl2].code < lc)
      break;
  bl2 += 1;
  for (al2 = al-2; al2 >= 0; al2--)
    if (alist[al2].code < lc)
      break;
  al2 += 1;

  aplot = (int *) alist;
  anull = 2*al;

  y = 0;
  i = j = 0;
  while (i < bl2)
    { kb = blist[i].code;
      while (alist[j].code < kb)
        j += 1;

      if (alist[j].code == kb)
        { blist[i++].code = y;
          while (blist[i].code == kb)
            blist[i++].code = y;
          aplot[y++] = alist[j++].pos;
          while (alist[j].code == kb)
            aplot[y++] = alist[j++].pos;
          aplot[y++] = -1;
        }
      else
        { blist[i++].code = anull;
          while (blist[i].code == kb)
            blist[i++].code = anull;
        }
    }

  if (bl2 < bl && blist[i].code == lc)
    { blist[i++].code = y;
      while (i < bl && blist[i].code == lc)
        blist[i++].code = y;
      while (al2 < al)
        aplot[y++] = alist[al2++].pos;
      aplot[y++] = -1;
    }
  while (i < bl)
    blist[i++].code = anull;

  *alen = y;
}

static void compress(int blen, Tuple *blist, int arun, int *aplot)
{ int    *bplot;
  int     i, x;

  bplot = ((int *) aplot) + arun;

  arun -= 1;
  for (i = 0; i < blen; i++)
    { x = blist[i].code;
      if (x >= arun)
        bplot[i] = arun; 
      else
        bplot[i] = blist[i].code;
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
{ static int first = 1;

  DotList *dots;
  int      km1, arun;
  Tuple   *lista, *listb;

  km1 = kmer-1;

  if (kmer == 32)
    Kmask = 0xffffffffffffffffllu;
  else
    Kmask = (0x1llu << 2*kmer) - 1;

  Cumber[0] = (0x3llu << 2*km1);
  Cumber[1] = (0x2llu << 2*km1);
  Cumber[2] = (0x1llu << 2*km1);
  Cumber[3] = 0x0llu;

  if (first)
    { int s, r, x;

      first = 0;

      for (r = 0; r < 16; r++)
        for (s = 0; s < 16; s++)
          { x = (r << 4) | s;
            HghRgt[x] = (r*s);
            HghLft[x] = (r*(16-s));
            LowRgt[x] = ((16-r)*s);
            LowLft[x] = ((16-r)*(16-s));
            if (r == 0)
              { if (s == 0)
                  HghRgt[x] = HghLft[x] = LowRgt[x] = 1;
                else
                  HghRgt[x] = HghLft[x] = 1;
              }
            else
              { if (s == 0)
                  HghRgt[x] = LowRgt[x] = 1;
              }
          }
      LowLft[0] = 255;
    }

  dots = (DotList *) Malloc(sizeof(DotList),"Allocating dot data structure");

  alen -= km1;
  blen -= km1;

  lista = (Tuple *) Malloc(sizeof(Tuple)*(alen+blen),"Tuple list");
  listb = lista + alen;

  build_vector(alen,aseq,kmer,lista);
  build_vector(blen,bseq,kmer,listb);

  qsort(lista,alen,sizeof(Tuple),TSORT);
  qsort(listb,blen,sizeof(Tuple),TSORT);

#ifdef DEBUG
  { int i;

    for (i = 1; i < alen; i++)
      if (lista[i].code < lista[i-1].code)
        printf("Not sorted\n");

    for (i = 1; i < blen; i++)
      if (listb[i].code < listb[i-1].code)
        printf("Not sorted\n");
  }
#endif

  arun = alen;
  merge(blen,listb,&arun,lista);

  dots->aplot = (int *) lista;
  dots->blen  = blen;
  dots->arun  = arun;

  qsort(listb,blen,sizeof(Tuple),PSORT);

#ifdef DEBUG
  { int i;

    printf("arun = %d\n",arun);
    for (i = 0; i < blen; i++)
      if (listb[i].pos != i)
        printf("  Unassigned pos %d vs %d\n",i,listb[i].pos);
    printf("All good\n"); fflush(stdout);
  }
#endif

  compress(blen,listb,arun,dots->aplot);

  dots->aplot = Realloc(dots->aplot,sizeof(int)*(blen+arun),"Adjusting hit list\n");
  dots->bplot = dots->aplot + arun;

#ifdef DEBUG
  { int i, j;

    printf("Scan Lines:\n");
    for (i = 0; i < blen; i++)
      { printf(" %5d:",i);
        j = dots->bplot[i];
        while (dots->aplot[j] >= 0)
          { printf(" %5d",dots->aplot[j]);
            j += 1;
          }
        printf("\n");
      }
  }
#endif

#ifdef DEBUG
  { int i, j;
    int nz, nel;

    printf("Scan Stats:\n");
    nz = nel = 0;
    for (i = 0; i < blen; i++)
      { j = dots->bplot[i];
        if (dots->aplot[j] >= 0)
          { nz += 1;
            while (dots->aplot[j] >= 0)
              { nel += 1;
                j += 1;
              }
          }
      }

    printf("  Non-Zero lines: %d (out of %d)\n",nz,blen);
    printf("  Av/line = %.1f (out of %d)\n",(1.*nel)/alen,alen);
    printf("  Density = %.3f%%\n",((100.*nel)/alen)/blen);
  }
#endif

  return (dots);
}

void free_dots(DotList *dots)
{ free(dots->aplot);
  free(dots);
}

void render_plot(DotList *dots, Frame *frame, int rectW, int rectH, uchar **raster)
{ int  blen  = dots->blen;
  int *aplot = dots->aplot;
  int *bplot = dots->bplot;

  double vX = frame->x;
  double vY = frame->y;
  double vW = frame->w;
  double vH = frame->h;

  double xa = (rectH-44.)/vH;
  double xb = -vY*xa + 22.;

  double ya = (rectW-44.)/vW;
  double yb = -vX*ya + 22.;

  double recth = rectH - 20.;
  double rectw = rectW - 20.;

  // printf("Paint = (%g,%g) %g x %g into %d x %d\n",vX,vY,vW,vH,rectW,rectH);

  int    _MATRIX0[rectW+2], _MATRIX1[rectW+2];
  int    *MATRIX0 = _MATRIX0+1, *MATRIX1 = _MATRIX1+1;
  int    PUSH0[rectW], PUSH1[rectW];

  uchar  *ras;
  int    *matrix0, *matrix1;
  int    *push0, *push1;
  int     top0, top1;

  int    u;
  int    s, s0, s1;
  int    ir, is;
  double r, nr;
  int    i, ib, ie, p, y, x;

  for (i = -1; i <= rectW; i++)
    MATRIX0[i] = MATRIX1[i] = 0;
  matrix0 = MATRIX0;
  matrix1 = MATRIX1;
  push0   = PUSH0;
  push1   = PUSH1;
  top0 = top1 = 0;

  ib = ceil((20.-xb)/xa);
  if (ib < 0)
    ib = 0;
  ie = floor((recth-xb)/xa);
  if (ie > blen)
    ie = blen;
  u = (int) (xa*ib + xb);

  xa *= 16.;
  xb *= 16.;
  ya *= 16.;
  yb *= 16.;
  rectw *= 16;
  recth *= 16;

  r  = xa*ib+xb;
  nr = ((((int) floor(r)) >> 4) + 1) * 16.;
#ifdef DEBUG_FILL
  printf("lines %d - %d: %.3f/%.3f pixels/coord line\n",ib,ie,xa,ya);
#endif
  for (i = ib; i < ie; i++)
    { p = bplot[i];
#ifdef DEBUG_FILL
      printf("  Line i,r,ir = %d,%.3f,%1x\n",i,r/16.,((int) floor(r) & 0xf));
#endif
      if ((y = aplot[p]) >= 0)
        { ir = (((int) floor(r)) & 0xf) << 4;
          while (y >= 0)
            { s = ((int) floor(ya*y+yb));
              if (s >= 320 && s < rectw)
                { is = ir | (s & 0xf);
                  s0 = (s >> 4);
                  s1 = s0+1;
#ifdef DEBUG_FILL
                  printf("     s,s0,is = %.3f,%d,%1x\n",(ya*y+yb)/16.,s0,(s & 0xf));
#endif

#define MATRIX(m,s,p,t,v)       \
  x = m[s];                     \
  if (x == 0)                   \
    p[t++] = s;                 \
  if (v > x)			\
    m[s] = v;

                  MATRIX(matrix0,s1,push0,top0,LowRgt[is]);
                  MATRIX(matrix1,s1,push1,top1,HghRgt[is]);
                  MATRIX(matrix0,s0,push0,top0,LowLft[is]);
                  MATRIX(matrix1,s0,push1,top1,HghLft[is]);
                }
              
              y = aplot[++p];
            }
        }
      r += xa;
      if (r > nr)
        { ras = raster[u++];
#ifdef DEBUG_FILL
          printf("OUT %d: ",u-1); fflush(stdout);
#endif
          while (top0 > 0)
            { s0 = push0[--top0];
#ifdef DEBUG_FILL
              printf(" %d(%d)",s0,matrix0[s0]); fflush(stdout);
#endif
              ras[s0] = (uchar) matrix0[s0];
              matrix0[s0] = 0.;
            }
#ifdef DEBUG_FILL
          printf("\n"); fflush(stdout);
#endif
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
          nr += 16.;
        }
    }
  if (r < rectH+16.)
    { ras = raster[u++];
      while (top1 > 0)
        { s1 = push1[--top1];
          ras[s1] = (uchar) matrix1[s1];
        }
    }
}
