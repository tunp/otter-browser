/**************************************************************************
* Otter Browser: Web browser controlled by the user, not vice-versa.
* Copyright (C) 2013 - 2017 Michal Dutkiewicz aka Emdek <michal@emdek.pl>
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

#include "BookmarksContentsWidget.h"
#include "../../../core/Application.h"
#include "../../../core/SessionModel.h"
#include "../../../core/SessionsManager.h"
#include "../../../core/SettingsManager.h"
#include "../../../core/ThemesManager.h"
#include "../../../core/Utils.h"
#include "../../../ui/Action.h"
#include "../../../ui/BookmarkPropertiesDialog.h"
#include "../../../ui/MainWindow.h"
#include "../../../ui/ProxyModel.h"
#include "../../../ui/Window.h"

#include "ui_BookmarksContentsWidget.h"

#include <QtCore/QTimer>
#include <QtGui/QClipboard>
#include <QtGui/QMouseEvent>
#include <QtWidgets/QDesktopWidget>
#include <QtWidgets/QMenu>
#include <QtWidgets/QToolTip>

namespace Otter
{

BookmarksContentsWidget::BookmarksContentsWidget(const QVariantMap &parameters, Window *window) : ContentsWidget(parameters, window),
	m_ui(new Ui::BookmarksContentsWidget)
{
	m_ui->setupUi(this);

	QMenu *addMenu(new QMenu(m_ui->addButton));
	addMenu->addAction(ThemesManager::createIcon(QLatin1String("inode-directory")), tr("Add Folder…"), this, SLOT(addFolder()));
	addMenu->addAction(tr("Add Bookmark…"), this, SLOT(addBookmark()));
	addMenu->addAction(tr("Add Separator"), this, SLOT(addSeparator()));

	ProxyModel *model(new ProxyModel(BookmarksManager::getModel(), QVector<QPair<QString, int> >({{tr("Title"), BookmarksModel::TitleRole}, {tr("Address"), BookmarksModel::UrlRole}, {tr("Description"), BookmarksModel::DescriptionRole}, {tr("Keyword"), BookmarksModel::KeywordRole}, {tr("Added"), BookmarksModel::TimeAddedRole}, {tr("Modified"), BookmarksModel::TimeModifiedRole}, {tr("Visited"), BookmarksModel::TimeVisitedRole}, {tr("Visits"), BookmarksModel::VisitsRole}}), this));

	m_ui->addButton->setMenu(addMenu);
	m_ui->bookmarksViewWidget->setViewMode(ItemViewWidget::TreeViewMode);
	m_ui->bookmarksViewWidget->setModel(model);
	m_ui->bookmarksViewWidget->setExpanded(m_ui->bookmarksViewWidget->model()->index(0, 0), true);
	m_ui->bookmarksViewWidget->setFilterRoles(QSet<int>({BookmarksModel::UrlRole, BookmarksModel::TitleRole, BookmarksModel::DescriptionRole, BookmarksModel::KeywordRole}));
	m_ui->bookmarksViewWidget->installEventFilter(this);
	m_ui->bookmarksViewWidget->viewport()->installEventFilter(this);
	m_ui->bookmarksViewWidget->viewport()->setMouseTracking(true);
	m_ui->filterLineEdit->installEventFilter(this);

	if (isSidebarPanel())
	{
		m_ui->detailsWidget->hide();
	}

	connect(BookmarksManager::getModel(), SIGNAL(modelReset()), this, SLOT(updateActions()));
	connect(m_ui->propertiesButton, SIGNAL(clicked()), this, SLOT(bookmarkProperties()));
	connect(m_ui->deleteButton, SIGNAL(clicked()), this, SLOT(removeBookmark()));
	connect(m_ui->addButton, SIGNAL(clicked()), this, SLOT(addBookmark()));
	connect(m_ui->filterLineEdit, SIGNAL(textChanged(QString)), m_ui->bookmarksViewWidget, SLOT(setFilterString(QString)));
	connect(m_ui->bookmarksViewWidget, SIGNAL(doubleClicked(QModelIndex)), this, SLOT(openBookmark(QModelIndex)));
	connect(m_ui->bookmarksViewWidget, SIGNAL(customContextMenuRequested(QPoint)), this, SLOT(showContextMenu(QPoint)));
	connect(m_ui->bookmarksViewWidget, SIGNAL(needsActionsUpdate()), this, SLOT(updateActions()));
}

BookmarksContentsWidget::~BookmarksContentsWidget()
{
	delete m_ui;
}

void BookmarksContentsWidget::changeEvent(QEvent *event)
{
	ContentsWidget::changeEvent(event);

	if (event->type() == QEvent::LanguageChange)
	{
		m_ui->retranslateUi(this);
	}
}

void BookmarksContentsWidget::addBookmark()
{
	const QModelIndex index(m_ui->bookmarksViewWidget->currentIndex());
	BookmarksItem *folder(findFolder(index));
	BookmarkPropertiesDialog dialog(QUrl(), QString(), QString(), folder, ((folder && folder->index() == index) ? -1 : (index.row() + 1)), true, this);
	dialog.exec();
}

void BookmarksContentsWidget::addFolder()
{
	const QModelIndex index(m_ui->bookmarksViewWidget->currentIndex());
	BookmarksItem *folder(findFolder(index));
	BookmarkPropertiesDialog dialog(QUrl(), QString(), QString(), folder, ((folder && folder->index() == index) ? -1 : (index.row() + 1)), false, this);
	dialog.exec();
}

void BookmarksContentsWidget::addSeparator()
{
	const QModelIndex index(m_ui->bookmarksViewWidget->currentIndex());
	BookmarksItem *folder(findFolder(index));

	BookmarksManager::addBookmark(BookmarksModel::SeparatorBookmark, {}, folder, ((folder && folder->index() == index) ? -1 : (index.row() + 1)));
}

void BookmarksContentsWidget::removeBookmark()
{
	BookmarksManager::getModel()->trashBookmark(BookmarksManager::getModel()->getBookmark(m_ui->bookmarksViewWidget->currentIndex()));
}

void BookmarksContentsWidget::restoreBookmark()
{
	BookmarksManager::getModel()->restoreBookmark(BookmarksManager::getModel()->getBookmark(m_ui->bookmarksViewWidget->currentIndex()));
}

void BookmarksContentsWidget::openBookmark(const QModelIndex &index)
{
	const BookmarksItem *bookmark(BookmarksManager::getModel()->getBookmark(index.isValid() ? index : m_ui->bookmarksViewWidget->currentIndex()));

	if (bookmark)
	{
		const QAction *action(qobject_cast<QAction*>(sender()));

		Application::triggerAction(ActionsManager::OpenBookmarkAction, {{QLatin1String("bookmark"), bookmark->data(BookmarksModel::IdentifierRole)}, {QLatin1String("hints"), QVariant((action ? static_cast<SessionsManager::OpenHints>(action->data().toInt()) : SessionsManager::DefaultOpen))}}, parentWidget());
	}
}

void BookmarksContentsWidget::bookmarkProperties()
{
	BookmarksItem *bookmark(BookmarksManager::getModel()->getBookmark(m_ui->bookmarksViewWidget->currentIndex()));

	if (bookmark)
	{
		BookmarkPropertiesDialog dialog(bookmark, this);
		dialog.exec();

		updateActions();
	}
}

void BookmarksContentsWidget::showContextMenu(const QPoint &position)
{
	const QModelIndex index(m_ui->bookmarksViewWidget->indexAt(position));
	const BookmarksModel::BookmarkType type(static_cast<BookmarksModel::BookmarkType>(index.data(BookmarksModel::TypeRole).toInt()));
	QMenu menu(this);

	switch (type)
	{
		case BookmarksModel::TrashBookmark:
			menu.addAction(ThemesManager::createIcon(QLatin1String("trash-empty")), tr("Empty Trash"), BookmarksManager::getModel(), SLOT(emptyTrash()))->setEnabled(BookmarksManager::getModel()->getTrashItem()->rowCount() > 0);

			break;
		case BookmarksModel::UnknownBookmark:
			menu.addAction(ThemesManager::createIcon(QLatin1String("inode-directory")), tr("Add Folder…"), this, SLOT(addFolder()));
			menu.addAction(tr("Add Bookmark…"), this, SLOT(addBookmark()));
			menu.addAction(tr("Add Separator"), this, SLOT(addSeparator()));

			break;
		default:
			{
				const bool isInTrash(index.data(BookmarksModel::IsTrashedRole).toBool());

				menu.addAction(ThemesManager::createIcon(QLatin1String("document-open")), tr("Open"), this, SLOT(openBookmark()));
				menu.addAction(tr("Open in New Tab"), this, SLOT(openBookmark()))->setData(SessionsManager::NewTabOpen);
				menu.addAction(tr("Open in New Background Tab"), this, SLOT(openBookmark()))->setData(static_cast<int>(SessionsManager::NewTabOpen | SessionsManager::BackgroundOpen));
				menu.addSeparator();
				menu.addAction(tr("Open in New Window"), this, SLOT(openBookmark()))->setData(SessionsManager::NewWindowOpen);
				menu.addAction(tr("Open in New Background Window"), this, SLOT(openBookmark()))->setData(static_cast<int>(SessionsManager::NewWindowOpen | SessionsManager::BackgroundOpen));

				if (type == BookmarksModel::SeparatorBookmark || (type == BookmarksModel::FolderBookmark && index.child(0, 0).data(BookmarksModel::TypeRole).toInt() == 0))
				{
					for (int i = 0; i < menu.actions().count(); ++i)
					{
						menu.actions().at(i)->setEnabled(false);
					}
				}

				MainWindow *mainWindow(MainWindow::findMainWindow(this));
				ActionExecutor::Object executor(this, this);
				QVariantMap parameters;

				if (type == BookmarksModel::FolderBookmark)
				{
					parameters[QLatin1String("folder")] = index.data(BookmarksModel::IdentifierRole);
				}

				menu.addSeparator();
				menu.addAction(new Action(ActionsManager::BookmarkAllOpenPagesAction, parameters, ActionExecutor::Object(mainWindow, mainWindow), &menu));
				menu.addSeparator();
				menu.addAction(new Action(ActionsManager::CopyLinkToClipboardAction, {}, executor, &menu));

				if (!isInTrash)
				{
					menu.addSeparator();

					QMenu *addMenu(menu.addMenu(tr("Add Bookmark")));
					addMenu->addAction(ThemesManager::createIcon(QLatin1String("inode-directory")), tr("Add Folder"), this, SLOT(addFolder()));
					addMenu->addAction(tr("Add Bookmark"), this, SLOT(addBookmark()));
					addMenu->addAction(tr("Add Separator"), this, SLOT(addSeparator()));
				}

				if (type != BookmarksModel::RootBookmark)
				{
					menu.addSeparator();

					if (isInTrash)
					{
						menu.addAction(tr("Restore Bookmark"), this, SLOT(restoreBookmark()));
					}
					else
					{
						menu.addAction(new Action(ActionsManager::DeleteAction, {}, executor, &menu));
					}

					if (type != BookmarksModel::SeparatorBookmark)
					{
						menu.addSeparator();
						menu.addAction(tr("Properties…"), this, SLOT(bookmarkProperties()));
					}
				}
			}

			break;
	}

	menu.exec(m_ui->bookmarksViewWidget->mapToGlobal(position));
}

void BookmarksContentsWidget::triggerAction(int identifier, const QVariantMap &parameters)
{
	switch (identifier)
	{
		case ActionsManager::DeleteAction:
			removeBookmark();

			break;
		case ActionsManager::CopyLinkToClipboardAction:
			if (static_cast<BookmarksModel::BookmarkType>(m_ui->bookmarksViewWidget->currentIndex().data(BookmarksModel::TypeRole).toInt()) == BookmarksModel::UrlBookmark)
			{
				QGuiApplication::clipboard()->setText(m_ui->bookmarksViewWidget->currentIndex().data(BookmarksModel::UrlRole).toString());
			}

			break;
		case ActionsManager::FindAction:
		case ActionsManager::QuickFindAction:
			m_ui->filterLineEdit->setFocus();

			break;
		case ActionsManager::ActivateContentAction:
			m_ui->bookmarksViewWidget->setFocus();

			break;
		default:
			ContentsWidget::triggerAction(identifier, parameters);

			break;
	}
}

void BookmarksContentsWidget::updateActions()
{
	const bool hasSelecion(!m_ui->bookmarksViewWidget->selectionModel()->selectedIndexes().isEmpty());
	const QModelIndex index(hasSelecion ? m_ui->bookmarksViewWidget->selectionModel()->currentIndex().sibling(m_ui->bookmarksViewWidget->selectionModel()->currentIndex().row(), 0) : QModelIndex());
	const BookmarksModel::BookmarkType type(static_cast<BookmarksModel::BookmarkType>(index.data(BookmarksModel::TypeRole).toInt()));

	m_ui->addressLabelWidget->setText(index.data(BookmarksModel::UrlRole).toString());
	m_ui->titleLabelWidget->setText(index.data(BookmarksModel::TitleRole).toString());
	m_ui->descriptionLabelWidget->setText(index.data(BookmarksModel::DescriptionRole).toString());
	m_ui->keywordLabelWidget->setText(index.data(BookmarksModel::KeywordRole).toString());
	m_ui->propertiesButton->setEnabled((hasSelecion && (type == BookmarksModel::FolderBookmark || type == BookmarksModel::UrlBookmark)));
	m_ui->deleteButton->setEnabled(hasSelecion && type != BookmarksModel::RootBookmark && type != BookmarksModel::TrashBookmark);

	emit actionsStateChanged(ActionsManager::ActionDefinition::EditingCategory | ActionsManager::ActionDefinition::LinkCategory);
}

void BookmarksContentsWidget::print(QPrinter *printer)
{
	m_ui->bookmarksViewWidget->render(printer);
}

BookmarksItem* BookmarksContentsWidget::findFolder(const QModelIndex &index)
{
	BookmarksItem *item(BookmarksManager::getModel()->getBookmark(index));

	if (!item || item == BookmarksManager::getModel()->getRootItem() || item == BookmarksManager::getModel()->getTrashItem())
	{
		return BookmarksManager::getModel()->getRootItem();
	}

	const BookmarksModel::BookmarkType type(static_cast<BookmarksModel::BookmarkType>(item->data(BookmarksModel::TypeRole).toInt()));

	return ((type == BookmarksModel::RootBookmark || type == BookmarksModel::FolderBookmark) ? item : static_cast<BookmarksItem*>(item->parent()));
}

QString BookmarksContentsWidget::getTitle() const
{
	return tr("Bookmarks");
}

QLatin1String BookmarksContentsWidget::getType() const
{
	return QLatin1String("bookmarks");
}

QUrl BookmarksContentsWidget::getUrl() const
{
	return QUrl(QLatin1String("about:bookmarks"));
}

QIcon BookmarksContentsWidget::getIcon() const
{
	return ThemesManager::createIcon(QLatin1String("bookmarks"), false);
}

ActionsManager::ActionDefinition::State BookmarksContentsWidget::getActionState(int identifier, const QVariantMap &parameters) const
{
	ActionsManager::ActionDefinition::State state(ActionsManager::getActionDefinition(identifier).getDefaultState());

	switch (identifier)
	{
		case ActionsManager::CopyLinkToClipboardAction:
			state.isEnabled = (static_cast<BookmarksModel::BookmarkType>(m_ui->bookmarksViewWidget->currentIndex().data(BookmarksModel::TypeRole).toInt()) == BookmarksModel::UrlBookmark);

			return state;
		case ActionsManager::DeleteAction:
			state.text = QT_TRANSLATE_NOOP("actions", "Remove Bookmark");
			state.isEnabled = m_ui->deleteButton->isEnabled();

			return state;
		default:
			break;
	}

	return ContentsWidget::getActionState(identifier, parameters);
}

bool BookmarksContentsWidget::eventFilter(QObject *object, QEvent *event)
{
	if (object == m_ui->bookmarksViewWidget && event->type() == QEvent::KeyPress)
	{
		const QKeyEvent *keyEvent(static_cast<QKeyEvent*>(event));

		if (keyEvent && (keyEvent->key() == Qt::Key_Enter || keyEvent->key() == Qt::Key_Return))
		{
			openBookmark();

			return true;
		}

		if (keyEvent && keyEvent->key() == Qt::Key_Delete)
		{
			removeBookmark();

			return true;
		}
	}
	else if (object == m_ui->bookmarksViewWidget->viewport() && event->type() == QEvent::MouseButtonRelease)
	{
		const QMouseEvent *mouseEvent(static_cast<QMouseEvent*>(event));

		if (mouseEvent && ((mouseEvent->button() == Qt::LeftButton && mouseEvent->modifiers() != Qt::NoModifier) || mouseEvent->button() == Qt::MiddleButton))
		{
			const BookmarksItem *bookmark(BookmarksManager::getModel()->getBookmark(m_ui->bookmarksViewWidget->indexAt(mouseEvent->pos())));

			if (bookmark)
			{
				Application::triggerAction(ActionsManager::OpenBookmarkAction, {{QLatin1String("bookmark"), bookmark->data(BookmarksModel::IdentifierRole)}, {QLatin1String("hints"), QVariant(SessionsManager::calculateOpenHints(SessionsManager::NewTabOpen, mouseEvent->button(), mouseEvent->modifiers()))}}, parentWidget());

				return true;
			}
		}
	}
	else if (object == m_ui->bookmarksViewWidget->viewport() && event->type() == QEvent::ToolTip)
	{
		const QHelpEvent *helpEvent(static_cast<QHelpEvent*>(event));

		if (helpEvent)
		{
			const QModelIndex index(m_ui->bookmarksViewWidget->indexAt(helpEvent->pos()));
			const BookmarksItem *bookmark(BookmarksManager::getModel()->getBookmark(index));

			if (bookmark)
			{
				QToolTip::showText(helpEvent->globalPos(), QFontMetrics(QToolTip::font()).elidedText(bookmark->toolTip(), Qt::ElideRight, (QApplication::desktop()->screenGeometry(m_ui->bookmarksViewWidget).width() / 2)), m_ui->bookmarksViewWidget, m_ui->bookmarksViewWidget->visualRect(index));
			}

			return true;
		}
	}
	else if (object == m_ui->filterLineEdit && event->type() == QEvent::KeyPress)
	{
		const QKeyEvent *keyEvent(static_cast<QKeyEvent*>(event));

		if (keyEvent->key() == Qt::Key_Escape)
		{
			m_ui->filterLineEdit->clear();
		}
	}

	return ContentsWidget::eventFilter(object, event);
}

}
