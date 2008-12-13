// -*- Mode: C++; c-basic-offset: 4; indent-tabs-mode: nil; tab-width: 8; -*-
/* This file is part of the KDE project

   Copyright (C) by Andrew Stanley-Jones
   Copyright (C) 2000 by Carsten Pfeiffer <pfeiffer@kde.org>
   Copyright (C) 2004  Esben Mose Hansen <kde@mosehansen.dk>

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; see the file COPYING.  If not, write to
   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.
*/

#include <QClipboard>
#include <QtDBus/QDBusConnection>

#include <kaboutdata.h>
#include <klocale.h>
#include <kmessagebox.h>
#include <ksavefile.h>
#include <ksessionmanager.h>
#include <kstandarddirs.h>
#include <ksystemtrayicon.h>
#include <kdebug.h>
#include <kglobalsettings.h>
#include <kactioncollection.h>
#include <ktoggleaction.h>
#include <KConfigSkeleton>

#include "configdialog.h"
#include "klipper.h"
#include "urlgrabber.h"
#include "version.h"
#include "clipboardpoll.h"
#include "history.h"
#include "historyitem.h"
#include "historystringitem.h"
#include "klipperpopup.h"

#include <zlib.h>

#include <X11/Xlib.h>
#include <X11/Xatom.h>

//#define NOISY_KLIPPER

namespace {
    /**
     * Use this when manipulating the clipboard
     * from within clipboard-related signals.
     *
     * This avoids issues such as mouse-selections that immediately
     * disappear.
     * pattern: Resource Acqusition is Initialisation (RAII)
     *
     * (This is not threadsafe, so don't try to use such in threaded
     * applications).
     */
    struct Ignore {
        Ignore(int& locklevel) : locklevelref(locklevel)  {
            locklevelref++;
        }
        ~Ignore() {
            locklevelref--;
        }
    private:
        int& locklevelref;
    };
}

/**
 * Helper class to save history upon session exit.
 */
class KlipperSessionManager : public KSessionManager
{
public:
    KlipperSessionManager( Klipper* k )
        : klipper( k )
        {}

    virtual ~KlipperSessionManager() {}

    /**
     * Save state upon session exit.
     *
     * Saving history on session save
     */
    virtual bool commitData( QSessionManager& ) {
        klipper->saveSession();
        return true;
    }
private:
    Klipper* klipper;
};

static void ensureGlobalSyncOff(KSharedConfigPtr config);

