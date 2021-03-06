/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 1999, 2000 Matthias Ettrich <ettrich@kde.org>
Copyright (C) 2003 Lubos Lunak <l.lunak@kde.org>

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

//#define QT_CLEAN_NAMESPACE

#include "workspace.h"

#include <kapplication.h>
#include <kstartupinfo.h>
#include <fixx11h.h>
#include <kconfig.h>
#include <kglobal.h>
#include <QDesktopWidget>
#include <QRegExp>
#include <QPainter>
#include <QBitmap>
#include <QClipboard>
#include <kmenubar.h>
#include <kprocess.h>
#include <kglobalaccel.h>
#include <QToolButton>
#include <kactioncollection.h>
#include <kaction.h>
#include <kconfiggroup.h>
#include <kcmdlineargs.h>
#include <kdeversion.h>
#include <QtDBus/QtDBus>
#include <QtConcurrentRun>

#include "client.h"
#include "composite.h"
#include "focuschain.h"
#ifdef KWIN_BUILD_TABBOX
#include "tabbox.h"
#endif
#include "atoms.h"
#include "cursor.h"
#include "placement.h"
#include "notifications.h"
#include "outline.h"
#include "group.h"
#include "rules.h"
#include "dbusinterface.h"
#include "unmanaged.h"
#include "deleted.h"
#include "effects.h"
#include "overlaywindow.h"
#include "useractions.h"
#include "virtualdesktops.h"
#include "xcbutils.h"
#include <kwinglplatform.h>
#include <kwinglutils.h>
#ifdef KWIN_BUILD_SCREENEDGES
#include "screenedge.h"
#endif
#ifdef KWIN_BUILD_SCRIPTING
#include "scripting/scripting.h"
#endif
#ifdef KWIN_BUILD_KAPPMENU
#include "appmenu.h"
#endif

#include <X11/extensions/shape.h>
#include <X11/keysym.h>
#include <X11/keysymdef.h>
#include <X11/cursorfont.h>
#include <stdio.h>
#include <kglobalsettings.h>
#include <kwindowsystem.h>
#include <kwindowinfo.h>

