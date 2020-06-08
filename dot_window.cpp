#include <stdlib.h>
#include <math.h>

#include <QtGui>

extern "C" {
#include "DB.h"
#include "float.h"
#include "doter.h"
}

#undef DEBUG

#include "dot_window.h"
#include "main_window.h"


/***************************************************************************************/
/*                                                                                     */
/*   CONSTANTS & CUSTOMIZED WIDGETS                                                    */
/*                                                                                     */
/***************************************************************************************/

#define CANVAS_MARGIN 20

#define LOCATOR_RECTANGLE_SIZE  100

static QRect *screenGeometry = NULL;


MarginWidget::MarginWidget(QWidget *parent, bool vertical) : QFrame(parent)
{ if (vertical)
    { setFixedHeight(CANVAS_MARGIN);
      setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    }
  else
    { setFixedWidth(CANVAS_MARGIN);
      setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
    }

  setFrameShape(QFrame::NoFrame);
  setAutoFillBackground(true);

  QPalette p = palette();
    p.setColor(QPalette::Active,QPalette::Window,QColor(0,0,0));
    p.setColor(QPalette::Inactive,QPalette::Window,QColor(0,0,0));
    setPalette(p);
}


DotLineEdit::DotLineEdit(QWidget *parent) : QLineEdit(parent) {}

void DotLineEdit::setChain(QWidget *p, QWidget *s)
{ mypred = p;
  mysucc = s;
}

void DotLineEdit::focusOutEvent(QFocusEvent *event)
{ printf("Lost focus %d\n",event->reason());
  if (event->reason() <= Qt::BacktabFocusReason)
    emit focusOut();
  QLineEdit::focusOutEvent(event);
}

void DotLineEdit::keyPressEvent(QKeyEvent *event)
{ int key = event->key();
  if (key == Qt::Key_Return || key == Qt::Key_Tab || key == Qt::Key_Backtab)
    { if ((event->modifiers() & Qt::ShiftModifier) != 0)
        mypred->setFocus();
      else
        mysucc->setFocus();
      printf("key\n");
    }
  else
    QLineEdit::keyPressEvent(event);
}

bool DotLineEdit::focusNextPrevChild(bool next)
{ (void) next;
  return (false);
}


/***************************************************************************************/
/*                                                                                     */
/*   DOT CANVAS                                                                        */
/*                                                                                     */
/***************************************************************************************/

DotCanvas::DotCanvas(QWidget *parent) : QWidget(parent)
{ setSizePolicy(QSizePolicy::MinimumExpanding,QSizePolicy::MinimumExpanding);
  noFrame = true;
  rubber  = new QRubberBand(QRubberBand::Rectangle, this);
  timer   = new QBasicTimer();
}

Frame *DotCanvas::shareData(DotState *s, DotPlot *p)
{ state = s;
  plot  = p;
  return (&frame);
}

#define ASSIGN(frame,fx,fy,fw,fh)	\
{ frame.x = fx;				\
  frame.y = fy;				\
  frame.w = fw;				\
  frame.h = fh;				\
}

bool DotCanvas::zoomView(double zoomDel)
{ double vX = frame.x;
  double vY = frame.y;
  double vW = frame.w;
  double vH = frame.h;
  double nW = vW / zoomDel;
  double nH = vH / zoomDel;
  if (nW > plot->Alen && nH > plot->Blen)
    { if (plot->Alen/nW > plot->Blen/nH)
        { nH = nH*(plot->Alen/nW);
          nW = plot->Alen;
        }
      else
        { nW = nW*(plot->Blen/nH);
          nH = plot->Blen;
        }
    }
  double nX = vX + (vW-nW)/2;
  double nY = vY + (vH-nH)/2;
  if (nW > plot->Alen)
    nX = (plot->Alen-nW)/2.;
  else
    { if (nX < 0.) nX = 0.;
      if (nX + nW > plot->Alen) nX = plot->Alen - nW; 
    }
  if (nH > plot->Blen)
    nY = (plot->Blen-nH)/2.;
  else
    { if (nY < 0.) nY = 0.;
      if (nY + nH > plot->Blen) nY = plot->Blen - nH; 
    }

  if (nW < rectW)
    return (false);

#ifdef DEBUG
  printf("Frame = (%.1f,%.1f) %.1f vs %.1f\n",nX,nY,nW,nH);
#endif

  ASSIGN(frame,nX,nY,nW,nH);
  
  double zW = (1.*plot->Alen) / nW;
  double zH = (1.*plot->Blen) / nH;
  if (zW > zH)
    emit NewFrame(zW);
  else
    emit NewFrame(zH);

  return (true);
}

