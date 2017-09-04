/**************************************************************************
* Otter Browser: Web browser controlled by the user, not vice-versa.
* Copyright (C) 2013 - 2017 Michal Dutkiewicz aka Emdek <michal@emdek.pl>
* Copyright (C) 2015 - 2016 Jan Bajer aka bajasoft <jbajer@gmail.com>
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

#ifndef OTTER_APPLICATION_H
#define OTTER_APPLICATION_H

#include "ActionsManager.h"
#include "UpdateChecker.h"
#include "SessionsManager.h"

#include <QtCore/QCommandLineParser>
#include <QtCore/QUrl>
#include <QtWidgets/QApplication>
#include <QtNetwork/QLocalServer>

namespace Otter
{

class MainWindow;
class Notification;
class PlatformIntegration;
class Style;
class TrayIcon;

class Application : public QApplication, public ActionExecutor
{
	Q_OBJECT

public:
	enum ReportOption
	{
		BasicReport = 0,
		EnvironmentReport = 1,
		KeyboardShortcutsReport = 2,
		PathsReport = 4,
		SettingsReport = 8,
		FullReport = (EnvironmentReport | KeyboardShortcutsReport | PathsReport | SettingsReport)
	};

	Q_DECLARE_FLAGS(ReportOptions, ReportOption)

	explicit Application(int &argc, char **argv);
	~Application();

	static void triggerAction(int identifier, const QVariantMap &parameters, QObject *target);
	static void removeWindow(MainWindow* window);
	static void showNotification(Notification *notification);
	static void handlePositionalArguments(QCommandLineParser *parser);
	static void setHidden(bool isHidden);
	static void setLocale(const QString &locale);
	static MainWindow* createWindow(const QVariantMap &parameters = {}, const SessionMainWindow &windows = SessionMainWindow());
	static Application* getInstance();
	static MainWindow* getWindow();
	static MainWindow* getActiveWindow();
	static Style* getStyle();
	static TrayIcon* getTrayIcon();
	static PlatformIntegration* getPlatformIntegration();
	static QCommandLineParser* getCommandLineParser();
	static QString createReport(ReportOptions options = FullReport);
	static QString getFullVersion();
	static QString getLocalePath();
	ActionsManager::ActionDefinition::State getActionState(int identifier, const QVariantMap &parameters = {}) const override;
	static QVector<MainWindow*> getWindows();
	static bool canClose();
	static bool isAboutToQuit();
	static bool isHidden();
	static bool isUpdating();
	static bool isRunning();

public slots:
	void triggerAction(int identifier, const QVariantMap &parameters = {}) override;
	void close();

protected slots:
	void openUrl(const QUrl &url);
	void periodicUpdateCheck();
	void handleOptionChanged(int identifier, const QVariant &value);
	void handleAboutToQuit();
	void handleNewConnection();
	void handleUpdateCheckResult(const QVector<UpdateChecker::UpdateInformation> &availableUpdates);
	void showUpdateDetails();

private:
	static Application *m_instance;
	static PlatformIntegration *m_platformIntegration;
	static TrayIcon *m_trayIcon;
	static QTranslator *m_qtTranslator;
	static QTranslator *m_applicationTranslator;
	static QLocalServer *m_localServer;
	static QPointer<MainWindow> m_activeWindow;
	static QString m_localePath;
	static QCommandLineParser m_commandLineParser;
	static QVector<MainWindow*> m_windows;
	static bool m_isAboutToQuit;
	static bool m_isHidden;
	static bool m_isUpdating;

signals:
	void windowAdded(MainWindow *window);
	void windowRemoved(MainWindow *window);
	void actionsStateChanged(const QVector<int> &identifiers);
};

}

Q_DECLARE_OPERATORS_FOR_FLAGS(Otter::Application::ReportOptions)

#endif
