#ifndef PILER
#define PILER

#include "DB.h"

#define PANEL_TARGET  50000   //  Desired panel size for big A-reads
#define PANEL_FUDGE    5000   //  Last panel can be PANEL_SIZE + PANEL_FUDGE in size

extern int PANEL_SIZE;        //  Actual panel size, must be a multiple of getModel()->tspace

#define ISCOMP(x) (x & 0x1)
#define BREAD(x)  (x >> 1)

  //   4 bit flags hidden in btip field of LA's to allow chain encodings

#define DATA_MASK 0x1fff  //  Get btip value
#define DRAW_FLAG 0x2000  //  This LA has been drawn (set/reset during paint event)
#define INIT_FLAG 0x4000  //  This LA is the first in a chain
#define LINK_FLAG 0x8000  //  This LA is linked to a following LA (via .level field)
#define DRAW_OFF  0xdfff  //  ~ DRAW_FLAG

#define DATA_ETIP 0x1fff  //  Get etip value
#define STRT_FLAG 0x8000  //  Chain start
#define SUCC_FLAG 0x4000  //  Chain next
#define ELIM_BIT  0x2000  //  Eliminated LA

typedef struct
  { int   bread;           //  I = 1: (B-read << 1) | complement flag
                           //  IL = 01: index of last LA in chain
                           //  IL = 00: index of first LA in chain
    int   level;           //  L = 1: index of next LA in chain
                           //  L = 0: level at which to draw chain
    short btip, etip;      //  length of tips (or 0)
    int   abpos, aepos;    //  coords of alignment
    int   bbpos, bepos;
    int64 toff;            // Index in input file of trace
  } LA;

typedef struct
  { int   first;   // Index of first alignment in A-pile
    int   where;   // Global x-coordinate of A-read's start point
    int64 offset;  // Index in input file of first record
    int   panels;  // Index of first panel of LAs
  } Pile;

typedef struct
  { int        nref;   // 0 => clone, otherwise main window with nref-1 clones open
    DAZZ_DB   *db1;    //  Model is db1[first..last) vs db2 with alignments in stream input
    DAZZ_DB   *db2;
    DAZZ_STUB *stub;
    int        first;  // First read index in pile (0-based)
    int        last;   // Last read index + 1
    int        block;  // las file is for this block (or all db if 0)

    Pile    *pile;   // pile[i] is defines pile-a-gram for read i
    LA      *local;  // B-alignments for i are in local[pile[i].first .. pile[i+1].first)

    int      depth;  // Number of rows in deepest pile

    FILE    *input;  // .las file (remains open for TP reading)
    int      omax;   // Max # of overlaps in a pile
    void    *tbuf;   // Max space needed for a complete encoding of a pile
    int      tspace; // Trace spacing
    int      tbytes; // Trace unit isze

    DAZZ_TRACK *qvs; // Quality track (if != NULL)
    DAZZ_TRACK *prf; // Repeat Profile track (if != NULL)

    int      *panels; // plist[panel[i],panel[i+1]) is the list of LAs covering a
    int      *plists; //   part of panel i.
  } DataModel;

DataModel *openModel(char *las, char *Adb, char *Bdb, int first, int last,
                     int nolink, int nolap, int elim, int max_comp, int max_expn, char **mesg);

DataModel *openClone(char *las, int read,
                     int nolink, int nolap, int elim, int max_comp, int max_expn, char **mesg);

char *freeClone(DataModel *clone);

  //  Layout the current model (if any) with links iff nolink == zero and overlaps iff nolap == 0
  //    This operation resets the depth field of the model and the level field of every LA.

int        reLayoutModel(DataModel *model, int nolink, int nolap, int elim,
                         int max_comp, int max_expn);

int        symmetric(DataModel *model);
int        dataWidth(DataModel *model);
int        dataHeight(DataModel *model);
int        readSpan(DataModel *model, int read1, int read2, int *beg, int *end);

#endif
