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

#include "QtWebEngineWebWidget.h"
#include "QtWebEnginePage.h"
#include "../../../../core/Application.h"
#include "../../../../core/BookmarksManager.h"
#include "../../../../core/Console.h"
#include "../../../../core/GesturesManager.h"
#include "../../../../core/NetworkManager.h"
#include "../../../../core/NetworkManagerFactory.h"
#include "../../../../core/NotesManager.h"
#include "../../../../core/SearchEnginesManager.h"
#include "../../../../core/ThemesManager.h"
#include "../../../../core/TransfersManager.h"
#include "../../../../core/UserScript.h"
#include "../../../../core/Utils.h"
#include "../../../../core/WebBackend.h"
#include "../../../../ui/AuthenticationDialog.h"
#include "../../../../ui/ContentsDialog.h"
#include "../../../../ui/ContentsWidget.h"
#include "../../../../ui/ImagePropertiesDialog.h"
#include "../../../../ui/MainWindow.h"
#include "../../../../ui/SearchEnginePropertiesDialog.h"
#include "../../../../ui/SourceViewerWebWidget.h"
#include "../../../../ui/WebsitePreferencesDialog.h"

#include <QtCore/QEventLoop>
#include <QtCore/QFileInfo>
#include <QtCore/QMimeData>
#include <QtCore/QTimer>
#include <QtGui/QClipboard>
#include <QtGui/QContextMenuEvent>
#include <QtGui/QImageWriter>
#include <QtWebEngineCore/QWebEngineCookieStore>
#include <QtWebEngineWidgets/QWebEngineHistory>
#include <QtWebEngineWidgets/QWebEngineProfile>
#include <QtWebEngineWidgets/QWebEngineScript>
#include <QtWebEngineWidgets/QWebEngineSettings>
#include <QtWidgets/QApplication>
#include <QtWidgets/QToolTip>
#include <QtWidgets/QVBoxLayout>

