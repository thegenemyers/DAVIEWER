/*******************************************************************************************
 *
 *  Load a .las and associated DB's into a "model" for the Qt Viewer
 *
 *  Author:  Gene Myers
 *  Date  :  December 2014
 *
 *******************************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <ctype.h>
#include <math.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "QV.h"
#include "DB.h"
#include "align.h"

#include "piler.h"

#define PILE_SPACING 2000
#define MAX_TIPLEN    500
#define LA_SPACE      100

static int       FIRSTCALL = 1;
static int       UNDEFINED = 1;
static DataModel MODEL;

int PANEL_SIZE;

int dataWidth()
{ if (UNDEFINED)
    return (1000000);
  return (MODEL.pile[MODEL.last].where);
}

int dataHeight()
{ if (UNDEFINED)
    return (256);
  return (MODEL.depth);
}

int readSpan(int read1, int read2, int *beg, int *end)
{ if (read1 < MODEL.first || read2 > MODEL.last)
    return (1);
  *beg = MODEL.pile[read1].where - PILE_SPACING/2;
  *end = MODEL.pile[read2-1].where + MODEL.db1->reads[read2-1].rlen + PILE_SPACING/2;
  return (0);
}

DataModel *getModel()
{ if (UNDEFINED)
    return (NULL);
  return (&MODEL);
}

static LA *LOCAL;

static int LEFTMOST(const void *l, const void *r)
{ int x = *((int *) l);
  int y = *((int *) r);

  return (LOCAL[x].abpos-LOCAL[y].abpos);
}

static int BIGGEST(const void *l, const void *r)
{ int x = *((int *) l);
  int y = *((int *) r);
  int e, f;

  if ((LOCAL[x].btip & LINK_FLAG) != 0)
    { e = LOCAL[x].level;
      if ((LOCAL[e].btip & LINK_FLAG) != 0)
        e = LOCAL[e].bread;
    }
  else
    e = x;

  if ((LOCAL[y].btip & LINK_FLAG) != 0)
    { f = LOCAL[y].level;
      if ((LOCAL[f].btip & LINK_FLAG) != 0)
        f = LOCAL[f].bread;
    }
  else
    f = y;

  return ((LOCAL[f].aepos-LOCAL[y].abpos) - (LOCAL[e].aepos - LOCAL[x].abpos));
}

static int layoutPile(Pile *pile, int *rail, int nolink, int nolap, int elim,
                                             double comp_factor, double expn_factor)
{ int i, j, k, h;
  int r, n;
  int b, v, max;
  int low;
  int last;
  int rtop;
  int blast;
  short EMASK;
  short EVALU;

  (void) LEFTMOST;
  (void) BIGGEST;

  LOCAL = MODEL.local;
  last  = pile[1].first;

  if (pile->first == last) return (0);

  n = 0;

  blast = LOCAL[last].bread;  //  Temporary EOL terminator
  LOCAL[last].bread = -1;

  if (elim == 0)
    EMASK = 0;
  else
    EMASK = ELIM_BIT;
  if (elim <= 0)
    EVALU = 0;
  else
    EVALU = ELIM_BIT;

  for (i = pile->first; i < last; i++)
    { if ((LOCAL[i].btip & INIT_FLAG) != 0)
        { b = LOCAL[i].bread;
          for (j = i; (LOCAL[j].btip & LINK_FLAG) != 0; j = LOCAL[j].level)
            { LOCAL[j].bread = b;
              LOCAL[j].btip &= DATA_MASK;
            }
          LOCAL[j].bread = b;
        }
      LOCAL[i].btip &= DATA_MASK;
    }

  if (nolink && nolap)
    { for (i = pile->first; i < last; i++)
        if ((LOCAL[i].etip & EMASK) == EVALU)
          { LOCAL[i].abpos -= LOCAL[i].btip;
            LOCAL[i].btip  |= INIT_FLAG;
            rail[n++] = i;
          }
    }

  else
    { if ((LOCAL[pile->first].etip & STRT_FLAG) != 0)

        for (i = pile->first; i < last; i++)
          if ((LOCAL[i].etip & SUCC_FLAG) != 0)
            { if ((LOCAL[i].etip & EMASK) == EVALU)
                { if (last >= 0)
                    { LOCAL[i].btip |= LINK_FLAG;
                      LOCAL[i].level = last;
                    }
                  last = i;
                }
            }
          else
            { if ((LOCAL[i].etip & EMASK) == EVALU)
                last = i;
              else
                last = -1;
            }

      //  Set up reverse link with L-flag only, use I-flag to mark elements already
      //    interior to a chain.

      else
        for (i = pile->first; i < last; i = j)
          { b = LOCAL[i].bread;
            low = i;
            for (j = i+1; LOCAL[j].bread == b; j++)

              { if ((LOCAL[j].etip & EMASK) != EVALU)
                  continue;

                for (k = j-1; k >= low; k--)
                  { int agap, bgap, pair;
  
                    if ((LOCAL[k].btip & INIT_FLAG) != 0)
                      continue;
                    if ((LOCAL[k].etip & EMASK) != EVALU)
                      continue;
  
                    agap = LOCAL[j].abpos - LOCAL[k].aepos;
                    if (agap < 0)
                      { if (nolap)
                          continue;
                      }
                    else if (agap < 20000)
                      { if (nolink)
                          continue;
                      }
                    else
                      { low = k+1;
                        break;
                      }
  
                    bgap = LOCAL[j].bbpos - LOCAL[k].bepos;
                    if (-300 < agap)
                      { bgap += 400;
                        agap += 400;
                        pair = (agap*comp_factor < bgap && bgap < agap*expn_factor);
                      }
                    else
                      pair = (agap*comp_factor > bgap && bgap > agap*expn_factor);
                    if (pair)
                      { LOCAL[j].btip |= LINK_FLAG;
                        LOCAL[j].level = k;
                        if (LOCAL[j].aepos <= LOCAL[k].aepos)
                          LOCAL[j].btip |= INIT_FLAG;
                        break;
                      }
                  }
              }
          }

      // Reverse links and add 1st element of each chain to the allocation rail
      //    and add btip if any (but not etip until during allocation)
      //    The L structure can be a tree, so be careful (while loop below makes
      //    the reversal a chain no matter what).

      for (k = pile->first; k < last; k++)
        if ((LOCAL[k].btip & LINK_FLAG) == 0)
          { if ((LOCAL[k].etip & EMASK) == EVALU)
              { LOCAL[k].btip  |= INIT_FLAG;
                LOCAL[k].abpos -= (LOCAL[k].btip & DATA_MASK);
                rail[n++] = k;
              }
          }
        else
          { h = LOCAL[k].level;
            while ((LOCAL[h].btip & LINK_FLAG) != 0)
              h = LOCAL[h].level;
            LOCAL[h].level = k;
            LOCAL[h].btip |= LINK_FLAG;
            LOCAL[k].btip &= DATA_MASK;
          }

      //  Have chains to draw, establish bread jumping links for each chain

      for (k = pile->first; k < last; k++)
        if ((LOCAL[k].btip & INIT_FLAG) != 0)
          { h = k;
            while ((LOCAL[h].btip & LINK_FLAG) != 0)
              h = LOCAL[h].level;
            if (k != h)
              { r = LOCAL[k].level;
                while (r != h)
                  { LOCAL[r].bread = h;
                    r = LOCAL[r].level;
                  }
                LOCAL[h].bread = k;
              }
          }
    }

  qsort(rail,n,sizeof(int),LEFTMOST);

  //  Pull chain starts from abpos (with btip) sorted rail array, allocate
  //    next free rail, restore abpos to not have tip, compute furthest x-coord
  //    of an end with its etip, and update rail to reflect allocation

  rtop = 0;
  for (i = 0; i < n; i++)
    { j = rail[i];
      v = LOCAL[j].abpos;
      for (r = 0; r < rtop; r++)
        if (v > rail[r])
          break;
      if (r >= rtop)
        rtop = r+1;

      LOCAL[j].abpos += (LOCAL[j].btip & DATA_MASK);
      max = LOCAL[j].aepos + (LOCAL[j].etip & DATA_ETIP);
      while ((LOCAL[j].btip & LINK_FLAG) != 0)
        { j = LOCAL[j].level;
          v = LOCAL[j].aepos + (LOCAL[j].etip & DATA_ETIP);
          if (v > max)
            max = v;
        }
      rail[r] = max + LA_SPACE;
      LOCAL[j].level = r;
    }

  LOCAL[last].bread = blast;

  return (rtop);
}

int reLayoutModel(int nolink, int nolap, int elim, int max_comp, int max_expn)
{ int   *rail;
  int    depth;
  double comp_factor, expn_factor;
  int    a, n;

  if (UNDEFINED)
    return (0);

  rail = (int *) Malloc(sizeof(int)*MODEL.omax,"Allocating layout vector");
  if (rail == NULL)
    return (1);

  comp_factor = 1. - max_comp/100.;
  expn_factor = 1. + max_expn/100.;

  depth = 0;
  for (a = MODEL.first; a < MODEL.last; a++)
    { n = layoutPile(MODEL.pile+a,rail,nolink,nolap,elim,comp_factor,expn_factor);
      if (n > depth)
        depth = n;
    }

  free(rail);
  
  MODEL.depth = depth;
  return (0);
}

static int64 OvlIOSize = sizeof(Overlap) - sizeof(void *);
static int   TRACE_SPACING;
static int   TBYTES;

static int tiplen(int ae, int be)
{ int tip;

  if (be > 0 && ae > 0)
    { tip = MAX_TIPLEN;
      if (tip > be)
        tip = be;
      if (tip > ae)
        tip = ae;
    }
  else
    tip = 0;
  return (tip);
}

static int buildModel(int nolink, int nolap, int elim, int max_comp, int max_expn)
{ int        first, last;
  FILE      *input;
  DAZZ_READ *reads;

  LA   *local;
  Pile *pile;
  void *tbuffer;
  int  *panels, *plists;

  Overlap ovl;
  int64   novl;
  int     a, pos;
  int     omax, n, m;
  int64   smax, tlen;
  int     btip, etip;
  int     npan, nspl, nlas;

  reads = MODEL.db1->reads;
  input = MODEL.input;
  first = MODEL.first;
  last  = MODEL.last;

  rewind(input);
  fread(&novl,sizeof(int64),1,input);
  fread(&TRACE_SPACING,sizeof(int),1,input);
  if (TRACE_SPACING <= TRACE_XOVR && TRACE_SPACING != 0)
    TBYTES = sizeof(uint8);
  else
    TBYTES = sizeof(uint16);
  if (TRACE_SPACING > 0)
    PANEL_SIZE = ((PANEL_TARGET-1)/TRACE_SPACING+1)*TRACE_SPACING;
  else
    PANEL_SIZE = ((PANEL_TARGET-1)/100+1)*100;

  Read_Overlap(input,&ovl);
  if (ovl.aread >= first)
    MODEL.first = first = ovl.aread;
  else
    while (ovl.aread < first)
      { fseek(input,TBYTES*ovl.path.tlen,SEEK_CUR);
        Read_Overlap(input,&ovl);
        novl -= 1;
      }

  local = (LA *) Malloc(novl*sizeof(LA),"Allocating alignments");
  if (local == NULL)
    return (-1);
  pile  = (Pile *) Malloc(((last-first)+1)*sizeof(Pile),"Allocating piles");
  if (pile == NULL)
    { free(local);
      return (-1);
    }
  pile -= first;

  npan = nspl = nlas = 0;
  pos  = PILE_SPACING/2;
  a    = first-1;
  omax = m = n = 0;
  smax = 0;
  while (ovl.aread < last)
    { while (a < ovl.aread)
        { int j, b, e, p;

          if (a >= first)
            { if (reads[a].rlen < PANEL_FUDGE)
                p = 0;
              else
                p = ((reads[a].rlen-PANEL_FUDGE) / PANEL_SIZE);
              for (j = m; j < n; j++)
                { b = local[j].abpos / PANEL_SIZE;
                  e = (local[j].aepos-1) / PANEL_SIZE;
                  if (e > p)
                    e = p;
                  nlas += e-b;
                }
              nspl += 1;
              npan += p+1;
            }

          a += 1;
          pile[a].first  = n; 
          pile[a].where  = pos;
          pile[a].offset = ftello(input) - OvlIOSize;
          pile[a].panels = npan;
          if (n-m > omax)
            omax = n-m;
          m    = n;
          pos += reads[a].rlen + PILE_SPACING;
        }

      local[n].bread = (ovl.bread << 1);
      if (COMP(ovl.flags))
        local[n].bread |= 0x1;
      local[n].bbpos = ovl.path.bbpos;
      local[n].bepos = ovl.path.bepos;
      local[n].btip  = btip = tiplen(ovl.path.abpos,ovl.path.bbpos);
      local[n].etip  = etip = tiplen(reads[ovl.aread].rlen-ovl.path.aepos,
                                     MODEL.db2->reads[ovl.bread].rlen-ovl.path.bepos);
      local[n].abpos = ovl.path.abpos - btip;
      local[n].aepos = ovl.path.aepos + etip;
      local[n].toff  = ftell(input);
      if (CHAIN_START(ovl.flags))
        local[n].etip |= STRT_FLAG;
      if (CHAIN_NEXT(ovl.flags))
        local[n].etip |= SUCC_FLAG;
      if (ELIM(ovl.flags))
        local[n].etip |= ELIM_BIT;
   
      n += 1;

      tlen = TBYTES*ovl.path.tlen;
      if (smax < tlen)
        smax = tlen;
      fseeko(input,tlen,SEEK_CUR);

      if (feof(input))
        break;
      if (Read_Overlap(input,&ovl))
        break;
    }

  { int j, b, e, p;

    if (a < last-1)
      { MODEL.last = last = a+1;
        pile = (Pile *) Realloc(pile+first,((last-first)+1)*sizeof(Pile),"Reallocating piles");
        pile -= first;
      }

    if (a >= first)
      { if (reads[a].rlen < PANEL_FUDGE)
          p = 0;
        else
          p = ((reads[a].rlen-PANEL_FUDGE) / PANEL_SIZE);
        for (j = m; j < n; j++)
          { b = local[j].abpos / PANEL_SIZE;
            e = (local[j].aepos-1) / PANEL_SIZE;
            if (e > p)
              e = p;
            nlas += e-b;
          }
        nspl += 1;
        npan += p+1;
      }

    pile[last].first  = n;
    pile[last].where  = pos;
    pile[last].offset = ftello(input) - OvlIOSize;
    pile[last].panels = npan;
    if (n-m > omax)
      omax = n-m;
    pile[last].where  -= PILE_SPACING/2;
    pile[last].offset += OvlIOSize;

    if (n < novl)
      local = (LA *) Realloc(local,n*sizeof(LA),"Allocating alignments");
  }

  { LA   *temp;

    nlas = n + nlas;

    panels = (int *) Malloc((npan+1)*sizeof(int),"Panels");
    if (panels == NULL)
      goto error;

    plists = (int *) Malloc(nlas*sizeof(int),"Panel Lists");
    if (plists == NULL)
      goto error;

    temp = (LA *) Realloc(local,(n+1)*sizeof(LA),"Finalizing alignments");
    if (temp == NULL)
      goto error;
    local = temp;
    
    tbuffer = Malloc(smax,"Allocating trace buffer");
    if (tbuffer == NULL)
      goto error;
  }

  { int  a, k;
    int *manels;
    int *count;

    for (k = 0; k <= npan; k++)
      panels[k] = 0;

    manels = panels+1;
    for (a = first; a < last; a++)
      { int j, b, e, p;

        count = manels + pile[a].panels;
        if (reads[a].rlen < PANEL_FUDGE)
          p = 0;
        else
          p = ((reads[a].rlen-PANEL_FUDGE) / PANEL_SIZE);
        for (j = pile[a].first; j < pile[a+1].first; j++)
          { b = local[j].abpos / PANEL_SIZE;
            e = (local[j].aepos-1) / PANEL_SIZE;
            if (e > p)
              e = p;
            for (k = b; k <= e; k++)
              count[k] += 1;
          }
      }

    for (k = 2; k <= npan; k++)
      panels[k] += panels[k-1];

    for (a = first; a < last; a++)
      { int j, b, e, p;

        count = panels + pile[a].panels;
        if (reads[a].rlen < PANEL_FUDGE)
          p = 0;
        else
          p = ((reads[a].rlen-PANEL_FUDGE) / PANEL_SIZE);
        p = ((reads[a].rlen-PANEL_FUDGE) / PANEL_SIZE);
        for (j = pile[a].first; j < pile[a+1].first; j++)
          { b = local[j].abpos / PANEL_SIZE;
            e = (local[j].aepos-1) / PANEL_SIZE;
            if (e > p)
              e = p;
            for (k = b; k <= e; k++)
              plists[count[k]++] = j;
            local[j].abpos += local[j].btip;
            local[j].aepos -= (local[j].etip & DATA_ETIP);
          }
      }

    for (k = npan; k >= 1; k--)
      panels[k] = panels[k-1];
    panels[0] = 0;
  }

  MODEL.local  = local;
  MODEL.pile   = pile;
  MODEL.omax   = omax;
  MODEL.tbuf   = tbuffer;
  MODEL.tspace = TRACE_SPACING;
  MODEL.tbytes = TBYTES;
  MODEL.panels = panels;
  MODEL.plists = plists;

  UNDEFINED = 0;
  if (reLayoutModel(nolink,nolap,elim,max_comp,max_expn))
    { UNDEFINED = 1;
      goto error;
    }

  return (0);

error:
  free(local);
  free(pile+first);
  return (1);
}

static int scanLAS(FILE *input, int *first, int *last)
{ Overlap _ovl, *ovl = &_ovl;
  int     j, novl, tlen;

  fread(&novl,sizeof(int64),1,input);
  fread(&TRACE_SPACING,sizeof(int),1,input);
  if (TRACE_SPACING <= TRACE_XOVR && TRACE_SPACING != 0)
    TBYTES = sizeof(uint8);
  else
    TBYTES = sizeof(uint16);

  Read_Overlap(input,ovl);
  tlen = ovl->path.tlen;
  fseeko(input,tlen*TBYTES,SEEK_CUR);
  *first = *last = ovl->aread;

  for (j = 1; j < novl; j++)
    { Read_Overlap(input,ovl);
      tlen = ovl->path.tlen;
      fseeko(input,tlen*TBYTES,SEEK_CUR);
      *last = ovl->aread;
    }

  return (novl == 0);
}

static void OPEN_MASKS(char *path, char *extn)
{ char *dot, *dot2;
  FILE *data, *anno;
  int   size;

  dot = rindex(extn,'.');
  if (dot != NULL && strcmp(dot,".anno") == 0)
    { *dot = '\0';
      if (rindex(extn,'.') == NULL)
        { dot2 = rindex(path,'.');
          strcpy(dot2,".data");
          data = fopen(path,"r");
          if (data != NULL)
            { strcpy(dot2,".anno");
              anno = fopen(path,"r");
              if (anno != NULL)
                { if (fread(&size,sizeof(int),1,anno) == 1)
                    if (fread(&size,sizeof(int),1,anno) == 1)
                      if (size == 0)
                        Load_Track(MODEL.db1,extn);
                  fclose(anno);
                }
             }
          fclose(data);
        }
      *dot = '.';
    }
}

char *openModel(char *Alas, char *Adb, char *Bdb, int first, int last,
                int nolink, int nolap, int elim, int max_comp, int max_expn)
{ static char buffer[2*MAX_NAME+100];
  static DAZZ_DB _db1;
  static DAZZ_DB _db2;

  FILE *dbfile;

  if (FIRSTCALL)
    { FIRSTCALL = 0;
      Prog_Name = Strdup("DaViewer","");
    }

  if ( ! UNDEFINED)
    { Close_DB(MODEL.db1);
      if (MODEL.db1 != MODEL.db2)
        Close_DB(MODEL.db2);
      fclose(MODEL.input);
      free(MODEL.local);
      free(MODEL.pile+MODEL.first);
      free(MODEL.tbuf);
    }
  UNDEFINED = 1;

  { int   i, nfiles, nblocks, cutoff, all, oindx;
    int64 size;
    int   la_first, la_last;
    int   db_first, db_last;
    FILE *input;

    input = fopen(Alas,"r");
    if (input == NULL)
      { if (rindex(Alas,'/') != NULL)
          Alas = rindex(Alas,'/')+1;
        EPRINTF(EPLACE,"%s: Cannot open file %s\n",Prog_Name,Alas);
        return (Ebuffer);
      }

    if (scanLAS(input,&la_first,&la_last))
      { if (rindex(Alas,'/') != NULL)
          Alas = rindex(Alas,'/')+1;
        EPRINTF(EPLACE,"%s: LAS file %s has no overlaps !\n",Prog_Name,Alas);
        fclose(input);
        return (Ebuffer);
      }
    fclose(input);
 
    dbfile = Fopen(Adb,"r");
    if (dbfile == NULL)
      return (Ebuffer);
    if (fscanf(dbfile,DB_NFILE,&nfiles) != 1)
      goto junk;
    for (i = 0; i < nfiles; i++)
      if (fgets(buffer,2*MAX_NAME+100,dbfile) == NULL)
        goto junk;
    if (fscanf(dbfile,DB_NBLOCK,&nblocks) != 1)
      { if (feof(dbfile))
          { EPRINTF(EPLACE,"%s: Database has not been split!",Prog_Name);
            goto error;
          }
        goto junk;
      }
    if (fscanf(dbfile,DB_PARAMS,&size,&cutoff,&all) != 3)
      goto junk;

    for (i = 0; i <= nblocks; i++)
      { if (fscanf(dbfile,DB_BDATA,&oindx,&db_last) != 2)
          goto junk;
        if (la_first >= db_last)
          db_first = db_last; 
        if (la_last < db_last)
          break;
      }
    fclose(dbfile);

    if (first >= 0)
      { if (first >= db_last)
          { EPRINTF(EPLACE,"First requested read %d is > last read in las block %d\n",
                           first+1,db_last);
            return (Ebuffer);
          }
        else if (first > db_first)
          db_first = first;
      }
    if (last >= 0)
      { if (last <= db_first)
          { EPRINTF(EPLACE,"Last requested read %d is < first read in las block %d\n",
                           last,db_first+1);
            return (Ebuffer);
          }
        else if (last < db_last)
          db_last = last;
      }
    MODEL.first = db_first;
    MODEL.last  = db_last;
  }

  MODEL.db1 = &_db1;
  if (Open_DB(Adb,MODEL.db1) < 0)
    return (Ebuffer);
  if (Bdb == NULL)
    MODEL.db2 = MODEL.db1;
  else
    { MODEL.db2 = &_db2;
      if (Open_DB(Bdb,MODEL.db2) < 0)
        { Close_DB(MODEL.db1);
          return (Ebuffer);
        }
      Trim_DB(MODEL.db2);
    }
  Trim_DB(MODEL.db1);

  if (List_DB_Files(Adb,OPEN_MASKS))
    return (Ebuffer);

  { int        i, n;
    DAZZ_READ *read = MODEL.db2->reads;
 
    n = MODEL.db2->nreads;
    for (i = 0; i < n; i++)
      read[i].flags &= (DB_CSS | DB_BEST);
  }

  { int kind;

    if (Check_Track(MODEL.db1,"prof",&kind) > -2)
      { MODEL.prf = Load_Track(MODEL.db1,"prof");
        if (MODEL.prf == NULL)
          { Close_DB(MODEL.db1);
            if (MODEL.db2 != MODEL.db1)
              Close_DB(MODEL.db2);
            return (Ebuffer);
          }
      }
    else
      MODEL.prf = NULL;

    if (Check_Track(MODEL.db1,"qual",&kind) > -2)
      { MODEL.qvs = Load_Track(MODEL.db1,"qual");
        if (MODEL.qvs == NULL)
          { Close_DB(MODEL.db1);
            if (MODEL.db2 != MODEL.db1)
              Close_DB(MODEL.db2);
            return (Ebuffer);
          }
      }
    else
      MODEL.qvs = NULL;
  }

  MODEL.input = fopen(Alas,"r");

  if (buildModel(nolink,nolap,elim,max_comp,max_expn))
    { fclose(MODEL.input);
      Close_DB(MODEL.db1);
      if (MODEL.db2 != MODEL.db1)
        Close_DB(MODEL.db2);
      return (Ebuffer);
    }

  { DAZZ_TRACK *t, *u, *v;
    int         j;
    int64      *anno;

    u = NULL;
    for (t = MODEL.db1->tracks; t != NULL; t = v)
      { v = t->next;
        t->next = u; 
        u = t;
      }
    MODEL.db1->tracks = u;

    for (t = MODEL.db1->tracks; t != MODEL.qvs && t != MODEL.prf; t = t->next)
      { anno = (int64 *) (t->anno);
        for (j = 0; j <= MODEL.db1->nreads; j++) 
          anno[j] >>= 2;
      }
  }

  UNDEFINED = 0;
  return (NULL);

junk:
  if (rindex(Adb,'/') != NULL)
    Adb = rindex(Adb,'/')+1;
  EPRINTF(EPLACE,"%s: Stub file %s is junk!\n",Prog_Name,Adb);
error:
  fclose(dbfile);
  return (Ebuffer);
}
