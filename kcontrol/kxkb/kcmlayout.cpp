#include <qlayout.h>
#include <qlabel.h>
#include <qcombobox.h>
#include <qtabwidget.h>
#include <QGroupBox>
#include <qpushbutton.h>
#include <Q3ListView>
#include <Q3ListViewItem>
#include <Q3CheckListItem>
#include <QHeaderView>
#include <qwhatsthis.h>
#include <qcheckbox.h>
#include <qradiobutton.h>
#include <qlineedit.h>
#include <qlistview.h>
#include <qbuttongroup.h>
#include <qspinbox.h>
#include <QWidget>
#include <QtGui>

#include <kkeydialog.h>
#include <kglobal.h>
#include <kconfig.h>
#include <klocale.h>
#include <kstandarddirs.h>
#include <kdebug.h>
#include <kapplication.h>
#include <kiconloader.h>
#include <ktoolinvocation.h>
#include <kgenericfactory.h>

#include "extension.h"
#include "kxkbconfig.h"
#include "rules.h"
#include "pixmap.h"
#include "kcmmisc.h"
#include "ui_kcmlayoutwidget.h"

#include "kcmlayout.h"
#include "kcmlayout.moc"


static inline QString i18n(const QString& str) { return i18n( str.toLatin1().constData() ); }

enum {
 LAYOUT_COLUMN_FLAG = 0,
 LAYOUT_COLUMN_NAME = 1,
 LAYOUT_COLUMN_MAP = 2,
 LAYOUT_COLUMN_VARIANT = 3,
 LAYOUT_COLUMN_INCLUDE = 4,
 LAYOUT_COLUMN_DISPLAY_NAME = 5,
 SRC_LAYOUT_COLUMN_COUNT = 3,
 DST_LAYOUT_COLUMN_COUNT = 6
};

static const QString DEFAULT_VARIANT_NAME("<default>");


class OptionListItem : public Q3CheckListItem
{
	public:

		OptionListItem(  OptionListItem *parent, const QString &text, Type tt,
						 const QString &optionName );
		OptionListItem(  Q3ListView *parent, const QString &text, Type tt,
						 const QString &optionName );
		~OptionListItem() {}

		QString optionName() const { return m_OptionName; }

		OptionListItem *findChildItem(  const QString& text );

	protected:
		QString m_OptionName;
};


static QString lookupLocalized(const QHash<QString, QString> &dict, const QString& text)
{
  QHashIterator<QString, QString> it(dict);
  while (it.hasNext())
  {
	  it.next();
		if ( i18n( it.value() ) == text )
        return it.key();
    }

  return QString();
}

static Q3ListViewItem* copyLVI(const Q3ListViewItem* src, Q3ListView* parent)
{
    Q3ListViewItem* ret = new Q3ListViewItem(parent);
	for(int i = 0; i < SRC_LAYOUT_COLUMN_COUNT; i++)
    {
        ret->setText(i, src->text(i));
        if ( src->pixmap(i) )
            ret->setPixmap(i, *src->pixmap(i));
    }

    return ret;
}

typedef KGenericFactory<LayoutConfig> LayoutConfigFactory;
K_EXPORT_COMPONENT_FACTORY(keyboard_layout, LayoutConfigFactory("kcmlayout"))