void DotCanvas::resizeEvent(QResizeEvent *event)
{ QSize oldS = event->oldSize();
  QSize newS = event->size();

  rectW = newS.width();
  rectH = newS.height();

  if (noFrame)
    { event->accept();
      return;
    }

  double vX = frame.x;
  double vY = frame.y;
  double vW = frame.w;
  double vH = frame.h;

  int oW = oldS.width();
  int nW = newS.width();
  int oH = oldS.height();
  int nH = newS.height();

  vW += (vW / oW) * (nW-oW);
  vH += (vH / oH) * (nH-oH);
  if (vW >= plot->Alen && vH >= plot->Blen)
    { if (plot->Alen/vW > plot->Blen/vH)
        { vH = vH*(plot->Alen/vW);
          vW = plot->Alen;
          vX = 0;
          vY = (plot->Blen-vH)/2.;
        }
      else
        { vW = vW*(plot->Blen/vH);
          vH = plot->Blen;
          vX = (plot->Alen-vW)/2.;
          vY = 0;
        }
    }
  else
    { if (vW > plot->Alen)
        vX = (plot->Alen-vW)/2.;
      else if (vX < 0)
        vX = 0.;
      if (vH > plot->Blen)
        vY = (plot->Blen-vH)/2.;
      else if (vY < 0.)
        vY = 0.;
    }

  ASSIGN(frame,vX,vY,vW,vH);

  event->accept();

  double zW = (1.*plot->Alen) / vW;
  double zH = (1.*plot->Blen) / vH;
  if (zW > zH)
    emit NewFrame(zW);
  else
    emit NewFrame(zH);
}

void DotCanvas::resetView()
{ double vX, vY, vW, vH;

  int rectW = width();
  int rectH = height();
  double h = (1.*rectW)/plot->Alen;
  double v = (1.*rectH)/plot->Blen;

  if (h > v)
    { vY = 0;
      vH = plot->Blen;
      vW = rectW / v;
      vX = (plot->Alen - vW) / 2.;
    }
  else
    { vX = 0;
      vW = plot->Alen;
      vH = rectH / h;
      vY = (plot->Blen - vH) / 2.;
    }

  ASSIGN(frame,vX,vY,vW,vH);

#ifdef DEBUG
  printf("Reset frame = (%.1f,%.1f) %.8f vs %.8f\n",vX,vY,vW,vH);
#endif
}

bool DotCanvas::viewToFrame()
{ double vX, vY, vW, vH;

  double h = (1.*rectW)/state->view.width();
  double v = (1.*rectH)/state->view.height();

  if (h > v)
    { vY = state->view.y();
      vH = state->view.height();
      vW = rectW / v;
      if (vW > plot->Alen)
        vX = (plot->Alen - vW) / 2.;
      else
        { vX = state->view.x() + (state->view.width() - vW) / 2.;
          if (vX < 0.)
            vX = 0.;
          if (vX + vW > plot->Alen)
            vX = plot->Alen - vW;
        }
    }
  else
    { vX = state->view.x();
      vW = state->view.width();
      vH = rectH / h;
      if (vH > plot->Blen)
        vY = (plot->Blen - vH) / 2.;
      else
        { vY = state->view.y() + (state->view.height() - vH) / 2.;
          if (vY < 0.)
            vY = 0.;
          if (vY + vH > plot->Blen)
            vY = plot->Blen - vH;
        }
    }

  if (vW < rectW)
    return (false);

  ASSIGN(frame,vX,vY,vW,vH);

#ifdef DEBUG
  printf("view 2 frame = (%.1f,%.1f) %.8f vs %.8f\n",vX,vY,vW,vH);
#endif

  double zW = (1.*plot->Alen) / vW;
  double zH = (1.*plot->Blen) / vH;
  if (zW > zH)
    emit NewFrame(zW);
  else
    emit NewFrame(zH);

  return (true);
}

void DotCanvas::enterEvent(QEvent *)
{ QApplication::setOverrideCursor(Qt::ArrowCursor);
  setFocus();
#ifdef DEBUG
  printf("Enter\n");
#endif
}

void DotCanvas::leaveEvent(QEvent *)
{ QApplication::restoreOverrideCursor();
  clearFocus();
#ifdef DEBUG
  printf("Exit\n");
#endif
}

void DotCanvas::keyPressEvent(QKeyEvent *event)
{ if ((event->modifiers() & Qt::ShiftModifier) != 0)
    QApplication::changeOverrideCursor(Qt::CrossCursor);
  else
    QApplication::changeOverrideCursor(Qt::ArrowCursor);
#ifdef DEBUG
  printf("Key Press\n");
#endif
}

void DotCanvas::keyReleaseEvent(QKeyEvent *event)
{ if ((event->modifiers() & Qt::ShiftModifier) != 0)
    QApplication::changeOverrideCursor(Qt::CrossCursor);
  else
    QApplication::changeOverrideCursor(Qt::ArrowCursor);
#ifdef DEBUG
  printf("Key Release\n");
#endif
}

void DotCanvas::mousePressEvent(QMouseEvent *event)
{ mouseX = event->x();
  mouseY = event->y();
#ifdef DEBUG
  printf("Press %d %d\n",mouseX,mouseY);
#endif
  if ((QApplication::keyboardModifiers() & Qt::ShiftModifier) != 0)
    { select = true;
      rubber->setGeometry(QRect(mouseX,mouseY,0,0));
      rubber->show();
      QApplication::changeOverrideCursor(Qt::CrossCursor);
    }
  else
    { select = false;
      nograb = true;
      scaleX = frame.w / width();
      scaleY = frame.h / height();
    }
}

