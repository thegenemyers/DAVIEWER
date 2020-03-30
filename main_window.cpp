#include <stdio.h>
#include <math.h>

#include <QtGui>

extern "C" {
#include "piler.h"
#include "align.h"
}

#undef HIFI

#include "main_window.h"

#define DAVIEW_MIN_WIDTH   500   //  Minimum display window width & height
#define DAVIEW_MIN_HEIGHT  350

#define PALETTE_MIN_WIDTH   400   //  Minimum display window width & height
#define PALETTE_MIN_HEIGHT  600

#define POS_TICKS   1000000000    //  Resolution of sliders
#define REAL_TICKS  1000000000.

#define MIN_HORZ_VIEW  1000    //  Minimum bp in a view
#define MIN_VERT_VIEW    30    //  Minimum rows in a view

#define MIN_FEATURE_SIZE  3.   //  Minimum pixel length to display a feature

QRect *MainWindow::screenGeometry = 0;
int    MainWindow::frameWidth;
int    MainWindow::frameHeight;

int    MyCanvas::rulerHeight = -1;
int    MyCanvas::rulerWidth;
int    MyCanvas::labelWidth;
int    MyCanvas::rulerT1;
int    MyCanvas::rulerT2;


/*****************************************************************************\
*
*  MY LINE-EDIT
*
\*****************************************************************************/

MyLineEdit::MyLineEdit(QWidget *parent) : QLineEdit(parent) { process = false; }

void MyLineEdit::keyPressEvent(QKeyEvent *event)
{ if (process)
    { process = false;
      emit touched();
    }
  QLineEdit::keyPressEvent(event);
}

void MyLineEdit::mousePressEvent(QMouseEvent *event)
{ if (process)
    { process = false;
      emit touched();
    }
  QLineEdit::mousePressEvent(event);
}

void MyLineEdit::focusOutEvent(QFocusEvent *event)
{ emit focusOut();
  QLineEdit::focusOutEvent(event);
}

void MyLineEdit::processed(bool on)
{ process = on; }


/*****************************************************************************\
*
*  MY CANVAS
*
\*****************************************************************************/

MyMenu::MyMenu(QWidget *parent) : QMenu(parent) { }

void MyMenu::mouseReleaseEvent(QMouseEvent *ev)
{ QAction *act = actionAt(ev->pos());

  if (act != NULL)
    act->trigger();
  QCoreApplication::sendEvent(parent(),ev);
}

void MyMenu::mousePressEvent(QMouseEvent *ev)
{ QCoreApplication::sendEvent(parent(),ev); }

int MyCanvas::digits(int a, int b)
{ int l;

  if (a < b)
    a = b;
  l = 0;
  while (a > 0)
    { a /= 10;
      l += 1;
    }
  return (l);
}

int MyCanvas::find_pile(int beg, Pile *a, int l, int r)
{ int m;

  // smallest k s.t. a[k].where >= beg (or r if does not exist)

  while (l < r)
    { m = ((l+r) >> 1);
      if (a[m].where < beg)
        l = m+1;
      else
        r = m;
    }
  return (l);
}

int MyCanvas::find_mask(int beg, int *a, int l, int r)
{ int m;

  // smallest k (multiple of 2 s.t. a[k+1] >= beg (or r if does not exist)

  l >>= 1;
  r >>= 1;
  while (l < r)
    { m = ((l+r) >> 1);
      if (a[(m<<1)+1] < beg)
        l = m+1;
      else
        r = m;
    }
  return (l << 1);
}

int MyCanvas::pick(int x, int y, int &aread, int &bread)
{ MyScroll  *scroll  = (MyScroll *) parent();

  DAZZ_DB *db1, *db2;
  int      first, last;
  Pile     *pile;
  LA       *align;
  int      *panel, *plist;

  double   hpos, vpos;
  double   hbp, vbp;
  int      fap, len;

  //  Put model in variables

  db1   = model->db1;
  db2   = model->db2;
  first = model->first;
  last  = model->last;
  pile  = model->pile;
  align = model->local;
  panel = model->panels;
  plist = model->plists;

  { QScrollBar *vscroll, *hscroll;
    int         page;

    vscroll = scroll->vertScroll;
    hscroll = scroll->horzScroll;

    page  = hscroll->pageStep();
    hpos  = (x * 2. * page) / rect().width() + hscroll->value();
    hpos *= scroll->hmax / (REAL_TICKS+page);
    hbp   = 3. * (2.*page / rect().width()) * (scroll->hmax / (REAL_TICKS+page));

    page  = vscroll->pageStep();
    vpos  = ((y - 3) * 2. * page) / (rect().height() - rulerHeight) + vscroll->value();
    vpos *= scroll->vmax / (REAL_TICKS+page);
    vbp   = 5. * (2.*page / (rect().height()-rulerHeight)) * (scroll->vmax / (REAL_TICKS+page));
  }

  //  Find pile and panel(s) containing event->x()

  { int d;

    aread = find_pile(hpos,pile,first,last);

    if (aread > first)
      { d = pile[aread-1].where + db1->reads[aread-1].rlen - hpos;
        if (d > 0 || aread >= last || hpos - pile[aread].where < d)
          aread -= 1;
        else
          d = hpos - pile[aread].where;
      } 
    else
      d = hpos - pile[aread].where;
    if (d < -hbp)
      return (-2);
    hpos -= pile[aread].where;

    len = 1;
    if (hpos < 0)
      fap = 0;
    else
      { fap = ((int) (hpos+hbp))/PANEL_SIZE;
        if (fap >= pile[aread+1].panels - pile[aread].panels)
          fap -= 1;
        else
          { d = ((int) hpos) % PANEL_SIZE;
            if (d <= hbp || d >= PANEL_SIZE-hbp)
              { fap -= 1;
                len = 2;
              }
          }
      }
    fap += pile[aread].panels;
  }

  { int    p, bst;
    double d, dst;
    int    readRow, firstBRow;
    Palette_State *palette = &(MainWindow::palette);

    readRow   = 3 + MainWindow::numLive;
    firstBRow = readRow+2;

    bst = -1;
    dst = INT32_MAX;

    if (vpos <= readRow + vbp)
      { int j, y;

        d = fabs(readRow-vpos);
        if (d <= vbp)
          { bread = -1;
            bst   = 0;
            dst   = d;
          }

        for (j = 0; j < palette->nmasks; j++)
          if (palette->showTrack[j])
            { int64 *anno = (int64 *) palette->track[j]->anno;
              int   *data = (int *) palette->track[j]->data;
              int64  a;

              if (anno[aread] >= anno[aread+1])
                continue;

              y = readRow - palette->Arail[j];
              d = fabs(y-vpos);
              if (d > vbp)
                continue;
         
              a = find_mask(hpos,data,anno[aread],anno[aread+1]);
              if (data[a] <= hpos + hbp && data[a+1] >= hpos - hbp)
                { if (d <= dst)
                    { bread = -(j+2);
                      bst   = a;
                      dst   = d;
                    }
                }
            }
      }

    if (vpos >= firstBRow-vbp)
      { short EMASK;
        short EVALU;

        if (palette->drawElim < 0)
          { EMASK = ELIM_BIT;
            EVALU = 0;
          }
        else if (palette->drawElim == 0)
          { EMASK = 0;;
            EVALU = 0;
          }
        else
          { EMASK = ELIM_BIT;
            EVALU = ELIM_BIT;
          }

        for (p = panel[fap]; p < panel[fap+len]; p++) 
          { int y, f, e, b;

            f = plist[p];
            if ((align[f].etip & EMASK) != EVALU)
              continue;
            b = align[f].btip;
            if ((b & LINK_FLAG) != 0)
              { if ((b & INIT_FLAG) == 0)
                  e = align[f].bread; 
                else
                  { e = align[f].level; 
                    if ((align[e].btip & LINK_FLAG) != 0)
                      e = align[e].bread;
                  }
              }
            else
              e = f;

            y = align[e].level+firstBRow;
            d = fabs(y-vpos);
            if (d > vbp)
              continue;

            if (align[f].abpos <= hpos + hbp && align[f].aepos >= hpos - hbp)
              { if (d < dst)
                  { if ((align[e].btip & INIT_FLAG) == 0)
                      e = align[e].bread;
                    bread = align[e].bread;
                    bst   = e;
                    dst   = d;
                  }
              }
          }
      }

    return (bst);
  }
}

void MyCanvas::haloUpdate(bool ison)
{ if (ison)
    { popup->addAction(colorAct);
      setMouseTracking(true);
    }
  else
    { popup->removeAction(colorAct);
      setMouseTracking(false);
    }
}

void MyCanvas::setModel(DataModel *m)
{ model = m; }

void MyCanvas::assignColor()
{ DAZZ_READ *read = model->db2->reads;
  int        cidx = read[haloed].flags & DB_QV;
  int        i;
  static QColor newColor = QColor(255,125,255);

  if (cidx == 0)
    { for (i = 1; i <= DB_QV; i++)
        if (avail[i])
          break;
      if (i > DB_QV)
        { printf("Overflow\n");
          return;
        }
      newColor = QColorDialog::getColor(newColor,this,tr("Read Color"));
      if ( ! newColor.isValid())
        return;
      avail[i] = false;
      read[haloed].flags |= i;
      colors[i] = newColor;
      update();
    }
  else
    { i = (read[haloed].flags & DB_QV);
      avail[i] = true;
      read[haloed].flags &= (DB_BEST | DB_CCS);
      update();
    }
}

void MyCanvas::setColor(QColor &color, int read)
{ DAZZ_READ *reads = model->db2->reads;
  int        cidx  = reads[read].flags & DB_QV;
  int i;

  if (cidx == 0)
    { for (i = 1; i <= DB_QV; i++)
        if (avail[i])
          break;
      if (i > DB_QV)
        { printf("Overflow\n");
          return;
        }
    }
  else
    i = cidx;

  avail[i]  = false;
  colors[i] = color;
  reads[read].flags |= i;
  update();
}

void MyScroll::setColor(QColor &color, int read)
{ mycanvas->setColor(color,read); }

void MyCanvas::showPile()
{ DataModel  *pile; 
  MainWindow *main;
  char       *v;

  Open_State    *os = &MainWindow::dataset;
  Palette_State *ps = &MainWindow::palette;

  pile = openClone( os->lasInfo->absoluteFilePath().toLatin1().data(),
                    haloed,!ps->bridges,!ps->overlaps,ps->drawElim,
                    ps->compressMax,ps->stretchMax,&v);

  if (pile == NULL)
    { MainWindow::warning(tr(v+10),this,MainWindow::ERROR,tr("OK"));
      return;
    }

  main = new MainWindow(NULL);

  main->setModel(pile);
  main->setWindowTitle(tr("Pile - %1").arg(haloed));
  main->update();

  main->raise();
  main->show();
}

void MainWindow::setModel(DataModel *pile)
{ model = pile;
  myscroll->setModel(pile);
  myscroll->setRange(dataWidth(pile),dataHeight(pile)+5+MainWindow::numLive);
  if (dataWidth(pile) > 1000000)
    myscroll->hsToRange(0,1000000);
  else
    myscroll->hsToRange(0,dataWidth(pile));
}

MyCanvas::MyCanvas(QWidget *parent) : QWidget(parent)
{ int i;
 
  colorAct = new QAction(tr("Color"),this);
    colorAct->setToolTip(tr("Assign a color to this read"));
    colorAct->setFont(QFont(tr("Monaco"),11));

  viewAct = new QAction(tr("View Pile"),this);
    viewAct->setToolTip(tr("View pile for this read"));
    viewAct->setFont(QFont(tr("Monaco"),11));

  aline = new QAction(tr(""),this);
    aline->setFont(QFont(tr("Monaco"),11));
    aline->setEnabled(false);

  bline = new QAction(tr(""),this);
    bline->setFont(QFont(tr("Monaco"),11));
    bline->setEnabled(false);

  popup = new MyMenu(this);
    popup->addAction(aline);
    popup->addAction(bline);
    popup->addAction(viewAct);
    popup->addAction(colorAct);

  mline = new QAction(tr(""),this);
    mline->setFont(QFont(tr("Monaco"),11));
    mline->setEnabled(false);

  annup = new MyMenu(this);
    annup->addAction(mline);

  haloed = -1;
  for (i = 1; i <= DB_QV; i++)
    avail[i] = true;
  setMouseTracking(true);

  buttonDown = false;
  menuLock   = false;
  model      = NULL;

  QPalette pal = popup->palette();
  pal.setBrush(QPalette::Disabled,QPalette::Text,QBrush(Qt::black));
  popup->setPalette(pal);
  annup->setPalette(pal);

  connect(colorAct, SIGNAL(triggered()), this, SLOT(assignColor()));
  connect(viewAct, SIGNAL(triggered()), this, SLOT(showPile()));

  connect(popup, SIGNAL(aboutToHide()), this, SLOT(hidingMenu()));
  connect(annup, SIGNAL(aboutToHide()), this, SLOT(hidingMenu()));
}

void MyCanvas::mouseMoveEvent(QMouseEvent *event)
{ if (menuLock)
    return;
  if (buttonDown)
    { int xpos = event->x();
      int ypos = event->y();
      double xdel = hscale*(xpos-mouseX);
      double ydel = vscale*(ypos-mouseY);
      if (xdel < 0.) xdel -= 1.;
      if (ydel < 0.) ydel -= 1.;
      int xval = hbar->value() - xdel;
      int yval = vbar->value() - ydel;
      hbar->setValue(xval);
      vbar->setValue(yval);
      mouseX = xpos;
      mouseY = ypos;
      haloed = -1;
      return;
    }
  if (doHalo)
    { if (event->y() < rect().height()-rulerHeight && model != NULL)
        { int bst, aread, bread;

          bst = pick(event->x(),event->y(),aread,bread);
          if (haloed < 0)
            { if (bst >= 0 && bread >= 0)
                { haloed = (bread >> 1);
                  update();
                }
            }
          else
            { if (bst < 0 || bread < 0)
                { haloed = -1;
                  update();
                }
              else
                { bread >>= 1;
                  if (bread != haloed)
                    { haloed = bread;
                      update();
                    }
                }
            }
        }
    }
}

void MyCanvas::mouseReleaseEvent(QMouseEvent *event)
{ (void) event;
  buttonDown = false;
  setCursor(Qt::ArrowCursor);
  if ( ! menuLock)
    { popup->hide();
      annup->hide();
    }
}

void MyCanvas::hidingMenu()
{ menuLock = false; }

void MyCanvas::mousePressEvent(QMouseEvent *event)
{ MyScroll *scroll  = (MyScroll *) parent();

/*
  printf("  At (%d,%d)",event->x(),event->y());
  if (event->modifiers() & Qt::SHIFT)
    printf(" SHIFT");
  if (event->modifiers() & Qt::ALT)
    printf(" ALT");
  if (event->modifiers() & Qt::CTRL)
    printf(" COMM");
  if (event->modifiers() & Qt::META)
    printf(" CTRL");
  printf("\n");
*/

  if (menuLock)
    { menuLock = false;
      return;
    }

  if (event->y() >= rect().height()-rulerHeight)

    { int i, page, hpos;

      page = scroll->horzScroll->pageStep();
      hpos = (event->x() * 2. * page) / rect().width() + scroll->horzScroll->value();

      if (event->modifiers() & Qt::SHIFT)
        { hpos *= (REAL_TICKS + page) / (REAL_TICKS + .7*page);
          page /= .7;
          hpos -= page;

          for (i = scroll->horzZoom->value(); i < SIZE_TICKS; i++)
            if (scroll->hstep[i] < page)
              break;
        }
      else
        { hpos *= (REAL_TICKS + .7*page) / (REAL_TICKS + page);
          page *= .7;
          hpos -= page;

          for (i = scroll->horzZoom->value(); i > 0; i--)
            if (scroll->hstep[i] < page)
              break;
        }

      scroll->horzZoom->setValue(i);
      scroll->horzScroll->setMaximum(POS_TICKS-page);
      scroll->horzScroll->setPageStep(page);
      scroll->horzScroll->setValue(hpos);
      update();
    }

  else if (model != NULL)

    { int bst, aread, bread;

      bst = pick(event->x(),event->y(),aread,bread);

      if (bst >= 0)
        if (bread >= 0)
          { DAZZ_DB *db1, *db2;
            LA       *align;

            int     w, e;
            int     alv, blv;
            int     av, bv;
            int     blen, comp, cont;
            QString astr, bstr;

            db1   = model->db1;
            db2   = model->db2;
            align = model->local;

            comp    = (bread % 2);
            bread >>= 1;
            blen    = db2->reads[bread].rlen;

            av = aread+1;
            bv = bread+1;
            w  = digits(av,bv);
            astr.append(tr("%1:").arg(av,w));
            bstr.append(tr("%1:").arg(bv,w));

            av = align[bst].abpos;
            if (comp)
              bv = blen - align[bst].bbpos;
            else
              bv = align[bst].bbpos;

            w = digits(av,bv);
            astr.append(tr(" %1").arg(av,w));
            bstr.append(tr(" %1").arg(bv,w));

            alv = align[bst].aepos;
            if (comp)
              blv = blen - align[bst].bepos;
            else
              blv = align[bst].bepos;

            e = bst;
            while ((align[e].btip & LINK_FLAG) != 0)
              { e = align[e].level;

                cont = (align[e].aepos <= alv);

                if (cont)
                  { astr.append(tr(" - ["));
                    bstr.append(tr(" - ["));
                  }
                else
                  { w = digits(alv,blv);
                    astr.append(tr(" - %1").arg(alv,w));
                    bstr.append(tr(" - %1").arg(blv,w));
  
                    av = align[e].abpos - alv;
                    if (comp)
                      bv = align[e].bbpos - (blen-blv);
                    else
                      bv = align[e].bbpos - blv;
                    w  = digits(av,bv);
  
                    astr.append(tr(" {%1}").arg(av,w));
                    bstr.append(tr(" {%1}").arg(bv,w));
                  }
  
                av = align[e].abpos;
                if (comp)
                  bv = blen - align[e].bbpos;
                else
                  bv = align[e].bbpos;
  
                w  = digits(av,bv);
                astr.append(tr(" %1").arg(av,w));
                bstr.append(tr(" %1").arg(bv,w));
  
                av = align[e].aepos;
                if (comp)
                  bv = blen - align[e].bepos;
                else
                  bv = align[e].bepos;
  
                if (cont)
                  { w = digits(av,bv);
                    astr.append(tr(" - %1 ]").arg(av,w));
                    bstr.append(tr(" - %1 ]").arg(bv,w));
                  }
                else
                  { alv = av;
                    blv = bv;
                  }
              }
            w = digits(alv,blv);
            astr.append(tr(" - %1").arg(alv,w));
            bstr.append(tr(" - %1").arg(blv,w));
  
            aline->setText(astr);
            bline->setText(bstr);
            if ((db2->reads[bread].flags & DB_QV) == 0)
              colorAct->setText(tr("Color"));
            else
              colorAct->setText(tr("Uncolor"));

            menuLock = ((event->modifiers() & Qt::SHIFT) != 0);

            popup->exec(event->globalPos());
          }

        else
          { QString astr;

            if (bread == -1)
             { int alen;

               alen = model->db1->reads[aread].rlen;
               astr.append(tr("%1[0..%2]").arg(aread+1).arg(alen));
             }
            else
              { Palette_State *palette = &(MainWindow::palette);
                int j, *data;

                j = -(bread+2); 
                data = (int *) palette->track[j]->data;
                astr.append(tr("%1[%2..%3]").arg(palette->track[j]->name)
                                            .arg(data[bst]).arg(data[bst+1]));
              }
            mline->setText(astr);

            menuLock = ((event->modifiers() & Qt::SHIFT) != 0);

            annup->exec(event->globalPos());
          }

      else
        { int page;

          buttonDown = true;
          mouseX = event->x();
          mouseY = event->y();
          setCursor(Qt::ClosedHandCursor);

          hbar   = scroll->horzScroll;
          page   = hbar->pageStep();
          hscale = (2.*page) / rect().width();
          vbar   = scroll->vertScroll;
          page   = vbar->pageStep();
          vscale = (2.*page) / (rect().height()-rulerHeight);
        }
    }
}

int MyCanvas::mapToA(int mode, LA *align, uint8 *trace, int tspace, int bp, int lim)
{ static int pt, ab, ae, bb, be;
  static int comp;
  
  if (mode >= 0)
    { comp = mode;
      if (mode == 0)
        { pt = 1;
          be = align->bbpos + trace[pt];
          ae = ((align->abpos/tspace)+1)*tspace;
          while (ae <= lim)
            { pt += 2;
              be += trace[pt];
              ae += tspace;
            }
          if (ae > align->aepos)
            ae = align->aepos;
          if (pt <= 0)
            { bb = align->bbpos;
              ab = align->abpos;
            }
          else
            { bb = be - trace[pt];
              ab = ae - tspace;
            }
        }
      else
        { pt = bp+1;
          bb = align->bepos - trace[pt];
          ab = ((align->aepos-1)/tspace)*tspace;
          while (ab >= lim)
            { pt -= 2;
              bb -= trace[pt];
              ab -= tspace;
            }
          if (ab < align->abpos)
            ab = align->abpos;
          if (pt > bp)
            { be = align->bepos;
              ae = align->aepos;
            }
          else
            { be = bb + trace[pt];
              ae = ab + tspace;
            }
        }
      return (0);
    }
  else
    if (comp == 0)
      { while (be < bp)
          { pt += 2;
            bb  = be;
            be += trace[pt];
            ab  = ae;
            ae += tspace;
          }
        if (ae > align->aepos)
          ae = align->aepos;
      }
    else
      { while (bb > bp)
          { pt -= 2;
            be  = bb;
            bb -= trace[pt];
            ae  = ab;
            ab -= tspace;
          }
        if (ab < align->abpos)
          ab = align->abpos;
      }
  return (ab + ((1.*(bp-bb))/(be-bb))*(ae-ab));
}