LayoutConfig::LayoutConfig(QWidget *parent, const QStringList &)
  : KCModule(LayoutConfigFactory::componentData(), parent),
    m_rules(NULL)
{
 // QVBoxLayout *main = new QVBoxLayout(this, 0, KDialog::spacingHint());

  widget = new Ui_LayoutConfigWidget();
  widget->setupUi(this);
//  main->addWidget(widget);

  connect( widget->chkEnable, SIGNAL( toggled( bool )), this, SLOT(changed()));
  connect( widget->chkShowSingle, SIGNAL( toggled( bool )), this, SLOT(changed()));
  connect( widget->chkShowFlag, SIGNAL( toggled( bool )), this, SLOT(changed()));
  connect( widget->comboModel, SIGNAL(activated(int)), this, SLOT(changed()));

  connect( widget->listLayoutsSrc, SIGNAL(doubleClicked(Q3ListViewItem*,const QPoint&, int)),
									this, SLOT(add()));
  connect( widget->btnAdd, SIGNAL(clicked()), this, SLOT(add()));
  connect( widget->btnRemove, SIGNAL(clicked()), this, SLOT(remove()));

  connect( widget->comboVariant, SIGNAL(activated(int)), this, SLOT(changed()));
  connect( widget->comboVariant, SIGNAL(activated(int)), this, SLOT(variantChanged()));
  connect( widget->listLayoutsDst, SIGNAL(selectionChanged(Q3ListViewItem *)),
		this, SLOT(layoutSelChanged(Q3ListViewItem *)));

  connect( widget->editDisplayName, SIGNAL(textChanged(const QString&)), this, SLOT(displayNameChanged(const QString&)));

  connect( widget->chkLatin, SIGNAL(clicked()), this, SLOT(changed()));
  connect( widget->chkLatin, SIGNAL(clicked()), this, SLOT(latinChanged()));

  widget->btnUp->setIconSet(KIcon("arrow-up"));
  connect( widget->btnUp, SIGNAL(clicked()), this, SLOT(changed()));
  connect( widget->btnUp, SIGNAL(clicked()), this, SLOT(moveUp()));
  widget->btnDown->setIconSet(KIcon("arrow-down"));
  connect( widget->btnDown, SIGNAL(clicked()), this, SLOT(changed()));
  connect( widget->btnDown, SIGNAL(clicked()), this, SLOT(moveDown()));

  connect( widget->grpSwitching, SIGNAL( clicked( int ) ), SLOT(changed()));

  connect( widget->chkEnableSticky, SIGNAL(toggled(bool)), this, SLOT(changed()));
  connect( widget->spinStickyDepth, SIGNAL(valueChanged(int)), this, SLOT(changed()));

  widget->listLayoutsSrc->setColumnText(LAYOUT_COLUMN_FLAG, "");
  widget->listLayoutsDst->setColumnText(LAYOUT_COLUMN_FLAG, "");
  widget->listLayoutsDst->setColumnText(LAYOUT_COLUMN_INCLUDE, "");
//  widget->listLayoutsDst->setColumnText(LAYOUT_COLUMN_DISPLAY_NAME, "");

  widget->listLayoutsSrc->setColumnWidth(LAYOUT_COLUMN_FLAG, 28);
  widget->listLayoutsDst->setColumnWidth(LAYOUT_COLUMN_FLAG, 28);

  widget->listLayoutsDst->header()->setResizeEnabled(false, LAYOUT_COLUMN_INCLUDE);
  widget->listLayoutsDst->header()->setResizeEnabled(false, LAYOUT_COLUMN_DISPLAY_NAME);
  widget->listLayoutsDst->setColumnWidthMode(LAYOUT_COLUMN_INCLUDE, Q3ListView::Manual);
  widget->listLayoutsDst->setColumnWidth(LAYOUT_COLUMN_INCLUDE, 0);
//  widget->listLayoutsDst->setColumnWidth(LAYOUT_COLUMN_DISPLAY_NAME, 0);

  widget->listLayoutsDst->setSorting(-1);
#if 0
  widget->listLayoutsDst->setResizeMode(QListView::LastColumn);
  widget->listLayoutsSrc->setResizeMode(QListView::LastColumn);
#endif
  widget->listLayoutsDst->setResizeMode(Q3ListView::LastColumn);

  //Read rules - we _must_ read _before_ creating xkb-options comboboxes
  loadRules();

  makeOptionsTab();

  load();
}


LayoutConfig::~LayoutConfig()
{
	delete m_rules;
}


void LayoutConfig::load()
{
	m_kxkbConfig.load(KxkbConfig::LOAD_ALL);

	initUI();
}

