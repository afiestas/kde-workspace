/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 1999, 2000 Matthias Ettrich <ettrich@kde.org>
Copyright (C) 2003 Lubos Lunak <l.lunak@kde.org>
Copyright (C) 2009 Lucas Murray <lmurray@undefinedfire.com>

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

#ifndef KWIN_WORKSPACE_H
#define KWIN_WORKSPACE_H

#include <QTimer>
#include <QVector>
#include <kshortcut.h>
#include <QCursor>
#include <netwm.h>
#include <kxmessages.h>
#include <QElapsedTimer>
#include <kmanagerselection.h>

// need to include utils.h before we use the ifdefs
#include "utils.h"
#ifdef KWIN_BUILD_ACTIVITIES
#include <KActivities/Controller>
#endif

#include "plugins.h"
#include "kdecoration.h"
#include "kdecorationfactory.h"
#include "sm.h"
#include "killwindow.h"

#include <X11/Xlib.h>

// TODO: Cleanup the order of things in this .h file

class QMenu;
class QActionGroup;
class QStringList;
class KConfig;
class KActionCollection;
class KStartupInfo;
class KStartupInfoId;
class KStartupInfoData;

namespace KWin
{

#ifdef KWIN_BUILD_TABBOX
namespace TabBox
{
class TabBox;
}
#endif

class Client;
class Outline;
class RootInfo;
class PluginMgr;
class Rules;
class Scripting;
class UserActionsMenu;
class WindowRules;
class Compositor;

class Workspace : public QObject, public KDecorationDefines
{
    Q_OBJECT
public:
    explicit Workspace(bool restore = false);
    virtual ~Workspace();

    static Workspace* self() {
        return _self;
    }

    bool workspaceEvent(XEvent*);
    bool workspaceEvent(QEvent*);

    KDecoration* createDecoration(KDecorationBridge* bridge);
    bool hasDecorationPlugin() const;

    bool hasClient(const Client*);

    template<typename T> Client* findClient(T predicate) const;
    template<typename T1, typename T2> void forEachClient(T1 procedure, T2 predicate);
    template<typename T> void forEachClient(T procedure);
    template<typename T> Unmanaged* findUnmanaged(T predicate) const;
    template<typename T1, typename T2> void forEachUnmanaged(T1 procedure, T2 predicate);
    template<typename T> void forEachUnmanaged(T procedure);

    QRect clientArea(clientAreaOption, const QPoint& p, int desktop) const;
    QRect clientArea(clientAreaOption, const Client* c) const;
    QRect clientArea(clientAreaOption, int screen, int desktop) const;

    QRegion restrictedMoveArea(int desktop, StrutAreas areas = StrutAreaAll) const;

    /**
     * @internal
     */
    void killWindowId(Window window);

    void killWindow() {
        slotKillWindow();
    }

    bool initializing() const;

    /**
     * Returns the active client, i.e. the client that has the focus (or None
     * if no client has the focus)
     */
    Client* activeClient() const;
    /**
     * Client that was activated, but it's not yet really activeClient(), because
     * we didn't process yet the matching FocusIn event. Used mostly in focus
     * stealing prevention code.
     */
    Client* mostRecentlyActivatedClient() const;

    Client* clientUnderMouse(int screen) const;

    void activateClient(Client*, bool force = false);
    void requestFocus(Client* c, bool force = false);
    void takeActivity(Client* c, int flags, bool handled);   // Flags are ActivityFlags
    void handleTakeActivity(Client* c, Time timestamp, int flags);   // Flags are ActivityFlags
    bool allowClientActivation(const Client* c, Time time = -1U, bool focus_in = false,
                               bool ignore_desktop = false);
    void restoreFocus();
    void gotFocusIn(const Client*);
    void setShouldGetFocus(Client*);
    bool activateNextClient(Client* c);
    bool focusChangeEnabled() {
        return block_focus == 0;
    }

    void updateColormap();

    /**
     * Indicates that the client c is being moved around by the user.
     */
    void setClientIsMoving(Client* c);

