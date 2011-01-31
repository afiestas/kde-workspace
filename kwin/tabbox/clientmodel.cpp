/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2009 Martin Gräßlin <kde@martin-graesslin.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/

// own
#include "clientmodel.h"
// tabbox
#include "tabboxconfig.h"
#include "tabboxhandler.h"
// Qt
#include <QTextStream>
// KDE
#include <KLocale>
// other
#include <math.h>

namespace KWin
{
namespace TabBox
{

ClientModel::ClientModel(QObject* parent)
    : QAbstractItemModel(parent)
{
}

ClientModel::~ClientModel()
{
}

QVariant ClientModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid())
        return QVariant();

    if (m_clientList.isEmpty()) {
        if (role == EmptyRole)
            return true;
        else
            return i18n("*** No Windows ***");
    }

    int clientIndex = index.row() * columnCount() + index.column();
    if (clientIndex >= m_clientList.count())
        return QVariant();
    switch(role) {
    case Qt::DisplayRole:
    case CaptionRole:
        return m_clientList[ clientIndex ]->caption();
    case ClientRole:
        return qVariantFromValue((void*)m_clientList[ clientIndex ]);
    case DesktopNameRole: {
        return tabBox->desktopName(m_clientList[ clientIndex ]);
    }
    case EmptyRole:
        return false;
    case WIdRole:
        return qulonglong(m_clientList[ clientIndex ]->window());
    case MinimizedRole:
        return m_clientList[ clientIndex ]->isMinimized();
    default:
        return QVariant();
    }
}

int ClientModel::columnCount(const QModelIndex& parent) const
{
    Q_UNUSED(parent)
    int count = 1;
    switch(tabBox->config().layout()) {
    case TabBoxConfig::HorizontalLayout:
        count = m_clientList.count();
        break;
    case TabBoxConfig::VerticalLayout:
        count = 1;
        break;
    case TabBoxConfig::HorizontalVerticalLayout:
        count = qRound(sqrt(float(m_clientList.count())));
        if (count * count < m_clientList.count())
            count++;
        break;
    }
    return qMax(count, 1);
}

int ClientModel::rowCount(const QModelIndex& parent) const
{
    Q_UNUSED(parent)
    int count = 1;
    switch(tabBox->config().layout()) {
    case TabBoxConfig::HorizontalLayout:
        count = 1;
        break;
    case TabBoxConfig::VerticalLayout:
        count = m_clientList.count();
        break;
    case TabBoxConfig::HorizontalVerticalLayout:
        count = qRound(sqrt(float(m_clientList.count())));
        break;
    }
    return qMax(count, 1);
}

QModelIndex ClientModel::parent(const QModelIndex& child) const
{
    Q_UNUSED(child)
    return QModelIndex();
}

QModelIndex ClientModel::index(int row, int column, const QModelIndex& parent) const
{
    Q_UNUSED(parent)
    int index = row * columnCount() + column;
    if (index >= m_clientList.count() && !m_clientList.isEmpty())
        return QModelIndex();
    return createIndex(row, column);
}

QModelIndex ClientModel::index(TabBoxClient* client) const
{
    if (!m_clientList.contains(client))
        return QModelIndex();
    int index = m_clientList.indexOf(client);
    int row = index / columnCount();
    int column = index % columnCount();
    return createIndex(row, column);
}

void ClientModel::createClientList(bool partialReset)
{
    createClientList(tabBox->currentDesktop(), partialReset);
}

void ClientModel::createClientList(int desktop, bool partialReset)
{
    TabBoxClient* start = tabBox->activeClient();
    // TODO: new clients are not added at correct position
    if (partialReset && !m_clientList.isEmpty())
        start = m_clientList.first();

    m_clientList.clear();

    switch(tabBox->config().clientSwitchingMode()) {
    case TabBoxConfig::FocusChainSwitching: {
        TabBoxClient* c = tabBox->nextClientFocusChain(start);
        TabBoxClient* stop = c;
        while (c) {
            TabBoxClient* add = tabBox->clientToAddToList(c, desktop,
                                tabBox->config().clientListMode() == TabBoxConfig::AllDesktopsClientList ||
                                tabBox->config().clientListMode() == TabBoxConfig::AllDesktopsApplicationList);
            if (add != NULL) {
                if (start == add) {
                    m_clientList.removeAll(add);
                    m_clientList.prepend(add);
                } else
                    m_clientList += add;
            }
            c = tabBox->nextClientFocusChain(c);

            if (c == stop)
                break;
        }
        break;
    }
    case TabBoxConfig::StackingOrderSwitching: {
        // TODO: needs improvement
        TabBoxClientList stacking = tabBox->stackingOrder();
        TabBoxClient* c = stacking.first();
        TabBoxClient* stop = c;
        int index = 0;
        while (c) {
            TabBoxClient* add = tabBox->clientToAddToList(c, desktop,
                                tabBox->config().clientListMode() == TabBoxConfig::AllDesktopsClientList ||
                                tabBox->config().clientListMode() == TabBoxConfig::AllDesktopsApplicationList);
            if (add != NULL) {
                if (start == add) {
                    m_clientList.removeAll(add);
                    m_clientList.prepend(add);
                } else
                    m_clientList += add;
            }
            if (index >= stacking.size() - 1) {
                c = NULL;
            } else {
                c = stacking[++index];
            }

            if (c == stop)
                break;
        }
        break;
    }
    }
    if (tabBox->config().isShowDesktop()) {
        TabBoxClient* desktopClient = tabBox->desktopClient();
        if (desktopClient)
            m_clientList.append(desktopClient);
    }
    reset();
}

} // namespace Tabbox
} // namespace KWin
