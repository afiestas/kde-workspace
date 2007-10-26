
//Own
#include "core/favoritesmodel.h"

// Qt
#include <QHash>
#include <QList>

// KDE
#include <KConfigGroup>
#include <KService>
#include <kdebug.h>

// Local
#include "core/models.h"

using namespace Kickoff;

class FavoritesModel::Private
{
public:
    Private(FavoritesModel *parent)
        :q(parent)
    {
        headerItem = new QStandardItem(i18n("Favorites"));
        q->appendRow(headerItem);
    }

    void addFavoriteItem(const QString& url)
    {
        QStandardItem *item = StandardItemFactory::createItemForUrl(url);
        headerItem->appendRow(item);
    }
    void removeFavoriteItem(const QString& url)
    {
       QModelIndexList matches = q->match(q->index(0,0),UrlRole,
                                          url,-1,
                                          Qt::MatchFlags(Qt::MatchStartsWith | Qt::MatchWrap | Qt::MatchRecursive));

       qDebug() << "Removing item matches" << matches;

       foreach(const QModelIndex& index,matches) {
         QStandardItem *item = q->itemFromIndex(index);
         if (item->parent()) {
            item->parent()->removeRow(item->row());
         } else {
            qDeleteAll(q->takeRow(item->row()));
         }
       }
    }

    FavoritesModel * const q;
    QStandardItem *headerItem;

    static void loadFavorites()
    {
       KConfigGroup favoritesGroup = componentData().config()->group("Favorites");
       QList<QString> favoriteList = favoritesGroup.readEntry("FavoriteURLs",QList<QString>());
       if (favoriteList.isEmpty()) {
           favoriteList = defaultFavorites();
       }

       foreach(QString favorite,favoriteList) {
           FavoritesModel::add(favorite);
       }
    }
    static QList<QString> defaultFavorites()
    {
        QList<QString> applications;
        applications << "konqbrowser" << "kmail" << "systemsettings" << "dolphin";

        QList<QString> desktopFiles;

        foreach(const QString& application,applications) {
            KService::Ptr service = KService::serviceByStorageId(application);
            if (service) {
                desktopFiles << service->entryPath();
            }
        }

        return desktopFiles;
    }
    static void saveFavorites()
    {
        KConfigGroup favoritesGroup = componentData().config()->group("Favorites");
        favoritesGroup.writeEntry("FavoriteURLs",globalFavoriteList);
    }
    static QList<QString> globalFavoriteList;
    static QSet<QString> globalFavoriteSet;
    static QSet<FavoritesModel*> models;
};

QList<QString> FavoritesModel::Private::globalFavoriteList;
QSet<QString> FavoritesModel::Private::globalFavoriteSet;
QSet<FavoritesModel*> FavoritesModel::Private::models;

FavoritesModel::FavoritesModel(QObject *parent)
    : QStandardItemModel(parent)
    , d(new Private(this))
{
    Private::models << this;
    if (Private::models.count() == 1 && Private::globalFavoriteList.isEmpty()) {
        Private::loadFavorites();
    }
}
FavoritesModel::~FavoritesModel()
{
    Private::models.remove(this);

    if (Private::models.isEmpty()) {
        Private::saveFavorites();
    }

    delete d;
}
void FavoritesModel::add(const QString& url)
{
    Private::globalFavoriteList << url;
    Private::globalFavoriteSet << url;

    foreach(FavoritesModel* model,Private::models) {
        model->d->addFavoriteItem(url);
    }
}
void FavoritesModel::remove(const QString& url)
{
   Private::globalFavoriteList.removeAll(url);
   Private::globalFavoriteSet.remove(url);

   foreach(FavoritesModel* model,Private::models) {
       model->d->removeFavoriteItem(url);
   }
}
bool FavoritesModel::isFavorite(const QString& url)
{
    return Private::globalFavoriteSet.contains(url);
}

#include "favoritesmodel.moc"