namespace Otter
{

QtWebEngineWebWidget::QtWebEngineWebWidget(bool isPrivate, WebBackend *backend, ContentsWidget *parent) : WebWidget(isPrivate, backend, parent),
	m_webView(nullptr),
	m_page(new QtWebEnginePage(isPrivate, this)),
#if QT_VERSION < 0x050700
	m_iconReply(nullptr),
#endif
	m_loadingTime(nullptr),
	m_loadingState(FinishedLoadingState),
	m_documentLoadingProgress(0),
	m_focusProxyTimer(0),
#if QT_VERSION < 0x050700
	m_scrollTimer(startTimer(1000)),
#endif
	m_updateNavigationActionsTimer(0),
	m_isEditing(false),
	m_isFullScreen(false),
	m_isTyped(false)
{
	setFocusPolicy(Qt::StrongFocus);

	connect(m_page, SIGNAL(loadProgress(int)), this, SLOT(notifyDocumentLoadingProgress(int)));
	connect(m_page, SIGNAL(loadStarted()), this, SLOT(pageLoadStarted()));
	connect(m_page, SIGNAL(loadFinished(bool)), this, SLOT(pageLoadFinished()));
	connect(m_page, SIGNAL(linkHovered(QString)), this, SLOT(linkHovered(QString)));
#if QT_VERSION < 0x050700
	connect(m_page, SIGNAL(iconUrlChanged(QUrl)), this, SLOT(handleIconChange(QUrl)));
#else
	connect(m_page, SIGNAL(iconChanged(QIcon)), this, SIGNAL(iconChanged(QIcon)));
#endif
	connect(m_page, SIGNAL(requestedPopupWindow(QUrl,QUrl)), this, SIGNAL(requestedPopupWindow(QUrl,QUrl)));
	connect(m_page, SIGNAL(aboutToNavigate(QUrl,QWebEnginePage::NavigationType)), this, SIGNAL(aboutToNavigate()));
	connect(m_page, SIGNAL(requestedNewWindow(WebWidget*,SessionsManager::OpenHints)), this, SIGNAL(requestedNewWindow(WebWidget*,SessionsManager::OpenHints)));
	connect(m_page, SIGNAL(authenticationRequired(QUrl,QAuthenticator*)), this, SLOT(handleAuthenticationRequired(QUrl,QAuthenticator*)));
	connect(m_page, SIGNAL(proxyAuthenticationRequired(QUrl,QAuthenticator*,QString)), this, SLOT(handleProxyAuthenticationRequired(QUrl,QAuthenticator*,QString)));
	connect(m_page, SIGNAL(windowCloseRequested()), this, SLOT(handleWindowCloseRequest()));
	connect(m_page, SIGNAL(fullScreenRequested(QWebEngineFullScreenRequest)), this, SLOT(handleFullScreenRequest(QWebEngineFullScreenRequest)));
	connect(m_page, SIGNAL(featurePermissionRequested(QUrl,QWebEnginePage::Feature)), this, SLOT(handlePermissionRequest(QUrl,QWebEnginePage::Feature)));
	connect(m_page, SIGNAL(featurePermissionRequestCanceled(QUrl,QWebEnginePage::Feature)), this, SLOT(handlePermissionCancel(QUrl,QWebEnginePage::Feature)));
#if QT_VERSION >= 0x050700
	connect(m_page, SIGNAL(recentlyAudibleChanged(bool)), this, SIGNAL(isAudibleChanged(bool)));
#endif
	connect(m_page, SIGNAL(viewingMediaChanged(bool)), this, SLOT(notifyNavigationActionsChanged()));
	connect(m_page, SIGNAL(titleChanged(QString)), this, SLOT(notifyTitleChanged()));
	connect(m_page, SIGNAL(urlChanged(QUrl)), this, SLOT(notifyUrlChanged(QUrl)));
#if QT_VERSION < 0x050700
	connect(m_page, SIGNAL(iconUrlChanged(QUrl)), this, SLOT(notifyIconChanged()));
#endif
	connect(m_page->action(QWebEnginePage::Redo), &QAction::changed, this, &QtWebEngineWebWidget::notifyRedoActionStateChanged);
	connect(m_page->action(QWebEnginePage::Undo), &QAction::changed, this, &QtWebEngineWebWidget::notifyUndoActionStateChanged);
}

void QtWebEngineWebWidget::timerEvent(QTimerEvent *event)
{
	if (event->timerId() == m_focusProxyTimer)
	{
		if (focusWidget())
		{
			focusWidget()->removeEventFilter(this);
			focusWidget()->installEventFilter(this);
		}
	}
#if QT_VERSION < 0x050700
	else if (event->timerId() == m_scrollTimer)
	{
		m_page->runJavaScript(QLatin1String("[window.scrollX, window.scrollY]"), [&](const QVariant &result)
		{
			if (result.isValid())
			{
				m_scrollPosition = QPoint(result.toList()[0].toInt(), result.toList()[1].toInt());
			}
		});
	}
#endif
	else if (event->timerId() == m_updateNavigationActionsTimer)
	{
		killTimer(m_updateNavigationActionsTimer);

		m_updateNavigationActionsTimer = 0;

		emit actionsStateChanged(ActionsManager::ActionDefinition::NavigationCategory);
	}
	else
	{
		WebWidget::timerEvent(event);
	}
}

void QtWebEngineWebWidget::showEvent(QShowEvent *event)
{
	WebWidget::showEvent(event);

	ensureInitialized();

	if (m_focusProxyTimer == 0)
	{
		m_focusProxyTimer = startTimer(500);
	}
}

void QtWebEngineWebWidget::hideEvent(QHideEvent *event)
{
	WebWidget::hideEvent(event);

	killTimer(m_focusProxyTimer);

	m_focusProxyTimer = 0;
}

void QtWebEngineWebWidget::focusInEvent(QFocusEvent *event)
{
	WebWidget::focusInEvent(event);

	ensureInitialized();

	m_webView->setFocus();
}

void QtWebEngineWebWidget::ensureInitialized()
{
	if (!m_webView)
	{
		m_webView = new QWebEngineView(this);
		m_webView->setPage(m_page);
		m_webView->setContextMenuPolicy(Qt::CustomContextMenu);
		m_webView->installEventFilter(this);

		QVBoxLayout *layout(new QVBoxLayout(this));
		layout->addWidget(m_webView);
		layout->setContentsMargins(0, 0, 0, 0);

		setLayout(layout);

		connect(m_webView, SIGNAL(renderProcessTerminated(QWebEnginePage::RenderProcessTerminationStatus,int)), this, SLOT(notifyRenderProcessTerminated(QWebEnginePage::RenderProcessTerminationStatus)));
	}
}

void QtWebEngineWebWidget::search(const QString &query, const QString &searchEngine)
{
	QNetworkRequest request;
	QNetworkAccessManager::Operation method;
	QByteArray body;

	if (SearchEnginesManager::setupSearchQuery(query, searchEngine, &request, &method, &body))
	{
		setRequestedUrl(request.url(), false, true);
		updateOptions(request.url());

		if (method == QNetworkAccessManager::PostOperation)
		{
#if QT_VERSION < 0x050900
			QFile file(QLatin1String(":/modules/backends/web/qtwebengine/resources/sendPost.js"));

			if (file.open(QIODevice::ReadOnly))
			{
				m_page->runJavaScript(QString(file.readAll()).arg(request.url().toString()).arg(QString(body)));

				file.close();
			}
#else
			QWebEngineHttpRequest httpRequest(request.url(), QWebEngineHttpRequest::Post);
			httpRequest.setPostData(body);

			m_page->load(httpRequest);
#endif
		}
		else
		{
			setUrl(request.url(), false);
		}
	}
}

void QtWebEngineWebWidget::print(QPrinter *printer)
{
#if QT_VERSION < 0x050800
	ensureInitialized();

	m_webView->render(printer);
#else
	QEventLoop eventLoop;

	m_page->print(printer, [&](bool)
	{
		eventLoop.quit();
	});

	eventLoop.exec();
#endif
}

void QtWebEngineWebWidget::pageLoadStarted()
{
	m_lastUrlClickTime = QDateTime();
	m_loadingState = OngoingLoadingState;
	m_documentLoadingProgress = 0;

	if (!m_loadingTime)
	{
		m_loadingTime = new QTime();
		m_loadingTime->start();
	}

	setStatusMessage(QString());
	setStatusMessage(QString(), true);

	emit progressBarGeometryChanged();
	emit loadingStateChanged(OngoingLoadingState);
	emit pageInformationChanged(DocumentLoadingProgressInformation, 0);
}

void QtWebEngineWebWidget::pageLoadFinished()
{
	m_loadingState = FinishedLoadingState;

	notifyNavigationActionsChanged();
	startReloadTimer();

	emit contentStateChanged(getContentState());
	emit loadingStateChanged(FinishedLoadingState);
}

void QtWebEngineWebWidget::linkHovered(const QString &link)
{
	setStatusMessage(link, true);
}

void QtWebEngineWebWidget::clearOptions()
{
	WebWidget::clearOptions();

	updateOptions(getUrl());
}

void QtWebEngineWebWidget::goToHistoryIndex(int index)
{
	m_page->history()->goToItem(m_page->history()->itemAt(index));
}

void QtWebEngineWebWidget::removeHistoryIndex(int index, bool purge)
{
	Q_UNUSED(purge)

	WindowHistoryInformation history(getHistory());

	if (index < 0 || index >= history.entries.count())
	{
		return;
	}

	history.entries.removeAt(index);

	if (history.index >= index)
	{
		history.index = (history.index - 1);
	}

	setHistory(history);
}

void QtWebEngineWebWidget::triggerAction(int identifier, const QVariantMap &parameters)
{
	switch (identifier)
	{
		case ActionsManager::SaveAction:
#if QT_VERSION >= 0x050700
			m_page->triggerAction(QWebEnginePage::SavePage);
#else

			{
				const QString path(Utils::getSavePath(suggestSaveFileName(SingleHtmlFileSaveFormat)).path);

				if (!path.isEmpty())
				{
					QNetworkRequest request(getUrl());
					request.setHeader(QNetworkRequest::UserAgentHeader, m_page->profile()->httpUserAgent());

					new Transfer(request, path, (Transfer::CanAskForPathOption | Transfer::CanAutoDeleteOption | Transfer::CanOverwriteOption | Transfer::IsPrivateOption));
				}
			}
#endif

			return;
		case ActionsManager::ClearTabHistoryAction:
			setUrl(QUrl(QLatin1String("about:blank")));

			m_page->history()->clear();

			notifyNavigationActionsChanged();

			return;
#if QT_VERSION >= 0x050700
		case ActionsManager::MuteTabMediaAction:
			m_page->setAudioMuted(!m_page->isAudioMuted());

			emit actionsStateChanged(QVector<int>({ActionsManager::MuteTabMediaAction}));

			return;
#endif
		case ActionsManager::OpenLinkAction:
			{
				ensureInitialized();

				QMouseEvent mousePressEvent(QEvent::MouseButtonPress, QPointF(getClickPosition()), Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
				QMouseEvent mouseReleaseEvent(QEvent::MouseButtonRelease, QPointF(getClickPosition()), Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);

				QCoreApplication::sendEvent(m_webView, &mousePressEvent);
				QCoreApplication::sendEvent(m_webView, &mouseReleaseEvent);

				setClickPosition(QPoint());
			}

			return;
		case ActionsManager::OpenLinkInCurrentTabAction:
			if (m_hitResult.linkUrl.isValid())
			{
				openUrl(m_hitResult.linkUrl, SessionsManager::CurrentTabOpen);
			}

			return;
		case ActionsManager::OpenLinkInNewTabAction:
			if (m_hitResult.linkUrl.isValid())
			{
				openUrl(m_hitResult.linkUrl, SessionsManager::calculateOpenHints(SessionsManager::NewTabOpen));
			}

			return;
		case ActionsManager::OpenLinkInNewTabBackgroundAction:
			if (m_hitResult.linkUrl.isValid())
			{
				openUrl(m_hitResult.linkUrl, (SessionsManager::NewTabOpen | SessionsManager::BackgroundOpen));
			}

			return;
		case ActionsManager::OpenLinkInNewWindowAction:
			if (m_hitResult.linkUrl.isValid())
			{
				openUrl(m_hitResult.linkUrl, SessionsManager::calculateOpenHints(SessionsManager::NewWindowOpen));
			}

			return;
		case ActionsManager::OpenLinkInNewWindowBackgroundAction:
			if (m_hitResult.linkUrl.isValid())
			{
				openUrl(m_hitResult.linkUrl, (SessionsManager::NewWindowOpen | SessionsManager::BackgroundOpen));
			}

			return;
		case ActionsManager::OpenLinkInNewPrivateTabAction:
			if (m_hitResult.linkUrl.isValid())
			{
				openUrl(m_hitResult.linkUrl, (SessionsManager::NewTabOpen | SessionsManager::PrivateOpen));
			}

			return;
		case ActionsManager::OpenLinkInNewPrivateTabBackgroundAction:
			if (m_hitResult.linkUrl.isValid())
			{
				openUrl(m_hitResult.linkUrl, (SessionsManager::NewTabOpen | SessionsManager::BackgroundOpen | SessionsManager::PrivateOpen));
			}

			return;
		case ActionsManager::OpenLinkInNewPrivateWindowAction:
			if (m_hitResult.linkUrl.isValid())
			{
				openUrl(m_hitResult.linkUrl, (SessionsManager::NewWindowOpen | SessionsManager::PrivateOpen));
			}

			return;
		case ActionsManager::OpenLinkInNewPrivateWindowBackgroundAction:
			if (m_hitResult.linkUrl.isValid())
			{
				openUrl(m_hitResult.linkUrl, (SessionsManager::NewWindowOpen | SessionsManager::BackgroundOpen | SessionsManager::PrivateOpen));
			}

			return;
		case ActionsManager::CopyLinkToClipboardAction:
			if (!m_hitResult.linkUrl.isEmpty())
			{
				QGuiApplication::clipboard()->setText(m_hitResult.linkUrl.toString(QUrl::EncodeReserved | QUrl::EncodeSpaces));
			}

			return;
		case ActionsManager::BookmarkLinkAction:
			if (m_hitResult.linkUrl.isValid())
			{
				Application::triggerAction(ActionsManager::BookmarkPageAction, {{QLatin1String("url"), m_hitResult.linkUrl}, {QLatin1String("title"), m_hitResult.title}}, parentWidget());
			}

			return;
		case ActionsManager::SaveLinkToDiskAction:
			startTransfer(new Transfer(m_hitResult.linkUrl.toString(), QString(), (Transfer::CanNotifyOption | (isPrivate() ? Transfer::IsPrivateOption : Transfer::NoOption))));

			return;
		case ActionsManager::SaveLinkToDownloadsAction:
			TransfersManager::addTransfer(new Transfer(m_hitResult.linkUrl.toString(), QString(), (Transfer::CanNotifyOption | Transfer::CanAskForPathOption | Transfer::IsQuickTransferOption | (isPrivate() ? Transfer::IsPrivateOption : Transfer::NoOption))));

			return;
		case ActionsManager::OpenFrameInCurrentTabAction:
			if (m_hitResult.frameUrl.isValid())
			{
				setUrl(m_hitResult.frameUrl, false);
			}

			return;
		case ActionsManager::OpenFrameInNewTabAction:
			if (m_hitResult.frameUrl.isValid())
			{
				openUrl(m_hitResult.frameUrl, SessionsManager::calculateOpenHints(SessionsManager::NewTabOpen));
			}

			return;
		case ActionsManager::OpenFrameInNewTabBackgroundAction:
			if (m_hitResult.frameUrl.isValid())
			{
				openUrl(m_hitResult.frameUrl, (SessionsManager::NewTabOpen | SessionsManager::BackgroundOpen));
			}

			return;
		case ActionsManager::CopyFrameLinkToClipboardAction:
			if (m_hitResult.frameUrl.isValid())
			{
				QGuiApplication::clipboard()->setText(m_hitResult.frameUrl.toString(QUrl::EncodeReserved | QUrl::EncodeSpaces));
			}

			return;
		case ActionsManager::ReloadFrameAction:
			if (m_hitResult.frameUrl.isValid())
			{
//TODO Improve
				m_page->runJavaScript(QStringLiteral("var frames = document.querySelectorAll('iframe[src=\"%1\"], frame[src=\"%1\"]'); for (var i = 0; i < frames.length; ++i) { frames[i].src = ''; frames[i].src = \'%1\'; }").arg(m_hitResult.frameUrl.toString()));
			}

			return;
		case ActionsManager::ViewFrameSourceAction:
			if (m_hitResult.frameUrl.isValid())
			{
				const QString defaultEncoding(m_page->settings()->defaultTextEncoding());
				QNetworkRequest request(m_hitResult.frameUrl);
				request.setAttribute(QNetworkRequest::CacheLoadControlAttribute, QNetworkRequest::PreferCache);
				request.setHeader(QNetworkRequest::UserAgentHeader, m_page->profile()->httpUserAgent());

				QNetworkReply *reply(NetworkManagerFactory::getNetworkManager()->get(request));
				SourceViewerWebWidget *sourceViewer(new SourceViewerWebWidget(isPrivate()));
				sourceViewer->setRequestedUrl(QUrl(QLatin1String("view-source:") + m_hitResult.frameUrl.toString()), false);

				if (!defaultEncoding.isEmpty())
				{
					sourceViewer->setOption(SettingsManager::Content_DefaultCharacterEncodingOption, defaultEncoding);
				}

				m_viewSourceReplies[reply] = sourceViewer;

				connect(reply, SIGNAL(finished()), this, SLOT(viewSourceReplyFinished()));
				connect(reply, SIGNAL(error(QNetworkReply::NetworkError)), this, SLOT(viewSourceReplyFinished(QNetworkReply::NetworkError)));

				emit requestedNewWindow(sourceViewer, SessionsManager::DefaultOpen);
			}

			return;
		case ActionsManager::OpenImageInNewTabAction:
			if (!m_hitResult.imageUrl.isEmpty())
			{
				openUrl(m_hitResult.imageUrl, SessionsManager::calculateOpenHints(SessionsManager::NewTabOpen));
			}

			return;
		case ActionsManager::OpenImageInNewTabBackgroundAction:
			if (!m_hitResult.imageUrl.isEmpty())
			{
				openUrl(m_hitResult.imageUrl, (SessionsManager::NewTabOpen | SessionsManager::BackgroundOpen));
			}

			return;
		case ActionsManager::SaveImageToDiskAction:
			if (m_hitResult.imageUrl.isValid())
			{
				if (m_hitResult.imageUrl.url().contains(QLatin1String(";base64,")))
				{
					const QString imageUrl(m_hitResult.imageUrl.url());
					const QString imageType(imageUrl.mid(11, (imageUrl.indexOf(QLatin1Char(';')) - 11)));
					const QString path(Utils::getSavePath(tr("file") + QLatin1Char('.') + imageType).path);

					if (path.isEmpty())
					{
						return;
					}

					QImageWriter writer(path);

					if (!writer.write(QImage::fromData(QByteArray::fromBase64(imageUrl.mid(imageUrl.indexOf(QLatin1String(";base64,")) + 7).toUtf8()), imageType.toStdString().c_str())))
					{
						Console::addMessage(tr("Failed to save image: %1").arg(writer.errorString()), Console::OtherCategory, Console::ErrorLevel, path, -1, getWindowIdentifier());
					}
				}
				else
				{
					QNetworkRequest request(m_hitResult.imageUrl);
					request.setHeader(QNetworkRequest::UserAgentHeader, m_page->profile()->httpUserAgent());

					new Transfer(request, QString(), (Transfer::CanAskForPathOption | Transfer::CanAutoDeleteOption | Transfer::IsPrivateOption));
				}
			}

			return;
		case ActionsManager::CopyImageToClipboardAction:
			m_page->triggerAction(QWebEnginePage::CopyImageToClipboard);

			return;
		case ActionsManager::CopyImageUrlToClipboardAction:
			if (!m_hitResult.imageUrl.isEmpty())
			{
				QApplication::clipboard()->setText(m_hitResult.imageUrl.toString(QUrl::EncodeReserved | QUrl::EncodeSpaces));
			}

			return;
		case ActionsManager::ReloadImageAction:
			if (!m_hitResult.imageUrl.isEmpty())
			{
				if (getUrl().matches(m_hitResult.imageUrl, (QUrl::NormalizePathSegments | QUrl::RemoveFragment | QUrl::StripTrailingSlash)))
				{
					triggerAction(ActionsManager::ReloadAndBypassCacheAction);
				}
				else
				{
//TODO Improve
					m_page->runJavaScript(QStringLiteral("var images = document.querySelectorAll('img[src=\"%1\"]'); for (var i = 0; i < images.length; ++i) { images[i].src = ''; images[i].src = \'%1\'; }").arg(m_hitResult.imageUrl.toString()));
				}
			}

			return;
		case ActionsManager::ImagePropertiesAction:
			if (m_hitResult.imageUrl.scheme() == QLatin1String("data"))
			{
				ImagePropertiesDialog *imagePropertiesDialog(new ImagePropertiesDialog(m_hitResult.imageUrl, {{QLatin1String("alternativeText"), m_hitResult.alternateText}, {QLatin1String("longDescription"), m_hitResult.longDescription}}, nullptr, this));
				imagePropertiesDialog->setButtonsVisible(false);

				ContentsDialog *dialog(new ContentsDialog(ThemesManager::createIcon(QLatin1String("dialog-information")), imagePropertiesDialog->windowTitle(), QString(), QString(), QDialogButtonBox::Close, imagePropertiesDialog, this));

				connect(this, SIGNAL(aboutToReload()), dialog, SLOT(close()));
				connect(imagePropertiesDialog, SIGNAL(finished(int)), dialog, SLOT(close()));

				showDialog(dialog, false);
			}
			else
			{
				m_page->runJavaScript(QStringLiteral("var element = ((%1 >= 0) ? document.elementFromPoint((%1 + window.scrollX), (%2 + window.scrollX)) : document.activeElement); if (element && element.tagName && element.tagName.toLowerCase() == 'img') { [element.naturalWidth, element.naturalHeight]; }").arg(getClickPosition().x() / m_page->zoomFactor()).arg(getClickPosition().y() / m_page->zoomFactor()), [&](const QVariant &result)
				{
					QVariantMap properties({{QLatin1String("alternativeText"), m_hitResult.alternateText}, {QLatin1String("longDescription"), m_hitResult.longDescription}});

					if (result.isValid())
					{
						properties[QLatin1String("width")] = result.toList()[0].toInt();
						properties[QLatin1String("height")] = result.toList()[1].toInt();
					}

					ImagePropertiesDialog *imagePropertiesDialog(new ImagePropertiesDialog(m_hitResult.imageUrl, properties, nullptr, this));
					imagePropertiesDialog->setButtonsVisible(false);

					ContentsDialog dialog(ThemesManager::createIcon(QLatin1String("dialog-information")), imagePropertiesDialog->windowTitle(), QString(), QString(), QDialogButtonBox::Close, imagePropertiesDialog, this);

					connect(this, SIGNAL(aboutToReload()), &dialog, SLOT(close()));

					showDialog(&dialog);
				});
			}

			return;
		case ActionsManager::SaveMediaToDiskAction:
			if (m_hitResult.mediaUrl.isValid())
			{
				QNetworkRequest request(m_hitResult.mediaUrl);
				request.setHeader(QNetworkRequest::UserAgentHeader, m_page->profile()->httpUserAgent());

				new Transfer(request, QString(), (Transfer::CanAskForPathOption | Transfer::CanAutoDeleteOption | Transfer::IsPrivateOption));
			}

			return;
		case ActionsManager::CopyMediaUrlToClipboardAction:
			if (!m_hitResult.mediaUrl.isEmpty())
			{
				QApplication::clipboard()->setText(m_hitResult.mediaUrl.toString(QUrl::EncodeReserved | QUrl::EncodeSpaces));
			}

			return;
		case ActionsManager::MediaControlsAction:
			m_page->triggerAction(QWebEnginePage::ToggleMediaControls, parameters.value(QLatin1String("isChecked"), !getActionState(identifier, parameters).isChecked).toBool());

			return;
		case ActionsManager::MediaLoopAction:
			m_page->triggerAction(QWebEnginePage::ToggleMediaLoop, parameters.value(QLatin1String("isChecked"), !getActionState(identifier, parameters).isChecked).toBool());

			return;
		case ActionsManager::MediaPlayPauseAction:
			m_page->triggerAction(QWebEnginePage::ToggleMediaPlayPause);

			return;
		case ActionsManager::MediaMuteAction:
			m_page->triggerAction(QWebEnginePage::ToggleMediaMute);

			return;
		case ActionsManager::MediaPlaybackRateAction:
			m_page->runJavaScript(QStringLiteral("var element = ((%1 >= 0) ? document.elementFromPoint((%1 + window.scrollX), (%2 + window.scrollX)) : document.activeElement); if (element) { element.playbackRate = %3; }").arg(getClickPosition().x() / m_page->zoomFactor()).arg(getClickPosition().y() / m_page->zoomFactor()).arg(parameters.value(QLatin1String("rate"), 1).toReal()));

			return;
		case ActionsManager::GoBackAction:
			m_page->triggerAction(QWebEnginePage::Back);

			return;
		case ActionsManager::GoForwardAction:
			m_page->triggerAction(QWebEnginePage::Forward);

			return;
		case ActionsManager::RewindAction:
			m_page->history()->goToItem(m_page->history()->itemAt(0));

			return;
		case ActionsManager::FastForwardAction:
			m_page->runJavaScript(getFastForwardScript(true), [&](const QVariant &result)
			{
				const QUrl url(result.toUrl());

				if (url.isValid())
				{
					setUrl(url);
				}
				else if (canGoForward())
				{
					m_page->triggerAction(QWebEnginePage::Forward);
				}
			});

			return;
		case ActionsManager::StopAction:
			m_page->triggerAction(QWebEnginePage::Stop);

			return;
		case ActionsManager::ReloadAction:
			emit aboutToReload();

			m_page->triggerAction(QWebEnginePage::Stop);
			m_page->triggerAction(QWebEnginePage::Reload);

			return;
		case ActionsManager::ReloadOrStopAction:
			if (m_loadingState == OngoingLoadingState)
			{
				triggerAction(ActionsManager::StopAction);
			}
			else
			{
				triggerAction(ActionsManager::ReloadAction);
			}

			return;
		case ActionsManager::ReloadAndBypassCacheAction:
			m_page->triggerAction(QWebEnginePage::ReloadAndBypassCache);

			return;
		case ActionsManager::ContextMenuAction:
			showContextMenu(getClickPosition());

			return;
		case ActionsManager::UndoAction:
			m_page->triggerAction(QWebEnginePage::Undo);

			return;
		case ActionsManager::RedoAction:
			m_page->triggerAction(QWebEnginePage::Redo);

			return;
		case ActionsManager::CutAction:
			m_page->triggerAction(QWebEnginePage::Cut);

			return;
		case ActionsManager::CopyAction:
			if (parameters.value(QLatin1String("mode")) == QLatin1String("plainText"))
			{
				const QString text(getSelectedText());

				if (!text.isEmpty())
				{
					QApplication::clipboard()->setText(text);
				}
			}
			else
			{
				m_page->triggerAction(QWebEnginePage::Copy);
			}

			return;
		case ActionsManager::CopyPlainTextAction:
			triggerAction(ActionsManager::CopyAction, {{QLatin1String("mode"), QLatin1String("plainText")}});

			return;
		case ActionsManager::CopyAddressAction:
			QApplication::clipboard()->setText(getUrl().toString());

			return;
		case ActionsManager::CopyToNoteAction:
			{
				BookmarksItem *note(NotesManager::addNote(BookmarksModel::UrlBookmark, getUrl()));
				note->setData(getSelectedText(), BookmarksModel::DescriptionRole);
			}

			return;
		case ActionsManager::PasteAction:
			if (parameters.contains(QLatin1String("note")))
			{
				const BookmarksItem *bookmark(NotesManager::getModel()->getBookmark(parameters[QLatin1String("note")].toULongLong()));

				if (bookmark)
				{
					triggerAction(ActionsManager::PasteAction, {{QLatin1String("text"), bookmark->data(BookmarksModel::DescriptionRole).toString()}});
				}
			}
			else if (parameters.contains(QLatin1String("text")))
			{
				QMimeData *mimeData(new QMimeData());
				const QStringList mimeTypes(QGuiApplication::clipboard()->mimeData()->formats());

				for (int i = 0; i < mimeTypes.count(); ++i)
				{
					mimeData->setData(mimeTypes.at(i), QGuiApplication::clipboard()->mimeData()->data(mimeTypes.at(i)));
				}

				QGuiApplication::clipboard()->setText(parameters[QLatin1String("text")].toString());

				m_page->triggerAction(QWebEnginePage::Paste);

				QGuiApplication::clipboard()->setMimeData(mimeData);
			}
			else
			{
				m_page->triggerAction(QWebEnginePage::Paste);
			}

			return;
		case ActionsManager::DeleteAction:
			m_page->runJavaScript(QLatin1String("window.getSelection().deleteFromDocument()"));

			return;
		case ActionsManager::SelectAllAction:
			m_page->triggerAction(QWebEnginePage::SelectAll);

			return;
		case ActionsManager::UnselectAction:
#if QT_VERSION >= 0x050700
			m_page->triggerAction(QWebEnginePage::Unselect);
#else
			m_page->runJavaScript(QLatin1String("window.getSelection().empty()"));
#endif
			return;
		case ActionsManager::ClearAllAction:
			triggerAction(ActionsManager::SelectAllAction);
			triggerAction(ActionsManager::DeleteAction);

			return;
		case ActionsManager::CreateSearchAction:
			{
				QFile file(QLatin1String(":/modules/backends/web/qtwebengine/resources/createSearch.js"));

				if (!file.open(QIODevice::ReadOnly))
				{
					return;
				}

				m_page->runJavaScript(QString(file.readAll()).arg(getClickPosition().x() / m_page->zoomFactor()).arg(getClickPosition().y() / m_page->zoomFactor()), [&](const QVariant &result)
				{
					if (result.isNull())
					{
						return;
					}

					const QUrlQuery parameters(result.toMap().value(QLatin1String("query")).toString());
					const QStringList identifiers(SearchEnginesManager::getSearchEngines());
					const QStringList keywords(SearchEnginesManager::getSearchKeywords());
					const QIcon icon(getIcon());
					const QUrl url(result.toMap().value(QLatin1String("url")).toString());
					SearchEnginesManager::SearchEngineDefinition searchEngine;
					searchEngine.identifier = Utils::createIdentifier(getUrl().host(), identifiers);
					searchEngine.title = getTitle();
					searchEngine.formUrl = getUrl();
					searchEngine.icon = (icon.isNull() ? ThemesManager::createIcon(QLatin1String("edit-find")) : icon);
					searchEngine.resultsUrl.url = (url.isEmpty() ? getUrl() : (url.isRelative() ? getUrl().resolved(url) : url)).toString();
					searchEngine.resultsUrl.enctype = result.toMap().value(QLatin1String("enctype")).toString();
					searchEngine.resultsUrl.method = result.toMap().value(QLatin1String("method")).toString();
					searchEngine.resultsUrl.parameters = parameters;

					SearchEnginePropertiesDialog dialog(searchEngine, keywords, this);

					if (dialog.exec() == QDialog::Rejected)
					{
						return;
					}

					SearchEnginesManager::addSearchEngine(dialog.getSearchEngine());
				});

				file.close();
			}

			return;
		case ActionsManager::ScrollToStartAction:
			m_page->runJavaScript(QLatin1String("window.scrollTo(0, 0)"));

			return;
		case ActionsManager::ScrollToEndAction:
			m_page->runJavaScript(QLatin1String("window.scrollTo(0, document.body.scrollHeigh)"));

			return;
		case ActionsManager::ScrollPageUpAction:
			m_page->runJavaScript(QLatin1String("window.scrollByPages(1)"));

			return;
		case ActionsManager::ScrollPageDownAction:
			m_page->runJavaScript(QLatin1String("window.scrollByPages(-1)"));

			return;
		case ActionsManager::ScrollPageLeftAction:
			ensureInitialized();

			m_page->runJavaScript(QStringLiteral("window.scrollBy(-%1, 0)").arg(m_webView->width()));

			return;
		case ActionsManager::ScrollPageRightAction:
			ensureInitialized();

			m_page->runJavaScript(QStringLiteral("window.scrollBy(%1, 0)").arg(m_webView->width()));

			return;
		case ActionsManager::ActivateContentAction:
			{
				ensureInitialized();

				m_webView->setFocus();

				m_page->runJavaScript(QLatin1String("var element = document.activeElement; if (element && element.tagName && (element.tagName.toLowerCase() == 'input' || element.tagName.toLowerCase() == 'textarea'))) { document.activeElement.blur(); }"));
			}

			return;
		case ActionsManager::ViewSourceAction:
			if (canViewSource())
			{
				const QString defaultEncoding(m_page->settings()->defaultTextEncoding());
				QNetworkRequest request(getUrl());
				request.setAttribute(QNetworkRequest::CacheLoadControlAttribute, QNetworkRequest::PreferCache);
				request.setHeader(QNetworkRequest::UserAgentHeader, m_page->profile()->httpUserAgent());

				QNetworkReply *reply(NetworkManagerFactory::getNetworkManager()->get(request));
				SourceViewerWebWidget *sourceViewer(new SourceViewerWebWidget(isPrivate()));
				sourceViewer->setRequestedUrl(QUrl(QLatin1String("view-source:") + getUrl().toString()), false);

				if (!defaultEncoding.isEmpty())
				{
					sourceViewer->setOption(SettingsManager::Content_DefaultCharacterEncodingOption, defaultEncoding);
				}

				m_viewSourceReplies[reply] = sourceViewer;

				connect(reply, SIGNAL(finished()), this, SLOT(viewSourceReplyFinished()));
				connect(reply, SIGNAL(error(QNetworkReply::NetworkError)), this, SLOT(viewSourceReplyFinished(QNetworkReply::NetworkError)));

				emit requestedNewWindow(sourceViewer, SessionsManager::DefaultOpen);
			}

			return;
		case ActionsManager::FullScreenAction:
			{
				MainWindow *mainWindow(MainWindow::findMainWindow(this));

				if (mainWindow && !mainWindow->isFullScreen())
				{
					m_page->triggerAction(QWebEnginePage::ExitFullScreen);
				}
			}

			return;
		case ActionsManager::WebsitePreferencesAction:
			{
				const QUrl url(getUrl());
				WebsitePreferencesDialog dialog(url, QVector<QNetworkCookie>(), this);

				if (dialog.exec() == QDialog::Accepted)
				{
					updateOptions(getUrl());

					const QVector<QNetworkCookie> cookiesToDelete(dialog.getCookiesToDelete());

					for (int i = 0; i < cookiesToDelete.count(); ++i)
					{
						m_page->profile()->cookieStore()->deleteCookie(cookiesToDelete.at(i));
					}

					const QVector<QNetworkCookie> cookiesToInsert(dialog.getCookiesToInsert());

					for (int i = 0; i < cookiesToInsert.count(); ++i)
					{
						m_page->profile()->cookieStore()->setCookie(cookiesToInsert.at(i));
					}
				}
			}

			return;
		default:
			break;
	}
}

#if QT_VERSION < 0x050700
void QtWebEngineWebWidget::iconReplyFinished()
{
	if (!m_iconReply)
	{
		return;
	}

	m_icon = QIcon(QPixmap::fromImage(QImage::fromData(m_iconReply->readAll())));

	emit iconChanged(getIcon());

	m_iconReply->deleteLater();
	m_iconReply = nullptr;
}
#endif

void QtWebEngineWebWidget::viewSourceReplyFinished(QNetworkReply::NetworkError error)
{
	QNetworkReply *reply(qobject_cast<QNetworkReply*>(sender()));

	if (error == QNetworkReply::NoError && m_viewSourceReplies.contains(reply) && m_viewSourceReplies[reply])
	{
		m_viewSourceReplies[reply]->setContents(reply->readAll(), reply->header(QNetworkRequest::ContentTypeHeader).toString());
	}

	m_viewSourceReplies.remove(reply);

	reply->deleteLater();
}

#if QT_VERSION < 0x050700
void QtWebEngineWebWidget::handleIconChange(const QUrl &url)
{
	if (m_iconReply && m_iconReply->url() != url)
	{
		m_iconReply->abort();
		m_iconReply->deleteLater();
	}

	m_icon = QIcon();

	emit iconChanged(getIcon());

	QNetworkRequest request(url);
	request.setHeader(QNetworkRequest::UserAgentHeader, m_page->profile()->httpUserAgent());

	m_iconReply = NetworkManagerFactory::getNetworkManager()->get(request);
	m_iconReply->setParent(this);

	connect(m_iconReply, SIGNAL(finished()), this, SLOT(iconReplyFinished()));
}
#endif

void QtWebEngineWebWidget::handleAuthenticationRequired(const QUrl &url, QAuthenticator *authenticator)
{
	AuthenticationDialog *authenticationDialog(new AuthenticationDialog(url, authenticator, AuthenticationDialog::HttpAuthentication, this));
	authenticationDialog->setButtonsVisible(false);

	ContentsDialog dialog(ThemesManager::createIcon(QLatin1String("dialog-password")), authenticationDialog->windowTitle(), QString(), QString(), (QDialogButtonBox::Ok | QDialogButtonBox::Cancel), authenticationDialog, this);

	connect(&dialog, SIGNAL(accepted(bool)), authenticationDialog, SLOT(accept()));
	connect(this, SIGNAL(aboutToReload()), &dialog, SLOT(close()));

	showDialog(&dialog);
}

void QtWebEngineWebWidget::handleProxyAuthenticationRequired(const QUrl &url, QAuthenticator *authenticator, const QString &proxy)
{
	Q_UNUSED(url)

	AuthenticationDialog *authenticationDialog(new AuthenticationDialog(proxy, authenticator, AuthenticationDialog::ProxyAuthentication, this));
	authenticationDialog->setButtonsVisible(false);

	ContentsDialog dialog(ThemesManager::createIcon(QLatin1String("dialog-password")), authenticationDialog->windowTitle(), QString(), QString(), (QDialogButtonBox::Ok | QDialogButtonBox::Cancel), authenticationDialog, this);

	connect(&dialog, SIGNAL(accepted(bool)), authenticationDialog, SLOT(accept()));
	connect(this, SIGNAL(aboutToReload()), &dialog, SLOT(close()));

	showDialog(&dialog);
}

void QtWebEngineWebWidget::handleFullScreenRequest(QWebEngineFullScreenRequest request)
{
	request.accept();

	if (request.toggleOn())
	{
		const QString value(SettingsManager::getOption(SettingsManager::Permissions_EnableFullScreenOption, request.origin()).toString());

		if (value == QLatin1String("allow"))
		{
			MainWindow *mainWindow(MainWindow::findMainWindow(this));

			if (mainWindow && !mainWindow->isFullScreen())
			{
				mainWindow->triggerAction(ActionsManager::FullScreenAction);
			}
		}
		else if (value == QLatin1String("ask"))
		{
			emit requestedPermission(FullScreenFeature, request.origin(), false);
		}
	}
	else
	{
		MainWindow *mainWindow(MainWindow::findMainWindow(this));

		if (mainWindow && mainWindow->isFullScreen())
		{
			mainWindow->triggerAction(ActionsManager::FullScreenAction);
		}

		emit requestedPermission(FullScreenFeature, request.origin(), true);
	}

	m_isFullScreen = request.toggleOn();

	emit isFullScreenChanged(m_isFullScreen);
}

void QtWebEngineWebWidget::handlePermissionRequest(const QUrl &url, QWebEnginePage::Feature feature)
{
	notifyPermissionRequested(url, feature, false);
}

void QtWebEngineWebWidget::handlePermissionCancel(const QUrl &url, QWebEnginePage::Feature feature)
{
	notifyPermissionRequested(url, feature, true);
}

void QtWebEngineWebWidget::notifyTitleChanged()
{
	emit titleChanged(getTitle());
}

void QtWebEngineWebWidget::notifyUrlChanged(const QUrl &url)
{
	notifyNavigationActionsChanged();
	updateOptions(url);

#if QT_VERSION < 0x050700
	m_icon = QIcon();
#endif

	emit iconChanged(getIcon());
	emit urlChanged((url.toString() == QLatin1String("about:blank")) ? m_page->requestedUrl() : url);
	emit actionsStateChanged(ActionsManager::ActionDefinition::PageCategory);

	SessionsManager::markSessionModified();
}

void QtWebEngineWebWidget::notifyIconChanged()
{
	emit iconChanged(getIcon());
}

void QtWebEngineWebWidget::notifyPermissionRequested(const QUrl &url, QWebEnginePage::Feature nativeFeature, bool cancel)
{
	FeaturePermission feature(UnknownFeature);

	switch (nativeFeature)
	{
		case QWebEnginePage::Geolocation:
			feature = GeolocationFeature;

			break;
		case QWebEnginePage::MediaAudioCapture:
			feature = CaptureAudioFeature;

			break;
		case QWebEnginePage::MediaVideoCapture:
			feature = CaptureVideoFeature;

			break;
		case QWebEnginePage::MediaAudioVideoCapture:
			feature = CaptureAudioVideoFeature;

			break;
		case QWebEnginePage::Notifications:
			feature = NotificationsFeature;

			break;
		case QWebEnginePage::MouseLock:
			feature = PointerLockFeature;

			break;
		default:
			return;
	}

	if (cancel)
	{
		emit requestedPermission(feature, url, true);
	}
	else
	{
		switch (getPermission(feature, url))
		{
			case GrantedPermission:
				m_page->setFeaturePermission(url, nativeFeature, QWebEnginePage::PermissionGrantedByUser);

				break;
			case DeniedPermission:
				m_page->setFeaturePermission(url, nativeFeature, QWebEnginePage::PermissionDeniedByUser);

				break;
			default:
				emit requestedPermission(feature, url, false);

				break;
		}
	}
}

void QtWebEngineWebWidget::notifyRenderProcessTerminated(QWebEnginePage::RenderProcessTerminationStatus status)
{
	if (status != QWebEnginePage::NormalTerminationStatus)
	{
		m_loadingState = CrashedLoadingState;

		emit loadingStateChanged(CrashedLoadingState);
	}
}

void QtWebEngineWebWidget::notifyDocumentLoadingProgress(int progress)
{
	m_documentLoadingProgress = progress;

	emit pageInformationChanged(DocumentLoadingProgressInformation, progress);
}

void QtWebEngineWebWidget::notifyNavigationActionsChanged()
{
	if (m_updateNavigationActionsTimer == 0)
	{
		m_updateNavigationActionsTimer = startTimer(0);
	}
}

void QtWebEngineWebWidget::updateOptions(const QUrl &url)
{
	const QString encoding(getOption(SettingsManager::Content_DefaultCharacterEncodingOption, url).toString());
	QWebEngineSettings *settings(m_page->settings());
#if QT_VERSION >= 0x050800
	settings->setAttribute(QWebEngineSettings::AllowRunningInsecureContent, getOption(SettingsManager::Security_AllowMixedContentOption, url).toBool());
#endif
	settings->setAttribute(QWebEngineSettings::AutoLoadImages, (getOption(SettingsManager::Permissions_EnableImagesOption, url).toString() != QLatin1String("onlyCached")));
	settings->setAttribute(QWebEngineSettings::JavascriptEnabled, getOption(SettingsManager::Permissions_EnableJavaScriptOption, url).toBool());
	settings->setAttribute(QWebEngineSettings::JavascriptCanAccessClipboard, getOption(SettingsManager::Permissions_ScriptsCanAccessClipboardOption, url).toBool());
	settings->setAttribute(QWebEngineSettings::JavascriptCanOpenWindows, (getOption(SettingsManager::Permissions_ScriptsCanOpenWindowsOption, url).toString() != QLatin1String("blockAll")));
	settings->setAttribute(QWebEngineSettings::LocalStorageEnabled, getOption(SettingsManager::Permissions_EnableLocalStorageOption, url).toBool());
#if QT_VERSION >= 0x050700
	settings->setAttribute(QWebEngineSettings::WebGLEnabled, getOption(SettingsManager::Permissions_EnableWebglOption, url).toBool());
#endif
	settings->setDefaultTextEncoding((encoding == QLatin1String("auto")) ? QString() : encoding);

	m_page->profile()->setHttpUserAgent(getBackend()->getUserAgent(NetworkManagerFactory::getUserAgent(getOption(SettingsManager::Network_UserAgentOption, url).toString()).value));

	disconnect(m_page, SIGNAL(geometryChangeRequested(QRect)), this, SIGNAL(requestedGeometryChange(QRect)));

	if (getOption(SettingsManager::Permissions_ScriptsCanChangeWindowGeometryOption, url).toBool())
	{
		connect(m_page, SIGNAL(geometryChangeRequested(QRect)), this, SIGNAL(requestedGeometryChange(QRect)));
	}
}

void QtWebEngineWebWidget::setScrollPosition(const QPoint &position)
{
#if QT_VERSION < 0x050700
	m_page->runJavaScript(QStringLiteral("window.scrollTo(%1, %2); [window.scrollX, window.scrollY];").arg(position.x()).arg(position.y()), [&](const QVariant &result)
	{
		if (result.isValid())
		{
			m_scrollPosition = QPoint(result.toList()[0].toInt(), result.toList()[1].toInt());
		}
	});
#else
	m_page->runJavaScript(QStringLiteral("window.scrollTo(%1, %2); [window.scrollX, window.scrollY];").arg(position.x()).arg(position.y()));
#endif
}

void QtWebEngineWebWidget::setHistory(const WindowHistoryInformation &history)
{
	if (history.entries.count() == 0)
	{
		m_page->history()->clear();

		updateOptions(QUrl());

		const QVector<UserScript*> scripts(UserScript::getUserScriptsForUrl(QUrl(QLatin1String("about:blank"))));

		for (int i = 0; i < scripts.count(); ++i)
		{
#if QT_VERSION >= 0x050700
			m_page->runJavaScript(scripts.at(i)->getSource(), QWebEngineScript::UserWorld);
#else
			m_page->runJavaScript(scripts.at(i)->getSource());
#endif
		}

		notifyNavigationActionsChanged();

		emit actionsStateChanged(ActionsManager::ActionDefinition::PageCategory);

		return;
	}

	QByteArray data;
	QDataStream stream(&data, QIODevice::ReadWrite);
	stream << int(3) << history.entries.count() << history.index;

	for (int i = 0; i < history.entries.count(); ++i)
	{
		stream << QUrl(history.entries.at(i).url) << history.entries.at(i).title << QByteArray() << qint32(0) << false << QUrl() << qint32(0) << QUrl(history.entries.at(i).url) << false << qint64(QDateTime::currentDateTime().toMSecsSinceEpoch() / 1000) << int(200);
	}

	stream.device()->reset();
	stream >> *(m_page->history());

	m_page->history()->goToItem(m_page->history()->itemAt(history.index));

	const QUrl url(m_page->history()->itemAt(history.index).url());

	setRequestedUrl(url, false, true);
	updateOptions(url);

	m_page->triggerAction(QWebEnginePage::Reload);

	notifyNavigationActionsChanged();

	emit actionsStateChanged(ActionsManager::ActionDefinition::PageCategory);
}

void QtWebEngineWebWidget::setHistory(QDataStream &stream)
{
	stream.device()->reset();
	stream >> *(m_page->history());

	setRequestedUrl(m_page->history()->currentItem().url(), false, true);
	updateOptions(m_page->history()->currentItem().url());
}

void QtWebEngineWebWidget::setZoom(int zoom)
{
	if (zoom != getZoom())
	{
		m_page->setZoomFactor(qBound(0.1, (static_cast<qreal>(zoom) / 100), static_cast<qreal>(100)));

		SessionsManager::markSessionModified();

		emit zoomChanged(zoom);
		emit progressBarGeometryChanged();
	}
}

void QtWebEngineWebWidget::setUrl(const QUrl &url, bool isTyped)
{
	if (url.scheme() == QLatin1String("javascript"))
	{
		m_page->runJavaScript(url.toDisplayString(QUrl::RemoveScheme | QUrl::DecodeReserved));

		return;
	}

	if (!url.fragment().isEmpty() && url.matches(getUrl(), (QUrl::RemoveFragment | QUrl::StripTrailingSlash | QUrl::NormalizePathSegments)))
	{
		m_page->runJavaScript(QStringLiteral("var element = document.querySelector('a[name=%1], [id=%1]'); if (element) { var geometry = element.getBoundingClientRect(); [(geometry.left + window.scrollX), (geometry.top + window.scrollY)]; }").arg(url.fragment()), [&](const QVariant &result)
		{
			if (result.isValid())
			{
				setScrollPosition(QPoint(result.toList()[0].toInt(), result.toList()[1].toInt()));
			}
		});

		return;
	}

	m_isTyped = isTyped;

	QUrl targetUrl(url);

	if (url.isValid() && url.scheme().isEmpty() && !url.path().startsWith('/'))
	{
		QUrl httpUrl(url);
		httpUrl.setScheme(QLatin1String("http"));

		targetUrl = httpUrl;
	}
	else if (url.isValid() && (url.scheme().isEmpty() || url.scheme() == "file"))
	{
		QUrl localUrl(url);
		localUrl.setScheme(QLatin1String("file"));

		targetUrl = localUrl;
	}

	updateOptions(targetUrl);

	m_page->load(targetUrl);

	notifyTitleChanged();
	notifyIconChanged();
}

void QtWebEngineWebWidget::setPermission(FeaturePermission feature, const QUrl &url, PermissionPolicies policies)
{
	WebWidget::setPermission(feature, url, policies);

	if (feature == FullScreenFeature)
	{
		if (policies.testFlag(GrantedPermission))
		{
			MainWindow *mainWindow(MainWindow::findMainWindow(this));

			if (mainWindow && !mainWindow->isFullScreen())
			{
				mainWindow->triggerAction(ActionsManager::FullScreenAction);
			}
		}

		return;
	}

	QWebEnginePage::Feature nativeFeature(QWebEnginePage::Geolocation);

	switch (feature)
	{
		case GeolocationFeature:
			nativeFeature = QWebEnginePage::Geolocation;

			break;
		case NotificationsFeature:
			nativeFeature = QWebEnginePage::Notifications;

			break;
		case PointerLockFeature:
			nativeFeature = QWebEnginePage::MouseLock;

			break;
		case CaptureAudioFeature:
			nativeFeature = QWebEnginePage::MediaAudioCapture;

			break;
		case CaptureVideoFeature:
			nativeFeature = QWebEnginePage::MediaVideoCapture;

			break;
		case CaptureAudioVideoFeature:
			nativeFeature = QWebEnginePage::MediaAudioVideoCapture;

			break;
		default:
			return;
	}

	m_page->setFeaturePermission(url, nativeFeature, (policies.testFlag(GrantedPermission) ? QWebEnginePage::PermissionGrantedByUser : QWebEnginePage::PermissionDeniedByUser));
}

void QtWebEngineWebWidget::setOption(int identifier, const QVariant &value)
{
	WebWidget::setOption(identifier, value);

	updateOptions(getUrl());

	if (identifier == SettingsManager::Content_DefaultCharacterEncodingOption)
	{
		m_page->triggerAction(QWebEnginePage::Reload);
	}
}

void QtWebEngineWebWidget::setOptions(const QHash<int, QVariant> &options, const QStringList &excludedOptions)
{
	WebWidget::setOptions(options, excludedOptions);

	updateOptions(getUrl());
}

WebWidget* QtWebEngineWebWidget::clone(bool cloneHistory, bool isPrivate, const QStringList &excludedOptions) const
{
	QtWebEngineWebWidget *widget(new QtWebEngineWebWidget((this->isPrivate() || isPrivate), getBackend()));
	widget->setOptions(getOptions(), excludedOptions);

	if (cloneHistory)
	{
		QByteArray data;
		QDataStream stream(&data, QIODevice::ReadWrite);
		stream << *(m_page->history());

		widget->setHistory(stream);
	}

	widget->setZoom(getZoom());

	return widget;
}

QWidget* QtWebEngineWebWidget::getViewport()
{
	return (focusWidget() ? focusWidget() : m_webView);
}

QWebEnginePage* QtWebEngineWebWidget::getPage() const
{
	return m_page;
}

QString QtWebEngineWebWidget::getTitle() const
{
	const QString title(m_page->title());

	if (title.isEmpty())
	{
		const QUrl url(getUrl());

		if (url.scheme() == QLatin1String("about") && (url.path().isEmpty() || url.path() == QLatin1String("blank") || url.path() == QLatin1String("start")))
		{
			return tr("Blank Page");
		}

		if (url.isLocalFile())
		{
			return QFileInfo(url.toLocalFile()).canonicalFilePath();
		}

		if (!url.isEmpty())
		{
			return url.toString();
		}

		return tr("(Untitled)");
	}

	return title;
}

QString QtWebEngineWebWidget::getSelectedText() const
{
	return m_page->selectedText();
}

QVariant QtWebEngineWebWidget::getPageInformation(PageInformation key) const
{
	if (key == DocumentLoadingProgressInformation || key == TotalLoadingProgressInformation)
	{
		return m_documentLoadingProgress;
	}

	return WebWidget::getPageInformation(key);
}

QUrl QtWebEngineWebWidget::getUrl() const
{
	const QUrl url(m_page->url());

	return ((url.isEmpty() || url.toString() == QLatin1String("about:blank")) ? m_page->requestedUrl() : url);
}

QIcon QtWebEngineWebWidget::getIcon() const
{
	if (isPrivate())
	{
		return ThemesManager::createIcon(QLatin1String("tab-private"));
	}

#if QT_VERSION < 0x050700
	return (m_icon.isNull() ? ThemesManager::createIcon(QLatin1String("tab")) : m_icon);
#else
	const QIcon icon(m_page->icon());

	return (icon.isNull() ? ThemesManager::createIcon(QLatin1String("tab")) : icon);
#endif
}

QDateTime QtWebEngineWebWidget::getLastUrlClickTime() const
{
	return m_lastUrlClickTime;
}

QPixmap QtWebEngineWebWidget::createThumbnail()
{
	return QPixmap();
}

QPoint QtWebEngineWebWidget::getScrollPosition() const
{
#if QT_VERSION < 0x050700
	return m_scrollPosition;
#else
	return m_page->scrollPosition().toPoint();
#endif
}

ActionsManager::ActionDefinition::State QtWebEngineWebWidget::getActionState(int identifier, const QVariantMap &parameters) const
{
	switch (identifier)
	{
		case ActionsManager::InspectPageAction:
		case ActionsManager::InspectElementAction:
			{
				ActionsManager::ActionDefinition::State state(ActionsManager::getActionDefinition(identifier).defaultState);
				state.isEnabled = false;

				return state;
			}

			break;
		default:
			break;
	}

	return WebWidget::getActionState(identifier, parameters);
}

WindowHistoryInformation QtWebEngineWebWidget::getHistory() const
{
	const QWebEngineHistory *history(m_page->history());
	WindowHistoryInformation information;
	information.index = history->currentItemIndex();

	const QString requestedUrl(m_page->requestedUrl().toString());
	const int historyCount(history->count());

	for (int i = 0; i < historyCount; ++i)
	{
		const QWebEngineHistoryItem item(history->itemAt(i));
		WindowHistoryEntry entry;
		entry.url = item.url().toString();
		entry.title = item.title();

		information.entries.append(entry);
	}

	if (m_loadingState == OngoingLoadingState && requestedUrl != history->itemAt(history->currentItemIndex()).url().toString())
	{
		WindowHistoryEntry entry;
		entry.url = requestedUrl;
		entry.title = getTitle();

		information.index = historyCount;
		information.entries.append(entry);
	}

	return information;
}

WebWidget::HitTestResult QtWebEngineWebWidget::getHitTestResult(const QPoint &position)
{
	QFile file(QLatin1String(":/modules/backends/web/qtwebengine/resources/hitTest.js"));

	if (!file.open(QIODevice::ReadOnly))
	{
		return HitTestResult();
	}

	QEventLoop eventLoop;

	m_page->runJavaScript(QString(file.readAll()).arg(position.x() / m_page->zoomFactor()).arg(position.y() / m_page->zoomFactor()), [&](const QVariant &result)
	{
		m_hitResult = HitTestResult(result);

		eventLoop.quit();
	});

	file.close();

	connect(this, SIGNAL(aboutToReload()), &eventLoop, SLOT(quit()));
	connect(this, SIGNAL(destroyed()), &eventLoop, SLOT(quit()));

	eventLoop.exec();

	return m_hitResult;
}

QVector<SpellCheckManager::DictionaryInformation> QtWebEngineWebWidget::getDictionaries() const
{
	return QVector<SpellCheckManager::DictionaryInformation>();
}

QHash<QByteArray, QByteArray> QtWebEngineWebWidget::getHeaders() const
{
	return QHash<QByteArray, QByteArray>();
}

WebWidget::LoadingState QtWebEngineWebWidget::getLoadingState() const
{
	return m_loadingState;
}

int QtWebEngineWebWidget::getZoom() const
{
	return (m_page->zoomFactor() * 100);
}

int QtWebEngineWebWidget::findInPage(const QString &text, FindFlags flags)
{
	if (text.isEmpty())
	{
		m_page->findText(text);

		return 0;
	}

	QWebEnginePage::FindFlags nativeFlags(0);

	if (flags.testFlag(BackwardFind))
	{
		nativeFlags |= QWebEnginePage::FindBackward;
	}

	if (flags.testFlag(CaseSensitiveFind))
	{
		nativeFlags |= QWebEnginePage::FindCaseSensitively;
	}

	QEventLoop eventLoop;
	bool hasFound(false);

	m_page->findText(text, nativeFlags, [&](const QVariant &result)
	{
		hasFound = result.toBool();

		eventLoop.quit();
	});

	connect(this, SIGNAL(aboutToReload()), &eventLoop, SLOT(quit()));
	connect(this, SIGNAL(destroyed()), &eventLoop, SLOT(quit()));

	eventLoop.exec();

	return (hasFound ? -1 : 0);
}

bool QtWebEngineWebWidget::canGoBack() const
{
	return m_page->history()->canGoBack();
}

bool QtWebEngineWebWidget::canGoForward() const
{
	return m_page->history()->canGoForward();
}

bool QtWebEngineWebWidget::canFastForward() const
{
	if (canGoForward())
	{
		return true;
	}

	QEventLoop eventLoop;
	bool canFastFoward(false);

	m_page->runJavaScript(getFastForwardScript(false), [&](const QVariant &result)
	{
		canFastFoward = result.toBool();

		eventLoop.quit();
	});

	connect(this, SIGNAL(aboutToReload()), &eventLoop, SLOT(quit()));
	connect(this, SIGNAL(destroyed()), &eventLoop, SLOT(quit()));

	eventLoop.exec();

	return canFastFoward;
}

bool QtWebEngineWebWidget::canRedo() const
{
	return m_page->action(QWebEnginePage::Redo)->isEnabled();
}

bool QtWebEngineWebWidget::canUndo() const
{
	return m_page->action(QWebEnginePage::Undo)->isEnabled();
}

bool QtWebEngineWebWidget::canShowContextMenu(const QPoint &position) const
{
	Q_UNUSED(position)

	return true;
}

bool QtWebEngineWebWidget::canViewSource() const
{
	return (!m_page->isViewingMedia() && !Utils::isUrlEmpty(getUrl()));
}

bool QtWebEngineWebWidget::hasSelection() const
{
	return (m_page->hasSelection() && !m_page->selectedText().isEmpty());
}

#if QT_VERSION >= 0x050700
bool QtWebEngineWebWidget::isAudible() const
{
	return m_page->recentlyAudible();
}

bool QtWebEngineWebWidget::isAudioMuted() const
{
	return m_page->isAudioMuted();
}
#endif

bool QtWebEngineWebWidget::isFullScreen() const
{
	return m_isFullScreen;
}

bool QtWebEngineWebWidget::isPopup() const
{
	return m_page->isPopup();
}

bool QtWebEngineWebWidget::isPrivate() const
{
	return m_page->profile()->isOffTheRecord();
}

bool QtWebEngineWebWidget::isScrollBar(const QPoint &position) const
{
	Q_UNUSED(position)

	return false;
}

bool QtWebEngineWebWidget::eventFilter(QObject *object, QEvent *event)
{
	if (object == m_webView && event->type() == QEvent::ChildAdded)
	{
		const QChildEvent *childEvent(static_cast<QChildEvent*>(event));

		if (childEvent->child())
		{
			childEvent->child()->installEventFilter(this);
		}
	}
	else if (event->type() == QEvent::MouseButtonPress || event->type() == QEvent::MouseButtonDblClick || event->type() == QEvent::Wheel)
	{
		const QMouseEvent *mouseEvent(static_cast<QMouseEvent*>(event));

		if (mouseEvent)
		{
			setClickPosition(mouseEvent->pos());
			updateHitTestResult(mouseEvent->pos());

			if (mouseEvent->button() == Qt::LeftButton && !getCurrentHitTestResult().linkUrl.isEmpty())
			{
				m_lastUrlClickTime = QDateTime::currentDateTime();
			}
		}

		QVector<GesturesManager::GesturesContext> contexts;

		if (getCurrentHitTestResult().flags.testFlag(HitTestResult::IsContentEditableTest))
		{
			contexts.append(GesturesManager::ContentEditableContext);
		}

		if (getCurrentHitTestResult().linkUrl.isValid())
		{
			contexts.append(GesturesManager::LinkContext);
		}

		contexts.append(GesturesManager::GenericContext);

		if ((!mouseEvent || !isScrollBar(mouseEvent->pos())) && GesturesManager::startGesture(object, event, contexts))
		{
			return true;
		}

		if (event->type() == QEvent::MouseButtonDblClick && mouseEvent->button() == Qt::LeftButton && SettingsManager::getOption(SettingsManager::Browser_ShowSelectionContextMenuOnDoubleClickOption).toBool())
		{
			const HitTestResult hitResult(getHitTestResult(mouseEvent->pos()));

			if (!hitResult.flags.testFlag(HitTestResult::IsContentEditableTest) && hitResult.tagName != QLatin1String("textarea") && hitResult.tagName!= QLatin1String("select") && hitResult.tagName != QLatin1String("input"))
			{
				setClickPosition(mouseEvent->pos());

				QTimer::singleShot(250, this, SLOT(showContextMenu()));
			}
		}
	}
	else if (object == m_webView && event->type() == QEvent::ContextMenu)
	{
		const QContextMenuEvent *contextMenuEvent(static_cast<QContextMenuEvent*>(event));

		if (contextMenuEvent && contextMenuEvent->reason() != QContextMenuEvent::Mouse)
		{
			triggerAction(ActionsManager::ContextMenuAction, {{QLatin1String("context"), contextMenuEvent->reason()}});
		}
	}
	else if (object == m_webView && (event->type() == QEvent::Move || event->type() == QEvent::Resize))
	{
		emit progressBarGeometryChanged();
	}
	else if (event->type() == QEvent::ToolTip)
	{
		QHelpEvent *helpEvent(static_cast<QHelpEvent*>(event));

		if (helpEvent)
		{
			handleToolTipEvent(helpEvent, m_webView);
		}

		return true;
	}
	else if (event->type() == QEvent::ShortcutOverride)
	{
		QEventLoop eventLoop;

		m_page->runJavaScript(QLatin1String("var element = document.body.querySelector(':focus'); var tagName = (element ? element.tagName.toLowerCase() : ''); var result = false; if (tagName == 'textarea' || tagName == 'input') { var type = (element.type ? element.type.toLowerCase() : ''); if ((type == '' || tagName == 'textarea' || type == 'text' || type == 'search') && !element.hasAttribute('readonly') && !element.hasAttribute('disabled')) { result = true; } } result;"), [&](const QVariant &result)
		{
			m_isEditing = result.toBool();

			eventLoop.quit();
		});

		connect(this, SIGNAL(aboutToReload()), &eventLoop, SLOT(quit()));
		connect(this, SIGNAL(destroyed()), &eventLoop, SLOT(quit()));

		eventLoop.exec();

		if (m_isEditing)
		{
			event->accept();

			return true;
		}
	}

	return QObject::eventFilter(object, event);
}

}