    QPoint adjustClientPosition(Client* c, QPoint pos, bool unrestricted, double snapAdjust = 1.0);
    QRect adjustClientSize(Client* c, QRect moveResizeGeom, int mode);
    void raiseClient(Client* c, bool nogroup = false);
    void lowerClient(Client* c, bool nogroup = false);
    void raiseClientRequest(Client* c, NET::RequestSource src, Time timestamp);
    void lowerClientRequest(Client* c, NET::RequestSource src, Time timestamp);
    void restackClientUnderActive(Client*);
    void restack(Client *c, Client *under);
    void updateClientLayer(Client* c);
    void raiseOrLowerClient(Client*);
    void resetUpdateToolWindowsTimer();
    void restoreSessionStackingOrder(Client* c);
    void updateStackingOrder(bool propagate_new_clients = false);
    void forceRestacking();

    void clientHidden(Client*);
    void clientAttentionChanged(Client* c, bool set);

    /**
     * @return List of clients currently managed by Workspace
     **/
    const ClientList &clientList() const {
        return clients;
    }
    /**
     * @return List of unmanaged "clients" currently registered in Workspace
     **/
    const UnmanagedList &unmanagedList() const {
        return unmanaged;
    }
    /**
     * @return List of desktop "clients" currently managed by Workspace
     **/
    const ClientList &desktopList() const {
        return desktops;
    }
    /**
     * @return List of deleted "clients" currently managed by Workspace
     **/
    const DeletedList &deletedList() const {
        return deleted;
    }

    Outline* outline();

#ifdef KWIN_BUILD_SCREENEDGES
    void stackScreenEdgesUnderOverrideRedirect();
#endif

public:
    QPoint cascadeOffset(const Client *c) const;

private:
    QString activity_;
    QStringList allActivities_, openActivities_;
#ifdef KWIN_BUILD_ACTIVITIES
    KActivities::Controller activityController_;
#endif

    Outline* m_outline;
    Compositor *m_compositor;

    //-------------------------------------------------
    // Unsorted

public:
    int activeScreen() const;
    int numScreens() const;
    void checkActiveScreen(const Client* c);
    bool isOnCurrentHead();
    void setActiveScreenMouse(const QPoint& mousepos);
    QRect screenGeometry(int screen) const;
    int screenNumber(const QPoint& pos) const;
    QString currentActivity() const {
        return activity_;
    }
    QStringList activityList() const {
        return allActivities_;
    }
    const QStringList &openActivities() const {
        return openActivities_;
    }
#ifdef KWIN_BUILD_ACTIVITIES
    void updateActivityList(bool running, bool updateCurrent, QObject *target = NULL, QString slot = QString());
#endif
    // True when performing Workspace::updateClientArea().
    // The calls below are valid only in that case.
    bool inUpdateClientArea() const;
    QRegion previousRestrictedMoveArea(int desktop, StrutAreas areas = StrutAreaAll) const;
    QVector< QRect > previousScreenSizes() const;
    int oldDisplayWidth() const;
    int oldDisplayHeight() const;

    // Tab box
#ifdef KWIN_BUILD_TABBOX
    TabBox::TabBox *tabBox() const;
#endif
    bool hasTabBox() const;

    KActionCollection* actionCollection() const {
        return keys;
    }
    KActionCollection* disableShortcutsKeys() const {
        return disable_shortcuts_keys;
    }
    KActionCollection* clientKeys() const {
        return client_keys;
    }

#ifdef KWIN_BUILD_SCRIPTING
    Scripting *scripting() {
        return m_scripting;
    }
#endif

    /**
     * Returns the list of clients sorted in stacking order, with topmost client
     * at the last position
     */
    const ToplevelList& stackingOrder() const;
    ToplevelList xStackingOrder() const;
    ClientList ensureStackingOrder(const ClientList& clients) const;

    Client* topClientOnDesktop(int desktop, int screen, bool unconstrained = false,
                               bool only_normal = true) const;
    Client* findDesktop(bool topmost, int desktop) const;
    void sendClientToDesktop(Client* c, int desktop, bool dont_activate);
    void toggleClientOnActivity(Client* c, const QString &activity, bool dont_activate);
    void windowToPreviousDesktop(Client* c);
    void windowToNextDesktop(Client* c);
    void sendClientToScreen(Client* c, int screen);

    // KDE4 remove me - And it's also in the DCOP interface :(
    void showWindowMenuAt(unsigned long id, int x, int y);


