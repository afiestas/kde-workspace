// KDE Display color scheme setup module
//
// Copyright (c)  Mark Donohoe 1997
//
// Converted to a kcc module by Matthias Hoelzer 1997
// Ported to Qt-2.0 by Matthias Ettrich 1999
// Ported to kcontrol2 by Geert Jansen 1999
// Made maintable by Waldo Bastian 2000

#include <assert.h>
#include <config.h>
#include <stdlib.h>
#include <unistd.h>

#include <qvgroupbox.h>
#include <qcheckbox.h>
#include <qlabel.h>
#include <qpushbutton.h>
#include <qslider.h>
#include <qpixmap.h>
#include <qpainter.h>
#include <qcombobox.h>
#include <klistbox.h>
#include <qlayout.h>
#include <qcursor.h>
#include <qstringlist.h>
#include <qfileinfo.h>
#include <qwhatsthis.h>

#include <kfiledialog.h>
#include <ksimpleconfig.h>
#include <kmessagebox.h>
#include <kcursor.h>
#include <kglobal.h>
#include <kglobalsettings.h>
#include <klineeditdlg.h>
#include <kstandarddirs.h>
#include <kipc.h>
#include <kcolordialog.h>
#include <kcolorbutton.h>
#include <kbuttonbox.h>
#include <kstringhandler.h>
#include <kgenericfactory.h>
#include <kprocess.h>

#include <kio/netaccess.h>

#include <X11/Xlib.h>

#include "../krdb/krdb.h"

#include "colorscm.h"
#include "widgetcanvas.h"


/**** DLL Interface ****/
typedef KGenericFactory<KColorScheme , QWidget> KolorFactory;
K_EXPORT_COMPONENT_FACTORY( kcm_colors, KolorFactory("kcmcolors") );

class KColorSchemeEntry {
public:
    KColorSchemeEntry(const QString &_path, const QString &_name, bool _local)
    	: path(_path), name(_name), local(_local) { }

    QString path;
    QString name;
    bool local;
};

class KColorSchemeList : public QPtrList<KColorSchemeEntry> {
public:
    KColorSchemeList() 
        { setAutoDelete(true); }
    
    int compareItems(QPtrCollection::Item item1, QPtrCollection::Item item2)
        {
           KColorSchemeEntry *i1 = (KColorSchemeEntry*)item1;
           KColorSchemeEntry *i2 = (KColorSchemeEntry*)item2;
           if (i1->local != i2->local)
              return i1->local ? -1 : 1;
           return i1->name.localeAwareCompare(i2->name);
        }
};

#define SIZE 8

// make a 24 * 8 pixmap with the main colors in a scheme
QPixmap mkColorPreview(const WidgetCanvas *cs) 
{
   QPixmap group(SIZE*3,SIZE);
   QPixmap block(SIZE,SIZE);
   group.fill(QColor(0,0,0));
   block.fill(cs->back);   bitBlt(&group,0*SIZE,0,&block,0,0,SIZE,SIZE);
   block.fill(cs->window); bitBlt(&group,1*SIZE,0,&block,0,0,SIZE,SIZE);
   block.fill(cs->aTitle); bitBlt(&group,2*SIZE,0,&block,0,0,SIZE,SIZE);
   QPainter p(&group);
   p.drawRect(0,0,3*SIZE,SIZE);
   return group;
}

/**** KColorScheme ****/

