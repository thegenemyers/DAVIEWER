#include <stdio.h>
#include <math.h>

#include <QtGui>

extern "C" {
#include "align.h"
#include "libfastk.h"
#include "piler.h"
#include "doter.h"
}

#include "prof_window.h"
#include "main_window.h"

#define PROF_MIN_WIDTH   500   //  Minimum display window width & height
#define PROF_MIN_HEIGHT  350

#define POS_TICKS   1000000000    //  Resolution of sliders
#define REAL_TICKS  1000000000.

#define MIN_HORZ_VIEW    50    //  Minimum bp in a view
#define MIN_VERT_VIEW    20    //  Minimum rows in a view

#define MIN_FEATURE_SIZE  3.   //  Minimum pixel length to display a feature

QRect *ProfWindow::screenGeometry = 0;
int    ProfWindow::frameWidth;
int    ProfWindow::frameHeight;


/*****************************************************************************\
*
*  PROF CANVAS
*
\*****************************************************************************/

int ProfCanvas::pick(int x, int y)
{ ProfScroll *scroll  = (ProfScroll *) parent();

  (void) scroll;
  (void) x;
  (void) y;

  return (1);
}

ProfCanvas::ProfCanvas(Profile *pro, QWidget *parent) : QWidget(parent)
{ buttonDown = false;
  prof       = pro;
}

void ProfCanvas::mouseMoveEvent(QMouseEvent *event)
{ if (buttonDown)
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
      return;
    }
}

void ProfCanvas::mouseReleaseEvent(QMouseEvent *event)
{ (void) event;
  buttonDown = false;
  setCursor(Qt::ArrowCursor);
}

