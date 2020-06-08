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
static DAZZ_DB   DB1;
static DAZZ_DB   DB2;

int PANEL_SIZE;

int symmetric(DataModel *model)
{ return (model->db1 == model->db2); }

int dataWidth(DataModel *model)
{ if (model == NULL)
    return (1000000);
  return (model->pile[model->last].where);
}

int dataHeight(DataModel *model)
{ if (model == NULL)
    return (256);
  return (model->depth);
}

int readSpan(DataModel *model, int read1, int read2, int *beg, int *end)
{ if (read1 < model->first || read2 > model->last)
    return (1);
  *beg = model->pile[read1].where - PILE_SPACING/2;
  *end = model->pile[read2-1].where + model->db1->reads[read2-1].rlen + PILE_SPACING/2;
  return (0);
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
                { if (low >= 0)
                    { LOCAL[i].btip |= LINK_FLAG;
                      LOCAL[i].level = low;
                    }
                  low = i;
                }
            }
          else
            { if ((LOCAL[i].etip & EMASK) == EVALU)
                low = i;
              else
                low = -1;
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

#ifdef DEBUG_LAYOUT
  for (i = pile->first; i < last; i++)
    { printf("%5d: %d %d",i,(LOCAL[i].btip&INIT_FLAG)!=0,(LOCAL[i].btip&LINK_FLAG)!=0);
      if ((LOCAL[i].btip&INIT_FLAG)!=0)
        printf(" B=%d%c",LOCAL[i].bread>>2,(LOCAL[i].bread&0x1)?'c':'n');
      else
        printf(" @%d",LOCAL[i].bread);
      printf(" @%d\n",LOCAL[i].level);       
    }
#endif

  return (rtop);
}

int reLayoutModel(DataModel *model, int nolink, int nolap, int elim, int max_comp, int max_expn)
{ int   *rail;
  int    depth;
  double comp_factor, expn_factor;
  int    a, n;

  if (model == NULL)
    return (0);

  rail = (int *) Malloc(sizeof(int)*model->omax,"Allocating layout vector");
  if (rail == NULL)
    return (1);

  comp_factor = 1. - max_comp/100.;
  expn_factor = 1. + max_expn/100.;

  LOCAL = model->local;

  depth = 0;
  for (a = model->first; a < model->last; a++)
    { n = layoutPile(model->pile+a,rail,nolink,nolap,elim,comp_factor,expn_factor);
      if (n > depth)
        depth = n;
    }

  free(rail);
  
  model->depth = depth;
  return (0);
}

static int64 OvlIOSize = sizeof(Overlap) - sizeof(void *);

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

static int buildModel(DataModel *model, int64 novl, int64 toff,
                      int nolink, int nolap, int elim, int max_comp, int max_expn)
{ int        first, last;
  FILE      *input;
  DAZZ_READ *reads;
  int        tspace;
  int        tbytes;

  LA   *local;
  Pile *pile;
  void *tbuffer;
  int  *panels, *plists;

  Overlap ovl;
  int     a, pos;
  int     omax, n, m;
  int64   smax, tlen;
  int     btip, etip;
  int     npan, nspl, nlas;

  reads = model->db1->reads;
  input = model->input;
  first = model->first;
  last  = model->last;

  rewind(input);
  fread(&smax,sizeof(int64),1,input);
  fread(&tspace,sizeof(int),1,input);
  if (tspace <= TRACE_XOVR && tspace != 0)
    tbytes = sizeof(uint8);
  else
    tbytes = sizeof(uint16);
  if (tspace > 0)
    PANEL_SIZE = ((PANEL_TARGET-1)/tspace+1)*tspace;
  else
    PANEL_SIZE = ((PANEL_TARGET-1)/100+1)*100;

  fseek(input,toff-OvlIOSize,SEEK_SET);
  Read_Overlap(input,&ovl);

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
  while (1)
    { while (a < ovl.aread && a < last)
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
          m = n;

          if (a >= last)
            break;

          pos += reads[a].rlen + PILE_SPACING;
        }

      if (ovl.aread >= last)
        break;

      local[n].bread = (ovl.bread << 1);
      if (COMP(ovl.flags))
        local[n].bread |= 0x1;
      local[n].bbpos = ovl.path.bbpos;
      local[n].bepos = ovl.path.bepos;
      local[n].btip  = btip = tiplen(ovl.path.abpos,ovl.path.bbpos);
      local[n].etip  = etip = tiplen(reads[ovl.aread].rlen-ovl.path.aepos,
                                     model->db2->reads[ovl.bread].rlen-ovl.path.bepos);
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

      tlen = tbytes*ovl.path.tlen;
      if (smax < tlen)
        smax = tlen;
      fseeko(input,tlen,SEEK_CUR);

      if (Read_Overlap(input,&ovl))
        ovl.aread = last;
    }

  pile[last].where  -= PILE_SPACING/2;
  pile[last].offset += OvlIOSize;

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
    nlas = n + nlas;
  }

  panels = (int *) Malloc((npan+1)*sizeof(int),"Panels");
  plists = (int *) Malloc(nlas*sizeof(int),"Panel Lists");
  tbuffer = Malloc(smax,"Allocating trace buffer");
  if (panels == NULL || plists == NULL || tbuffer == NULL)
    goto error;

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

  model->local  = local;
  model->pile   = pile;
  model->omax   = omax;
  model->tbuf   = tbuffer;
  model->tspace = tspace;
  model->tbytes = tbytes;
  model->panels = panels;
  model->plists = plists;

  if ( ! reLayoutModel(model,nolink,nolap,elim,max_comp,max_expn))
    return (0);

