#ifndef DOTER
#define DOTER

#include "DB.h"

typedef struct
{ double x, y;
  double w, h;
} Frame;

typedef struct
  { int     alen;
    int     brun;
    int    *aplot;
    int    *bplot;
  } DotList;

DotList *build_dots(int alen, char *aseq, int blen, char *bseq, int kmer);

void free_dots(DotList *dots);

double scale_plot(DotList *plot, Frame *frame, int rectW, int rectH);

#endif