void LayoutConfig::initUI() {
	QString modelName = m_rules->models()[m_kxkbConfig.m_model];
	if( modelName.isEmpty() )
		modelName = DEFAULT_MODEL;

	widget->comboModel->setCurrentText(i18n(modelName));

	QList<LayoutUnit> otherLayouts = m_kxkbConfig.m_layouts;
	widget->listLayoutsDst->clear();
// to optimize we should have gone from it.end to it.begin
	for (QListIterator<LayoutUnit> it(otherLayouts); it.hasNext();  ) {
		LayoutUnit layoutUnit = it.next();

		Q3ListViewItemIterator src_it( widget->listLayoutsSrc );

		for ( ; src_it.current(); ++src_it) {
			Q3ListViewItem* srcItem = src_it.current();

			if ( layoutUnit.layout == srcItem->text(LAYOUT_COLUMN_MAP) ) {	// check if current config knows about this layout
				Q3ListViewItem* newItem = copyLVI(srcItem, widget->listLayoutsDst);

				newItem->setText(LAYOUT_COLUMN_VARIANT, layoutUnit.variant);
				newItem->setText(LAYOUT_COLUMN_INCLUDE, layoutUnit.includeGroup);
				newItem->setText(LAYOUT_COLUMN_DISPLAY_NAME, layoutUnit.displayName);
				widget->listLayoutsDst->insertItem(newItem);
				newItem->moveItem(widget->listLayoutsDst->lastItem());

				break;
			}
		}
	}

	// display KXKB switching options
	widget->chkShowSingle->setChecked(m_kxkbConfig.m_showSingle);
	widget->chkShowFlag->setChecked(m_kxkbConfig.m_showFlag);

	widget->chkEnableOptions->setChecked( m_kxkbConfig.m_enableXkbOptions );
	widget->checkResetOld->setChecked(m_kxkbConfig.m_resetOldOptions);

	switch( m_kxkbConfig.m_switchingPolicy ) {
		default:
		case SWITCH_POLICY_GLOBAL:
			widget->grpSwitching->setButton(0);
			break;
		case SWITCH_POLICY_WIN_CLASS:
			widget->grpSwitching->setButton(1);
			break;
		case SWITCH_POLICY_WINDOW:
			widget->grpSwitching->setButton(2);
			break;
	}

	widget->chkEnableSticky->setChecked(m_kxkbConfig.m_stickySwitching);
	widget->spinStickyDepth->setEnabled(m_kxkbConfig.m_stickySwitching);
	widget->spinStickyDepth->setValue( m_kxkbConfig.m_stickySwitchingDepth);

	updateStickyLimit();

	widget->chkEnable->setChecked( m_kxkbConfig.m_useKxkb );
	widget->grpLayouts->setEnabled( m_kxkbConfig.m_useKxkb );
	widget->optionsFrame->setEnabled( m_kxkbConfig.m_useKxkb );

	// display xkb options
	QStringList options = m_kxkbConfig.m_options.split(',');
	for (QListIterator<QString> it(options); it.hasNext(); )
	{
		QString optionName = it.next();
		if( optionName.trimmed().isEmpty() ) {
			kWarning() << "skipping empty option name" << endl;
  			continue;
		}

		const XkbOption& option = m_rules->options()[optionName];
		OptionListItem *item = m_optionGroups[ option.group->name ];

		if (item != NULL) {
			OptionListItem *child = item->findChildItem( option.name );

			if ( child )
				child->setState( Q3CheckListItem::On );
			else
				kDebug() << "load: Unknown option: " << optionName << endl;
		}
		else {
			kDebug() << "load: Unknown option group: " << option.group->name << " of " << optionName << endl;
		}
	}

	updateOptionsCommand();
	emit KCModule::changed( false );
}


void LayoutConfig::save()
{
	QString model = lookupLocalized(m_rules->models(), widget->comboModel->currentText());
	m_kxkbConfig.m_model = model;

	m_kxkbConfig.m_enableXkbOptions = widget->chkEnableOptions->isChecked();
	m_kxkbConfig.m_resetOldOptions = widget->checkResetOld->isChecked();
	m_kxkbConfig.m_options = createOptionString();

	Q3ListViewItem *item = widget->listLayoutsDst->firstChild();
	QList<LayoutUnit> layouts;
	while (item) {
		QString layout = item->text(LAYOUT_COLUMN_MAP);
		QString variant = item->text(LAYOUT_COLUMN_VARIANT);
		QString includes = item->text(LAYOUT_COLUMN_INCLUDE);
		QString displayName = item->text(LAYOUT_COLUMN_DISPLAY_NAME);

		LayoutUnit layoutUnit(layout, variant);
		layoutUnit.includeGroup = includes;
		layoutUnit.displayName = displayName;
		layouts.append( layoutUnit );

		item = item->nextSibling();
		kDebug() << "To save: layout " << layoutUnit.toPair()
				<< ", inc: " << layoutUnit.includeGroup
				<< ", disp: " << layoutUnit.displayName << endl;
	}
	m_kxkbConfig.m_layouts = layouts;

	if( m_kxkbConfig.m_layouts.count() == 0 ) {
		m_kxkbConfig.m_layouts.append(LayoutUnit(DEFAULT_LAYOUT_UNIT));
 		widget->chkEnable->setChecked(false);
 	}

	m_kxkbConfig.m_useKxkb = widget->chkEnable->isChecked();
	m_kxkbConfig.m_showSingle = widget->chkShowSingle->isChecked();
	m_kxkbConfig.m_showFlag = widget->chkShowFlag->isChecked();

	int modeId = widget->grpSwitching->id(widget->grpSwitching->selected());
	switch( modeId ) {
		default:
		case 0:
			m_kxkbConfig.m_switchingPolicy = SWITCH_POLICY_GLOBAL;
			break;
		case 1:
			m_kxkbConfig.m_switchingPolicy = SWITCH_POLICY_WIN_CLASS;
			break;
		case 2:
			m_kxkbConfig.m_switchingPolicy = SWITCH_POLICY_WINDOW;
			break;
	}

	m_kxkbConfig.m_stickySwitching = widget->chkEnableSticky->isChecked();
	m_kxkbConfig.m_stickySwitchingDepth = widget->spinStickyDepth->value();

	m_kxkbConfig.save();

	KToolInvocation::kdeinitExec("kxkb");
	emit KCModule::changed( false );
}