    /**
     * Shows the menu operations menu for the client and makes it active if
     * it's not already.
     */
    void showWindowMenu(const QRect& pos, Client* cl);
    /**
     * Backwards compatibility.
     */
    void showWindowMenu(int x, int y, Client* cl);
    void showWindowMenu(QPoint pos, Client* cl);
    const UserActionsMenu *userActionsMenu() const {
        return m_userActionsMenu;
    }

    void updateMinimizedOfTransients(Client*);
    void updateOnAllDesktopsOfTransients(Client*);
    void updateOnAllActivitiesOfTransients(Client*);
    void checkTransients(Window w);

    void performWindowOperation(Client* c, WindowOperation op);

    void storeSession(KConfig* config, SMSavePhase phase);
    void storeClient(KConfigGroup &cg, int num, Client *c);
    void storeSubSession(const QString &name, QSet<QByteArray> sessionIds);

    SessionInfo* takeSessionInfo(Client*);
    WindowRules findWindowRules(const Client*, bool);
    void rulesUpdated();
    void discardUsedWindowRules(Client* c, bool withdraw);
    void disableRulesUpdates(bool disable);
    bool rulesUpdatesDisabled() const;

    bool hasDecorationShadows() const;
    Qt::Corner decorationCloseButtonCorner();
    bool decorationHasAlpha() const;
    bool decorationSupportsAnnounceAlpha() const;
    bool decorationSupportsTabbing() const; // Returns true if the decoration supports tabs.
    bool decorationSupportsFrameOverlap() const;
    bool decorationSupportsBlurBehind() const;

    // D-Bus interface
    /**
     * @deprecated
     * @todo: remove KDE5
     **/
    QList<int> decorationSupportedColors() const;
    bool waitForCompositingSetup();
    bool stopActivity(const QString &id);
    bool startActivity(const QString &id);
    QString supportInformation() const;

    void setCurrentScreen(int new_screen);

    void setShowingDesktop(bool showing);
    void resetShowingDesktop(bool keep_hidden);
    bool showingDesktop() const;

    void sendPingToWindow(Window w, Time timestamp);   // Called from Client::pingWindow()
    void sendTakeActivity(Client* c, Time timestamp, long flags);   // Called from Client::takeActivity()

    void removeClient(Client*, allowed_t);   // Only called from Client::destroyClient() or Client::releaseWindow()
    void setActiveClient(Client*, allowed_t);
    Group* findGroup(Window leader) const;
    void addGroup(Group* group, allowed_t);
    void removeGroup(Group* group, allowed_t);
    Group* findClientLeaderGroup(const Client* c) const;

    void removeUnmanaged(Unmanaged*, allowed_t);   // Only called from Unmanaged::release()
    void removeDeleted(Deleted*, allowed_t);
    void addDeleted(Deleted*, Toplevel*, allowed_t);

    bool checkStartupNotification(Window w, KStartupInfoId& id, KStartupInfoData& data);

    void focusToNull(); // SELI TODO: Public?

    bool forcedGlobalMouseGrab() const;
    void clientShortcutUpdated(Client* c);
    bool shortcutAvailable(const KShortcut& cut, Client* ignore = NULL) const;
    bool globalShortcutsDisabled() const;
    void disableGlobalShortcuts(bool disable);
    void disableGlobalShortcutsForClient(bool disable);

    void sessionSaveStarted();
    void sessionSaveDone();
    void setWasUserInteraction();
    bool wasUserInteraction() const;
    bool sessionSaving() const;

    int packPositionLeft(const Client* cl, int oldx, bool left_edge) const;
    int packPositionRight(const Client* cl, int oldx, bool right_edge) const;
    int packPositionUp(const Client* cl, int oldy, bool top_edge) const;
    int packPositionDown(const Client* cl, int oldy, bool bottom_edge) const;

    void cancelDelayFocus();
    void requestDelayFocus(Client*);
    void updateFocusMousePosition(const QPoint& pos);
    QPoint focusMousePosition() const;

    Client* getMovingClient() {
        return movingClient;
    }

    /**
     * @returns Whether we have a Compositor and it is active (Scene created)
     **/
    bool compositing() const;

public slots:
    // Keybindings
    //void slotSwitchToWindow( int );
    void slotWindowToDesktop();

