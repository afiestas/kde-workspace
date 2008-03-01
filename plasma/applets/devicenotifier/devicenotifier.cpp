/***************************************************************************
 *   Copyright (C) 2007 by Alexis Ménard <darktears31@gmail.com>           *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA .        *
 ***************************************************************************/

#include "devicenotifier.h"
#include "itemdelegate.h"
#include "notifierview.h"

#include <QPainter>
#include <QColor>
#include <QTreeView>
#include <QApplication>
#include <QStandardItemModel>
#include <QGraphicsView>
#include <QtDBus/QDBusInterface>
#include <QtDBus/QDBusReply>

#include <KDialog>
#include <KRun>
#include <KStandardDirs>
#include <KDesktopFile>
#include <kdesktopfileactions.h>
#include <KGlobalSettings>

#include <plasma/svg.h>
#include <plasma/widgets/widget.h>
#include <plasma/containment.h>
#include <plasma/dialog.h>
#include <plasma/phase.h>

//use for desktop view
#include <plasma/layouts/boxlayout.h>
#include <plasma/widgets/icon.h>

#include <solid/device.h>

using namespace Plasma;
using namespace Notifier;

DeviceNotifier::DeviceNotifier(QObject *parent, const QVariantList &args)
    : Plasma::Applet(parent, args),
      m_solidEngine(0),
      m_hotplugModel(0),
      m_widget(0),
      m_icon(0),
      m_layout(0),
      m_proxy(0),
      m_dialog(0),
      m_displayTime(0),
      m_numberItems(0),
      m_itemsValidity(0),
      m_timer(0)
{
    setHasConfigurationInterface(true);
}

void DeviceNotifier::init()
{
    KConfigGroup cg = config();
    m_timer = new QTimer();
    m_displayTime = cg.readEntry("TimeDisplayed", 8);
    m_numberItems = cg.readEntry("NumberItems", 4);
    m_itemsValidity = cg.readEntry("ItemsValidity", 5);

    m_solidEngine = dataEngine("hotplug");

    m_widget = new Dialog();

    QVBoxLayout *l_layout = new QVBoxLayout();
    l_layout->setSpacing(0);
    l_layout->setMargin(0);

    m_hotplugModel = new QStandardItemModel(this);

    QLabel *label = new QLabel(i18n("<font color=\"white\">Recently plugged devices:</font>"));
    QLabel *icon = new QLabel();
    icon->setPixmap(KIcon("emblem-mounted").pixmap(ItemDelegate::ICON_SIZE, ItemDelegate::ICON_SIZE));
    icon->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

    QHBoxLayout *l_layout2 = new QHBoxLayout();
    l_layout2->setSpacing(0);
    l_layout2->setMargin(0);

    l_layout2->addWidget(icon);
    l_layout2->addWidget(label);

    Notifier::NotifierView *m_notifierView= new NotifierView(m_widget);
    m_notifierView->setModel(m_hotplugModel);
    ItemDelegate *delegate = new ItemDelegate;
    m_notifierView->setItemDelegate(delegate);

    l_layout->addLayout(l_layout2);
    l_layout->addWidget(m_notifierView);
    m_widget->setLayout(l_layout);

    m_widget->setWindowFlags(Qt::Popup);
    m_widget->setFocusPolicy(Qt::NoFocus);
    m_widget->adjustSize();

    kDebug() << "I'm in containment : " << containment()->containmentType();
    if (containment()->containmentType() != Containment::DesktopContainment) {
        initSysTray();
	isOnDesktop=false;
    }
    else
    {
	m_layout = new Plasma::BoxLayout(Plasma::BoxLayout::LeftToRight, this);
	//m_layout = new QGraphicsGridLayout();
	//m_layout->setMargin(0);
	m_layout->setSpacing(0);
	m_proxy= new QGraphicsProxyWidget(this);
	m_proxy->setWidget(m_widget);
        m_proxy->show();
	//m_layout->addItem(m_proxy);
    	isOnDesktop=true;
    }

    //feed the list with what is already reported by the engine
    isNotificationEnabled = false;
    foreach (const QString &source, m_solidEngine->sources()) {
        onSourceAdded(source);
    }
    isNotificationEnabled = true;

    //connect to engine when a device is plug
    connect(m_solidEngine, SIGNAL(newSource(const QString&)),
            this, SLOT(onSourceAdded(const QString&)));
    connect(m_solidEngine, SIGNAL(sourceRemoved(const QString&)),
            this, SLOT(onSourceRemoved(const QString&)));

    //FIXME : For KDE4.1 need to use to KStyle to use correct click behaviour
    if (KGlobalSettings::singleClick()) {
        connect(m_notifierView, SIGNAL(clicked (const QModelIndex &)), this, SLOT(slotOnItemClicked(const QModelIndex &)));
    } else {
        connect(m_notifierView, SIGNAL(doubleClicked (const QModelIndex &)), this, SLOT(slotOnItemClicked(const QModelIndex &)));
    }
    connect(m_timer, SIGNAL(timeout()), this, SLOT(onTimerExpired()));
}


