#ifndef __KCM_FONT_INST_H__
#define __KCM_FONT_INST_H__

////////////////////////////////////////////////////////////////////////////////
//
// Class Name    : CKCmFontInst
// Author        : Craig Drummond
// Project       : K Font Installer
// Creation Date : 26/04/2003
// Version       : $Revision$ $Date$
//
////////////////////////////////////////////////////////////////////////////////
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
//
////////////////////////////////////////////////////////////////////////////////
// (C) Craig Drummond, 2003
////////////////////////////////////////////////////////////////////////////////

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include <kcmodule.h>
#include <qstringlist.h>
#include <qsize.h>
#include <kurl.h>
#include <kconfig.h>
#include <kio/job.h>
#include <kparts/part.h>

class KDirOperator;
class KAction;
class KRadioAction;
class KActionMenu;
class KFileItem;
class QLabel;
class QSplitter;
class KURLLabel;
class QDropEvent;
class KFileItem;

class CKCmFontInst : public KCModule
{
    Q_OBJECT

    public:

    CKCmFontInst(QWidget *parent=NULL, const char *name=NULL, const QStringList &list=QStringList());
    virtual ~CKCmFontInst();

    QSize sizeHint() const;

    public slots:

    QString quickHelp() const;
    void    gotoTop();
    void    goUp();
    void    goBack();
    void    goForward();
    void    listView();
    void    iconView();
    void    setupViewMenu();
    void    urlEntered(const KURL &url);
    void    fileHighlighted(const KFileItem *item);
    void    loadingFinished();
    void    addFonts();
    void    removeFonts();
    void    createFolder();
    void    enable();
    void    disable();
    void    dropped(const KFileItem *i, QDropEvent *e, const KURL::List &urls);
    void    openUrlInBrowser(const QString &url);
    void    infoMessage(const QString &msg);
    void    updateInformation(int dirs, int fonts);
    void    jobResult(KIO::Job *job);

    private:

    void    setUpAct();
    void    enableItems(bool enable);
    void    addFonts(const KURL::List &src, const KURL &dest);

    private:

    KDirOperator         *itsDirOp;
    KURL                 itsTop;
    KAction              *itsUpAct,
                         *itsSepDirsAct,
                         *itsShowHiddenAct,
                         *itsDeleteAct,
                         *itsEnableAct,
                         *itsDisableAct;
    KRadioAction         *itsListAct,
                         *itsIconAct;
    KActionMenu          *itsViewMenuAct;
    KURLLabel            *itsLabel;
    KParts::ReadOnlyPart *itsPreview;
    QSplitter            *itsSplitter;
    KConfig              itsConfig;
    bool                 itsEmbeddedAdmin,
                         itsKCmshell;
    QLabel               *itsStatusLabel;
    QSize                itsSizeHint;
};

#endif