void MyCanvas::paintEvent(QPaintEvent *event)
{ QPainter       painter(this);
  MyScroll      *scroll  = (MyScroll *) parent();
  Palette_State *palette = &(MainWindow::palette);

  QWidget::paintEvent(event);

  //  First call with a new model?  Then establish ruler parameters

  if (model != NULL && rulerHeight < 0)
    { QRect *bound = new QRect();

      painter.setFont(QFont(tr("Monaco")));
      painter.drawText(0,DAVIEW_MIN_HEIGHT/2,DAVIEW_MIN_WIDTH,DAVIEW_MIN_HEIGHT/2,
                       Qt::AlignLeft|Qt::AlignBottom,tr("%1").arg(model->last),bound);
      rulerHeight = bound->height() + 4;
      labelWidth  = bound->width();

      painter.drawText(0,DAVIEW_MIN_HEIGHT/2,DAVIEW_MIN_WIDTH,DAVIEW_MIN_HEIGHT/2,
                       Qt::AlignLeft|Qt::AlignBottom,tr("%1").arg(model->db1->maxlen),bound);

      rulerWidth = bound->width();
      if (labelWidth > rulerWidth)
        rulerWidth = labelWidth;

      rulerT1 = 30.*dataWidth(model)/(model->last - model->first);
      rulerT2 =  2.*dataWidth(model)/(model->last - model->first);

      delete bound;
    }

  //  Paint background
  
  painter.setBrush(QBrush(palette->backColor));
  painter.drawRect(rect());

  //  If have a model, then draw

  if (model != NULL)

    { DAZZ_DB *db1, *db2;
      int      first, last;
      Pile     *pile;
      LA       *align;
      int      *panel, *plist;

      int      high, wide;
      double   vbeg, vend;
      double   hbeg, hend;
      int      ibeg, iend;
      double   hbp, vbp;

      int      p, fpile, lpile;
      int      fpanel, lpanel;

      bool        doGrid, doBridge, doOverlap;
      bool        doRead, doPile, doProf, doElim;
      short       EMASK, EVALU;
      int         bAnno;
      DAZZ_TRACK *track[palette->nmasks];
      int         tIndex[palette->nmasks];
      int         readRow, firstBRow;

      QPen     cPen, dPen, rPen, hPen, ePen, qPen[10], mPen[10], pPen[20];
      QColor   stretch [21];
      QColor   compress[21];
      double   stfact, cmfact;

      //  Put model in variables

      db1   = model->db1;
      db2   = model->db2;
      first = model->first;
      last  = model->last;
      pile  = model->pile;
      align = model->local;
      panel = model->panels;
      plist = model->plists;

      //  Get the mapping to the current view Pixel(x,y) = ((x-hbeg)*hbp, (y-vbeg)*vbp) 

      { QScrollBar *vscroll, *hscroll;
        int         vmax, hmax;
        int         page;

        vscroll = scroll->vertScroll;
        hscroll = scroll->horzScroll;

        wide    = rect().width();
        high    = rect().height();

        hmax    = scroll->hmax;
        page    = hscroll->pageStep();
        hbeg    = (hscroll->value() / (REAL_TICKS+page)) * hmax;
        hend    = hbeg + ((2.*page) / (REAL_TICKS+page)) * hmax;
        hbp     = wide / (hend-hbeg);

        vmax    = scroll->vmax;
        page    = vscroll->pageStep();
        vbeg    = (vscroll->value() / (REAL_TICKS+page)) * vmax;
        vend    = vbeg + ((2.*page) / (REAL_TICKS+page)) * vmax;
        vbp     = (high - rulerHeight) / (1.*(vend-vbeg));
      }

      //  Find range [fpile,lpile] of piles either completely or partially in the current view

      ibeg = hbeg;
      iend = hend;

      fpile = find_pile(ibeg,pile,first,last);
      if (fpile > first && pile[fpile-1].where + db1->reads[fpile-1].rlen > ibeg)
        fpile -= 1;
      for (lpile = fpile; lpile < last; lpile++)
        if (pile[lpile].where >= iend)
          break;

      fpanel = pile[fpile].panels + (ibeg - pile[fpile].where)/PANEL_SIZE;
      lpanel = pile[lpile-1].panels + (iend - pile[lpile-1].where)/PANEL_SIZE + 1;
      if (lpanel > pile[lpile].panels)
        lpanel = pile[lpile].panels;

      //  Set up drawing flags and constants based on palette

      { int  j;
        bool sym = ! (MainWindow::dataset).asym;

        doHalo = (MIN_HORZ_VIEW*hbp >= MIN_FEATURE_SIZE && palette->showHalo);
        doElim = palette->showElim;

        if (palette->drawElim < 0)
          { EMASK = ELIM_BIT;
            EVALU = 0;
          }
        else if (palette->drawElim == 0)
          { EMASK = 0;;
            EVALU = 0;
          }
        else
          { EMASK = ELIM_BIT;
            EVALU = ELIM_BIT;
            doElim = false;
          }

        doGrid    = palette->showGrid;
        doBridge  = palette->bridges;
        doOverlap = palette->overlaps;
        doPile = ! (model->tspace*hbp >= MIN_FEATURE_SIZE && (palette->matchqv ||
                        (palette->qualVis && palette->qualqv && palette->qualonB && sym)));
        doRead = ! (model->tspace*hbp >= MIN_FEATURE_SIZE && palette->qualVis && palette->qualqv);
        doProf = (model->tspace*hbp*5 >= MIN_FEATURE_SIZE && palette->profVis && palette->profqv
                    && vbp >= 5.);

        bAnno = 0;
        if (MIN_HORZ_VIEW*hbp >= MIN_FEATURE_SIZE)
          for (j = 0; j < palette->nmasks; j++)
            if (palette->showTrack[j] && palette->showonB[j] && sym)
              { track[bAnno]  = palette->track[j];
                tIndex[bAnno] = j;
                bAnno += 1;
              }

        readRow   = 3 + MainWindow::numLive;
        firstBRow = readRow+2;
      }

      //  Set up drawing pens and colors

      { QVector<qreal> dash, rdash;

        painter.setRenderHint(QPainter::Antialiasing,false);

        cPen.setWidth(2);
        cPen.setCapStyle(Qt::FlatCap);

        dPen.setWidth(2);
        dPen.setCapStyle(Qt::FlatCap);
        dash << 2 << 2;
        dPen.setDashPattern(dash);

        rPen.setColor(palette->gridColor);
        rPen.setWidth(1);
        rPen.setCapStyle(Qt::FlatCap);
        rdash << 1 << 2;
        rPen.setDashPattern(rdash);

        hPen.setWidth(4);
        hPen.setCapStyle(Qt::FlatCap);

        ePen.setWidth(4);
        ePen.setCapStyle(Qt::FlatCap);
        ePen.setColor(palette->elimColor);

        if (doBridge || doOverlap)
          { int nr = palette->neutralColor.red();
            int ng = palette->neutralColor.green();
            int nb = palette->neutralColor.blue();
            int sr = palette->stretchColor.red();
            int sg = palette->stretchColor.green();
            int sb = palette->stretchColor.blue();
            int cr = palette->compressColor.red();
            int cg = palette->compressColor.green();
            int cb = palette->compressColor.blue();

            double in, out;
            int    idx;

            in  = 1.0;
            out = 0.0;
            for (idx = 0; idx <= 20; idx++)
              { stretch [idx].setRgb((int) (in*nr+out*sr),(int) (in*ng+out*sg),
                                                          (int) (in*nb+out*sb));
                compress[idx].setRgb((int) (in*nr+out*cr),(int) (in*ng+out*cg),
                                                                (int) (in*nb+out*cb));
                in  -= .05;
                out += .05;
              }
            stfact = (100.*20.99) / palette->stretchMax;
            cmfact = (100.*20.99) / palette->compressMax;
          }

        if ( ! doRead)
          { int j;

            if (palette->qualMode == 0)
              { for (j = 0; j < 10; j++)
                  { qPen[j].setWidth(2);
                    qPen[j].setCapStyle(Qt::FlatCap);
                    qPen[j].setColor(palette->qualColor[j]);
                  }
              }
            else
              for (j = 0; j < 3; j++)
                { qPen[j].setWidth(2);
                  qPen[j].setCapStyle(Qt::FlatCap);
                  qPen[j].setColor(palette->qualHue[j]);
                }
          }

        if (doProf)
          { int j;

            if (palette->profMode == 0)
              { for (j = 0; j < 5; j++)
                  { if (vbp >= 10.)
                      pPen[j].setWidth(2+2*j);
                    else
                      pPen[j].setWidth(2);
                    pPen[j].setCapStyle(Qt::FlatCap);
                    pPen[j].setColor(palette->profColor[j]);
                  }
                for (j = 5; j < 10; j++)
                  { if (vbp >= 10.)
                      pPen[j].setWidth(10);
                    else
                      pPen[j].setWidth(2);
                    pPen[j].setCapStyle(Qt::FlatCap);
                    pPen[j].setColor(palette->profColor[4]);
                  }
              }
            else
              for (j = 0; j < 3; j++)
                { if (vbp >= 10.)
                    pPen[j].setWidth(2);
                  else
                    pPen[j].setWidth(2+2*j);
                  pPen[j].setCapStyle(Qt::FlatCap);
                  pPen[j].setColor(palette->profHue[j]);
                }
          }

        if ( ! doPile && palette->matchqv)
          { int j;

            if (palette->matchMode == 0)
              { for (j = 0; j < 10; j++)
                  { mPen[j].setWidth(2);
                    mPen[j].setCapStyle(Qt::FlatCap);
                    mPen[j].setColor(palette->matchColor[j]);
                  }
              }
            else
              for (j = 0; j < 3; j++)
                { mPen[j].setWidth(2);
                  mPen[j].setCapStyle(Qt::FlatCap);
                  mPen[j].setColor(palette->matchHue[j]);
                }
          }
      }

      //  Draw Ruler

      { int y, scale;

        cPen.setColor(palette->readColor);
        painter.setPen(cPen);
        y = high - (rulerHeight-2);

        scale = rulerWidth/hbp;

        if (hend - hbeg > rulerT1)  //  lpile - fpile > 30
          { int n, x;

            painter.drawLine(0,y,wide,y);

            scale *= 2;
            n = ((hbeg/scale)+1)*scale;
            for (p = fpile; p < lpile; p++)
              if (pile[p].where >= n)
                { x = (pile[p].where-hbeg)*hbp;
                  cPen.setColor(palette->readColor);
                  painter.setPen(cPen);
                  painter.drawLine(x,y,x,y+4);
                  painter.setPen(cPen);
                  painter.drawText(x,high-2,tr("%1").arg(p+1));
                  n = pile[p].where + scale;
                }
          }
        else
          { int x1, x2, lp, z;

            z  = (readRow-vbeg)*vbp;
            lp = (pile[fpile].where - hbeg)*hbp - (labelWidth + 20);
            for (p = fpile; p < lpile; p++)
              { x1 = (pile[p].where - hbeg)*hbp;
                x2 = x1 + db1->reads[p].rlen*hbp;
                if (x1 - lp >= labelWidth+5)
                  { if (x1 >= 0)
                      lp = x1;
                    else
                      lp = 1;
                    cPen.setColor(palette->readColor);
                    painter.setPen(cPen);
                    painter.drawText(lp,high-2,tr("%1").arg(p+1));
                  }
                if (x1 < 0)
                  x1 = 0;
                else
                  { if (doGrid && hend-hbeg <= rulerT2)
                      { painter.setPen(rPen);
                        painter.drawLine(x1,y,x1,z);
                      }
                  }
                if (x2 > wide)
                  x2 = wide;
                cPen.setColor(palette->readColor);
                painter.setPen(cPen);
                painter.drawLine(x1,y,x2,y);
                painter.drawLine(x1,y,x1,y+4);
              }

            if (hend - hbeg <= rulerT2)  //  lpile - fpile <= 2
              { int log, step;
                int s, b1, f1, f2;

                scale *= 1.3;
                log    = 1000000000;
                while (log > 1)
                  { if (scale > log) break;
                    log /= 10;
                  }
                if (scale >= 5*log)
                  step = 10*log;
                else if (scale >= 2*log)
                  step = 5*log;
                else
                  step = 2*log;

                cPen.setColor(palette->readColor);
                for (p = fpile; p < lpile; p++)
                  { b1 = f1 = pile[p].where;
                    f2 = f1 + db1->reads[p].rlen;
                    if (f1 < ibeg)
                      f1 += step*((ibeg-f1)/step);
                    if (f2 > iend)
                      f2 -= step*((f2-iend)/step);
                    for (s = f1+step; s < f2; s += step)
                      { x1 = (s-hbeg)*hbp;
                        if (x1 > labelWidth+5)
                          { painter.setPen(cPen);
                            painter.drawLine(x1,y,x1,y+4);
                            painter.drawText(x1,high-2,tr("%1").arg(s-b1));
                            if (doGrid)
                              { painter.setPen(rPen);
                                painter.drawLine(x1,y,x1,z);
                              }
                          }
                      }
                  }
              }
          }
      }

      //  Draw Piles: plain segments, overlaps, and bridges

      { int cpi, cpa, npa;
        int k, x1;

        cpi  = fpile-1;
        npa  = pile[fpile].panels;
        for (cpa = fpanel; cpa < lpanel; cpa++)
          { if (cpa >= npa)
              { cpi += 1;
                npa  = pile[cpi+1].panels;
                x1 = pile[cpi].where;
                if (doRead)
                  { int x2, y;

                    y  = (readRow-vbeg)*vbp;
                    x2 = x1 + db1->reads[cpi].rlen;
                    x1 = (x1-hbeg)*hbp;
                    x2 = (x2-hbeg)*hbp;
                    if (x2 > wide)
                      x2 = wide;
                    cPen.setColor(palette->readColor);
                    painter.setPen(cPen);
                    if (x1 < 0)
                      painter.drawLine(0,y,x2,y);
                    else
                      painter.drawLine(x1,y,x2,y);
                  }
                else
                  x1 = (x1-hbeg)*hbp;
              }

            for (k = panel[cpa]; k < panel[cpa+1]; k++)
              { int   f, c;
                int   y, lev;
                int   xs, xf;
                int   b, cmp, bln;
                bool  halo;

                f = plist[k];

                // if ((align[f].etip & EMASK) != EVALU)
                // continue;

                b = align[f].btip;
                // if ((b & DRAW_FLAG) != 0)
                  // continue;

                if ((b & INIT_FLAG) == 0)
                  continue;
                  // { if ((b & LINK_FLAG) != 0)
                      // f = align[f].bread;
                    // lev = align[f].level;
                    // f = align[f].bread;
                  // }
                // else

                if ((b & LINK_FLAG) != 0)
                  { y = align[f].level;
                    if ((align[y].btip & LINK_FLAG) != 0)
                      y = align[y].bread;
                    lev = align[y].level;
                  }
                else
                  lev = align[f].level;

                y = ((lev+firstBRow) - vbeg)*vbp;
                if (y >= high - rulerHeight)
                  continue;

                b   = align[f].bread;
                cmp = (b & 0x1);
                b >>= 1;
                bln = db2->reads[b].rlen;

                if (doHalo)
                  { int cidx;

                    cidx = (db2->reads[b].flags & DB_QV);
                    if (b == haloed)
                      { hPen.setColor(palette->haloColor);
                        halo = 1;
                      }
                    else if (cidx > 0)
                      { hPen.setColor(colors[cidx]);
                        halo = 1;
                      }
                    else
                      halo = 0;
                  }
                else
                  halo = 0;

                xs = x1 + align[f].abpos*hbp;
                c  = (align[f].btip & DATA_MASK);
                if (c > 0)
                  { xf = xs - c*hbp;
                    cPen.setColor(palette->branchColor);
                    painter.setPen(cPen);
                    painter.drawLine(xf,y,xs,y);
                  }
  
                xf = x1 + align[f].aepos*hbp;
                if (doElim && (align[f].etip & ELIM_BIT) != 0)
                  { painter.setPen(ePen);
                    painter.drawLine(xs,y,xf,y);
                  }
                if (halo)
                  { painter.setPen(hPen);
                    painter.drawLine(xs,y,xf,y);
                  }
                if (doPile)
                  { cPen.setColor(palette->alignColor);
                    painter.setPen(cPen);
                    painter.drawLine(xs,y,xf,y);
                  }
                // align[f].btip |= DRAW_FLAG;

                c = f;
                while ((align[f].btip & LINK_FLAG) != 0)
                  { int     as, bs;
                    double  fact;
                    QColor *exp;
  
                    f = align[f].level;
                    xs = x1 + align[f].abpos*hbp;
                    as = align[f].abpos - align[c].aepos;
                    bs = align[f].bbpos - align[c].bepos;
                    if (as >= 0 && doBridge)
                      { cPen.setColor(palette->branchColor);
                        painter.setPen(cPen);
                        xf = x1 + align[c].aepos*hbp;
                        painter.drawLine(xf,y,xs,y);
                        if (bs > as)
                          { fact = stfact*(bs-as) / (as+10.);
                            if (fact > 20.) fact = 20.;
                            exp = stretch + (int) fact;
                          }
                        else
                          { fact = cmfact*(as-bs) / (as+10.);
                            if (fact > 20.) fact = 20.;
                            exp = compress + (int) fact;
                          }
                        dPen.setColor(*exp);
                        painter.setPen(dPen);
                        painter.drawLine(xf,y,xs,y);
                      }
                    else if (as < 0 && doOverlap)
                      { as = -as;
                        bs = -bs;
                        if (bs > as)
                          { fact  = stfact*(bs-as) / (as+10.);
                            if (fact > 20.) fact = 20.;
                            exp = stretch + (int) fact;
                          }
                        else
                          { fact = cmfact*(as-bs) / (as+10.);
                            if (fact > 20.) fact = 20.;
                            exp = compress + (int) fact;
                          }
                        if (align[f].aepos < align[c].aepos)
                          xf = x1 + align[f].aepos*hbp;
                        dPen.setColor(*exp);
                        painter.setPen(dPen);
                        painter.drawLine(xs,y+2,xf,y+2);
                      }
                    if (align[f].aepos > align[c].aepos)
                      c = f;
  
                    xf = x1 + align[f].aepos*hbp;
                    if (doElim && (align[f].etip & ELIM_BIT) != 0)
                      { painter.setPen(ePen);
                        painter.drawLine(xs,y,xf,y);
                      }
                    if (halo)
                      { painter.setPen(hPen);
                        painter.drawLine(xs,y,xf,y);
                      }
                    if (doPile)
                      { cPen.setColor(palette->alignColor);
                        painter.setPen(cPen);
                        painter.drawLine(xs,y,xf,y);
                      }
                    // align[f].btip |= DRAW_FLAG;
                  }
                f = (align[c].etip & DATA_ETIP);
                if (f > 0)
                  { xf = x1 + align[c].aepos*hbp;
                    xs = xf + f*hbp;
                    cPen.setColor(palette->branchColor);
                    painter.setPen(cPen);
                    painter.drawLine(xf,y,xs,y);
                  }
              }
          }
      }

/*
      //  Reset all DRAW_FLAGS

      { int k, f, b;

        for (k = panel[fpanel]; k < panel[lpanel]; k++)
          { f = plist[k];
            b = align[f].btip;
            if ((b & DRAW_FLAG) != 0)
              { if ((b & INIT_FLAG) == 0)
                  { if ((b & LINK_FLAG) != 0)
                      f = align[f].bread;
                    f = align[f].bread;
                  }
                while ((align[f].btip & LINK_FLAG) != 0)
                  { align[f].btip &= DRAW_OFF;
                    f = align[f].level;
                  }
                align[f].btip &= DRAW_OFF;
              }
          }
      }
*/


      //  Draw A-read:  quality value segments
      //     Requires quality value track;

      if ( ! doRead)
        { int64 *annoQV = (int64 *) model->qvs->anno;
          uint8 *dataQV = (uint8 *) model->qvs->data;
          int    tspace = model->tspace;
          int    lowT   = palette->qualGood;
          int    hghT   = palette->qualBad;
          int    y      = (readRow-vbeg)*vbp;
 
          for (p = fpile; p < lpile; p++)
            { int  x1, xs, xf;
              int  ab, ae, vl, aend;
              int  pt, o;

              pt   = annoQV[p];
              x1   = (pile[p].where-hbeg)*hbp;
              xs   = x1;
              y    = (readRow-vbeg)*vbp;
              aend = db1->reads[p].rlen;
              if (p+1 == lpile && pile[p].where + aend > iend)
                aend = iend;
              if (p == fpile && pile[p].where < ibeg)
                { o   = (ibeg - pile[p].where) / tspace;
                  pt += o;
                }
              else
                o = 0;
              if (palette->qualMode == 0)
                for (ab = o*tspace; ab < aend; ab += tspace)
                  { ae = ab+tspace;
                    if (ae > aend)
                      ae = aend;
                    xf = x1 + ae*hbp;
#ifdef HIFI
                    vl = dataQV[pt++];
#else
                    vl = dataQV[pt++]/5;
#endif
                    if (vl >= 10)
                      vl = 9;
                    painter.setPen(qPen[vl]);
                    painter.drawLine(xs,y,xf,y);
                    xs = xf;
                  }
              else
                for (ab = o*tspace; ab < aend; ab += tspace)
                  { ae = ab+tspace;
                    if (ae > aend)
                      ae = aend;
                    xf = x1 + ae*hbp;
                    vl = dataQV[pt++];
                    if (vl <= lowT)
                      painter.setPen(qPen[0]);
                    else if (vl >= hghT)
                      painter.setPen(qPen[2]);
                    else
                      painter.setPen(qPen[1]);
                    painter.drawLine(xs,y,xf,y);
                    xs = xf;
                  }
            }
        }


      //  Draw A-read repeat profile if track is present and on

      if (doProf)
        { int64 *annoQV = (int64 *) model->prf->anno;
          uint8 *dataQV = (uint8 *) model->prf->data;
          int    tspace = model->tspace;
          int    lowT   = palette->profLow;
          int    hghT   = palette->profHgh;
          int    y      = (readRow-vbeg)*vbp + 4;
          bool   tall   = (vbp >= 10.);

          for (p = fpile; p < lpile; p++)
            { int  x1, xs, xf;
              int  ab, ae, vl, aend;
              int  pt, o;

              pt   = annoQV[p];
              x1   = (pile[p].where-hbeg)*hbp;
              xs   = x1;
              aend = db1->reads[p].rlen;
              if (p+1 == lpile && pile[p].where + aend > iend)
                aend = iend;
              if (p == fpile && pile[p].where < ibeg)
                { o   = (ibeg - pile[p].where) / tspace;
                  pt += o;
                }
              else
                o = 0;
              if (palette->profMode == 0)
                for (ab = o*tspace; ab < aend; ab += tspace)
                  { ae = ab+tspace;
                    if (ae > aend)
                      ae = aend;
                    xf = x1 + ae*hbp;
                    vl = dataQV[pt++];
                    if (vl > 0)
                      { if (vl == 1)
                          { painter.setPen(pPen[0]);
                            painter.drawLine(xs,y,xf,y);
                          }
                        else
                          { vl = vl/10 + 1;
                            painter.setPen(pPen[vl]);
                            if (tall)
                              painter.drawLine(xs,y+vl,xf,y+vl);
                            else
                              painter.drawLine(xs,y,xf,y);
                          }
                      }
                    xs = xf;
                  }
              else
                for (ab = o*tspace; ab < aend; ab += tspace)
                  { ae = ab+tspace;
                    if (ae > aend)
                      ae = aend;
                    xf = x1 + ae*hbp;
                    vl = dataQV[pt++];
                    if (vl > 0)
                      { if (vl <= lowT)
                          { painter.setPen(pPen[0]);
                            painter.drawLine(xs,y,xf,y);
                          }
                        else if (vl >= hghT)
                          { painter.setPen(pPen[2]);
                            if (tall)
                              painter.drawLine(xs,y-2,xf,y+2);
                            else
                              painter.drawLine(xs,y,xf,y);
                          }
                        else
                          { painter.setPen(pPen[1]);
                            if (tall)
                              painter.drawLine(xs,y-1,xf,y+1);
                            else
                              painter.drawLine(xs,y,xf,y);
                          }
                      }
                    xs = xf;
                  }
            }
        }

      //  Draw A-reads:  Masks

      if (palette->nmasks > 0)
        { int j, p, x1;

          for (j = 0; j < palette->nmasks; j++)
            if (palette->showTrack[j])
              { int64 *anno = (int64 *) palette->track[j]->anno;
                int   *data = (int *) palette->track[j]->data;
                int64  d;
                int    ae, mb;
                int    y, xs, xf;
 
	        cPen.setColor(palette->trackColor[j]);
                painter.setPen(cPen);
                y = ((readRow - palette->Arail[j]) - vbeg)*vbp;
 
                p  = fpile+1;
                x1 = (pile[fpile].where-hbeg)*hbp;
                ae = anno[p];
                d  = find_mask(ibeg-pile[fpile].where,data,anno[fpile],ae);
                while (d < ae)
                  { mb = data[d++];
                    if (mb >= iend)
                      break;
                    xs = x1 + mb*hbp;
                    xf = x1 + data[d++]*hbp;
                    painter.drawLine(xs,y,xf,y);
                  }

                while (p < lpile)
                  { x1 = (pile[p++].where-hbeg)*hbp;
                    ae = anno[p];
                    while (d < ae)
                      { mb = data[d++];
                        if (mb >= iend)
                          break;
                        xs = x1 + mb*hbp;
                        xf = x1 + data[d++]*hbp;
                        painter.drawLine(xs,y,xf,y);
                      }
                  }
              }
        }

      //  Draw B-reads:  quality value segments, match value segments, overlaid masks
      //     All require loading trace points

      if ( ! doPile || bAnno > 0)
        { FILE  *inp    = model->input;
          uint8 *trace  = (uint8 *) model->tbuf;
          int    tspace = model->tspace;
          int    iosize = sizeof(Overlap) - sizeof(void *);
          int    lowT   = palette->matchGood;
          int    hghT   = palette->matchBad;
          int    lowQ   = palette->qualGood;
          int    hghQ   = palette->qualBad;

          int    cpi, cpa, npa, thr, ehr;
          int    pbeg, pend, pthr, ptck;
          int    x1, k, alen;
          bool   doDiff, doQV;
          int64 *annoQV;
          uint8 *dataQV;

          doDiff = ( ! doPile && palette->matchqv);
          doQV   = ( ! doPile && ! palette->matchqv);
          if (doQV)
            { annoQV = (int64 *) model->qvs->anno;
              dataQV = (uint8 *) model->qvs->data;
            }

          cpi  = fpile-1;
          npa  = pile[fpile].panels;
          for (cpa = fpanel; cpa < lpanel; cpa++)
            { if (cpa >= npa)
                { cpi += 1;
                  npa  = pile[cpi+1].panels;
                  alen = db1->reads[cpi].rlen;
                  x1   = pile[cpi].where;
                  pthr = ibeg-x1;
                  ptck = pthr/tspace;
                  pbeg = ptck*tspace;
                  pthr = pthr-tspace;
                  pend = (((iend-x1)-1)/tspace+1)*tspace;
                  x1   = (x1-hbeg)*hbp;
                  thr  = (fpanel - pile[cpi].panels)*PANEL_SIZE;
                  ehr = 0;
                }
              else
                { thr += PANEL_SIZE;
                  ehr = thr;
                }
              for (k = panel[cpa]; k < panel[cpa+1]; k++)
                { int   f, t, y, lev, tlen;
                  LA   *aln;
                  int   b, cmp, bln;

                  f = plist[k];
                  if ((align[f].etip & EMASK) != EVALU)
                    continue;

                  aln = align+f;
                  if (aln->abpos < ehr || aln->abpos >= pend || aln->aepos <= pbeg)
                    continue;

                  b = aln->btip;
                  if ((b & INIT_FLAG) == 0)
                    { if ((b & LINK_FLAG) == 0)
                        { y = f;
                          lev = aln->level;
                        }
                      else
                        { y = aln->bread;
                          lev = align[y].level;
                        }
                      b = align[align[y].bread].bread;
                    }
                  else
                    { if ((b & LINK_FLAG) == 0)
                        lev = aln->level;
                      else
                        { y = aln->level;
                          if ((align[y].btip & LINK_FLAG) != 0)
                            y = align[y].bread;
                          lev = align[y].level;
                        }
                      b = aln->bread;
                    }

                  y = ((lev+firstBRow) - vbeg)*vbp;
                  if (y >= high - rulerHeight)
                    continue;

                  cmp = (b & 0x1);
                  b >>= 1;
                  bln = db2->reads[b].rlen;

                  tlen = 2*((aln->aepos-1)/tspace - aln->abpos/tspace);
                  fseek(inp,aln->toff,SEEK_SET);
                  fread(trace,tlen+2,1,inp);

                  //  Draw match diffs on each B-read

                  if (doDiff)
                    { int   abeg, aend;
                      int   ab, ae;
                      int   pt, vl;
                      int   xs, xf;

                      aend = align[f].aepos;
                      abeg = align[f].abpos;

                      if (pthr > abeg)
                        { pt = 2*(ptck - abeg/tspace);  
                          ab = pbeg;
                        }
                      else
                        { pt = 0;
                          ab = abeg;
                        }
                     if (pend < aend)
                       aend = pend;

                      ae = (ab/tspace)*tspace;
                      xs = x1 + ab*hbp;
                      if (palette->matchMode == 0)
                        { if (ae < ab)
                            { ae = ae+tspace;
                              xf = x1 + ae*hbp;
#ifdef HIFI
                              vl = (trace[pt]*100.)/(ae-ab);
#else
                              vl = (trace[pt]*20.)/(ae-ab);
#endif
                              if (vl >= 10)
                                vl = 9;
			      // if (ae-ab >= 20)
                                // vl = (trace[pt]*40.)/((ae-ab) + trace[pt+1]);
                              // else
                                // vl = 19;
                              pt += 2;
                              painter.setPen(mPen[vl]);
                              painter.drawLine(xs,y,xf,y);
                              ab = ae;
                              xs = xf;
                            }
                          while (ab < aend)
                            { ae = ae + tspace;
                              if (ae > aend)
                                { xf = x1 + aend*hbp;
#ifdef HIFI
                                  vl = (trace[pt]*100.)/(aend-ab);
#else
                                  vl = (trace[pt]*20.)/(aend-ab);
#endif
                                  if (vl >= 10)
                                    vl = 9;
                                  // if (aend-ab >= 20)
                                    // vl = (trace[pt]*40.)/((aend-ab) + trace[pt+1]);
                                  // else
                                    // vl = 19;
                                  pt += 2;
                                  painter.setPen(mPen[vl]);
                                  painter.drawLine(xs,y,xf,y);
                                  break;
                                }
                              xf = x1 + ae*hbp;
#ifdef HIFI
                              vl = trace[pt];
#else
                              vl = trace[pt]/5;
#endif
                              if (vl >= 10)
                                vl = 9;
                              pt += 2;
                              painter.setPen(mPen[vl]);
                              painter.drawLine(xs,y,xf,y);
                              ab = ae;
                              xs = xf;
                            }
                        }
                      else
                        { if (ae < ab)
                            { ae = ae+tspace;
                              xf = x1 + ae*hbp;
                              vl = (trace[pt]*100.)/(ae-ab);
                              // if (ae-ab >= 20)
                                // vl = (trace[pt]*200.)/((ae-ab) + trace[pt+1]);
                              // else
                                // vl = 100;
                              pt += 2;
                              if (vl <= lowT)
                                painter.setPen(mPen[0]);
                              else if (vl >= hghT)
                                painter.setPen(mPen[2]);
                              else
                                painter.setPen(mPen[1]);
                              painter.drawLine(xs,y,xf,y);
                              ab = ae;
                              xs = xf;
                            }
                          while (ab < aend)
                            { ae = ae + tspace;
                              if (ae > aend)
                                { xf = x1 + aend*hbp;
                                  vl = (trace[pt]*100.)/(aend-ab);
                                  // if (aend-ab >= 20)
                                    // vl = (trace[pt]*200.)/((aend-ab) + trace[pt+1]);
                                  // else
                                    // vl = 100;
                                  pt += 2;
                                  if (vl <= lowT)
                                    painter.setPen(mPen[0]);
                                  else if (vl >= hghT)
                                    painter.setPen(mPen[2]);
                                  else
                                    painter.setPen(mPen[1]);
                                  painter.drawLine(xs,y,xf,y);
                                  break;
                                }
                              xf = x1 + ae*hbp;
                              vl = trace[pt];
                              pt += 2;
                              if (vl <= lowT)
                                painter.setPen(mPen[0]);
                              else if (vl >= hghT)
                                painter.setPen(mPen[2]);
                              else
                                painter.setPen(mPen[1]);
                              painter.drawLine(xs,y,xf,y);
                              ab = ae;
                              xs = xf;
                            }
                        }
                      pt += iosize;
                    }

                  //  Draw QVs on B reads

                  if (doQV)
                    { int   bb, be, ae;
                      int   pt, vl;
                      int   xs, xf;
 
                      mapToA(cmp,aln,trace,tspace,tlen,cmp?pend:pbeg);
                      if (cmp)
                        { xs = x1 + aln->aepos*hbp;
                          bb = (bln-aln->bepos)/tspace;
                          pt = annoQV[b] + bb;
                          if (palette->qualMode == 0)
                            for (bb = bln - bb*tspace; bb > aln->bbpos; bb -= tspace)
                              { be = bb - tspace;
                                if (be < aln->bbpos)
                                  be = aln->bbpos;
                                ae = mapToA(-1,aln,trace,tspace,be,0);
                                xf = x1 + ae*hbp;
#ifdef HIFI
                                vl = dataQV[pt++];
#else
                                vl = dataQV[pt++]/5;
#endif
                                if (vl >= 10)
                                  vl = 9;
                                painter.setPen(qPen[vl]);
                                painter.drawLine(xs,y,xf,y);
                                xs = xf;
                                if (ae <= pbeg)
                                  break;
                              }
                          else
                            for (bb = bln - bb*tspace; bb > aln->bbpos; bb -= tspace)
                              { be = bb - tspace;
                                if (be < aln->bbpos)
                                  be = aln->bbpos;
                                ae = mapToA(-1,aln,trace,tspace,be,0);
                                xf = x1 + ae*hbp;
                                vl = dataQV[pt++];
                                if (vl <= lowQ)
                                  painter.setPen(qPen[0]);
                                else if (vl >= hghQ)
                                  painter.setPen(qPen[2]);
                                else
                                  painter.setPen(qPen[1]);
                                painter.drawLine(xs,y,xf,y);
                                xs = xf;
                                if (ae <= pbeg)
                                  break;
                              }
                        }
                      else
                        { xs = x1 + aln->abpos*hbp;
                          bb = aln->bbpos/tspace;
                          pt = annoQV[b] + bb;
                          if (palette->qualMode == 0)
                            for (bb = bb*tspace; bb < aln->bepos; bb += tspace)
                              { be = bb + tspace;
                                if (be > aln->bepos)
                                  be = aln->bepos;
                                ae = mapToA(-1,aln,trace,tspace,be,0);
                                xf = x1 + ae*hbp;
#ifdef HIFI
                                vl = dataQV[pt++];
#else
                                vl = dataQV[pt++]/5;
#endif
                                if (vl >= 10)
                                  vl = 9;
                                painter.setPen(qPen[vl]);
                                painter.drawLine(xs,y,xf,y);
                                xs = xf;
                                if (ae >= pend)
                                  break;
                              }
                          else
                            for (bb = bb*tspace; bb < aln->bepos; bb += tspace)
                              { be = bb + tspace;
                                if (be > aln->bepos)
                                  be = aln->bepos;
                                ae = mapToA(-1,aln,trace,tspace,be,0);
                                xf = x1 + ae*hbp;
                                vl = dataQV[pt++];
                                if (vl <= lowQ)
                                  painter.setPen(qPen[0]);
                                else if (vl >= hghQ)
                                  painter.setPen(qPen[2]);
                                else
                                  painter.setPen(qPen[1]);
                                painter.drawLine(xs,y,xf,y);
                                xs = xf;
                                if (ae >= pend)
                                  break;
                              }
                        }
                    }

                  //  Draw masks on B reads

                  for (t = 0; t < bAnno; t++)
                    { int64 *anno = (int64 *) track[t]->anno;
                      int   *data = (int *) track[t]->data;
                      int64  d;
                      int    bb, be;
                      int    xs, xf;

		      cPen.setColor(palette->trackColor[tIndex[t]]);
                      painter.setPen(cPen);
 
                      mapToA(cmp,aln,trace,tspace,tlen,cmp?pend:pbeg);
                      for (d = anno[b]; d < anno[b+1]; d += 2)
                        if (cmp)
                          { bb = bln - data[d];
                            be = bln - data[d+1];
                            if (be >= aln->bepos)
                              continue;
                            if (bb <= aln->bbpos)
                              break;
                            if (bb > aln->bepos)
                              bb = aln->bepos;
                            if (be < aln->bbpos)
                              be = aln->bbpos;
                            bb = mapToA(-1,aln,trace,tspace,bb,0);
                            be = mapToA(-1,aln,trace,tspace,be,0);
                            if (be >= pend)
                              continue;
                            if (bb <= pbeg)
                              break;
                            if (bb > pend)
                              bb = pend;
                            if (be < pbeg)
                              be = pbeg;
                            xs = x1 + bb*hbp;
                            xf = x1 + be*hbp;
                            painter.drawLine(xs,y,xf,y);
                            if (be <= pbeg)
                              break;
                          }
                        else
                          { bb = data[d];
                            be = data[d+1];
                            if (be <= aln->bbpos)
                              continue;
                            if (bb >= aln->bepos)
                              break;
                            if (bb < aln->bbpos)
                              bb = aln->bbpos;
                            if (be > aln->bepos)
                              be = aln->bepos;
                            bb = mapToA(-1,aln,trace,tspace,bb,0);
                            be = mapToA(-1,aln,trace,tspace,be,0);
                            if (be <= pbeg)
                              continue;
                            if (bb >= pend)
                              break;
                            if (bb < pbeg)
                              bb = pbeg;
                            if (be > pend)
                              be = pend;
                            xs = x1 + bb*hbp;
                            xf = x1 + be*hbp;
                            painter.drawLine(xs,y,xf,y);
                            if (be >= pend)
                              break;
                          }
                    }
                }
            }
        }

    }
}