void DeviceNotifier::initSysTray()
{
     setDrawStandardBackground(false);
    //we display the icon corresponding to the computer
    QList<Solid::Device> list = Solid::Device::allDevices();

    if (list.size() > 0) {
        Solid::Device device=list[0];

        while (device.parent().isValid()) {
            device = device.parent();
        }
        m_icon = new Plasma::Icon(KIcon(device.icon()), QString(), this);
    } else {
        //default icon if problem
        m_icon = new Plasma::Icon(KIcon("computer"), QString(), this);
    }
    setMinimumContentSize(m_icon->sizeFromIconSize(IconSize(KIconLoader::Small)));
    connect(m_icon, SIGNAL(clicked()), this, SLOT(onClickNotifier()));
    setContentSize(size());
    m_icon->resize(size());
    updateGeometry();
    update();
}

DeviceNotifier::~DeviceNotifier()
{
    delete m_widget;
    if(!isOnDesktop) {
        delete m_icon;
    }
    delete m_layout;
    m_layout = 0;
    delete m_dialog;
    delete m_hotplugModel;
    delete m_timer;
}

Qt::Orientations DeviceNotifier::expandingDirections() const
{
    if (formFactor() == Plasma::Horizontal) {
        return Qt::Vertical;
    } else {
        return Qt::Horizontal;
    }
}

QSizeF DeviceNotifier::contentSizeHint() const
{
    QSizeF sizeHint = contentSize();
    switch (formFactor()) {
    case Plasma::Vertical:
        sizeHint.setHeight(sizeHint.width());
        break;
    case Plasma::Horizontal:
        sizeHint.setWidth(sizeHint.height());
        break;
    default:
        break;
    }

    return sizeHint;
}

void DeviceNotifier::paintInterface(QPainter *p, const QStyleOptionGraphicsItem *option, const QRect &rect)
{
    Applet::paintInterface(p,option,rect);
}

void DeviceNotifier::dataUpdated(const QString &source, Plasma::DataEngine::Data data)
{
    if (data.size() > 0) {
        int nb_actions = 0;
        QString last_action_label;
        foreach (QString desktop, data["predicateFiles"].toStringList()) {
            QString filePath = KStandardDirs::locate("data", "solid/actions/" + desktop);
            QList<KServiceAction> services = KDesktopFileActions::userDefinedServices(filePath, true);
            nb_actions += services.size();
            if (services.size() > 0) {
                last_action_label = QString(services[0].text());
            }
        }
        QModelIndex index = indexForUdi(source);
	Q_ASSERT(index.isValid());

	m_hotplugModel->setData(index, data["predicateFiles"], PredicateFilesRole);
	m_hotplugModel->setData(index, data["text"], Qt::DisplayRole);
	m_hotplugModel->setData(index, KIcon(data["icon"].toString()), Qt::DecorationRole);

	if (nb_actions > 1) {
	    QString s = i18np("1 action for this device",
			      "%1 actions for this device",
			      nb_actions);
	    m_hotplugModel->setData(index, s, ActionRole);
	} else {
	    m_hotplugModel->setData(index,last_action_label, ActionRole);
	}
	if(!isOnDesktop && isNotificationEnabled) {
	    m_widget->move(popupPosition(m_widget->sizeHint()));
	    m_widget->show();
	    m_widget->clearFocus();
	    m_timer->start(m_displayTime*1000);
	}
   }
}