KColorScheme::KColorScheme(QWidget *parent, const char *name, const QStringList &)
    : KCModule(KolorFactory::instance(), parent, name)
{
    m_bChanged = false;
    nSysSchemes = 2;

    KConfig *cfg = new KConfig("kcmdisplayrc");
    cfg->setGroup("X11");
    useRM = cfg->readBoolEntry("useResourceManager", true);
    delete cfg;

    cs = new WidgetCanvas( this );
    cs->setCursor( KCursor::handCursor() );

    // LAYOUT

    QGridLayout *topLayout = new QGridLayout( this, 3, 2, 10 );
    topLayout->setRowStretch(0,0);
    topLayout->setRowStretch(1,0);
    topLayout->setColStretch(0,1);
    topLayout->setColStretch(1,1);

    cs->setFixedHeight(160);
    cs->setMinimumWidth(440);

    QWhatsThis::add( cs, i18n("This is a preview of the color settings which"
       " will be applied if you click \"Apply\" or \"OK\". You can click on"
       " different parts of this preview image. The widget name in the"
       " \"Widget color\" box will change to reflect the part of the preview"
       " image you clicked.") );

    connect( cs, SIGNAL( widgetSelected( int ) ),
         SLOT( slotWidgetColor( int ) ) );
    connect( cs, SIGNAL( colorDropped( int, const QColor&)),
         SLOT( slotColorForWidget( int, const QColor&)));
    topLayout->addMultiCellWidget( cs, 0, 0, 0, 1 );

    QGroupBox *group = new QVGroupBox( i18n("Color Scheme"), this );
    topLayout->addWidget( group, 1, 0 );

    sList = new KListBox( group );
    mSchemeList = new KColorSchemeList();
    readSchemeNames();
    sList->setCurrentItem( 0 );
    connect(sList, SIGNAL(highlighted(int)), SLOT(slotPreviewScheme(int)));

    QWhatsThis::add( sList, i18n("This is a list of predefined color schemes,"
       " including any that you may have created. You can preview an existing"
       " color scheme by selecting it from the list. The current scheme will"
       " be replaced by the selected color scheme.<p>"
       " Warning: if you have not yet applied any changes you may have made"
       " to the current scheme, those changes will be lost if you select"
       " another color scheme.") );

    addBt = new QPushButton(i18n("&Save Scheme..."), group);
    connect(addBt, SIGNAL(clicked()), SLOT(slotAdd()));

    QWhatsThis::add( addBt, i18n("Press this button if you want to save"
       " the current color settings as a color scheme. You will be"
       " prompted for a name.") );

    removeBt = new QPushButton(i18n("&Remove Scheme"), group);
    removeBt->setEnabled(FALSE);
    connect(removeBt, SIGNAL(clicked()), SLOT(slotRemove()));

    QWhatsThis::add( removeBt, i18n("Press this button to remove the selected"
       " color scheme. Note that this button is disabled if you do not have"
       " permission to delete the color scheme.") );

	importBt = new QPushButton(i18n("&Import Scheme..."), group);
	connect(importBt, SIGNAL(clicked()),SLOT(slotImport()));

	QWhatsThis::add( importBt, i18n("Press this button to import a new color"
		" scheme. Note that the color scheme will only be available for the"
		" current user." ));
			
	
    QBoxLayout *stackLayout = new QVBoxLayout;
    topLayout->addLayout(stackLayout, 1, 1);

    group = new QGroupBox(i18n("Widget Color"), this);
    stackLayout->addWidget(group);
    QBoxLayout *groupLayout = new QVBoxLayout(group, 10);
    groupLayout->addSpacing(10);

    wcCombo = new QComboBox(false, group);
    for(int i=0; i < CSM_LAST;i++)
    {
       wcCombo->insertItem(QString::null);
    }

    setColorName(i18n("Inactive title bar") , CSM_Inactive_title_bar);
    setColorName(i18n("Inactive title text"), CSM_Inactive_title_text);
    setColorName(i18n("Inactive title blend"), CSM_Inactive_title_blend);
    setColorName(i18n("Active title bar"), CSM_Active_title_bar);
    setColorName(i18n("Active title text"), CSM_Active_title_text);
    setColorName(i18n("Active title blend"), CSM_Active_title_blend);
    setColorName(i18n("Window background"), CSM_Background);
    setColorName(i18n("Window text"), CSM_Text);
    setColorName(i18n("Selected background"), CSM_Select_background);
    setColorName(i18n("Selected text"), CSM_Select_text);
    setColorName(i18n("Standard Background"), CSM_Standard_background);
    setColorName(i18n("Standard Text"), CSM_Standard_text);
    setColorName(i18n("Button background"), CSM_Button_background);
    setColorName(i18n("Button text"), CSM_Button_text);
    setColorName(i18n("Active title button"), CSM_Active_title_button);
    setColorName(i18n("Inactive title button"), CSM_Inactive_title_button);
    setColorName(i18n("Link"), CSM_Link);
    setColorName(i18n("Followed Link"), CSM_Followed_Link);
    setColorName(i18n("Alternate background in lists"), CSM_Alternate_background);

    wcCombo->adjustSize();
    connect(wcCombo, SIGNAL(activated(int)), SLOT(slotWidgetColor(int)));
    groupLayout->addWidget(wcCombo);

    QWhatsThis::add( wcCombo, i18n("Click here to select an element of"
       " the KDE desktop whose color you want to change. You may either"
       " choose the \"widget\" here, or click on the corresponding part"
       " of the preview image above.") );

    colorButton = new KColorButton( group );
    connect( colorButton, SIGNAL( changed(const QColor &)),
             SLOT(slotSelectColor(const QColor &)));

    groupLayout->addWidget( colorButton );

    QWhatsThis::add( colorButton, i18n("Click here to bring up a dialog"
       " box where you can choose a color for the \"widget\" selected"
       " in the above list.") );

    group = new QGroupBox(  i18n("Contrast"), this );
    stackLayout->addWidget(group);

    QVBoxLayout *groupLayout2 = new QVBoxLayout(group, 10);
    groupLayout2->addSpacing(10);
    groupLayout = new QHBoxLayout;
    groupLayout2->addLayout(groupLayout);

    sb = new QSlider( QSlider::Horizontal,group,"Slider" );
    sb->setRange( 0, 10 );
    sb->setFocusPolicy( QWidget::StrongFocus );
    connect(sb, SIGNAL(valueChanged(int)), SLOT(sliderValueChanged(int)));

    QWhatsThis::add(sb, i18n("Use this slider to change the contrast level"
       " of the current color scheme. Contrast does not affect all of the"
       " colors, only the edges of 3D objects."));

    QLabel *label = new QLabel(sb, i18n("Low Contrast", "Low"), group);
    groupLayout->addWidget(label);
    groupLayout->addWidget(sb, 10);
    label = new QLabel(group);
    label->setText(i18n("High Contrast", "High"));
    groupLayout->addWidget( label );

    cbExportColors = new QCheckBox(i18n("Apply colors to &non-KDE applications"), this);
    topLayout->addMultiCellWidget( cbExportColors, 2, 2, 0, 1 );
    connect(cbExportColors, SIGNAL(toggled(bool)), this, SLOT(slotChanged()));

    load();
}


