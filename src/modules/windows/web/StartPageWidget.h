/**************************************************************************
* Otter Browser: Web browser controlled by the user, not vice-versa.
* Copyright (C) 2015 - 2018 Michal Dutkiewicz aka Emdek <michal@emdek.pl>
* Copyright (C) 2016 - 2017 Piotr Wójcik <chocimier@tlen.pl>
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

#ifndef OTTER_STARTPAGEWIDGET_H
#define OTTER_STARTPAGEWIDGET_H

#include "StartPagePreferencesDialog.h"

#include <QtCore/QTime>
#include <QtWidgets/QListView>
#include <QtWidgets/QScrollArea>
#include <QtWidgets/QStyledItemDelegate>

namespace Otter
{

class Animation;
class SearchWidget;
class StartPageModel;
class Window;

class TileDelegate final : public QStyledItemDelegate
{
	Q_OBJECT

public:
	enum BackgroundMode
	{
		NoBackground = 0,
		FaviconBackground,
		ThumbnailBackground
	};

	explicit TileDelegate(QObject *parent = nullptr);

	void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override;
	QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override;

protected slots:
	void handleOptionChanged(int identifier, const QVariant &value);

private:
	BackgroundMode m_mode;
};

class StartPageContentsWidget final : public QWidget
{
public:
	enum BackgroundMode
	{
		NoCustomBackground = 0,
		BestFitBackground,
		CenterBackground,
		StretchBackground,
		TileBackground
	};

	explicit StartPageContentsWidget(QWidget *parent);

	void setBackgroundMode(BackgroundMode mode);

protected:
	void paintEvent(QPaintEvent *event) override;

private:
	QString m_path;
	QColor m_color;
	BackgroundMode m_mode;
};

class StartPageWidget final : public QScrollArea
{
	Q_OBJECT

public:
	explicit StartPageWidget(Window *parent);
	~StartPageWidget();

	void triggerAction(int identifier, const QVariantMap &parameters = {});
	void scrollContents(const QPoint &delta);
	void markForDeletion();
	static Animation* getLoadingAnimation();
	QPixmap createThumbnail();
	bool event(QEvent *event) override;
	bool eventFilter(QObject *object, QEvent *event) override;

protected:
	void timerEvent(QTimerEvent *event) override;
	void resizeEvent(QResizeEvent *event) override;
	void contextMenuEvent(QContextMenuEvent *event) override;
	void wheelEvent(QWheelEvent *event) override;
	void dragEnterEvent(QDragEnterEvent *event) override;
	void dropEvent(QDropEvent *event) override;

protected slots:
	void configure();
	void addTile();
	void openTile();
	void editTile();
	void reloadTile();
	void removeTile();
	void handleOptionChanged(int identifier, const QVariant &value);
	void handleIsReloadingTileChanged(const QModelIndex &index);
	void updateSize();
	void showContextMenu(const QPoint &position = {});

private:
	Window *m_window;
	StartPageContentsWidget *m_contentsWidget;
	QListView *m_listView;
	SearchWidget *m_searchWidget;
	QPixmap m_thumbnail;
	QTime m_urlOpenTime;
	QModelIndex m_currentIndex;
	int m_deleteTimer;
	bool m_isIgnoringEnter;

	static StartPageModel *m_model;
	static Animation *m_spinnerAnimation;
	static QPointer<StartPagePreferencesDialog> m_preferencesDialog;
};

}

#endif