void DotCanvas::mouseMoveEvent(QMouseEvent *event)
{ xpos = event->x();
  ypos = event->y();
  if (select)
    rubber->setGeometry(QRect(mouseX,mouseY,xpos-mouseX,ypos-mouseY).normalized());
  else
    { if (nograb)
        { if (abs(xpos-mouseX) <= 1 && abs(ypos-mouseY) <= 1)
            return;
          QApplication::changeOverrideCursor(Qt::ClosedHandCursor);
          nograb = false;
        }
      if (frame.w < plot->Alen)
        { frame.x += scaleX * (mouseX - xpos);
          if (frame.x < 0.)
            frame.x = 0.;
          if (frame.x + frame.w > plot->Alen)
            frame.x = plot->Alen - frame.w;
        }
      if (frame.h < plot->Blen)
        { frame.y += scaleY * (mouseY - ypos);
          if (frame.y < 0.)
            frame.y = 0.;
          if (frame.y + frame.h > plot->Blen)
            frame.y = plot->Blen - frame.h;
        }
      mouseX = xpos;
      mouseY = ypos;

      update();
      emit NewFrame(0.);
    }
}

void DotCanvas::mouseReleaseEvent(QMouseEvent *event)
{ if (select)
    { rubber->hide();
#ifdef DEBUG
      printf("Selected\n");
#endif
      QRect reg  = rubber->geometry();
      QRect undo = state->view;
      int xb = frame.x + (reg.x()/(1.*rectW))*frame.w;
      int yb = frame.y + (reg.y()/(1.*rectH))*frame.h;
      int xe = frame.x + ((reg.x()+reg.width())/(1.*rectW))*frame.w;
      int ye = frame.y + ((reg.y()+reg.height())/(1.*rectH))*frame.h;
      if (xb < 0) xb = 0;
      if (yb < 0) yb = 0;
      if (xe > plot->Alen) xe = plot->Alen;
      if (ye > plot->Blen) ye = plot->Blen;
      state->view = QRect(xb,yb,xe-xb,ye-yb);
#ifdef DEBUG
      printf("Region select (%d,%d) %d x %d\n",reg.x(),reg.y(),reg.width(),reg.height());
      printf("      = view  (%d,%d) (%d,%d)\n",xb,yb,xe,ye);
#endif
      if (viewToFrame())
        update();
      else
        state->view = undo;
    }
  else if (nograb)
    { double ex = event->x();
      double ey = event->y();
#ifdef DEBUG
      printf("Clicked\n");
#endif
      int x = frame.x + (ex/rectW)*frame.w;
      int y = frame.y + (ey/rectH)*frame.h;
      state->focus = QPoint(x,y);
      update();
      emit NewFocus();
    }

  if ((QApplication::keyboardModifiers() & Qt::ShiftModifier) != 0)
    QApplication::changeOverrideCursor(Qt::CrossCursor);
  else
    QApplication::changeOverrideCursor(Qt::ArrowCursor);
}

void DotCanvas::paintEvent(QPaintEvent *event)
{ QPainter painter;

  (void) event;

  painter.begin(this);

  if (noFrame)    //  First paint => set Frame (cannot do earlier as do not know size)
    { resetView();
      noFrame = false;
    }

  double vX = frame.x;
  double vY = frame.y;
  double vW = frame.w;
  double vH = frame.h;

  double xa = rectW/vW;
  double xb = -vX*(rectW/vW);

  double ya = rectH/vH;
  double yb = -vY*(rectH/vH);

  QPen dPen;

  painter.fillRect(0,0,rectW,rectH,QBrush(QColor(0,0,0)));

  dPen.setColor(QColor(255,255,255));
  dPen.setWidth(2);
  painter.setPen(dPen);

  scale_plot(plot->dots,&frame,rectW,rectH);

  for (int i = 500; i < plot->Alen; i += 500)
    painter.drawLine(xa*i+xb,yb,xa*i+xb,ya*plot->Blen+yb);
  for (int j = 0; j < plot->Blen; j += 500)
    painter.drawLine(xb,ya*j+yb,xa*plot->Alen+xb,ya*j+yb);

  dPen.setColor(QColor(255,255,0));
  painter.setPen(dPen);

  painter.drawLine(xb+1,yb,xb+1,ya*plot->Blen+yb);
  painter.drawLine(xa*plot->Alen+xb-1,yb,xa*plot->Alen+xb-1,ya*plot->Blen+yb);
  painter.drawLine(xb,yb+1,xa*plot->Alen+xb,yb+1);
  painter.drawLine(xb,ya*plot->Blen+yb-1,xa*plot->Alen+xb,ya*plot->Blen+yb-1);

  if (state->lViz == Qt::Checked)
    if (vW < plot->Alen*.7 || vH < plot->Blen*.7)
      { int lX, lY;
        int lW, lH;
	int x, y, w, h;

        painter.setPen(state->lColor);

        if (plot->Alen > plot->Blen)
          { lW = LOCATOR_RECTANGLE_SIZE;
            lH = LOCATOR_RECTANGLE_SIZE*((1.*plot->Blen)/plot->Alen);
          }
        else
          { lH = LOCATOR_RECTANGLE_SIZE;
            lW = LOCATOR_RECTANGLE_SIZE*((1.*plot->Alen)/plot->Blen);
          }
            
        if (state->lQuad%2)
          lX = rectW - (lW + 5);
        else
          lX = 5;
        if (state->lQuad < 2)
          lY = 5;
        else
          lY = rectH - (lH + 5);

        painter.drawRect(lX,lY,lW,lH);

        if (vX < 0)
          { x = 0;
            w = lW;
          }
        else
          { x = lW*(vX/plot->Alen);
            w = lW*(vW/plot->Alen); 
          }
        if (vY < 0)
          { y = 0;
            h = lH;
          }
        else
          { y = lH*(vY/plot->Blen);
            h = lH*(vH/plot->Blen); 
          }

        painter.drawRect(lX+x,lY+y,w,h);
      }

  if (state->focus != QPoint(0,0))
    { painter.setPen(state->fColor);
      int x = xa*state->focus.x() + xb;
      int y = ya*state->focus.y() + yb;
      if (state->fViz == Qt::Checked)
        { painter.drawLine(0,y,rectW,y);
          painter.drawLine(x,0,x,rectH);
        }
      else
        { painter.drawLine(x-10,y,x+10,y);
          painter.drawLine(x,y-10,x,y+10);
        }
    }

  painter.end();
}