/*****************************************************************************\
*
*  MY SCROLL
*
\*****************************************************************************/

MyScroll::MyScroll(QWidget *parent) : QWidget(parent)
{
  mycanvas = new MyCanvas(this);

  vertScroll = new QScrollBar(Qt::Vertical,this);
    vertScroll->setTracking(false);
    vertScroll->setMinimum(0);
    vertScroll->setMaximum(1);
    vertScroll->setPageStep(POS_TICKS-1);
    vertScroll->setValue(0);

  vertZoom = new QSlider(Qt::Vertical,this);
    vertZoom->setTracking(false);
    vertZoom->setMinimum(0);
    vertZoom->setMaximum(SIZE_TICKS);
    vertZoom->setValue(SIZE_TICKS);

    QHBoxLayout *vertControl = new QHBoxLayout();
      vertControl->addWidget(vertScroll);
      vertControl->addWidget(vertZoom);
      vertControl->setSpacing(2);
      vertControl->setMargin(5);

  horzScroll = new QScrollBar(Qt::Horizontal,this);
    horzScroll->setTracking(false);
    horzScroll->setMinimum(0);
    horzScroll->setMaximum(1);
    horzScroll->setPageStep(POS_TICKS-1);
    horzScroll->setValue(0);

  horzZoom = new QSlider(Qt::Horizontal,this);
    horzZoom->setTracking(false);
    horzZoom->setMinimum(0);
    horzZoom->setMaximum(SIZE_TICKS);
    horzZoom->setValue(SIZE_TICKS);

    QVBoxLayout *horzControl = new QVBoxLayout();
      horzControl->addWidget(horzScroll);
      horzControl->addWidget(horzZoom);
      horzControl->setSpacing(2);
      horzControl->setMargin(5);

  QFrame *corner = new QFrame(this,Qt::Widget);
    corner->setFrameStyle(QFrame::Panel | QFrame::Raised);
    corner->setLineWidth(4);

    QGridLayout *drawPanel = new QGridLayout();
      drawPanel->addWidget(mycanvas,   0,0);
      drawPanel->addLayout(vertControl,0,1);
      drawPanel->addLayout(horzControl,1,0);
      drawPanel->addWidget(corner,     1,1);
      drawPanel->setColumnStretch(0,1);
      drawPanel->setColumnStretch(1,0);
      drawPanel->setRowStretch(0,1);
      drawPanel->setRowStretch(1,0);
      drawPanel->setSpacing(0);
      drawPanel->setMargin(0);

  setLayout(drawPanel);

  setRange(256,256);

  connect(vertScroll,SIGNAL(valueChanged(int)),this,SLOT(vsValue(int)));
  connect(vertZoom,SIGNAL(valueChanged(int)),this,SLOT(vsSize(int)));

  connect(horzScroll,SIGNAL(valueChanged(int)),this,SLOT(hsValue(int)));
  connect(horzZoom,SIGNAL(valueChanged(int)),this,SLOT(hsSize(int)));
}

void MyScroll::getState(Scroll_State &state)
{ state.hzPos  = horzZoom->value();
  state.hsPage = horzScroll->pageStep();
  state.hsMax  = horzScroll->maximum();
  state.hsPos  = horzScroll->value();
  state.hmax   = hmax;
  state.vzPos  = vertZoom->value();
  state.vsPage = vertScroll->pageStep();
  state.vsMax  = vertScroll->maximum();
  state.vsPos  = vertScroll->value();
  state.vmax   = vmax;
}

void MyScroll::putState(Scroll_State &state)
{ setRange(state.hmax,state.vmax);
  horzZoom->setValue(state.hzPos);
  horzScroll->setPageStep(state.hsPage);
  horzScroll->setMaximum(state.hsMax);
  horzScroll->setValue(state.hsPos);
  vertZoom->setValue(state.vzPos);
  vertScroll->setPageStep(state.vsPage);
  vertScroll->setMaximum(state.vsMax);
  vertScroll->setValue(state.vsPos);
}

void MyScroll::haloUpdate(bool ison)
{ mycanvas->haloUpdate(ison); }

void MyScroll::setModel(DataModel *model)
{ mycanvas->setModel(model); }

void MyScroll::setRange(int h, int v)
{ int    i;
  double base;
  double fact;

  vmax = v;
  hmax = h;

  base = pow(hmax/MIN_HORZ_VIEW,1./SIZE_TICKS);
  for (i = 0; i < SIZE_TICKS; i++)
    { fact = (pow(base,i)*MIN_HORZ_VIEW)/hmax;
      hstep[i] = POS_TICKS*(fact/(2.-fact));
    }
  hstep[SIZE_TICKS] = POS_TICKS-1;

  base = pow(vmax/MIN_VERT_VIEW,1./SIZE_TICKS);
  for (i = 0; i < SIZE_TICKS; i++)
    { fact = (pow(base,i)*MIN_VERT_VIEW)/vmax;
      vstep[i] = POS_TICKS*(fact/(2.-fact));
    }
  vstep[SIZE_TICKS] = POS_TICKS-1;

  MyCanvas::rulerHeight = -1;
}

void MyScroll::vsValue(int value)
{ (void) value;
  mycanvas->update();
}

void MyScroll::vsSize(int size)
{ int page;

  page = vstep[size];
  vertScroll->setMaximum(POS_TICKS-page);
  vertScroll->setPageStep(page);
  mycanvas->update();
}

void MyScroll::hsValue(int value)
{ (void) value;
  mycanvas->update();
}

void MyScroll::hsSize(int size)
{ int page;

  page = hstep[size];
  horzScroll->setMaximum(POS_TICKS-page);
  horzScroll->setPageStep(page);
  mycanvas->update();
}

void MyScroll::hsToRange(int beg, int end)
{ int i, rng, page, hpos;

  rng = end-beg;
  rng = (1.*rng*REAL_TICKS) / (2.*hmax-rng);
  for (i = 0; i < SIZE_TICKS; i++)
    if (hstep[i] > rng)
      break;
  page = rng; // hstep[i];
  hpos = (beg * (REAL_TICKS+page)) / hmax;

  horzZoom->setValue(i);
  horzScroll->setMaximum(POS_TICKS-page);
  horzScroll->setPageStep(page);
  horzScroll->setValue(hpos);
  mycanvas->update();
}


/*****************************************************************************\
*
*  DRAG AND DROP TRACK WIDGET
*
\*****************************************************************************/

TrackWidget::TrackWidget(QWidget *parent) : QWidget(parent) { setAcceptDrops(true); }

void TrackWidget::mousePressEvent(QMouseEvent *ev)
{
  QWidget *child = static_cast<QWidget *>(childAt(ev->pos()));

  if (child == NULL)
    return;
  if (children().contains(child))
    return;

  QWidget *list = static_cast<QWidget *>(child->parent());
  if (child != list->children().at(1))
    return;

  QPixmap line(100,5);
    QPainter pnt(&line);
    QPen     pen(Qt::darkBlue,5);
    pnt.setPen(pen);
    pnt.drawLine(0,2,100,2);

  QDrag *drag = new QDrag(this);

  QMimeData *mimeData = new QMimeData;
    drag->setMimeData(mimeData);

  drag->setPixmap(line);  // list->grab()

  QPoint hotSpot = ev->pos() - list->pos();
    hotSpot.setY(hotSpot.y()-list->height()/2);
    drag->setHotSpot(hotSpot);

  bool wasOn = list->isEnabled();
  list->setEnabled(false);

  if (drag->exec() == Qt::IgnoreAction)
    { list->setEnabled(wasOn);
      return;
   }

  { int w, f, t;
    QVBoxLayout *lman = static_cast<QVBoxLayout *>(layout());

    w = lman->itemAt(1)->widget()->pos().y();
    f = list->pos().y()/w;
    t = (dropY - hotSpot.y() + w/2) / w;

    if (t < f || t > f+1)
      { lman->removeWidget(list);
        if (t < f)
          lman->insertWidget(t,list);
        else
          lman->insertWidget(t-1,list);
      }
  }

  list->setEnabled(wasOn);
}

void TrackWidget::dragEnterEvent(QDragEnterEvent *ev)
{ ev->setDropAction(Qt::MoveAction);
  ev->accept();
}

void TrackWidget::dragMoveEvent(QDragMoveEvent *ev)
{ ev->setDropAction(Qt::MoveAction);
  ev->accept();
}

void TrackWidget::dropEvent(QDropEvent *ev)
{ dropY = ev->pos().y();
  ev->accept();
}


/*****************************************************************************\
*
*  PALETTE DIALOG
*
\*****************************************************************************/

#define COLOR_CHANGE(routine,color,box) 		\
							\
void PaletteDialog::routine()				\
{ QColor newColor = QColorDialog::getColor(color,this);	\
  box->setDown(false);					\
  if ( ! newColor.isValid()) return;			\
							\
  color = newColor;					\
  QPixmap blob = QPixmap(16,16);			\
    blob.fill(color);					\
  box->setIcon(QIcon(blob));				\
}

COLOR_CHANGE(backChange,backColor,backBox)
COLOR_CHANGE(readChange,readColor,readBox)
COLOR_CHANGE(alignChange,alignColor,alignBox)
COLOR_CHANGE(branchChange,branchColor,branchBox)
COLOR_CHANGE(gridChange,gridColor,gridBox)
COLOR_CHANGE(haloChange,haloColor,haloBox)
COLOR_CHANGE(elimChange,elimColor,elimBox)
COLOR_CHANGE(stretchChange,stretchColor,stretchBox)
COLOR_CHANGE(neutralChange,neutralColor,neutralBox)
COLOR_CHANGE(compressChange,compressColor,compressBox)

void PaletteDialog::matchRampChange()
{ int j;
  for (j = 0; j < 10; j++)
    if (matchBox[j]->isDown())
      break;
  QColor newColor = QColorDialog::getColor(matchColor[j],this);
  matchBox[j]->setDown(false);
  if ( ! newColor.isValid()) return;

  matchColor[j] = newColor;
  QPixmap blob = QPixmap(16,16);
    blob.fill(newColor);
  matchBox[j]->setIcon(QIcon(blob));
}

void PaletteDialog::matchTriChange()
{ int j;
  for (j = 0; j < 3; j++)
    if (matchLev[j]->isDown())
      break;
  QColor newColor = QColorDialog::getColor(matchHue[j],this);
  matchLev[j]->setDown(false);
  if ( ! newColor.isValid()) return;

  matchHue[j] = newColor;
  QPixmap blob = QPixmap(16,16);
    blob.fill(newColor);
  matchLev[j]->setIcon(QIcon(blob));
}

void PaletteDialog::qualRampChange()
{ int j;
  for (j = 0; j < 10; j++)
    if (qualBox[j]->isDown())
      break;
  QColor newColor = QColorDialog::getColor(qualColor[j],this);
  qualBox[j]->setDown(false);
  if ( ! newColor.isValid()) return;

  qualColor[j] = newColor;
  QPixmap blob = QPixmap(16,16);
    blob.fill(newColor);
  qualBox[j]->setIcon(QIcon(blob));
}

void PaletteDialog::qualTriChange()
{ int j;
  for (j = 0; j < 3; j++)
    if (qualLev[j]->isDown())
      break;
  QColor newColor = QColorDialog::getColor(qualHue[j],this);
  qualLev[j]->setDown(false);
  if ( ! newColor.isValid()) return;

  qualHue[j] = newColor;
  QPixmap blob = QPixmap(16,16);
    blob.fill(newColor);
  qualLev[j]->setIcon(QIcon(blob));
}

