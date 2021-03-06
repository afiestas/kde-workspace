/*
 * This file was generated by qdbusxml2cpp version 0.7
 * Command line was: qdbusxml2cpp -N -m -p nm-manager-clientinterface introspection/nm-manager-client.xml
 *
 * qdbusxml2cpp is Copyright (C) 2011 Nokia Corporation and/or its subsidiary(-ies).
 *
 * This is an auto-generated file.
 * Do not edit! All changes made to it will be lost.
 */

#ifndef NM_MANAGER_CLIENTINTERFACE_H_1307846322
#define NM_MANAGER_CLIENTINTERFACE_H_1307846322

#include <QtCore/QObject>
#include <QtCore/QByteArray>
#include <QtCore/QList>
#include <QtCore/QMap>
#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtCore/QVariant>
#include <QtDBus/QtDBus>

/*
 * Proxy class for interface org.freedesktop.NetworkManager
 */
class OrgFreedesktopNetworkManagerInterface: public QDBusAbstractInterface
{
    Q_OBJECT
public:
    static inline const char *staticInterfaceName()
    { return "org.freedesktop.NetworkManager"; }

public:
    OrgFreedesktopNetworkManagerInterface(const QString &service, const QString &path, const QDBusConnection &connection, QObject *parent = 0);

    ~OrgFreedesktopNetworkManagerInterface();

    Q_PROPERTY(QList<QDBusObjectPath> ActiveConnections READ activeConnections)
    inline QList<QDBusObjectPath> activeConnections() const
    { return qvariant_cast< QList<QDBusObjectPath> >(property("ActiveConnections")); }

    Q_PROPERTY(bool NetworkingEnabled READ networkingEnabled)
    inline bool networkingEnabled() const
    { return qvariant_cast< bool >(property("NetworkingEnabled")); }

    Q_PROPERTY(uint State READ state)
    inline uint state() const
    { return qvariant_cast< uint >(property("State")); }

    Q_PROPERTY(QString Version READ version)
    inline QString version() const
    { return qvariant_cast< QString >(property("Version")); }

    Q_PROPERTY(bool WirelessEnabled READ wirelessEnabled WRITE setWirelessEnabled)
    inline bool wirelessEnabled() const
    { return qvariant_cast< bool >(property("WirelessEnabled")); }
    inline void setWirelessEnabled(bool value)
    { setProperty("WirelessEnabled", qVariantFromValue(value)); }

    Q_PROPERTY(bool WirelessHardwareEnabled READ wirelessHardwareEnabled)
    inline bool wirelessHardwareEnabled() const
    { return qvariant_cast< bool >(property("WirelessHardwareEnabled")); }

    Q_PROPERTY(bool WwanEnabled READ wwanEnabled WRITE setWwanEnabled)
    inline bool wwanEnabled() const
    { return qvariant_cast< bool >(property("WwanEnabled")); }
    inline void setWwanEnabled(bool value)
    { setProperty("WwanEnabled", qVariantFromValue(value)); }

    Q_PROPERTY(bool WwanHardwareEnabled READ wwanHardwareEnabled)
    inline bool wwanHardwareEnabled() const
    { return qvariant_cast< bool >(property("WwanHardwareEnabled")); }

public Q_SLOTS: // METHODS
    inline QDBusPendingReply<QDBusObjectPath> ActivateConnection(const QString &service_name, const QDBusObjectPath &connection, const QDBusObjectPath &device, const QDBusObjectPath &specific_object)
    {
        QList<QVariant> argumentList;
        argumentList << qVariantFromValue(service_name) << qVariantFromValue(connection) << qVariantFromValue(device) << qVariantFromValue(specific_object);
        return asyncCallWithArgumentList(QLatin1String("ActivateConnection"), argumentList);
    }

    inline QDBusPendingReply<> DeactivateConnection(const QDBusObjectPath &active_connection)
    {
        QList<QVariant> argumentList;
        argumentList << qVariantFromValue(active_connection);
        return asyncCallWithArgumentList(QLatin1String("DeactivateConnection"), argumentList);
    }

    inline QDBusPendingReply<> Enable(bool enable)
    {
        QList<QVariant> argumentList;
        argumentList << qVariantFromValue(enable);
        return asyncCallWithArgumentList(QLatin1String("Enable"), argumentList);
    }

    inline QDBusPendingReply<QList<QDBusObjectPath> > GetDevices()
    {
        QList<QVariant> argumentList;
        return asyncCallWithArgumentList(QLatin1String("GetDevices"), argumentList);
    }

    inline QDBusPendingReply<> Sleep(bool sleep)
    {
        QList<QVariant> argumentList;
        argumentList << qVariantFromValue(sleep);
        return asyncCallWithArgumentList(QLatin1String("Sleep"), argumentList);
    }

Q_SIGNALS: // SIGNALS
    void DeviceAdded(const QDBusObjectPath &state);
    void DeviceRemoved(const QDBusObjectPath &state);
    void PropertiesChanged(const QVariantMap &properties);
    void StateChanged(uint state);
};

#endif