    //void slotWindowToListPosition( int );
    void slotSwitchToScreen();
    void slotWindowToScreen();
    void slotSwitchToNextScreen();
    void slotWindowToNextScreen();
    void slotToggleShowDesktop();

    void slotWindowMaximize();
    void slotWindowMaximizeVertical();
    void slotWindowMaximizeHorizontal();
    void slotWindowMinimize();
    void slotWindowShade();
    void slotWindowRaise();
    void slotWindowLower();
    void slotWindowRaiseOrLower();
    void slotActivateAttentionWindow();
    void slotWindowPackLeft();
    void slotWindowPackRight();
    void slotWindowPackUp();
    void slotWindowPackDown();
    void slotWindowGrowHorizontal();
    void slotWindowGrowVertical();
    void slotWindowShrinkHorizontal();
    void slotWindowShrinkVertical();
    void slotWindowQuickTileLeft();
    void slotWindowQuickTileRight();
    void slotWindowQuickTileTopLeft();
    void slotWindowQuickTileTopRight();
    void slotWindowQuickTileBottomLeft();
    void slotWindowQuickTileBottomRight();

    void slotSwitchWindowUp();
    void slotSwitchWindowDown();
    void slotSwitchWindowRight();
    void slotSwitchWindowLeft();

    void slotIncreaseWindowOpacity();
    void slotLowerWindowOpacity();

    void slotWindowOperations();
    void slotWindowClose();
    void slotWindowMove();
    void slotWindowResize();
    void slotWindowAbove();
    void slotWindowBelow();
    void slotWindowOnAllDesktops();
    void slotWindowFullScreen();
    void slotWindowNoBorder();

    void slotWindowToNextDesktop();
    void slotWindowToPreviousDesktop();
    void slotWindowToDesktopRight();
    void slotWindowToDesktopLeft();
    void slotWindowToDesktopUp();
    void slotWindowToDesktopDown();

    void slotDisableGlobalShortcuts();

    void slotSettingsChanged(int category);

    void reconfigure();
    void slotReconfigure();
    void slotCompositingToggled();

    void slotKillWindow();

    void slotSetupWindowShortcut();
    void setupWindowShortcutDone(bool);
    void slotToggleCompositing();
    void slotInvertScreen();

    void updateClientArea();

    void slotActivateNextTab(); // Slot to move left the active Client.
    void slotActivatePrevTab(); // Slot to move right the active Client.
    void slotUntab(); // Slot to remove the active client from its group.

private slots:
    void desktopResized();
    void screenChangeTimeout();
    void slotUpdateToolWindows();
    void delayFocus();
    void gotTemporaryRulesMessage(const QString&);
    void cleanupTemporaryRules();
    void writeWindowRules();
    void slotBlockShortcuts(int data);
    void slotReloadConfig();
    void updateCurrentActivity(const QString &new_activity);
    void slotActivityRemoved(const QString &activity);
    void slotActivityAdded(const QString &activity);
    void reallyStopActivity(const QString &id);   //dbus deadlocks suck
    void handleActivityReply();
    // virtual desktop handling
    void moveClientsFromRemovedDesktops();
    void slotDesktopCountChanged(uint previousCount, uint newCount);
    void slotCurrentDesktopChanged(uint oldDesktop, uint newDesktop);

Q_SIGNALS:
    /**
     * Emitted after the Workspace has setup the complete initialization process.
     * This can be used to connect to for performing post-workspace initialization.
     **/
    void workspaceInitialized();

    //Signals required for the scripting interface
signals:
    void desktopPresenceChanged(KWin::Client*, int);
    void currentDesktopChanged(int, KWin::Client*);
    void clientAdded(KWin::Client*);
    void clientRemoved(KWin::Client*);
    void clientActivated(KWin::Client*);
    void clientDemandsAttentionChanged(KWin::Client*, bool);
    void groupAdded(KWin::Group*);
    void unmanagedAdded(KWin::Unmanaged*);
    void deletedRemoved(KWin::Deleted*);
    void propertyNotify(long a);
    void configChanged();
    void reinitializeCompositing();
    /**
     * This signal is emitted when the global
     * activity is changed
     * @param id id of the new current activity
     */
    void currentActivityChanged(const QString &id);
    /**
     * This signal is emitted when a new activity is added
     * @param id id of the new activity
     */
    void activityAdded(const QString &id);
    /**
     * This signal is emitted when the activity
     * is removed
     * @param id id of the removed activity
     */
    void activityRemoved(const QString &id);
    /**
     * This signels is emitted when ever the stacking order is change, ie. a window is risen
     * or lowered
     */
    void stackingOrderChanged();

private:
    void init();
    void initShortcuts();
    void setupWindowShortcut(Client* c);
    enum Direction {
        DirectionNorth,
        DirectionEast,
        DirectionSouth,
        DirectionWest
    };
    void switchWindow(Direction direction);