void ProfCanvas::mousePressEvent(QMouseEvent *event)
{ ProfScroll *scroll  = (ProfScroll *) parent();

  int i, page, hpos;

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

void ProfCanvas::paintEvent(QPaintEvent *event)
{ QPainter       painter(this);
  ProfScroll    *scroll  = (ProfScroll *) parent();
  Palette_State *palette = &(MainWindow::palette);

  QWidget::paintEvent(event);

  //  Paint background
  
  painter.setBrush(QBrush(palette->backColor));
  painter.drawRect(rect());

  QScrollBar *vscroll, *hscroll;
  int      vmax, hmax;
  int      high, wide;
  double   vbeg, vend;
  double   hbeg, hend;
  int      ibeg, iend;
  double   hbp, vbp;
  int      page;

  vscroll = scroll->vertScroll;   //  Mapping is Pixel(x,y) = ((x-hbeg)*hbp, (y-vbeg)*vbp)
  hscroll = scroll->horzScroll;

  wide    = rect().width() - 40;
  high    = rect().height() - 40;

  hmax    = scroll->hmax;
  page    = hscroll->pageStep();
  hbeg    = (hscroll->value() / REAL_TICKS) * hmax;
  hend    = hbeg + (page / REAL_TICKS) * hmax;
  hbp     = wide / (1.*(hend-hbeg));

  vmax    = scroll->vmax;
  page    = vscroll->pageStep();
  vbeg    = ((POS_TICKS - (vscroll->value() + page)) / REAL_TICKS) * vmax;
  vend    = vbeg + (page / REAL_TICKS) * vmax;
  vbp     = high / (1.*(vend-vbeg));

  ibeg = hbeg;

  vbeg   += (high+20.)/vbp;
  hbeg   -= 20./hbp;

  { int  p, q, g, h;
    QPen cPen, dPen;
    uint16 *cnts = prof->cnts;

    cPen.setColor(QColor(255,255,255));
    cPen.setWidth(2);
    painter.setPen(cPen);

    p = (vbeg - 0x7fff)*vbp;
    if (p < 20)
      if (p < 0)
        { vmax = 0;
          painter.drawLine(20,0,20,high+20);
        }
      else
        { vmax = p;
          painter.drawLine(20,p,20,high+20);
        }
    else
      { vmax = 20;
        painter.drawLine(20,20,20,high+20);
      }

    p = (prof->plen-hbeg)*hbp;
    if (p > wide+20)
      if (p > wide+40)
        { hmax = wide+40;
          iend = (wide+40)/hbp + hbeg + 1;
        }
      else
        { hmax = p;
          iend = prof->plen;
        }
    else
      { hmax = wide+20;
        iend = prof->plen;
      }
    painter.drawLine(20,high+20,hmax,high+20);

    { int x, y, u;

      u = divide_bar((int) 100/vbp);
      x = (((int) (vbeg - vmax/vbp)) / units[u]) * units[u];
      y = (((int) (vbeg - (high+20)/vbp)) / units[u] + 1) * units[u];
      painter.save();
      painter.rotate(-90.);
      for (; x >= y; x -= units[u])
        { g = (vbeg - x)*vbp;
          painter.drawLine(-g,17,-g,20);
          painter.drawText(QRect(-g-10,12,20,4),Qt::AlignCenter|Qt::AlignBottom|Qt::TextDontClip,
                           tr("%1%2").arg(x/divot[u]).arg(suffix[u]));
        }
      painter.restore();

      u = divide_bar((int) 100/hbp);
      x = (((int) (hbeg + 20/hbp)) / units[u] + 1) * units[u];
      y = (((int) (hbeg + (hmax)/hbp)) / units[u] + 1) * units[u];
      for (; x <= y; x += units[u])
        { g = (x - hbeg)*hbp;
          painter.drawLine(g,high+20,g,high+23);
          painter.drawText(QRect(g-10,high+28,20,4),
                           Qt::AlignCenter|Qt::AlignTop|Qt::TextDontClip,
                           tr("%1%2").arg(x/divot[u]).arg(suffix[u]));
        }
    }

    painter.setClipRect(19,vmax-1,hmax-19,(high+21)-vmax,Qt::ReplaceClip);

    dPen.setColor(QColor(96,96,96));
    dPen.setDashPattern(QVector<qreal>()<<2.<<4.);
    dPen.setWidth(1);
    painter.setPen(dPen);
    h = (vbeg - cnts[ibeg-1])*vbp;
    for (p = ibeg; p < iend; p++)
      { g = (vbeg - cnts[p])*vbp;
        q = (p-hbeg)*hbp;
        painter.drawLine(q,h,q,g);
        h = g;
      }

    cPen.setColor(QColor(0,255,255));
    cPen.setWidth(1);
    painter.setPen(cPen);
    for (p = ibeg; p < iend; p++)
      { g = (vbeg - cnts[p])*vbp;
        q = (p-hbeg)*hbp;
        painter.drawLine(q,g,q+hbp,g);
      }
  }
}


/*****************************************************************************\
*
*  PROF SCROLL
*
\*****************************************************************************/

ProfScroll::ProfScroll(Profile *prof, QWidget *parent) : QWidget(parent)
{
  profcanvas = new ProfCanvas(prof,this);

  vertScroll = new QScrollBar(Qt::Vertical,this);
    vertScroll->setTracking(false);
    vertScroll->setMinimum(0);
    vertScroll->setMaximum(0);
    vertScroll->setPageStep(POS_TICKS);
    vertScroll->setValue(0);

  vertZoom = new QSlider(Qt::Vertical,this);
    vertZoom->setTracking(false);
    vertZoom->setMinimum(0);
    vertZoom->setMaximum(SIZE_TICKS);
    vertZoom->setValue(SIZE_TICKS);
    vsize = SIZE_TICKS;

    QHBoxLayout *vertControl = new QHBoxLayout();
      vertControl->addWidget(vertScroll);
      vertControl->addWidget(vertZoom);
      vertControl->setSpacing(2);
      vertControl->setMargin(5);

  horzScroll = new QScrollBar(Qt::Horizontal,this);
    horzScroll->setTracking(false);
    horzScroll->setMinimum(0);
    horzScroll->setMaximum(0);
    horzScroll->setPageStep(POS_TICKS);
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
      drawPanel->addWidget(profcanvas,   0,0);
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

  { int    i;
    double coef;
    double base;

    vmax = 0x7fff;
    hmax = prof->plen;

    coef = (1.*MIN_HORZ_VIEW)/hmax;
    base = pow(1./coef,1./SIZE_TICKS);
    for (i = 0; i < SIZE_TICKS; i++)
      hstep[i] = POS_TICKS * (pow(base,i) * coef);
    hstep[SIZE_TICKS] = POS_TICKS;
  
    coef = (1.*MIN_VERT_VIEW)/vmax;
    base = pow(1./coef,1./SIZE_TICKS);
    for (i = 0; i < SIZE_TICKS; i++)
      vstep[i] = POS_TICKS * (pow(base,i) * coef);
    vstep[SIZE_TICKS] = POS_TICKS;
  }

  connect(vertScroll,SIGNAL(valueChanged(int)),this,SLOT(vsValue(int)));
  connect(vertZoom,SIGNAL(valueChanged(int)),this,SLOT(vsSize(int)));

  connect(horzScroll,SIGNAL(valueChanged(int)),this,SLOT(hsValue(int)));
  connect(horzZoom,SIGNAL(valueChanged(int)),this,SLOT(hsSize(int)));
}


void ProfScroll::vsValue(int value)
{ (void) value;
  profcanvas->update();
}

void ProfScroll::vsSize(int size)
{ int page, xp;

  xp = POS_TICKS - (vertScroll->value() + vertScroll->pageStep());
  page = vstep[size];
  vertScroll->setMaximum(POS_TICKS-page);
  vertScroll->setPageStep(page);
  vertScroll->setValue(POS_TICKS - (xp + page));
  vsize = size;
  profcanvas->update();
}

void ProfScroll::hsValue(int value)
{ (void) value;
  profcanvas->update();
}

void ProfScroll::hsSize(int size)
{ int page;

  page = hstep[size];
  horzScroll->setMaximum(POS_TICKS-page);
  horzScroll->setPageStep(page);
  profcanvas->update();
}

void ProfScroll::hsToRange(int beg, int end)
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
  profcanvas->update();
}