void PaletteDialog::profRampChange()
{ int j;
  for (j = 0; j < 5; j++)
    if (profBox[j]->isDown())
      break;
  QColor newColor = QColorDialog::getColor(profColor[j],this);
  profBox[j]->setDown(false);
  if ( ! newColor.isValid()) return;

  profColor[j] = newColor;
  QPixmap blob = QPixmap(36,16);
    blob.fill(newColor);
  profBox[j]->setIcon(QIcon(blob));
}

void PaletteDialog::profTriChange()
{ int j;
  for (j = 0; j < 3; j++)
    if (profLev[j]->isDown())
      break;
  QColor newColor = QColorDialog::getColor(profHue[j],this);
  profLev[j]->setDown(false);
  if ( ! newColor.isValid()) return;

  profHue[j] = newColor;
  QPixmap blob = QPixmap(16,16);
    blob.fill(newColor);
  profLev[j]->setIcon(QIcon(blob));
}

void PaletteDialog::trackChange()
{ int j;

  for (j = 0; j < nmasks; j++)
    if (trackBox[j]->isDown())
      break;
  QColor newColor = QColorDialog::getColor(trackColor[j],this);
  trackBox[j]->setDown(false);
  if ( ! newColor.isValid()) return;

  trackColor[j] = newColor;
  QPixmap blob = QPixmap(16,16);
    blob.fill(newColor);
  trackBox[j]->setIcon(QIcon(blob));
}

void PaletteDialog::activateGrid(int state)
{ bool on;

  on = (state == Qt::Checked);
  gridLabel->setEnabled(on);
  gridBox->setEnabled(on);
}

void PaletteDialog::activateHalo(int state)
{ bool on;

  on = (state == Qt::Checked);
  haloLabel->setEnabled(on);
  haloBox->setEnabled(on);
}

void PaletteDialog::activateElim(int state)
{ bool on;

  on = (state == Qt::Checked);
  elimLabel->setEnabled(on);
  elimBox->setEnabled(on);
}

void PaletteDialog::activateMatchQV(int state)
{ bool on;
  int  j;

  (void) state;

  on = matchCheck->isChecked();

  matchLabel->setEnabled(on);
  matchLabelScale->setEnabled(on);
  matchRadioScale->setEnabled(on);
  matchLabelTri->setEnabled(on);
  matchRadioTri->setEnabled(on);
  for (j = 0; j < 10; j++)
    matchBox[j]->setEnabled(on);
  for (j = 0; j < 3; j++)
    { matchLev[j]->setEnabled(on);
      matchLevLabel[j]->setEnabled(on);
    }
  if (on)
    { matchBot->setText(tr("%1").arg(matchGood));
      matchTop->setText(tr("%1").arg(matchBad));
      qualonB->setChecked(false);
    }
  else
    { matchBot->setText(tr(""));
      matchTop->setText(tr(""));
      matchBot->clearFocus();
      matchTop->clearFocus();
    }
}

void PaletteDialog::activateQualQV(int state)
{ bool on;
  int  j;

  (void) state;

  on = qualCheck->isChecked();

  qualLabel->setEnabled(on);
  qualonB->setEnabled(on);
  qualLabelScale->setEnabled(on);
  qualRadioScale->setEnabled(on);
  qualLabelTri->setEnabled(on);
  qualRadioTri->setEnabled(on);
  for (j = 0; j < 10; j++)
    qualBox[j]->setEnabled(on);
  for (j = 0; j < 3; j++)
    { qualLev[j]->setEnabled(on);
      qualLevLabel[j]->setEnabled(on);
    }
  if (on)
    { qualBot->setText(tr("%1").arg(qualGood));
      qualTop->setText(tr("%1").arg(qualBad));
    }
  else
    { qualBot->setText(tr(""));
      qualTop->setText(tr(""));
      qualBot->clearFocus();
      qualTop->clearFocus();
    }
}

void PaletteDialog::activateRepProfile(int state)
{ bool on;
  int  j;

  (void) state;

  on = profCheck->isChecked();

  profLabel->setEnabled(on);
  profLabelScale->setEnabled(on);
  profRadioScale->setEnabled(on);
  profLabelTri->setEnabled(on);
  profRadioTri->setEnabled(on);
  for (j = 0; j < 5; j++)
    profBox[j]->setEnabled(on);
  for (j = 0; j < 3; j++)
    { profLev[j]->setEnabled(on);
      profLevLabel[j]->setEnabled(on);
    }
  if (on)
    { profBot->setText(tr("%1").arg(profLow));
      profTop->setText(tr("%1").arg(profHgh));
    }
  else
    { profBot->setText(tr(""));
      profTop->setText(tr(""));
      profBot->clearFocus();
      profTop->clearFocus();
    }
}

void PaletteDialog::enforceMatchOff(int state)
{ bool on;

  (void) state;

  on = qualonB->isChecked();
  if (on)
    matchCheck->setChecked(false);
}

void PaletteDialog::activateElastic(int state)
{ bool on, oon, bon;

  (void) state;

  oon = overlapCheck->isChecked();
  bon = bridgeCheck->isChecked();
  on  = (oon || bon);

  overLabel->setEnabled(oon);
  bridgeLabel->setEnabled(bon);

  leftLabel->setEnabled(on);
  leftDash->setEnabled(on);
  neutralLabel->setEnabled(on);
  rightLabel->setEnabled(on);
  rightDash->setEnabled(on);
  stretchBox->setEnabled(on);
  compressBox->setEnabled(on);
  neutralBox->setEnabled(on);
  maxStretch->setEnabled(on);
  maxCompress->setEnabled(on);
  if (on)
    { maxStretch->setText(tr("%1").arg(stretchMax));
      maxCompress->setText(tr("%1").arg(compressMax));
      drawLeftDash(QColor(0,0,0));
      drawRightDash(QColor(0,0,0));
    }
  else
    { maxStretch->setText(tr(""));
      maxCompress->setText(tr(""));
      drawLeftDash(QColor(196,196,196));
      drawRightDash(QColor(196,196,196));
    }
}

void PaletteDialog::activateTracks(int state)
{ bool on;
  int  j;

  (void) state;

  for (j = 0; j < nmasks; j++)
    { on = trackCheck[j]->isChecked();
      trackLabel[j]->setEnabled(on);
      trackBox[j]->setEnabled(on);
      trackApos[j]->setEnabled(on);
      trackonB[j]->setEnabled(on);
    }
}

int PaletteDialog::liveCount()
{ int j, v, cnt;

  cnt = 0;
  for (j = 0; j < nmasks; j++)
    if (trackCheck[j]->isChecked())
      { v = trackApos[j]->value();
        if (v > cnt)
          cnt = v;
      }
  return (cnt);
}

int PaletteDialog::getView()
{ int j;

  MainWindow::views.clear();
  for (j = 2; j < viewList->count(); j++)
    MainWindow::views.append(viewList->itemText(j));
  MainWindow::cview = viewList->currentIndex();
  return (MainWindow::cview);
}

void PaletteDialog::setView()
{ viewList->clear();
  viewList->addItem(tr("Preferences"));
  viewList->insertSeparator(1);
  viewList->addItems(MainWindow::views);
  viewList->setCurrentIndex(MainWindow::cview);
}

void PaletteDialog::getState(Palette_State &state)
{ static QColor black = QColor(0,0,0);
  static QColor white = QColor(255,255,255);

  QHash<void *,int> hash;
  int order[MAX_TRACKS];
  int j, k;

  state.backColor     = backColor;
  state.readColor     = readColor;
  state.alignColor    = alignColor;
  state.branchColor   = branchColor;

  state.gridColor     = gridColor;
  state.haloColor     = haloColor;
  state.elimColor     = elimColor;
  state.showGrid      = gridCheck->isChecked();
  state.showHalo      = haloCheck->isChecked();
  state.showElim      = elimCheck->isChecked();
  if (elimColor == black)
    state.drawElim = -1;
  else if (elimColor == white)
    state.drawElim = 1;
  else
    state.drawElim = 0;

  state.bridges       = bridgeCheck->isChecked();
  state.overlaps      = overlapCheck->isChecked();
  state.stretchColor  = stretchColor;
  state.neutralColor  = neutralColor;
  state.compressColor = compressColor;
  state.stretchMax    = stretchMax;
  state.compressMax   = compressMax;

  state.matchqv       = matchCheck->isChecked();
  state.matchMode     = matchRadioTri->isChecked();
  for (j = 0; j < 10; j++)
    state.matchColor[j] = matchColor[j];
  for (j = 0; j < 3; j++)
    state.matchHue[j] = matchHue[j];
  state.matchGood     = matchGood;
  state.matchBad      = matchBad;

  state.qualVis       = qualVis;
  state.qualqv        = qualCheck->isChecked();
  state.qualMode      = qualRadioTri->isChecked();
  for (j = 0; j < 10; j++)
    state.qualColor[j] = qualColor[j];
  for (j = 0; j < 3; j++)
    state.qualHue[j] = qualHue[j];
  state.qualGood      = qualGood;
  state.qualBad       = qualBad;
  state.qualonB       = qualonB->isChecked();

  state.profVis       = profVis;
  state.profqv        = profCheck->isChecked();
  state.profMode      = profRadioTri->isChecked();
  for (j = 0; j < 5; j++)
    state.profColor[j] = profColor[j];
  for (j = 0; j < 3; j++)
    state.profHue[j] = profHue[j];
  state.profLow       = profLow;
  state.profHgh       = profHgh;

  //  Determine order of masks in layout

  QVBoxLayout *lman = static_cast<QVBoxLayout *>(maskPanel->layout());
  QList<QObject *> ord(maskPanel->children());

  for (j = 0; j < nmasks; j++)
    hash[ord.at(j+1)] = j;
  for (j = 0; j < nmasks; j++)
    order[j] = hash[lman->itemAt(j)->widget()];

  state.nmasks = nmasks;
  for (j = 0; j < nmasks; j++)
    { k = order[j];
      state.showTrack[j]  = trackCheck[k]->isChecked();
      state.trackColor[j] = trackColor[k];
      state.track[j]      = track[k];
      state.Arail[j]      = trackApos[k]->value();
      state.showonB[j]    = trackonB[k]->isChecked();
    }
}

void PaletteDialog::putState(Palette_State &state)
{ QPixmap blob = QPixmap(16,16);
  QPixmap wlob = QPixmap(36,16);
  int j;

  backColor     = state.backColor;
  readColor     = state.readColor;
  alignColor    = state.alignColor;
  branchColor   = state.branchColor;
  gridColor     = state.gridColor;
  haloColor     = state.haloColor;
  elimColor     = state.elimColor;
  stretchColor  = state.stretchColor;
  neutralColor  = state.neutralColor;
  compressColor = state.compressColor;
  compressMax   = state.compressMax;
  stretchMax    = state.stretchMax;
  matchGood     = state.matchGood;
  matchBad      = state.matchBad;
  qualGood      = state.qualGood;
  qualBad       = state.qualBad;
  profLow       = state.profLow;
  profHgh       = state.profHgh;

  for (j = 0; j < 10; j++)
    matchColor[j] = state.matchColor[j];
  for (j = 0; j < 3; j++)
    matchHue[j] = state.matchHue[j];
  for (j = 0; j < 10; j++)
    qualColor[j] = state.qualColor[j];
  for (j = 0; j < 3; j++)
    qualHue[j] = state.qualHue[j];
  for (j = 0; j < 5; j++)
    profColor[j] = state.profColor[j];
  for (j = 0; j < 3; j++)
    profHue[j] = state.profHue[j];

  blob.fill(backColor);
  backBox->setIcon(QIcon(blob));

  blob.fill(readColor);
  readBox->setIcon(QIcon(blob));

  blob.fill(alignColor);
  alignBox->setIcon(QIcon(blob));

  blob.fill(branchColor);
  branchBox->setIcon(QIcon(blob));

  blob.fill(gridColor);
  gridBox->setIcon(QIcon(blob));

  blob.fill(haloColor);
  haloBox->setIcon(QIcon(blob));

  blob.fill(elimColor);
  elimBox->setIcon(QIcon(blob));

  blob.fill(stretchColor);
  stretchBox->setIcon(QIcon(blob));

  blob.fill(neutralColor);
  neutralBox->setIcon(QIcon(blob));

  blob.fill(compressColor);
  compressBox->setIcon(QIcon(blob));

  for (j = 0; j < 10; j++)
    { blob.fill(matchColor[j]);
      matchBox[j]->setIcon(QIcon(blob));
    }
  for (j = 0; j < 3; j++)
    { blob.fill(matchHue[j]);
      matchLev[j]->setIcon(QIcon(blob));
    }
  for (j = 0; j < 10; j++)
    { blob.fill(qualColor[j]);
      qualBox[j]->setIcon(QIcon(blob));
    }
  for (j = 0; j < 3; j++)
    { blob.fill(qualHue[j]);
      qualLev[j]->setIcon(QIcon(blob));
    }
  for (j = 0; j < 5; j++)
    { wlob.fill(profColor[j]);
      profBox[j]->setIcon(QIcon(wlob));
    }
  for (j = 0; j < 3; j++)
    { blob.fill(profHue[j]);
      profLev[j]->setIcon(QIcon(blob));
    }

  gridCheck->setChecked(state.showGrid);
  haloCheck->setChecked(state.showHalo);
  elimCheck->setChecked(state.showElim);

  if (matchGood >= 0)
    matchBot->setText(tr("%1").arg(matchGood));
  if (matchBad >= 0)
    matchTop->setText(tr("%1").arg(matchBad));

  if (qualGood >= 0)
    qualBot->setText(tr("%1").arg(qualGood));
  if (qualBad >= 0)
    qualTop->setText(tr("%1").arg(qualBad));

  if (profLow >= 0)
    profBot->setText(tr("%1").arg(profLow));
  if (profHgh >= 0)
    profTop->setText(tr("%1").arg(profHgh));

  if (compressMax >= 0)
    maxCompress->setText(tr("%1").arg(compressMax));
  if (stretchMax >= 0)
    maxStretch->setText(tr("%1").arg(stretchMax));

  bridgeCheck->setChecked(state.bridges);
  overlapCheck->setChecked(state.overlaps);
  matchCheck->setChecked(state.matchqv);
  qualCheck->setChecked(state.qualqv);
  qualonB->setChecked(state.qualonB);
  profCheck->setChecked(state.profqv);
  if (state.matchMode == 0)
    { matchRadioScale->setChecked(true);
      matchStack->setCurrentIndex(0);
    }
  else
    { matchRadioTri->setChecked(true);
      matchStack->setCurrentIndex(1);
    }
  if (state.qualMode == 0)
    { qualRadioScale->setChecked(true);
      qualStack->setCurrentIndex(0);
    }
  else
    { qualRadioTri->setChecked(true);
      qualStack->setCurrentIndex(1);
    }
  if (state.profMode == 0)
    { profRadioScale->setChecked(true);
      profStack->setCurrentIndex(0);
    }
  else
    { profRadioTri->setChecked(true);
      profStack->setCurrentIndex(1);
    }

  if (state.matchVis)
    matchCheck->setEnabled(true);
  else
    { matchCheck->setChecked(false);
      matchCheck->setEnabled(false);
    }
  if (state.qualVis)
    { qualPanel->setVisible(true);
      qualVis = true;
    }
  else
    { qualPanel->setVisible(false);
      qualVis = false;
    }

  if (state.profVis)
    { profPanel->setVisible(true);
      profVis = true;
    }
  else
    { profPanel->setVisible(false);
      profVis = false;
    }

  for (j = 0; j < state.nmasks; j++)
    { trackPanel[j]->setVisible(true);
      trackVis[j] = true;
      trackCheck[j]->setChecked(state.showTrack[j]);
      trackColor[j] = state.trackColor[j];
      blob.fill(trackColor[j]);
      trackBox[j]->setIcon(QIcon(blob));
      trackLabel[j]->setText(QString(state.track[j]->name));
      track[j] = state.track[j];
      trackApos[j]->setValue(state.Arail[j]);
      trackonB[j]->setChecked(state.showonB[j]);
    }
  for ( ; j < nmasks; j++)
    { trackPanel[j]->setVisible(false);
      trackVis[j] = false;
    }
  nmasks = state.nmasks;

  activateElastic(1);
  activateMatchQV(1);
  activateQualQV(1);
  activateTracks(1);
}

void PaletteDialog::compressCheck()
{ if (maxCompress->text().isEmpty())
    compressMax = -1;
  else
    compressMax = maxCompress->text().toInt();
}

void PaletteDialog::stretchCheck()
{ if (maxStretch->text().isEmpty())
    stretchMax = -1;
  else
    stretchMax = maxStretch->text().toInt();
}

void PaletteDialog::qualGoodCheck()
{ if (qualBot->text().isEmpty())
    qualGood = -1;
  else
    qualGood = qualBot->text().toInt();
}

void PaletteDialog::qualBadCheck()
{ if (qualTop->text().isEmpty())
    qualBad = -1;
  else
    qualBad = qualTop->text().toInt();
}

void PaletteDialog::profLowCheck()
{ if (profBot->text().isEmpty())
    profLow = -1;
  else
    profLow = profBot->text().toInt();
}

void PaletteDialog::profHghCheck()
{ if (profTop->text().isEmpty())
    profHgh = -1;
  else
    profHgh = profTop->text().toInt();
}

void PaletteDialog::matchGoodCheck()
{ if (matchBot->text().isEmpty())
    matchGood = -1;
  else
    matchGood = matchBot->text().toInt();
}

void PaletteDialog::matchBadCheck()
{ if (matchTop->text().isEmpty())
    matchBad = -1;
  else
    matchBad = matchTop->text().toInt();
}

void PaletteDialog::addView()
{ Palette_State state;
  bool ok;
  int  i;

  QString name = QInputDialog::getText(this,tr("Add View"),tr("Enter name:"),
                                       QLineEdit::Normal,tr(""),&ok);
  if (!ok) return;

  for (i = 0; i < viewList->count(); i++)
    if (i != 1 && name == viewList->itemText(i))
      { MainWindow::warning(tr("View \'%1\' previously defined.").arg(name),
                            this,MainWindow::ERROR,tr("OK"));
        return;
      }

  viewList->blockSignals(true);
  viewList->addItem(name);
  viewList->setCurrentIndex(viewList->count()-1);
  viewList->blockSignals(false);
  getState(state);
  writeView(state,name);
}

void PaletteDialog::updateView()
{ Palette_State state;
  QString       name;
  int           idx;

  idx = viewList->currentIndex();
  if (idx == 0)
    { QSettings settings("mpi-cbg", "DaView");
      writeSettings(settings);
    }
  else
    { name = viewList->itemText(idx);
      getState(state);
      writeView(state,name);
    }
}

void PaletteDialog::deleteView()
{ if (viewList->currentIndex() >= 2)
    if (MainWindow::warning(tr("Are you sure?"),this,MainWindow::INFORM,tr("Yes"),tr("No")) == 0)
      { viewList->blockSignals(true);
        viewList->removeItem(viewList->currentIndex()); 
        viewList->setCurrentIndex(0);
        viewList->blockSignals(false);
      }
}

void PaletteDialog::changeView(int idx)
{ Palette_State state;
  QString       name;

  if (idx == 0)
    { QSettings settings("mpi-cbg", "DaView");
      readAndApplySettings(settings);
    }
  else
    { name = viewList->itemText(idx);
      if (readView(state,name))
        putState(state);
    }
}

void PaletteDialog::symmetricDB(bool yes)
{ int i;

  for (i = 0; i < MAX_TRACKS; i++)
    trackonB[i]->setVisible(yes);
  qualonB->setVisible(yes);
}

void PaletteDialog::drawLeftDash(const QColor &color)
{ QPainter p;
  QPicture d;
  QPoint   points[4] = { QPoint(0,20), QPoint(10,15), QPoint(7,20), QPoint(10,25) };
  QBrush   b = QBrush(color);
  QPen     x = QPen(color);

  p.begin(&d);
  x.setJoinStyle(Qt::RoundJoin);
  x.setCapStyle(Qt::RoundCap);
  p.setPen(x);
  p.setBrush(b);
  p.drawConvexPolygon(points,4);
  x.setWidth(2);
  p.setPen(x);
  p.drawLine(2,20,40,20);
  p.end();
  leftDash->setPicture(d);
}

void PaletteDialog::drawRightDash(const QColor &color)
{ QPainter p;
  QPicture d;
  QPoint   points[4] = { QPoint(40,20), QPoint(30,15), QPoint(33,20), QPoint(30,25) };
  QBrush   b = QBrush(color);
  QPen     x = QPen(color);

  p.begin(&d);
  x.setJoinStyle(Qt::RoundJoin);
  x.setCapStyle(Qt::RoundCap);
  p.setPen(x);
  p.setBrush(b);
  p.drawConvexPolygon(points,4);
  x.setWidth(2);
  p.setPen(x);
  p.drawLine(0,20,38,20);
  p.end();
  rightDash->setPicture(d);
}