error:
  free(local);
  free(pile+first);
  return (1);
}

static int64 scanLAS(FILE *input, int *first, int *last, int *all, int64 *off)
{ Overlap _ovl, *ovl = &_ovl;
  int     j, tlen;
  int64   novl, aovl, toff;
  int     tspace, tbytes;
  int     rfirst, rlast;
  int     afirst, alast;
  int     bfirst, blast;

  if (fread(&novl,sizeof(int64),1,input) != 1)
    return (-1);
  if (fread(&tspace,sizeof(int),1,input) != 1)
    return (-1);
  if (tspace <= TRACE_XOVR && tspace != 0)
    tbytes = sizeof(uint8);
  else
    tbytes = sizeof(uint16);

  rfirst = *first;
  rlast  = *last;
  if (rfirst < 0)
    rfirst = 0;
  if (rlast < 0)
    rlast = 0x7fffffff;

  aovl = 0;
  afirst = bfirst = 0x7fffffff;
  alast  = blast  = 0;
  for (j = 0; j < novl; j++)
    { if (Read_Overlap(input,ovl))
        return (-1);
      alast = ovl->aread;
      if (ovl->bread < bfirst)
        bfirst = ovl->bread;
      else if (ovl->bread > blast)
        blast = ovl->bread;
      if (j == 0)
        afirst = alast;
      if (rfirst <= alast && alast < rlast)
        { if (rfirst >= 0)
            { toff = ftello(input);
              rfirst = -1;
            }
          aovl += 1;
        }
      tlen = ovl->path.tlen;
      fseeko(input,tlen*tbytes,SEEK_CUR);
    }

  *all   = (bfirst >= afirst && blast <= alast);
  *first = afirst;
  *last  = alast;
  *off   = toff;
  return (aovl);
}

static int64 scanPile(FILE *input, int read, int64 *off)
{ Overlap _ovl, *ovl = &_ovl;
  int     j, tlen;
  int64   novl, aovl, toff;
  int     tspace, tbytes;
  int     aread, first;

  if (fread(&novl,sizeof(int64),1,input) != 1)
    return (-1);
  if (fread(&tspace,sizeof(int),1,input) != 1)
    return (-1);
  if (tspace <= TRACE_XOVR && tspace != 0)
    tbytes = sizeof(uint8);
  else
    tbytes = sizeof(uint16);

  first = 1;
  aovl  = 0;
  for (j = 0; j < novl; j++)
    { if (Read_Overlap(input,ovl))
        return (-1);
      aread = ovl->aread;
      if (aread > read)
        break;
      if (read <= aread)
        { if (first)
            { toff  = ftello(input);
              first = 0;
            }
          aovl += 1;
        }
      tlen = ovl->path.tlen;
      fseeko(input,tlen*tbytes,SEEK_CUR);
    }

  *off = toff;
  return (aovl);
}

