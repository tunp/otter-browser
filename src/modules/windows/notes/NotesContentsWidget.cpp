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

#include "NotesContentsWidget.h"
#include "../../../core/Application.h"
#include "../../../core/NotesManager.h"
#include "../../../core/SettingsManager.h"
#include "../../../core/ThemesManager.h"
#include "../../../core/Utils.h"
#include "../../../ui/Action.h"

#include "ui_NotesContentsWidget.h"

#include <QtCore/QTimer>
#include <QtGui/QClipboard>
#include <QtGui/QMouseEvent>
#include <QtWidgets/QDesktopWidget>
#include <QtWidgets/QInputDialog>
#include <QtWidgets/QMenu>
#include <QtWidgets/QToolTip>

namespace Otter
{

NotesContentsWidget::NotesContentsWidget(const QVariantMap &parameters, Window *window) : ContentsWidget(parameters, window),
	m_ui(new Ui::NotesContentsWidget)
{
	m_ui->setupUi(this);

	QMenu *addMenu(new QMenu(m_ui->addButton));
	addMenu->addAction(ThemesManager::createIcon(QLatin1String("inode-directory")), tr("Add Folder…"), this, SLOT(addFolder()));
	addMenu->addAction(tr("Add Note"), this, SLOT(addNote()));
	addMenu->addAction(tr("Add Separator"), this, SLOT(addSeparator()));

	m_ui->addButton->setMenu(addMenu);
	m_ui->notesViewWidget->setViewMode(ItemViewWidget::TreeViewMode);
	m_ui->notesViewWidget->setModel(NotesManager::getModel());
	m_ui->notesViewWidget->setExpanded(NotesManager::getModel()->getRootItem()->index(), true);
	m_ui->notesViewWidget->setFilterRoles(QSet<int>({BookmarksModel::UrlRole, BookmarksModel::TitleRole, BookmarksModel::DescriptionRole, BookmarksModel::KeywordRole}));
	m_ui->notesViewWidget->viewport()->installEventFilter(this);
	m_ui->notesViewWidget->viewport()->setMouseTracking(true);
	m_ui->filterLineEdit->installEventFilter(this);
	m_ui->textEdit->setPlaceholderText(tr("Add note…"));

	if (isSidebarPanel())
	{
		m_ui->actionsWidget->hide();
	}

	connect(QGuiApplication::clipboard(), &QClipboard::dataChanged, [&]()
	{
		emit actionsStateChanged(QVector<int>({ActionsManager::PasteAction}));
	});
	connect(NotesManager::getModel(), SIGNAL(modelReset()), this, SLOT(updateActions()));
	connect(m_ui->deleteButton, SIGNAL(clicked()), this, SLOT(removeNote()));
	connect(m_ui->addButton, SIGNAL(clicked()), this, SLOT(addNote()));
	connect(m_ui->textEdit, SIGNAL(textChanged()), this, SLOT(updateText()));
	connect(m_ui->textEdit, &QPlainTextEdit::selectionChanged, [&]()
	{
		emit actionsStateChanged(QVector<int>({ActionsManager::CopyAction, ActionsManager::CutAction}));
	});
	connect(m_ui->filterLineEdit, SIGNAL(textChanged(QString)), m_ui->notesViewWidget, SLOT(setFilterString(QString)));
	connect(m_ui->notesViewWidget, SIGNAL(doubleClicked(QModelIndex)), this, SLOT(openUrl(QModelIndex)));
	connect(m_ui->notesViewWidget, SIGNAL(customContextMenuRequested(QPoint)), this, SLOT(showContextMenu(QPoint)));
	connect(m_ui->notesViewWidget, SIGNAL(needsActionsUpdate()), this, SLOT(updateActions()));
}

NotesContentsWidget::~NotesContentsWidget()
{
	delete m_ui;
}

void NotesContentsWidget::changeEvent(QEvent *event)
{
	ContentsWidget::changeEvent(event);

	if (event->type() == QEvent::LanguageChange)
	{
		m_ui->retranslateUi(this);
	}
}

void NotesContentsWidget::addNote()
{
	NotesManager::addNote(BookmarksModel::UrlBookmark, {}, findFolder(m_ui->notesViewWidget->currentIndex()));
}

void NotesContentsWidget::addFolder()
{
	const QString title(QInputDialog::getText(this, tr("Select Folder Name"), tr("Enter folder name:")));

	if (!title.isEmpty())
	{
		NotesManager::addNote(BookmarksModel::FolderBookmark, {{BookmarksModel::TitleRole, title}}, findFolder(m_ui->notesViewWidget->currentIndex()));
	}
}

void NotesContentsWidget::addSeparator()
{
	NotesManager::addNote(BookmarksModel::SeparatorBookmark, {}, findFolder(m_ui->notesViewWidget->currentIndex()));
}

void NotesContentsWidget::removeNote()
{
	NotesManager::getModel()->trashBookmark(NotesManager::getModel()->getBookmark(m_ui->notesViewWidget->currentIndex()));
}

void NotesContentsWidget::restoreNote()
{
	NotesManager::getModel()->restoreBookmark(NotesManager::getModel()->getBookmark(m_ui->notesViewWidget->currentIndex()));
}

void NotesContentsWidget::openUrl(const QModelIndex &index)
{
	const BookmarksItem *bookmark(NotesManager::getModel()->getBookmark(index.isValid() ? index : m_ui->notesViewWidget->currentIndex()));

	if (bookmark && bookmark->data(BookmarksModel::UrlRole).toUrl().isValid())
	{
		Application::triggerAction(ActionsManager::OpenBookmarkAction, {{QLatin1String("bookmark"), bookmark->data(BookmarksModel::IdentifierRole)}}, parentWidget());
	}
}

void NotesContentsWidget::showContextMenu(const QPoint &position)
{
	const QModelIndex index(m_ui->notesViewWidget->indexAt(position));
	const BookmarksModel::BookmarkType type(static_cast<BookmarksModel::BookmarkType>(index.data(BookmarksModel::TypeRole).toInt()));
	ActionExecutor::Object executor(this, this);
	QMenu menu(this);

	if (type != BookmarksModel::UrlBookmark && type != BookmarksModel::TrashBookmark)
	{
		menu.addAction(new Action(ActionsManager::PasteAction, {}, executor, &menu));
		menu.addSeparator();
	}

	if (type == BookmarksModel::TrashBookmark)
	{
		menu.addAction(ThemesManager::createIcon(QLatin1String("trash-empty")), tr("Empty Trash"), NotesManager::getModel(), SLOT(emptyTrash()))->setEnabled(NotesManager::getModel()->getTrashItem()->rowCount() > 0);
	}
	else if (type == BookmarksModel::UnknownBookmark)
	{
		menu.addAction(ThemesManager::createIcon(QLatin1String("inode-directory")), tr("Add Folder"), this, SLOT(addFolder()));
		menu.addAction(tr("Add Bookmark"), this, SLOT(addNote()));
		menu.addAction(tr("Add Separator"), this, SLOT(addSeparator()));
	}
	else
	{
		const bool isInTrash(index.data(BookmarksModel::IsTrashedRole).toBool());

		if (type == BookmarksModel::UrlBookmark)
		{
			menu.addAction(new Action(ActionsManager::CutAction, {}, executor, &menu));
			menu.addAction(new Action(ActionsManager::CopyAction, {}, executor, &menu));
			menu.addAction(new Action(ActionsManager::PasteAction, {}, executor, &menu));
			menu.addSeparator();
		}

		menu.addAction(ThemesManager::createIcon(QLatin1String("document-open")), tr("Open source page"), this, SLOT(openUrl()))->setEnabled(type == BookmarksModel::UrlBookmark && index.data(BookmarksModel::UrlRole).toUrl().isValid());

		if (type != BookmarksModel::RootBookmark)
		{
			menu.addAction(new Action(ActionsManager::CopyLinkToClipboardAction, {}, executor, &menu));
		}

		if (!isInTrash)
		{
			menu.addSeparator();

			QMenu *addMenu(menu.addMenu(tr("Add Note")));
			addMenu->addAction(ThemesManager::createIcon(QLatin1String("inode-directory")), tr("Add Folder…"), this, SLOT(addFolder()));
			addMenu->addAction(tr("Add Note"), this, SLOT(addNote()));
			addMenu->addAction(tr("Add Separator"), this, SLOT(addSeparator()));
		}

		if (type != BookmarksModel::RootBookmark)
		{
			menu.addSeparator();

			if (isInTrash)
			{
				menu.addAction(tr("Restore Note"), this, SLOT(restoreNote()));
			}
			else
			{
				menu.addAction(new Action(ActionsManager::DeleteAction, {}, executor, &menu));
			}
		}
	}

	menu.exec(m_ui->notesViewWidget->mapToGlobal(position));
}

void NotesContentsWidget::triggerAction(int identifier, const QVariantMap &parameters)
{
	switch (identifier)
	{
		case ActionsManager::CopyLinkToClipboardAction:
			if (static_cast<BookmarksModel::BookmarkType>(m_ui->notesViewWidget->currentIndex().data(BookmarksModel::TypeRole).toInt()) == BookmarksModel::UrlBookmark)
			{
				QGuiApplication::clipboard()->setText(m_ui->notesViewWidget->currentIndex().data(BookmarksModel::UrlRole).toString());
			}

			break;
		case ActionsManager::CutAction:
			triggerAction(ActionsManager::CopyAction);
			triggerAction(ActionsManager::DeleteAction);

			break;
		case ActionsManager::CopyAction:
			if (static_cast<BookmarksModel::BookmarkType>(m_ui->notesViewWidget->currentIndex().data(BookmarksModel::TypeRole).toInt()) == BookmarksModel::UrlBookmark)
			{
				QGuiApplication::clipboard()->setText(m_ui->notesViewWidget->currentIndex().data(BookmarksModel::DescriptionRole).toString());
			}

			break;
		case ActionsManager::PasteAction:
			if (!QGuiApplication::clipboard()->text().isEmpty())
			{
				BookmarksItem *bookmark(NotesManager::addNote(BookmarksModel::UrlBookmark, {{BookmarksModel::DescriptionRole, QGuiApplication::clipboard()->text()}}, findFolder(m_ui->notesViewWidget->currentIndex())));
			}

			break;
		case ActionsManager::DeleteAction:
			removeNote();

			break;
		case ActionsManager::FindAction:
		case ActionsManager::QuickFindAction:
			m_ui->filterLineEdit->setFocus();

			break;
		case ActionsManager::ActivateContentAction:
			m_ui->notesViewWidget->setFocus();

			break;
		default:
			ContentsWidget::triggerAction(identifier, parameters);

			break;
	}
}

void NotesContentsWidget::updateActions(bool updateText)
{
	const bool hasSelecion(!m_ui->notesViewWidget->selectionModel()->selectedIndexes().isEmpty());
	const QModelIndex index(m_ui->notesViewWidget->getCurrentIndex());
	const BookmarksModel::BookmarkType type(static_cast<BookmarksModel::BookmarkType>(index.data(BookmarksModel::TypeRole).toInt()));

	m_ui->addressLabelWidget->setText((type == BookmarksModel::UrlBookmark) ? index.data(BookmarksModel::UrlRole).toString() : QString());
	m_ui->addressLabelWidget->setUrl((type == BookmarksModel::UrlBookmark) ? index.data(BookmarksModel::UrlRole).toUrl() : QUrl());
	m_ui->dateLabelWidget->setText((type == BookmarksModel::UrlBookmark) ? Utils::formatDateTime(index.data(BookmarksModel::TimeAddedRole).toDateTime()) : QString());
	m_ui->deleteButton->setEnabled(hasSelecion && type != BookmarksModel::RootBookmark && type != BookmarksModel::TrashBookmark);

	if (updateText)
	{
		disconnect(m_ui->textEdit, SIGNAL(textChanged()), this, SLOT(updateText()));

		m_ui->textEdit->setPlainText(index.data(BookmarksModel::DescriptionRole).toString());

		connect(m_ui->textEdit, SIGNAL(textChanged()), this, SLOT(updateText()));
	}

	emit actionsStateChanged(ActionsManager::ActionDefinition::EditingCategory);
}

void NotesContentsWidget::updateText()
{
	const QModelIndex index(m_ui->notesViewWidget->getCurrentIndex());

	disconnect(m_ui->notesViewWidget, SIGNAL(needsActionsUpdate()), this, SLOT(updateActions()));

	if (index.isValid() && static_cast<BookmarksModel::BookmarkType>(index.data(BookmarksModel::TypeRole).toInt()) == BookmarksModel::UrlBookmark)
	{
		m_ui->notesViewWidget->model()->setData(index, m_ui->textEdit->toPlainText(), BookmarksModel::DescriptionRole);
		m_ui->notesViewWidget->model()->setData(index, QDateTime::currentDateTime(), BookmarksModel::TimeModifiedRole);
	}
	else
	{
		const BookmarksItem *bookmark(NotesManager::addNote(BookmarksModel::UrlBookmark, {{BookmarksModel::DescriptionRole, m_ui->textEdit->toPlainText()}}, findFolder(index)));

		if (bookmark)
		{
			m_ui->notesViewWidget->setCurrentIndex(bookmark->index());
		}

		updateActions(false);
	}

	connect(m_ui->notesViewWidget, SIGNAL(needsActionsUpdate()), this, SLOT(updateActions()));
}

void NotesContentsWidget::print(QPrinter *printer)
{
	m_ui->notesViewWidget->render(printer);
}

BookmarksItem* NotesContentsWidget::findFolder(const QModelIndex &index)
{
	BookmarksItem *item(NotesManager::getModel()->getBookmark(index));

	if (!item || item == NotesManager::getModel()->getRootItem() || item == NotesManager::getModel()->getTrashItem())
	{
		return NotesManager::getModel()->getRootItem();
	}

	const BookmarksModel::BookmarkType type(static_cast<BookmarksModel::BookmarkType>(item->data(BookmarksModel::TypeRole).toInt()));

	return ((type == BookmarksModel::RootBookmark || type == BookmarksModel::FolderBookmark) ? item : static_cast<BookmarksItem*>(item->parent()));
}

QString NotesContentsWidget::getTitle() const
{
	return tr("Notes");
}

QLatin1String NotesContentsWidget::getType() const
{
	return QLatin1String("notes");
}

QUrl NotesContentsWidget::getUrl() const
{
	return QUrl(QLatin1String("about:notes"));
}

QIcon NotesContentsWidget::getIcon() const
{
	return ThemesManager::createIcon(QLatin1String("notes"), false);
}

ActionsManager::ActionDefinition::State NotesContentsWidget::getActionState(int identifier, const QVariantMap &parameters) const
{
	ActionsManager::ActionDefinition::State state(ActionsManager::getActionDefinition(identifier).getDefaultState());

	switch (identifier)
	{
		case ActionsManager::CopyLinkToClipboardAction:
			state.text = QT_TRANSLATE_NOOP("actions", "Copy address of source page");
			state.isEnabled = (static_cast<BookmarksModel::BookmarkType>(m_ui->notesViewWidget->currentIndex().data(BookmarksModel::TypeRole).toInt()) == BookmarksModel::UrlBookmark && !m_ui->notesViewWidget->currentIndex().data(BookmarksModel::UrlRole).toString().isEmpty());

			return state;
		case ActionsManager::CutAction:
		case ActionsManager::CopyAction:
			state.isEnabled = m_ui->textEdit->textCursor().hasSelection();

			return state;
		case ActionsManager::PasteAction:
			state.isEnabled = m_ui->textEdit->canPaste();

			return state;
		case ActionsManager::DeleteAction:
			state.isEnabled = m_ui->deleteButton->isEnabled();

			return state;
		default:
			break;
	}

	return ContentsWidget::getActionState(identifier, parameters);
}

bool NotesContentsWidget::eventFilter(QObject *object, QEvent *event)
{
	if (object == m_ui->notesViewWidget->viewport() && event->type() == QEvent::MouseButtonRelease)
	{
		const QMouseEvent *mouseEvent(static_cast<QMouseEvent*>(event));

		if (mouseEvent && ((mouseEvent->button() == Qt::LeftButton && mouseEvent->modifiers() != Qt::NoModifier) || mouseEvent->button() == Qt::MiddleButton))
		{
			const BookmarksItem *bookmark(NotesManager::getModel()->getBookmark(m_ui->notesViewWidget->indexAt(mouseEvent->pos())));

			if (bookmark)
			{
				Application::triggerAction(ActionsManager::OpenBookmarkAction, {{QLatin1String("bookmark"), bookmark->data(BookmarksModel::IdentifierRole)}, {QLatin1String("hints"), QVariant(SessionsManager::calculateOpenHints(SessionsManager::NewTabOpen, mouseEvent->button(), mouseEvent->modifiers()))}}, parentWidget());

				return true;
			}
		}
	}
	else if (object == m_ui->notesViewWidget->viewport() && event->type() == QEvent::ToolTip)
	{
		const QHelpEvent *helpEvent(static_cast<QHelpEvent*>(event));

		if (helpEvent)
		{
			const QModelIndex index(m_ui->notesViewWidget->indexAt(helpEvent->pos()));
			const BookmarksItem *bookmark(NotesManager::getModel()->getBookmark(index));

			if (bookmark)
			{
				QToolTip::showText(helpEvent->globalPos(), QFontMetrics(QToolTip::font()).elidedText(bookmark->toolTip(), Qt::ElideRight, (QApplication::desktop()->screenGeometry(m_ui->notesViewWidget).width() / 2)), m_ui->notesViewWidget, m_ui->notesViewWidget->visualRect(index));
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