    void propagateClients(bool propagate_new_clients);   // Called only from updateStackingOrder
    ToplevelList constrainedStackingOrder();
    void raiseClientWithinApplication(Client* c);
    void lowerClientWithinApplication(Client* c);
    bool allowFullClientRaising(const Client* c, Time timestamp);
    bool keepTransientAbove(const Client* mainwindow, const Client* transient);
    void blockStackingUpdates(bool block);
    void updateToolWindows(bool also_hide);
    void fixPositionAfterCrash(xcb_window_t w, const xcb_get_geometry_reply_t *geom);
    void saveOldScreenSizes();

    /// This is the right way to create a new client
    Client* createClient(Window w, bool is_mapped);
    void addClient(Client* c, allowed_t);
    Unmanaged* createUnmanaged(Window w);
    void addUnmanaged(Unmanaged* c, allowed_t);

    Window findSpecialEventWindow(XEvent* e);

    //---------------------------------------------------------------------

    void closeActivePopup();
    void updateClientArea(bool force);
    void resetClientAreas(uint desktopCount);
    void updateClientVisibilityOnDesktopChange(uint oldDesktop, uint newDesktop);
    void activateClientOnNewDesktop(uint desktop);
    Client *findClientToActivateOnDesktop(uint desktop);

    QWidget* active_popup;
    Client* active_popup_client;

    void loadSessionInfo();
    void addSessionInfo(KConfigGroup &cg);
    void loadSubSessionInfo(const QString &name);

    void loadWindowRules();
    void editWindowRules(Client* c, bool whole_app);

    QList<SessionInfo*> session;
    QList<Rules*> rules;
    KXMessages temporaryRulesMessages;
    QTimer rulesUpdatedTimer;
    QTimer screenChangedTimer;
    bool rules_updates_disabled;
    static const char* windowTypeToTxt(NET::WindowType type);
    static NET::WindowType txtToWindowType(const char* txt);
    static bool sessionInfoWindowTypeMatch(Client* c, SessionInfo* info);

    Client* active_client;
    Client* last_active_client;
    Client* most_recently_raised; // Used ONLY by raiseOrLowerClient()
    Client* movingClient;
    Client* pending_take_activity;
    int active_screen;

    // Delay(ed) window focus timer and client
    QTimer* delayFocusTimer;
    Client* delayfocus_client;
    QPoint focusMousePos;

    ClientList clients;
    ClientList desktops;
    UnmanagedList unmanaged;
    DeletedList deleted;

    ToplevelList unconstrained_stacking_order; // Topmost last
    ToplevelList stacking_order; // Topmost last
    bool force_restacking;
    mutable ToplevelList x_stacking; // From XQueryTree()
    mutable bool x_stacking_dirty;
    ClientList should_get_focus; // Last is most recent
    ClientList attention_chain;

    bool showing_desktop;
    ClientList showing_desktop_clients;
    int block_showing_desktop;

    GroupList groups;

    bool was_user_interaction;
    bool session_saving;
    int session_active_client;
    int session_desktop;

    int block_focus;

#ifdef KWIN_BUILD_TABBOX
    TabBox::TabBox* tab_box;
#endif

    /**
     * Holds the menu containing the user actions which is shown
     * on e.g. right click the window decoration.
     **/
    UserActionsMenu *m_userActionsMenu;

    void modalActionsSwitch(bool enabled);

    KActionCollection* keys;
    KActionCollection* client_keys;
    KActionCollection* disable_shortcuts_keys;
    ShortcutDialog* client_keys_dialog;
    Client* client_keys_client;
    bool global_shortcuts_disabled;
    bool global_shortcuts_disabled_for_client;