static void OPEN_MASKS(char *path, char *extn)
{ char *dot, *dot2;
  FILE *data, *anno;
  int   size;
  DAZZ_TRACK *track;

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
                        { track = Open_Track(MODEL.db1,extn);
                          Load_All_Track_Data(track);
                        }
                  fclose(anno);
                }
              fclose(data);
            }
        }
      *dot = '.';
    }
}

DataModel *openClone(char *Alas, int bread,
                     int nolink, int nolap, int elim, int max_comp, int max_expn, char **mesg)
{ DataModel *clone;
  int        cblock;
  int64      novl, off;

  *mesg = EPLACE;

  //  MODEL cannot be undefined and bread is in DB if this is called

  clone = Malloc(sizeof(DataModel),"Allocating data model");
  if (clone == NULL)
    return (NULL);

  *clone = MODEL;

  if (MODEL.block != 0)
    { char *root, *path, *bptr;

      for (cblock = 1; cblock <= MODEL.stub->nblocks; cblock++)
        if (MODEL.stub->tblocks[cblock-1] <= bread && bread < MODEL.stub->tblocks[cblock])
          break;
   
      root  = Root(Alas,".las");
      path  = PathTo(Alas);
      bptr  = rindex(root,'.');
      *bptr = '\0';
      clone->input = fopen(Catenate(path,"/",root,Numbered_Suffix(".",cblock,".las")),"r");
      *bptr = '.';
      free(path);
      free(root);
      clone->block = cblock;
    }
  else
    { clone->input = fopen(Alas,"r");
      clone->block = 0;
    }

  novl = scanPile(clone->input,bread,&off);
  rewind(clone->input);

  clone->first = bread;
  clone->last  = bread+1;

  if (buildModel(clone,novl,off,nolink,nolap,elim,max_comp,max_expn))
    { fclose(clone->input);
      free(clone);
    }

  clone->nref = 0;
  MODEL.nref += 1;

  return (clone);
}

char *freeClone(DataModel *clone)
{ fclose(clone->input);
  free(clone->local);
  free(clone->pile+clone->first);
  free(clone->tbuf);

  free(clone);
  MODEL.nref -= 1;
  return (NULL);
}