PaletteDialog::PaletteDialog(QWidget *parent) : QDialog(parent)
{ int j;

  QIntValidator *validInt = new QIntValidator(0,INT32_MAX,this);

  // TAB 1: Basic Options

  // Basic Color Items

  QLabel *backLabel = new QLabel(tr("Background"));

  backBox = new QToolButton();
    backBox->setIconSize(QSize(16,16));
    backBox->setFixedSize(20,20);
    backBox->setIcon(QIcon(QPixmap(16,16)));

  QLabel *readLabel = new QLabel(tr("Reference read"));

  readBox = new QToolButton();
    readBox->setIconSize(QSize(16,16));
    readBox->setFixedSize(20,20);
    readBox->setIcon(QIcon(QPixmap(16,16)));

  QLabel *alignLabel = new QLabel(tr("Aligned segment"));

  alignBox = new QToolButton();
    alignBox->setIconSize(QSize(16,16));
    alignBox->setFixedSize(20,20);
    alignBox->setIcon(QIcon(QPixmap(16,16)));

  QLabel *branchLabel = new QLabel(tr("Unaligned branch"));

  branchBox = new QToolButton();
    branchBox->setIconSize(QSize(16,16));
    branchBox->setFixedSize(20,20);
    branchBox->setIcon(QIcon(QPixmap(16,16)));

  gridBox = new QToolButton();
    gridBox->setIconSize(QSize(16,16));
    gridBox->setFixedSize(20,20);
    gridBox->setIcon(QIcon(QPixmap(16,16)));

  gridLabel = new QLabel(tr("Grid lines"));

  gridCheck = new QCheckBox();
    gridCheck->setFixedWidth(30);
    gridLabel->setEnabled(false);
    gridBox->setEnabled(false);

  haloBox = new QToolButton();
    haloBox->setIconSize(QSize(16,16));
    haloBox->setFixedSize(20,20);
    haloBox->setIcon(QIcon(QPixmap(16,16)));

  haloLabel = new QLabel(tr("Hilight hover"));

  haloCheck = new QCheckBox();
    haloCheck->setFixedWidth(30);
    haloLabel->setEnabled(false);
    haloBox->setEnabled(false);

  elimBox = new QToolButton();
    elimBox->setIconSize(QSize(16,16));
    elimBox->setFixedSize(20,20);
    elimBox->setIcon(QIcon(QPixmap(16,16)));

  elimLabel = new QLabel(tr("Peeled LA"));

  elimCheck = new QCheckBox();
    elimCheck->setFixedWidth(30);
    elimLabel->setEnabled(false);
    elimBox->setEnabled(false);

  QGridLayout *grid = new QGridLayout();
    grid->addWidget(backBox,   0,1,1,1,Qt::AlignVCenter);
    grid->addWidget(backLabel, 0,2,1,1,Qt::AlignLeft|Qt::AlignVCenter);

    grid->addWidget(readBox,   1,1,1,1,Qt::AlignVCenter);
    grid->addWidget(readLabel, 1,2,1,1,Qt::AlignLeft|Qt::AlignVCenter);

    grid->addWidget(alignBox ,  2,1,1,1,Qt::AlignVCenter);
    grid->addWidget(alignLabel, 2,2,1,1,Qt::AlignLeft|Qt::AlignVCenter);

    grid->addWidget(branchBox ,  3,1,1,1,Qt::AlignVCenter);
    grid->addWidget(branchLabel, 3,2,1,1,Qt::AlignLeft|Qt::AlignVCenter);

    grid->addWidget(gridCheck, 4,0,1,1);
    grid->addWidget(gridBox ,  4,1,1,1,Qt::AlignVCenter);
    grid->addWidget(gridLabel, 4,2,1,1,Qt::AlignLeft|Qt::AlignVCenter);

    grid->addWidget(haloCheck, 5,0,1,1);
    grid->addWidget(haloBox ,  5,1,1,1,Qt::AlignVCenter);
    grid->addWidget(haloLabel, 5,2,1,1,Qt::AlignLeft|Qt::AlignVCenter);

    grid->addWidget(elimCheck, 6,0,1,1);
    grid->addWidget(elimBox ,  6,1,1,1,Qt::AlignVCenter);
    grid->addWidget(elimLabel, 6,2,1,1,Qt::AlignLeft|Qt::AlignVCenter);

    grid->setVerticalSpacing(12);

  QWidget *gridPanel = new QWidget();
    gridPanel->setLayout(grid);


  //  Visualization and control of mutliple overlaps with the same B-read

  overLabel = new QLabel(tr("Show overlaps"));

  overlapCheck = new QCheckBox();
    overlapCheck->setFixedWidth(30);

  QHBoxLayout *overLayout = new QHBoxLayout();
    overLayout->addWidget(overlapCheck);
    overLayout->addWidget(overLabel);
    overLayout->addStretch(1);

  bridgeLabel = new QLabel(tr("Show bridges"));

  bridgeCheck = new QCheckBox();
    bridgeCheck->setFixedWidth(30);

  QHBoxLayout *bridgeLayout = new QHBoxLayout();
    bridgeLayout->addWidget(bridgeCheck);
    bridgeLayout->addWidget(bridgeLabel);
    bridgeLayout->addStretch(1);

  stretchBox = new QToolButton();
    stretchBox->setIconSize(QSize(16,16));
    stretchBox->setFixedSize(20,20);
    stretchBox->setIcon(QIcon(QPixmap(16,16)));

  leftLabel = new QLabel(tr("Stretch"));
    leftLabel->setAlignment(Qt::AlignRight);

  leftDash = new QLabel();
    leftDash->setFixedSize(40,20);
    drawLeftDash(QColor(0,0,0));

  maxStretch = new QLineEdit();
    maxStretch->setFixedWidth(22);
    maxStretch->setTextMargins(1,0,0,0);
    maxStretch->setValidator(validInt);

  neutralBox = new QToolButton();
    neutralBox->setIconSize(QSize(16,16));
    neutralBox->setFixedSize(20,20);
    neutralBox->setIcon(QIcon(QPixmap(16,16)));

  neutralLabel = new QLabel(tr("0%"));

  compressBox = new QToolButton();
    compressBox->setIconSize(QSize(16,16));
    compressBox->setFixedSize(20,20);
    compressBox->setIcon(QIcon(QPixmap(16,16)));

  rightLabel = new QLabel(tr("Compress"));
    rightLabel->setAlignment(Qt::AlignLeft);

  rightDash = new QLabel();
    rightDash->setFixedSize(40,20);
    drawRightDash(QColor(0,0,0));

  maxCompress = new QLineEdit();
    maxCompress->setFixedWidth(24);
    maxCompress->setTextMargins(1,0,0,0);
    maxCompress->setValidator(validInt);

  QGridLayout *elastic = new QGridLayout();
    elastic->setColumnStretch(5,1);
    elastic->addLayout(overLayout,    0,0,1,6,Qt::AlignVCenter|Qt::AlignLeft);
    elastic->addLayout(bridgeLayout,  1,0,1,6,Qt::AlignVCenter|Qt::AlignLeft);

    elastic->addWidget(rightLabel,  2,0,1,2,Qt::AlignVCenter|Qt::AlignLeft);
    elastic->addWidget(leftLabel,   2,3,1,2,Qt::AlignVCenter|Qt::AlignRight);

    elastic->addWidget(compressBox, 3,0,1,1,Qt::AlignVCenter|Qt::AlignHCenter);
    elastic->addWidget(leftDash,    3,1,1,1,Qt::AlignVCenter);
    elastic->addWidget(neutralBox,  3,2,1,1,Qt::AlignVCenter|Qt::AlignHCenter);
    elastic->addWidget(rightDash,   3,3,1,1,Qt::AlignVCenter);
    elastic->addWidget(stretchBox,  3,4,1,1,Qt::AlignVCenter|Qt::AlignHCenter);

    elastic->addWidget(maxCompress,  4,0,1,1,Qt::AlignVCenter|Qt::AlignHCenter);
    elastic->addWidget(neutralLabel, 4,2,1,1,Qt::AlignVCenter|Qt::AlignHCenter);
    elastic->addWidget(maxStretch,   4,4,1,1,Qt::AlignVCenter|Qt::AlignHCenter);

  QWidget *elasticPanel = new QWidget();
    elasticPanel->setLayout(elastic);

  QVBoxLayout *basicLayout = new QVBoxLayout();
    basicLayout->addWidget(gridPanel,0,Qt::AlignHCenter);
    basicLayout->addSpacing(20);
    basicLayout->addWidget(elasticPanel,0,Qt::AlignHCenter);
    basicLayout->addStretch(1);

  QWidget *basicTab = new QWidget();
    basicTab->setLayout(basicLayout);


  // TAB 2: Quality Options

  //  Match QV display, both ramp and tri-state

  matchLabel = new QLabel(tr("Show match qv's"));

  matchCheck = new QCheckBox();
    matchCheck->setFixedWidth(30);

  QHBoxLayout *matchSelect = new QHBoxLayout();
    matchSelect->addWidget(matchCheck);
    matchSelect->addSpacing(11);
    matchSelect->addWidget(matchLabel);
    matchSelect->addStretch(1);

  matchLabelScale = new QLabel(tr("Ramp"));
  matchRadioScale = new QRadioButton();

  QHBoxLayout *matchScale = new QHBoxLayout();
    matchScale->addSpacing(40);
    matchScale->addWidget(matchRadioScale);
    matchScale->addSpacing(11);
    matchScale->addWidget(matchLabelScale);
    matchScale->addStretch(1);

  matchLabelTri = new QLabel(tr("Tri-State"));
  matchRadioTri = new QRadioButton();

  QHBoxLayout *matchTristate = new QHBoxLayout();
    matchTristate->addSpacing(40);
    matchTristate->addWidget(matchRadioTri);
    matchTristate->addSpacing(11);
    matchTristate->addWidget(matchLabelTri);
    matchTristate->addStretch(1);

  QButtonGroup *matchOpt = new QButtonGroup(this);
    matchOpt->addButton(matchRadioScale,0);
    matchOpt->addButton(matchRadioTri,1);

  for (j = 0; j < 10; j++)
    { matchBox[j] = new QToolButton();
        matchBox[j]->setIconSize(QSize(16,16));
        matchBox[j]->setFixedSize(20,20);
        matchBox[j]->setIcon(QIcon(QPixmap(16,16)));
    }

  QHBoxLayout *matchRamp = new QHBoxLayout();
    matchRamp->setContentsMargins(10,5,10,5);
    matchRamp->setSpacing(0);
    for (j = 0; j < 10; j++)
      matchRamp->addWidget(matchBox[j]);
    matchRamp->addStretch(1);

  for (j = 0; j < 3; j++)
    { matchLev[j] = new QToolButton();
        matchLev[j]->setIconSize(QSize(16,16));
        matchLev[j]->setFixedSize(20,20);
        matchLev[j]->setIcon(QIcon(QPixmap(16,16)));
    }

  matchLevLabel[0] = new QLabel(tr("Good"));
  matchLevLabel[1] = new QLabel(tr("Unsure"));
  matchLevLabel[2] = new QLabel(tr("Bad"));

  matchBot = new QLineEdit();
    matchBot->setFixedWidth(24);
    matchBot->setTextMargins(1,0,0,0);
    matchBot->setValidator(validInt);

  matchTop = new QLineEdit();
    matchTop->setFixedWidth(24);
    matchTop->setTextMargins(1,0,0,0);
    matchTop->setValidator(validInt);

  QGridLayout *matchTri = new QGridLayout();
    matchTri->setContentsMargins(10,5,10,5);
    matchTri->setHorizontalSpacing(10);
    matchTri->setVerticalSpacing(2);
    for (j = 0; j < 3; j++)
      { matchTri->addWidget(matchLev[j], 0,2*j,1,1, Qt::AlignVCenter|Qt::AlignHCenter);
        matchTri->addWidget(matchLevLabel[j], 0,2*j+1,1,1, Qt::AlignVCenter|Qt::AlignHCenter);
      }
    matchTri->addWidget(matchBot, 1,0, 1,1, Qt::AlignVCenter|Qt::AlignHCenter);
    matchTri->addWidget(matchTop, 1,4, 1,1, Qt::AlignVCenter|Qt::AlignHCenter);
    matchTri->setColumnStretch(6,1.);

  QWidget *matchWidgetScale = new QWidget();
    matchWidgetScale->setLayout(matchRamp);

  QWidget *matchWidgetTri = new QWidget();
    matchWidgetTri->setLayout(matchTri);

  matchStack = new QStackedLayout();
    matchStack->addWidget(matchWidgetScale);
    matchStack->addWidget(matchWidgetTri);

  QVBoxLayout *matchLayout = new QVBoxLayout();
    matchLayout->setSpacing(0);
    matchLayout->addLayout(matchSelect);
    matchLayout->addLayout(matchScale);
    matchLayout->addLayout(matchTristate);
    matchLayout->addLayout(matchStack);

  QWidget *matchPanel = new QWidget();
    matchPanel->setLayout(matchLayout);

  //  Consensus QV display, both ramp and tri-state

  qualLabel = new QLabel(tr("Show qual qv's"));

  qualCheck = new QCheckBox();
    qualCheck->setFixedWidth(30);

  qualonB = new QCheckBox(tr("on B"));

  QHBoxLayout *qualSelect = new QHBoxLayout();
    qualSelect->addWidget(qualCheck);
    qualSelect->addSpacing(11);
    qualSelect->addWidget(qualLabel);
    qualSelect->addSpacing(15);
    qualSelect->addWidget(qualonB);
    qualSelect->addStretch(1);

  qualLabelScale = new QLabel(tr("Ramp"));
  qualRadioScale = new QRadioButton();

  QHBoxLayout *qualScale = new QHBoxLayout();
    qualScale->addSpacing(40);
    qualScale->addWidget(qualRadioScale);
    qualScale->addSpacing(11);
    qualScale->addWidget(qualLabelScale);
    qualScale->addStretch(1);

  qualLabelTri = new QLabel(tr("Tri-State"));
  qualRadioTri = new QRadioButton();

  QHBoxLayout *qualTristate = new QHBoxLayout();
    qualTristate->addSpacing(40);
    qualTristate->addWidget(qualRadioTri);
    qualTristate->addSpacing(11);
    qualTristate->addWidget(qualLabelTri);
    qualTristate->addStretch(1);

  QButtonGroup *qualOpt = new QButtonGroup(this);
    qualOpt->addButton(qualRadioScale,0);
    qualOpt->addButton(qualRadioTri,1);

  for (j = 0; j < 10; j++)
    { qualBox[j] = new QToolButton();
        qualBox[j]->setIconSize(QSize(16,16));
        qualBox[j]->setFixedSize(20,20);
        qualBox[j]->setIcon(QIcon(QPixmap(16,16)));
    }

  QHBoxLayout *qualRamp = new QHBoxLayout();
    qualRamp->setContentsMargins(10,5,10,5);
    qualRamp->setSpacing(0);
    for (j = 0; j < 10; j++)
      qualRamp->addWidget(qualBox[j]);
    qualRamp->addStretch(1);

  for (j = 0; j < 3; j++)
    { qualLev[j] = new QToolButton();
        qualLev[j]->setIconSize(QSize(16,16));
        qualLev[j]->setFixedSize(20,20);
        qualLev[j]->setIcon(QIcon(QPixmap(16,16)));
    }

  qualLevLabel[0] = new QLabel(tr("Good")); 
  qualLevLabel[1] = new QLabel(tr("Unsure"));
  qualLevLabel[2] = new QLabel(tr("Bad"));

  qualBot = new QLineEdit();
    qualBot->setFixedWidth(24);
    qualBot->setTextMargins(1,0,0,0);
    qualBot->setValidator(validInt);

  qualTop = new QLineEdit();
    qualTop->setFixedWidth(24);
    qualTop->setTextMargins(1,0,0,0);
    qualTop->setValidator(validInt);

  QGridLayout *qualTri = new QGridLayout();
    qualTri->setContentsMargins(10,5,10,5);
    qualTri->setHorizontalSpacing(10);
    qualTri->setVerticalSpacing(2);
    for (j = 0; j < 3; j++)
      { qualTri->addWidget(qualLev[j], 0,2*j,1,1, Qt::AlignVCenter|Qt::AlignHCenter);
        qualTri->addWidget(qualLevLabel[j], 0,2*j+1,1,1, Qt::AlignVCenter|Qt::AlignHCenter);
      }
    qualTri->addWidget(qualBot, 1,0, 1,1, Qt::AlignVCenter|Qt::AlignHCenter);
    qualTri->addWidget(qualTop, 1,4, 1,1, Qt::AlignVCenter|Qt::AlignHCenter);
    qualTri->setColumnStretch(6,1.);

  QWidget *qualWidgetScale = new QWidget();
    qualWidgetScale->setLayout(qualRamp);

  QWidget *qualWidgetTri = new QWidget();
    qualWidgetTri->setLayout(qualTri);

  qualStack = new QStackedLayout();
    qualStack->addWidget(qualWidgetScale);
    qualStack->addWidget(qualWidgetTri);

  QVBoxLayout *qualLayout = new QVBoxLayout();
    qualLayout->setContentsMargins(0,20,0,0);
    qualLayout->setSpacing(0);
    qualLayout->addLayout(qualSelect);
    qualLayout->addLayout(qualScale);
    qualLayout->addLayout(qualTristate);
    qualLayout->addLayout(qualStack);

  qualPanel = new QWidget();
    qualPanel->setLayout(qualLayout);

  //  Repeat profile display, both ramp and tri-state

  profLabel = new QLabel(tr("Show repeat profile"));

  profCheck = new QCheckBox();
    profCheck->setFixedWidth(30);

  QHBoxLayout *profSelect = new QHBoxLayout();
    profSelect->addWidget(profCheck);
    profSelect->addSpacing(11);
    profSelect->addWidget(profLabel);
    profSelect->addStretch(1);

  profLabelScale = new QLabel(tr("Ramp"));
  profRadioScale = new QRadioButton();

  QHBoxLayout *profScale = new QHBoxLayout();
    profScale->addSpacing(40);
    profScale->addWidget(profRadioScale);
    profScale->addSpacing(11);
    profScale->addWidget(profLabelScale);
    profScale->addStretch(1);

  profLabelTri = new QLabel(tr("Tri-State"));
  profRadioTri = new QRadioButton();

  QHBoxLayout *profTristate = new QHBoxLayout();
    profTristate->addSpacing(40);
    profTristate->addWidget(profRadioTri);
    profTristate->addSpacing(11);
    profTristate->addWidget(profLabelTri);
    profTristate->addStretch(1);

  QButtonGroup *profOpt = new QButtonGroup(this);
    profOpt->addButton(profRadioScale,0);
    profOpt->addButton(profRadioTri,1);

  for (j = 0; j < 5; j++)
    { profBox[j] = new QToolButton();
        profBox[j]->setIconSize(QSize(36,16));
        profBox[j]->setFixedSize(40,20);
        profBox[j]->setIcon(QIcon(QPixmap(36,16)));
    }

  QHBoxLayout *profRamp = new QHBoxLayout();
    profRamp->setContentsMargins(10,5,10,5);
    profRamp->setSpacing(0);
    for (j = 0; j < 5; j++)
      profRamp->addWidget(profBox[j]);
    profRamp->addStretch(1);

  for (j = 0; j < 3; j++)
    { profLev[j] = new QToolButton();
        profLev[j]->setIconSize(QSize(16,16));
        profLev[j]->setFixedSize(20,20);
        profLev[j]->setIcon(QIcon(QPixmap(16,16)));
    }

  profLevLabel[0] = new QLabel(tr("Low")); 
  profLevLabel[1] = new QLabel(tr("Medium"));
  profLevLabel[2] = new QLabel(tr("High"));

  profBot = new QLineEdit();
    profBot->setFixedWidth(24);
    profBot->setTextMargins(1,0,0,0);
    profBot->setValidator(validInt);

  profTop = new QLineEdit();
    profTop->setFixedWidth(24);
    profTop->setTextMargins(1,0,0,0);
    profTop->setValidator(validInt);

  QGridLayout *profTri = new QGridLayout();
    profTri->setContentsMargins(10,5,10,5);
    profTri->setHorizontalSpacing(10);
    profTri->setVerticalSpacing(2);
    for (j = 0; j < 3; j++)
      { profTri->addWidget(profLev[j], 0,2*j,1,1, Qt::AlignVCenter|Qt::AlignHCenter);
        profTri->addWidget(profLevLabel[j], 0,2*j+1,1,1, Qt::AlignVCenter|Qt::AlignHCenter);
      }
    profTri->addWidget(profBot, 1,0, 1,1, Qt::AlignVCenter|Qt::AlignHCenter);
    profTri->addWidget(profTop, 1,4, 1,1, Qt::AlignVCenter|Qt::AlignHCenter);
    profTri->setColumnStretch(6,1.);

  QWidget *profWidgetScale = new QWidget();
    profWidgetScale->setLayout(profRamp);

  QWidget *profWidgetTri = new QWidget();
    profWidgetTri->setLayout(profTri);

  profStack = new QStackedLayout();
    profStack->addWidget(profWidgetScale);
    profStack->addWidget(profWidgetTri);

  QVBoxLayout *profLayout = new QVBoxLayout();
    profLayout->setContentsMargins(0,20,0,0);
    profLayout->setSpacing(0);
    profLayout->addLayout(profSelect);
    profLayout->addLayout(profScale);
    profLayout->addLayout(profTristate);
    profLayout->addLayout(profStack);

  profPanel = new QWidget();
    profPanel->setLayout(profLayout);

  QVBoxLayout *qualityLayout = new QVBoxLayout();
    qualityLayout->addWidget(matchPanel,0,Qt::AlignHCenter);
    qualityLayout->addWidget(qualPanel,0,Qt::AlignHCenter);
    qualityLayout->addWidget(profPanel,0,Qt::AlignHCenter);
    qualityLayout->addStretch(1);

  QWidget *qualityTab = new QWidget();
    qualityTab->setLayout(qualityLayout);


  // TAB 3: Mask Options

  //  All potential mask options

  QPixmap upd = QPixmap(tr(":/images/UpDown.png")).
                    scaled(16,16,Qt::IgnoreAspectRatio,Qt::SmoothTransformation);

  for (j = 0; j < MAX_TRACKS; j++)
    { trackBox[j] = new QToolButton();
      trackBox[j]->setIconSize(QSize(16,16));
      trackBox[j]->setFixedSize(20,20);
      trackBox[j]->setIcon(QIcon(QPixmap(16,16)));
        trackBox[j]->setEnabled(false);

      trackLabel[j] = new QLabel(tr("Track"));
        trackLabel[j]->setEnabled(false);

      trackCheck[j] = new QCheckBox();
        trackCheck[j]->setChecked(false);
        trackCheck[j]->setFixedWidth(30);

      trackApos[j] = new QSpinBox();
        trackApos[j]->setEnabled(false);
        trackApos[j]->setMinimum(0);
        trackApos[j]->setMaximum(MAX_TRACKS-1);
        trackApos[j]->setSingleStep(1);
        trackApos[j]->setValue(1);
        trackApos[j]->setFixedWidth(40);
        trackApos[j]->setAlignment(Qt::AlignRight);

      trackonB[j] = new QCheckBox(tr("on B"));
        trackonB[j]->setEnabled(false);

      QLabel *tb = new QLabel();
        tb->setFixedSize(16,16);
        tb->setPixmap(upd);

      QHBoxLayout *trackLayout = new QHBoxLayout();
        trackLayout->setContentsMargins(0,0,0,0);
        trackLayout->setSpacing(0);
        trackLayout->addWidget(tb);
        trackLayout->addSpacing(30);
        trackLayout->addWidget(trackCheck[j]);
        trackLayout->addWidget(trackApos[j]);
        trackLayout->addSpacing(12);
        trackLayout->addWidget(trackonB[j]);
        trackLayout->addSpacing(20);
        trackLayout->addWidget(trackBox[j]);
        trackLayout->addSpacing(7);
        trackLayout->addWidget(trackLabel[j]);
        trackLayout->addStretch(1);

      trackPanel[j] = new QWidget();
        trackPanel[j]->setLayout(trackLayout);
    }

  QVBoxLayout *maskLayout = new QVBoxLayout();
    maskLayout->setContentsMargins(10,0,0,0);
    maskLayout->setSpacing(0);
    maskLayout->setSizeConstraint(QLayout::SetFixedSize);
    for (j = 0; j < MAX_TRACKS; j++)
      maskLayout->addWidget(trackPanel[j]);

  maskPanel = new TrackWidget();
    maskPanel->setLayout(maskLayout);

  QScrollArea *maskArea = new QScrollArea();
    maskArea->setWidget(maskPanel);
    maskArea->setAlignment(Qt::AlignHCenter|Qt::AlignTop);
    maskArea->setWidgetResizable(false);

  QVBoxLayout *maskMargin = new QVBoxLayout();
    maskMargin->setContentsMargins(2,15,2,2);
    maskMargin->addWidget(maskArea);

  QWidget *maskTab = new QWidget();
    maskTab->setLayout(maskMargin);

  //  Exit buttons and overall layout

  QTabWidget *tabs = new QTabWidget();
    tabs->addTab(basicTab,tr("Basic"));
    tabs->addTab(qualityTab,tr("Quality"));
    tabs->addTab(maskTab,tr("Masks"));

  QPushButton *addView = new QPushButton("Add");
    addView->setFixedWidth(80);
  QPushButton *updView = new QPushButton("Update");
    updView->setFixedWidth(80);
  QPushButton *delView = new QPushButton("Delete");
    delView->setFixedWidth(80);
  viewList = new QComboBox();
    viewList->addItem("Preferences");
    viewList->insertSeparator(1);
    viewList->setInsertPolicy(QComboBox::InsertAlphabetically);
  QLabel *viewLab = new QLabel("Views:");

  QHBoxLayout *viewBox = new QHBoxLayout();
    viewBox->addWidget(viewLab);
    viewBox->addWidget(viewList,1);

  QHBoxLayout *viewButts = new QHBoxLayout();
    viewButts->addStretch(1);
    viewButts->addWidget(addView);
    viewButts->addWidget(updView);
    viewButts->addWidget(delView);

  QPushButton *cancel = new QPushButton("Cancel");
  QPushButton *open = new QPushButton("OK");

  QHBoxLayout *decision = new QHBoxLayout();
    decision->addStretch(1);
    decision->addWidget(cancel);
    decision->addSpacing(5);
    decision->addWidget(open);

  QVBoxLayout *central = new QVBoxLayout();
    central->addWidget(tabs,1);
    central->addSpacing(15);
    central->addLayout(viewBox);
    central->addLayout(viewButts);
    central->addSpacing(15);
    central->addLayout(decision);

  qualPanel->setVisible(false);
  qualVis = false;
  profPanel->setVisible(false);
  profVis = false;
  nmasks  = 0;
  for (j = 0; j < MAX_TRACKS; j++)
    { trackPanel[j]->setVisible(false);
      trackVis[j] = false;
    }

  setLayout(central);
  open->setDefault(true);
  setWindowTitle(tr("Display Palette"));
  setMinimumSize(PALETTE_MIN_WIDTH,PALETTE_MIN_HEIGHT);
  setModal(true);
  setSizeGripEnabled(false);

  connect(backBox,SIGNAL(pressed()),this,SLOT(backChange()));
  connect(readBox,SIGNAL(pressed()),this,SLOT(readChange()));
  connect(alignBox,SIGNAL(pressed()),this,SLOT(alignChange()));
  connect(branchBox,SIGNAL(pressed()),this,SLOT(branchChange()));
  connect(gridBox,SIGNAL(pressed()),this,SLOT(gridChange()));
  connect(haloBox,SIGNAL(pressed()),this,SLOT(haloChange()));
  connect(elimBox,SIGNAL(pressed()),this,SLOT(elimChange()));
  connect(stretchBox,SIGNAL(pressed()),this,SLOT(stretchChange()));
  connect(neutralBox,SIGNAL(pressed()),this,SLOT(neutralChange()));
  connect(compressBox,SIGNAL(pressed()),this,SLOT(compressChange()));

  connect(open,SIGNAL(clicked()),this,SLOT(accept()));
  connect(cancel,SIGNAL(clicked()),this,SLOT(reject()));

  connect(addView,SIGNAL(clicked()),this,SLOT(addView()));
  connect(updView,SIGNAL(clicked()),this,SLOT(updateView()));
  connect(delView,SIGNAL(clicked()),this,SLOT(deleteView()));
  connect(viewList,SIGNAL(activated(int)),this,SLOT(changeView(int)));

  connect(gridCheck,SIGNAL(stateChanged(int)),this,SLOT(activateGrid(int)));
  connect(haloCheck,SIGNAL(stateChanged(int)),this,SLOT(activateHalo(int)));
  connect(elimCheck,SIGNAL(stateChanged(int)),this,SLOT(activateElim(int)));

  connect(maxStretch,SIGNAL(editingFinished()),this,SLOT(stretchCheck()));
  connect(maxCompress,SIGNAL(editingFinished()),this,SLOT(compressCheck()));

  connect(bridgeCheck,SIGNAL(stateChanged(int)),this,SLOT(activateElastic(int)));
  connect(overlapCheck,SIGNAL(stateChanged(int)),this,SLOT(activateElastic(int)));

  connect(matchCheck,SIGNAL(stateChanged(int)),this,SLOT(activateMatchQV(int)));
  for (j = 0; j < 10; j++)
    connect(matchBox[j],SIGNAL(pressed()),this,SLOT(matchRampChange()));
  for (j = 0; j < 3; j++)
    connect(matchLev[j],SIGNAL(pressed()),this,SLOT(matchTriChange()));

  connect(qualCheck,SIGNAL(stateChanged(int)),this,SLOT(activateQualQV(int)));
  for (j = 0; j < 10; j++)
    connect(qualBox[j],SIGNAL(pressed()),this,SLOT(qualRampChange()));
  for (j = 0; j < 3; j++)
    connect(qualLev[j],SIGNAL(pressed()),this,SLOT(qualTriChange()));

  connect(profCheck,SIGNAL(stateChanged(int)),this,SLOT(activateRepProfile(int)));
  for (j = 0; j < 5; j++)
    connect(profBox[j],SIGNAL(pressed()),this,SLOT(profRampChange()));
  for (j = 0; j < 3; j++)
    connect(profLev[j],SIGNAL(pressed()),this,SLOT(profTriChange()));

  for (j = 0; j < MAX_TRACKS; j++)
    { connect(trackBox[j],SIGNAL(pressed()),this,SLOT(trackChange()));
      connect(trackCheck[j],SIGNAL(stateChanged(int)),this,SLOT(activateTracks(int)));
    }

  connect(matchOpt,SIGNAL(buttonClicked(int)),matchStack,SLOT(setCurrentIndex(int)));
  connect(matchOpt,SIGNAL(buttonClicked(int)),matchStack,SLOT(setCurrentIndex(int)));
  connect(matchBot,SIGNAL(editingFinished()),this,SLOT(matchGoodCheck()));
  connect(matchTop,SIGNAL(editingFinished()),this,SLOT(matchBadCheck()));
  matchRadioScale->setChecked(true);

  connect(qualOpt,SIGNAL(buttonClicked(int)),qualStack,SLOT(setCurrentIndex(int)));
  connect(profOpt,SIGNAL(buttonClicked(int)),profStack,SLOT(setCurrentIndex(int)));

  connect(qualBot,SIGNAL(editingFinished()),this,SLOT(qualGoodCheck()));
  connect(qualTop,SIGNAL(editingFinished()),this,SLOT(qualBadCheck()));
  connect(qualonB,SIGNAL(stateChanged(int)),this,SLOT(enforceMatchOff(int)));
  connect(profBot,SIGNAL(editingFinished()),this,SLOT(profLowCheck()));
  connect(profTop,SIGNAL(editingFinished()),this,SLOT(profHghCheck()));
  qualRadioScale->setChecked(true);
}