    PluginMgr* mgr;

    RootInfo* rootInfo;
    QWidget* supportWindow;

    // Colormap handling
    Colormap default_colormap;
    Colormap installed_colormap;

    // Timer to collect requests for 'reconfigure'
    QTimer reconfigureTimer;

    QTimer updateToolWindowsTimer;

    static Workspace* _self;

    bool workspaceInit;

    KStartupInfo* startup;

    QVector<QRect> workarea; // Array of workareas for virtual desktops
    // Array of restricted areas that window cannot be moved into
    QVector<StrutRects> restrictedmovearea;
    // Array of the previous restricted areas that window cannot be moved into
    QVector<StrutRects> oldrestrictedmovearea;
    QVector< QVector<QRect> > screenarea; // Array of workareas per xinerama screen for all virtual desktops
    QVector< QRect > oldscreensizes; // array of previous sizes of xinerama screens
    QSize olddisplaysize; // previous sizes od displayWidth()/displayHeight()

    int set_active_client_recursion;
    int block_stacking_updates; // When > 0, stacking updates are temporarily disabled
    bool blocked_propagating_new_clients; // Propagate also new clients after enabling stacking updates?
    Window null_focus_window;
    bool forced_global_mouse_grab;
    friend class StackingUpdatesBlocker;

    Scripting *m_scripting;