// config == KGlobal::config for process, otherwise applet
Klipper::Klipper(QObject *parent, const KSharedConfigPtr &config)
    : QObject( parent )
    , m_overflowCounter( 0 )
    , m_locklevel( 0 )
    , m_config( config )
    , m_pendingContentsCheck( false )
    , m_session_managed( new KlipperSessionManager( this ))
{
    QDBusConnection::sessionBus().registerObject("/klipper", this, QDBusConnection::ExportScriptableSlots);

    // We don't use the clipboardsynchronizer anymore, and it confuses Klipper
    ensureGlobalSyncOff(m_config);

    updateTimestamp(); // read initial X user time
    m_clip = kapp->clipboard();

    connect( &m_overflowClearTimer, SIGNAL( timeout()), SLOT( slotClearOverflow()));
    m_overflowClearTimer.start( 1000 );

    m_pendingCheckTimer.setSingleShot( true );
    connect( &m_pendingCheckTimer, SIGNAL( timeout()), SLOT( slotCheckPending()));

    m_history = new History( this );

    // we need that collection, otherwise KToggleAction is not happy :}
    //QString defaultGroup( "default" );
    m_collection = new KActionCollection( this );
    m_toggleURLGrabAction = new KToggleAction( this );
    m_collection->addAction( "toggleUrlGrabAction", m_toggleURLGrabAction );
    m_toggleURLGrabAction->setEnabled( true );
    m_toggleURLGrabAction->setText(i18n("Enable &Actions"));
    //m_toggleURLGrabAction->setGroup( defaultGroup );
    m_clearHistoryAction = m_collection->addAction( "clearHistoryAction" );
    m_clearHistoryAction->setIcon( KIcon("edit-clear-history") );
    m_clearHistoryAction->setText( i18n("C&lear Clipboard History") );
    connect(m_clearHistoryAction, SIGNAL(triggered() ), history(), SLOT( slotClear() ));
    connect( m_clearHistoryAction, SIGNAL( triggered() ), SLOT( slotClearClipboard() ) );
    //m_clearHistoryAction->setGroup( defaultGroup );
    m_configureAction = m_collection->addAction( "configureAction" );
    m_configureAction->setIcon( KIcon("configure") );
    m_configureAction->setText( i18n("&Configure Klipper...") );
    connect(m_configureAction, SIGNAL(triggered(bool) ), SLOT( slotConfigure() ));
    //m_configureAction->setGroup( defaultGroup );
    m_quitAction = m_collection->addAction( "quitAction" );
    m_quitAction->setIcon( KIcon("application-exit") );
    m_quitAction->setText( i18n("&Quit") );
    connect(m_quitAction, SIGNAL(triggered(bool) ), SLOT( slotQuit() ));
    //quitAction->setGroup( "exit" );
    m_myURLGrabber = 0L;
    KConfig *kc = m_config.data();
    readConfiguration( kc );
    setURLGrabberEnabled( m_bURLGrabber );

    m_hideTimer = new QTime();
    m_showTimer = new QTime();

    readProperties(m_config.data());
    connect(KGlobalSettings::self(), SIGNAL(settingsChanged(int)), SLOT(slotSettingsChanged(int)));

    m_poll = new ClipboardPoll;
    connect( m_poll, SIGNAL( clipboardChanged( bool ) ),
             this, SLOT( newClipData( bool ) ) );

    QAction *a = m_collection->addAction("show_klipper_popup");
    a->setText(i18n("Show Klipper Popup-Menu"));
    qobject_cast<KAction*>(a)->setGlobalShortcut(KShortcut(Qt::ALT+Qt::CTRL+Qt::Key_V));
    connect(a, SIGNAL(triggered()), SLOT(slotPopupMenu()));

    a = m_collection->addAction("repeat_action");
    a->setText(i18n("Manually Invoke Action on Current Clipboard"));
    qobject_cast<KAction*>(a)->setGlobalShortcut(KShortcut(Qt::ALT+Qt::CTRL+Qt::Key_R));
    connect(a, SIGNAL(triggered()), SLOT(slotRepeatAction()));

    a = m_collection->addAction("clipboard_action");
    a->setText(i18n("Enable/Disable Clipboard Actions"));
    qobject_cast<KAction*>(a)->setGlobalShortcut(KShortcut(Qt::ALT+Qt::CTRL+Qt::Key_X));
    connect(a, SIGNAL(triggered()), SLOT(toggleURLGrabber()));

    m_toggleURLGrabAction->setShortcut(qobject_cast<KAction*>(m_collection->action("clipboard_action"))->globalShortcut());

    connect( m_toggleURLGrabAction, SIGNAL( toggled( bool )),
             this, SLOT( setURLGrabberEnabled( bool )));

    KlipperPopup* popup = history()->popup();
    connect ( history(), SIGNAL( topChanged() ), SLOT( slotHistoryTopChanged() ) );
    connect( popup, SIGNAL( aboutToHide() ), SLOT( slotStartHideTimer() ) );
    connect( popup, SIGNAL( aboutToShow() ), SLOT( slotStartShowTimer() ) );

    popup->plugAction( m_toggleURLGrabAction );
    popup->plugAction( m_clearHistoryAction );
    popup->plugAction( m_configureAction );
    if ( !isApplet() ) {
        popup->plugAction( m_quitAction );
    }
}

Klipper::~Klipper()
{
    delete m_poll;
    delete m_session_managed;
    delete m_showTimer;
    delete m_hideTimer;
    delete m_myURLGrabber;
}

// DCOP
QString Klipper::getClipboardContents()
{
    return getClipboardHistoryItem(0);
}

void Klipper::showKlipperPopupMenu() {
    slotPopupMenu();
}
void Klipper::showKlipperManuallyInvokeActionMenu() {
    slotRepeatAction();
}


// DCOP - don't call from Klipper itself
void Klipper::setClipboardContents(QString s)
{
    if (s.isEmpty())
        return;
    Ignore lock( m_locklevel );
    updateTimestamp();
    HistoryStringItem* item = new HistoryStringItem( s );
    setClipboard( *item, Clipboard | Selection);
    history()->insert( item );
}

// DCOP - don't call from Klipper itself
void Klipper::clearClipboardContents()
{
    updateTimestamp();
    slotClearClipboard();
}

// DCOP - don't call from Klipper itself
void Klipper::clearClipboardHistory()
{
    updateTimestamp();
    slotClearClipboard();
    history()->slotClear();
    saveSession();
}


void Klipper::slotStartHideTimer()
{
    m_hideTimer->start();
}

void Klipper::slotStartShowTimer()
{
    m_showTimer->start();
}