KColorScheme::~KColorScheme()
{
    delete mSchemeList;
}

void KColorScheme::slotChanged()
{
    m_bChanged = true;
    emit changed(true);
}

void KColorScheme::setColorName( const QString &name, int id )
{
    wcCombo->changeItem(name, id);
    cs->addToolTip( id, name );
}

void KColorScheme::load()
{
    KConfig *config = KGlobal::config();
    config->setGroup("KDE");
    sCurrentScheme = config->readEntry("colorScheme");

    sList->setCurrentItem(findSchemeByName(sCurrentScheme));
    readScheme(0);

    cs->drawSampleWidgets();
    slotWidgetColor(0);
    sb->blockSignals(true);
    sb->setValue(cs->contrast);
    sb->blockSignals(false);

    KConfig cfg("kcmdisplayrc", true, false);
    cfg.setGroup("X11");
    bool exportColors = cfg.readBoolEntry("exportKDEColors", true);
    cbExportColors->setChecked(exportColors);

    m_bChanged = false;
    emit changed(false);
}


void KColorScheme::save()
{
    if (!m_bChanged)
    return;

    KConfig *cfg = KGlobal::config();
    cfg->setGroup( "General" );
    cfg->writeEntry("background", cs->back, true, true);
    cfg->writeEntry("selectBackground", cs->select, true, true);
    cfg->writeEntry("foreground", cs->txt, true, true);
    cfg->writeEntry("windowForeground", cs->windowTxt, true, true);
    cfg->writeEntry("windowBackground", cs->window, true, true);
    cfg->writeEntry("selectForeground", cs->selectTxt, true, true);
    cfg->writeEntry("buttonBackground", cs->button, true, true);
    cfg->writeEntry("buttonForeground", cs->buttonTxt, true, true);
    cfg->writeEntry("linkColor", cs->link, true, true);
    cfg->writeEntry("visitedLinkColor", cs->visitedLink, true, true);
    cfg->writeEntry("alternateBackground", cs->alternateBackground, true, true);

    cfg->setGroup( "WM" );
    cfg->writeEntry("activeForeground", cs->aTxt, true, true);
    cfg->writeEntry("inactiveBackground", cs->iaTitle, true, true);
    cfg->writeEntry("inactiveBlend", cs->iaBlend, true, true);
    cfg->writeEntry("activeBackground", cs->aTitle, true, true);
    cfg->writeEntry("activeBlend", cs->aBlend, true, true);
    cfg->writeEntry("inactiveForeground", cs->iaTxt, true, true);
    cfg->writeEntry("activeTitleBtnBg", cs->aTitleBtn, true, true);
    cfg->writeEntry("inactiveTitleBtnBg", cs->iTitleBtn, true, true);

    cfg->setGroup( "KDE" );
    cfg->writeEntry("contrast", cs->contrast, true, true);
    cfg->writeEntry("colorScheme", sCurrentScheme, true, true);
    cfg->sync();

    // KDE-1.x support
    KSimpleConfig *config =
    new KSimpleConfig( QCString(::getenv("HOME")) + "/.kderc" );
    config->setGroup( "General" );
    config->writeEntry("background", cs->back );
    config->writeEntry("selectBackground", cs->select );
    config->writeEntry("foreground", cs->txt, true, true);
    config->writeEntry("windowForeground", cs->windowTxt );
    config->writeEntry("windowBackground", cs->window );
    config->writeEntry("selectForeground", cs->selectTxt );
    config->sync();
    delete config;

    KConfig cfg2("kcmdisplayrc", false, false);
    cfg2.setGroup("X11");
    bool exportColors = cbExportColors->isChecked();
    cfg2.writeEntry("exportKDEColors", exportColors);
    cfg2.sync();
    QApplication::syncX();

    // Notify all qt-only apps of the KDE palette changes
    uint flags = KRdbExportQtColors;
    if ( exportColors )
        flags |= KRdbExportColors;
    else
    {
        // Undo the property xrdb has placed on the root window (if any),
        // i.e. remove all entries, including ours
        Atom resource_manager;
        resource_manager = XInternAtom( qt_xdisplay(), "RESOURCE_MANAGER", True);
        if (resource_manager != None)
          XDeleteProperty( qt_xdisplay(), qt_xrootwin(), resource_manager);
        // and run xrdb with ~/.Xdefaults at least to get non-KDE defaults
        KProcess proc;
        proc << "xrdb" << ( QDir::homeDirPath() + "/.Xdefaults" );
        proc.start( KProcess::Block, KProcess::Stdin );
    }
    runRdb( flags );	// Save the palette to qtrc for KStyles

    // Notify all KDE applications
    KIPC::sendMessageAll(KIPC::PaletteChanged);

    m_bChanged = false;
    emit changed(false);
}


