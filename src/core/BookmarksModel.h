/**************************************************************************
* Otter Browser: Web browser controlled by the user, not vice-versa.
* Copyright (C) 2013 - 2017 Michal Dutkiewicz aka Emdek <michal@emdek.pl>
* Copyright (C) 2017 Jan Bajer aka bajasoft <jbajer@gmail.com>
*
* This program is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program. If not, see <http://www.gnu.org/licenses/>.
*
**************************************************************************/

#ifndef OTTER_BOOKMARKSMODEL_H
#define OTTER_BOOKMARKSMODEL_H

#include <QtCore/QUrl>
#include <QtCore/QXmlStreamReader>
#include <QtCore/QXmlStreamWriter>
#include <QtGui/QStandardItemModel>

namespace Otter
{

class BookmarksItem : public QStandardItem
{
public:
	void remove();
	void setData(const QVariant &value, int role) override;
	void setItemData(const QVariant &value, int role);
	QStandardItem* clone() const override;
	QVariant data(int role) const override;
	QVariant rawData(int role) const;
	QVector<QUrl> getUrls() const;
	bool isAncestorOf(BookmarksItem *child) const;
	bool operator<(const QStandardItem &other) const override;

protected:
	explicit BookmarksItem();

friend class BookmarksModel;
};

class BookmarksModel final : public QStandardItemModel
{
	Q_OBJECT

public:
	enum BookmarkType
	{
		UnknownBookmark = 0,
		RootBookmark,
		TrashBookmark,
		FolderBookmark,
		UrlBookmark,
		SeparatorBookmark
	};

	enum BookmarkRole
	{
		TitleRole = Qt::DisplayRole,
		UrlRole = Qt::StatusTipRole,
		DescriptionRole = Qt::ToolTipRole,
		IdentifierRole = Qt::UserRole,
		TypeRole,
		KeywordRole,
		TimeAddedRole,
		TimeModifiedRole,
		TimeVisitedRole,
		VisitsRole,
		IsTrashedRole,
		UserRole
	};

	enum FormatMode
	{
		BookmarksMode = 0,
		NotesMode
	};

	struct BookmarkMatch
	{
		BookmarksItem *bookmark = nullptr;
		QString match;
	};

	explicit BookmarksModel(const QString &path, FormatMode mode, QObject *parent = nullptr);

	void beginImport(BookmarksItem *target, int estimatedUrlsAmount = 0, int estimatedKeywordsAmount = 0);
	void endImport();
	void trashBookmark(BookmarksItem *bookmark);
	void restoreBookmark(BookmarksItem *bookmark);
	void removeBookmark(BookmarksItem *bookmark);
	BookmarksItem* addBookmark(BookmarkType type, const QMap<int, QVariant> &metaData = {}, BookmarksItem *parent = nullptr, int index = -1);
	BookmarksItem* getBookmark(const QString &keyword) const;
	BookmarksItem* getBookmark(const QModelIndex &index) const;
	BookmarksItem* getBookmark(quint64 identifier) const;
	BookmarksItem* getRootItem() const;
	BookmarksItem* getTrashItem() const;
	BookmarksItem* getItem(const QString &path) const;
	QMimeData* mimeData(const QModelIndexList &indexes) const override;
	QStringList mimeTypes() const override;
	QStringList getKeywords() const;
	QVector<BookmarkMatch> findBookmarks(const QString &prefix) const;
	QVector<BookmarksItem*> findUrls(const QUrl &url, QStandardItem *branch = nullptr) const;
	QVector<BookmarksItem*> getBookmarks(const QUrl &url) const;
	FormatMode getFormatMode() const;
	bool moveBookmark(BookmarksItem *bookmark, BookmarksItem *newParent, int newRow = -1);
	bool canDropMimeData(const QMimeData *data, Qt::DropAction action, int row, int column, const QModelIndex &parent) const override;
	bool dropMimeData(const QMimeData *data, Qt::DropAction action, int row, int column, const QModelIndex &parent) override;
	bool save(const QString &path) const;
	bool setData(const QModelIndex &index, const QVariant &value, int role) override;
	bool hasBookmark(const QUrl &url) const;
	bool hasKeyword(const QString &keyword) const;

public slots:
	void emptyTrash();

protected:
	void readBookmark(QXmlStreamReader *reader, BookmarksItem *parent);
	void writeBookmark(QXmlStreamWriter *writer, BookmarksItem *bookmark) const;
	void removeBookmarkUrl(BookmarksItem *bookmark);
	void readdBookmarkUrl(BookmarksItem *bookmark);
	void handleKeywordChanged(BookmarksItem *bookmark, const QString &newKeyword, const QString &oldKeyword = {});
	void handleUrlChanged(BookmarksItem *bookmark, const QUrl &newUrl, const QUrl &oldUrl = {});
	static QDateTime readDateTime(QXmlStreamReader *reader, const QString &attribute);

protected slots:
	void notifyBookmarkModified(const QModelIndex &index);

private:
	BookmarksItem *m_rootItem;
	BookmarksItem *m_trashItem;
	BookmarksItem *m_importTargetItem;
	QHash<BookmarksItem*, QPair<QModelIndex, int> > m_trash;
	QHash<QUrl, QVector<BookmarksItem*> > m_urls;
	QHash<QString, BookmarksItem*> m_keywords;
	QMap<quint64, BookmarksItem*> m_identifiers;
	FormatMode m_mode;

signals:
	void bookmarkAdded(BookmarksItem *bookmark);
	void bookmarkModified(BookmarksItem *bookmark);
	void bookmarkMoved(BookmarksItem *bookmark, BookmarksItem *previousParent, int previousRow);
	void bookmarkTrashed(BookmarksItem *bookmark, BookmarksItem *previousParent);
	void bookmarkRestored(BookmarksItem *bookmark);
	void bookmarkRemoved(BookmarksItem *bookmark, BookmarksItem *previousParent);
	void modelModified();

friend class BookmarksItem;
};

}

#endif