void PaletteDialog::readAndApplySettings(QSettings &settings)
{ static QColor black = QColor(0,0,0);
  static QColor white = QColor(255,255,255);

  Palette_State state;
  QRgb matchRGB[10];
  QRgb matchTri[3];
  QRgb qualRGB[10];
  QRgb qualTri[3];
  QRgb profRGB[10];
  QRgb profTri[3];
  int  j;

  settings.beginGroup("palette");
    QRgb backRGB      = settings.value("back",QColor(0,0,0).rgb()).toUInt();
    QRgb readRGB      = settings.value("read",QColor(255,255,50).rgb()).toUInt();
    QRgb alignRGB     = settings.value("align",QColor(255,255,255).rgb()).toUInt();
    QRgb branchRGB    = settings.value("branch",QColor(150,100,50).rgb()).toUInt();
    QRgb gridRGB      = settings.value("grid",QColor(255,255,50).rgb()).toUInt();
    QRgb haloRGB      = settings.value("halo",QColor(0,255,255).rgb()).toUInt();
    QRgb elimRGB      = settings.value("elim",QColor(0,255,255).rgb()).toUInt();
    QRgb stretchRGB   = settings.value("stretch",QColor(125,125,255).rgb()).toUInt();
    QRgb neutralRGB   = settings.value("neutral",QColor(255,255,255).rgb()).toUInt();
    QRgb compressRGB  = settings.value("compress",QColor(255,125,125).rgb()).toUInt();

    matchRGB[0] = settings.value("match0",QColor(221,221,221).rgb()).toUInt();
    matchRGB[1] = settings.value("match1",QColor(221,221,221).rgb()).toUInt();
    matchRGB[2] = settings.value("match2",QColor(221,221,221).rgb()).toUInt();
    matchRGB[3] = settings.value("match3",QColor(221,221,221).rgb()).toUInt();
    matchRGB[4] = settings.value("match4",QColor(221,221,221).rgb()).toUInt();
    matchRGB[5] = settings.value("match5",QColor(255,221,0).rgb()).toUInt();
    matchRGB[6] = settings.value("match6",QColor(255,170,68).rgb()).toUInt();
    matchRGB[7] = settings.value("match7",QColor(255,68,68).rgb()).toUInt();
    matchRGB[8] = settings.value("match8",QColor(204,68,221).rgb()).toUInt();
    matchRGB[9] = settings.value("match9",QColor(204,68,221).rgb()).toUInt();

    matchTri[0] = settings.value("matchLow",QColor(100,255,100).rgb()).toUInt();
    matchTri[1] = settings.value("matchMid",QColor(255,255,100).rgb()).toUInt();
    matchTri[2] = settings.value("matchHgh",QColor(255,100,100).rgb()).toUInt();

    qualRGB[0] = settings.value("qual0",QColor(221,221,221).rgb()).toUInt();
    qualRGB[1] = settings.value("qual1",QColor(221,221,221).rgb()).toUInt();
    qualRGB[2] = settings.value("qual2",QColor(221,221,221).rgb()).toUInt();
    qualRGB[3] = settings.value("qual3",QColor(221,221,221).rgb()).toUInt();
    qualRGB[4] = settings.value("qual4",QColor(221,221,221).rgb()).toUInt();
    qualRGB[5] = settings.value("qual5",QColor(255,221,0).rgb()).toUInt();
    qualRGB[6] = settings.value("qual6",QColor(255,170,68).rgb()).toUInt();
    qualRGB[7] = settings.value("qual7",QColor(255,68,68).rgb()).toUInt();
    qualRGB[8] = settings.value("qual8",QColor(204,68,221).rgb()).toUInt();
    qualRGB[9] = settings.value("qual9",QColor(204,68,221).rgb()).toUInt();

    qualTri[0] = settings.value("qualLow",QColor(100,255,100).rgb()).toUInt();
    qualTri[1] = settings.value("qualMid",QColor(255,255,100).rgb()).toUInt();
    qualTri[2] = settings.value("qualHgh",QColor(255,100,100).rgb()).toUInt();

    profRGB[0] = settings.value("prof0",QColor(205,255,255).rgb()).toUInt();
    profRGB[1] = settings.value("prof1",QColor(155,225,225).rgb()).toUInt();
    profRGB[2] = settings.value("prof2",QColor(105,195,195).rgb()).toUInt();
    profRGB[3] = settings.value("prof3",QColor( 55,165,165).rgb()).toUInt();
    profRGB[4] = settings.value("prof4",QColor(  5,135,135).rgb()).toUInt();

    profTri[0] = settings.value("profGood",QColor(205,255,255).rgb()).toUInt();
    profTri[1] = settings.value("profMid",QColor(105,195,195).rgb()).toUInt();
    profTri[2] = settings.value("profBad",QColor(  5,135,135).rgb()).toUInt();

    state.showGrid    = settings.value("showG",false).toBool();
    state.showHalo    = settings.value("showH",false).toBool();
    state.showElim    = settings.value("showP",false).toBool();
    state.stretchMax  = settings.value("strMax",30).toInt();
    state.compressMax = settings.value("compMax",30).toInt();
    state.bridges     = settings.value("bridge",false).toBool();
    state.overlaps    = settings.value("overlap",false).toBool();

    state.matchqv     = settings.value("matchQV",false).toBool();
    state.matchMode   = settings.value("matchMode",0).toInt();
    state.matchGood   = settings.value("matchGood",23).toInt();
    state.matchBad    = settings.value("matchBad",27).toInt();

    state.qualqv     = settings.value("qualQV",false).toBool();
    state.qualMode   = settings.value("qualMode",0).toInt();
    state.qualGood   = settings.value("qualGood",23).toInt();
    state.qualBad    = settings.value("qualBad",27).toInt();
    state.qualonB    = settings.value("qualonB",false).toInt();

    state.profqv     = settings.value("profQV",false).toBool();
    state.profMode   = settings.value("profMode",0).toInt();
    state.profLow    = settings.value("profLow",1).toInt();
    state.profHgh    = settings.value("profHgh",11).toInt();
  settings.endGroup();

  state.backColor.setRgb(backRGB);
  state.readColor.setRgb(readRGB);
  state.alignColor.setRgb(alignRGB);
  state.branchColor.setRgb(branchRGB);
  state.gridColor.setRgb(gridRGB);
  state.haloColor.setRgb(haloRGB);
  state.elimColor.setRgb(elimRGB);
  state.stretchColor.setRgb(stretchRGB);
  state.neutralColor.setRgb(neutralRGB);
  state.compressColor.setRgb(compressRGB);
  for (j = 0; j < 10; j++)
    state.matchColor[j].setRgb(matchRGB[j]);
  for (j = 0; j < 3; j++)
    state.matchHue[j].setRgb(matchTri[j]);
  for (j = 0; j < 10; j++)
    state.qualColor[j].setRgb(qualRGB[j]);
  for (j = 0; j < 3; j++)
    state.qualHue[j].setRgb(qualTri[j]);
  for (j = 0; j < 5; j++)
    state.profColor[j].setRgb(profRGB[j]);
  for (j = 0; j < 3; j++)
    state.profHue[j].setRgb(profTri[j]);

  if (state.elimColor == black)
    state.drawElim = -1;
  else if (state.elimColor == white)
    state.drawElim = 1;
  else
    state.drawElim = 0;

  state.matchVis = matchCheck->isEnabled();
  state.qualVis = qualVis;
  state.profVis = profVis;

  for (j = 0; j < nmasks; j++)
    { char *s = track[j]->name;
      int   a = settings.value(tr("track.%1.Apos").arg(s),-1).toInt();
      if (a < 0)
        { state.trackColor[j] = trackColor[j];
          state.showTrack[j]  = trackCheck[j]->isChecked();
          state.Arail[j]      = trackApos[j]->value();
          state.showonB[j]    = trackonB[j]->isChecked();
        }
      else
        { state.trackColor[j].setRgb(settings.value(tr("track.%1.color").arg(s)).toUInt());
          state.showTrack[j] = settings.value(tr("track.%1.show").arg(s)).toBool();
          state.showonB[j]   = settings.value(tr("track.%1.onB").arg(s)).toBool();
          state.Arail[j]     = a;
        }
      state.track[j] = track[j];
    }
  state.nmasks = nmasks;

  putState(state);
}

void PaletteDialog::writeSettings(QSettings &settings)
{ int j;

  if (viewList->currentIndex() != 0)
    return;

  settings.beginGroup("palette");
    settings.setValue("back",backColor.rgb());
    settings.setValue("read",readColor.rgb());
    settings.setValue("align",alignColor.rgb());
    settings.setValue("branch",branchColor.rgb());
    settings.setValue("grid",gridColor.rgb());
    settings.setValue("halo",haloColor.rgb());
    settings.setValue("elim",elimColor.rgb());
    settings.setValue("stretch",stretchColor.rgb());
    settings.setValue("neutral",neutralColor.rgb());
    settings.setValue("compress",compressColor.rgb());
    settings.setValue("showG", gridCheck->isChecked());
    settings.setValue("showH", haloCheck->isChecked());
    settings.setValue("showP", elimCheck->isChecked());
    settings.setValue("strMax", stretchMax);
    settings.setValue("compMax", compressMax);
    settings.setValue("bridge", bridgeCheck->isChecked());
    settings.setValue("overlap", overlapCheck->isChecked());

    settings.setValue("matchQV", matchCheck->isChecked());
    for (j = 0; j < 10; j++)
      settings.setValue(tr("match%1").arg(j),matchColor[j].rgb());
    settings.setValue(tr("matchLow"),matchHue[0].rgb());
    settings.setValue(tr("matchMid"),matchHue[1].rgb());
    settings.setValue(tr("matchHgh"),matchHue[2].rgb());
    settings.setValue("matchMode", matchRadioTri->isChecked());
    settings.setValue("matchGood", matchGood);
    settings.setValue("matchBad", matchBad);

    settings.setValue("qualQV", qualCheck->isChecked());
    for (j = 0; j < 10; j++)
      settings.setValue(tr("qual%1").arg(j),qualColor[j].rgb());
    settings.setValue(tr("qualLow"),qualHue[0].rgb());
    settings.setValue(tr("qualMid"),qualHue[1].rgb());
    settings.setValue(tr("qualHgh"),qualHue[2].rgb());
    settings.setValue("qualMode", qualRadioTri->isChecked());
    settings.setValue("qualGood", qualGood);
    settings.setValue("qualBad", qualBad);
    settings.setValue("qualonB", qualonB->isChecked());

    settings.setValue("profQV", profCheck->isChecked());
    for (j = 0; j < 5; j++)
      settings.setValue(tr("prof%1").arg(j),profColor[j].rgb());
    settings.setValue(tr("profGood"),profHue[0].rgb());
    settings.setValue(tr("profMid"),profHue[1].rgb());
    settings.setValue(tr("profBad"),profHue[2].rgb());
    settings.setValue("profMode", profRadioTri->isChecked());
    settings.setValue("profLow", profLow);
    settings.setValue("profHgh", profHgh);

    for (j = 0; j < nmasks; j++)
      { char *name = track[j]->name;
        settings.setValue(tr("track.%1.show").arg(name),trackCheck[j]->isChecked());
        settings.setValue(tr("track.%1.color").arg(name),trackColor[j].rgb());
        settings.setValue(tr("track.%1.Apos").arg(name),trackApos[j]->value());
        settings.setValue(tr("track.%1.onB").arg(name),trackonB[j]->isChecked());
      }
  settings.endGroup();
}

bool PaletteDialog::readView(Palette_State &state, QString &view)
{ static QColor black = QColor(0,0,0);
  static QColor white = QColor(255,255,255);

  QHash<QString,int> mhash;
  QHash< void *,int> phash;

  QSettings settings("mpi-cbg", "DaView");
  int     j, k, h, s;
  int     order[MAX_TRACKS];
  QString name;

  if ( ! QFile(settings.fileName()).exists())
    { MainWindow::warning(tr("Whoops, can't find view %1").arg(view),this,
                          MainWindow::ERROR,tr("OK"));
      return (false);
    }

  settings.beginGroup(view);
    if ( ! settings.value("exists",0).toBool())
      { MainWindow::warning(tr("Whoops, can't find view %1").arg(view),this,
                            MainWindow::ERROR,tr("OK"));
        return (false);
      }

    state.backColor.setRgb(settings.value("back").toUInt());
    state.readColor.setRgb(settings.value("read").toUInt());
    state.alignColor.setRgb(settings.value("align").toUInt());
    state.branchColor.setRgb(settings.value("branch").toUInt());

    state.gridColor.setRgb(settings.value("grid").toUInt());
    state.haloColor.setRgb(settings.value("halo").toUInt());
    state.elimColor.setRgb(settings.value("elim").toUInt());
    state.showGrid = settings.value("showG").toBool();
    state.showHalo = settings.value("showH").toBool();
    state.showElim = settings.value("showP").toBool();

    if (state.elimColor == black)
      state.drawElim = -1;
    else if (state.elimColor == white)
      state.drawElim = 1;
    else
      state.drawElim = 0;

    state.bridges     = settings.value("bridge").toBool();
    state.overlaps    = settings.value("overlap").toBool();
    state.stretchMax  = settings.value("strMax").toInt();
    state.compressMax = settings.value("compMax").toInt();
    state.neutralColor.setRgb(settings.value("neutral").toUInt());
    state.compressColor.setRgb(settings.value("compress").toUInt());

    state.matchVis  = true;
    state.matchqv   = settings.value("matchQV").toBool();
    state.matchMode = settings.value("matchMode").toInt();
    for (j = 0; j < 10; j++)
      state.matchColor[j].setRgb(settings.value(tr("match%1").arg(j)).toUInt());
    state.matchHue[0].setRgb(settings.value("matchLow").toUInt());
    state.matchHue[1].setRgb(settings.value("matchMid").toUInt());
    state.matchHue[2].setRgb(settings.value("matchHgh").toUInt());
    state.matchGood = settings.value("matchGood").toInt();
    state.matchBad  = settings.value("matchBad").toInt();

    state.qualVis  = settings.value("qualVis").toBool();
    if (state.qualVis)
      { state.qualqv   = settings.value("qualQV").toBool();
        state.qualMode = settings.value("qualMode").toInt();
        for (j = 0; j < 10; j++)
          state.qualColor[j].setRgb(settings.value(tr("qual%1").arg(j)).toUInt());
        state.qualHue[0].setRgb(settings.value("qualLow").toUInt());
        state.qualHue[1].setRgb(settings.value("qualMid").toUInt());
        state.qualHue[2].setRgb(settings.value("qualHgh").toUInt());
        state.qualGood = settings.value("qualGood").toInt();
        state.qualBad  = settings.value("qualBad").toInt();
        state.qualonB  = settings.value("qualonB").toInt();
      }
    else
      { state.qualqv        = qualCheck->isChecked();
        state.qualMode      = qualRadioTri->isChecked();
        for (j = 0; j < 10; j++)
          state.qualColor[j] = qualColor[j];
        for (j = 0; j < 3; j++)
          state.qualHue[j] = qualHue[j];
        state.qualGood      = qualGood;
        state.qualBad       = qualBad;
        state.qualonB       = qualonB->isChecked();
      }
    state.qualVis = qualVis;

    state.profVis  = settings.value("profVis").toBool();
    if (state.profVis)
      { state.profqv   = settings.value("profQV").toBool();
        state.profMode = settings.value("profMode").toInt();
        for (j = 0; j < 5; j++)
          state.profColor[j].setRgb(settings.value(tr("prof%1").arg(j)).toUInt());
        state.profHue[0].setRgb(settings.value("profGood").toUInt());
        state.profHue[1].setRgb(settings.value("profMid").toUInt());
        state.profHue[2].setRgb(settings.value("profBad").toUInt());
        state.profLow  = settings.value("profLow").toInt();
        state.profHgh  = settings.value("profHgh").toInt();
      }
    else
      { state.profqv        = profCheck->isChecked();
        state.profMode      = profRadioTri->isChecked();
        for (j = 0; j < 5; j++)
          state.profColor[j] = profColor[j];
        for (j = 0; j < 3; j++)
          state.profHue[j] = profHue[j];
        state.profLow       = profLow;
        state.profHgh       = profHgh;
      }
    state.profVis = profVis;

    QVBoxLayout *lman = static_cast<QVBoxLayout *>(maskPanel->layout());
    QList<QObject *> ord(maskPanel->children());

    for (j = 0; j < nmasks; j++)
      { phash[ord.at(j+1)] = j;
        mhash[track[j]->name] = j;
      }
    for (j = 0; j < nmasks; j++)
      order[j] = phash[lman->itemAt(j)->widget()];

    // row j of VBoxLayout contains track widgets order[j] of VBoxLayout
    // track j with given name is mhash[name]

    h = 0;
    state.nmasks = settings.value("nmasks").toInt();
    for (s = 0; s < state.nmasks; s++)
      { name = settings.value(tr("track.%1.name").arg(s)).toString();
        if (mhash.contains(name))
          { k = order[h++];
            j = mhash[name];
            state.track[k] = track[j];
            state.showTrack[k] = settings.value(tr("track.%1.show").arg(s)).toBool();
            state.trackColor[k].setRgb(settings.value(tr("track.%1.color").arg(s)).toUInt());
            state.Arail[k] = settings.value(tr("track.%1.Apos").arg(s)).toInt();
            state.showonB[k] = settings.value(tr("track.%1.onB").arg(s)).toBool();
            mhash[name] = -1;
          }
      }

    for (s = 0; s < nmasks; s++)
      { j = order[s];
        if (mhash[track[j]->name] >= 0)
          { k = order[h++];
            state.track[k] = track[j];
            state.showTrack[k] = trackCheck[j]->isChecked();
            state.trackColor[k] = trackColor[j];
            state.Arail[k] = trackApos[j]->value();
            state.showonB[k] = trackonB[j]->isChecked();
          }
      }

    state.nmasks = nmasks;
 
  settings.endGroup();

  return (true);
}