void KColorScheme::defaults()
{
    readScheme(1);
    sList->setCurrentItem(1);

    cs->drawSampleWidgets();
    slotWidgetColor(0);
    sb->blockSignals(true);
    sb->setValue(cs->contrast);
    sb->blockSignals(false);

    cbExportColors->setChecked(true);

    m_bChanged = true;
    emit changed(true);
}

QString KColorScheme::quickHelp() const
{
    return i18n("<h1>Colors</h1> This module allows you to choose"
       " the color scheme used for the KDE desktop. The different"
       " elements of the desktop, such as title bars, menu text, etc.,"
       " are called \"widgets\". You can choose the widget whose"
       " color you want to change by selecting it from a list, or by"
       " clicking on a graphical representation of the desktop.<p>"
       " You can save color settings as complete color schemes,"
       " which can also be modified or deleted. KDE comes with several"
       " predefined color schemes on which you can base your own.<p>"
       " All KDE applications will obey the selected color scheme."
       " Non-KDE applications may also obey some or all of the color"
       " settings. See the \"Style\" control module for more details.");
}


void KColorScheme::sliderValueChanged( int val )
{
    cs->contrast = val;
    cs->drawSampleWidgets();

    sCurrentScheme = QString::null;

    m_bChanged = true;
    emit changed(true);
}


