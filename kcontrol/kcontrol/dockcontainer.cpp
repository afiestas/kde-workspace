/*
  Copyright (c) 2000 Matthias Elter <elter@kde.org>

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

*/

#include <qapplication.h>
#include <qlabel.h>

#include <kmessagebox.h>
#include <kglobal.h>
#include <klocale.h>
#include <kcmodule.h>
#include <kdebug.h>

#include "dockcontainer.h"
#include "dockcontainer.moc"

#include "global.h"
#include "modules.h"
#include "proxywidget.h"
#include <qobjectlist.h>
#include <qaccel.h>

DockContainer::DockContainer(QWidget *parent)
  : QWidget(parent, "DockContainer")
  , _basew(0L)
  , _module(0L)
{
  _busy = new QLabel(i18n("<big>Loading ...</big>"), this);
  _busy->setAlignment(AlignCenter);
  _busy->setTextFormat(RichText);
  _busy->setGeometry(0,0, width(), height());
  _busy->hide();
}

DockContainer::~DockContainer()
{
  deleteModule();
}

bool DockContainer::event( QEvent * e )
{
    if ( e->type() == QEvent::LayoutHint ) {
	qDebug("dock container layout hint");
	updateGeometryEx();
    }
    return QWidget::event( e );
}

void DockContainer::setBaseWidget(QWidget *widget)
{
  delete _basew;
  _basew = 0;
  if (!widget) return;

  _basew = widget;
  _basew->reparent(this, 0, QPoint(0,0), true);

  _basew->resize( size() );
  emit newModule(widget->caption(), "", "");
  updateGeometryEx();
}

QSize DockContainer::minimumSizeHint() const
{
    if (_module)
	return _module->module()->minimumSizeHint();
    if ( _basew )
	return _basew->minimumSizeHint().expandedTo( _basew->minimumSize() );
    return QWidget::minimumSizeHint();
}

bool DockContainer::dockModule(ConfigModule *module)
{
  if (module == _module) return true;

  if (_module && _module->isChanged())
    {

      int res = KMessageBox::warningYesNoCancel(this,
module ?
i18n("There are unsaved changes in the active module.\n"
     "Do you want to apply the changes before running "
     "the new module or forget the changes?") :
i18n("There are unsaved changes in the active module.\n"
     "Do you want to apply the changes before exiting "
     "the Control Center or forget the changes?"),
                                          i18n("Unsaved Changes"),
                                          i18n("&Apply"),
                                          i18n("&Forget"));
      if (res == KMessageBox::Yes)
        _module->module()->applyClicked();
      if (res == KMessageBox::Cancel)
        return false;
    }

  deleteModule();
  if (!module) return true;

  _busy->raise();
  _busy->show();
  _busy->repaint();
  QApplication::setOverrideCursor( waitCursor );

  ProxyWidget *widget = module->module();

  if (widget)
    {
      _module = module;
      connect(_module, SIGNAL(childClosed()),
              this, SLOT(removeModule()));
      connect(_module, SIGNAL(changed(ConfigModule *)),
              SIGNAL(changedModule(ConfigModule *)));
      //####### this doesn't work anymore, what was it supposed to do? The signal is gone.
//       connect(_module, SIGNAL(quickHelpChanged()),
//               this, SLOT(quickHelpChanged()));

      widget->reparent(this, 0 , QPoint(0,0), false);
      widget->resize(size());

      emit newModule(widget->caption(), module->docPath(), widget->quickHelp());
      QApplication::restoreOverrideCursor();
    }
  else
    QApplication::restoreOverrideCursor();

  if (widget)  {
      widget->show();
      QApplication::sendPostedEvents( widget, QEvent::ShowWindowRequest ); // show NOW
  }
  _busy->hide();

  KCGlobal::repairAccels( topLevelWidget() );
  updateGeometryEx();
  return true;
}

void DockContainer::updateGeometryEx()
{
    // workaround a QSplitter bug in Qt 3.0.3
    updateGeometry();
//     if ( parentWidget() && parentWidget()->inherits("QSplitter") )
// 	parentWidget()->updateGeometry();
}

void DockContainer::removeModule()
{
  deleteModule();

  resizeEvent(0L);

  if (_basew)
      emit newModule(_basew->caption(), "", "");
  else
      emit newModule("", "", "");

  updateGeometryEx();
}

void DockContainer::deleteModule()
{
  if(_module) {
	_module->deleteClient();
	_module = 0;
  }
}

void DockContainer::resizeEvent(QResizeEvent *)
{
  _busy->resize(width(), height());
  if (_module)
	{
	  _module->module()->resize(size());
	  _basew->hide();
	}
  else if (_basew)
	{
	  _basew->resize(size());
	  _basew->show();
	}
}

void DockContainer::quickHelpChanged()
{
  if (_module && _module->module())
	emit newModule(_module->module()->caption(),  _module->docPath(), _module->module()->quickHelp());
}