/***************************************************************************************/
/*                                                                                     */
/*   DOT WINDOW DATA TYPE                                                              */
/*                                                                                     */
/***************************************************************************************/

DotState DotWindow::lastState;
bool     DotWindow::firstTime = true;

DotWindow::~DotWindow()
{ free_dots(plot.dots);
  free(plot.Bseq-1);
  free(plot.Aseq-1);
}

DotWindow::DotWindow(QString title, int alen, char *aseq, int blen, char *bseq) : QMainWindow()
{ if (firstTime)
    { state.kmer = 10;

      state.fColor = QColor(255,255,0);
      state.fViz   = Qt::Unchecked;
      state.sViz   = Qt::Unchecked;

      state.lColor = QColor(255,0,255);
      state.lViz   = Qt::Checked;
      state.lQuad  = TOP_LEFT;
      state.wGeom  = QRect(0,0,600,400);

      firstTime = false;
      lastState = state;
    }
  else
    state = lastState;

  state.view.setRect(0,0,alen,blen);
  state.zoom = 1.0;
  state.focus = QPoint(0,0);

  plot.Alen = alen;
  plot.Blen = blen;
  plot.Aseq = aseq;
  plot.Bseq = bseq;
  plot.dots = build_dots(plot.Alen,plot.Aseq,plot.Blen,plot.Bseq,state.kmer);

      canvas = new DotCanvas(this);

      frame = canvas->shareData(&state,&plot);

      MarginWidget *leftMargin   = new MarginWidget(this,false);
      MarginWidget *rightMargin  = new MarginWidget(this,false);
      MarginWidget *topMargin    = new MarginWidget(this,true);
      MarginWidget *bottomMargin = new MarginWidget(this,true);

    QHBoxLayout *imageMargin = new QHBoxLayout;
      imageMargin->addWidget(leftMargin,0);
      imageMargin->addWidget(canvas,1);
      imageMargin->addWidget(rightMargin,0);
      imageMargin->setSpacing(0);
      imageMargin->setMargin(0);

  QVBoxLayout *imageLayout = new QVBoxLayout;
    imageLayout->addWidget(topMargin,0);
    imageLayout->addLayout(imageMargin,1);
    imageLayout->addWidget(bottomMargin,0);
    imageLayout->setSpacing(0);
    imageLayout->setMargin(0);

      QLabel *zoomLabel = new QLabel(tr("Zoom: "));

      QToolButton *zoomIn = new QToolButton();
        zoomIn->setToolButtonStyle(Qt::ToolButtonTextOnly);
        zoomIn->setText(tr("+"));
        zoomIn->setFixedSize(26,26);
        zoomIn->setToolTip(tr("Click to zoom a notch.  Double click to\n") +
                       tr("zoom a notch and expand window to\n") +
                       tr("maximum size (if it fits)."));

      QDoubleValidator *zVal = new QDoubleValidator(0.,DBL_MAX,2,this);

      zoomEdit = new DotLineEdit(this);
        zoomEdit->setFixedHeight(24);
        zoomEdit->setFixedWidth(73);
        zoomEdit->setFrame(false);
        zoomEdit->setFont(QFont(tr("Monaco")));
        zoomEdit->setAlignment(Qt::AlignCenter);
        zoomEdit->setValidator(zVal);

      QToolButton *zoomOut = new QToolButton();
        zoomOut->setToolButtonStyle(Qt::ToolButtonTextOnly);
        zoomOut->setText(tr("-"));
        zoomOut->setFixedSize(26,26);

      QToolButton *zoom0 = new QToolButton();
        zoom0->setToolButtonStyle(Qt::ToolButtonTextOnly);
        zoom0->setText(tr("1"));
        zoom0->setFixedSize(26,26);
    
  QHBoxLayout *zoomLayout = new QHBoxLayout;
    zoomLayout->addWidget(zoomLabel);
    zoomLayout->addSpacing(10);
    zoomLayout->addWidget(zoomOut);
    zoomLayout->addWidget(zoomEdit);
    zoomLayout->addWidget(zoomIn);
    zoomLayout->addSpacing(10);
    zoomLayout->addWidget(zoom0);
    zoomLayout->addStretch(1);
    zoomLayout->setSpacing(0);

      QLabel *kmerLabel = new QLabel(tr("K-mer: "));

      kmerSpin = new QSpinBox();
        kmerSpin->setMinimum(5);
        kmerSpin->setMaximum(32);
        kmerSpin->setSingleStep(1);
        kmerSpin->setValue(10);
        kmerSpin->setFixedWidth(40);
        kmerSpin->setAlignment(Qt::AlignRight);
        kmerSpin->setFocusPolicy(Qt::NoFocus);

  QHBoxLayout *kmerLayout = new QHBoxLayout;
    kmerLayout->addWidget(kmerLabel);
    kmerLayout->addSpacing(5);
    kmerLayout->addWidget(kmerSpin);
    kmerLayout->addStretch(1);
    kmerLayout->setSpacing(0);

      QLabel *viewLabel = new QLabel(tr("View: "));
      QLabel *Alabel = new QLabel(tr("A: "));

      Arng = new DotLineEdit(this);
        Arng->setFixedHeight(24);
        Arng->setFixedWidth(125);
        Arng->setFrame(false);
        Arng->setFont(QFont(tr("Monaco")));
        Arng->setAlignment(Qt::AlignCenter);

  QHBoxLayout *viewLayoutA = new QHBoxLayout;
    viewLayoutA->addWidget(viewLabel);
    viewLayoutA->addWidget(Alabel);
    viewLayoutA->addWidget(Arng);
    viewLayoutA->addStretch(1);
    viewLayoutA->setSpacing(0);

      QLabel *Blabel = new QLabel(tr("B: "));

      Brng = new DotLineEdit(this);
        Brng->setFixedHeight(24);
        Brng->setFixedWidth(125);
        Brng->setFrame(false);
        Brng->setFont(QFont(tr("Monaco")));
        Brng->setAlignment(Qt::AlignCenter);

      QToolButton *Rpush = new QToolButton();
        Rpush->setToolButtonStyle(Qt::ToolButtonTextOnly);
        Rpush->setText(tr("="));
        Rpush->setFixedSize(26,26);
        Rpush->setFocusPolicy(Qt::NoFocus);

  QHBoxLayout *viewLayoutB = new QHBoxLayout;
    viewLayoutB->addSpacing(37);
    viewLayoutB->addWidget(Blabel);
    viewLayoutB->addWidget(Brng);
    viewLayoutB->addSpacing(10);
    viewLayoutB->addWidget(Rpush);
    viewLayoutB->addStretch(1);
    viewLayoutB->setSpacing(0);

      QLabel *posLabel = new QLabel(tr("Focus:  "));

      Fpnt = new DotLineEdit(this);
        Fpnt->setFixedHeight(24);
	Fpnt->setFixedWidth(130);
	Fpnt->setFrame(false);
        Fpnt->setFont(QFont(tr("Monaco")));
        Fpnt->setAlignment(Qt::AlignCenter);

      QToolButton *Fpush = new QToolButton();
        Fpush->setToolButtonStyle(Qt::ToolButtonTextOnly);
        Fpush->setText(tr("="));
        Fpush->setFixedSize(26,26);

  QHBoxLayout *cursorLayout = new QHBoxLayout;
    cursorLayout->addWidget(posLabel);
    cursorLayout->addWidget(Fpnt);
    cursorLayout->addSpacing(10);
    cursorLayout->addWidget(Fpush);
    cursorLayout->addStretch(1);
    cursorLayout->setSpacing(0);

      focusBox = new QToolButton();
        focusBox->setIconSize(QSize(16,16));
        focusBox->setFixedSize(20,20);

      focusCheck = new QCheckBox(tr("Cross Hairs"));
    
  QHBoxLayout *focusLayout = new QHBoxLayout;
    focusLayout->addSpacing(48);
    focusLayout->addWidget(focusBox);
    focusLayout->addSpacing(10);
    focusLayout->addWidget(focusCheck);
    focusLayout->addStretch(1);
    focusLayout->setSpacing(0);
    
      seqCheck = new QCheckBox(tr("View sequences"));

  QHBoxLayout *seqLayout = new QHBoxLayout;
    seqLayout->addSpacing(48);
    seqLayout->addWidget(seqCheck);
    seqLayout->addStretch(1);
    seqLayout->setSpacing(0);

      QLabel *locatorLabel = new QLabel(tr("Navi: "));

      locatorBox = new QToolButton();
        locatorBox->setIconSize(QSize(16,16));
        locatorBox->setFixedSize(20,20);

      QToolButton *locatorTL = new QToolButton();
        locatorTL->setIconSize(QSize(6,6));
        locatorTL->setFixedSize(10,10);
        locatorTL->setCheckable(true);
        locatorTL->setIcon(QIcon(":/images/topleft.png"));

      QToolButton *locatorTR = new QToolButton();
        locatorTR->setIconSize(QSize(6,6));
        locatorTR->setFixedSize(10,10);
        locatorTR->setCheckable(true);
        locatorTR->setIcon(QIcon(":/images/topright.png"));

      QToolButton *locatorBL = new QToolButton();
        locatorBL->setIconSize(QSize(6,6));
        locatorBL->setFixedSize(10,10);
        locatorBL->setCheckable(true);
        locatorBL->setIcon(QIcon(":/images/bottomleft.png"));

      QToolButton *locatorBR = new QToolButton();
        locatorBR->setIconSize(QSize(6,6));
        locatorBR->setFixedSize(10,10);
        locatorBR->setCheckable(true);
        locatorBR->setIcon(QIcon(":/images/bottomright.png"));

      locatorQuad = new QButtonGroup;
        locatorQuad->addButton(locatorTL,0);
        locatorQuad->addButton(locatorTR,1);
        locatorQuad->addButton(locatorBL,2);
        locatorQuad->addButton(locatorBR,3);
        locatorQuad->setExclusive(true);

      QGridLayout *directLayout = new QGridLayout;
        directLayout->addWidget(locatorTL,0,0);
        directLayout->addWidget(locatorTR,0,1);
        directLayout->addWidget(locatorBL,1,0);
        directLayout->addWidget(locatorBR,1,1);
        directLayout->setSpacing(0);
        directLayout->setMargin(0);

      locatorCheck = new QCheckBox(tr("On"));

  QHBoxLayout *locatorLayout = new QHBoxLayout;
    locatorLayout->addWidget(locatorLabel);
    locatorLayout->addSpacing(6);
    locatorLayout->addWidget(locatorBox);
    locatorLayout->addLayout(directLayout);
    locatorLayout->addWidget(locatorCheck);
    locatorLayout->addStretch(1);
    locatorLayout->setMargin(0);

  QFrame *panel = new QFrame();
    panel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    panel->setFrameStyle(QFrame::StyledPanel|QFrame::Plain);
    panel->setLineWidth(1);
    panel->setAutoFillBackground(true);

      QPalette panelColor = panel->palette();
        panelColor.setColor(QPalette::Active,QPalette::Window,QColor(200,200,220));
        panel->setPalette(panelColor);

  controlLayout = new QVBoxLayout;
    // controlLayout->addStrut(120);
    controlLayout->addLayout(kmerLayout);
    controlLayout->addSpacing(30);
    controlLayout->addLayout(viewLayoutA);
    controlLayout->addLayout(viewLayoutB);
    controlLayout->addLayout(zoomLayout);
    controlLayout->addSpacing(30);
    controlLayout->addLayout(cursorLayout);
    controlLayout->addLayout(focusLayout);
    controlLayout->addLayout(seqLayout);
    controlLayout->addSpacing(30);
    controlLayout->addLayout(locatorLayout);
    controlLayout->addWidget(panel);

  controlPart = new QWidget;
    controlPart->setLayout(controlLayout);
    controlPart->setSizePolicy(QSizePolicy::Fixed,QSizePolicy::Expanding);

  QHBoxLayout *toolLayout = new QHBoxLayout;
    toolLayout->addLayout(imageLayout,1);
    toolLayout->addWidget(controlPart,0);
    toolLayout->setSpacing(0);
    toolLayout->setMargin(0);

  QWidget *centralFrame = new QWidget();
    centralFrame->setLayout(toolLayout);

  setCentralWidget(centralFrame);

  pushState();

  connect(canvas,SIGNAL(NewFrame(double)),this,SLOT(frameToView(double)));
  connect(canvas,SIGNAL(NewFocus()),this,SLOT(clickToFocus()));

  connect(kmerSpin,SIGNAL(valueChanged(int)),this,SLOT(kmerChange()));

  connect(zoomIn,SIGNAL(clicked()),this,SLOT(zoomUp()));
  connect(zoomOut,SIGNAL(clicked()),this,SLOT(zoomDown()));
  connect(zoom0,SIGNAL(clicked()),this,SLOT(zoomOut()));
  connect(zoomEdit,SIGNAL(focusOut()),this,SLOT(zoomTo()));

  connect(Arng,SIGNAL(focusOut()),this,SLOT(checkRangeA()));
  connect(Brng,SIGNAL(focusOut()),this,SLOT(checkRangeB()));
  connect(Fpnt,SIGNAL(focusOut()),this,SLOT(checkFocus()));

  connect(Rpush,SIGNAL(clicked()),this,SLOT(viewChange()));
  connect(Fpush,SIGNAL(clicked()),this,SLOT(focusChange()));

  connect(focusBox,SIGNAL(clicked()),this,SLOT(focusColorChange()));
  connect(focusCheck,SIGNAL(stateChanged(int)),this,SLOT(hairsChange()));
  connect(seqCheck,SIGNAL(stateChanged(int)),this,SLOT(seqChange()));

  connect(locatorCheck,SIGNAL(stateChanged(int)),this,SLOT(locatorChange()));
  connect(locatorBox,SIGNAL(clicked()),this,SLOT(locatorColorChange()));
  connect(locatorQuad,SIGNAL(buttonClicked(int)),this,SLOT(locatorChange()));

  if (screenGeometry == NULL)
    screenGeometry = new QRect(QApplication::desktop()->availableGeometry(this));

  createActions();
  createMenus();

  setWindowTitle(title);
  setMinimumWidth(600);
  setMinimumHeight(500);

  Arng->setFocus();
  Arng->setChain(Fpnt,Brng);
  Brng->setChain(Arng,zoomEdit);
  zoomEdit->setChain(Brng,Fpnt);
  Fpnt->setChain(zoomEdit,Arng);
}