void LayoutConfig::updateStickyLimit()
{
    int layoutsCnt = widget->listLayoutsDst->childCount();
	int maxDepth = layoutsCnt - 1;

	if( maxDepth < 2 ) {
		maxDepth = 2;
	}

	widget->spinStickyDepth->setMaximum(maxDepth);
/*	if( value > maxDepth )
		setValue(maxDepth);*/
}

void LayoutConfig::add()
{
    Q3ListViewItem* sel = widget->listLayoutsSrc->selectedItem();
    if( sel == 0 )
		return;

    // Create a copy of the sel widget, as one might add the same layout more
    // than one time, with different variants.
    Q3ListViewItem* toadd = copyLVI(sel, widget->listLayoutsDst);

    widget->listLayoutsDst->insertItem(toadd);
    if( widget->listLayoutsDst->childCount() > 1 )
		toadd->moveItem(widget->listLayoutsDst->lastItem());
// disabling temporary: does not work reliable in Qt :(
//    widget->listLayoutsDst->setSelected(sel, true);
//    layoutSelChanged(sel);

    updateStickyLimit();
    changed();
}

void LayoutConfig::remove()
{
    Q3ListViewItem* sel = widget->listLayoutsDst->selectedItem();
    Q3ListViewItem* newSel = 0;

    if( sel == 0 )
        return;

    if( sel->itemBelow() )
        newSel = sel->itemBelow();
    else
        if( sel->itemAbove() )
            newSel = sel->itemAbove();

    delete sel;
    if( newSel )
        widget->listLayoutsSrc->setSelected(newSel, true);
    layoutSelChanged(newSel);

    updateStickyLimit();
    changed();
}

void LayoutConfig::moveUp()
{
    Q3ListViewItem* sel = widget->listLayoutsDst->selectedItem();
    if( sel == 0 || sel->itemAbove() == 0 )
		return;

    if( sel->itemAbove()->itemAbove() == 0 ) {
		widget->listLayoutsDst->takeItem(sel);
		widget->listLayoutsDst->insertItem(sel);
		widget->listLayoutsDst->setSelected(sel, true);
    }
    else
		sel->moveItem(sel->itemAbove()->itemAbove());
}

void LayoutConfig::moveDown()
{
    Q3ListViewItem* sel = widget->listLayoutsDst->selectedItem();
    if( sel == 0 || sel->itemBelow() == 0 )
	return;

    sel->moveItem(sel->itemBelow());
}

void LayoutConfig::variantChanged()
{
    Q3ListViewItem* selLayout = widget->listLayoutsDst->selectedItem();
    if( selLayout == NULL ) {
      widget->comboVariant->clear();
      widget->comboVariant->setEnabled(false);
      return;
    }

	QString selectedVariant = widget->comboVariant->currentText();
	if( selectedVariant == DEFAULT_VARIANT_NAME )
		selectedVariant = "";
	selLayout->setText(LAYOUT_COLUMN_VARIANT, selectedVariant);
}

// helper
LayoutUnit LayoutConfig::getLayoutUnitKey(Q3ListViewItem *sel)
{
	QString kbdLayout = sel->text(LAYOUT_COLUMN_MAP);
	QString kbdVariant = sel->text(LAYOUT_COLUMN_VARIANT);
	return LayoutUnit(kbdLayout, kbdVariant);
}