void Klipper::showPopupMenu( QMenu *menu )
{
    Q_ASSERT( menu != 0L );

    QSize size = menu->sizeHint(); // geometry is not valid until it's shown
    if (m_bPopupAtMouse) {
        QPoint g = QCursor::pos();
        if ( size.height() < g.y() )
            menu->popup(QPoint( g.x(), g.y() - size.height()));
        else
            menu->popup(QPoint(g.x(), g.y()));
    } else {
        if( KSystemTrayIcon* tray = dynamic_cast< KSystemTrayIcon* >( parent())) {
            QRect g = tray->geometry();
            QRect screen = KGlobalSettings::desktopGeometry(g.center());

            if ( g.x()-screen.x() > screen.width()/2 &&
                 g.y()-screen.y() + size.height() > screen.height() )
                menu->popup(QPoint( g.x(), g.y() - size.height()));
            else
                menu->popup(QPoint( g.x() + g.width(), g.y() + g.height()));
        } else
            abort();

        //      menu->exec(mapToGlobal(QPoint( width()/2, height()/2 )));
    }
}

bool Klipper::loadHistory() {
    static const char* const failed_load_warning =
        "Failed to load history resource. Clipboard history cannot be read.";
    // don't use "appdata", klipper is also a kicker applet
    QString history_file_name = KStandardDirs::locateLocal( "data", "klipper/history2.lst" );
    QFile history_file( history_file_name );
    bool oldfile = false;
    if ( !history_file.exists() ) { // backwards compatibility
        oldfile = true;
        history_file_name = KStandardDirs::locateLocal( "data", "klipper/history.lst" );
        history_file.setFileName( history_file_name );
        if ( !history_file.exists() ) {
            history_file_name = KStandardDirs::locateLocal( "data", "kicker/history.lst" );
            history_file.setFileName( history_file_name );
            if ( !history_file.exists() ) {
                return false;
            }
        }
    }
    if ( !history_file.open( QIODevice::ReadOnly ) ) {
        kWarning() << failed_load_warning << ": " << history_file.errorString() ;
        return false;
    }
    QDataStream file_stream( &history_file );
    if( file_stream.atEnd()) {
        kWarning() << failed_load_warning ;
        return false;
    }
    QDataStream* history_stream = &file_stream;
    QByteArray data;
    if( !oldfile ) {
        quint32 crc;
        file_stream >> crc >> data;
        if( crc32( 0, reinterpret_cast<unsigned char *>( data.data() ), data.size() ) != crc ) {
            kWarning() << failed_load_warning << ": " << history_file.errorString() ;
            return false;
        }
        history_stream = new QDataStream( &data, QIODevice::ReadOnly );
    }
    char* version;
    *history_stream >> version;
    delete[] version;

    // The list needs to be reversed, as it is saved
    // youngest-first to keep the most important clipboard
    // items at the top, but the history is created oldest
    // first.
    QList<HistoryItem*> reverseList;
    for ( HistoryItem* item = HistoryItem::create( *history_stream );
          item;
          item = HistoryItem::create( *history_stream ) )
    {
        reverseList.prepend( item );
    }

    for ( QList<HistoryItem*>::const_iterator it = reverseList.constBegin();
          it != reverseList.constEnd();
          ++it )
    {
        history()->forceInsert( *it );
    }

    if ( !history()->empty() ) {
        m_lastSelection = -1;
        m_lastClipboard = -1;
        setClipboard( *history()->first(), Clipboard | Selection );
    }

    if( history_stream != &file_stream )
        delete history_stream;

    return true;
}

void Klipper::saveHistory() {
    static const char* const failed_save_warning =
        "Failed to save history. Clipboard history cannot be saved.";
    // don't use "appdata", klipper is also a kicker applet
    QString history_file_name( KStandardDirs::locateLocal( "data", "klipper/history2.lst" ) );
    if ( history_file_name.isNull() || history_file_name.isEmpty() ) {
        kWarning() << failed_save_warning ;
        return;
    }
    KSaveFile history_file( history_file_name );
    if ( !history_file.open() ) {
        kWarning() << failed_save_warning ;
        return;
    }
    QByteArray data;
    QDataStream history_stream( &data, QIODevice::WriteOnly );
    history_stream << klipper_version; // const char*

    History::iterator it = history()->youngest();
    while (it.hasNext()) {
        const HistoryItem *item = it.next();
        history_stream << item;
    }

    quint32 crc = crc32( 0, reinterpret_cast<unsigned char *>( data.data() ), data.size() );
    QDataStream ds ( &history_file );
    ds << crc << data;
}

void Klipper::readProperties(KConfig *kc)
{
    QStringList dataList;

    history()->slotClear();

    if (m_bKeepContents) { // load old clipboard if configured
        if ( !loadHistory() ) {
            // Try to load from the old config file.
            // Remove this at some point.
            KConfigGroup configGroup(kc, "General");
            dataList = configGroup.readEntry("ClipboardData",QStringList());

            for (QStringList::ConstIterator it = dataList.constEnd();
                 it != dataList.constBegin();
             )
            {
                history()->forceInsert( new HistoryStringItem( *( --it ) ) );
            }

            if ( !dataList.isEmpty() )
            {
                m_lastSelection = -1;
                m_lastClipboard = -1;
                setClipboard( *history()->first(), Clipboard | Selection );
            }
        }
    }

}


