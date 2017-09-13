/**************************************************************************
* Otter Browser: Web browser controlled by the user, not vice-versa.
* Copyright (C) 2013 - 2017 Michal Dutkiewicz aka Emdek <michal@emdek.pl>
* Copyright (C) 2015 Piotr Wójcik <chocimier@tlen.pl>
* Copyright (C) 2016 - 2017 Jan Bajer aka bajasoft <jbajer@gmail.com>
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

#include "Window.h"
#include "MainWindow.h"
#include "OpenAddressDialog.h"
#include "../core/Application.h"
#include "../core/HistoryManager.h"
#include "../core/SettingsManager.h"
#include "../core/Utils.h"
#include "../modules/widgets/address/AddressWidget.h"
#include "../modules/widgets/search/SearchWidget.h"
#include "../modules/windows/addons/AddonsContentsWidget.h"
#include "../modules/windows/bookmarks/BookmarksContentsWidget.h"
#include "../modules/windows/cache/CacheContentsWidget.h"
#include "../modules/windows/cookies/CookiesContentsWidget.h"
#include "../modules/windows/configuration/ConfigurationContentsWidget.h"
#include "../modules/windows/history/HistoryContentsWidget.h"
#include "../modules/windows/notes/NotesContentsWidget.h"
#include "../modules/windows/passwords/PasswordsContentsWidget.h"
#include "../modules/windows/transfers/TransfersContentsWidget.h"
#include "../modules/windows/web/WebContentsWidget.h"
#include "../modules/windows/windows/WindowsContentsWidget.h"

#include <QtCore/QTimer>
#include <QtGui/QPainter>
#include <QtWidgets/QBoxLayout>
#include <QtWidgets/QMdiSubWindow>

namespace Otter
{

quint64 Window::m_identifierCounter(0);

WindowToolBarWidget::WindowToolBarWidget(int identifier, Window *parent) : ToolBarWidget(identifier, parent, parent)
{
}

void WindowToolBarWidget::paintEvent(QPaintEvent *event)
{
	Q_UNUSED(event)

	QPainter painter(this);
	QStyleOptionToolBar toolBarOption;
	toolBarOption.initFrom(this);
	toolBarOption.lineWidth = style()->pixelMetric(QStyle::PM_ToolBarFrameWidth, nullptr, this);
	toolBarOption.positionOfLine = QStyleOptionToolBar::End;
	toolBarOption.positionWithinLine = QStyleOptionToolBar::OnlyOne;
	toolBarOption.state |= QStyle::State_Horizontal;
	toolBarOption.toolBarArea = Qt::TopToolBarArea;

	style()->drawControl(QStyle::CE_ToolBar, &toolBarOption, &painter, this);
}

Window::Window(const QVariantMap &parameters, ContentsWidget *widget, MainWindow *mainWindow) : QWidget(mainWindow->centralWidget()), ActionExecutor(),
	m_mainWindow(mainWindow),
	m_addressBar(nullptr),
	m_contentsWidget(nullptr),
	m_parameters(parameters),
	m_identifier(++m_identifierCounter),
	m_suspendTimer(0),
	m_isAboutToClose(false),
	m_isPinned(false)
{
	QBoxLayout *layout(new QBoxLayout(QBoxLayout::TopToBottom, this));
	layout->setContentsMargins(0, 0, 0, 0);
	layout->setSpacing(0);

	setLayout(layout);

	if (widget)
	{
		setContentsWidget(widget);
	}

	connect(this, SIGNAL(titleChanged(QString)), this, SLOT(setWindowTitle(QString)));
	connect(this, SIGNAL(iconChanged(QIcon)), this, SLOT(handleIconChanged(QIcon)));
}

void Window::timerEvent(QTimerEvent *event)
{
	if (event->timerId() == m_suspendTimer)
	{
		killTimer(m_suspendTimer);

		m_suspendTimer = 0;

		triggerAction(ActionsManager::SuspendTabAction);
	}
}

void Window::hideEvent(QHideEvent *event)
{
	QWidget::hideEvent(event);

	const int suspendTime(SettingsManager::getOption(SettingsManager::Browser_InactiveTabTimeUntilSuspendOption).toInt());

	if (suspendTime >= 0)
	{
		m_suspendTimer = startTimer(suspendTime * 1000);
	}
}

void Window::focusInEvent(QFocusEvent *event)
{
	QWidget::focusInEvent(event);

	if (m_suspendTimer > 0)
	{
		killTimer(m_suspendTimer);

		m_suspendTimer = 0;
	}

	AddressWidget *addressWidget(findAddressWidget());

	if (Utils::isUrlEmpty(getUrl()) && (!m_contentsWidget || m_contentsWidget->getLoadingState() != WebWidget::OngoingLoadingState) && addressWidget)
	{
		addressWidget->setFocus();
	}
	else if (m_contentsWidget)
	{
		m_contentsWidget->setFocus();
	}
}

void Window::triggerAction(int identifier, const QVariantMap &parameters)
{
	switch (identifier)
	{
		case ActionsManager::CloneTabAction:
			if (canClone())
			{
				m_mainWindow->addWindow(clone(true, m_mainWindow));
			}

			break;
		case ActionsManager::PinTabAction:
			setPinned(!isPinned());

			break;
		case ActionsManager::DetachTabAction:
			if (m_mainWindow->getWindowCount() > 1)
			{
				m_mainWindow->moveWindow(this);
			}

			break;
		case ActionsManager::MaximizeTabAction:
		case ActionsManager::MinimizeTabAction:
		case ActionsManager::RestoreTabAction:
		case ActionsManager::AlwaysOnTopTabAction:
			{
				QVariantMap mutableParameters(parameters);
				mutableParameters[QLatin1String("tab")] = m_identifier;

				m_mainWindow->triggerAction(identifier, mutableParameters);
			}

			break;
		case ActionsManager::SuspendTabAction:
			if (!m_contentsWidget || m_contentsWidget->close())
			{
				m_session = getSession();

				setContentsWidget(nullptr);
			}

			break;
		case ActionsManager::CloseTabAction:
			if (!isPinned())
			{
				requestClose();
			}

			break;
		case ActionsManager::GoAction:
		case ActionsManager::ActivateAddressFieldAction:
		case ActionsManager::ActivateSearchFieldAction:
			{
				AddressWidget *addressWidget(findAddressWidget());
				SearchWidget *searchWidget(nullptr);

				for (int i = 0; i < m_searchWidgets.count(); ++i)
				{
					if (m_searchWidgets.at(i) && m_searchWidgets.at(i)->isVisible())
					{
						searchWidget = m_searchWidgets.at(i);

						break;
					}
				}

				if (identifier == ActionsManager::ActivateSearchFieldAction && searchWidget)
				{
					searchWidget->activate(Qt::ShortcutFocusReason);
				}
				else if (addressWidget)
				{
					if (identifier == ActionsManager::ActivateAddressFieldAction)
					{
						addressWidget->activate(Qt::ShortcutFocusReason);
					}
					else if (identifier == ActionsManager::ActivateSearchFieldAction)
					{
						addressWidget->setText(QLatin1String("? "));
						addressWidget->activate(Qt::OtherFocusReason);
					}
					else if (identifier == ActionsManager::GoAction)
					{
						addressWidget->handleUserInput(addressWidget->text(), SessionsManager::CurrentTabOpen);

						return;
					}
				}
				else if (identifier == ActionsManager::ActivateAddressFieldAction || identifier == ActionsManager::ActivateSearchFieldAction)
				{
					OpenAddressDialog dialog(this);

					if (identifier == ActionsManager::ActivateSearchFieldAction)
					{
						dialog.setText(QLatin1String("? "));
					}

					connect(&dialog, SIGNAL(requestedLoadUrl(QUrl,SessionsManager::OpenHints)), this, SLOT(handleOpenUrlRequest(QUrl,SessionsManager::OpenHints)));
					connect(&dialog, SIGNAL(requestedOpenBookmark(BookmarksItem*,SessionsManager::OpenHints)), this, SIGNAL(requestedOpenBookmark(BookmarksItem*,SessionsManager::OpenHints)));
					connect(&dialog, SIGNAL(requestedSearch(QString,QString,SessionsManager::OpenHints)), this, SLOT(handleSearchRequest(QString,QString,SessionsManager::OpenHints)));

					dialog.exec();
				}
			}

			break;
		case ActionsManager::FullScreenAction:
			if (m_addressBar)
			{
				m_addressBar->setVisible(m_addressBar->shouldBeVisible(m_mainWindow->isFullScreen()));
			}

			if (m_contentsWidget)
			{
				m_contentsWidget->triggerAction(identifier, parameters);
			}

			break;
		default:
			getContentsWidget()->triggerAction(identifier, parameters);

			break;
	}
}

void Window::clear()
{
	if (!m_contentsWidget || m_contentsWidget->close())
	{
		setContentsWidget(new WebContentsWidget(m_parameters, QHash<int, QVariant>(), nullptr, this));

		m_isAboutToClose = false;

		emit urlChanged(getUrl(), true);
	}
}

void Window::attachAddressWidget(AddressWidget *widget)
{
	if (!m_addressWidgets.contains(widget))
	{
		m_addressWidgets.append(widget);

		if (widget->isVisible() && isActive() && Utils::isUrlEmpty(m_contentsWidget->getUrl()))
		{
			const AddressWidget *addressWidget(qobject_cast<AddressWidget*>(QApplication::focusWidget()));

			if (!addressWidget)
			{
				widget->setFocus();
			}
		}
	}
}

void Window::detachAddressWidget(AddressWidget *widget)
{
	m_addressWidgets.removeAll(widget);
}

void Window::attachSearchWidget(SearchWidget *widget)
{
	if (!m_searchWidgets.contains(widget))
	{
		m_searchWidgets.append(widget);
	}
}

void Window::detachSearchWidget(SearchWidget *widget)
{
	m_searchWidgets.removeAll(widget);
}

void Window::requestClose()
{
	if (!m_contentsWidget || m_contentsWidget->close())
	{
		m_isAboutToClose = true;

		emit aboutToClose();

		QTimer::singleShot(50, this, SLOT(notifyRequestedCloseWindow()));
	}
}

void Window::search(const QString &query, const QString &searchEngine)
{
	WebContentsWidget *widget(qobject_cast<WebContentsWidget*>(m_contentsWidget));

	if (!widget)
	{
		if (m_contentsWidget && !m_contentsWidget->close())
		{
			return;
		}

		QVariantMap parameters;

		if (isPrivate())
		{
			parameters[QLatin1String("hints")] = SessionsManager::PrivateOpen;
		}

		widget = new WebContentsWidget(parameters, QHash<int, QVariant>(), nullptr, this);

		setContentsWidget(widget);
	}

	widget->search(query, searchEngine);

	emit urlChanged(getUrl(), true);
}

void Window::markAsActive(bool updateLastActivity)
{
	if (m_suspendTimer > 0)
	{
		killTimer(m_suspendTimer);

		m_suspendTimer = 0;
	}

	if (!m_contentsWidget)
	{
		setUrl(m_session.getUrl(), false);
	}

	if (updateLastActivity)
	{
		m_lastActivity = QDateTime::currentDateTime();
	}

	emit activated();
}

void Window::handleIconChanged(const QIcon &icon)
{
	QMdiSubWindow *subWindow(qobject_cast<QMdiSubWindow*>(parentWidget()));

	if (subWindow)
	{
		subWindow->setWindowIcon(icon);
	}
}

void Window::handleOpenUrlRequest(const QUrl &url, SessionsManager::OpenHints hints)
{
	if (hints == SessionsManager::DefaultOpen || hints == SessionsManager::CurrentTabOpen)
	{
		setUrl(url);

		return;
	}

	if (isPrivate())
	{
		hints |= SessionsManager::PrivateOpen;
	}

	emit requestedOpenUrl(url, hints);
}

void Window::handleSearchRequest(const QString &query, const QString &searchEngine, SessionsManager::OpenHints hints)
{
	if ((getType() == QLatin1String("web") && Utils::isUrlEmpty(getUrl())) || (hints == SessionsManager::DefaultOpen || hints == SessionsManager::CurrentTabOpen))
	{
		search(query, searchEngine);
	}
	else
	{
		emit requestedSearch(query, searchEngine, hints);
	}
}

void Window::handleGeometryChangeRequest(const QRect &geometry)
{
	Application::triggerAction(ActionsManager::RestoreTabAction, {{QLatin1String("tab"), getIdentifier()}}, m_mainWindow);

	QMdiSubWindow *subWindow(qobject_cast<QMdiSubWindow*>(parentWidget()));

	if (subWindow)
	{
		subWindow->setWindowFlags(Qt::SubWindow);
		subWindow->showNormal();
		subWindow->resize(geometry.size() + (subWindow->geometry().size() - m_contentsWidget->size()));
		subWindow->move(geometry.topLeft());
	}
}

void Window::notifyRequestedCloseWindow()
{
	emit requestedCloseWindow(this);
}

void Window::updateNavigationBar()
{
	if (m_addressBar)
	{
		m_addressBar->reload();
	}
}

void Window::setSession(const SessionWindow &session, bool deferLoading)
{
	m_session = session;

	setPinned(session.isPinned);

	if (deferLoading)
	{
		setWindowTitle(session.getTitle());
	}
	else
	{
		setUrl(session.getUrl(), false);
	}
}

void Window::setOption(int identifier, const QVariant &value)
{
	if (m_contentsWidget)
	{
		m_contentsWidget->setOption(identifier, value);
	}
	else if (value != m_session.options.value(identifier))
	{
		if (value.isNull())
		{
			m_session.options.remove(identifier);
		}
		else
		{
			m_session.options[identifier] = value;
		}

		SessionsManager::markSessionModified();

		emit optionChanged(identifier, value);
	}
}

void Window::setUrl(const QUrl &url, bool isTyped)
{
	ContentsWidget *newWidget(nullptr);

	if (url.scheme() == QLatin1String("about"))
	{
		if (m_session.historyIndex < 0 && !Utils::isUrlEmpty(getUrl()) && SessionsManager::hasUrl(url, true))
		{
			emit urlChanged(url, true);

			return;
		}

		if (url.path() == QLatin1String("addons"))
		{
			newWidget = new AddonsContentsWidget(QVariantMap(), this);
		}
		else if (url.path() == QLatin1String("bookmarks"))
		{
			newWidget = new BookmarksContentsWidget(QVariantMap(), this);
		}
		else if (url.path() == QLatin1String("cache"))
		{
			newWidget = new CacheContentsWidget(QVariantMap(), this);
		}
		else if (url.path() == QLatin1String("config"))
		{
			newWidget = new ConfigurationContentsWidget(QVariantMap(), this);
		}
		else if (url.path() == QLatin1String("cookies"))
		{
			newWidget = new CookiesContentsWidget(QVariantMap(), this);
		}
		else if (url.path() == QLatin1String("history"))
		{
			newWidget = new HistoryContentsWidget(QVariantMap(), this);
		}
		else if (url.path() == QLatin1String("notes"))
		{
			newWidget = new NotesContentsWidget(QVariantMap(), this);
		}
		else if (url.path() == QLatin1String("passwords"))
		{
			newWidget = new PasswordsContentsWidget(QVariantMap(), this);
		}
		else if (url.path() == QLatin1String("transfers"))
		{
			newWidget = new TransfersContentsWidget(QVariantMap(), this);
		}
		else if (url.path() == QLatin1String("windows"))
		{
			newWidget = new WindowsContentsWidget(QVariantMap(), this);
		}

		if (newWidget && !newWidget->canClone())
		{
			SessionsManager::removeStoredUrl(newWidget->getUrl().toString());
		}
	}

	const bool isRestoring(!m_contentsWidget && m_session.historyIndex >= 0);

	if (!newWidget && (!m_contentsWidget || m_contentsWidget->getType() != QLatin1String("web")))
	{
		newWidget = new WebContentsWidget(m_parameters, m_session.options, nullptr, this);
	}

	if (newWidget)
	{
		if (m_contentsWidget && !m_contentsWidget->close())
		{
			return;
		}

		setContentsWidget(newWidget);
	}

	if (m_contentsWidget && url.isValid())
	{
		if (!isRestoring)
		{
			m_contentsWidget->setUrl(url, isTyped);
		}

		if (!Utils::isUrlEmpty(getUrl()) || m_contentsWidget->getLoadingState() == WebWidget::OngoingLoadingState)
		{
			emit urlChanged(url, true);
		}
	}
}

void Window::setZoom(int zoom)
{
	if (m_contentsWidget)
	{
		m_contentsWidget->setZoom(zoom);
	}
	else if (m_session.historyIndex >= 0 && m_session.historyIndex < m_session.history.count())
	{
		m_session.history[m_session.historyIndex].zoom = zoom;
	}
}

void Window::setPinned(bool isPinned)
{
	if (isPinned != m_isPinned)
	{
		m_isPinned = isPinned;

		emit actionsStateChanged(QVector<int>({ActionsManager::PinTabAction, ActionsManager::CloseTabAction}));
		emit isPinnedChanged(isPinned);
	}
}

void Window::setContentsWidget(ContentsWidget *widget)
{
	if (m_contentsWidget)
	{
		layout()->removeWidget(m_contentsWidget);

		m_contentsWidget->deleteLater();
	}

	m_contentsWidget = widget;

	if (!m_contentsWidget)
	{
		if (m_addressBar)
		{
			layout()->removeWidget(m_addressBar);

			m_addressBar->deleteLater();
			m_addressBar = nullptr;
		}

		emit actionsStateChanged();

		return;
	}

	m_contentsWidget->setParent(this);

	if (!m_addressBar)
	{
		m_addressBar = new WindowToolBarWidget(ToolBarsManager::AddressBar, this);
		m_addressBar->setVisible(m_addressBar->shouldBeVisible(m_mainWindow->isFullScreen()));

		layout()->addWidget(m_addressBar);
	}

	layout()->addWidget(m_contentsWidget);

	if (m_session.historyIndex >= 0 || !m_contentsWidget->getWebWidget() || m_contentsWidget->getWebWidget()->getRequestedUrl().isEmpty())
	{
		WindowHistoryInformation history;

		if (m_session.historyIndex >= 0)
		{
			history.index = m_session.historyIndex;
			history.entries = m_session.history;
		}

		m_contentsWidget->setHistory(history);
		m_contentsWidget->setZoom(m_session.getZoom());
	}

	if (isActive())
	{
		if (m_session.historyIndex >= 0)
		{
			m_contentsWidget->setFocus();
		}
		else
		{
			AddressWidget *addressWidget(findAddressWidget());

			if (Utils::isUrlEmpty(m_contentsWidget->getUrl()) && addressWidget)
			{
				QTimer::singleShot(100, addressWidget, SLOT(setFocus()));
			}
		}
	}

	m_session = SessionWindow();

	emit titleChanged(m_contentsWidget->getTitle());
	emit urlChanged(m_contentsWidget->getUrl());
	emit iconChanged(m_contentsWidget->getIcon());
	emit actionsStateChanged();
	emit loadingStateChanged(m_contentsWidget->getLoadingState());
	emit canZoomChanged(m_contentsWidget->canZoom());

	connect(m_contentsWidget, SIGNAL(aboutToNavigate()), this, SIGNAL(aboutToNavigate()));
	connect(m_contentsWidget, SIGNAL(needsAttention()), this, SIGNAL(needsAttention()));
	connect(m_contentsWidget, SIGNAL(requestedOpenUrl(QUrl,SessionsManager::OpenHints)), this, SIGNAL(requestedOpenUrl(QUrl,SessionsManager::OpenHints)));
	connect(m_contentsWidget, SIGNAL(requestedNewWindow(ContentsWidget*,SessionsManager::OpenHints)), this, SIGNAL(requestedNewWindow(ContentsWidget*,SessionsManager::OpenHints)));
	connect(m_contentsWidget, SIGNAL(requestedSearch(QString,QString,SessionsManager::OpenHints)), this, SIGNAL(requestedSearch(QString,QString,SessionsManager::OpenHints)));
	connect(m_contentsWidget, SIGNAL(requestedGeometryChange(QRect)), this, SLOT(handleGeometryChangeRequest(QRect)));
	connect(m_contentsWidget, SIGNAL(statusMessageChanged(QString)), this, SIGNAL(statusMessageChanged(QString)));
	connect(m_contentsWidget, SIGNAL(titleChanged(QString)), this, SIGNAL(titleChanged(QString)));
	connect(m_contentsWidget, SIGNAL(urlChanged(QUrl)), this, SIGNAL(urlChanged(QUrl)));
	connect(m_contentsWidget, SIGNAL(iconChanged(QIcon)), this, SIGNAL(iconChanged(QIcon)));
	connect(m_contentsWidget, SIGNAL(requestBlocked(NetworkManager::ResourceInformation)), this, SIGNAL(requestBlocked(NetworkManager::ResourceInformation)));
	connect(m_contentsWidget, SIGNAL(actionsStateChanged(QVector<int>)), this, SIGNAL(actionsStateChanged(QVector<int>)));
	connect(m_contentsWidget, SIGNAL(actionsStateChanged(ActionsManager::ActionDefinition::ActionCategories)), this, SIGNAL(actionsStateChanged(ActionsManager::ActionDefinition::ActionCategories)));
	connect(m_contentsWidget, SIGNAL(contentStateChanged(WebWidget::ContentStates)), this, SIGNAL(contentStateChanged(WebWidget::ContentStates)));
	connect(m_contentsWidget, SIGNAL(loadingStateChanged(WebWidget::LoadingState)), this, SIGNAL(loadingStateChanged(WebWidget::LoadingState)));
	connect(m_contentsWidget, SIGNAL(pageInformationChanged(WebWidget::PageInformation,QVariant)), this, SIGNAL(pageInformationChanged(WebWidget::PageInformation,QVariant)));
	connect(m_contentsWidget, SIGNAL(optionChanged(int,QVariant)), this, SIGNAL(optionChanged(int,QVariant)));
	connect(m_contentsWidget, SIGNAL(zoomChanged(int)), this, SIGNAL(zoomChanged(int)));
	connect(m_contentsWidget, SIGNAL(canZoomChanged(bool)), this, SIGNAL(canZoomChanged(bool)));
	connect(m_contentsWidget, SIGNAL(webWidgetChanged()), this, SLOT(updateNavigationBar()));
}

AddressWidget* Window::findAddressWidget() const
{
	for (int i = 0; i < m_addressWidgets.count(); ++i)
	{
		if (m_addressWidgets.at(i) && m_addressWidgets.at(i)->isVisible())
		{
			return m_addressWidgets.at(i);
		}
	}

	return m_addressWidgets.value(0, nullptr);
}

Window* Window::clone(bool cloneHistory, MainWindow *mainWindow) const
{
	if (!m_contentsWidget || !canClone())
	{
		return nullptr;
	}

	QVariantMap parameters({{QLatin1String("size"), size()}});

	if (isPrivate())
	{
		parameters[QLatin1String("hints")] = SessionsManager::PrivateOpen;
	}

	return new Window(parameters, m_contentsWidget->clone(cloneHistory), mainWindow);
}

MainWindow* Window::getMainWindow() const
{
	return m_mainWindow;
}

ContentsWidget* Window::getContentsWidget()
{
	if (!m_contentsWidget)
	{
		setUrl(m_session.getUrl(), false);
	}

	return m_contentsWidget;
}

WebWidget* Window::getWebWidget()
{
	if (!m_contentsWidget)
	{
		setUrl(m_session.getUrl(), false);
	}

	return m_contentsWidget->getWebWidget();
}

QString Window::getTitle() const
{
	return (m_contentsWidget ? m_contentsWidget->getTitle() : m_session.getTitle());
}

QLatin1String Window::getType() const
{
	return (m_contentsWidget ? m_contentsWidget->getType() : QLatin1String("unknown"));
}

QVariant Window::getOption(int identifier) const
{
	if (m_contentsWidget)
	{
		return m_contentsWidget->getOption(identifier);
	}

	return (m_session.options.contains(identifier) ? m_session.options[identifier] : SettingsManager::getOption(identifier, m_session.getUrl()));
}

QUrl Window::getUrl() const
{
	return (m_contentsWidget ? m_contentsWidget->getUrl() : m_session.getUrl());
}

QIcon Window::getIcon() const
{
	return (m_contentsWidget ? m_contentsWidget->getIcon() : HistoryManager::getIcon(m_session.getUrl()));
}

QPixmap Window::createThumbnail() const
{
	return (m_contentsWidget ? m_contentsWidget->createThumbnail() : QPixmap());
}

QDateTime Window::getLastActivity() const
{
	return m_lastActivity;
}

ActionsManager::ActionDefinition::State Window::getActionState(int identifier, const QVariantMap &parameters) const
{
	ActionsManager::ActionDefinition::State state(ActionsManager::getActionDefinition(identifier).getDefaultState());

	switch (identifier)
	{
		case ActionsManager::CloneTabAction:
			state.isEnabled = canClone();

			break;
		case ActionsManager::DetachTabAction:
			state.isEnabled = (m_mainWindow->getWindowCount() > 1);

			break;
		case ActionsManager::PinTabAction:
			state.text = (m_isPinned ? QT_TRANSLATE_NOOP("actions", "Unpin Tab") : QT_TRANSLATE_NOOP("actions", "Pin Tab"));

			break;
		case ActionsManager::CloseTabAction:
			state.isEnabled = !m_isPinned;

			break;
		case ActionsManager::GoToParentDirectoryAction:
			state.isEnabled = !Utils::isUrlEmpty(getUrl());

			break;
		case ActionsManager::MaximizeTabAction:
			state.isEnabled = (getWindowState().state != Qt::WindowMaximized);

			break;
		case ActionsManager::MinimizeTabAction:
			state.isEnabled = (getWindowState().state != Qt::WindowMinimized);

			break;
		case ActionsManager::RestoreTabAction:
			state.isEnabled = (getWindowState().state != Qt::WindowNoState);

			break;
		case ActionsManager::AlwaysOnTopTabAction:
			{
				const QMdiSubWindow *subWindow(qobject_cast<QMdiSubWindow*>(parentWidget()));

				if (subWindow)
				{
					state.isEnabled = subWindow->windowFlags().testFlag(Qt::WindowStaysOnTopHint);
				}
			}

			break;
		default:
			if (m_contentsWidget)
			{
				state = m_contentsWidget->getActionState(identifier, parameters);
			}

			break;
	}

	return state;
}

WindowHistoryInformation Window::getHistory() const
{
	if (m_contentsWidget)
	{
		return m_contentsWidget->getHistory();
	}

	WindowHistoryInformation history;
	history.entries = m_session.history;
	history.index = m_session.historyIndex;

	return history;
}

SessionWindow Window::getSession() const
{
	const QMdiSubWindow *subWindow(qobject_cast<QMdiSubWindow*>(parentWidget()));
	SessionWindow session;

	if (m_contentsWidget)
	{
		const WindowHistoryInformation history(m_contentsWidget->getHistory());

		if (m_contentsWidget->getType() == QLatin1String("web"))
		{
			WebContentsWidget *webWidget(qobject_cast<WebContentsWidget*>(m_contentsWidget));

			if (webWidget)
			{
				session.options = webWidget->getOptions();
			}
		}

		session.history = history.entries;
		session.parentGroup = 0;
		session.historyIndex = history.index;
		session.isPinned = isPinned();
	}
	else
	{
		session = m_session;
	}

	session.state = getWindowState();
	session.isAlwaysOnTop = (subWindow && subWindow->windowFlags().testFlag(Qt::WindowStaysOnTopHint));

	return session;
}

WindowState Window::getWindowState() const
{
	const QMdiSubWindow *subWindow(qobject_cast<QMdiSubWindow*>(parentWidget()));
	WindowState windowState;
	windowState.state = Qt::WindowMaximized;

	if (subWindow && !subWindow->isMaximized())
	{
		if (subWindow->isMinimized())
		{
			windowState.state = Qt::WindowMinimized;
		}
		else
		{
			windowState.geometry = subWindow->geometry();
			windowState.state = Qt::WindowNoState;
		}
	}

	return windowState;
}

QSize Window::sizeHint() const
{
	return QSize(800, 600);
}

WebWidget::LoadingState Window::getLoadingState() const
{
	return (m_contentsWidget ? m_contentsWidget->getLoadingState() : WebWidget::DeferredLoadingState);
}

WebWidget::ContentStates Window::getContentState() const
{
	return (m_contentsWidget ? m_contentsWidget->getContentState() : WebWidget::UnknownContentState);
}

quint64 Window::getIdentifier() const
{
	return m_identifier;
}

int Window::getZoom() const
{
	return (m_contentsWidget ? m_contentsWidget->getZoom() : m_session.getZoom());
}

bool Window::canClone() const
{
	return (m_contentsWidget ? m_contentsWidget->canClone() : false);
}

bool Window::canZoom() const
{
	return (m_contentsWidget ? m_contentsWidget->canZoom() : false);
}

bool Window::isAboutToClose() const
{
	return m_isAboutToClose;
}

bool Window::isActive() const
{
	return (isActiveWindow() && isAncestorOf(QApplication::focusWidget()));
}

bool Window::isPinned() const
{
	return m_isPinned;
}

bool Window::isPrivate() const
{
	return (m_contentsWidget ? m_contentsWidget->isPrivate() : SessionsManager::calculateOpenHints(m_parameters).testFlag(SessionsManager::PrivateOpen));
}

bool Window::event(QEvent *event)
{
	if (event->type() == QEvent::ParentChange)
	{
		QMdiSubWindow *subWindow(qobject_cast<QMdiSubWindow*>(parentWidget()));

		if (subWindow)
		{
			connect(subWindow, &QMdiSubWindow::windowStateChanged, [&](Qt::WindowStates oldState, Qt::WindowStates newState)
			{
				Q_UNUSED(oldState)
				Q_UNUSED(newState)

				emit actionsStateChanged(QVector<int>({ActionsManager::MaximizeTabAction, ActionsManager::MinimizeTabAction, ActionsManager::RestoreTabAction}));
			});
		}
	}

	return QWidget::event(event);
}

}