void LayoutConfig::displayNameChanged(const QString& newDisplayName)
{
	Q3ListViewItem* selLayout = widget->listLayoutsDst->selectedItem();
	if( selLayout == NULL )
		return;

	const LayoutUnit layoutUnitKey = getLayoutUnitKey( selLayout );
	LayoutUnit& layoutUnit = *m_kxkbConfig.m_layouts.find(layoutUnitKey);

	QString oldName = selLayout->text(LAYOUT_COLUMN_DISPLAY_NAME);

	if( oldName.isEmpty() )
		oldName = KxkbConfig::getDefaultDisplayName( layoutUnit );

	if( oldName != newDisplayName ) {
		kDebug() << "setting label for " << layoutUnit.toPair() << " : " << newDisplayName << endl;
		selLayout->setText(LAYOUT_COLUMN_DISPLAY_NAME, newDisplayName);
		updateIndicator(selLayout);
		emit changed();
	}
}

/** will update flag with label if layout label has been edited
*/
void LayoutConfig::updateIndicator(Q3ListViewItem*)
{
}


void LayoutConfig::latinChanged()
{
    Q3ListViewItem* selLayout = widget->listLayoutsDst->selectedItem();
    if (  !selLayout ) {
      widget->chkLatin->setChecked( false );
      widget->chkLatin->setEnabled( false );
      return;
    }

	QString include;
	if( widget->chkLatin->isChecked() )
		include = "us";
    else
		include = "";
	selLayout->setText(LAYOUT_COLUMN_INCLUDE, include);

 	LayoutUnit layoutUnitKey = getLayoutUnitKey(selLayout);
	kDebug() << "layout " << layoutUnitKey.toPair() << ", inc: " << include << endl;
}

void LayoutConfig::layoutSelChanged(Q3ListViewItem *sel)
{
    widget->comboVariant->clear();
    widget->comboVariant->setEnabled( sel != NULL );
    widget->chkLatin->setChecked( false );
    widget->chkLatin->setEnabled( sel != NULL );

    if( sel == NULL ) {
        updateLayoutCommand();
        return;
    }


	LayoutUnit layoutUnitKey = getLayoutUnitKey(sel);
	QString kbdLayout = layoutUnitKey.layout;

	// TODO: need better algorithm here for determining if needs us group
    if (  ! m_rules->isSingleGroup(kbdLayout)
	    		|| kbdLayout.startsWith("us") || kbdLayout.startsWith("en") ) {
        widget->chkLatin->setEnabled( false );
    }
    else {
		QString inc = sel->text(LAYOUT_COLUMN_INCLUDE);
		if ( inc.startsWith("us") || inc.startsWith("en") ) {
            widget->chkLatin->setChecked(true);
        }
        else {
            widget->chkLatin->setChecked(false);
        }
    }

	QStringList vars = m_rules->getAvailableVariants(kbdLayout);
	kDebug() << "layout " << kbdLayout << " has " << vars.count() << " variants" << endl;

	if( vars.count() > 0 ) {
		vars.prepend(DEFAULT_VARIANT_NAME);
		widget->comboVariant->addItems(vars);

		QString variant = sel->text(LAYOUT_COLUMN_VARIANT);
		if( variant != NULL && variant.isEmpty() == false ) {
			widget->comboVariant->setCurrentText(variant);
		}
		else {
			widget->comboVariant->setCurrentIndex(0);
		}
	}
    updateLayoutCommand();
}

