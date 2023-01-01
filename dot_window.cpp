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

DotLineEdit::DotLineEdit(QWidget *parent) : QLineEdit(parent) {}

void DotLineEdit::setChain(QWidget *p, QWidget *s)
{ mypred = p;
  mysucc = s;
}

void DotLineEdit::focusOutEvent(QFocusEvent *event)
{ if (event->reason() <= Qt::BacktabFocusReason)
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
    }
  else
    QLineEdit::keyPressEvent(event);
}

bool DotLineEdit::focusNextPrevChild(bool next)
{ (void) next;
  return (false);
}

SeqLineEdit::SeqLineEdit(QWidget *parent, int id) : QLineEdit(parent)
{ setFixedHeight(20);
  setStyleSheet("border: 1px solid black");
  setReadOnly(true);
  setFont(QFont(tr("Monaco")));
  myid = id;
}

void SeqLineEdit::enterEvent(QEvent *)
{ setFocus(); }

void SeqLineEdit::leaveEvent(QEvent *)
{ clearFocus(); }

void SeqLineEdit::keyPressEvent(QKeyEvent *event)
{ int key = event->key();
  if (key == Qt::Key_Left)
    emit focusShift(-1,myid);
  else if (key == Qt::Key_Right)
    emit focusShift(+1,myid);
  else
    return;
}

void SeqLineEdit::mousePressEvent(QMouseEvent *event)
{ (void) event; }

void SeqLineEdit::mouseMoveEvent(QMouseEvent *event)
{ (void) event; }

void SeqLineEdit::mouseReleaseEvent(QMouseEvent *event)
{ (void) event; }


/***************************************************************************************/
/*                                                                                     */
/*   DOT CANVAS                                                                        */
/*                                                                                     */
/***************************************************************************************/

DotCanvas::~DotCanvas()
{ free(raster); }

DotCanvas::DotCanvas(QWidget *parent) : QWidget(parent)
{ setSizePolicy(QSizePolicy::MinimumExpanding,QSizePolicy::MinimumExpanding);
  noFrame = true;
  rubber  = new QRubberBand(QRubberBand::Rectangle, this);
  timer   = new QBasicTimer();
  image   = NULL;
  raster  = new uchar *[screenGeometry->height()];
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
  printf("Frame (zoom) = (%.1f,%.1f) %.1f vs %.1f\n",nX,nY,nW,nH);
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

  vW = (vW / oW) * nW;
  vH = (vH / oH) * nH;
  if (frame.w >= plot->Alen && frame.h >= plot->Blen)
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
else if (vX + vW > plot->Alen)
  vX = plot->Alen - vW;
      if (vH > plot->Blen)
        vY = (plot->Blen-vH)/2.;
      else if (vY < 0.)
        vY = 0.;
else if (vY + vH > plot->Blen)
  vY = plot->Blen - vH;
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
      int xb = frame.x + ((reg.x()-20.)/(rectW-40.))*frame.w;
      int yb = frame.y + ((reg.y()-20.)/(rectH-40.))*frame.h;
      int xe = frame.x + (((reg.x()-20.)+reg.width())/(rectW-40.))*frame.w;
      int ye = frame.y + (((reg.y()-20.)+reg.height())/(rectH-40.))*frame.h;
      if (xb < 0) xb = 0;
      if (xb < frame.x) xb = frame.x;
      if (yb < 0) yb = 0;
      if (yb < frame.y) yb = frame.y;
      if (xe > plot->Alen) xe = plot->Alen;
      if (xe > frame.x+frame.w) xe = frame.x+frame.w;
      if (ye > plot->Blen) ye = plot->Blen;
      if (ye > frame.y+frame.h) ye = frame.y+frame.h;
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
    { double ex = event->x()-20.;
      double ey = event->y()-20.;
#ifdef DEBUG
      printf("Clicked\n");
#endif
      int x = frame.x + (ex/(rectW-40))*frame.w;
      int y = frame.y + (ey/(rectH-40))*frame.h;
      if (x < 0) x = 0;
      if (x < frame.x) x = frame.x;
      if (x > plot->Alen) x = plot->Alen;
      if (x > frame.x+frame.w) x = frame.x+frame.w;
      if (y < 0) y = 0;
      if (y < frame.y) y = frame.y;
      if (y > plot->Blen) y = plot->Blen;
      if (y > frame.y+frame.h) y = frame.y+frame.h;
      state->focus = QPoint(x,y);
      update();
      emit NewFocus();
    }

  if ((QApplication::keyboardModifiers() & Qt::ShiftModifier) != 0)
    QApplication::changeOverrideCursor(Qt::CrossCursor);
  else
    QApplication::changeOverrideCursor(Qt::ArrowCursor);
}