void DotWindow::createActions()
{ fullScreenAct = new QAction(tr("&Full Screen"), this);
    fullScreenAct->setShortcut(tr("Ctrl+F"));
    fullScreenAct->setToolTip(tr("Expand window to full screen"));

  minimizeAct = new QAction(tr("&Minimize"), this);
    minimizeAct->setShortcut(tr("Ctrl+M"));
    minimizeAct->setToolTip(tr("Minimize the current window"));

  connect(fullScreenAct, SIGNAL(triggered()), this, SLOT(fullScreen()));
  connect(minimizeAct, SIGNAL(triggered()), this, SLOT(showMinimized()));

  zoomInAct = new QAction(QIcon(":/images/zoomin.png"), tr("Zoom In"), this);
    zoomInAct->setShortcut(tr("Ctrl++"));
    zoomInAct->setToolTip(tr("Zoom in a notch on the image in active window"));

  zoomOutAct = new QAction(QIcon(":/images/zoomout.png"), tr("Zoom Out"), this);
    zoomOutAct->setShortcut(tr("Ctrl+-"));
    zoomOutAct->setToolTip(tr("Zoom out a notch on the image in active window"));

  connect(zoomInAct,SIGNAL(triggered()),this,SLOT(zoomUp()));
  connect(zoomOutAct,SIGNAL(triggered()),this,SLOT(zoomDown()));
}