void Klipper::readConfiguration( KConfig *_kc )
{
    KConfigGroup kc( _kc, "General");
    m_bPopupAtMouse = kc.readEntry("PopupAtMousePosition", false);
    m_bKeepContents = kc.readEntry("KeepClipboardContents", true);
    m_bURLGrabber = kc.readEntry("URLGrabberEnabled", false);
    m_bReplayActionInHistory = kc.readEntry("ReplayActionInHistory", false);
    m_bNoNullClipboard = kc.readEntry("NoEmptyClipboard", true);
    m_bUseGUIRegExpEditor = kc.readEntry("UseGUIRegExpEditor", true);
    history()->max_size( kc.readEntry("MaxClipItems", 7) );
    m_bIgnoreSelection = kc.readEntry("IgnoreSelection", false);
    m_bSynchronize = kc.readEntry("Synchronize", false);
    m_bSelectionTextOnly = kc.readEntry("SelectionTextOnly",true);
    m_bIgnoreImages = kc.readEntry("IgnoreImages", true);
}

void Klipper::writeConfiguration( KConfig *_kc )
{
    KConfigGroup kc( _kc, "General");
    kc.writeEntry("PopupAtMousePosition", m_bPopupAtMouse);
    kc.writeEntry("KeepClipboardContents", m_bKeepContents);
    kc.writeEntry("ReplayActionInHistory", m_bReplayActionInHistory);
    kc.writeEntry("NoEmptyClipboard", m_bNoNullClipboard);
    kc.writeEntry("UseGUIRegExpEditor", m_bUseGUIRegExpEditor);
    kc.writeEntry("MaxClipItems", history()->max_size() );
    kc.writeEntry("IgnoreSelection", m_bIgnoreSelection);
    kc.writeEntry("Synchronize", m_bSynchronize );
    kc.writeEntry("SelectionTextOnly", m_bSelectionTextOnly);
    kc.writeEntry("IgnoreImages", m_bIgnoreImages);
    kc.writeEntry("Version", klipper_version );

    if ( m_myURLGrabber )
        m_myURLGrabber->writeConfiguration( _kc );

    kc.sync();
}

// save session on shutdown. Don't simply use the c'tor, as that may not be called.
void Klipper::saveSession()
{
    if ( m_bKeepContents ) { // save the clipboard eventually
        saveHistory();
    }
}

void Klipper::slotSettingsChanged( int category )
{
    if ( category == (int) KGlobalSettings::SETTINGS_SHORTCUTS ) {
        m_toggleURLGrabAction->setShortcut(qobject_cast<KAction*>(m_collection->action("clipboard_action"))->globalShortcut());
    }
}

void Klipper::disableURLGrabber()
{
    KMessageBox::information( 0L,
                              i18n( "You can enable URL actions later by right-clicking on the "
                                    "Klipper icon and selecting 'Enable Actions'" ) );

    setURLGrabberEnabled( false );
}

void Klipper::slotConfigure()
{
    bool haveURLGrabber = m_bURLGrabber;
    if ( !m_myURLGrabber ) { // temporary, for the config-dialog
        setURLGrabberEnabled( true );
        readConfiguration( m_config.data() );
    }
    KConfigSkeleton *skeleton = new KConfigSkeleton();
    ConfigDialog *dlg = new ConfigDialog( 0, skeleton, m_myURLGrabber->actionList(), m_collection, isApplet() );
    dlg->setKeepContents( m_bKeepContents );
    dlg->setPopupAtMousePos( m_bPopupAtMouse );
    dlg->setStripWhiteSpace( m_myURLGrabber->trimmed() );
    dlg->setReplayActionInHistory( m_bReplayActionInHistory );
    dlg->setNoNullClipboard( m_bNoNullClipboard );
    dlg->setUseGUIRegExpEditor( m_bUseGUIRegExpEditor );
    dlg->setPopupTimeout( m_myURLGrabber->popupTimeout() );
    dlg->setMaxItems( history()->max_size() );
    dlg->setIgnoreSelection( m_bIgnoreSelection );
    dlg->setIgnoreImages( m_bIgnoreImages );
    dlg->setSynchronize( m_bSynchronize );
    dlg->setNoActionsFor( m_myURLGrabber->avoidWindows() );

    if ( dlg->exec() == QDialog::Accepted ) {
        m_bKeepContents = dlg->keepContents();
        m_bPopupAtMouse = dlg->popupAtMousePos();
        m_bReplayActionInHistory = dlg->replayActionInHistory();
        m_bNoNullClipboard = dlg->noNullClipboard();
        m_bIgnoreSelection = dlg->ignoreSelection();
        m_bIgnoreImages = dlg->ignoreImages();
        m_bSynchronize = dlg->synchronize();
        m_bUseGUIRegExpEditor = dlg->useGUIRegExpEditor();
        dlg->commitShortcuts();

        m_toggleURLGrabAction->setShortcut(qobject_cast<KAction*>(m_collection->action("clipboard_action"))->globalShortcut());

        m_myURLGrabber->setActionList( dlg->actionList() );
        m_myURLGrabber->setPopupTimeout( dlg->popupTimeout() );
        m_myURLGrabber->setStripWhiteSpace( dlg->trimmed() );
        m_myURLGrabber->setAvoidWindows( dlg->noActionsFor() );

        history()->max_size( dlg->maxItems() );

        writeConfiguration( m_config.data() );

    }
    setURLGrabberEnabled( haveURLGrabber );

    delete skeleton;
    delete dlg;
}

