/**************************************************************************
* Otter Browser: Web browser controlled by the user, not vice-versa.
* Copyright (C) 2014 Piotr Wójcik <chocimier@tlen.pl>
* Copyright (C) 2014 - 2017 Michal Dutkiewicz aka Emdek <michal@emdek.pl>
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

#ifndef OTTER_OPERABOOKMARKSIMPORTER_H
#define OTTER_OPERABOOKMARKSIMPORTER_H

#include "../../../core/BookmarksImporter.h"

namespace Otter
{

class BookmarksImporterWidget;

class OperaBookmarksImporter final : public BookmarksImporter
{
	Q_OBJECT

public:
	explicit OperaBookmarksImporter(QObject *parent = nullptr);
	~OperaBookmarksImporter();

	QWidget* getOptionsWidget() override;
	QString getTitle() const override;
	QString getDescription() const override;
	QString getVersion() const override;
	QString getSuggestedPath(const QString &path = {}) const override;
	QString getBrowser() const override;
	QUrl getHomePage() const override;
	QStringList getFileFilters() const override;

public slots:
	bool import(const QString &path) override;

protected:
	enum OperaBookmarkEntry
	{
		NoEntry = 0,
		UrlEntry,
		FolderStartEntry,
		FolderEndEntry,
		SeparatorEntry
	};

private:
	BookmarksImporterWidget *m_optionsWidget;
};

}

#endif