namespace KWin
{

extern int screen_number;
extern bool is_multihead;

Workspace* Workspace::_self = 0;

Workspace::Workspace(bool restore)
    : QObject(0)
    , m_compositor(NULL)
    // Unsorted
    , active_popup(NULL)
    , active_popup_client(NULL)
    , temporaryRulesMessages("_KDE_NET_WM_TEMPORARY_RULES", NULL)
    , rules_updates_disabled(false)
    , active_client(0)
    , last_active_client(0)
    , most_recently_raised(0)
    , movingClient(0)
    , pending_take_activity(NULL)
    , active_screen(0)
    , delayfocus_client(0)
    , force_restacking(false)
    , x_stacking_dirty(true)
    , showing_desktop(false)
    , block_showing_desktop(0)
    , was_user_interaction(false)
    , session_saving(false)
    , block_focus(0)
#ifdef KWIN_BUILD_TABBOX
    , tab_box(0)
#endif
    , m_userActionsMenu(new UserActionsMenu(this))
    , keys(0)
    , client_keys(NULL)
    , disable_shortcuts_keys(NULL)
    , client_keys_dialog(NULL)
    , client_keys_client(NULL)
    , global_shortcuts_disabled(false)
    , global_shortcuts_disabled_for_client(false)
    , workspaceInit(true)
    , startup(0)
    , set_active_client_recursion(0)
    , block_stacking_updates(0)
    , forced_global_mouse_grab(false)
    , m_scripting(NULL)
{
    // If KWin was already running it saved its configuration after loosing the selection -> Reread
    QFuture<void> reparseConfigFuture = QtConcurrent::run(options, &Options::reparseConfiguration);

#ifdef KWIN_BUILD_KAPPMENU
    ApplicationMenu::create(this);
#endif

    _self = this;

    // first initialize the extensions
    Extensions::init();
    Xcb::Extensions::self();

    // start the cursor support
    Cursor::create(this);

    // PluginMgr needs access to the config file, so we need to wait for it for finishing
    reparseConfigFuture.waitForFinished();
    options->loadConfig();
    options->loadCompositingConfig(false);
    mgr = new PluginMgr;
    default_colormap = DefaultColormap(display(), screen_number);
    installed_colormap = default_colormap;

    connect(&temporaryRulesMessages, SIGNAL(gotMessage(QString)),
            this, SLOT(gotTemporaryRulesMessage(QString)));
    connect(&rulesUpdatedTimer, SIGNAL(timeout()), this, SLOT(writeWindowRules()));
    updateXTime(); // Needed for proper initialization of user_time in Client ctor

    delayFocusTimer = 0;

    if (restore)
        loadSessionInfo();

    loadWindowRules();

    // Call this before XSelectInput() on the root window
    startup = new KStartupInfo(
        KStartupInfo::DisableKWinModule | KStartupInfo::AnnounceSilenceChanges, this);

    // Select windowmanager privileges
    XSelectInput(display(), rootWindow(),
                 KeyPressMask |
                 PropertyChangeMask |
                 ColormapChangeMask |
                 SubstructureRedirectMask |
                 SubstructureNotifyMask |
                 FocusChangeMask | // For NotifyDetailNone
                 ExposureMask
                );

#ifdef KWIN_BUILD_SCREENEDGES
    ScreenEdges::create(this);
#endif

    // VirtualDesktopManager needs to be created prior to init shortcuts
    // and prior to TabBox, due to TabBox connecting to signals
    // actual initialization happens in init()
    VirtualDesktopManager::create(this);

#ifdef KWIN_BUILD_TABBOX
    // need to create the tabbox before compositing scene is setup
    tab_box = new TabBox::TabBox(this);
#endif

    m_compositor = Compositor::createCompositor(this);
    connect(this, SIGNAL(currentDesktopChanged(int,KWin::Client*)), m_compositor, SLOT(addRepaintFull()));
    connect(m_compositor, SIGNAL(compositingToggled(bool)), SLOT(slotCompositingToggled()));

    new DBusInterface(this);

    // Compatibility
    long data = 1;

    XChangeProperty(
        display(),
        rootWindow(),
        atoms->kwin_running,
        atoms->kwin_running,
        32,
        PropModeAppend,
        (unsigned char*)(&data),
        1
    );

    client_keys = new KActionCollection(this);

    m_outline = new Outline();

    initShortcuts();

    init();

    connect(QApplication::desktop(), SIGNAL(screenCountChanged(int)), &screenChangedTimer, SLOT(start()));
    connect(QApplication::desktop(), SIGNAL(resized(int)), &screenChangedTimer, SLOT(start()));

#ifdef KWIN_BUILD_ACTIVITIES
    connect(&activityController_, SIGNAL(currentActivityChanged(QString)), SLOT(updateCurrentActivity(QString)));
    connect(&activityController_, SIGNAL(activityRemoved(QString)), SLOT(slotActivityRemoved(QString)));
    connect(&activityController_, SIGNAL(activityRemoved(QString)), SIGNAL(activityRemoved(QString)));
    connect(&activityController_, SIGNAL(activityAdded(QString)), SLOT(slotActivityAdded(QString)));
    connect(&activityController_, SIGNAL(activityAdded(QString)), SIGNAL(activityAdded(QString)));
    connect(&activityController_, SIGNAL(currentActivityChanged(QString)), SIGNAL(currentActivityChanged(QString)));
#endif

    connect(&screenChangedTimer, SIGNAL(timeout()), SLOT(screenChangeTimeout()));
    screenChangedTimer.setSingleShot(true);
    screenChangedTimer.setInterval(100);
}

void Workspace::screenChangeTimeout()
{
    kDebug() << "It is time to call desktopResized";
    desktopResized();
}

void Workspace::init()
{
#ifdef KWIN_BUILD_SCREENEDGES
    ScreenEdges *screenEdges = ScreenEdges::self();
    screenEdges->setConfig(KGlobal::config());
    screenEdges->init();
    connect(options, SIGNAL(configChanged()), screenEdges, SLOT(reconfigure()));
    connect(VirtualDesktopManager::self(), SIGNAL(layoutChanged(int,int)), screenEdges, SLOT(updateLayout()));
    connect(this, SIGNAL(clientActivated(KWin::Client*)), screenEdges, SIGNAL(checkBlocking()));
#endif

    FocusChain *focusChain = FocusChain::create(this);
    connect(this, SIGNAL(clientRemoved(KWin::Client*)), focusChain, SLOT(remove(KWin::Client*)));
    connect(this, SIGNAL(clientActivated(KWin::Client*)), focusChain, SLOT(setActiveClient(KWin::Client*)));
    connect(VirtualDesktopManager::self(), SIGNAL(countChanged(uint,uint)), focusChain, SLOT(resize(uint,uint)));
    connect(VirtualDesktopManager::self(), SIGNAL(currentChanged(uint,uint)), focusChain, SLOT(setCurrentDesktop(uint,uint)));
    connect(options, SIGNAL(separateScreenFocusChanged(bool)), focusChain, SLOT(setSeparateScreenFocus(bool)));
    focusChain->setSeparateScreenFocus(options->isSeparateScreenFocus());

    supportWindow = new QWidget(NULL, Qt::X11BypassWindowManagerHint);
    XLowerWindow(display(), supportWindow->winId());   // See usage in layers.cpp

    XSetWindowAttributes attr;
    attr.override_redirect = 1;
    null_focus_window = XCreateWindow(display(), rootWindow(), -1, -1, 1, 1, 0, CopyFromParent,
                                      InputOnly, CopyFromParent, CWOverrideRedirect, &attr);
    XMapWindow(display(), null_focus_window);

    unsigned long protocols[5] = {
        NET::Supported |
        NET::SupportingWMCheck |
        NET::ClientList |
        NET::ClientListStacking |
        NET::DesktopGeometry |
        NET::NumberOfDesktops |
        NET::CurrentDesktop |
        NET::ActiveWindow |
        NET::WorkArea |
        NET::CloseWindow |
        NET::DesktopNames |
        NET::WMName |
        NET::WMVisibleName |
        NET::WMDesktop |
        NET::WMWindowType |
        NET::WMState |
        NET::WMStrut |
        NET::WMIconGeometry |
        NET::WMIcon |
        NET::WMPid |
        NET::WMMoveResize |
        NET::WMFrameExtents |
        NET::WMPing
        ,
        NET::NormalMask |
        NET::DesktopMask |
        NET::DockMask |
        NET::ToolbarMask |
        NET::MenuMask |
        NET::DialogMask |
        NET::OverrideMask |
        NET::UtilityMask |
        NET::SplashMask |
        // No compositing window types here unless we support them also as managed window types
        0
        ,
        NET::Modal |
        //NET::Sticky | // Large desktops not supported (and probably never will be)
        NET::MaxVert |
        NET::MaxHoriz |
        NET::Shaded |
        NET::SkipTaskbar |
        NET::KeepAbove |
        //NET::StaysOnTop | // The same like KeepAbove
        NET::SkipPager |
        NET::Hidden |
        NET::FullScreen |
        NET::KeepBelow |
        NET::DemandsAttention |
        0
        ,
        NET::WM2UserTime |
        NET::WM2StartupId |
        NET::WM2AllowedActions |
        NET::WM2RestackWindow |
        NET::WM2MoveResizeWindow |
        NET::WM2ExtendedStrut |
        NET::WM2KDETemporaryRules |
        NET::WM2ShowingDesktop |
        NET::WM2DesktopLayout |
        NET::WM2FullPlacement |
        NET::WM2FullscreenMonitors |
        NET::WM2KDEShadow |
        0
        ,
        NET::ActionMove |
        NET::ActionResize |
        NET::ActionMinimize |
        NET::ActionShade |
        //NET::ActionStick | // Sticky state is not supported
        NET::ActionMaxVert |
        NET::ActionMaxHoriz |
        NET::ActionFullScreen |
        NET::ActionChangeDesktop |
        NET::ActionClose |
        0
        ,
    };

    if (hasDecorationPlugin() && mgr->factory()->supports(AbilityExtendIntoClientArea))
        protocols[ NETRootInfo::PROTOCOLS2 ] |= NET::WM2FrameOverlap;

    rootInfo = new RootInfo(this, display(), supportWindow->winId(), "KWin", protocols, 5, screen_number);

    // create VirtualDesktopManager and perform dependency injection
    VirtualDesktopManager *vds = VirtualDesktopManager::self();
    connect(vds, SIGNAL(desktopsRemoved(uint)), SLOT(moveClientsFromRemovedDesktops()));
    connect(vds, SIGNAL(countChanged(uint,uint)), SLOT(slotDesktopCountChanged(uint,uint)));
    connect(vds, SIGNAL(currentChanged(uint,uint)), SLOT(slotCurrentDesktopChanged(uint,uint)));
    vds->setNavigationWrappingAround(options->isRollOverDesktops());
    connect(options, SIGNAL(rollOverDesktopsChanged(bool)), vds, SLOT(setNavigationWrappingAround(bool)));
    vds->setRootInfo(rootInfo);
    vds->setConfig(KGlobal::config());

    // Now we know how many desktops we'll have, thus we initialize the positioning object
    Placement::create(this);

    // positioning object needs to be created before the virtual desktops are loaded.
    vds->load();
    vds->updateLayout();

    // Extra NETRootInfo instance in Client mode is needed to get the values of the properties
    NETRootInfo client_info(display(), NET::ActiveWindow | NET::CurrentDesktop);
    int initial_desktop;
    if (!kapp->isSessionRestored())
        initial_desktop = client_info.currentDesktop();
    else {
        KConfigGroup group(kapp->sessionConfig(), "Session");
        initial_desktop = group.readEntry("desktop", 1);
    }
    if (!VirtualDesktopManager::self()->setCurrent(initial_desktop))
        VirtualDesktopManager::self()->setCurrent(1);
#ifdef KWIN_BUILD_ACTIVITIES
    updateActivityList(false, true);
#endif

    reconfigureTimer.setSingleShot(true);
    updateToolWindowsTimer.setSingleShot(true);

    connect(&reconfigureTimer, SIGNAL(timeout()), this, SLOT(slotReconfigure()));
    connect(&updateToolWindowsTimer, SIGNAL(timeout()), this, SLOT(slotUpdateToolWindows()));

    connect(KGlobalSettings::self(), SIGNAL(appearanceChanged()), this, SLOT(reconfigure()));
    connect(KGlobalSettings::self(), SIGNAL(settingsChanged(int)), this, SLOT(slotSettingsChanged(int)));
    connect(KGlobalSettings::self(), SIGNAL(blockShortcuts(int)), this, SLOT(slotBlockShortcuts(int)));

    active_client = NULL;
    rootInfo->setActiveWindow(None);
    focusToNull();
    if (!kapp->isSessionRestored())
        ++block_focus; // Because it will be set below

    {
        // Begin updates blocker block
        StackingUpdatesBlocker blocker(this);

        bool fixoffset = KCmdLineArgs::parsedArgs()->getOption("crashes").toInt() > 0;

        Xcb::Tree tree(rootWindow());
        xcb_window_t *wins = xcb_query_tree_children(tree.data());

        QVector<Xcb::WindowAttributes> windowAttributes(tree->children_len);
        QVector<Xcb::WindowGeometry> windowGeometries(tree->children_len);

        // Request the attributes and geometries of all toplevel windows
        for (int i = 0; i < tree->children_len; i++) {
            windowAttributes[i] = Xcb::WindowAttributes(wins[i]);
            windowGeometries[i] = Xcb::WindowGeometry(wins[i]);
        }

        // Get the replies
        for (int i = 0; i < tree->children_len; i++) {
            Xcb::WindowAttributes attr(windowAttributes.at(i));

            if (attr.isNull()) {
                continue;
            }

            if (attr->override_redirect) {
                if (attr->map_state == XCB_MAP_STATE_VIEWABLE &&
                    attr->_class != XCB_WINDOW_CLASS_INPUT_ONLY)
                    // ### This will request the attributes again
                    createUnmanaged(wins[i]);
            } else if (attr->map_state != XCB_MAP_STATE_UNMAPPED) {
                if (fixoffset) {
                    fixPositionAfterCrash(wins[i], windowGeometries[i].data());
                }

                // ### This will request the attributes again
                createClient(wins[i], true);
            }
        }

        // Propagate clients, will really happen at the end of the updates blocker block
        updateStackingOrder(true);

        saveOldScreenSizes();
        updateClientArea();

        // NETWM spec says we have to set it to (0,0) if we don't support it
        NETPoint* viewports = new NETPoint[VirtualDesktopManager::self()->count()];
        rootInfo->setDesktopViewport(VirtualDesktopManager::self()->count(), *viewports);
        delete[] viewports;
        QRect geom;
        for (int i = 0; i < QApplication::desktop()->screenCount(); i++) {
            geom |= QApplication::desktop()->screenGeometry(i);
        }
        NETSize desktop_geometry;
        desktop_geometry.width = geom.width();
        desktop_geometry.height = geom.height();
        rootInfo->setDesktopGeometry(-1, desktop_geometry);
        setShowingDesktop(false);

    } // End updates blocker block

    Client* new_active_client = NULL;
    if (!kapp->isSessionRestored()) {
        --block_focus;
        new_active_client = findClient(WindowMatchPredicate(client_info.activeWindow()));
    }
    if (new_active_client == NULL
            && activeClient() == NULL && should_get_focus.count() == 0) {
        // No client activated in manage()
        if (new_active_client == NULL)
            new_active_client = topClientOnDesktop(VirtualDesktopManager::self()->current(), -1);
        if (new_active_client == NULL && !desktops.isEmpty())
            new_active_client = findDesktop(true, VirtualDesktopManager::self()->current());
    }
    if (new_active_client != NULL)
        activateClient(new_active_client);


#ifdef KWIN_BUILD_SCRIPTING
    m_scripting = new Scripting(this);
#endif

    // SELI TODO: This won't work with unreasonable focus policies,
    // and maybe in rare cases also if the selected client doesn't
    // want focus
    workspaceInit = false;

    // broadcast that Workspace is ready, but first process all events.
    QMetaObject::invokeMethod(this, "workspaceInitialized", Qt::QueuedConnection);

    // TODO: ungrabXServer()
}

Workspace::~Workspace()
{
    delete m_compositor;
    m_compositor = NULL;
    blockStackingUpdates(true);

    // TODO: grabXServer();

    // Use stacking_order, so that kwin --replace keeps stacking order
    for (ToplevelList::iterator it = stacking_order.begin(), end = stacking_order.end(); it != end; ++it) {
        Client *c = qobject_cast<Client*>(*it);
        if (!c) {
            continue;
        }
        // Only release the window
        c->releaseWindow(true);
        // No removeClient() is called, it does more than just removing.
        // However, remove from some lists to e.g. prevent performTransiencyCheck()
        // from crashing.
        clients.removeAll(c);
        desktops.removeAll(c);
    }
    for (UnmanagedList::iterator it = unmanaged.begin(), end = unmanaged.end(); it != end; ++it)
        (*it)->release(true);
    delete m_outline;
    XDeleteProperty(display(), rootWindow(), atoms->kwin_running);

    writeWindowRules();
    KGlobal::config()->sync();

    delete rootInfo;
    delete supportWindow;
    delete mgr;
    delete startup;
    delete Placement::self();
    delete client_keys_dialog;
    while (!rules.isEmpty()) {
        delete rules.front();
        rules.pop_front();
    }
    foreach (SessionInfo * s, session)
    delete s;
    XDestroyWindow(display(), null_focus_window);

    // TODO: ungrabXServer();

    Xcb::Extensions::destroy();
    _self = 0;
}

Client* Workspace::createClient(Window w, bool is_mapped)
{
    StackingUpdatesBlocker blocker(this);
    Client* c = new Client(this);
    connect(c, SIGNAL(needsRepaint()), m_compositor, SLOT(scheduleRepaint()));
    connect(c, SIGNAL(activeChanged()), m_compositor, SLOT(checkUnredirect()));
    connect(c, SIGNAL(fullScreenChanged()), m_compositor, SLOT(checkUnredirect()));
    connect(c, SIGNAL(geometryChanged()), m_compositor, SLOT(checkUnredirect()));
    connect(c, SIGNAL(geometryShapeChanged(KWin::Toplevel*,QRect)), m_compositor, SLOT(checkUnredirect()));
    connect(c, SIGNAL(blockingCompositingChanged(KWin::Client*)), m_compositor, SLOT(updateCompositeBlocking(KWin::Client*)));
#ifdef KWIN_BUILD_SCREENEDGES
    connect(c, SIGNAL(clientFullScreenSet(KWin::Client*,bool,bool)), ScreenEdges::self(), SIGNAL(checkBlocking()));
#endif
    if (!c->manage(w, is_mapped)) {
        Client::deleteClient(c, Allowed);
        return NULL;
    }
    addClient(c, Allowed);
    return c;
}

Unmanaged* Workspace::createUnmanaged(Window w)
{
    if (m_compositor && m_compositor->checkForOverlayWindow(w))
        return NULL;
    Unmanaged* c = new Unmanaged(this);
    if (!c->track(w)) {
        Unmanaged::deleteUnmanaged(c, Allowed);
        return NULL;
    }
    connect(c, SIGNAL(needsRepaint()), m_compositor, SLOT(scheduleRepaint()));
    addUnmanaged(c, Allowed);
    emit unmanagedAdded(c);
    return c;
}

void Workspace::addClient(Client* c, allowed_t)
{
    Group* grp = findGroup(c->window());

    KWindowInfo info = KWindowSystem::windowInfo(c->window(), -1U, NET::WM2WindowClass);

    emit clientAdded(c);

    if (grp != NULL)
        grp->gotLeader(c);

    if (c->isDesktop()) {
        desktops.append(c);
        if (active_client == NULL && should_get_focus.isEmpty() && c->isOnCurrentDesktop())
            requestFocus(c);   // TODO: Make sure desktop is active after startup if there's no other window active
    } else {
        FocusChain::self()->update(c, FocusChain::Update);
        clients.append(c);
    }
    if (!unconstrained_stacking_order.contains(c))
        unconstrained_stacking_order.append(c);   // Raise if it hasn't got any stacking position yet
    if (!stacking_order.contains(c))    // It'll be updated later, and updateToolWindows() requires
        stacking_order.append(c);      // c to be in stacking_order
    x_stacking_dirty = true;
    updateClientArea(); // This cannot be in manage(), because the client got added only now
    updateClientLayer(c);
    if (c->isDesktop()) {
        raiseClient(c);
        // If there's no active client, make this desktop the active one
        if (activeClient() == NULL && should_get_focus.count() == 0)
            activateClient(findDesktop(true, VirtualDesktopManager::self()->current()));
    }
    c->checkActiveModal();
    checkTransients(c->window());   // SELI TODO: Does this really belong here?
    updateStackingOrder(true);   // Propagate new client
    if (c->isUtility() || c->isMenu() || c->isToolbar())
        updateToolWindows(true);
    checkNonExistentClients();
#ifdef KWIN_BUILD_TABBOX
    if (tabBox()->isDisplayed())
        tab_box->reset(true);
#endif
#ifdef KWIN_BUILD_KAPPMENU
        if (ApplicationMenu::self()->hasMenu(c->window()))
            c->setAppMenuAvailable();
#endif
}

void Workspace::addUnmanaged(Unmanaged* c, allowed_t)
{
    unmanaged.append(c);
    x_stacking_dirty = true;
}

/**
 * Destroys the client \a c
 */
void Workspace::removeClient(Client* c, allowed_t)
{
    emit clientRemoved(c);

    if (c == active_popup_client)
        closeActivePopup();
    if (m_userActionsMenu->isMenuClient(c)) {
        m_userActionsMenu->close();
    }

    c->untab(QRect(), true);

    if (client_keys_client == c)
        setupWindowShortcutDone(false);
    if (!c->shortcut().isEmpty()) {
        c->setShortcut(QString());   // Remove from client_keys
        clientShortcutUpdated(c);   // Needed, since this is otherwise delayed by setShortcut() and wouldn't run
    }

    if (c->isDialog())
        Notify::raise(Notify::TransDelete);
    if (c->isNormalWindow())
        Notify::raise(Notify::Delete);

#ifdef KWIN_BUILD_TABBOX
    if (tabBox()->isDisplayed() && tabBox()->currentClient() == c)
        tab_box->nextPrev(true);
#endif

    Q_ASSERT(clients.contains(c) || desktops.contains(c));
    // TODO: if marked client is removed, notify the marked list
    clients.removeAll(c);
    desktops.removeAll(c);
    x_stacking_dirty = true;
    attention_chain.removeAll(c);
    showing_desktop_clients.removeAll(c);
    Group* group = findGroup(c->window());
    if (group != NULL)
        group->lostLeader();

    if (c == most_recently_raised)
        most_recently_raised = 0;
    should_get_focus.removeAll(c);
    Q_ASSERT(c != active_client);
    if (c == last_active_client)
        last_active_client = 0;
    if (c == pending_take_activity)
        pending_take_activity = NULL;
    if (c == delayfocus_client)
        cancelDelayFocus();

    updateStackingOrder(true);

    if (m_compositor) {
        m_compositor->updateCompositeBlocking();
    }

#ifdef KWIN_BUILD_TABBOX
    if (tabBox()->isDisplayed())
        tab_box->reset(true);
#endif

    updateClientArea();
}

void Workspace::removeUnmanaged(Unmanaged* c, allowed_t)
{
    assert(unmanaged.contains(c));
    unmanaged.removeAll(c);
    x_stacking_dirty = true;
}

void Workspace::addDeleted(Deleted* c, Toplevel *orig, allowed_t)
{
    assert(!deleted.contains(c));
    deleted.append(c);
    const int unconstraintedIndex = unconstrained_stacking_order.indexOf(orig);
    if (unconstraintedIndex != -1) {
        unconstrained_stacking_order.replace(unconstraintedIndex, c);
    } else {
        unconstrained_stacking_order.append(c);
    }
    const int index = stacking_order.indexOf(orig);
    if (index != -1) {
        stacking_order.replace(index, c);
    } else {
        stacking_order.append(c);
    }
    x_stacking_dirty = true;
    connect(c, SIGNAL(needsRepaint()), m_compositor, SLOT(scheduleRepaint()));
}

void Workspace::removeDeleted(Deleted* c, allowed_t)
{
    assert(deleted.contains(c));
    emit deletedRemoved(c);
    deleted.removeAll(c);
    unconstrained_stacking_order.removeAll(c);
    stacking_order.removeAll(c);
    x_stacking_dirty = true;
}

void Workspace::updateToolWindows(bool also_hide)
{
    // TODO: What if Client's transiency/group changes? should this be called too? (I'm paranoid, am I not?)
    if (!options->isHideUtilityWindowsForInactive()) {
        for (ClientList::ConstIterator it = clients.constBegin(); it != clients.constEnd(); ++it)
            if (!(*it)->tabGroup() || (*it)->tabGroup()->current() == *it)
                (*it)->hideClient(false);
        return;
    }
    const Group* group = NULL;
    const Client* client = active_client;
    // Go up in transiency hiearchy, if the top is found, only tool transients for the top mainwindow
    // will be shown; if a group transient is group, all tools in the group will be shown
    while (client != NULL) {
        if (!client->isTransient())
            break;
        if (client->groupTransient()) {
            group = client->group();
            break;
        }
        client = client->transientFor();
    }
    // Use stacking order only to reduce flicker, it doesn't matter if block_stacking_updates == 0,
    // I.e. if it's not up to date

    // SELI TODO: But maybe it should - what if a new client has been added that's not in stacking order yet?
    ClientList to_show, to_hide;
    for (ToplevelList::ConstIterator it = stacking_order.constBegin();
            it != stacking_order.constEnd();
            ++it) {
        Client *c = qobject_cast<Client*>(*it);
        if (!c) {
            continue;
        }
        if (c->isUtility() || c->isMenu() || c->isToolbar()) {
            bool show = true;
            if (!c->isTransient()) {
                if (c->group()->members().count() == 1)   // Has its own group, keep always visible
                    show = true;
                else if (client != NULL && c->group() == client->group())
                    show = true;
                else
                    show = false;
            } else {
                if (group != NULL && c->group() == group)
                    show = true;
                else if (client != NULL && client->hasTransient(c, true))
                    show = true;
                else
                    show = false;
            }
            if (!show && also_hide) {
                const ClientList mainclients = c->mainClients();
                // Don't hide utility windows which are standalone(?) or
                // have e.g. kicker as mainwindow
                if (mainclients.isEmpty())
                    show = true;
                for (ClientList::ConstIterator it2 = mainclients.constBegin();
                        it2 != mainclients.constEnd();
                        ++it2) {
                    if (c->isSpecialWindow())
                        show = true;
                }
                if (!show)
                    to_hide.append(c);
            }
            if (show)
                to_show.append(c);
        }
    } // First show new ones, then hide
    for (int i = to_show.size() - 1;
            i >= 0;
            --i)  // From topmost
        // TODO: Since this is in stacking order, the order of taskbar entries changes :(
        to_show.at(i)->hideClient(false);
    if (also_hide) {
        for (ClientList::ConstIterator it = to_hide.constBegin();
                it != to_hide.constEnd();
                ++it)  // From bottommost
            (*it)->hideClient(true);
        updateToolWindowsTimer.stop();
    } else // setActiveClient() is after called with NULL client, quickly followed
        // by setting a new client, which would result in flickering
        resetUpdateToolWindowsTimer();
}


void Workspace::resetUpdateToolWindowsTimer()
{
    updateToolWindowsTimer.start(200);
}

void Workspace::slotUpdateToolWindows()
{
    updateToolWindows(true);
}

/**
 * Updates the current colormap according to the currently active client
 */
void Workspace::updateColormap()
{
    Colormap cmap = default_colormap;
    if (activeClient() && activeClient()->colormap() != None)
        cmap = activeClient()->colormap();
    if (cmap != installed_colormap) {
        XInstallColormap(display(), cmap);
        installed_colormap = cmap;
    }
}

void Workspace::slotReloadConfig()
{
    reconfigure();
}

void Workspace::reconfigure()
{
    reconfigureTimer.start(200);
}

/**
 * This D-Bus call is used by the compositing kcm. Since the reconfigure()
 * D-Bus call delays the actual reconfiguring, it is not possible to immediately
 * call compositingActive(). Therefore the kcm will instead call this to ensure
 * the reconfiguring has already happened.
 */
bool Workspace::waitForCompositingSetup()
{
    if (reconfigureTimer.isActive()) {
        reconfigureTimer.stop();
        slotReconfigure();
    }
    if (m_compositor) {
        return m_compositor->isActive();
    }
    return false;
}

void Workspace::slotSettingsChanged(int category)
{
    kDebug(1212) << "Workspace::slotSettingsChanged()";
    if (category == KGlobalSettings::SETTINGS_SHORTCUTS)
        m_userActionsMenu->discard();
}

/**
 * Reread settings
 */
KWIN_PROCEDURE(CheckBorderSizesProcedure, Client, cl->checkBorderSizes(true));

void Workspace::slotReconfigure()
{
    kDebug(1212) << "Workspace::slotReconfigure()";
    reconfigureTimer.stop();

    bool borderlessMaximizedWindows = options->borderlessMaximizedWindows();

    KGlobal::config()->reparseConfiguration();
    unsigned long changed = options->updateSettings();

    emit configChanged();
    m_userActionsMenu->discard();
    updateToolWindows(true);

    if (hasDecorationPlugin() && mgr->reset(changed)) {
        // Decorations need to be recreated

        // This actually seems to make things worse now
        //QWidget curtain;
        //curtain.setBackgroundMode( NoBackground );
        //curtain.setGeometry( Kephal::ScreenUtils::desktopGeometry() );
        //curtain.show();

        for (ClientList::ConstIterator it = clients.constBegin(); it != clients.constEnd(); ++it)
            (*it)->updateDecoration(true, true);
        // If the new decoration doesn't supports tabs then ungroup clients
        if (!decorationSupportsTabbing()) {
            foreach (Client * c, clients)
                c->untab();
        }
        mgr->destroyPreviousPlugin();
    } else {
        forEachClient(CheckBorderSizesProcedure());
        foreach (Client * c, clients)
            c->triggerDecorationRepaint();
    }

    loadWindowRules();
    for (ClientList::Iterator it = clients.begin();
            it != clients.end();
            ++it) {
        (*it)->setupWindowRules(true);
        (*it)->applyWindowRules();
        discardUsedWindowRules(*it, false);
    }

    if (borderlessMaximizedWindows != options->borderlessMaximizedWindows() &&
            !options->borderlessMaximizedWindows()) {
        // in case borderless maximized windows option changed and new option
        // is to have borders, we need to unset the borders for all maximized windows
        for (ClientList::Iterator it = clients.begin();
                it != clients.end();
                ++it) {
            if ((*it)->maximizeMode() == MaximizeFull)
                (*it)->checkNoBorder();
        }
    }

    if (hasDecorationPlugin()) {
        rootInfo->setSupported(NET::WM2FrameOverlap, mgr->factory()->supports(AbilityExtendIntoClientArea));
    } else {
        rootInfo->setSupported(NET::WM2FrameOverlap, false);
    }
}

/**
 * During virt. desktop switching, desktop areas covered by windows that are
 * going to be hidden are first obscured by new windows with no background
 * ( i.e. transparent ) placed right below the windows. These invisible windows
 * are removed after the switch is complete.
 * Reduces desktop ( wallpaper ) repaints during desktop switching
 */
class ObscuringWindows
{
public:
    ~ObscuringWindows();
    void create(Client* c);
private:
    QList<Window> obscuring_windows;
    static QList<Window>* cached;
    static unsigned int max_cache_size;
};

QList<Window>* ObscuringWindows::cached = 0;
unsigned int ObscuringWindows::max_cache_size = 0;

void ObscuringWindows::create(Client* c)
{
    if (cached == 0)
        cached = new QList<Window>;
    Window obs_win;
    XWindowChanges chngs;
    int mask = CWSibling | CWStackMode;
    if (cached->count() > 0) {
        cached->removeAll(obs_win = cached->first());
        chngs.x = c->x();
        chngs.y = c->y();
        chngs.width = c->width();
        chngs.height = c->height();
        mask |= CWX | CWY | CWWidth | CWHeight;
    } else {
        XSetWindowAttributes a;
        a.background_pixmap = None;
        a.override_redirect = True;
        obs_win = XCreateWindow(display(), rootWindow(), c->x(), c->y(),
                                c->width(), c->height(), 0, CopyFromParent, InputOutput,
                                CopyFromParent, CWBackPixmap | CWOverrideRedirect, &a);
    }
    chngs.sibling = c->frameId();
    chngs.stack_mode = Below;
    XConfigureWindow(display(), obs_win, mask, &chngs);
    XMapWindow(display(), obs_win);
    obscuring_windows.append(obs_win);
}

ObscuringWindows::~ObscuringWindows()
{
    max_cache_size = qMax(int(max_cache_size), obscuring_windows.count() + 4) - 1;
    for (QList<Window>::ConstIterator it = obscuring_windows.constBegin();
            it != obscuring_windows.constEnd();
            ++it) {
        XUnmapWindow(display(), *it);
        if (cached->count() < int(max_cache_size))
            cached->prepend(*it);
        else
            XDestroyWindow(display(), *it);
    }
}

void Workspace::slotCurrentDesktopChanged(uint oldDesktop, uint newDesktop)
{
    closeActivePopup();
    ++block_focus;
    StackingUpdatesBlocker blocker(this);
    updateClientVisibilityOnDesktopChange(oldDesktop, newDesktop);
    // Restore the focus on this desktop
    --block_focus;

    activateClientOnNewDesktop(newDesktop);
    emit currentDesktopChanged(oldDesktop, movingClient);
}

void Workspace::updateClientVisibilityOnDesktopChange(uint oldDesktop, uint newDesktop)
{
    ++block_showing_desktop;
    ObscuringWindows obs_wins;
    for (ToplevelList::ConstIterator it = stacking_order.constBegin();
            it != stacking_order.constEnd();
            ++it) {
        Client *c = qobject_cast<Client*>(*it);
        if (!c) {
            continue;
        }
        if (!c->isOnDesktop(newDesktop) && c != movingClient && c->isOnCurrentActivity()) {
            if (c->isShown(true) && c->isOnDesktop(oldDesktop) && !compositing())
                obs_wins.create(c);
            (c)->updateVisibility();
        }
    }
    // Now propagate the change, after hiding, before showing
    rootInfo->setCurrentDesktop(VirtualDesktopManager::self()->current());

    if (movingClient && !movingClient->isOnDesktop(newDesktop)) {
        movingClient->setDesktop(newDesktop);
    }

    for (int i = stacking_order.size() - 1; i >= 0 ; --i) {
        Client *c = qobject_cast<Client*>(stacking_order.at(i));
        if (!c) {
            continue;
        }
        if (c->isOnDesktop(newDesktop) && c->isOnCurrentActivity())
            c->updateVisibility();
    }
    --block_showing_desktop;
    if (showingDesktop())   // Do this only after desktop change to avoid flicker
        resetShowingDesktop(false);
}

void Workspace::activateClientOnNewDesktop(uint desktop)
{
    Client* c = NULL;
    if (options->focusPolicyIsReasonable()) {
        c = findClientToActivateOnDesktop(desktop);
    }
    // If "unreasonable focus policy" and active_client is on_all_desktops and
    // under mouse (Hence == old_active_client), conserve focus.
    // (Thanks to Volker Schatz <V.Schatz at thphys.uni-heidelberg.de>)
    else if (active_client && active_client->isShown(true) && active_client->isOnCurrentDesktop())
        c = active_client;

    if (c == NULL && !desktops.isEmpty())
        c = findDesktop(true, desktop);

    if (c != active_client)
        setActiveClient(NULL, Allowed);

    if (c)
        requestFocus(c);
    else if (!desktops.isEmpty())
        requestFocus(findDesktop(true, desktop));
    else
        focusToNull();
}

Client *Workspace::findClientToActivateOnDesktop(uint desktop)
{
    if (movingClient != NULL && active_client == movingClient &&
        FocusChain::self()->contains(active_client, desktop) &&
        active_client->isShown(true) && active_client->isOnCurrentDesktop()) {
        // A requestFocus call will fail, as the client is already active
        return active_client;
    }
    // from actiavtion.cpp
    if (options->isNextFocusPrefersMouse()) {
        ToplevelList::const_iterator it = stackingOrder().constEnd();
        while (it != stackingOrder().constBegin()) {
            Client *client = qobject_cast<Client*>(*(--it));
            if (!client) {
                continue;
            }

            if (!(client->isShown(false) && client->isOnDesktop(desktop) &&
                client->isOnCurrentActivity() && client->isOnScreen(activeScreen())))
                continue;

            if (client->geometry().contains(Cursor::pos())) {
                if (!client->isDesktop())
                    return client;
            break; // unconditional break  - we do not pass the focus to some client below an unusable one
            }
        }
    }
    return FocusChain::self()->getForActivation(desktop);
}

#ifdef KWIN_BUILD_ACTIVITIES

//BEGIN threaded activity list fetching
typedef QPair<QStringList*, QStringList> AssignedList;
typedef QPair<QString, QStringList> CurrentAndList;

static AssignedList
fetchActivityList(KActivities::Controller *controller, QStringList *target, bool running) // could be member function, but actually it's much simpler this way
{
    return AssignedList(target, running ? controller->listActivities(KActivities::Info::Running) :
                                          controller->listActivities());
}

static CurrentAndList
fetchActivityListAndCurrent(KActivities::Controller *controller)
{
    QStringList l   = controller->listActivities();
    QString c       = controller->currentActivity();
    return CurrentAndList(c, l);
}

void Workspace::updateActivityList(bool running, bool updateCurrent, QObject *target, QString slot)
{
    if (updateCurrent) {
        QFutureWatcher<CurrentAndList>* watcher = new QFutureWatcher<CurrentAndList>;
        connect( watcher, SIGNAL(finished()), SLOT(handleActivityReply()) );
        if (!slot.isEmpty()) {
            watcher->setProperty("activityControllerCallback", slot); // "activity reply trigger"
            watcher->setProperty("activityControllerCallbackTarget", qVariantFromValue((void*)target));
        }
        watcher->setFuture(QtConcurrent::run(fetchActivityListAndCurrent, &activityController_ ));
    } else {
        QFutureWatcher<AssignedList>* watcher = new QFutureWatcher<AssignedList>;
        connect(watcher, SIGNAL(finished()), SLOT(handleActivityReply()));
        if (!slot.isEmpty()) {
            watcher->setProperty("activityControllerCallback", slot); // "activity reply trigger"
            watcher->setProperty("activityControllerCallbackTarget", qVariantFromValue((void*)target));
        }
        QStringList *target = running ? &openActivities_ : &allActivities_;
        watcher->setFuture(QtConcurrent::run(fetchActivityList, &activityController_, target, running));
    }
}

void Workspace::handleActivityReply()
{
    QObject *watcherObject = 0;
    if (QFutureWatcher<AssignedList>* watcher = dynamic_cast< QFutureWatcher<AssignedList>* >(sender())) {
        *(watcher->result().first) = watcher->result().second; // cool trick, ehh? :-)
        watcherObject = watcher;
    }

    if (!watcherObject) {
        if (QFutureWatcher<CurrentAndList>* watcher = dynamic_cast< QFutureWatcher<CurrentAndList>* >(sender())) {
            allActivities_ = watcher->result().second;
            updateCurrentActivity(watcher->result().first);
            emit currentActivityChanged(currentActivity());
            watcherObject = watcher;
        }
    }

    if (watcherObject) {
        QString slot = watcherObject->property("activityControllerCallback").toString();
        QObject *target = static_cast<QObject*>(watcherObject->property("activityControllerCallbackTarget").value<void*>());
        watcherObject->deleteLater(); // has done it's job
        if (!slot.isEmpty())
            QMetaObject::invokeMethod(target, slot.toAscii().data(), Qt::DirectConnection);
    }
}
//END threaded activity list fetching

#else // make gcc happy - stupd moc cannot handle preproc defs so we MUST define
void Workspace::handleActivityReply() {}
#endif // KWIN_BUILD_ACTIVITIES

/**
 * Updates the current activity when it changes
 * do *not* call this directly; it does not set the activity.
 *
 * Shows/Hides windows according to the stacking order
 */

void Workspace::updateCurrentActivity(const QString &new_activity)
{

    //closeActivePopup();
    ++block_focus;
    // TODO: Q_ASSERT( block_stacking_updates == 0 ); // Make sure stacking_order is up to date
    StackingUpdatesBlocker blocker(this);

    if (new_activity != activity_) {
        ++block_showing_desktop; //FIXME should I be using that?
        // Optimized Desktop switching: unmapping done from back to front
        // mapping done from front to back => less exposure events
        //Notify::raise((Notify::Event) (Notify::DesktopChange+new_desktop));

        ObscuringWindows obs_wins;

        QString old_activity = activity_;
        activity_ = new_activity;

        for (ToplevelList::ConstIterator it = stacking_order.constBegin();
                it != stacking_order.constEnd();
                ++it) {
            Client *c = qobject_cast<Client*>(*it);
            if (!c) {
                continue;
            }
            if (!c->isOnActivity(new_activity) && c != movingClient && c->isOnCurrentDesktop()) {
                if (c->isShown(true) && c->isOnActivity(old_activity) && !compositing())
                    obs_wins.create(c);
                c->updateVisibility();
            }
        }

        // Now propagate the change, after hiding, before showing
        //rootInfo->setCurrentDesktop( currentDesktop() );

        /* TODO someday enable dragging windows to other activities
        if ( movingClient && !movingClient->isOnDesktop( new_desktop ))
            {
            movingClient->setDesktop( new_desktop );
            */

        for (int i = stacking_order.size() - 1; i >= 0 ; --i) {
            Client *c = qobject_cast<Client*>(stacking_order.at(i));
            if (!c) {
                continue;
            }
            if (c->isOnActivity(new_activity))
                c->updateVisibility();
        }

        --block_showing_desktop;
        //FIXME not sure if I should do this either
        if (showingDesktop())   // Do this only after desktop change to avoid flicker
            resetShowingDesktop(false);
    }

    // Restore the focus on this desktop
    --block_focus;
    Client* c = 0;

    //FIXME below here is a lot of focuschain stuff, probably all wrong now
    if (options->focusPolicyIsReasonable()) {
        // Search in focus chain
        c = FocusChain::self()->getForActivation(VirtualDesktopManager::self()->current());
    }
    // If "unreasonable focus policy" and active_client is on_all_desktops and
    // under mouse (Hence == old_active_client), conserve focus.
    // (Thanks to Volker Schatz <V.Schatz at thphys.uni-heidelberg.de>)
    else if (active_client && active_client->isShown(true) && active_client->isOnCurrentDesktop() && active_client->isOnCurrentActivity())
        c = active_client;

    if (c == NULL && !desktops.isEmpty())
        c = findDesktop(true, VirtualDesktopManager::self()->current());

    if (c != active_client)
        setActiveClient(NULL, Allowed);

    if (c)
        requestFocus(c);
    else if (!desktops.isEmpty())
        requestFocus(findDesktop(true, VirtualDesktopManager::self()->current()));
    else
        focusToNull();

    // Not for the very first time, only if something changed and there are more than 1 desktops

    //if ( effects != NULL && old_desktop != 0 && old_desktop != new_desktop )
    //    static_cast<EffectsHandlerImpl*>( effects )->desktopChanged( old_desktop );
    if (compositing() && m_compositor)
        m_compositor->addRepaintFull();

}

/**
 * updates clients when an activity is destroyed.
 * this ensures that a client does not get 'lost' if the only activity it's on is removed.
 */
void Workspace::slotActivityRemoved(const QString &activity)
{
    allActivities_.removeOne(activity);
    foreach (Toplevel * toplevel, stacking_order) {
        if (Client *client = qobject_cast<Client*>(toplevel)) {
            client->setOnActivity(activity, false);
        }
    }
    //toss out any session data for it
    KConfigGroup cg(KGlobal::config(), QString("SubSession: ") + activity);
    cg.deleteGroup();
}

void Workspace::slotActivityAdded(const QString &activity)
{
    allActivities_ << activity;
}

void Workspace::moveClientsFromRemovedDesktops()
{
    for (ClientList::ConstIterator it = clients.constBegin(); it != clients.constEnd(); ++it) {
        if (!(*it)->isOnAllDesktops() && (*it)->desktop() > static_cast<int>(VirtualDesktopManager::self()->count()))
            sendClientToDesktop(*it, VirtualDesktopManager::self()->count(), true);
    }
}

void Workspace::slotDesktopCountChanged(uint previousCount, uint newCount)
{
    Q_UNUSED(previousCount)
    Placement::self()->reinitCascading(0);

    resetClientAreas(newCount);
}

void Workspace::resetClientAreas(uint desktopCount)
{
    // Make it +1, so that it can be accessed as [1..numberofdesktops]
    workarea.clear();
    workarea.resize(desktopCount + 1);
    restrictedmovearea.clear();
    restrictedmovearea.resize(desktopCount + 1);
    screenarea.clear();

    updateClientArea(true);
}

/**
 * Sends client \a c to desktop \a desk.
 *
 * Takes care of transients as well.
 */
void Workspace::sendClientToDesktop(Client* c, int desk, bool dont_activate)
{
    if ((desk < 1 && desk != NET::OnAllDesktops) || desk > static_cast<int>(VirtualDesktopManager::self()->count()))
        return;
    int old_desktop = c->desktop();
    bool was_on_desktop = c->isOnDesktop(desk) || c->isOnAllDesktops();
    c->setDesktop(desk);
    if (c->desktop() != desk)   // No change or desktop forced
        return;
    desk = c->desktop(); // Client did range checking

    emit desktopPresenceChanged(c, old_desktop);

    if (c->isOnDesktop(VirtualDesktopManager::self()->current())) {
        if (c->wantsTabFocus() && options->focusPolicyIsReasonable() &&
                !was_on_desktop && // for stickyness changes
                !dont_activate)
            requestFocus(c);
        else
            restackClientUnderActive(c);
    } else
        raiseClient(c);

    c->checkWorkspacePosition( QRect(), old_desktop );

    ClientList transients_stacking_order = ensureStackingOrder(c->transients());
    for (ClientList::ConstIterator it = transients_stacking_order.constBegin();
            it != transients_stacking_order.constEnd();
            ++it)
        sendClientToDesktop(*it, desk, dont_activate);
    updateClientArea();
}

/**
 * Adds/removes client \a c to/from \a activity.
 *
 * Takes care of transients as well.
 */
void Workspace::toggleClientOnActivity(Client* c, const QString &activity, bool dont_activate)
{
    //int old_desktop = c->desktop();
    bool was_on_activity = c->isOnActivity(activity);
    bool was_on_all = c->isOnAllActivities();
    //note: all activities === no activities
    bool enable = was_on_all || !was_on_activity;
    c->setOnActivity(activity, enable);
    if (c->isOnActivity(activity) == was_on_activity && c->isOnAllActivities() == was_on_all)   // No change
        return;

    if (c->isOnCurrentActivity()) {
        if (c->wantsTabFocus() && options->focusPolicyIsReasonable() &&
                !was_on_activity && // for stickyness changes
                //FIXME not sure if the line above refers to the correct activity
                !dont_activate)
            requestFocus(c);
        else
            restackClientUnderActive(c);
    } else
        raiseClient(c);

    //notifyWindowDesktopChanged( c, old_desktop );

    ClientList transients_stacking_order = ensureStackingOrder(c->transients());
    for (ClientList::ConstIterator it = transients_stacking_order.constBegin();
            it != transients_stacking_order.constEnd();
            ++it)
        toggleClientOnActivity(*it, activity, dont_activate);
    updateClientArea();
}

int Workspace::numScreens() const
{
    return QApplication::desktop()->screenCount();
}

int Workspace::activeScreen() const
{
    if (!options->isActiveMouseScreen()) {
        if (activeClient() != NULL && !activeClient()->isOnScreen(active_screen))
            return activeClient()->screen();
        return active_screen;
    }
    return QApplication::desktop()->screenNumber(cursorPos());
}

/**
 * Check whether a client moved completely out of what's considered the active screen,
 * if yes, set a new active screen.
 */
void Workspace::checkActiveScreen(const Client* c)
{
    if (!c->isActive())
        return;
    if (!c->isOnScreen(active_screen))
        active_screen = c->screen();
}

/**
 * checks whether the X Window with the input focus is on our X11 screen
 * if the window cannot be determined or inspected, resturn depends on whether there's actually
 * more than one screen
 *
 * this is NOT in any way related to XRandR multiscreen
 *
 */
extern bool is_multihead; // main.cpp
bool Workspace::isOnCurrentHead()
{
    if (!is_multihead) {
        return true;
    }

    Xcb::CurrentInput currentInput;
    if (currentInput.window() == XCB_WINDOW_NONE) {
        return !is_multihead;
    }

    Xcb::WindowGeometry geometry(currentInput.window());
    if (geometry.isNull()) { // should not happen
        return !is_multihead;
    }

    return rootWindow() == geometry->root;
}

/**
 * Called e.g. when a user clicks on a window, set active screen to be the screen
 * where the click occurred
 */
void Workspace::setActiveScreenMouse(const QPoint& mousepos)
{
    active_screen = QApplication::desktop()->screenNumber(mousepos);
}

QRect Workspace::screenGeometry(int screen) const
{
    return QApplication::desktop()->screenGeometry(screen);
}

int Workspace::screenNumber(const QPoint& pos) const
{
    return QApplication::desktop()->screenNumber(pos);
}

void Workspace::sendClientToScreen(Client* c, int screen)
{
    screen = c->rules()->checkScreen(screen);
    if (c->isActive()) {
        active_screen = screen;
        // might impact the layer of a fullscreen window
        foreach (Client *cc, clientList()) {
            if (cc->isFullScreen() && cc->screen() == screen) {
                updateClientLayer(cc);
            }
        }
    }
    if (c->screen() == screen)   // Don't use isOnScreen(), that's true even when only partially
        return;
    GeometryUpdatesBlocker blocker(c);
    QRect old_sarea = clientArea(MaximizeArea, c);
    QRect sarea = clientArea(MaximizeArea, screen, c->desktop());
    QRect oldgeom = c->geometry();
    QRect geom = c->geometry();
    // move the window to have the same relative position to the center of the screen
    // (i.e. one near the middle of the right edge will also end up near the middle of the right edge)
    geom.moveCenter(
        QPoint(( geom.center().x() - old_sarea.center().x()) * sarea.width() / old_sarea.width() + sarea.center().x(),
            ( geom.center().y() - old_sarea.center().y()) * sarea.height() / old_sarea.height() + sarea.center().y()));
    c->setGeometry( geom );
    // If the window was inside the old screen area, explicitly make sure its inside also the new screen area.
    // Calling checkWorkspacePosition() should ensure that, but when moving to a small screen the window could
    // be big enough to overlap outside of the new screen area, making struts from other screens come into effect,
    // which could alter the resulting geometry.
    if( old_sarea.contains( oldgeom ))
        c->keepInArea( sarea );
    c->checkWorkspacePosition( oldgeom );
    ClientList transients_stacking_order = ensureStackingOrder(c->transients());
    for (ClientList::ConstIterator it = transients_stacking_order.constBegin();
            it != transients_stacking_order.constEnd();
            ++it)
        sendClientToScreen(*it, screen);
}

void Workspace::killWindowId(Window window_to_kill)
{
    if (window_to_kill == None)
        return;
    Window window = window_to_kill;
    Client* client = NULL;
    for (;;) {
        client = findClient(FrameIdMatchPredicate(window));
        if (client != NULL)
            break; // Found the client
        Window parent, root;
        Window* children;
        unsigned int children_count;
        XQueryTree(display(), window, &root, &parent, &children, &children_count);
        if (children != NULL)
            XFree(children);
        if (window == root)   // We didn't find the client, probably an override-redirect window
            break;
        window = parent; // Go up
    }
    if (client != NULL)
        client->killWindow();
    else
        XKillClient(display(), window_to_kill);
}

void Workspace::sendPingToWindow(Window window, Time timestamp)
{
    rootInfo->sendPing(window, timestamp);
}

void Workspace::sendTakeActivity(Client* c, Time timestamp, long flags)
{
    rootInfo->takeActivity(c->window(), timestamp, flags);
    pending_take_activity = c;
}

/**
 * Delayed focus functions
 */
void Workspace::delayFocus()
{
    requestFocus(delayfocus_client);
    cancelDelayFocus();
}

void Workspace::requestDelayFocus(Client* c)
{
    delayfocus_client = c;
    delete delayFocusTimer;
    delayFocusTimer = new QTimer(this);
    connect(delayFocusTimer, SIGNAL(timeout()), this, SLOT(delayFocus()));
    delayFocusTimer->setSingleShot(true);
    delayFocusTimer->start(options->delayFocusInterval());
}

void Workspace::cancelDelayFocus()
{
    delete delayFocusTimer;
    delayFocusTimer = 0;
}

KDecoration* Workspace::createDecoration(KDecorationBridge* bridge)
{
    if (!hasDecorationPlugin()) {
        return NULL;
    }
    return mgr->createDecoration(bridge);
}

/**
 * Returns a list of all colors (KDecorationDefines::ColorType) the current
 * decoration supports
 */
QList<int> Workspace::decorationSupportedColors() const
{
    QList<int> ret;
    if (!hasDecorationPlugin()) {
        return ret;
    }
    KDecorationFactory* factory = mgr->factory();
    for (Ability ab = ABILITYCOLOR_FIRST;
            ab < ABILITYCOLOR_END;
            ab = static_cast<Ability>(ab + 1))
        if (factory->supports(ab))
            ret << ab;
    return ret;
}

bool Workspace::checkStartupNotification(Window w, KStartupInfoId& id, KStartupInfoData& data)
{
    return startup->checkStartup(w, id, data) == KStartupInfo::Match;
}

/**
 * Puts the focus on a dummy window
 * Just using XSetInputFocus() with None would block keyboard input
 */
void Workspace::focusToNull()
{
    XSetInputFocus(display(), null_focus_window, RevertToPointerRoot, xTime());
}

void Workspace::setShowingDesktop(bool showing)
{
    rootInfo->setShowingDesktop(showing);
    showing_desktop = showing;
    ++block_showing_desktop;
    if (showing_desktop) {
        showing_desktop_clients.clear();
        ++block_focus;
        ToplevelList cls = stackingOrder();
        // Find them first, then minimize, otherwise transients may get minimized with the window
        // they're transient for
        for (ToplevelList::ConstIterator it = cls.constBegin();
                it != cls.constEnd();
                ++it) {
            Client *c = qobject_cast<Client*>(*it);
            if (!c) {
                continue;
            }
            if (c->isOnCurrentActivity() && c->isOnCurrentDesktop() && c->isShown(true) && !c->isSpecialWindow())
                showing_desktop_clients.prepend(c);   // Topmost first to reduce flicker
        }
        for (ClientList::ConstIterator it = showing_desktop_clients.constBegin();
                it != showing_desktop_clients.constEnd();
                ++it)
            (*it)->minimize();
        --block_focus;
        if (Client* desk = findDesktop(true, VirtualDesktopManager::self()->current()))
            requestFocus(desk);
    } else {
        for (ClientList::ConstIterator it = showing_desktop_clients.constBegin();
                it != showing_desktop_clients.constEnd();
                ++it)
            (*it)->unminimize();
        if (showing_desktop_clients.count() > 0)
            requestFocus(showing_desktop_clients.first());
        showing_desktop_clients.clear();
    }
    --block_showing_desktop;
}

/**
 * Following Kicker's behavior:
 * Changing a virtual desktop resets the state and shows the windows again.
 * Unminimizing a window resets the state but keeps the windows hidden (except
 * the one that was unminimized).
 * A new window resets the state and shows the windows again, with the new window
 * being active. Due to popular demand (#67406) by people who apparently
 * don't see a difference between "show desktop" and "minimize all", this is not
 * true if "showDesktopIsMinimizeAll" is set in kwinrc. In such case showing
 * a new window resets the state but doesn't show windows.
 */
void Workspace::resetShowingDesktop(bool keep_hidden)
{
    if (block_showing_desktop > 0)
        return;
    rootInfo->setShowingDesktop(false);
    showing_desktop = false;
    ++block_showing_desktop;
    if (!keep_hidden) {
        for (ClientList::ConstIterator it = showing_desktop_clients.constBegin();
                it != showing_desktop_clients.constEnd();
                ++it)
            (*it)->unminimize();
    }
    showing_desktop_clients.clear();
    --block_showing_desktop;
}

/**
 * Activating/deactivating this feature works like this:
 * When nothing is active, and the shortcut is pressed, global shortcuts are disabled
 *     (using global_shortcuts_disabled)
 * When a window that has disabling forced is activated, global shortcuts are disabled.
 *     (using global_shortcuts_disabled_for_client)
 * When a shortcut is pressed and global shortcuts are disabled (either by a shortcut
 * or for a client), they are enabled again.
 */
void Workspace::slotDisableGlobalShortcuts()
{
    if (global_shortcuts_disabled || global_shortcuts_disabled_for_client)
        disableGlobalShortcuts(false);
    else
        disableGlobalShortcuts(true);
}

static bool pending_dfc = false;

void Workspace::disableGlobalShortcutsForClient(bool disable)
{
    if (global_shortcuts_disabled_for_client == disable)
        return;
    if (!global_shortcuts_disabled) {
        if (disable)
            pending_dfc = true;
        KGlobalSettings::self()->emitChange(KGlobalSettings::BlockShortcuts, disable);
        // KWin will get the kipc message too
    }
}

void Workspace::disableGlobalShortcuts(bool disable)
{
    KGlobalSettings::self()->emitChange(KGlobalSettings::BlockShortcuts, disable);
    // KWin will get the kipc message too
}

void Workspace::slotBlockShortcuts(int data)
{
    if (pending_dfc && data) {
        global_shortcuts_disabled_for_client = true;
        pending_dfc = false;
    } else {
        global_shortcuts_disabled = data;
        global_shortcuts_disabled_for_client = false;
    }
    // Update also Alt+LMB actions etc.
    for (ClientList::ConstIterator it = clients.constBegin();
            it != clients.constEnd();
            ++it)
        (*it)->updateMouseGrab();
}

Outline* Workspace::outline()
{
    return m_outline;
}

bool Workspace::hasTabBox() const
{
#ifdef KWIN_BUILD_TABBOX
    return (tab_box != NULL);
#else
    return false;
#endif
}

#ifdef KWIN_BUILD_TABBOX
TabBox::TabBox* Workspace::tabBox() const
{
    return tab_box;
}
#endif

QString Workspace::supportInformation() const
{
    QString support;

    support.append(ki18nc("Introductory text shown in the support information.",
        "KWin Support Information:\n"
        "The following information should be used when requesting support on e.g. http://forum.kde.org.\n"
        "It provides information about the currently running instance, which options are used,\n"
        "what OpenGL driver and which effects are running.\n"
        "Please post the information provided underneath this introductory text to a paste bin service\n"
        "like http://paste.kde.org instead of pasting into support threads.\n").toString());
    support.append("\n==========================\n\n");
    // all following strings are intended for support. They need to be pasted to e.g forums.kde.org
    // it is expected that the support will happen in English language or that the people providing
    // help understand English. Because of that all texts are not translated
    support.append("Version\n");
    support.append("=======\n");
    support.append("KWin version: ");
    support.append(KWIN_VERSION_STRING);
    support.append('\n');
    support.append("KDE SC version (runtime): ");
    support.append(KDE::versionString());
    support.append('\n');
    support.append("KDE SC version (compile): ");
    support.append(KDE_VERSION_STRING);
    support.append('\n');
    support.append("Qt Version: ");
    support.append(qVersion());
    support.append("\n\n");
    support.append("Options\n");
    support.append("=======\n");
    const QMetaObject *metaOptions = options->metaObject();
    for (int i=0; i<metaOptions->propertyCount(); ++i) {
        const QMetaProperty property = metaOptions->property(i);
        if (QLatin1String(property.name()) == "objectName") {
            continue;
        }
        support.append(QLatin1String(property.name()) % ": " % options->property(property.name()).toString() % '\n');
    }
#ifdef KWIN_BUILD_SCREENEDGES
    support.append("\nScreen Edges\n");
    support.append(  "============\n");
    const QMetaObject *metaScreenEdges = ScreenEdges::self()->metaObject();
    for (int i=0; i<metaScreenEdges->propertyCount(); ++i) {
        const QMetaProperty property = metaScreenEdges->property(i);
        if (QLatin1String(property.name()) == "objectName") {
            continue;
        }
        support.append(QLatin1String(property.name()) % ": " % ScreenEdges::self()->property(property.name()).toString() % '\n');
    }
#endif
    support.append("\nScreens\n");
    support.append(  "=======\n");
    support.append("Multi-Head: ");
    if (is_multihead) {
        support.append("yes\n");
        support.append(QString("Head: %1\n").arg(screen_number));
    } else {
        support.append("no\n");
    }
    support.append(QString("Number of Screens: %1\n").arg(QApplication::desktop()->screenCount()));
    for (int i=0; i<QApplication::desktop()->screenCount(); ++i) {
        const QRect geo = QApplication::desktop()->screenGeometry(i);
        support.append(QString("Screen %1 Geometry: %2,%3,%4x%5\n")
                              .arg(i)
                              .arg(geo.x())
                              .arg(geo.y())
                              .arg(geo.width())
                              .arg(geo.height()));
    }
    support.append("\nCompositing\n");
    support.append(  "===========\n");
    support.append("Qt Graphics System: ");
    if (Extensions::nonNativePixmaps()) {
        support.append("raster\n");
    } else {
        support.append("native\n");
    }
    if (effects) {
        support.append("Compositing is active\n");
        switch (effects->compositingType()) {
        case OpenGL1Compositing:
        case OpenGL2Compositing:
        case OpenGLCompositing: {
#ifdef KWIN_HAVE_OPENGLES
            support.append("Compositing Type: OpenGL ES 2.0\n");
#else
            support.append("Compositing Type: OpenGL\n");
#endif

            GLPlatform *platform = GLPlatform::instance();
            support.append("OpenGL vendor string: " %   platform->glVendorString() % '\n');
            support.append("OpenGL renderer string: " % platform->glRendererString() % '\n');
            support.append("OpenGL version string: " %  platform->glVersionString() % '\n');

            if (platform->supports(LimitedGLSL))
                support.append("OpenGL shading language version string: " % platform->glShadingLanguageVersionString() % '\n');

            support.append("Driver: " % GLPlatform::driverToString(platform->driver()) % '\n');
            if (!platform->isMesaDriver())
                support.append("Driver version: " % GLPlatform::versionToString(platform->driverVersion()) % '\n');

            support.append("GPU class: " % GLPlatform::chipClassToString(platform->chipClass()) % '\n');

            support.append("OpenGL version: " % GLPlatform::versionToString(platform->glVersion()) % '\n');

            if (platform->supports(LimitedGLSL))
                support.append("GLSL version: " % GLPlatform::versionToString(platform->glslVersion()) % '\n');

            if (platform->isMesaDriver())
                support.append("Mesa version: " % GLPlatform::versionToString(platform->mesaVersion()) % '\n');
            if (platform->serverVersion() > 0)
                support.append("X server version: " % GLPlatform::versionToString(platform->serverVersion()) % '\n');
            if (platform->kernelVersion() > 0)
                support.append("Linux kernel version: " % GLPlatform::versionToString(platform->kernelVersion()) % '\n');

            support.append("Direct rendering: ");
            if (platform->isDirectRendering()) {
                support.append("yes\n");
            } else {
                support.append("no\n");
            }
            support.append("Requires strict binding: ");
            if (!platform->isLooseBinding()) {
                support.append("yes\n");
            } else {
                support.append("no\n");
            }
            support.append("GLSL shaders: ");
            if (platform->supports(GLSL)) {
                if (platform->supports(LimitedGLSL)) {
                    support.append(" limited\n");
                } else {
                    support.append(" yes\n");
                }
            } else {
                support.append(" no\n");
            }
            support.append("Texture NPOT support: ");
            if (platform->supports(TextureNPOT)) {
                if (platform->supports(LimitedNPOT)) {
                    support.append(" limited\n");
                } else {
                    support.append(" yes\n");
                }
            } else {
                support.append(" no\n");
            }
            support.append("Virtual Machine: ");
            if (platform->isVirtualMachine()) {
                support.append(" yes\n");
            } else {
                support.append(" no\n");
            }

            if (effects->compositingType() == OpenGL2Compositing) {
                support.append("OpenGL 2 Shaders are used\n");
            } else {
                support.append("OpenGL 2 Shaders are not used. Legacy OpenGL 1.x code path is used.\n");
            }
            break;
        }
        case XRenderCompositing:
            support.append("Compositing Type: XRender\n");
            break;
        case NoCompositing:
        default:
            support.append("Something is really broken, neither OpenGL nor XRender is used");
        }
        support.append("\nLoaded Effects:\n");
        support.append(  "---------------\n");
        foreach (const QString &effect, static_cast<EffectsHandlerImpl*>(effects)->loadedEffects()) {
            support.append(effect % '\n');
        }
        support.append("\nCurrently Active Effects:\n");
        support.append(  "-------------------------\n");
        foreach (const QString &effect, static_cast<EffectsHandlerImpl*>(effects)->activeEffects()) {
            support.append(effect % '\n');
        }
        support.append("\nEffect Settings:\n");
        support.append(  "----------------\n");
        foreach (const QString &effect, static_cast<EffectsHandlerImpl*>(effects)->loadedEffects()) {
            support.append(static_cast<EffectsHandlerImpl*>(effects)->supportInformation(effect));
            support.append('\n');
        }
    } else {
        support.append("Compositing is not active\n");
    }
    return support;
}

void Workspace::slotCompositingToggled()
{
    // notify decorations that composition state has changed
    if (hasDecorationPlugin()) {
        KDecorationFactory* factory = mgr->factory();
        factory->reset(SettingCompositing);
    }
}

void Workspace::slotToggleCompositing()
{
    if (m_compositor) {
        m_compositor->slotToggleCompositing();
    }
}

} // namespace

#include "workspace.moc"