void Klipper::slotQuit()
{
    // If the menu was just opened, likely the user
    // selected quit by accident while attempting to
    // click the Klipper icon.
    if ( m_showTimer->elapsed() < 300 ) {
        return;
    }

    saveSession();
    int autoStart = KMessageBox::questionYesNoCancel(0, i18n("Should Klipper start automatically when you login?"),
                                                     i18n("Automatically Start Klipper?"), KGuiItem(i18n("Start")),
                                                     KGuiItem(i18n("Do Not Start")), KStandardGuiItem::cancel(), "StartAutomatically");

    KConfigGroup config( KGlobal::config(), "General");
    if ( autoStart == KMessageBox::Yes ) {
        config.writeEntry("AutoStart", true);
    } else if ( autoStart == KMessageBox::No) {
        config.writeEntry("AutoStart", false);
    } else  // cancel chosen don't quit
        return;
    config.sync();

    kapp->quit();

}

void Klipper::slotPopupMenu() {
    KlipperPopup* popup = history()->popup();
    popup->ensureClean();
    showPopupMenu( popup );
}


void Klipper::slotRepeatAction()
{
    if ( !m_myURLGrabber ) {
        m_myURLGrabber = new URLGrabber( m_config );
        connect( m_myURLGrabber, SIGNAL( sigPopup( QMenu * )),
                 SLOT( showPopupMenu( QMenu * )) );
        connect( m_myURLGrabber, SIGNAL( sigDisablePopup() ),
                 this, SLOT( disableURLGrabber() ) );
    }

    const HistoryStringItem* top = dynamic_cast<const HistoryStringItem*>( history()->first() );
    if ( top ) {
        m_myURLGrabber->invokeAction( top->text() );
    }
}

void Klipper::setURLGrabberEnabled( bool enable )
{
    if (enable != m_bURLGrabber) {
      m_bURLGrabber = enable;
      KConfigGroup kc(m_config.data(), "General");
      kc.writeEntry("URLGrabberEnabled", m_bURLGrabber);
      m_lastURLGrabberTextSelection = QString();
      m_lastURLGrabberTextClipboard = QString();
    }

    m_toggleURLGrabAction->setChecked( enable );

    if ( !m_bURLGrabber ) {
        delete m_myURLGrabber;
        m_myURLGrabber = 0L;
    }

    else {
        if ( !m_myURLGrabber ) {
            m_myURLGrabber = new URLGrabber( m_config );
            connect( m_myURLGrabber, SIGNAL( sigPopup( QMenu * )),
                     SLOT( showPopupMenu( QMenu * )) );
            connect( m_myURLGrabber, SIGNAL( sigDisablePopup() ),
                     this, SLOT( disableURLGrabber() ) );
        }
    }
}

void Klipper::toggleURLGrabber()
{
    setURLGrabberEnabled( !m_bURLGrabber );
}

void Klipper::slotHistoryTopChanged() {
    if ( m_locklevel ) {
        return;
    }

    const HistoryItem* topitem = history()->first();
    if ( topitem ) {
        setClipboard( *topitem, Clipboard | Selection );
    }
    if ( m_bReplayActionInHistory && m_bURLGrabber ) {
        slotRepeatAction();
    }
}

void Klipper::slotClearClipboard()
{
    Ignore lock( m_locklevel );

    m_clip->clear(QClipboard::Selection);
    m_clip->clear(QClipboard::Clipboard);
}