void KColorScheme::slotSave( )
{
    KColorSchemeEntry *entry = mSchemeList->at(sList->currentItem()-nSysSchemes);
    if (!entry) return;
    sCurrentScheme = entry->path;
    KSimpleConfig *config = new KSimpleConfig(sCurrentScheme );
    int i = sCurrentScheme.findRev('/');
    if (i >= 0)
      sCurrentScheme = sCurrentScheme.mid(i+1);

    config->setGroup("Color Scheme" );
    config->writeEntry("background", cs->back );
    config->writeEntry("selectBackground", cs->select );
    config->writeEntry("foreground", cs->txt );
    config->writeEntry("activeForeground", cs->aTxt );
    config->writeEntry("inactiveBackground", cs->iaTitle );
    config->writeEntry("inactiveBlend", cs->iaBlend );
    config->writeEntry("activeBackground", cs->aTitle );
    config->writeEntry("activeBlend", cs->aBlend );
    config->writeEntry("inactiveForeground", cs->iaTxt );
    config->writeEntry("windowForeground", cs->windowTxt );
    config->writeEntry("windowBackground", cs->window );
    config->writeEntry("selectForeground", cs->selectTxt );
    config->writeEntry("contrast", cs->contrast );
    config->writeEntry("buttonForeground", cs->buttonTxt );
    config->writeEntry("buttonBackground", cs->button );
    config->writeEntry("activeTitleBtnBg", cs->aTitleBtn);
    config->writeEntry("inactiveTitleBtnBg", cs->iTitleBtn);
    config->writeEntry("linkColor", cs->link);
    config->writeEntry("visitedLinkColor", cs->visitedLink);
    config->writeEntry("alternateBackground", cs->alternateBackground);

    delete config;
}


void KColorScheme::slotRemove()
{
    uint ind = sList->currentItem();
    KColorSchemeEntry *entry = mSchemeList->at(ind-nSysSchemes);
    if (!entry) return;

    if (unlink(QFile::encodeName(entry->path).data())) {
        KMessageBox::error( 0,
          i18n("This color scheme could not be removed.\n"
           "Perhaps you do not have permission to alter the file"
           "system where the color scheme is stored." ));
        return;
    }

    sList->removeItem(ind);
    mSchemeList->remove(entry);
}


/*
 * Add a local color scheme.
 */
void KColorScheme::slotAdd()
{
    QString sName;
    if (sList->currentItem() >= nSysSchemes)
       sName = sList->currentText();

    KLineEditDlg dlg(i18n("Enter a name for the color scheme:"), sName, this);
    dlg.setCaption(i18n("Save Color Scheme"));

    QString sFile;

    bool valid = false;
    int exists = -1;

    while (!valid)
    {
        if (!dlg.exec())
            return;

        sName = dlg.text();
        sName = sName.simplifyWhiteSpace();
        if (sName.isEmpty())
            return;
        sFile = sName;

        int i = 0;

        // Capitalise each word
		sName = KStringHandler::capwords(sName);

        exists = -1;
        // Check if it's already there
        for (i=0; i < (int) sList->count(); i++)
        {
            if (sName == sList->text(i))
            {
                exists = i;
                int result = KMessageBox::warningContinueCancel( 0,
                   i18n("A color scheme with the name '%1' already exists.\n"
                        "Do you want to overwrite it?\n").arg(sName),
		   i18n("Save Color Scheme"),
                   i18n("Overwrite"));
                if (result == KMessageBox::Cancel)
                    break;
            }
        }
        if (i == (int) sList->count())
            valid = true;
    }

    disconnect(sList, SIGNAL(highlighted(int)), this,
        SLOT(slotPreviewScheme(int)));

    if (exists != -1)
    {
       sList->setFocus();
       sList->setCurrentItem(exists);
    }
    else
    {
       sFile = KGlobal::dirs()->saveLocation("data", "kdisplay/color-schemes/") + sFile + ".kcsrc";
       KSimpleConfig *config = new KSimpleConfig(sFile);
       config->setGroup( "Color Scheme");
       config->writeEntry("Name", sName);
       delete config;

	   insertEntry(sFile, sName);
       
   }
    slotSave();

    QPixmap preview = mkColorPreview(cs);
    int current = sList->currentItem();
    sList->changeItem(preview, sList->text(current), current);
    connect(sList, SIGNAL(highlighted(int)), SLOT(slotPreviewScheme(int)));
    slotPreviewScheme(current);
}

