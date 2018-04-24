/**************************************************************************
* Otter Browser: Web browser controlled by the user, not vice-versa.
* Copyright (C) 2013 - 2018 Michal Dutkiewicz aka Emdek <michal@emdek.pl>
* Copyright (C) 2014 - 2017 Jan Bajer aka bajasoft <jbajer@gmail.com>
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

#include "SearchWidget.h"
#include "../../../core/Job.h"
#include "../../../core/SearchEnginesManager.h"
#include "../../../core/SearchSuggester.h"
#include "../../../core/SettingsManager.h"
#include "../../../core/ThemesManager.h"
#include "../../../ui/MainWindow.h"
#include "../../../ui/ToolBarWidget.h"
#include "../../../ui/Window.h"

#include <QtGui/QPainter>
#include <QtWidgets/QApplication>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QToolTip>

namespace Otter
{

SearchDelegate::SearchDelegate(QObject *parent) : QItemDelegate(parent)
{
}

void SearchDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
	drawBackground(painter, option, index);

	if (index.data(Qt::AccessibleDescriptionRole).toString() == QLatin1String("separator"))
	{
		QStyleOptionFrame frameOption;
		frameOption.palette = option.palette;
		frameOption.palette.setCurrentColorGroup(QPalette::Disabled);
		frameOption.rect = option.rect.marginsRemoved(QMargins(3, 0, 3, 0));
		frameOption.state = QStyle::State_None;
		frameOption.frameShape = QFrame::HLine;

		QApplication::style()->drawControl(QStyle::CE_ShapedFrame, &frameOption, painter);

		return;
	}

	QRect titleRectangle(option.rect);

	if (index.data(Qt::DecorationRole).value<QIcon>().isNull())
	{
		drawDisplay(painter, option, titleRectangle, index.data(Qt::DisplayRole).toString());

		return;
	}

	QRect decorationRectangle(option.rect);
	const bool isRightToLeft(option.direction == Qt::RightToLeft);

	if (isRightToLeft)
	{
		decorationRectangle.setLeft(option.rect.width() - option.rect.height());
	}
	else
	{
		decorationRectangle.setRight(option.rect.height());
	}

	decorationRectangle = decorationRectangle.marginsRemoved(QMargins(2, 2, 2, 2));

	index.data(Qt::DecorationRole).value<QIcon>().paint(painter, decorationRectangle, option.decorationAlignment);

	if (isRightToLeft)
	{
		titleRectangle.setRight(option.rect.width() - option.rect.height());
	}
	else
	{
		titleRectangle.setLeft(option.rect.height());
	}

	if (index.data(Qt::AccessibleDescriptionRole).toString() == QLatin1String("configure"))
	{
		drawDisplay(painter, option, titleRectangle, index.data(Qt::DisplayRole).toString());

		return;
	}

	const int shortcutWidth((option.rect.width() > 150) ? 40 : 0);

	if (shortcutWidth > 0)
	{
		QRect shortcutReactangle(option.rect);

		if (isRightToLeft)
		{
			shortcutReactangle.setRight(shortcutWidth);

			titleRectangle.setLeft(shortcutWidth + 5);
		}
		else
		{
			shortcutReactangle.setLeft(option.rect.right() - shortcutWidth);

			titleRectangle.setRight(titleRectangle.right() - (shortcutWidth + 5));
		}

		drawDisplay(painter, option, shortcutReactangle, index.data(SearchEnginesManager::KeywordRole).toString());
	}

	drawDisplay(painter, option, titleRectangle, index.data(SearchEnginesManager::TitleRole).toString());
}

QSize SearchDelegate::sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const
{
	QSize size(index.data(Qt::SizeHintRole).toSize());

	if (index.data(Qt::AccessibleDescriptionRole).toString() == QLatin1String("separator"))
	{
		size.setHeight(qRound(option.fontMetrics.lineSpacing() * 0.75));
	}
	else
	{
		size.setHeight(qRound(option.fontMetrics.lineSpacing() * 1.25));
	}

	return size;
}

SearchWidget::SearchWidget(Window *window, QWidget *parent) : LineEditWidget(parent),
	m_window(nullptr),
	m_suggester(nullptr),
	m_hasAllWindowSearchEngines(true),
	m_isSearchEngineLocked(false)
{
	setMinimumWidth(100);
	setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
	handleOptionChanged(SettingsManager::AddressField_DropActionOption, SettingsManager::getOption(SettingsManager::AddressField_DropActionOption));
	handleOptionChanged(SettingsManager::AddressField_SelectAllOnFocusOption, SettingsManager::getOption(SettingsManager::AddressField_SelectAllOnFocusOption));
	handleOptionChanged(SettingsManager::Search_SearchEnginesSuggestionsOption, SettingsManager::getOption(SettingsManager::Search_SearchEnginesSuggestionsOption));

	const ToolBarWidget *toolBar(qobject_cast<ToolBarWidget*>(parent));

	if (toolBar && toolBar->getDefinition().isGlobal())
	{
		connect(toolBar, &ToolBarWidget::windowChanged, this, &SearchWidget::setWindow);
	}

	connect(SearchEnginesManager::getInstance(), &SearchEnginesManager::searchEnginesModified, this, &SearchWidget::storeCurrentSearchEngine);
	connect(SearchEnginesManager::getInstance(), &SearchEnginesManager::searchEnginesModelModified, this, &SearchWidget::restoreCurrentSearchEngine);
	connect(SettingsManager::getInstance(), &SettingsManager::optionChanged, this, &SearchWidget::handleOptionChanged);
	connect(this, &SearchWidget::textChanged, this, &SearchWidget::setQuery);
	connect(this, &SearchWidget::textDropped, this, &SearchWidget::sendRequest);
	connect(this, &SearchWidget::popupClicked, this, [&](const QModelIndex &index)
	{
		if (m_suggester && getPopup()->model() == m_suggester->getModel())
		{
			setText(index.data(Qt::DisplayRole).toString());
			sendRequest();
			hidePopup();
		}
		else
		{
			setSearchEngine(index);
		}
	});

	setWindow(window);
}

void SearchWidget::changeEvent(QEvent *event)
{
	LineEditWidget::changeEvent(event);

	switch (event->type())
	{
		case QEvent::LanguageChange:
			{
				const QString title(SearchEnginesManager::getSearchEngine(m_searchEngine).title);

				setToolTip(tr("Search using %1").arg(title));
				setPlaceholderText(tr("Search using %1").arg(title));
			}

			break;
		case QEvent::LayoutDirectionChange:
			updateGeometries();

			break;
		default:
			break;
	}
}

void SearchWidget::paintEvent(QPaintEvent *event)
{
	LineEditWidget::paintEvent(event);

	QPainter painter(this);

	if (isEnabled())
	{
		painter.drawPixmap(m_iconRectangle, SearchEnginesManager::getSearchEngine(m_searchEngine).icon.pixmap(m_iconRectangle.size()));

		QStyleOption arrow;
		arrow.initFrom(this);
		arrow.rect = m_dropdownArrowRectangle;

		style()->drawPrimitive(QStyle::PE_IndicatorArrowDown, &arrow, &painter, this);
	}

	if (m_addButtonRectangle.isValid())
	{
		painter.drawPixmap(m_addButtonRectangle, ThemesManager::createIcon(QLatin1String("list-add")).pixmap(m_addButtonRectangle.size(), (isEnabled() ? QIcon::Active : QIcon::Disabled)));
	}

	if (m_searchButtonRectangle.isValid())
	{
		painter.drawPixmap(m_searchButtonRectangle, ThemesManager::createIcon(QLatin1String("edit-find")).pixmap(m_searchButtonRectangle.size(), (isEnabled() ? QIcon::Active : QIcon::Disabled)));
	}
}

void SearchWidget::resizeEvent(QResizeEvent *event)
{
	LineEditWidget::resizeEvent(event);

	updateGeometries();
}

void SearchWidget::focusInEvent(QFocusEvent *event)
{
	LineEditWidget::focusInEvent(event);

	activate(event->reason());
}

void SearchWidget::keyPressEvent(QKeyEvent *event)
{
	switch (event->key())
	{
		case Qt::Key_Enter:
		case Qt::Key_Return:
			if (isPopupVisible() && getPopup()->getCurrentIndex().isValid())
			{
				sendRequest(getPopup()->getCurrentIndex().data(Qt::DisplayRole).toString());
			}
			else
			{
				sendRequest(text().trimmed());
			}

			hidePopup();

			event->accept();

			return;
		case Qt::Key_Down:
		case Qt::Key_Up:
			if (!m_isSearchEngineLocked && !isPopupVisible())
			{
				showSearchEngines();
			}

			break;
		default:
			break;
	}

	LineEditWidget::keyPressEvent(event);
}

void SearchWidget::mouseMoveEvent(QMouseEvent *event)
{
	if (m_dropdownArrowRectangle.united(m_iconRectangle).contains(event->pos()) || m_addButtonRectangle.contains(event->pos()) || m_searchButtonRectangle.contains(event->pos()))
	{
		setCursor(Qt::ArrowCursor);
	}
	else
	{
		setCursor(Qt::IBeamCursor);
	}

	LineEditWidget::mouseMoveEvent(event);
}

void SearchWidget::mouseReleaseEvent(QMouseEvent *event)
{
	if (event->button() == Qt::LeftButton)
	{
		if (m_addButtonRectangle.contains(event->pos()))
		{
			QMenu menu(this);
			const QVector<WebWidget::LinkUrl> searchEngines((m_window && m_window->getWebWidget()) ? m_window->getWebWidget()->getSearchEngines() : QVector<WebWidget::LinkUrl>());

			for (int i = 0; i < searchEngines.count(); ++i)
			{
				if (!SearchEnginesManager::hasSearchEngine(searchEngines.at(i).url))
				{
					menu.addAction(tr("Add %1").arg(searchEngines.at(i).title.isEmpty() ? tr("(untitled)") : searchEngines.at(i).title))->setData(searchEngines.at(i).url);
				}
			}

			connect(&menu, &QMenu::triggered, this, &SearchWidget::addSearchEngine);

			menu.exec(mapToGlobal(m_addButtonRectangle.bottomLeft()));
		}
		else if (m_searchButtonRectangle.contains(event->pos()))
		{
			sendRequest();
		}
		else if (!m_isSearchEngineLocked && !isPopupVisible() && m_dropdownArrowRectangle.united(m_iconRectangle).contains(event->pos()))
		{
			showSearchEngines();
		}
	}

	LineEditWidget::mouseReleaseEvent(event);
}

void SearchWidget::wheelEvent(QWheelEvent *event)
{
	LineEditWidget::wheelEvent(event);

	if (m_isSearchEngineLocked)
	{
		return;
	}

	const QStandardItemModel *model(SearchEnginesManager::getSearchEnginesModel());
	int row(getCurrentIndex().row());

	for (int i = 0; i < model->rowCount(); ++i)
	{
		if (event->delta() > 0)
		{
			if (row == 0)
			{
				row = (model->rowCount() - 1);
			}
			else
			{
				--row;
			}
		}
		else
		{
			if (row == (model->rowCount() - 1))
			{
				row = 0;
			}
			else
			{
				++row;
			}
		}

		const QModelIndex index(model->index(row, 0));

		if (index.data(Qt::AccessibleDescriptionRole).toString().isEmpty())
		{
			setSearchEngine(index, false);

			break;
		}
	}
}

void SearchWidget::showSearchEngines()
{
	PopupViewWidget *popupWidget(getPopup());
	popupWidget->setModel(SearchEnginesManager::getSearchEnginesModel());
	popupWidget->setItemDelegate(new SearchDelegate(this));

	showPopup();

	popupWidget->setCurrentIndex(getCurrentIndex());
}

void SearchWidget::showSearchSuggestions()
{
	if (m_suggester->getModel()->rowCount() > 0)
	{
		getPopup()->setModel(m_suggester->getModel());

		showPopup();
	}
}

void SearchWidget::sendRequest(const QString &query)
{
	if (!query.isEmpty())
	{
		m_query = query;
	}

	if (m_query.isEmpty())
	{
		const SearchEnginesManager::SearchEngineDefinition searchEngine(SearchEnginesManager::getSearchEngine(m_searchEngine));

		if (searchEngine.formUrl.isValid())
		{
			MainWindow *mainWindow(m_window ? MainWindow::findMainWindow(m_window) : MainWindow::findMainWindow(this));

			if (mainWindow)
			{
				mainWindow->triggerAction(ActionsManager::OpenUrlAction, {{QLatin1String("url"), searchEngine.formUrl}, {QLatin1String("hints"), QVariant(SessionsManager::calculateOpenHints())}});
			}
		}
	}
	else
	{
		emit requestedSearch(m_query, m_searchEngine, SessionsManager::calculateOpenHints());
	}
}

void SearchWidget::addSearchEngine(QAction *action)
{
	if (action)
	{
		SearchEngineFetchJob *job(new SearchEngineFetchJob(action->data().toUrl(), {}, true, this));

		connect(job, &SearchEngineFetchJob::jobFinished, [&](bool isSuccess)
		{
			if (!isSuccess)
			{
				QMessageBox::warning(this, tr("Error"), tr("Failed to add search engine."), QMessageBox::Close);
			}
		});
	}
}

void SearchWidget::storeCurrentSearchEngine()
{
	hidePopup();

	disconnect(this, &SearchWidget::textChanged, this, &SearchWidget::setQuery);
}

void SearchWidget::restoreCurrentSearchEngine()
{
	if (!m_searchEngine.isEmpty())
	{
		setSearchEngine(m_searchEngine);

		m_searchEngine.clear();
	}

	updateGeometries();
	setText(m_query);

	connect(this, &SearchWidget::textChanged, this, &SearchWidget::setQuery);
}

void SearchWidget::handleOptionChanged(int identifier, const QVariant &value)
{
	switch (identifier)
	{
		case SettingsManager::AddressField_DropActionOption:
			{
				const QString dropAction(value.toString());

				if (dropAction == QLatin1String("pasteAndGo"))
				{
					setDropMode(LineEditWidget::ReplaceAndNotifyDropMode);
				}
				else if (dropAction == QLatin1String("replace"))
				{
					setDropMode(LineEditWidget::ReplaceDropMode);
				}
				else
				{
					setDropMode(LineEditWidget::PasteDropMode);
				}
			}

			break;
		case SettingsManager::AddressField_SelectAllOnFocusOption:
			setSelectAllOnFocus(value.toBool());

			break;
		case SettingsManager::Search_SearchEnginesSuggestionsOption:
			if (value.toBool() && !m_suggester)
			{
				m_suggester = new SearchSuggester(m_searchEngine, this);

				connect(this, &SearchWidget::textEdited, m_suggester, &SearchSuggester::setQuery);
				connect(m_suggester, &SearchSuggester::suggestionsChanged, this, &SearchWidget::showSearchSuggestions);
			}
			else if (!value.toBool() && m_suggester)
			{
				m_suggester->deleteLater();
				m_suggester = nullptr;

				disconnect(m_suggester, &SearchSuggester::suggestionsChanged, this, &SearchWidget::showSearchSuggestions);
			}

			break;
		default:
			break;
	}
}

void SearchWidget::handleWindowOptionChanged(int identifier, const QVariant &value)
{
	if (identifier == SettingsManager::Search_DefaultSearchEngineOption)
	{
		setSearchEngine(value.toString());
	}
}

void SearchWidget::handleLoadingStateChanged()
{
	const QVector<WebWidget::LinkUrl> searchEngines((m_window && m_window->getWebWidget()) ? m_window->getWebWidget()->getSearchEngines() : QVector<WebWidget::LinkUrl>());
	bool hasAllSearchEngines(true);

	for (int i = 0; i < searchEngines.count(); ++i)
	{
		if (!SearchEnginesManager::hasSearchEngine(searchEngines.at(i).url))
		{
			hasAllSearchEngines = false;

			break;
		}
	}

	if (m_hasAllWindowSearchEngines != hasAllSearchEngines)
	{
		m_hasAllWindowSearchEngines = hasAllSearchEngines;

		updateGeometries();
	}
}

void SearchWidget::updateGeometries()
{
	QStyleOptionFrame panel;
	panel.initFrom(this);
	panel.rect = rect();
	panel.lineWidth = 1;

	QMargins margins(qMax(((height() - 16) / 2), 2), 0, 2, 0);
	const bool isRightToLeft(layoutDirection() == Qt::RightToLeft);

	m_searchButtonRectangle = {};
	m_addButtonRectangle = {};
	m_dropdownArrowRectangle = {};

	if (isRightToLeft)
	{
		m_iconRectangle = QRect((width() - margins.right() - 20), ((height() - 16) / 2), 16, 16);

		margins.setRight(margins.right() + 20);
	}
	else
	{
		m_iconRectangle = QRect(margins.left(), ((height() - 16) / 2), 16, 16);

		margins.setLeft(margins.left() + 20);
	}

	if (!m_isSearchEngineLocked)
	{
		if (isRightToLeft)
		{
			m_dropdownArrowRectangle = QRect((width() - margins.right() - 14), 0, 14, height());

			margins.setRight(margins.right() + 12);
		}
		else
		{
			m_dropdownArrowRectangle = QRect(margins.left(), 0, 14, height());

			margins.setLeft(margins.left() + 12);
		}
	}

	if (m_options.value(QLatin1String("showSearchButton"), true).toBool())
	{
		if (isRightToLeft)
		{
			m_searchButtonRectangle = QRect(margins.left(), ((height() - 16) / 2), 16, 16);

			margins.setLeft(margins.left() + 20);
		}
		else
		{
			m_searchButtonRectangle = QRect((width() - margins.right() - 20), ((height() - 16) / 2), 16, 16);

			margins.setRight(margins.right() + 20);
		}
	}

	if (!m_hasAllWindowSearchEngines && rect().marginsRemoved(margins).width() > 50)
	{
		if (isRightToLeft)
		{
			m_addButtonRectangle = QRect(margins.left(), ((height() - 16) / 2), 16, 16);

			margins.setLeft(margins.left() + 20);
		}
		else
		{
			m_addButtonRectangle = QRect((width() - margins.right() - 20), ((height() - 16) / 2), 16, 16);

			margins.setRight(margins.right() + 20);
		}
	}

	setTextMargins(margins);
}

void SearchWidget::setSearchEngine(const QString &searchEngine)
{
	if (m_isSearchEngineLocked && searchEngine != m_options.value(QLatin1String("searchEngine")).toString())
	{
		return;
	}

	const QStringList searchEngines(SearchEnginesManager::getSearchEngines());

	if (searchEngines.isEmpty())
	{
		m_searchEngine.clear();

		hidePopup();
		setEnabled(false);
		setToolTip({});
		setPlaceholderText({});

		return;
	}

	m_searchEngine = (searchEngines.contains(searchEngine) ? searchEngine : QString());

	setSearchEngine(getCurrentIndex(), false);

	if (m_suggester)
	{
		m_suggester->setSearchEngine(m_searchEngine);
	}
}

void SearchWidget::setSearchEngine(const QModelIndex &index, bool canSendRequest)
{
	if (index.data(Qt::AccessibleDescriptionRole).toString().isEmpty())
	{
		m_searchEngine = index.data(SearchEnginesManager::IdentifierRole).toString();

		if (m_window && !m_isSearchEngineLocked)
		{
			m_window->setOption(SettingsManager::Search_DefaultSearchEngineOption, m_searchEngine);
		}

		const QString title(index.data(SearchEnginesManager::TitleRole).toString());

		setToolTip(tr("Search using %1").arg(title));
		setPlaceholderText(tr("Search using %1").arg(title));
		setText(m_query);

		if (m_suggester)
		{
			m_suggester->setSearchEngine(m_searchEngine);
			m_suggester->setQuery({});
		}

		if (canSendRequest && !m_query.isEmpty())
		{
			sendRequest();
		}
	}
	else
	{
		const QString query(m_query);

		if (query != index.data(Qt::DisplayRole).toString())
		{
			setText(query);
		}
	}

	update();
	setEnabled(true);
	hidePopup();

	if (index.data(Qt::AccessibleDescriptionRole).toString() == QLatin1String("configure"))
	{
		MainWindow *mainWindow(m_window ? MainWindow::findMainWindow(m_window) : MainWindow::findMainWindow(this));

		if (mainWindow)
		{
			mainWindow->triggerAction(ActionsManager::PreferencesAction, {{QLatin1String("page"), QLatin1String("search")}});
		}
	}
}

void SearchWidget::setOptions(const QVariantMap &options)
{
	m_options = options;

	if (m_options.contains(QLatin1String("searchEngine")))
	{
		m_isSearchEngineLocked = true;

		setSearchEngine(m_options[QLatin1String("searchEngine")].toString());
	}
	else
	{
		m_isSearchEngineLocked = false;
	}

	resize(size());
}

void SearchWidget::setQuery(const QString &query)
{
	m_query = query;

	if (m_query.isEmpty() || getPopup()->model() == SearchEnginesManager::getSearchEnginesModel())
	{
		hidePopup();
	}
}

void SearchWidget::setWindow(Window *window)
{
	const MainWindow *mainWindow(MainWindow::findMainWindow(this));

	if (m_window && !m_window->isAboutToClose() && (!sender() || sender() != m_window))
	{
		m_window->detachSearchWidget(this);

		disconnect(this, &SearchWidget::requestedSearch, m_window.data(), &Window::requestedSearch);
		disconnect(m_window.data(), &Window::loadingStateChanged, this, &SearchWidget::handleLoadingStateChanged);
		disconnect(m_window.data(), &Window::optionChanged, this, &SearchWidget::handleWindowOptionChanged);
	}

	m_window = window;

	if (window)
	{
		if (mainWindow)
		{
			disconnect(this, &SearchWidget::requestedSearch, mainWindow, &MainWindow::search);
		}

		window->attachSearchWidget(this);

		setSearchEngine(window->getOption(SettingsManager::Search_DefaultSearchEngineOption).toString());

		connect(this, &SearchWidget::requestedSearch, window, &Window::requestedSearch);
		connect(window, &Window::loadingStateChanged, this, &SearchWidget::handleLoadingStateChanged);
		connect(window, &Window::optionChanged, this, &SearchWidget::handleWindowOptionChanged);
		connect(window, &Window::destroyed, this, [&](QObject *object)
		{
			if (qobject_cast<Window*>(object) == m_window)
			{
				setWindow(nullptr);
			}
		});

		const ToolBarWidget *toolBar(qobject_cast<ToolBarWidget*>(parentWidget()));

		if (!toolBar || toolBar->getDefinition().isGlobal())
		{
			connect(window, &Window::aboutToClose, this, [&]()
			{
				setWindow(nullptr);
			});
		}
	}
	else
	{
		if (mainWindow && !mainWindow->isAboutToClose())
		{
			connect(this, &SearchWidget::requestedSearch, mainWindow, &MainWindow::search);
		}

		setSearchEngine(SettingsManager::getOption(SettingsManager::Search_DefaultSearchEngineOption).toString());
	}

	updateGeometries();
}

QModelIndex SearchWidget::getCurrentIndex() const
{
	QString searchEngine(m_searchEngine);

	if (m_searchEngine.isEmpty())
	{
		searchEngine = (m_window ? m_window->getOption(SettingsManager::Search_DefaultSearchEngineOption) : SettingsManager::getOption(SettingsManager::Search_DefaultSearchEngineOption)).toString();
	}

	return SearchEnginesManager::getSearchEnginesModel()->index(qMax(0, SearchEnginesManager::getSearchEngines().indexOf(searchEngine)), 0);
}

QVariantMap SearchWidget::getOptions() const
{
	return m_options;
}

bool SearchWidget::event(QEvent *event)
{
	if (isEnabled() && event->type() == QEvent::ToolTip)
	{
		const QHelpEvent *helpEvent(static_cast<QHelpEvent*>(event));

		if (helpEvent)
		{
			if (m_iconRectangle.contains(helpEvent->pos()) || m_dropdownArrowRectangle.contains(helpEvent->pos()))
			{
				QToolTip::showText(helpEvent->globalPos(), tr("Select Search Engine"));

				return true;
			}

			if (m_addButtonRectangle.contains(helpEvent->pos()))
			{
				QToolTip::showText(helpEvent->globalPos(), tr("Add Search Engine…"));

				return true;
			}

			if (m_searchButtonRectangle.contains(helpEvent->pos()))
			{
				QToolTip::showText(helpEvent->globalPos(), tr("Search"));

				return true;
			}
		}
	}

	return LineEditWidget::event(event);
}

}