    QScopedPointer<KillWindow> m_windowKiller;

private:
    friend bool performTransiencyCheck();
};

/**
 * Helper for Workspace::blockStackingUpdates() being called in pairs (True/false)
 */
class StackingUpdatesBlocker
{
public:
    explicit StackingUpdatesBlocker(Workspace* w)
        : ws(w) {
        ws->blockStackingUpdates(true);
    }
    ~StackingUpdatesBlocker() {
        ws->blockStackingUpdates(false);
    }

private:
    Workspace* ws;
};

/**
 * NET WM Protocol handler class
 */
class RootInfo : public NETRootInfo
{
private:
    typedef KWin::Client Client;  // Because of NET::Client

public:
    RootInfo(Workspace* ws, Display* dpy, Window w, const char* name, unsigned long pr[],
             int pr_num, int scr = -1);

protected:
    virtual void changeNumberOfDesktops(int n);
    virtual void changeCurrentDesktop(int d);
    virtual void changeActiveWindow(Window w, NET::RequestSource src, Time timestamp, Window active_window);
    virtual void closeWindow(Window w);
    virtual void moveResize(Window w, int x_root, int y_root, unsigned long direction);
    virtual void moveResizeWindow(Window w, int flags, int x, int y, int width, int height);
    virtual void gotPing(Window w, Time timestamp);
    virtual void restackWindow(Window w, RequestSource source, Window above, int detail, Time timestamp);
    virtual void gotTakeActivity(Window w, Time timestamp, long flags);
    virtual void changeShowingDesktop(bool showing);

private:
    Workspace* workspace;
};

//---------------------------------------------------------
// Unsorted

inline bool Workspace::initializing() const
{
    return workspaceInit;
}

inline Client* Workspace::activeClient() const
{
    return active_client;
}

inline Client* Workspace::mostRecentlyActivatedClient() const
{
    return should_get_focus.count() > 0 ? should_get_focus.last() : active_client;
}

inline void Workspace::addGroup(Group* group, allowed_t)
{
    emit groupAdded(group);
    groups.append(group);
}

inline void Workspace::removeGroup(Group* group, allowed_t)
{
    groups.removeAll(group);
}

inline const ToplevelList& Workspace::stackingOrder() const
{
    // TODO: Q_ASSERT( block_stacking_updates == 0 );
    return stacking_order;
}

inline void Workspace::showWindowMenu(QPoint pos, Client* cl)
{
    showWindowMenu(QRect(pos, pos), cl);
}

inline void Workspace::showWindowMenu(int x, int y, Client* cl)
{
    showWindowMenu(QRect(QPoint(x, y), QPoint(x, y)), cl);
}

inline void Workspace::setWasUserInteraction()
{
    was_user_interaction = true;
}

inline bool Workspace::wasUserInteraction() const
{
    return was_user_interaction;
}

inline void Workspace::sessionSaveStarted()
{
    session_saving = true;
}

inline bool Workspace::sessionSaving() const
{
    return session_saving;
}

inline bool Workspace::forcedGlobalMouseGrab() const
{
    return forced_global_mouse_grab;
}

inline bool Workspace::showingDesktop() const
{
    return showing_desktop;
}

inline bool Workspace::globalShortcutsDisabled() const
{
    return global_shortcuts_disabled || global_shortcuts_disabled_for_client;
}

inline bool Workspace::rulesUpdatesDisabled() const
{
    return rules_updates_disabled;
}

inline void Workspace::forceRestacking()
{
    force_restacking = true;
    StackingUpdatesBlocker blocker(this);   // Do restacking if not blocked
}

inline void Workspace::updateFocusMousePosition(const QPoint& pos)
{
    focusMousePos = pos;
}

inline QPoint Workspace::focusMousePosition() const
{
    return focusMousePos;
}

template< typename T >
inline Client* Workspace::findClient(T predicate) const
{
    if (Client* ret = findClientInList(clients, predicate))
        return ret;
    if (Client* ret = findClientInList(desktops, predicate))
        return ret;
    return NULL;
}

template< typename T1, typename T2 >
inline void Workspace::forEachClient(T1 procedure, T2 predicate)
{
    for (ClientList::ConstIterator it = clients.constBegin(); it != clients.constEnd(); ++it)
        if (predicate(const_cast<const Client*>(*it)))
            procedure(*it);
    for (ClientList::ConstIterator it = desktops.constBegin(); it != desktops.constEnd(); ++it)
        if (predicate(const_cast<const Client*>(*it)))
            procedure(*it);
}

template< typename T >
inline void Workspace::forEachClient(T procedure)
{
    return forEachClient(procedure, TruePredicate());
}

template< typename T >
inline Unmanaged* Workspace::findUnmanaged(T predicate) const
{
    return findUnmanagedInList(unmanaged, predicate);
}

template< typename T1, typename T2 >
inline void Workspace::forEachUnmanaged(T1 procedure, T2 predicate)
{
    for (UnmanagedList::ConstIterator it = unmanaged.constBegin(); it != unmanaged.constEnd(); ++it)
        if (predicate(const_cast<const Unmanaged*>(*it)))
            procedure(*it);
}

template< typename T >
inline void Workspace::forEachUnmanaged(T procedure)
{
    return forEachUnmanaged(procedure, TruePredicate());
}

KWIN_COMPARE_PREDICATE(ClientMatchPredicate, Client, const Client*, cl == value);

inline bool Workspace::hasClient(const Client* c)
{
    return findClient(ClientMatchPredicate(c));
}

inline bool Workspace::hasDecorationPlugin() const
{
    if (!mgr) {
        return false;
    }
    return !mgr->hasNoDecoration();
}

inline bool Workspace::hasDecorationShadows() const
{
    if (!hasDecorationPlugin()) {
        return false;
    }
    return mgr->factory()->supports(AbilityProvidesShadow);
}

inline Qt::Corner Workspace::decorationCloseButtonCorner()
{
    if (!hasDecorationPlugin()) {
        return Qt::TopRightCorner;
    }
    return mgr->factory()->closeButtonCorner();
}

inline bool Workspace::decorationHasAlpha() const
{
    if (!hasDecorationPlugin()) {
        return false;
    }
    return mgr->factory()->supports(AbilityUsesAlphaChannel);
}

inline bool Workspace::decorationSupportsAnnounceAlpha() const
{
    if (!hasDecorationPlugin()) {
        return false;
    }
    return mgr->factory()->supports(AbilityAnnounceAlphaChannel);
}

inline bool Workspace::decorationSupportsTabbing() const
{
    if (!hasDecorationPlugin()) {
        return false;
    }
    return mgr->factory()->supports(AbilityTabbing);
}

inline bool Workspace::decorationSupportsFrameOverlap() const
{
    if (!hasDecorationPlugin()) {
        return false;
    }
    return mgr->factory()->supports(AbilityExtendIntoClientArea);
}

inline bool Workspace::decorationSupportsBlurBehind() const
{
    if (!hasDecorationPlugin()) {
        return false;
    }
    return mgr->factory()->supports(AbilityUsesBlurBehind);
}

} // namespace

#endif