void KColorScheme::slotImport()
{
	QString location = locateLocal( "data", "kdisplay/color-schemes/" );

	KURL file = KFileDialog::getOpenFileName(QString::null, "*.kcsrc", this);
	if ( file.isEmpty() )
		return;

	//kdDebug() << "Location: " << location << endl;
	if (!KIO::NetAccess::copy(file, location+file.fileName( false )  ) )
	{
		KMessageBox::error(this, KIO::NetAccess::lastErrorString(),i18n("Import failed!"));
		return;
	}
	else
	{
		QString sFile = location + file.fileName( false );
		KSimpleConfig *config = new KSimpleConfig(sFile);
		config->setGroup( "Color Scheme");
		QString sName = config->readEntry("Name", i18n("Untitled Theme"));
		delete config;


		insertEntry(sFile, sName);
		QPixmap preview = mkColorPreview(cs);
		int current = sList->currentItem();
		sList->changeItem(preview, sList->text(current), current);
		connect(sList, SIGNAL(highlighted(int)), SLOT(slotPreviewScheme(int)));
		slotPreviewScheme(current);
	}
}

QColor &KColorScheme::color(int index)
{
    switch(index) {
    case CSM_Inactive_title_bar:
    return cs->iaTitle;
    case CSM_Inactive_title_text:
    return cs->iaTxt;
    case CSM_Inactive_title_blend:
    return cs->iaBlend;
    case CSM_Active_title_bar:
    return cs->aTitle;
    case CSM_Active_title_text:
    return cs->aTxt;
    case CSM_Active_title_blend:
    return cs->aBlend;
    case CSM_Background:
    return cs->back;
    case CSM_Text:
    return cs->txt;
    case CSM_Select_background:
    return cs->select;
    case CSM_Select_text:
    return cs->selectTxt;
    case CSM_Standard_background:
    return cs->window;
    case CSM_Standard_text:
    return cs->windowTxt;
    case CSM_Button_background:
    return cs->button;
    case CSM_Button_text:
    return cs->buttonTxt;
    case CSM_Active_title_button:
    return cs->aTitleBtn;
    case CSM_Inactive_title_button:
    return cs->iTitleBtn;
    case CSM_Link:
    return cs->link;
    case CSM_Followed_Link:
    return cs->visitedLink;
    case CSM_Alternate_background:
    return cs->alternateBackground;
    }

    assert(0); // Should never be here!
    return cs->iaTxt; // Silence compiler
}


void KColorScheme::slotSelectColor(const QColor &col)
{
    int selection;
    selection = wcCombo->currentItem();

    // Adjust the alternate background color if the standard color changes
    // Only if the previous alternate color was not a user-configured one
    // of course
    if ( selection == CSM_Standard_background &&
         color(CSM_Alternate_background) ==
         KGlobalSettings::calculateAlternateBackgroundColor(
             color(CSM_Standard_background) ) ) 
    {
        color(CSM_Alternate_background) = 
            KGlobalSettings::calculateAlternateBackgroundColor( col );
    }
        
    color(selection) = col;

    cs->drawSampleWidgets();

    sCurrentScheme = QString::null;

    m_bChanged = true;
    emit changed(true);
}


void KColorScheme::slotWidgetColor(int indx)
{
    if (wcCombo->currentItem() != indx)
    wcCombo->setCurrentItem( indx );

    QColor col = color(indx);
    colorButton->setColor( col );
    colorPushColor = col;
}


