/*
 * This file was generated by qdbusxml2cpp version 0.7
 * Command line was: qdbusxml2cpp -N -m -p nm-ip4-configinterface /space/kde/sources/trunk/KDE/kdebase/workspace/solid/networkmanager-0.7/dbus/introspection/nm-ip4-config.xml
 *
 * qdbusxml2cpp is Copyright (C) 2009 Nokia Corporation and/or its subsidiary(-ies).
 *
 * This is an auto-generated file.
 * Do not edit! All changes made to it will be lost.
 */

#ifndef NM_IP4_CONFIGINTERFACE_H
#define NM_IP4_CONFIGINTERFACE_H

#include <QtCore/QObject>
#include <QtCore/QByteArray>
#include <QtCore/QList>
#include <QtCore/QMap>
#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtCore/QVariant>
#include <QtDBus/QtDBus>

#include "generic-types.h"
/*
 * Proxy class for interface org.freedesktop.NetworkManager.IP4Config
 */
class OrgFreedesktopNetworkManagerIP4ConfigInterface: public QDBusAbstractInterface
{
    Q_OBJECT
public:
    static inline const char *staticInterfaceName()
    { return "org.freedesktop.NetworkManager.IP4Config"; }

public:
    OrgFreedesktopNetworkManagerIP4ConfigInterface(const QString &service, const QString &path, const QDBusConnection &connection, QObject *parent = 0);

    ~OrgFreedesktopNetworkManagerIP4ConfigInterface();

    Q_PROPERTY(UIntListList Addresses READ addresses)
    inline UIntListList addresses() const
    { return qvariant_cast< UIntListList >(internalPropGet("Addresses")); }

    Q_PROPERTY(QStringList Domains READ domains)
    inline QStringList domains() const
    { return qvariant_cast< QStringList >(internalPropGet("Domains")); }

    Q_PROPERTY(UIntList Nameservers READ nameservers)
    inline UIntList nameservers() const
    { return qvariant_cast< UIntList >(internalPropGet("Nameservers")); }

    Q_PROPERTY(UIntListList Routes READ routes)
    inline UIntListList routes() const
    { return qvariant_cast< UIntListList >(internalPropGet("Routes")); }

    Q_PROPERTY(UIntList WinsServers READ winsServers)
    inline UIntList winsServers() const
    { return qvariant_cast< UIntList >(internalPropGet("WinsServers")); }

public Q_SLOTS: // METHODS
Q_SIGNALS: // SIGNALS
};

#endif
