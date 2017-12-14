/**************************************************************************
* Otter Browser: Web browser controlled by the user, not vice-versa.
* Copyright (C) 2015 - 2017 Michal Dutkiewicz aka Emdek <michal@emdek.pl>
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

#ifndef OTTER_BOOKMARKWIDGET_H
#define OTTER_BOOKMARKWIDGET_H

#include "../../../ui/ToolButtonWidget.h"

namespace Otter
{

class BookmarksItem;

class BookmarkWidget final : public ToolButtonWidget
{
	Q_OBJECT

public:
	explicit BookmarkWidget(BookmarksItem *bookmark, const ToolBarsManager::ToolBarDefinition::Entry &definition, QWidget *parent = nullptr);

protected:
	void changeEvent(QEvent *event) override;
	void mouseReleaseEvent(QMouseEvent *event) override;

protected slots:
	void removeBookmark(BookmarksItem *bookmark);
	void updateBookmark(BookmarksItem *bookmark);

private:
	BookmarksItem *m_bookmark;
};

}

#endif