void KColorScheme::slotColorForWidget(int indx, const QColor& col)
{
    if (wcCombo->currentItem() != indx)
    wcCombo->setCurrentItem( indx );

    slotSelectColor(col);
}


/*
 * Read a color scheme into "cs".
 *
 * KEEP IN SYNC with thememgr!
 */
void KColorScheme::readScheme( int index )
{
    KConfigBase* config;

    // define some KDE2 default colors
    QColor kde2Blue;
    if (QPixmap::defaultDepth() > 8)
      kde2Blue.setRgb(10, 95, 137);
    else
      kde2Blue.setRgb(0, 0, 192);

    QColor widget(220, 220, 220);

    QColor button;
    if (QPixmap::defaultDepth() > 8)
      button.setRgb(228, 228, 228);
    else
      button.setRgb(220, 220, 220);

    QColor link(0, 0, 192);
    QColor visitedLink(128, 0,128);

    // note: keep default color scheme in sync with default Current Scheme
    if (index == 1) {
      sCurrentScheme  = "<default>";
      cs->back        = widget;
      cs->txt         = black;
      cs->select      = kde2Blue;
      cs->selectTxt   = white;
      cs->window      = white;
      cs->windowTxt   = black;
      cs->iaTitle     = widget;
      cs->iaTxt       = black;
      cs->iaBlend     = widget;
      cs->aTitle      = kde2Blue;
      cs->aTxt        = white;
      cs->aBlend      = kde2Blue;
      cs->button      = button;
      cs->buttonTxt   = black;
      cs->aTitleBtn   = cs->back;
      cs->iTitleBtn   = cs->back;
      cs->link        = link;
      cs->visitedLink = visitedLink;
      cs->alternateBackground = KGlobalSettings::calculateAlternateBackgroundColor(cs->window);

      cs->contrast    = 7;

      return;
    }

    if (index == 0) {
      // Current scheme
      config = KGlobal::config();
      config->setGroup("General");
    } else {
      // Open scheme file
      KColorSchemeEntry *entry = mSchemeList->at(sList->currentItem()-nSysSchemes);
      if (!entry) return;
      sCurrentScheme = entry->path;
      config = new KSimpleConfig(sCurrentScheme, true);
      config->setGroup("Color Scheme");
      int i = sCurrentScheme.findRev('/');
      if (i >= 0)
        sCurrentScheme = sCurrentScheme.mid(i+1);
    }

    // note: defaults should be the same as the KDE default
    cs->txt = config->readColorEntry( "foreground", &black );
    cs->back = config->readColorEntry( "background", &widget );
    cs->select = config->readColorEntry( "selectBackground", &kde2Blue );
    cs->selectTxt = config->readColorEntry( "selectForeground", &white );
    cs->window = config->readColorEntry( "windowBackground", &white );
    cs->windowTxt = config->readColorEntry( "windowForeground", &black );
    cs->button = config->readColorEntry( "buttonBackground", &button );
    cs->buttonTxt = config->readColorEntry( "buttonForeground", &black );
    cs->link = config->readColorEntry( "linkColor", &link );
    cs->visitedLink = config->readColorEntry( "visitedLinkColor", &visitedLink );
    QColor alternate = KGlobalSettings::calculateAlternateBackgroundColor(cs->window);
    cs->alternateBackground = config->readColorEntry( "alternateBackground", &alternate );

    if (index == 0)
      config->setGroup( "WM" );

    cs->iaTitle = config->readColorEntry("inactiveBackground", &widget);
    cs->iaTxt = config->readColorEntry("inactiveForeground", &black);
    cs->iaBlend = config->readColorEntry("inactiveBlend", &widget);
    cs->aTitle = config->readColorEntry("activeBackground", &kde2Blue);
    cs->aTxt = config->readColorEntry("activeForeground", &white);
    cs->aBlend = config->readColorEntry("activeBlend", &kde2Blue);
    // hack - this is all going away. For now just set all to button bg
    cs->aTitleBtn = config->readColorEntry("activeTitleBtnBg", &cs->back);
    cs->iTitleBtn = config->readColorEntry("inactiveTitleBtnBg", &cs->back);

    if (index == 0)
      config->setGroup( "KDE" );

    cs->contrast = config->readNumEntry( "contrast", 7 );
    if (index != 0)
      delete config;
}