//XXX: Should die, and the DCOP signal handled sensible.
QString Klipper::clipboardContents( bool * /*isSelection*/ )
{
    kWarning() << "Obsolete function called. Please fix" ;

#if 0
    bool selection = true;
    QMimeSource* data = clip->data(QClipboard::Selection);

    if ( data->serialNumber() == m_lastSelection )
    {
        QString clipContents = clip->text(QClipboard::Clipboard);
        if ( clipContents != m_lastClipboard )
        {
            contents = clipContents;
            selection = false;
        }
        else
            selection = true;
    }

    if ( isSelection )
        *isSelection = selection;

#endif

    return 0;
}

void Klipper::applyClipChanges( const QMimeData* clipData )
{
    if ( m_locklevel )
        return;
    Ignore lock( m_locklevel );
    history()->insert( HistoryItem::create( clipData ) );

}

void Klipper::newClipData( bool selectionMode )
{
    if ( m_locklevel ) {
        return;
    }

    if( blockFetchingNewData())
        return;

    checkClipData( selectionMode );

}

void Klipper::clipboardSignalArrived( bool selectionMode )
{
    if ( m_locklevel ) {
        return;
    }
    if( blockFetchingNewData())
        return;

    updateTimestamp();
    checkClipData( selectionMode );
}

// Protection against too many clipboard data changes. Lyx responds to clipboard data
// requests with setting new clipboard data, so if Lyx takes over clipboard,
// Klipper notices, requests this data, this triggers "new" clipboard contents
// from Lyx, so Klipper notices again, requests this data, ... you get the idea.
const int MAX_CLIPBOARD_CHANGES = 10; // max changes per second

bool Klipper::blockFetchingNewData()
{
// Hacks for #85198 and #80302.
// #85198 - block fetching new clipboard contents if Shift is pressed and mouse is not,
//   this may mean the user is doing selection using the keyboard, in which case
//   it's possible the app sets new clipboard contents after every change - Klipper's
//   history would list them all.
// #80302 - OOo (v1.1.3 at least) has a bug that if Klipper requests its clipboard contents
//   while the user is doing a selection using the mouse, OOo stops updating the clipboard
//   contents, so in practice it's like the user has selected only the part which was
//   selected when Klipper asked first.
// Use XQueryPointer rather than QApplication::mouseButtons()/keyboardModifiers(), because
//   Klipper needs the very current state.
    Window root, child;
    int root_x, root_y, win_x, win_y;
    uint state;
    XQueryPointer( QX11Info::display(), QX11Info::appRootWindow(), &root, &child,
                   &root_x, &root_y, &win_x, &win_y, &state );
    if( ( state & ( ShiftMask | Button1Mask )) == ShiftMask // #85198
        || ( state & Button1Mask ) == Button1Mask ) { // #80302
        m_pendingContentsCheck = true;
        m_pendingCheckTimer.start( 100 );
        return true;
    }
    m_pendingContentsCheck = false;
    if( ++m_overflowCounter > MAX_CLIPBOARD_CHANGES )
        return true;
    return false;
}

void Klipper::slotCheckPending()
{
    if( !m_pendingContentsCheck )
        return;
    m_pendingContentsCheck = false; // blockFetchingNewData() will be called again
    updateTimestamp();
    newClipData( true ); // always selection
}