void DotWindow::createMenus()
{ QMenuBar *bar = menuBar();
 
/*
  QMenu *fileMenu = bar->addMenu(tr("&File"));
    fileMenu->addAction(manager->openAct);
    fileMenu->addAction(manager->saveAct);
    fileMenu->addSeparator();
    fileMenu->addAction(manager->toolAct);
    fileMenu->addSeparator();
    fileMenu->addAction(manager->exitAct);

  QMenu *imageMenu = bar->addMenu(tr("&Image"));
    imageMenu->addAction(manager->tileAct);
    imageMenu->addAction(manager->cascadeAct);
    imageMenu->addSeparator();
    imageMenu->addAction(manager->nextAct);
    imageMenu->addAction(manager->keepAct);
    imageMenu->addSeparator();
    imageMenu->addAction(manager->xProjAct);
    imageMenu->addAction(manager->yProjAct);
    imageMenu->addAction(manager->zProjAct);
*/

  QMenu *windowMenu = bar->addMenu(tr("&Window"));
    windowMenu->addAction(fullScreenAct);
    windowMenu->addAction(minimizeAct);
/*
    windowMenu->addSeparator();
    windowMenu->addAction(manager->unminimizeAllAct);
    windowMenu->addAction(manager->raiseAllAct);
*/
}