/*****************************************************************************\
*
*  PROFILE WINDOW
*
\*****************************************************************************/

ProfState ProfWindow::lastState;
bool      ProfWindow::firstTime = true;

ProfWindow::~ProfWindow()
{ free(prof.aseq-1);
  free(prof.cnts);
}

ProfWindow::ProfWindow(QString title, int alen, char *aseq, int plen, uint16 *cnts) : QMainWindow()
{ if (firstTime)
    { state.wGeom = QRect(0,0,750,400);

      firstTime = false;
      lastState = state;
    }
  else if ( ! MainWindow::profs.isEmpty())
    state = MainWindow::profs.last()->state;
  else
    state = lastState;

  prof.alen = alen;
  prof.aseq = aseq;
  prof.kmer = alen-plen;
  prof.plen = plen;
  prof.cnts = cnts;

  profscroll = new ProfScroll(&prof,this);
  profscroll->hsToRange(0,plen);

  drawPanel = new QVBoxLayout();
    drawPanel->addWidget(profscroll,1);
    drawPanel->setSpacing(0);
    drawPanel->setMargin(0);

  QWidget *centralFrame = new QWidget();
    centralFrame->setLayout(drawPanel);
  setCentralWidget(centralFrame);

  pushState();

  setWindowTitle(title);
  setMinimumSize(PROF_MIN_WIDTH,PROF_MIN_HEIGHT);

  createActions();
  createMenus();
}

void ProfWindow::createActions()
{ fullScreenAct = new QAction(tr("&Full Screen"), this);
    fullScreenAct->setShortcut(tr("Ctrl+F"));
    fullScreenAct->setToolTip(tr("Expand window to full screen"));

  minimizeAct = new QAction(tr("&Minimize"), this);
    minimizeAct->setShortcut(tr("Ctrl+M"));
    minimizeAct->setToolTip(tr("Minimize the current window"));

  connect(fullScreenAct, SIGNAL(triggered()), this, SLOT(fullScreen()));
  connect(minimizeAct, SIGNAL(triggered()), this, SLOT(showMinimized()));
}

void ProfWindow::createMenus()
{ QMenuBar *bar = menuBar();

  QMenu *windowMenu = bar->addMenu(tr("&Window"));
    windowMenu->addAction(fullScreenAct);
    windowMenu->addAction(minimizeAct);
/*
    windowMenu->addSeparator();
    windowMenu->addAction(manager->unminimizeAllAct);
    windowMenu->addAction(manager->raiseAllAct);
*/
}

void ProfWindow::pushState()
{ setGeometry(state.wGeom);
}


void ProfWindow::fullScreen()
{ if (isMaximized())
    { fullScreenAct->setToolTip(tr("Expand window to full screen"));
      fullScreenAct->setText(tr("Full screen"));
      showNormal();
    }
  else
    { fullScreenAct->setToolTip(tr("Return window to previous size"));
      fullScreenAct->setText(tr("Previous size"));
      showMaximized();
    }
}

void ProfWindow::closeEvent(QCloseEvent *event)
{ QList<ProfWindow *> profs = MainWindow::profs;
  int i;

  state.wGeom = geometry();
  lastState = state;
  firstTime = false;

  for (i = 0; i < profs.length(); i++)
    if (profs[i] == this)
      { profs.removeAt(i);
        break;
      }
  event->accept();
}