void Klipper::checkClipData( bool selectionMode )
{
    if ( ignoreClipboardChanges() ) // internal to klipper, ignoring QSpinBox selections
    {
        // keep our old clipboard, thanks
        // This won't quite work, but it's close enough for now.
        // The trouble is that the top selection =! top clipboard
        // but we don't track that yet. We will....
        const HistoryItem* top = history()->first();
        if ( top ) {
            setClipboard( *top, selectionMode ? Selection : Clipboard);
        }
        return;
    }

// debug code
#ifdef NOISY_KLIPPER
    kDebug() << "Checking clip data";
#endif
#if 0
    kDebug() << "====== c h e c k C l i p D a t a ============================"
              << kBacktrace()
              << "====== c h e c k C l i p D a t a ============================"
              << endl;;
#endif
#if 0
    if ( sender() ) {
        kDebug() << "sender=" << sender()->name();
    } else {
        kDebug() << "no sender";
    }
#endif
#if 0
    kDebug() << "\nselectionMode=" << selectionMode
              << "\nserialNo=" << clip->data()->serialNumber() << " (sel,cli)=(" << m_lastSelection << "," << m_lastClipboard << ")"
              << "\nowning (sel,cli)=(" << clip->ownsSelection() << "," << clip->ownsClipboard() << ")"
              << "\ntext=" << clip->text( selectionMode ? QClipboard::Selection : QClipboard::Clipboard) << endl;

#endif
#if 0
    const char *format;
    int i = 0;
    while ( (format = clip->data()->format( i++ )) )
    {
        qDebug( "    format: %s", format);
    }
#endif
    const QMimeData* data = m_clip->mimeData( selectionMode ? QClipboard::Selection : QClipboard::Clipboard );
    if ( !data ) {
        kWarning("No data in clipboard. This not not supposed to happen." );
        return;
    }
    // TODO: Rewrite to Qt4 !!!
    //int lastSerialNo = selectionMode ? m_lastSelection : m_lastClipboard;
    //bool changed = data->serialNumber() != lastSerialNo;
    bool changed = true; // ### FIXME
    bool clipEmpty = data->text().isEmpty() && !data->hasImage();

    if ( changed && clipEmpty && m_bNoNullClipboard ) {
        const HistoryItem* top = history()->first();
        if ( top ) {
            // keep old clipboard after someone set it to null
#ifdef NOISY_KLIPPER
            kDebug() << "Resetting clipboard (Prevent empty clipboard)";
#endif
            setClipboard( *top, selectionMode ? Selection : Clipboard );
        }
        return;
    }

    // this must be below the "bNoNullClipboard" handling code!
    // XXX: I want a better handling of selection/clipboard in general.
    // XXX: Order sensitive code. Must die.
    if ( selectionMode && m_bIgnoreSelection )
        return;

    if( selectionMode && m_bSelectionTextOnly && !data->hasText())
        return;

    if( KUrl::List::canDecode( data ) )
        ; // ok
    else if( data->hasText() )
        ; // ok
    else if( data->hasImage() )
    {
        if( m_bIgnoreImages )
            return;
    }
    else // unknown, ignore
        return;

    // store old contents:
#if 0
    if ( selectionMode )
        m_lastSelection = data->serialNumber();
    else
        m_lastClipboard = data->serialNumber();
#endif

    QString& lastURLGrabberText = selectionMode
        ? m_lastURLGrabberTextSelection : m_lastURLGrabberTextClipboard;
    if( data->hasText() )
    {
        if ( m_bURLGrabber && m_myURLGrabber )
        {
            QString text = data->text();

            // Make sure URLGrabber doesn't repeat all the time if klipper reads the same
            // text all the time (e.g. because XFixes is not available and the application
            // has broken TIMESTAMP target). Using most recent history item may not always
            // work.
            if ( text != lastURLGrabberText )
            {
                lastURLGrabberText = text;
                if ( m_myURLGrabber->checkNewData( text ) )
                {
                    return; // don't add into the history
                }
            }
        }
        else
            lastURLGrabberText = QString();
    }
    else
        lastURLGrabberText = QString();

    if (changed) {
        applyClipChanges( data );
#ifdef NOISY_KLIPPER
        kDebug() << "Synchronize?" << m_bSynchronize;
#endif
        if ( m_bSynchronize ) {
            const HistoryItem* topItem = history()->first();
            if ( topItem ) {
                setClipboard( *topItem, selectionMode ? Clipboard : Selection );
            }
        }
    }
}

void Klipper::setClipboard( const HistoryItem& item, int mode )
{
    Ignore lock( m_locklevel );

    Q_ASSERT( ( mode & 1 ) == 0 ); // Warn if trying to pass a boolean as a mode.

    if ( mode & Selection ) {
#ifdef NOISY_KLIPPER
        kDebug() << "Setting selection to <" << item.text() << ">";
#endif
        m_clip->setMimeData( item.mimeData(), QClipboard::Selection );
#if 0
        m_lastSelection = clip->data()->serialNumber();<
#endif
    }
    if ( mode & Clipboard ) {
#ifdef NOISY_KLIPPER
        kDebug() << "Setting clipboard to <" << item.text() << ">";
#endif
        m_clip->setMimeData( item.mimeData(), QClipboard::Clipboard );
#if 0
        m_lastClipboard = clip->data()->serialNumber();
#endif
    }

}

void Klipper::slotClearOverflow()
{
    if( m_overflowCounter > MAX_CLIPBOARD_CHANGES ) {
        kDebug() << "App owning the clipboard/selection is lame";
        // update to the latest data - this unfortunately may trigger the problem again
        newClipData( true ); // Always the selection.
    }
    m_overflowCounter = 0;
}

QStringList Klipper::getClipboardHistoryMenu()
{
    QStringList menu;

    History::iterator it = history()->youngest();
    while (it.hasNext()) {
        const HistoryItem *item = it.next();
        menu << item->text();
    }

    return menu;
}

QString Klipper::getClipboardHistoryItem(int i)
{
    History::iterator it = history()->youngest();
    while (it.hasNext()) {
        const HistoryItem *item = it.next();
        if ( i == 0 ) {
            return item->text();
        }
        i--;
    }
    return QString();

}