/*
 * Get all installed color schemes.
 */
void KColorScheme::readSchemeNames()
{
    mSchemeList->clear();
    sList->clear();
    // Always a current and a default scheme
    sList->insertItem( i18n("Current Scheme"), 0 );
    sList->insertItem( i18n("KDE Default"), 1 );
    nSysSchemes = 2;

    // Global + local schemes
    QStringList list = KGlobal::dirs()->findAllResources("data",
            "kdisplay/color-schemes/*.kcsrc", false, true);

    // And add them
    for (QStringList::ConstIterator it = list.begin(); it != list.end(); it++) {
       KSimpleConfig *config = new KSimpleConfig(*it);
       config->setGroup("Color Scheme");
       QString str = config->readEntry("Name");
       if (str.isEmpty()) {
          str =  config->readEntry("name");
          if (str.isEmpty())
             continue;
       }
       mSchemeList->append(new KColorSchemeEntry(*it, str, !config->isImmutable()));
       delete config;
    }

    mSchemeList->sort();

    for(KColorSchemeEntry *entry = mSchemeList->first(); entry; entry = mSchemeList->next())
    {
       sList->insertItem(entry->name);
    }

    for (uint i = 0; i < (nSysSchemes + mSchemeList->count()); i++)
    {
       sList->setCurrentItem(i);
       readScheme(i);
       QPixmap preview = mkColorPreview(cs);
       sList->changeItem(preview, sList->text(i), i);
    }

}

/*
 * Find scheme based on filename
 */
int KColorScheme::findSchemeByName(const QString &scheme)
{
   if (scheme.isEmpty())
      return 0;
   if (scheme == "<default>")
      return 1;

   QString search = scheme;
   int i = search.findRev('/');
   if (i >= 0)
      search = search.mid(i+1);

   i = 0;
   
   for(KColorSchemeEntry *entry = mSchemeList->first(); entry; entry = mSchemeList->next())
   {
      if (entry->path.endsWith(search))
         return i+nSysSchemes;
      i++;
   }

   return 0;
}


void KColorScheme::slotPreviewScheme(int indx)
{
    readScheme(indx);

    // Set various appropriate for the scheme

    cs->drawSampleWidgets();
    sb->blockSignals(true);
    sb->setValue(cs->contrast);
    sb->blockSignals(false);
    slotWidgetColor(0);
    if (indx < nSysSchemes)
       removeBt->setEnabled(false);
    else
    {
       KColorSchemeEntry *entry = mSchemeList->at(indx-nSysSchemes);
       removeBt->setEnabled(entry ? entry->local : false);
    }

    m_bChanged = (indx != 0);
    emit changed(m_bChanged);
}


/* this function should dissappear: colorscm should work directly on a Qt palette, since
   this will give us much more cusomization with qt-2.0.
   */
QPalette KColorScheme::createPalette()
{
    QColorGroup disabledgrp(cs->windowTxt, cs->back, cs->back.light(150),
                cs->back.dark(), cs->back.dark(120), cs->back.dark(120),
                cs->window);

    QColorGroup colgrp(cs->windowTxt, cs->back, cs->back.light(150),
               cs->back.dark(), cs->back.dark(120), cs->txt, cs->window);

    colgrp.setColor(QColorGroup::Highlight, cs->select);
    colgrp.setColor(QColorGroup::HighlightedText, cs->selectTxt);
    colgrp.setColor(QColorGroup::Button, cs->button);
    colgrp.setColor(QColorGroup::ButtonText, cs->buttonTxt);
    return QPalette( colgrp, disabledgrp, colgrp);
}

void KColorScheme::insertEntry(const QString &sFile, const QString &sName)
{
       KColorSchemeEntry *newEntry = new KColorSchemeEntry(sFile, sName, true);
       mSchemeList->inSort(newEntry);
       int newIndex = mSchemeList->findRef(newEntry)+nSysSchemes;
       sList->insertItem(sName, newIndex);
       sList->setCurrentItem(newIndex);
}

#include "colorscm.moc"