void PaletteDialog::writeView(Palette_State &state, QString &view)
{ QSettings settings("mpi-cbg", "DaView");
  int j;

  settings.beginGroup(view);
    settings.setValue("exists",1);

    settings.setValue("back",state.backColor.rgb());
    settings.setValue("read",state.readColor.rgb());
    settings.setValue("align",state.alignColor.rgb());
    settings.setValue("branch",state.branchColor.rgb());

    settings.setValue("grid",state.gridColor.rgb());
    settings.setValue("halo",state.haloColor.rgb());
    settings.setValue("elim",state.haloColor.rgb());
    settings.setValue("showG",state.showGrid);
    settings.setValue("showH",state.showHalo);
    settings.setValue("showP",state.showElim);

    settings.setValue("bridge",state.bridges);
    settings.setValue("overlap",state.overlaps);
    settings.setValue("strMax",state.stretchMax);
    settings.setValue("compMax",state.compressMax);
    settings.setValue("stretch",state.stretchColor.rgb());
    settings.setValue("neutral",state.neutralColor.rgb());
    settings.setValue("compress",state.compressColor.rgb());

    settings.setValue("matchQV",state.matchqv);
    settings.setValue("matchMode",state.matchMode);
    for (j = 0; j < 10; j++)
      settings.setValue(tr("match%1").arg(j),state.matchColor[j].rgb());
    settings.setValue(tr("matchLow"),state.matchHue[0].rgb());
    settings.setValue(tr("matchMid"),state.matchHue[1].rgb());
    settings.setValue(tr("matchHgh"),state.matchHue[2].rgb());
    settings.setValue("matchGood",state.matchGood);
    settings.setValue("matchBad",state.matchBad);

    settings.setValue("qualVis",state.qualVis);
    settings.setValue("qualQV",state.qualqv);
    settings.setValue("qualMode",state.qualMode);
    for (j = 0; j < 10; j++)
      settings.setValue(tr("qual%1").arg(j),state.qualColor[j].rgb());
    settings.setValue(tr("qualLow"),state.qualHue[0].rgb());
    settings.setValue(tr("qualMid"),state.qualHue[1].rgb());
    settings.setValue(tr("qualHgh"),state.qualHue[2].rgb());
    settings.setValue("qualGood", state.qualGood);
    settings.setValue("qualBad", state.qualBad);
    settings.setValue("qualonB", state.qualonB);

    settings.setValue("profVis",state.profVis);
    settings.setValue("profQV",state.profqv);
    settings.setValue("profMode",state.profMode);
    for (j = 0; j < 5; j++)
      settings.setValue(tr("prof%1").arg(j),state.profColor[j].rgb());
    settings.setValue(tr("profGood"),state.profHue[0].rgb());
    settings.setValue(tr("profMid"),state.profHue[1].rgb());
    settings.setValue(tr("profBad"),state.profHue[2].rgb());
    settings.setValue("profLow", state.profLow);
    settings.setValue("profHgh", state.profHgh);

    settings.setValue("nmasks",state.nmasks);
    for (j = 0; j < state.nmasks; j++)
      { settings.setValue(tr("track.%1.show").arg(j),state.showTrack[j]);
        settings.setValue(tr("track.%1.color").arg(j),state.trackColor[j].rgb());
        settings.setValue(tr("track.%1.name").arg(j),state.track[j]->name);
        settings.setValue(tr("track.%1.Apos").arg(j),state.Arail[j]);
        settings.setValue(tr("track.%1.onB").arg(j),state.showonB[j]);
      }
  settings.endGroup();
}


/*****************************************************************************\
*
*  OPEN DIALOG
*
\*****************************************************************************/

#define DIRECTORY(a,e,f,b,c,title,suffix)		\
  QString dir;						\
  if ((a) == NULL)					\
    if ((b) == NULL)					\
      if ((c) == NULL)					\
        dir = tr(".");					\
      else						\
        dir = (c)->absolutePath();			\
    else						\
      dir = (b)->absolutePath();			\
  else							\
    dir = (a)->absolutePath();				\
  QString name = QFileDialog::getOpenFileName(this,	\
                    tr(title),dir,tr(suffix));		\
  if ( ! name.isNull())					\
    { delete (a);					\
      (a) = new QFileInfo(name);			\
      (e)->setText((a)->fileName());			\
      (f) = (e)->text();				\
    }


void OpenDialog::openLAS()
{ DIRECTORY(lasInfo,lasFile,lasText,AInfo,BInfo,"Open .las file","Alignment file (*.las)"); }

void OpenDialog::openADB()
{ DIRECTORY(AInfo,AFile,AText,BInfo,lasInfo,"Open .db file","DB or DAM file (*.db *.dam)"); }

void OpenDialog::openBDB()
{ DIRECTORY(BInfo,BFile,BText,AInfo,lasInfo,"Open .db file","DB or DAM file (*.db *.dam)"); }

void OpenDialog::activateB(int state)
{ bool on;

  on = (state == Qt::Checked);

  BLabel->setEnabled(on);
  BFile->setEnabled(on);
  BSelect->setEnabled(on);
  if (on == false || BInfo == NULL)
    BFile->setText(tr(""));
  else
    BFile->setText(BInfo->fileName());
}

void OpenDialog::activateSubset(int state)
{ bool on;

  on = (state == Qt::Checked);

  rLabel->setEnabled(on);
  from->setEnabled(on);
  beg->setEnabled(on);
  to->setEnabled(on);
  end->setEnabled(on);
  if (on == false)
    { beg->setText(tr(""));
      end->setText(tr(""));
    }
  else
    { if (first >= 0)
        beg->setText(tr("%1").arg(first));
      if (last >= 0)
        end->setText(tr("%1").arg(last));
    }
}

#define CHECKCHANGE(info,edit,txt)								\
  QString    dir;										\
  QFileInfo *ninfo;										\
												\
  if ((txt) == (edit)->text())									\
    return;											\
  (txt) = (edit)->text();									\
  if ((txt).isEmpty())										\
    return;											\
  if ((info) == NULL)										\
    dir = tr(".");										\
  else												\
    dir = (info)->absolutePath();								\
  ninfo = new QFileInfo(dir + tr("/") + (txt));							\
  if ( ! ninfo->exists())									\
    { MainWindow::warning(									\
             tr("File ")+(ninfo)->absoluteFilePath()+tr(" does not exist"),			\
             this,MainWindow::WARNING,tr("OK"));						\
      delete ninfo;										\
      return;											\
    }												\
  delete info;											\
  (info) = ninfo;

void OpenDialog::lasCheck() { CHECKCHANGE(lasInfo,lasFile,lasText) }

void OpenDialog::ACheck() { CHECKCHANGE(AInfo,AFile,AText) }

void OpenDialog::BCheck() { CHECKCHANGE(BInfo,BFile,BText) }

void OpenDialog::firstCheck()
{ if (beg->text().isEmpty())
    first = -1;
  else
    first = beg->text().toInt();
}

void OpenDialog::lastCheck()
{ if (end->text().isEmpty())
    last = -1;
  else
    last = end->text().toInt();
}

#define CHECKPATH(info,edit,txt,name)								\
  (txt) = (edit)->text();									\
  if ((txt).isEmpty())										\
    { MainWindow::warning(									\
             tr("%1-file name is emtpy").arg(name),						\
             this,MainWindow::WARNING,tr("OK"));						\
      return;											\
    }												\
  if ((info) == NULL)										\
    dir = tr(".");										\
  else												\
    dir = (info)->absolutePath();								\
  ninfo = new QFileInfo(dir + tr("/") + (txt));							\
  if ( ! ninfo->exists())									\
    { MainWindow::warning(									\
             tr("File ")+(ninfo)->absoluteFilePath()+tr(" does not exist"),			\
             this,MainWindow::WARNING,tr("OK"));						\
      delete ninfo;										\
      return;											\
    }												\
  delete info;											\
  (info) = ninfo;

void OpenDialog::aboutTo()
{ QString    dir;
  QFileInfo *ninfo;

  CHECKPATH(lasInfo,lasFile,lasText,tr("las"))
  CHECKPATH(AInfo,AFile,AText,tr("A"))
  if (BBox->isChecked())
    { CHECKPATH(BInfo,BFile,BText,tr("B")) }

  if (rBox->isChecked())
    { if (beg->text().isEmpty())
        first = -1;
      else
        { QString s = beg->text();
          int     p = beg->cursorPosition();
          if (beg->validator()->validate(s,p) != QValidator::Acceptable)
            { MainWindow::warning(tr("First (")+s+tr(") is not a positive integer"),
                          this,MainWindow::ERROR,tr("OK"));
              return;
            }
          first = beg->text().toInt();
        }

      if (end->text().isEmpty())
        last = -1;
      else
        { QString s = end->text();
          int     p = end->cursorPosition();
          if (end->validator()->validate(s,p) != QValidator::Acceptable)
            { MainWindow::warning(tr("Last (")+s+tr(") is not a positive integer"),
                          this,MainWindow::ERROR,tr("OK"));
              return;
            }
          last = end->text().toInt();
        }

      if (first >= 0 && last >= 0 && first > last)
        { MainWindow::warning(tr("First (")+tr("%1").arg(first)+tr(") > last (") +
                              tr("%1").arg(last) + tr(")"),
                              this,MainWindow::ERROR,tr("OK"));
          return;
        }
    }
  accept();
}

DataModel *OpenDialog::openDataSet(int link, int laps, int elim, int comp, int expn, char **mesg)
{ char         *v;
  DataModel    *m;
  int           f, l;
  Palette_State palette;
    
  if (rBox->isChecked())
    { f = first-1;
      l = last;
    }
  else
    f = l = -1;
  if (BBox->isChecked())
    m = openModel( lasInfo->absoluteFilePath().toLatin1().data(),
                   AInfo->absoluteFilePath().toLatin1().data(),
                   BInfo->absoluteFilePath().toLatin1().data(),f,l,!link,!laps,elim,comp,expn,&v);
  else
    m = openModel( lasInfo->absoluteFilePath().toLatin1().data(),
                   AInfo->absoluteFilePath().toLatin1().data(),
                   NULL,f,l,!link,!laps,elim,comp,expn,&v);
  *mesg = v;
  return (m);
}

void OpenDialog::setView()
{ viewList->clear();
  viewList->addItem(tr("Preferences"));
  viewList->insertSeparator(1);
  viewList->addItems(MainWindow::views);
  viewList->setCurrentIndex(MainWindow::cview);
}

int OpenDialog::getView()
{ return (viewList->currentIndex()); }

void OpenDialog::getState(Open_State &state)
{
  state.lasInfo       = lasInfo;
  state.lasText       = lasText;
  state.AInfo         = AInfo;
  state.AText         = AText;
  state.asym          = BBox->isChecked();
  state.BInfo         = BInfo;
  state.BText         = BText;
  state.subrange      = rBox->isChecked();
  state.first         = first;
  state.last          = last;
}

void OpenDialog::putState(Open_State &state)
{ QPixmap blob = QPixmap(16,16);

  lasInfo       = state.lasInfo;
  AInfo         = state.AInfo;
  BInfo         = state.BInfo;

  AText         = state.AText;
  BText         = state.BText;
  lasText       = state.lasText;

  first         = state.first;
  last          = state.last;

  lasFile->setText(lasText);
  AFile->setText(AText);
  
  if (state.asym)
    activateB(Qt::Checked);
  else
    activateB(Qt::Unchecked);
  BBox->setChecked(state.asym);

  if (state.subrange)
    activateSubset(Qt::Checked);
  else
    activateSubset(Qt::Unchecked);
  rBox->setChecked(state.subrange);
}

OpenDialog::OpenDialog(QWidget *parent) : QDialog(parent)
{
  lasInfo = AInfo = BInfo = NULL;
  first = last = -1;

  QIntValidator *validInt = new QIntValidator(1,INT32_MAX,this);

  QLabel *lasLabel = new QLabel(tr("LA file:"));
  QPushButton *lasSelect = new QPushButton("Pick");

  QLabel *ALabel   = new QLabel(tr("A Database:"));
  QPushButton *ASelect = new QPushButton("Pick");

  BLabel  = new QLabel(tr("B Database:"));
  BSelect = new QPushButton("Pick");

  lasFile = new MyLineEdit();
  AFile   = new MyLineEdit();
  BFile   = new MyLineEdit();

  BBox = new QCheckBox();
    BBox->setCheckState(Qt::Unchecked);
    BLabel->setEnabled(false);
    BFile->setEnabled(false);
    BSelect->setEnabled(false);

  rLabel = new QLabel(tr("Reads:"));
  from = new QLabel(tr("from"));
  beg = new QLineEdit();
    beg->setFixedWidth(70);
    beg->setValidator(validInt);
  to   = new QLabel(tr("to"));
  end = new QLineEdit();
    end->setFixedWidth(70);
    end->setValidator(validInt);

  rBox = new QCheckBox();
    rBox->setCheckState(Qt::Unchecked);
    rLabel->setEnabled(false);
    from->setEnabled(false);
    beg->setEnabled(false);
    to->setEnabled(false);
    end->setEnabled(false);

  QHBoxLayout *subset = new QHBoxLayout();
    subset->addWidget(from);
    subset->addWidget(beg);
    subset->addWidget(to);
    subset->addWidget(end);
    subset->addStretch(1);
  
  QGridLayout *grid = new QGridLayout();
    grid->addWidget(lasLabel, 0,1,1,1,Qt::AlignRight|Qt::AlignVCenter);
    grid->addWidget(lasFile,  0,2,1,1,Qt::AlignVCenter);
    grid->addWidget(lasSelect,0,3,1,1,Qt::AlignVCenter);

    grid->addWidget(ALabel, 1,1,1,1,Qt::AlignRight|Qt::AlignVCenter);
    grid->addWidget(AFile,  1,2,1,1,Qt::AlignVCenter);
    grid->addWidget(ASelect,1,3,1,1,Qt::AlignVCenter);

    grid->addWidget(BBox,   2,0,1,1);
    grid->addWidget(BLabel, 2,1,1,1,Qt::AlignRight|Qt::AlignVCenter);
    grid->addWidget(BFile,  2,2,1,1,Qt::AlignVCenter);
    grid->addWidget(BSelect,2,3,1,1,Qt::AlignVCenter);

    grid->addWidget(rBox,   3,0,1,1);
    grid->addWidget(rLabel, 3,1,1,1,Qt::AlignRight|Qt::AlignVCenter);
    grid->addLayout(subset, 3,2,1,2,Qt::AlignVCenter);

    grid->setColumnStretch(2,1);
    grid->setVerticalSpacing(12);

  viewList = new QComboBox();
    viewList->addItem("Preferences");
    viewList->insertSeparator(1);
    viewList->setInsertPolicy(QComboBox::InsertAlphabetically);
  QLabel *viewLab = new QLabel("Views:");

  QHBoxLayout *viewBox = new QHBoxLayout();
    viewBox->addWidget(viewLab);
    viewBox->addWidget(viewList,1);

  cancel = new QPushButton("Cancel");
  open = new QPushButton("Open");

  QHBoxLayout *decision = new QHBoxLayout();
    decision->addStretch(1);
    decision->addWidget(cancel);
    decision->addSpacing(5);
    decision->addWidget(open);

  QVBoxLayout *central = new QVBoxLayout();
    central->addLayout(grid);
    central->addSpacing(15);
    central->addLayout(viewBox);
    central->addSpacing(15);
    central->addStretch(1);
    central->addLayout(decision);

  setLayout(central);
  open->setDefault(true);
  setWindowTitle(tr("Open LA Dataset"));
  setModal(true);
  setSizeGripEnabled(true);

  connect(open,SIGNAL(clicked()),this,SLOT(aboutTo()));

  connect(cancel,SIGNAL(clicked()),this,SLOT(reject()));
  connect(BBox,SIGNAL(stateChanged(int)),this,SLOT(activateB(int)));
  connect(rBox,SIGNAL(stateChanged(int)),this,SLOT(activateSubset(int)));

  connect(lasSelect,SIGNAL(clicked()),this,SLOT(openLAS()));
  connect(ASelect,SIGNAL(clicked()),this,SLOT(openADB()));
  connect(BSelect,SIGNAL(clicked()),this,SLOT(openBDB()));

  connect(lasFile,SIGNAL(focusOut()),this,SLOT(lasCheck()));
  connect(AFile,SIGNAL(focusOut()),this,SLOT(ACheck()));
  connect(BFile,SIGNAL(focusOut()),this,SLOT(BCheck()));

  connect(beg,SIGNAL(editingFinished()),this,SLOT(firstCheck()));
  connect(end,SIGNAL(editingFinished()),this,SLOT(lastCheck()));
}

void OpenDialog::readAndApplySettings(QSettings &settings)
{ Open_State state;

  settings.beginGroup("open");
    state.lasText  = settings.value("las",tr("")).toString();
    state.AText    = settings.value("A",tr("")).toString();
    state.BText    = settings.value("B",tr("")).toString();
    state.first    = settings.value("first",-1).toInt();
    state.last     = settings.value("last",-1).toInt();
    state.asym     = settings.value("asymmetric",false).toBool();
    state.subrange = settings.value("subset",false).toBool();
  settings.endGroup();

  if (state.lasText.isEmpty())
    state.lasInfo = NULL;
  else
    { state.lasInfo = new QFileInfo(state.lasText);
      state.lasText = state.lasInfo->fileName();
    }
  if (state.AText.isEmpty())
    state.AInfo = NULL;
  else
    { state.AInfo = new QFileInfo(state.AText);
      state.AText = state.AInfo->fileName();
    }
  if (state.BText.isEmpty())
    state.BInfo = NULL;
  else
    { state.BInfo = new QFileInfo(state.BText);
      state.BText = state.BInfo->fileName();
    }

  putState(state);
}

void OpenDialog::writeSettings(QSettings &settings)
{ settings.beginGroup("open");
    if (lasInfo != NULL)
      settings.setValue("las", lasInfo->absoluteFilePath());
    if (AInfo != NULL)
      settings.setValue("A", AInfo->absoluteFilePath());
    if (BInfo != NULL)
      settings.setValue("B", BInfo->absoluteFilePath());
    settings.setValue("first", first);
    settings.setValue("last", last);
    settings.setValue("asymmetric", BBox->isChecked());
    settings.setValue("subset", rBox->isChecked());
  settings.endGroup();
}

/*****************************************************************************\
*
*  MAIN WINDOW
*
\*****************************************************************************/

QList<MainWindow *> MainWindow::frames;
Palette_State       MainWindow::palette;
Open_State          MainWindow::dataset;
int                 MainWindow::numLive;

QStringList         MainWindow::views;
int                 MainWindow::cview;

MainWindow::MainWindow(MainWindow *origin) : QMainWindow()
{
  openDialog = new OpenDialog(this);

  paletteDialog = new PaletteDialog(this);

  myscroll = new MyScroll(this);

  model = NULL;

  QPushButton *queryButton = new QPushButton(tr("Query"));
    queryButton->setFixedWidth(80);

  queryEntry = new MyLineEdit(this);
    queryEntry->setText(tr(""));
    queryEntry->setFixedHeight(24);
    queryEntry->setFrame(false);
    queryEntry->setFont(QFont(tr("Monaco")));
    queryEntry->setToolTip(tr("# [- #]       view these piles\nC # (, #)*  color these reads"));

  QPalette redPal;
    redPal.setColor(QPalette::Text,Qt::red);

  queryMesg = new QLineEdit(this);
    queryMesg->setText(tr(""));
    queryMesg->setFixedHeight(24);
    queryMesg->setFrame(false);
    queryMesg->setFont(QFont(tr("Monaco")));
    queryMesg->setReadOnly(true);
    queryMesg->setFocusPolicy(Qt::NoFocus);
    queryMesg->setPalette(redPal);

    QVBoxLayout *queryArea = new QVBoxLayout();
      queryArea->addWidget(queryEntry);
      queryArea->addWidget(queryMesg);
      queryArea->setSpacing(0);
      queryArea->setMargin(0);

    QFrame *queryBorder = new QFrame();
     queryBorder->setFrameStyle(QFrame::StyledPanel|QFrame::Plain);
     queryBorder->setLayout(queryArea);

    QHBoxLayout *queryLayout = new QHBoxLayout();
      queryLayout->addSpacing(15);
      queryLayout->addWidget(queryButton);
      queryLayout->addSpacing(20);
      queryLayout->addWidget(queryBorder,1);
      queryLayout->addSpacing(15);
      queryLayout->setMargin(15);

    queryPanel = new QWidget();
      queryPanel->setLayout(queryLayout);

  hbar = new QFrame(this,Qt::Widget);
     hbar->setFrameStyle(QFrame::HLine | QFrame::Plain);

    drawPanel = new QVBoxLayout();
      drawPanel->addWidget(myscroll,1);
      drawPanel->addWidget(hbar,0);
      drawPanel->addWidget(queryPanel,0);
      drawPanel->setSpacing(0);
      drawPanel->setMargin(0);

  QWidget *centralFrame = new QWidget();
    centralFrame->setLayout(drawPanel);
  setCentralWidget(centralFrame);

  setWindowTitle(tr("DaViewer"));
  setMinimumSize(DAVIEW_MIN_WIDTH,DAVIEW_MIN_HEIGHT);

  createActions();
  createToolBars();
  if (origin == NULL && frames.length() > 0)
    createCloneMenus();
  else
    createMenus();

  if (origin == NULL)
    { if (frames.length() == 0)
        { readAndApplySettings();
          paletteDialog->getState(palette);
          openDialog->getState(dataset);
        }
      else
        { removeToolBar(fileToolBar);
          toolAct->setText(tr("Show Toolbar"));

          drawPanel->removeWidget(queryPanel);
          drawPanel->removeWidget(hbar);
          queryPanel->hide();
          hbar->hide();

          paletteDialog->putState(palette);
          openDialog->putState(dataset);
        }
    }
  else
    { resize(origin->size());
      move(origin->pos() + QPoint(frameHeight,frameHeight));
      if (origin->fileToolBar->isVisible())
        { toolArea = origin->toolBarArea(origin->fileToolBar);
          removeToolBar(fileToolBar);
          addToolBar(toolArea,fileToolBar);
          fileToolBar->setVisible(true);
          toolAct->setText(tr("Hide Toolbar"));
        }
      else
        { toolArea = origin->toolArea;
          removeToolBar(fileToolBar);
          toolAct->setText(tr("Show Toolbar"));
        }
      if (origin->drawPanel->count() < 3)
        { drawPanel->removeWidget(queryPanel);
          drawPanel->removeWidget(hbar);
          queryPanel->hide();
          hbar->hide();
          queryAct->setText(tr("Show Query Panel"));
        }

      model = origin->model;
      myscroll->setModel(model);

      { Scroll_State state;

        origin->myscroll->getState(state);
        myscroll->putState(state);
        myscroll->update();
      }

      paletteDialog->putState(origin->palette);
      openDialog->putState(origin->dataset);

      show();
    }

  if ( ! palette.showHalo)
    myscroll->haloUpdate(false);

  connect(queryButton,SIGNAL(clicked()),this,SLOT(query()));
  connect(queryEntry,SIGNAL(returnPressed()),this,SLOT(query()));
  connect(queryEntry,SIGNAL(touched()),this,SLOT(clearMesg()));

  frames += this;
  if (screenGeometry == NULL)
    { frameWidth     = frameGeometry().width() - geometry().width();
      frameHeight    = frameGeometry().height() - geometry().height();
      screenGeometry = new QRect(QApplication::desktop()->availableGeometry(this));
    }
}

void MainWindow::clearMesg()
{ queryMesg->setText(tr("")); }