DataModel *openModel(char *Alas, char *Adb, char *Bdb, int first, int last,
                     int nolink, int nolap, int elim, int max_comp, int max_expn, char **mesg)
{ DAZZ_STUB *stub;
  int64      novl, off;
  FILE      *input;

  if (FIRSTCALL)
    { FIRSTCALL = 0;
      Prog_Name = Strdup("DaViewer","");
      MODEL.nref = 1;
    }

  *mesg = EPLACE;

  if ( ! UNDEFINED)
    { if (MODEL.nref != 1)
        { EPRINTF(EPLACE,"%s: Cannot change data set until all clones are closed\n",Prog_Name);
          return (NULL);
        }
      fclose(MODEL.input);
      free(MODEL.local);
      free(MODEL.pile+MODEL.first);
      free(MODEL.tbuf);
      if (MODEL.db1 != MODEL.db2)
        Close_DB(MODEL.db2);
      Close_DB(MODEL.db1);
      Free_DB_Stub(MODEL.stub);
    }
  UNDEFINED = 1;

  { int   cblock;
    int   la_first, la_last, all;
    int   db_first, db_last;
    char *root, *path;

    root  = Root(Alas,".las");
    path  = PathTo(Alas);
    input = fopen(Catenate(path,"/",root,".las"),"r");
    if (input == NULL)
      { EPRINTF(EPLACE,"%s: Cannot open file %s/%s.las\n",Prog_Name,path,root);
        free(path);
        free(root);
        return (NULL);
      }
    free(path);
    free(root);

    la_first = first;
    la_last  = last;
    novl = scanLAS(input,&la_first,&la_last,&all,&off);
    if (novl < 0)
      { EPRINTF(EPLACE,"%s: LAS file %s econding error !\n",Prog_Name,Alas);
        goto error_las;
      }
    if (novl == 0)
      { EPRINTF(EPLACE,"%s: LAS file %s has no overlaps !\n",Prog_Name,Alas);
        goto error_las;
      }

    stub = Read_DB_Stub(Adb,DB_STUB_BLOCKS);
    if (stub == NULL)
      goto error_las;

    for (cblock = stub->nblocks; cblock > 0; cblock--)
      { if (stub->tblocks[cblock-1] <= la_first && la_last < stub->tblocks[cblock])
          break;
      }
    if (cblock == 0 || stub->nblocks == 1)
      { db_first = 0;
        db_last  = stub->tblocks[stub->nblocks];
        if (!all && (Bdb == NULL || strcmp(Adb,Bdb) == 0))
          { if (rindex(Alas,'/') != NULL)
              Alas = rindex(Alas,'/')+1;
            EPRINTF(EPLACE,"%s: LAS file %s must either be for a block or all of DB !\n",
                           Prog_Name,Alas);
            goto error_stub;
          }
        cblock = 0;
      }
    else
      { char *bptr, *fptr;
        int   part;

        db_first = stub->tblocks[cblock-1];
        db_last  = stub->tblocks[cblock];
        root  = Root(Alas,".las");
        path  = PathTo(Alas);
        bptr  = rindex(root,'.');
        if (bptr != NULL && bptr[1] != '\0' && bptr[1] != '-')
          { part = strtol(bptr+1,&fptr,10);
            if (*fptr != '\0' || part == 0)
              part = 0;
            else
              *bptr = '\0';
          }
        else
          part = 0;
        free(path);
        free(root);
        if (part == 0)
          { EPRINTF(EPLACE,"%s: %s is not an .las block file\n",Prog_Name,root);
            goto error_stub;
          }
        if (part != cblock)
          { EPRINTF(EPLACE,"%s: The block %d identified for %s does not match\n",
                           Prog_Name,cblock,root);
            goto error_stub;
          }
      }

    if (first >= 0)
      { if (first < db_first)
          { EPRINTF(EPLACE,"%s: First requested read %d is < first read, %d, in las block\n",
                           Prog_Name,first+1,db_first+1);
            goto error_stub;
          }
      }
    else
      first = db_first;
    if (last >= 0)
      { if (last > db_last)
          { if (cblock == 0)
              EPRINTF(EPLACE,"%s: Last requested read %d is > last read, %d, in the database\n",
                             Prog_Name,last,db_last);
            else
              EPRINTF(EPLACE,"%s: Last requested read %d is > last read, %d, in las block\n",
                             Prog_Name,last,db_last);
            goto error_stub;
          }
      }
    else
      last = db_last;

    MODEL.stub  = stub;
    MODEL.first = first;
    MODEL.last  = last;
    MODEL.block = cblock;
    MODEL.input = input;

    rewind(input);
  }

  MODEL.db1 = &DB1;
  if (Open_DB(Adb,MODEL.db1) < 0)
    goto error_stub;
  if (Bdb == NULL || strcmp(Adb,Bdb) == 0)
    MODEL.db2 = MODEL.db1;
  else
    { MODEL.db2 = &DB2;
      if (Open_DB(Bdb,MODEL.db2) < 0)
        { Close_DB(MODEL.db1);
          goto error_stub;
        }
      Trim_DB(MODEL.db2);
    }
  Trim_DB(MODEL.db1);

  if (List_DB_Files(Adb,OPEN_MASKS))
    goto error_db;

  { int        i, n;
    DAZZ_READ *read = MODEL.db2->reads;
 
    n = MODEL.db2->nreads;
    for (i = 0; i < n; i++)
      read[i].flags &= (DB_CCS | DB_BEST);
  }

  { int kind;

    if (Check_Track(MODEL.db1,"prof",&kind) > -2)
      { MODEL.prf = Open_Track(MODEL.db1,"prof");
        if (MODEL.prf == NULL)
          goto error_db;
        Load_All_Track_Data(MODEL.prf);
      }
    else
      MODEL.prf = NULL;

    if (Check_Track(MODEL.db1,"qual",&kind) > -2)
      { MODEL.qvs = Open_Track(MODEL.db1,"qual");
        if (MODEL.qvs == NULL)
          goto error_db;
        Load_All_Track_Data(MODEL.qvs);
      }
    else
      MODEL.qvs = NULL;
  }

  if (buildModel(&MODEL,novl,off,nolink,nolap,elim,max_comp,max_expn))
    goto error_db;

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
  return (&MODEL);

error_db:
  if (MODEL.db2 != MODEL.db1)
    Close_DB(MODEL.db2);
  Close_DB(MODEL.db1);
error_stub:
  Free_DB_Stub(stub);
error_las:
  fclose(input);
  return (NULL);
}
