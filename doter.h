#ifndef DOTER
#define DOTER

#include "DB.h"

typedef struct
{ double x, y;
  double w, h;
} Frame;

typedef struct
  { int     arun;
    int     blen;
    int    *aplot;
    int    *bplot;
  } DotList;

extern int   *units;
extern char **suffix;
extern int   *divot;

int divide_bar(int t);

DotList *build_dots(int alen, char *aseq, int blen, char *bseq, int kmer);

void free_dots(DotList *dots);

typedef unsigned char uchar;

void render_plot(DotList *plot, Frame *frame, int rectW, int rectH, uchar **raster);

#endif