void MainWindow::query()
{ static QColor newColor = QColor(255,125,255);

  int         read1, read2;
  int         beg, end;
  QString     frag;
  char       *text;
  int         r1pos, r2pos, dpos;
  int         i, len;

  queryEntry->processed(true);

  if (model == NULL)
    { queryMesg->setText(tr("^ No model loaded"));
      return;
    }

  frag = queryEntry->text();
  text = frag.toLatin1().data();
  len  = frag.length();

  i = 0;
  while (i < len && isspace(text[i]))
    i += 1;
  if (i >= len)
    { queryMesg->setText(tr("^ Empty request"));
      return;
    }

  //  Color query handling

  if (text[i] == 'C')
    { i += 1;
      while (i < len && isspace(text[i]))
        i += 1;

      r1pos = i;
      read2 = model->db2->nreads;
      while (i < len)
        { if ( ! isdigit(text[i]))
            { queryMesg->setText(tr("%1").arg(tr(""),i) + tr("^ Expecting a positive number"));
              return;
            }
          dpos = i;
          while (i < len && isdigit(text[i]))
            i += 1;
          read1 = atoi(text+dpos);
          if (read1 < 1 || read1 > read2)
            { queryMesg->setText(tr("%1").arg(tr(""),dpos) +
                                 tr("^ Out of range [1,%1]").arg(QString::number(read2)));
              return;
            }

          while (i < len && isspace(text[i]))
            i += 1;
          if (i < len)
            { if (text[i] != ',')
                { queryMesg->setText(tr("%1").arg(tr(""),i) + tr("^ Expecting a comma"));
                  return;
                }
              i += 1;
              while (i < len && isspace(text[i]))
                i += 1;
            }
        }

      newColor = QColorDialog::getColor(newColor,this,tr("Read Color"));
      if ( ! newColor.isValid())
        return;

      text = frag.toLatin1().data();
      i = r1pos;
      while (i < len)
        { read1 = atoi(text+i);
          while (i < len && isdigit(text[i]))
            i += 1;
          while (i < len && isspace(text[i]))
            i += 1;
          if (i < len)
            { i += 1;
              while (i < len && isspace(text[i]))
                i += 1;
            }
          myscroll->setColor(newColor,read1-1);
          text = frag.toLatin1().data();
        }

      return;
    }

  //  Pile query handling

  if ( ! isdigit(text[i]))
    { queryMesg->setText(tr("%1").arg(tr(""),i) + tr("^ Expecting a positive number"));
      return;
    }
  r1pos = i;
  while (i < len && isdigit(text[i]))
    i += 1;
  while (i < len && isspace(text[i]))
    i += 1;
  if (i < len && text[i] == '-')
    { dpos = i;
      i += 1; 
      while (i < len && isspace(text[i]))
        i += 1;
      if (i >= len)
        { queryMesg->setText(tr("%1").arg(tr(""),dpos+1) + tr("^ Expecting a positive number"));
          return;
        }
      if ( ! isdigit(text[i]))
        { queryMesg->setText(tr("%1").arg(tr(""),i) + tr("^ Expecting a positive number"));
          return;
        }
      r2pos = i;
      while (i < len && isdigit(text[i]))
        i += 1;
      while (i < len && isspace(text[i]))
        i += 1;
      if (i < len)
        { queryMesg->setText(tr("%1").arg(tr(""),i) + tr("^ Expecting end of query"));
          return;
        }
    }
  else if (i < len)
    { queryMesg->setText(tr("%1").arg(tr(""),i) + tr("^ Expecting a dash between two numbers"));
      return;
    }
  else
    dpos = -1;

  if (dpos < 0) 
    read1 = read2 = frag.toInt();
  else
    { read1 = frag.left(dpos).toInt();
      read2 = frag.right((len-dpos)-1).toInt();
    }

  if (read1 > read2)
    { queryMesg->setText(tr("%1").arg(tr(""),dpos) + tr("^ first read > second read ?"));
      return;
    }
  if (read1 < model->first+1)
    { queryMesg->setText(tr("%1").arg(tr(""),r1pos) +
                         tr("^ Out of range [%1,%2]").
                              arg(QString::number(model->first+1),QString::number(model->last)));
      return;
    }
  if (read2 > model->last)
    { if (dpos < 0)
        dpos = r1pos;
      else
        dpos = r2pos;
      queryMesg->setText(tr("%1").arg(tr(""),dpos) +
                         tr("^ Out of range [%1,%2]").
                              arg(QString::number(model->first+1),QString::number(model->last)));
      return;
    }

  readSpan(model,read1-1,read2,&beg,&end);
  myscroll->hsToRange(beg,end);
}

void MainWindow::closeEvent(QCloseEvent *event)
{ int i, nmain;

  if (model != NULL && model->nref == 0)
    { freeClone(model);
      for (i = 0; i < frames.length(); i++)
        if (frames[i] == this)
          { frames.removeAt(i);
            break;
          }
      event->accept();
      return;
    }

  nmain = 0;
  for (i = 0; i < frames.length(); i++)
    if (frames[i]->model == NULL || frames[i]->model->nref != 0)
      nmain += 1;

  if (nmain == 1)
    { writeSettings();
      for (i = frames.length()-1; i >= 0; i--)
        if (frames[i] != this)
          frames[i]->close();
      event->accept();
      return;
    }

  for (i = 0; i < frames.length(); i++)
    if (frames[i] == this)
      { frames.removeAt(i);
        break;
      }
  event->accept();
}

void MainWindow::closeAll()
{ int i;

  for (i = frames.length()-1; i >= 0; i--)
    frames[i]->close();
}

void MainWindow::fullScreen()
{ if ((windowState() & Qt::WindowFullScreen) != 0)
    { setWindowState( windowState() & ~ Qt::WindowFullScreen);
      fullScreenAct->setText(tr("Full Screen"));
    }
  else
    { setWindowState( windowState() | Qt::WindowFullScreen);
      fullScreenAct->setText(tr("Normal Size"));
    }
}

void MainWindow::unminimizeAll()
{ int i;

  for (i = 0; i < frames.length(); i++)
    frames[i]->setWindowState( windowState() & ~ Qt::WindowMinimized);
}

void MainWindow::raiseAll()
{ int i;

  for (i = 0; i < frames.length(); i++)
    frames[i]->raise();
}

void PaletteDialog::restoreLayout()
{ QVBoxLayout *lman = static_cast<QVBoxLayout *>(maskPanel->layout());
  int k;

  for (k = 0; k < nmasks; k++)
    lman->removeWidget(trackPanel[k]);
  for (k = nmasks-1; k >= 0; k--)
    lman->insertWidget(0,trackPanel[k]);
}

int PaletteDialog::loadTracks(Palette_State &state, DataModel *model)
{ DAZZ_TRACK   *t;
  QHash<QString,int> nhash;
  int           j, k, a, cnt;

  for (k = nmasks-1; k >= 0; k--)
    nhash[trackLabel[k]->text()] = k;

  QSettings settings("mpi-cbg", "DaView");

  settings.beginGroup("palette");
    j = 0;
    cnt = 0;
    for (t = model->db1->tracks; t != NULL; t = t->next)
      if (t != model->qvs && t != model->prf)
        { a = settings.value(tr("track.%1.Apos").arg(t->name),-1).toInt();
          if (a >= 0)
            { QRgb tRGB = settings.value(tr("track.%1.color").arg(t->name)).toUInt();
              state.trackColor[j].setRgb(tRGB);
              state.Arail[j]     = a;
              state.showonB[j]   = settings.value(tr("track.%1.onB").arg(t->name)).toBool();
              state.showTrack[j] = settings.value(tr("track.%1.show").arg(t->name)).toBool();
            }
          else if (nhash.contains(t->name))
            { k = nhash[t->name];
              state.trackColor[j] = trackColor[k];
              state.Arail[j]      = trackApos[k]->value();
              state.showonB[j]    = trackonB[k]->isChecked();
              state.showTrack[j]  = trackCheck[k]->isChecked();
            }
          else
            { state.trackColor[j] = QColor(0,0,0);
              state.Arail[j]      = 1;
              state.showonB[j]    = false;
              state.showTrack[j]  = false;
            }
          state.track[j] = t;
          if (state.showTrack[j] && state.Arail[j] > cnt)
            cnt = state.Arail[j];
          j += 1;
        }
    state.nmasks  = j;
    state.matchVis = (model->tspace != 0);
    state.qualVis = (model->qvs != NULL);
    state.profVis = (model->prf != NULL);
  settings.endGroup();

  return (cnt);
}

void MainWindow::openFiles()
{ Open_State state;
  char      *mesg;
  int        j;

  if (model != NULL && model->nref > 1)
    { for (j = frames.length()-1; j >= 0; j--)
        if (frames[j] != NULL && frames[j]->model->nref == 0)
          frames[j]->close();
    }

  openDialog->setView();
  openDialog->getState(state);
  if (openDialog->exec() == QDialog::Accepted)
    { openDialog->getState(dataset);
      cview = openDialog->getView();

      model = openDialog->openDataSet(palette.bridges,palette.overlaps,palette.drawElim,
                                      palette.compressMax,palette.stretchMax,&mesg);
      if (model != NULL)
        {
          numLive = paletteDialog->loadTracks(palette,model);

          if (cview > 0)
            { paletteDialog->putState(palette);
              paletteDialog->readView(palette,views[cview-2]);
            }

          for (j = 0; j < frames.length(); j++)
            { frames[j]->myscroll->setRange(dataWidth(model),dataHeight(model)+5+numLive);
              if (dataWidth(model) > 1000000)
                frames[j]->myscroll->hsToRange(0,1000000);
              else
                frames[j]->myscroll->hsToRange(0,dataWidth(model));
              frames[j]->openDialog->putState(dataset);
              frames[j]->paletteDialog->symmetricDB(! dataset.asym);
              frames[j]->paletteDialog->putState(palette);
              frames[j]->setWindowTitle(tr("DaViewer - %1").arg(dataset.lasText));
              frames[j]->model = model;
              frames[j]->myscroll->setModel(model);
              frames[j]->update();
            }
        }
      else
        { for (j = 0; j < frames.length(); j++)
            { frames[j]->model = model;
              frames[j]->myscroll->setModel(model);
              frames[j]->update();
            }

          MainWindow::warning(tr(mesg+10),this,MainWindow::ERROR,tr("OK"));
        }
    }
  // else
    // openDialog->putState(state);
}

void MainWindow::openPalette()
{ Palette_State state;
  int i;

  paletteDialog->setView();
  paletteDialog->getState(state);
  if (paletteDialog->exec() == QDialog::Accepted)
    { paletteDialog->getState(palette);
      paletteDialog->getView();
      paletteDialog->restoreLayout();

      if (state.bridges != palette.bridges || state.overlaps != palette.overlaps ||
          state.drawElim != palette.drawElim)
        reLayoutModel(model, ! palette.bridges, ! palette.overlaps, palette.drawElim,
                             palette.compressMax, palette.stretchMax );

      numLive = paletteDialog->liveCount();
      for (i = 0; i < frames.length(); i++)
        { frames[i]->myscroll->setRange(dataWidth(model),dataHeight(model)+5+numLive);
          if (state.showHalo != palette.showHalo)
            frames[i]->myscroll->haloUpdate(palette.showHalo);
          frames[i]->myscroll->update();
          frames[i]->paletteDialog->putState(palette);
        }
    }
  // else
    // paletteDialog->putState(state);
}

void MainWindow::openCopy()
{ MainWindow *main = new MainWindow(this);
  main->raise();
  main->show();
}

void MainWindow::toggleToolBar()
{ int i;

  if (fileToolBar->isVisible())
    { for (i = 0; i < frames.length(); i++)
        if (frames[i]->model == NULL || frames[i]->model->nref != 0)
          { frames[i]->toolArea = frames[i]->toolBarArea(frames[i]->fileToolBar);
            frames[i]->removeToolBar(frames[i]->fileToolBar);
            frames[i]->toolAct->setText(tr("Show Toolbar"));
          }
    }
  else
    { for (i = 0; i < frames.length(); i++)
        if (frames[i]->model == NULL || frames[i]->model->nref != 0)
          { frames[i]->addToolBar(frames[i]->toolArea,frames[i]->fileToolBar);
            frames[i]->fileToolBar->setVisible(true);
            frames[i]->toolAct->setText(tr("Hide Toolbar"));
          }
    }
}

void MainWindow::toggleQuery()
{ int i;

  if (drawPanel->count() == 3)
    { for (i = 0; i < frames.length(); i++)
        if (frames[i]->model == NULL || frames[i]->model->nref != 0)
          { frames[i]->drawPanel->removeWidget(frames[i]->queryPanel);
            frames[i]->drawPanel->removeWidget(frames[i]->hbar);
            frames[i]->queryPanel->hide();
            frames[i]->hbar->hide();
            frames[i]->queryAct->setText(tr("Show Query"));
          }
    }
  else
    { for (i = 0; i < frames.length(); i++)
        if (frames[i]->model == NULL || frames[i]->model->nref != 0)
          { frames[i]->drawPanel->insertWidget(1,frames[i]->hbar);
            frames[i]->drawPanel->insertWidget(2,frames[i]->queryPanel);
            frames[i]->queryPanel->show();
            frames[i]->hbar->show();
            frames[i]->queryAct->setText(tr("Hide Query"));
          }
    }
}

void MainWindow::createActions()
{ openAct = new QAction(QIcon(":/images/open.png"), tr("&Open"), this);
    openAct->setShortcut(tr("Ctrl+O"));
    openAct->setToolTip(tr("Open an overlap data set"));

  paletteAct = new QAction(QIcon(":/images/palette.png"), tr("&Palette"), this);
    paletteAct->setShortcut(tr("Ctrl+P"));
    paletteAct->setToolTip(tr("Set display options and colors"));

  copyAct = new QAction(QIcon(":/images/create.png"), tr("&Duplicate"), this);
    copyAct->setShortcut(tr("Ctrl+D"));
    copyAct->setToolTip(tr("Duplicate current pile window"));

  exitAct = new QAction(tr("Exit"), this);
    exitAct->setShortcut(tr("Ctrl+Q"));
    exitAct->setToolTip(tr("Exit the application"));

  fullScreenAct = new QAction(tr("&Full Screen"), this);
    fullScreenAct->setShortcut(tr("Ctrl+F"));
    fullScreenAct->setToolTip(tr("Expand window to full screen"));

  minimizeAct = new QAction(tr("&Minimize"), this);
    minimizeAct->setShortcut(tr("Ctrl+M"));
    minimizeAct->setToolTip(tr("Minimize the current window"));

  unminimizeAllAct = new QAction(tr("&Unminimize All"), this);
    unminimizeAllAct->setShortcut(tr("Ctrl+U"));
    unminimizeAllAct->setToolTip(tr("Un-minimize all windows"));

  raiseAllAct = new QAction(tr("Raise all windows"), this);
    raiseAllAct->setShortcut(tr("Ctrl+R"));
    raiseAllAct->setToolTip(tr("Bring all windows to front"));

  connect(openAct, SIGNAL(triggered()), this, SLOT(openFiles()));
  connect(paletteAct, SIGNAL(triggered()), this, SLOT(openPalette()));
  connect(copyAct, SIGNAL(triggered()), this, SLOT(openCopy()));
  connect(exitAct, SIGNAL(triggered()), this, SLOT(closeAll()));

  connect(fullScreenAct, SIGNAL(triggered()), this, SLOT(fullScreen()));
  connect(minimizeAct, SIGNAL(triggered()), this, SLOT(showMinimized()));

  connect(unminimizeAllAct, SIGNAL(triggered()), this, SLOT(unminimizeAll()));
  connect(raiseAllAct, SIGNAL(triggered()), this, SLOT(raiseAll()));

  toolAct = new QAction(tr("Hide Toolbar"), this);
    toolAct->setToolTip(tr("Show/Hide toolbars in all pile windows"));

  queryAct = new QAction(tr("Hide Query"), this);
    toolAct->setToolTip(tr("Show/Hide query pane in all pile windows"));

  tileAct = new QAction(QIcon(":/images/tile.png"), tr("&Tile"), this);
    tileAct->setShortcut(tr("Ctrl+T"));
    tileAct->setToolTip(tr("Tile all open image windows"));

  cascadeAct = new QAction(QIcon(":/images/cascade.png"), tr("&Cascade"), this);
    cascadeAct->setShortcut(tr("Ctrl+C"));
    cascadeAct->setToolTip(tr("Cascade all open image windows"));

  connect(queryAct, SIGNAL(triggered()), this, SLOT(toggleQuery()));
  connect(toolAct, SIGNAL(triggered()), this, SLOT(toggleToolBar()));
  connect(tileAct,SIGNAL(triggered()),this,SLOT(tileImages()));
  connect(cascadeAct,SIGNAL(triggered()),this,SLOT(cascadeImages()));
}


#define SCROLL_PADDING 4

void MainWindow::tileImages()
{ int tileH, tileW, tileSize;

  int screenH = screenGeometry->height();
  int screenW = screenGeometry->width();
  int maximH  = screenH;
  int maximW  = screenW;

  if (frames.length() != 0)
    { tileSize   = frames.length();
      for (int i = 0; i < frames.length(); i++)
        { MainWindow *win = frames[i];
          int h = win->maximumHeight();
          if (h < maximH)
            maximH = h;
          int w = win->maximumWidth();
          if (w < maximW)
            maximW = w;
        }
    }
  else
    return;

  maximH += frameHeight;
  maximW += frameWidth;

  int deltaW  = SCROLL_PADDING + frameWidth;
  int deltaH  = SCROLL_PADDING + frameHeight;
  int minimH  = minimumHeight() + frameHeight;
  int minimW  = minimumWidth() + deltaW;

  { int wmax, hmax, wmin;

    wmax = screenW / minimW;
    hmax = screenH / minimH;
    wmin = (tileSize-1) / hmax + 1;

    if (wmin > wmax)
      { tileW = wmax;
        tileH = hmax;
      }
    else
      { for (int h = 1; h <= hmax; h++)
          { tileH = h;
            tileW = (tileSize-1) / h + 1;
            if ( screenW / tileW - deltaW >= screenH / tileH - deltaH)
              break;
          }
      }
  }

  { int w, h;

    int screenX = screenGeometry->x();
    int screenY = screenGeometry->y();
    int stepY   = screenH / tileH;
    int stepX   = screenW / tileW;
    if (stepX > maximW)
      stepX = maximW;
    if (stepY > maximH)
      stepY = maximH;

    QSize winSize = QSize(stepX-frameWidth,stepY-frameHeight);

    w = 0;
    h = 0;
    for (int t = 0; t < tileSize; t++)
      { frames[t]->move(QPoint(screenX+w*stepX,screenY+h*stepY));
        frames[t]->resize(winSize);
        frames[t]->raise();
        h += 1;
        if (h >= tileH)
          { h = 0;
            w += 1;
            if (w >= tileW)
              w = 0;
          }
      }
    raise();
  }
}

void MainWindow::cascadeImages()
{ printf("Cascade\n");
  int x = screenGeometry->x();
  int y = screenGeometry->y();

  for (int i = 0; i < frames.length(); i++)
    { frames[i]->move(QPoint(x,y));
      frames[i]->raise();
      x += frameHeight;
      y += frameHeight;
    }
  raise();
}

void MainWindow::createToolBars()
{ fileToolBar = addToolBar(tr("File ToolBar"));
    fileToolBar->addAction(openAct);
    fileToolBar->addAction(paletteAct);
    fileToolBar->addAction(tileAct);
    fileToolBar->addAction(cascadeAct);
    fileToolBar->addAction(copyAct);
  toolArea = Qt::TopToolBarArea;
}

void MainWindow::createMenus()
{ QMenuBar *bar = menuBar();

  QMenu *fileMenu = bar->addMenu(tr("&File"));
    fileMenu->addAction(openAct);
    fileMenu->addAction(paletteAct);
    fileMenu->addAction(copyAct);
    fileMenu->addSeparator();
    fileMenu->addAction(queryAct);
    fileMenu->addAction(toolAct);
    fileMenu->addSeparator();
    fileMenu->addAction(exitAct);

  QMenu *imageMenu = bar->addMenu(tr("&Image"));
    imageMenu->addAction(tileAct);
    imageMenu->addAction(cascadeAct);

  QMenu *windowMenu = bar->addMenu(tr("&Window"));
    windowMenu->addAction(fullScreenAct);
    windowMenu->addAction(minimizeAct);
    windowMenu->addSeparator();
    windowMenu->addAction(unminimizeAllAct);
    windowMenu->addAction(raiseAllAct);
}

void MainWindow::createCloneMenus()
{ QMenuBar *bar = menuBar();

  QMenu *fileMenu = bar->addMenu(tr("&File"));
    fileMenu->addAction(paletteAct);
    fileMenu->addSeparator();
    fileMenu->addAction(exitAct);

  QMenu *windowMenu = bar->addMenu(tr("&Window"));
    windowMenu->addAction(fullScreenAct);
    windowMenu->addAction(minimizeAct);
    windowMenu->addSeparator();
    windowMenu->addAction(unminimizeAllAct);
    windowMenu->addAction(raiseAllAct);
}

void MainWindow::readAndApplySettings()
{ int j, h;

  QSettings settings("mpi-cbg", "DaView");

  if ( ! QFile(settings.fileName()).exists())
    settings.clear();

  settings.beginGroup("main");
    QPoint pos         = settings.value("pos", QPoint(40, 40)).toPoint();
    QSize  size        = settings.value("size",
                                  QSize(DAVIEW_MIN_WIDTH, DAVIEW_MIN_HEIGHT)).toSize();
    bool   tbarVisible = settings.value("tbarVisible", true).toBool();
    Qt::ToolBarArea tbarArea = 
        (Qt::ToolBarArea) settings.value("tbarArea", Qt::TopToolBarArea).toInt();
    bool   queryVisible = settings.value("queryVisible", true).toBool();

    views.clear();
    h = settings.value("nviews",0).toInt();
    cview = settings.value("cview",0).toInt();
    for (j = 0; j < h; j++)
      views.append( settings.value(tr("view.%1").arg(j)).toString() );
  settings.endGroup();

  resize(size);
  move(pos);
  if (tbarVisible)
    { removeToolBar(fileToolBar);
      addToolBar(tbarArea,fileToolBar);
      fileToolBar->setVisible(true);
      toolAct->setText(tr("Hide Toolbar"));
    }
  else
    { removeToolBar(fileToolBar);
      toolAct->setText(tr("Show Toolbar"));
    }
  toolArea = tbarArea;
  if (! queryVisible)
    { drawPanel->removeWidget(queryPanel);
      drawPanel->removeWidget(hbar);
      queryPanel->hide();
      hbar->hide();
      queryAct->setText(tr("Show Query Panel"));
    }
  this->show();

  openDialog->readAndApplySettings(settings);
  paletteDialog->readAndApplySettings(settings);
}

void MainWindow::writeSettings()
{ QSettings settings("mpi-cbg", "DaView");
  int j;

  settings.beginGroup("main");
    settings.setValue("pos", pos());
    settings.setValue("size", size());
    if (fileToolBar->isVisible())
      settings.setValue("tbarArea", toolBarArea(fileToolBar));
    else
      settings.setValue("tbarArea", toolArea);
    settings.setValue("tbarVisible", fileToolBar->isVisible());
    if (drawPanel->count() == 3)
      settings.setValue("queryVisible",true);
    else
      settings.setValue("queryVisible",false);

    settings.setValue(tr("nviews"),views.length());
    settings.setValue(tr("cview"),cview);
    for (j = 0; j < views.length(); j++)
      settings.setValue(tr("view.%1").arg(j),views[j]);
  settings.endGroup();

  openDialog->writeSettings(settings);
  paletteDialog->writeSettings(settings);
}

int MainWindow::warning(const QString& message, QWidget *parent, MessageKind kind,
                        const QString& label1, const QString& label2, const QString& label3)
{ QPixmap     *symbol;
  QPushButton *button1 = 0, *button2 = 0, *button3 = 0;

  QMessageBox msg(QMessageBox::Critical, QObject::tr("Image-Tagger"),
                  message, 0, parent,
                  Qt::Dialog | Qt::FramelessWindowHint);

  msg.setAutoFillBackground(true);
  msg.setPalette(QPalette(QColor(255,255,180,255)));
  if (kind == INFORM)
    symbol = new QPixmap(":/images/inform.png","PNG");
  else if (kind == WARNING)
    symbol = new QPixmap(":/images/warning.png","PNG");
  else //  kind == ERROR
    symbol = new QPixmap(":/images/error.png","PNG");
  msg.setIconPixmap(*symbol);

  if (label1.isEmpty())
    button1 = msg.addButton(QObject::tr("OK"),QMessageBox::AcceptRole);
  else
    { button1 = msg.addButton(label1,QMessageBox::AcceptRole);
      if ( ! label2.isEmpty())
        { button2 = msg.addButton(label2,QMessageBox::RejectRole);
          button2->setFocusPolicy(Qt::StrongFocus);
          msg.setTabOrder(button1,button2);
        }
      if ( ! label3.isEmpty())
        { button3 = msg.addButton(label3,QMessageBox::RejectRole);
          button3->setFocusPolicy(Qt::StrongFocus);
          msg.setTabOrder(button2,button3);
        }
    }
  button1->setFocusPolicy(Qt::StrongFocus);   //  Mac buttons are not tabable by default
  button1->setFocus(Qt::OtherFocusReason);    //    so ensure that it is the same on all gui's.

  QDialogButtonBox *bbox = msg.findChild<QDialogButtonBox*>();  //  Motif and CDE cutoff the focus
  bbox->layout()->setMargin(2);                                 //    border without this

  msg.exec();

  if (msg.clickedButton() == button1)
    return (0);
  else if (msg.clickedButton() == button2)
    return (1);
  else
    return (2);
}
