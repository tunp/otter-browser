/**************************************************************************
* Otter Browser: Web browser controlled by the user, not vice-versa.
* Copyright (C) 2013 - 2017 Michal Dutkiewicz aka Emdek <michal@emdek.pl>
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

#ifndef OTTER_QTWEBKITPAGE_H
#define OTTER_QTWEBKITPAGE_H

#include "../../../../core/SessionsManager.h"

#include <QtWebKit/QWebElement>
#include <QtWebKitWidgets/QWebPage>

namespace Otter
{

class QtWebKitNetworkManager;
class QtWebKitPage;
class QtWebKitWebWidget;
class WebWidget;

class QtWebKitFrame : public QObject
{
	Q_OBJECT

public:
	explicit QtWebKitFrame(QWebFrame *frame, QtWebKitWebWidget *parent);

	void runUserScripts(const QUrl &url) const;
	bool isErrorPage() const;

protected:
	void applyContentBlockingRules(const QStringList &rules, bool remove);

protected slots:
	void handleErrorPageChanged(QWebFrame *frame, bool isErrorPage);
	void handleLoadFinished();

private:
	QWebFrame *m_frame;
	QtWebKitWebWidget *m_widget;
	bool m_isErrorPage;
};

class QtWebKitPage final : public QWebPage
{
	Q_OBJECT

public:
	explicit QtWebKitPage(QtWebKitNetworkManager *networkManager, QtWebKitWebWidget *parent);
	~QtWebKitPage();

	void triggerAction(WebAction action, bool isChecked = false) override;
	QtWebKitFrame* getMainFrame() const;
	QVariant runScript(const QString &path, QWebElement element = QWebElement());
	bool event(QEvent *event) override;
	bool extension(Extension extension, const ExtensionOption *option = nullptr, ExtensionReturn *output = nullptr) override;
	bool shouldInterruptJavaScript() override;
	bool supportsExtension(Extension extension) const override;
	bool isErrorPage() const;
	bool isPopup() const;
	bool isViewingMedia() const;

public slots:
	void markAsErrorPage();
	void updateStyleSheets(const QUrl &url = {});

protected:
	QtWebKitPage();
	explicit QtWebKitPage(const QUrl &url);

	void markAsPopup();
	void javaScriptAlert(QWebFrame *frame, const QString &message) override;
#ifdef OTTER_ENABLE_QTWEBKIT_LEGACY
	void javaScriptConsoleMessage(const QString &note, int line, const QString &source) override;
#endif
	QWebPage* createWindow(WebWindowType type) override;
	QtWebKitWebWidget* createWidget(SessionsManager::OpenHints hints);
	QString chooseFile(QWebFrame *frame, const QString &suggestedFile) override;
	QString userAgentForUrl(const QUrl &url) const override;
	QString getDefaultUserAgent() const;
	QVariant getOption(int identifier) const;
	bool acceptNavigationRequest(QWebFrame *frame, const QNetworkRequest &request, QWebPage::NavigationType type) override;
	bool javaScriptConfirm(QWebFrame *frame, const QString &message) override;
	bool javaScriptPrompt(QWebFrame *frame, const QString &message, const QString &defaultValue, QString *result) override;

protected slots:
	void validatePopup(const QUrl &url);
	void handleOptionChanged(int identifier);
	void handleLoadFinished();
	void handleFrameCreation(QWebFrame *frame);
#ifndef OTTER_ENABLE_QTWEBKIT_LEGACY
	void handleConsoleMessage(MessageSource category, MessageLevel level, const QString &message, int line, const QString &source);
#endif

private:
	QtWebKitWebWidget *m_widget;
	QtWebKitNetworkManager *m_networkManager;
	QtWebKitFrame *m_mainFrame;
	QVector<QtWebKitPage*> m_popups;
	bool m_isIgnoringJavaScriptPopups;
	bool m_isPopup;
	bool m_isViewingMedia;

signals:
	void requestedNewWindow(WebWidget *widget, SessionsManager::OpenHints hints);
	void requestedPopupWindow(const QUrl &parentUrl, const QUrl &popupUrl);
	void aboutToNavigate(const QUrl &url, QWebFrame *frame, QWebPage::NavigationType navigationType);
	void errorPageChanged(QWebFrame *frame, bool isVisible);
	void viewingMediaChanged(bool viewingMedia);

friend class QtWebKitThumbnailFetchJob;
friend class QtWebKitWebBackend;
};

}

#endif