//
// changing a spinbox in klipper's config-dialog causes the lineedit-contents
// of the spinbox to be selected and hence the clipboard changes. But we don't
// want all those items in klipper's history. See #41917
//
bool Klipper::ignoreClipboardChanges() const
{
    QWidget *focusWidget = qApp->focusWidget();
    if ( focusWidget )
    {
        if ( focusWidget->inherits( "QSpinBox" ) ||
             (focusWidget->parentWidget() &&
              focusWidget->inherits("QLineEdit") &&
              focusWidget->parentWidget()->inherits("QSpinWidget")) )
        {
            return true;
        }
    }

    return false;
}

// QClipboard uses qt_x_time as the timestamp for selection operations.
// It is updated mainly from user actions, but Klipper polls the clipboard
// without any user action triggering it, so qt_x_time may be old,
// which could possibly lead to QClipboard reporting empty clipboard.
// Therefore, qt_x_time needs to be updated to current X server timestamp.

// Call KApplication::updateUserTime() only from functions that are
// called from outside (DCOP), or from QTimer timeout !

static Time next_x_time;
static Bool update_x_time_predicate( Display*, XEvent* event, XPointer )
{
    if( next_x_time != CurrentTime )
        return False;
    // from qapplication_x11.cpp
    switch ( event->type ) {
    case ButtonPress:
      // fallthrough intended
    case ButtonRelease:
      next_x_time = event->xbutton.time;
      break;
    case MotionNotify:
      next_x_time = event->xmotion.time;
      break;
    case KeyPress:
      // fallthrough intended
    case KeyRelease:
      next_x_time = event->xkey.time;
      break;
    case PropertyNotify:
      next_x_time = event->xproperty.time;
      break;
    case EnterNotify:
    case LeaveNotify:
      next_x_time = event->xcrossing.time;
      break;
    case SelectionClear:
      next_x_time = event->xselectionclear.time;
      break;
    default:
      break;
    }
    return False;
}

void Klipper::updateTimestamp()
{
    static QWidget* w = 0;
    if ( !w )
        w = new QWidget;
    unsigned char data[ 1 ];
    XChangeProperty( QX11Info::display(), w->winId(), XA_ATOM, XA_ATOM, 8, PropModeAppend, data, 1 );
    next_x_time = CurrentTime;
    XEvent dummy;
    XCheckIfEvent( QX11Info::display(), &dummy, update_x_time_predicate, NULL );
    if( next_x_time == CurrentTime )
        {
        XSync( QX11Info::display(), False );
        XCheckIfEvent( QX11Info::display(), &dummy, update_x_time_predicate, NULL );
        }
    Q_ASSERT( next_x_time != CurrentTime );
    QX11Info::setAppTime( next_x_time );
    XEvent ev; // remove the PropertyNotify event from the events queue
    XWindowEvent( QX11Info::display(), w->winId(), PropertyChangeMask, &ev );
}

static const char * const description =
      I18N_NOOP("KDE cut & paste history utility");

void Klipper::createAboutData()
{
  m_about_data = new KAboutData("klipper", 0, ki18n("Klipper"),
    klipper_version, ki18n(description), KAboutData::License_GPL,
		       ki18n("(c) 1998, Andrew Stanley-Jones\n"
		       "1998-2002, Carsten Pfeiffer\n"
		       "2001, Patrick Dubroy"));

  m_about_data->addAuthor(ki18n("Carsten Pfeiffer"),
                      ki18n("Author"),
                      "pfeiffer@kde.org");

  m_about_data->addAuthor(ki18n("Andrew Stanley-Jones"),
                      ki18n( "Original Author" ),
                      "asj@cban.com");

  m_about_data->addAuthor(ki18n("Patrick Dubroy"),
                      ki18n("Contributor"),
                      "patrickdu@corel.com");

  m_about_data->addAuthor( ki18n("Luboš Luňák"),
                      ki18n("Bugfixes and optimizations"),
                      "l.lunak@kde.org");

  m_about_data->addAuthor( ki18n("Esben Mose Hansen"),
                      ki18n("Maintainer"),
                      "kde@mosehansen.dk");
}

void Klipper::destroyAboutData()
{
  delete m_about_data;
  m_about_data = NULL;
}

KAboutData* Klipper::m_about_data;

KAboutData* Klipper::aboutData()
{
  return m_about_data;
}

static void ensureGlobalSyncOff(KSharedConfigPtr config) {
    KConfigGroup cg(config, "General");
    if ( cg.readEntry( "SynchronizeClipboardAndSelection" , false) ) {
        kDebug() << "Shutting off global synchronization";
        cg.writeEntry("SynchronizeClipboardAndSelection", false, KConfig::Normal | KConfig::Global );
        cg.sync();
        kapp->setSynchronizeClipboard(false);
        KGlobalSettings::self()->emitChange( KGlobalSettings::ClipboardConfigChanged, 0 );
    }

}


#include "klipper.moc"