QVector<QRgb> DotCanvas::ctable(256);

void DotCanvas::paintEvent(QPaintEvent *event)
{ QPainter painter;

  (void) event;

  painter.begin(this);
  painter.setRenderHint(QPainter::Antialiasing,true);
  painter.setRenderHint(QPainter::SmoothPixmapTransform,true);

  if (noFrame)    //  First paint => set Frame (cannot do earlier as do not know size)
    { resetView();
      noFrame = false;
    }

  double vX = frame.x;
  double vY = frame.y;
  double vW = frame.w;
  double vH = frame.h;

  // printf("Frame = (%g,%g) %g x %g into %d x %d\n",vX,vY,vW,vH,rectW,rectH);

  if (image == NULL || rectW != image->width() || rectH != image->height())
    { int j;

      if (image != NULL)
        delete image;
      image = new QImage(rectW,rectH,QImage::Format_Indexed8);
      image->setColorTable(ctable);
      for (j = 0; j < rectH; j++)
        raster[j] = image->scanLine(j);
    }

  // printf("  image = %d x %d\n",image->width(),image->height());

  image->fill(0);

  render_plot(plot->dots,&frame,rectW,rectH,raster);

  painter.drawImage(QPoint(0,0),*image);

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
          lX = (rectW-20) - (lW + 6);
        else
          lX = 26;
        if (state->lQuad < 2)
          lY = 26;
        else
          lY = (rectH-20) - (lH + 6);

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

  double xa = (rectW-44)/vW;
  double xb = -vX*xa+22;

  double ya = (rectH-44)/vH;
  double yb = -vY*ya+22;

  { QPen dPen;
    int  b, w;
    int  c, h;
    int  x, y;
    int  u, v;

    dPen.setColor(QColor(255,255,255));
    dPen.setWidth(2);
    painter.setPen(dPen);
  
    if (xb < 22)
      b = 21;
    else
      b = xb-1;
    w = xb + plot->Alen*xa + 1;
    if (w >= rectW-20)
      w = rectW-21;
    w = w-b;

    if (yb < 22)
      c = 21;
    else
      c = yb-1;
    h = yb + plot->Blen*ya + 1;
    if (h >= rectH-20)
      h = rectH-21;
    h = h-c;

    painter.drawRect(b,c,w,h);

    u = divide_bar((int) ((100./(rectW-40.))*vW));

    if (vX < 0)
      { x = units[u];
        y = ((plot->Alen-1)/units[u])*units[u];
      }
    else
      { x = ((((int) vX)/units[u])+1)*units[u];
        y = ((((int) vX+vW)-1)/units[u])*units[u];
      }
    for (; x <= y; x += units[u])
      { v = xb+xa*x;
        if (x > plot->Alen)
          break;
        painter.drawLine(v,c,v,c-4);
        painter.drawText(QRect(v-10,c-8,20,4),Qt::AlignHCenter|Qt::AlignBottom|Qt::TextDontClip,
                         tr("%1%2").arg(x/divot[u]).arg(suffix[u]));
      }

    if (vY < 0)
      { x = units[u];
        y = ((plot->Blen-1)/units[u])*units[u];
      }
    else
      { x = ((((int) vY)/units[u])+1)*units[u];
        y = ((((int) vY+vH)-1)/units[u])*units[u];
      }

    painter.save();
    painter.rotate(-90.);
    for (; x <= y; x += units[u])
      { v = yb+ya*x;
        if (x > plot->Blen)
          break;
        painter.drawLine(-v,b,-v,b-4);
        painter.drawText(QRect(-v-10,b-9,20,4),Qt::AlignHCenter|Qt::AlignBottom|Qt::TextDontClip,
                         tr("%1%2").arg(x/divot[u]).arg(suffix[u]));
      }
    painter.restore();
  }

  if (state->fOn == Qt::Checked)
    { painter.setPen(state->fColor);
      int x = xa*state->focus.x() + xb;
      int y = ya*state->focus.y() + yb;
      if (state->fViz == Qt::Checked)
        { if (xb-2 < 22)
            if (xa*plot->Alen+xb+2 > rectW-22)
              painter.drawLine(22,y,rectW-22,y);
            else
              painter.drawLine(22,y,xa*plot->Alen+xb+2,y);
          else
            if (xa*plot->Alen+xb+2 > rectW-22)
              painter.drawLine(xb-2,y,rectW-22,y);
            else
              painter.drawLine(xb-2,y,xa*plot->Alen+xb+2,y);
          if (yb-2 < 22)
            if (ya*plot->Blen+yb+2 > rectH-22)
              painter.drawLine(x,22,x,rectH-22);
            else
              painter.drawLine(x,22,x,ya*plot->Blen+yb+2);
          else
            if (ya*plot->Blen+yb+2 > rectH-22)
              painter.drawLine(x,yb-2,x,rectH-22);
            else
              painter.drawLine(x,yb-2,x,ya*plot->Blen+yb+2);
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
    { int j;

      state.kmer = 10;

      state.fColor = QColor(255,255,0);
      state.fViz   = Qt::Unchecked;
      state.sViz   = Qt::Unchecked;

      state.fOn    = Qt::Unchecked;
      state.lColor = QColor(255,0,255);
      state.lViz   = Qt::Checked;
      state.lQuad  = TOP_LEFT;
      state.wGeom  = QRect(0,0,600,400);

      firstTime = false;
      lastState = state;

      for (j = 0; j < 256; j++)
        DotCanvas::ctable[j] = qRgb(j,j,0);
    }
  else if ( ! MainWindow::plots.isEmpty())
    state = MainWindow::plots.last()->state;
  else
    state = lastState;

  if (screenGeometry == NULL)
    screenGeometry = new QRect(QApplication::desktop()->availableGeometry(this));

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
        kmerSpin->setFixedWidth(50);
        kmerSpin->setAlignment(Qt::AlignRight);
        // kmerSpin->setFocusPolicy(Qt::NoFocus);

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

      focusOn = new QCheckBox(tr("On"));

      focusBox = new QToolButton();
        focusBox->setIconSize(QSize(16,16));
        focusBox->setFixedSize(20,20);

      focusCheck = new QCheckBox(tr("Cross Hairs"));
    
  QHBoxLayout *focusLayout = new QHBoxLayout;
    focusLayout->addWidget(focusOn);
    focusLayout->addSpacing(12);
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

  QLabel *panel = new QLabel();
    panel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    panel->setFrameStyle(QFrame::StyledPanel|QFrame::Plain);
    panel->setLineWidth(1);
    panel->setAutoFillBackground(true);
    panel->setText(tr("%1<br>%2<br>%3<br>%4<br>%5<br>%6")
                         .arg("<b>Over plot area:</b>")
                         .arg("&nbsp;&nbsp;Click to set focus")
                         .arg("&nbsp;&nbsp;Push-Drag to pan")
                         .arg("&nbsp;&nbsp;Shift-Push-Drag for zoom area")
                         .arg("<b>Over sequences:</b>")
                         .arg("&nbsp;&nbsp;Left/Right Key to move focus 1bp"));
    panel->setAlignment(Qt::AlignLeft|Qt::AlignTop);

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

    seqUL = new SeqLineEdit(this,1);
      seqUL->setAlignment(Qt::AlignRight);
    seqUR = new SeqLineEdit(this,2);
      seqUR->setAlignment(Qt::AlignLeft);
    seqLL = new SeqLineEdit(this,3);
      seqLL->setAlignment(Qt::AlignRight);
    seqLR = new SeqLineEdit(this,4);
      seqLR->setAlignment(Qt::AlignLeft);

      QLabel *Alab = new QLabel(tr(" A: "));
      QLabel *Blab = new QLabel(tr(" B: "));

    QVBoxLayout *labs = new QVBoxLayout;
      labs->addWidget(Alab,0);
      labs->addWidget(Blab,0);

    QVBoxLayout *seqL = new QVBoxLayout;
      seqL->addWidget(seqUL,0);
      seqL->addWidget(seqLL,0);
      seqL->setSpacing(0);
      seqL->setMargin(0);

    QVBoxLayout *seqR = new QVBoxLayout;
      seqR->addWidget(seqUR,0);
      seqR->addWidget(seqLR,0);
      seqR->setSpacing(0);
      seqR->setMargin(0);

    QHBoxLayout *seqAll = new QHBoxLayout;
      seqAll->addLayout(labs,0);
      seqAll->addLayout(seqL,1);
      seqAll->addLayout(seqR,1);
      seqAll->setSpacing(0);
      seqAll->setMargin(0);

    QWidget *seqPart = new QWidget;
      seqPart->setLayout(seqAll);
      seqPart->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Fixed);

  QVBoxLayout *canLayout = new QVBoxLayout;
    canLayout->addWidget(canvas,1);
    canLayout->addWidget(seqPart,0);
    canLayout->setSpacing(0);
    canLayout->setMargin(0);

  QHBoxLayout *toolLayout = new QHBoxLayout;
    toolLayout->addLayout(canLayout,1);
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

  connect(focusOn,SIGNAL(stateChanged(int)),this,SLOT(focusOnChange()));
  connect(focusBox,SIGNAL(clicked()),this,SLOT(focusColorChange()));
  connect(focusCheck,SIGNAL(stateChanged(int)),this,SLOT(hairsChange()));
  connect(seqCheck,SIGNAL(stateChanged(int)),this,SLOT(seqChange()));

  connect(seqUL,SIGNAL(focusShift(int,int)),this,SLOT(seqMove(int,int)));
  connect(seqUR,SIGNAL(focusShift(int,int)),this,SLOT(seqMove(int,int)));
  connect(seqLL,SIGNAL(focusShift(int,int)),this,SLOT(seqMove(int,int)));
  connect(seqLR,SIGNAL(focusShift(int,int)),this,SLOT(seqMove(int,int)));

  connect(locatorCheck,SIGNAL(stateChanged(int)),this,SLOT(locatorChange()));
  connect(locatorBox,SIGNAL(clicked()),this,SLOT(locatorColorChange()));
  connect(locatorQuad,SIGNAL(buttonClicked(int)),this,SLOT(locatorChange()));

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

  focusOn->setCheckState(state.fOn);
  bool en = (state.fOn != Qt::Unchecked);

  Fpnt->setEnabled(en);
  focusBox->setEnabled(en);
  focusCheck->setEnabled(en);
  seqCheck->setEnabled(en);

  en = (en && state.sViz != Qt::Unchecked);

  seqUL->setEnabled(en);
  seqUR->setEnabled(en);
  seqLL->setEnabled(en);
  seqLR->setEnabled(en);

  QPixmap blob = QPixmap(16,16);

  blob.fill(state.fColor);
  Fpnt->setText(tr("%1,%2").arg(state.focus.x()).arg(state.focus.y()));
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
  free_dots(plot.dots);
  plot.dots = build_dots(plot.Alen,plot.Aseq,plot.Blen,plot.Bseq,state.kmer);
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
{ Fpnt->setText(tr("%1,%2").arg(state.focus.x()).arg(state.focus.y()));
  seqRealign();
}

void DotWindow::seqMove(int dir, int id)
{ if (id <= 2)
    { state.focus.setX(state.focus.x()-dir);
      if (state.focus.x() < 0)
        state.focus.setX(0);
      else if (state.focus.x() > plot.Alen)
        state.focus.setX(plot.Alen);
    }
  else
    { state.focus.setY(state.focus.y()-dir);
      if (state.focus.y() < 0)
        state.focus.setY(0);
      else if (state.focus.y() > plot.Blen)
        state.focus.setY(plot.Blen);
    }
  clickToFocus();
  update();
}

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
  seqRealign();
  update();
}

void DotWindow::seqRealign()
{ int c = state.focus.x();
  int v;

  v = plot.Aseq[c];
  plot.Aseq[c] = 0;
  if (c >= 500)
    seqUL->setText(tr("%1").arg(plot.Aseq+(c-500)));
  else
    seqUL->setText(tr("%1").arg(plot.Aseq));
  plot.Aseq[c] = v;

  if (c+500 > plot.Alen)
    { v = plot.Aseq[c+500];
      plot.Aseq[c+500] = 0;
      seqUR->setText(tr("%1").arg(plot.Aseq+c));
      plot.Aseq[c+500] = v;
    }
  else
    seqUR->setText(tr("%1").arg(plot.Aseq+c));
  seqUR->setCursorPosition(0);

  c = state.focus.y();

  v = plot.Bseq[c];
  plot.Bseq[c] = 0;
  if (c >= 500)
    seqLL->setText(tr("%1").arg(plot.Bseq+(c-500)));
  else
    seqLL->setText(tr("%1").arg(plot.Bseq));
  plot.Bseq[c] = v;

  if (c+500 > plot.Blen)
    { v = plot.Bseq[c+500];
      plot.Bseq[c+500] = 0;
      seqLR->setText(tr("%1").arg(plot.Bseq+c));
      plot.Bseq[c+500] = v;
    }
  else
    seqLR->setText(tr("%1").arg(plot.Bseq+c));
  seqLR->setCursorPosition(0);
}

void DotWindow::focusColorChange()
{ state.fColor = QColorDialog::getColor(state.fColor);
  focusBox->setDown(false);

  QPixmap blob = QPixmap(16,16);
     blob.fill(state.fColor);
  focusBox->setIcon(QIcon(blob));

  update();
}

void DotWindow::focusOnChange()
{ state.fOn = focusOn->checkState();

  bool en = (state.fOn != Qt::Unchecked);

  Fpnt->setEnabled(en);
  focusBox->setEnabled(en);
  focusCheck->setEnabled(en);
  seqCheck->setEnabled(en);

  en = en && (state.sViz != Qt::Unchecked);

  seqUL->setEnabled(en);
  seqUR->setEnabled(en);
  seqLL->setEnabled(en);
  seqLR->setEnabled(en);

  seqRealign();
  update();
}

void DotWindow::hairsChange()
{ state.fViz = focusCheck->checkState();
  update();
}

void DotWindow::seqChange()
{ state.sViz = seqCheck->checkState();

  bool en = (state.sViz != Qt::Unchecked);

  seqUL->setEnabled(en);
  seqUR->setEnabled(en);
  seqLL->setEnabled(en);
  seqLR->setEnabled(en);

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

void DotWindow::fullScreen()
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

void DotWindow::closeEvent(QCloseEvent *event)
{ QList<DotWindow *> plots = MainWindow::plots;
  int i;

  state.wGeom = geometry();
  lastState = state;
  firstTime = false;

  for (i = 0; i < plots.length(); i++)
    if (plots[i] == this)
      { plots.removeAt(i);
        break;
      }
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