void DotWindow::pushState()
{ setGeometry(state.wGeom);

  kmerSpin->setValue(state.kmer);

  Arng->setText(tr("%1 - %2").arg(state.view.left()).arg(state.view.right()+1));
  Brng->setText(tr("%1 - %2").arg(state.view.top()).arg(state.view.bottom()+1));

  zoomEdit->setText(tr("%1").arg(state.zoom,0,'f',2));

  Fpnt->setText(tr("%1,%2").arg(state.focus.x()).arg(state.focus.y()));
  QPixmap blob = QPixmap(16,16);
     blob.fill(state.fColor);
  focusBox->setIcon(QIcon(blob));
  focusCheck->setCheckState(state.fViz);
  seqCheck->setCheckState(state.sViz);

     blob.fill(state.lColor);
  locatorBox->setIcon(QIcon(blob));
  locatorCheck->setCheckState(state.lViz);
  locatorQuad->buttons().at(state.lQuad)->setChecked(true);
}

void DotWindow::kmerChange()
{ state.kmer = kmerSpin->value();
  update();
}

void DotWindow::frameToView(double newZ)
{ int ab = frame->x;
  int ae = frame->x + frame->w;
  int bb = frame->y;
  int be = frame->y + frame->h;
  if (ab < 0)
    { ab = 0;
      ae = plot.Alen;
    }
  if (bb < 0)
    { bb = 0;
      be = plot.Blen;
    }

  state.view = QRect(ab,bb,ae-ab,be-bb);
  if (newZ > 0.)
    state.zoom = newZ;

  Arng->setText(tr("%1 - %2").arg(state.view.left()).arg(state.view.right()+1));
  Brng->setText(tr("%1 - %2").arg(state.view.top()).arg(state.view.bottom()+1));
  zoomEdit->setText(tr("%1").arg(state.zoom,0,'f',2));
}

void DotWindow::clickToFocus()
{ Fpnt->setText(tr("%1,%2").arg(state.focus.x()).arg(state.focus.y())); }

void DotWindow::zoomDown()
{ if (canvas->zoomView(0.70710678118))
    update();
}

void DotWindow::zoomUp()
{ if (canvas->zoomView(1.41421356237))
    update();
}

void DotWindow::zoomOut()
{ canvas->resetView();
  frameToView(1.0);
  update();
}

void DotWindow::zoomTo()
{ double nZoom = zoomEdit->text().toDouble();
  if (canvas->zoomView(nZoom/state.zoom))
    update();
  else
    { zoomEdit->setText(tr("%1").arg(state.zoom));
      DotWindow::warning(tr("Beyond maximal zoom of 1bp per pixel"),this,DotWindow::ERROR,tr("OK"));
    }
}