QWidget* LayoutConfig::makeOptionsTab()
{
  Q3ListView *listView = widget->listOptions;

  listView->setMinimumHeight(150);
  listView->setSortColumn( -1 );
  listView->setColumnText( 0, i18n( "Options" ) );
  listView->clear();

  connect(listView, SIGNAL(clicked(Q3ListViewItem *)), SLOT(changed()));
  connect(listView, SIGNAL(clicked(Q3ListViewItem *)), SLOT(updateOptionsCommand()));

  connect(widget->chkEnableOptions, SIGNAL(toggled(bool)), SLOT(changed()));

  connect(widget->checkResetOld, SIGNAL(toggled(bool)), SLOT(changed()));
  connect(widget->checkResetOld, SIGNAL(toggled(bool)), SLOT(updateOptionsCommand()));

  //Create controllers for all options
  QHashIterator<QString, XkbOptionGroup> it( m_rules->optionGroups() );
  for (; it.hasNext(); )
  {
	  OptionListItem *parent;
	  const XkbOptionGroup& optionGroup = it.next().value();

      if( optionGroup.exclusive ) {
        parent = new OptionListItem(listView, i18n( optionGroup.description ),
          		Q3CheckListItem::RadioButtonController, optionGroup.name);
        OptionListItem *item = new OptionListItem(parent, i18n( "None" ),
          		Q3CheckListItem::RadioButton, "none");
        item->setState(Q3CheckListItem::On);
      }
      else {
        parent = new OptionListItem(listView, i18n( optionGroup.description ),
            Q3CheckListItem::CheckBoxController, optionGroup.name);
      }

      parent->setOpen(true);
      m_optionGroups.insert( optionGroup.name, parent);
	  kDebug() << "optionGroup insert: " << optionGroup.name << endl;
  }


  QHashIterator<QString, XkbOption> it2( m_rules->options() );
  for (; it2.hasNext(); )
  {
	  const XkbOption& option = it2.next().value();

	  OptionListItem *parent = m_optionGroups[option.group->name];
	  if( parent == NULL ) {
		kError() << "no option group item for group: " << option.group->name
			   << " for option " << option.name << endl;
		exit(1);
	  }

     if( parent->type() == Q3CheckListItem::RadioButtonController )
		new OptionListItem(parent, i18n( option.description ),
             Q3CheckListItem::RadioButton, option.name);
     else
	 	new OptionListItem(parent, i18n( option.description ),
            Q3CheckListItem::CheckBox, option.name);

//	  kDebug() << "option insert: " << option.name << endl;
  }

  //scroll->setMinimumSize(450, 330);

  return listView;
}

void LayoutConfig::updateOptionsCommand()
{
  QString setxkbmap;
  QString options = createOptionString();

  if( !options.isEmpty() ) {
    setxkbmap = "setxkbmap -option "; //-rules " + m_rule
    if( widget->checkResetOld->isChecked() )
      setxkbmap += "-option ";
    setxkbmap += options;
  }
  widget->editCmdLineOpt->setText(setxkbmap);
}

void LayoutConfig::updateLayoutCommand()
{
  QString setxkbmap;
  QString layoutDisplayName;
  Q3ListViewItem* sel = widget->listLayoutsDst->selectedItem();

  if( sel != NULL ) {
    QString kbdLayout = sel->text(LAYOUT_COLUMN_MAP);
    QString variant = widget->comboVariant->currentText();
	if( variant == DEFAULT_VARIANT_NAME )
		variant = "";

    setxkbmap = "setxkbmap"; //-rules " + m_rule
    setxkbmap += " -model " + lookupLocalized(m_rules->models(), widget->comboModel->currentText())
      + " -layout ";
    if( widget->chkLatin->isChecked() )
      setxkbmap += "us,";
    setxkbmap += kbdLayout;

/*	LayoutUnit layoutUnitKey = getLayoutUnitKey(sel);
	layoutDisplayName = m_kxkbConfig.getLayoutDisplayName( *m_kxkbConfig.m_layouts.find(layoutUnitKey) );*/
	layoutDisplayName = sel->text(LAYOUT_COLUMN_DISPLAY_NAME);
	if( layoutDisplayName.isEmpty() ) {
		int count = 0;
		Q3ListViewItem *item = widget->listLayoutsDst->firstChild();
		while (item) {
			QString layout_ = item->text(LAYOUT_COLUMN_MAP);
			if( layout_ == kbdLayout )
				++count;
			item = item->nextSibling();
		}
		bool single = count < 2;
		layoutDisplayName = m_kxkbConfig.getDefaultDisplayName(LayoutUnit(kbdLayout, variant), single);
	}
	kDebug() << "disp: '" << layoutDisplayName << "'" << endl;

    if( !variant.isEmpty() ) {
      setxkbmap += " -variant ";
      if( widget->chkLatin->isChecked() )
        setxkbmap += ',';
      setxkbmap += variant;
    }
  }

  widget->editCmdLine->setText(setxkbmap);

  widget->editDisplayName->setEnabled( sel != NULL );
  widget->editDisplayName->setText(layoutDisplayName);
}

void LayoutConfig::changed()
{
  updateLayoutCommand();
  emit KCModule::changed( true );
}