void DeviceNotifier::onSourceAdded(const QString &name)
{
    kDebug() << "DeviceNotifier:: source added" << name;
    if (m_hotplugModel->rowCount() == m_numberItems && m_numberItems != 0) {
        m_hotplugModel->removeRow(m_hotplugModel->rowCount() - 1);
    }
    QStandardItem *item = new QStandardItem();
    item->setData(name, SolidUdiRole);
    m_hotplugModel->insertRow(0, item);
    m_solidEngine->connectSource(name, this);
}

void DeviceNotifier::onSourceRemoved(const QString &name)
{
    QModelIndex index = indexForUdi(name);
    Q_ASSERT(index.isValid());
    m_hotplugModel->removeRow(index.row());
    if (m_hotplugModel->rowCount() == 0) {
         if(!isOnDesktop) {
	    m_widget->hide();
	 }
    }
}

QModelIndex DeviceNotifier::indexForUdi(const QString &udi) const
{
    int rowCount = m_hotplugModel->rowCount();
    for (int i=0; i < rowCount; ++i) {
        QModelIndex index = m_hotplugModel->index(i, 0);
        QString itemUdi = m_hotplugModel->data(index, SolidUdiRole).toString();
        if (itemUdi == udi) {
            return index;
        }
    }
    //Is it possible to go here?no...
    kDebug() << "We should not be here!";
    return QModelIndex();
}

void DeviceNotifier::onClickNotifier()
{
    m_widget->isVisible() ? m_widget->hide() : m_widget->show();
    m_widget->clearFocus();
}

void DeviceNotifier::hoverEnterEvent(QGraphicsSceneHoverEvent *event)
{
    if(!isOnDesktop) {
        m_widget->move(popupPosition(m_widget->sizeHint()));
    }
    Applet::hoverEnterEvent(event);
}

void DeviceNotifier::showConfigurationInterface()
{
    if (m_dialog == 0) {
        kDebug() << "DeviceNotifier:: Enter in configuration interface";
        m_dialog = new KDialog;
        m_dialog->setCaption(i18n("Configure New Device Notifier"));

        QWidget *widget = new QWidget;
        ui.setupUi(widget);
        m_dialog->setMainWidget(widget);
        m_dialog->setButtons(KDialog::Ok | KDialog::Cancel | KDialog::Apply);
        connect(m_dialog, SIGNAL(applyClicked()), this, SLOT(configAccepted()));
        connect(m_dialog, SIGNAL(okClicked()), this, SLOT(configAccepted()));
        ui.spinTime->setValue(m_displayTime);
        ui.spinItems->setValue(m_numberItems);
        ui.spinTimeItems->setValue(m_itemsValidity);
    }

    m_dialog->show();
}

void DeviceNotifier::configAccepted()
{
    kDebug() << "DeviceNotifier:: Config Accepted with params" << ui.spinTime->value() \
             << "," << ui.spinItems->value() \
             << "," << ui.spinTimeItems->value();
    m_displayTime = ui.spinTime->value();
    m_numberItems = ui.spinItems->value();
    m_itemsValidity = ui.spinTimeItems->value();
    KConfigGroup cg = config();
    cg.writeEntry("TimeDisplayed", m_displayTime);
    cg.writeEntry("NumberItems", m_numberItems);
    cg.writeEntry("ItemsValidity", m_itemsValidity);
    emit configNeedsSaving();
}

void DeviceNotifier::slotOnItemClicked(const QModelIndex &index)
{
    if(!isOnDesktop) {
       m_widget->hide();
       m_timer->stop();
    }
    QString udi = QString(m_hotplugModel->data(index, SolidUdiRole).toString());
    QStringList desktop_files = m_hotplugModel->data(index, PredicateFilesRole).toStringList();
    kDebug() << "DeviceNotifier:: call Solid Ui Server with params :" << udi \
             << "," << desktop_files;
    QDBusInterface soliduiserver("org.kde.kded", "/modules/soliduiserver", "org.kde.SolidUiServer");
    QDBusReply<void> reply = soliduiserver.call("showActionsDialog", udi, desktop_files);
}

void DeviceNotifier::onTimerExpired()
{
    m_timer->stop();
    m_widget->hide();
}

#include "devicenotifier.moc"