bool DotWindow::checkPair(QLineEdit *widget, int &ab, int &ae, char del)
{ char   *b, *e;
  QString t = widget->text();
  char   *s = t.toLatin1().data();

  b = s;
  ab = strtol(b,&e,10);
  if (b == e || ab < 0)
    goto error;
  while (isspace(*e))
    e += 1;
  if (*e != del)
    goto error;
  b = e+1;
  ae = strtol(b,&e,10);
  if (b == e || ae < 0)
    goto error;
  while (isspace(*e))
    e += 1;
  if (*e != '\0')
    goto error;
  return (false);

error:
  widget->setSelection(e-s,t.size()-(e-s));
  return (true);
}

bool DotWindow::checkRangeA()
{ int ab, ae;

printf("Check A\n"); fflush(stdout);
  if (checkPair(Arng,ab,ae,'-'))
    return (true);
  if (ab >= ae)
    DotWindow::warning(tr("start position >= end position"),this,DotWindow::ERROR,tr("OK"));
  else if (ae > plot.Alen)
    DotWindow::warning(tr("end position beyond end of sequence"),this,DotWindow::ERROR,tr("OK"));
  else
    { Aval = QPoint(ab,ae);
      return (false);
    }
  return (true);
}

bool DotWindow::checkRangeB()
{ int bb, be;
  if (checkPair(Brng,bb,be,'-'))
    return (true);
  if (bb >= be)
    DotWindow::warning(tr("start position >= end position"),this,DotWindow::ERROR,tr("OK"));
  else if (be > plot.Blen)
    DotWindow::warning(tr("end position beyond end of sequence"),this,DotWindow::ERROR,tr("OK"));
  else
    { Bval = QPoint(bb,be);
      return (false);
    }
  return (true);
}

void DotWindow::viewChange()
{ printf("changeView\n");
  if (checkRangeA())
{ printf("Check done\n"); fflush(stdout);
    return;
}
  if (checkRangeB())
    return;
  QRect undo = state.view;
  state.view = QRect(Aval.x(),Bval.x(),Aval.y()-Aval.x(),Bval.y()-Bval.x());
#ifdef DEBUG
  printf("View Change %d - %d %d - %d\n",Aval.x(),Aval.y(),Bval.x(),Bval.y()); fflush(stdout);
#endif
  if (canvas->viewToFrame())
    update();
  else
    { state.view = undo;
      DotWindow::warning(tr("Beyond maximal zoom of 1bp per pixel"),this,DotWindow::ERROR,tr("OK"));
      Arng->setText(tr("%1 - %2").arg(state.view.left()).arg(state.view.right()+1));
      Brng->setText(tr("%1 - %2").arg(state.view.top()).arg(state.view.bottom()+1));
    }
}

bool DotWindow::checkFocus()
{ int a, b;
  if (checkPair(Fpnt,a,b,','))
    return (true);
  if (a > plot.Alen || b > plot.Blen)
    { DotWindow::warning(tr("point is outside matrix"),this,DotWindow::ERROR,tr("OK"));
      return (true);
    }
  Fval = QPoint(a,b);
  return (false);
}

void DotWindow::focusChange()
{ if (checkFocus())
    return;
  state.focus = Fval;
  update();
}

void DotWindow::focusColorChange()
{ state.fColor = QColorDialog::getColor(state.fColor);
  focusBox->setDown(false);

  QPixmap blob = QPixmap(16,16);
     blob.fill(state.fColor);
  focusBox->setIcon(QIcon(blob));

  update();
}

void DotWindow::hairsChange()
{ state.fViz = focusCheck->checkState();
  update();
}

void DotWindow::seqChange()
{ state.sViz = seqCheck->checkState();
  update();
}

void DotWindow::locatorChange()
{ state.lViz   = locatorCheck->checkState();
  state.lQuad  = (LocatorQuad) locatorQuad->checkedId();
  update();
}

void DotWindow::locatorColorChange()
{ state.lColor = QColorDialog::getColor(state.lColor);
  locatorBox->setDown(false);

  QPixmap blob = QPixmap(16,16);
     blob.fill(state.lColor);
  locatorBox->setIcon(QIcon(blob));

  update();
}

void DotWindow::updateWindowMenu()
{ if (isMaximized())
    { fullScreenAct->setToolTip(tr("Return window to previous size"));
      fullScreenAct->setText(tr("Previous size"));
    }
  else
    { fullScreenAct->setToolTip(tr("Expand window to full screen"));
      fullScreenAct->setText(tr("Full screen"));
    }
}

void DotWindow::fullScreen()
{ if (isMaximized())
    showNormal();
  else
    showMaximized();
}

void DotWindow::closeEvent(QCloseEvent *event)
{ state.wGeom = geometry();
  lastState = state;
  firstTime = false;
  event->accept();
}


int DotWindow::warning(const QString& message, QWidget *parent, MessageKind kind,
                        const QString& label1, const QString& label2, const QString& label3)
{ QPixmap     *symbol;
  QPushButton *button1 = 0, *button2 = 0, *button3 = 0;

  QMessageBox msg(QMessageBox::Critical, QObject::tr("DaViewer"),
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

  msg.exec();

  if (msg.clickedButton() == button1)
    return (0);
  else if (msg.clickedButton() == button2)
    return (1);
  else
    return (2);
}