void LayoutConfig::loadRules()
{
    // do we need this ?
    // this could obly be used if rules are changed and 'Defaults' is pressed
    delete m_rules;
    m_rules = new XkbRules();

    QStringList modelsList;
    QHashIterator<QString, QString> it(m_rules->models());
    while (it.hasNext()) {
		modelsList.append(i18n(it.next().value()));
    }
    modelsList.sort();

	widget->comboModel->clear();
	widget->comboModel->addItems(modelsList);
	widget->comboModel->setCurrentIndex(0);

	// fill in the additional layouts
	widget->listLayoutsSrc->clear();
	widget->listLayoutsDst->clear();
	QHashIterator<QString, QString> it2(m_rules->layouts());

	while (it2.hasNext())
	{
		it2.next();
		QString layout = it2.key();
		QString layoutName = it2.value();
		Q3ListViewItem *item = new Q3ListViewItem(widget->listLayoutsSrc);

		item->setPixmap(LAYOUT_COLUMN_FLAG, LayoutIcon::getInstance().findPixmap(layout, true));
		item->setText(LAYOUT_COLUMN_NAME, i18n( layoutName.toLatin1().constData() ));
		item->setText(LAYOUT_COLUMN_MAP, layout);
	}
	widget->listLayoutsSrc->setSorting(LAYOUT_COLUMN_NAME);	// from Qt3 QListView sorts by language

	//TODO: reset options and xkb options
}


QString LayoutConfig::createOptionString()
{
  QString options;
  for (QHashIterator<QString, XkbOption> it(m_rules->options()); it.hasNext(); )
  {
    const XkbOption& option = it.next().value();

      OptionListItem *item = m_optionGroups[ option.group->name ];

      if( !item ) {
        kDebug() << "WARNING: skipping empty group for " << option.name
          << " - could not found group: " << option.group->name << endl;
        continue;
      }

      OptionListItem *child = item->findChildItem( option.name );

      if ( child ) {
        if ( child->state() == Q3CheckListItem::On ) {
          QString selectedName = child->optionName();
          if ( !selectedName.isEmpty() && selectedName != "none" ) {
            if (!options.isEmpty())
              options.append(',');
            options.append(selectedName);
          }
        }
      }
      else
        kDebug() << "Empty option button for group " << it.key() << endl;
  }
  return options;
}


void LayoutConfig::defaults()
{
	loadRules();
	m_kxkbConfig.setDefaults();

	initUI();

	emit KCModule::changed( true );
}


OptionListItem::OptionListItem( OptionListItem *parent, const QString &text,
								Type tt, const QString &optionName )
	: Q3CheckListItem( parent, text, tt ), m_OptionName( optionName )
{
}

OptionListItem::OptionListItem( Q3ListView *parent, const QString &text,
								Type tt, const QString &optionName )
	: Q3CheckListItem( parent, text, tt ), m_OptionName( optionName )
{
}

OptionListItem * OptionListItem::findChildItem( const QString& optionName )
{
	OptionListItem *child = static_cast<OptionListItem *>( firstChild() );

	while ( child )
	{
		if ( child->optionName() == optionName )
			break;
		child = static_cast<OptionListItem *>( child->nextSibling() );
	}

	return child;
}

extern "C"
{
	KDE_EXPORT void kcminit_keyboard()
	{
		KeyboardConfig::init_keyboard();

		KxkbConfig m_kxkbConfig;
		m_kxkbConfig.load(KxkbConfig::LOAD_INIT_OPTIONS);

		if( m_kxkbConfig.m_useKxkb == true ) {
			KToolInvocation::startServiceByDesktopName("kxkb");
		}
		else {
		// Even if the layouts have been disabled we still want to set Xkb options
		// user can always switch them off now in the "Options" tab
			if( m_kxkbConfig.m_enableXkbOptions ) {
				if( !XKBExtension::setXkbOptions(m_kxkbConfig.m_options, m_kxkbConfig.m_resetOldOptions) ) {
					kDebug() << "Setting XKB options failed!" << endl;
				}
			}
		}
	}
}



#if 0// do not remove!
// please don't change/fix messages below
// they're taken from XFree86 as is and should stay the same
   I18N_NOOP("Brazilian ABNT2");
   I18N_NOOP("Dell 101-key PC");
   I18N_NOOP("Everex STEPnote");
   I18N_NOOP("Generic 101-key PC");
   I18N_NOOP("Generic 102-key (Intl) PC");
   I18N_NOOP("Generic 104-key PC");
   I18N_NOOP("Generic 105-key (Intl) PC");
   I18N_NOOP("Japanese 106-key");
   I18N_NOOP("Microsoft Natural");
   I18N_NOOP("Northgate OmniKey 101");
   I18N_NOOP("Keytronic FlexPro");
   I18N_NOOP("Winbook Model XP5");

// These options are from XFree 4.1.0
 I18N_NOOP("Group Shift/Lock behavior");
 I18N_NOOP("R-Alt switches group while pressed");
 I18N_NOOP("Right Alt key changes group");
 I18N_NOOP("Caps Lock key changes group");
 I18N_NOOP("Menu key changes group");
 I18N_NOOP("Both Shift keys together change group");
 I18N_NOOP("Control+Shift changes group");
 I18N_NOOP("Alt+Control changes group");
 I18N_NOOP("Alt+Shift changes group");
 I18N_NOOP("Control Key Position");
 I18N_NOOP("Make CapsLock an additional Control");
 I18N_NOOP("Swap Control and Caps Lock");
 I18N_NOOP("Control key at left of 'A'");
 I18N_NOOP("Control key at bottom left");
 I18N_NOOP("Use keyboard LED to show alternative group");
 I18N_NOOP("Num_Lock LED shows alternative group");
 I18N_NOOP("Caps_Lock LED shows alternative group");
 I18N_NOOP("Scroll_Lock LED shows alternative group");

//these seem to be new in XFree86 4.2.0
 I18N_NOOP("Left Win-key switches group while pressed");
 I18N_NOOP("Right Win-key switches group while pressed");
 I18N_NOOP("Both Win-keys switch group while pressed");
 I18N_NOOP("Left Win-key changes group");
 I18N_NOOP("Right Win-key changes group");
 I18N_NOOP("Third level choosers");
 I18N_NOOP("Press Right Control to choose 3rd level");
 I18N_NOOP("Press Menu key to choose 3rd level");
 I18N_NOOP("Press any of Win-keys to choose 3rd level");
 I18N_NOOP("Press Left Win-key to choose 3rd level");
 I18N_NOOP("Press Right Win-key to choose 3rd level");
 I18N_NOOP("CapsLock key behavior");
 I18N_NOOP("uses internal capitalization. Shift cancels Caps.");
 I18N_NOOP("uses internal capitalization. Shift does not cancel Caps.");
 I18N_NOOP("acts as Shift with locking. Shift cancels Caps.");
 I18N_NOOP("acts as Shift with locking. Shift does not cancel Caps.");
 I18N_NOOP("Alt/Win key behavior");
 I18N_NOOP("Add the standard behavior to Menu key.");
 I18N_NOOP("Alt and Meta on the Alt keys (default).");
 I18N_NOOP("Meta is mapped to the Win-keys.");
 I18N_NOOP("Meta is mapped to the left Win-key.");
 I18N_NOOP("Super is mapped to the Win-keys (default).");
 I18N_NOOP("Hyper is mapped to the Win-keys.");
 I18N_NOOP("Right Alt is Compose");
 I18N_NOOP("Right Win-key is Compose");
 I18N_NOOP("Menu is Compose");

//these seem to be new in XFree86 4.3.0
 I18N_NOOP( "Both Ctrl keys together change group" );
 I18N_NOOP( "Both Alt keys together change group" );
 I18N_NOOP( "Left Shift key changes group" );
 I18N_NOOP( "Right Shift key changes group" );
 I18N_NOOP( "Right Ctrl key changes group" );
 I18N_NOOP( "Left Alt key changes group" );
 I18N_NOOP( "Left Ctrl key changes group" );
 I18N_NOOP( "Compose Key" );

//these seem to be new in XFree86 4.4.0
 I18N_NOOP("Shift with numpad keys works as in MS Windows.");
 I18N_NOOP("Special keys (Ctrl+Alt+<key>) handled in a server.");
 I18N_NOOP("Miscellaneous compatibility options");
 I18N_NOOP("Right Control key works as Right Alt");

//these seem to be in x.org and Debian XFree86 4.3
 I18N_NOOP("Right Alt key switches group while pressed");
 I18N_NOOP("Left Alt key switches group while pressed");
 I18N_NOOP("Press Right Alt-key to choose 3rd level");
#endif
