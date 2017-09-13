/**************************************************************************
* Otter Browser: Web browser controlled by the user, not vice-versa.
* Copyright (C) 2013 - 2017 Michal Dutkiewicz aka Emdek <michal@emdek.pl>
* Copyright (C) 2014 Jan Bajer aka bajasoft <jbajer@gmail.com>
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

#include "QtWebKitWebBackend.h"
#include "QtWebKitHistoryInterface.h"
#include "QtWebKitPage.h"
#include "QtWebKitWebWidget.h"
#include "../../../../core/NetworkManagerFactory.h"
#include "../../../../core/SettingsManager.h"
#include "../../../../core/Utils.h"

#include <QtCore/QCoreApplication>
#include <QtCore/QDir>
#include <QtCore/QPointer>
#include <QtCore/QRegularExpression>
#include <QtWebKit/QWebHistoryInterface>
#include <QtWebKit/QWebSettings>

namespace Otter
{

QtWebKitWebBackend* QtWebKitWebBackend::m_instance(nullptr);
QPointer<WebWidget> QtWebKitWebBackend::m_activeWidget(nullptr);
QMap<QString, QString> QtWebKitWebBackend::m_userAgentComponents;
QMap<QString, QString> QtWebKitWebBackend::m_userAgents;
#ifndef OTTER_ENABLE_QTWEBKIT_LEGACY
int QtWebKitWebBackend::m_enableMediaOption(-1);
int QtWebKitWebBackend::m_enableMediaSourceOption(-1);
int QtWebKitWebBackend::m_enableWebSecurityOption(-1);
#endif

QtWebKitWebBackend::QtWebKitWebBackend(QObject *parent) : WebBackend(parent),
	m_isInitialized(false)
{
	m_instance = this;
#ifndef OTTER_ENABLE_QTWEBKIT_LEGACY
	m_enableMediaOption = SettingsManager::registerOption(QLatin1String("QtWebKitBackend/EnableMedia"), SettingsManager::BooleanType, true);
	m_enableMediaSourceOption = SettingsManager::registerOption(QLatin1String("QtWebKitBackend/EnableMediaSource"), SettingsManager::BooleanType, false);
	m_enableWebSecurityOption = SettingsManager::registerOption(QLatin1String("QtWebKitBackend/EnableWebSecurity"), SettingsManager::BooleanType, true);
#endif

	const QString cachePath(SessionsManager::getCachePath());

	if (!cachePath.isEmpty())
	{
		QDir().mkpath(cachePath);

		QWebSettings::setIconDatabasePath(cachePath);
		QWebSettings::setOfflineStoragePath(cachePath + QLatin1String("/offlineStorage/"));
		QWebSettings::setOfflineWebApplicationCachePath(cachePath + QLatin1String("/offlineWebApplicationCache/"));
		QWebSettings::globalSettings()->setLocalStoragePath(cachePath + QLatin1String("/localStorage/"));
	}

	QtWebKitPage *page(new QtWebKitPage());
	const QRegularExpression platformExpression(QLatin1String("(\\([^\\)]+\\))"));

	m_userAgentComponents[QLatin1String("platform")] = platformExpression.match(page->getDefaultUserAgent()).captured(1);
	m_userAgentComponents[QLatin1String("engineVersion")] = QLatin1String("AppleWebKit/") + qWebKitVersion() + QLatin1String(" (KHTML, like Gecko)");
	m_userAgentComponents[QLatin1String("applicationVersion")] = QCoreApplication::applicationName() + QLatin1Char('/') + QCoreApplication::applicationVersion();

	page->deleteLater();
}

void QtWebKitWebBackend::handleOptionChanged(int identifier)
{
	switch (identifier)
	{
		case SettingsManager::Browser_OfflineStorageLimitOption:
			QWebSettings::globalSettings()->setOfflineStorageDefaultQuota(SettingsManager::getOption(SettingsManager::Browser_OfflineStorageLimitOption).toInt() * 1024);

			return;
		case SettingsManager::Browser_OfflineWebApplicationCacheLimitOption:
			QWebSettings::globalSettings()->setOfflineWebApplicationCacheQuota(SettingsManager::getOption(SettingsManager::Browser_OfflineWebApplicationCacheLimitOption).toInt() * 1024);

			return;
		case SettingsManager::Browser_PrintElementBackgroundsOption:
			QWebSettings::globalSettings()->setAttribute(QWebSettings::PrintElementBackgrounds, SettingsManager::getOption(SettingsManager::Browser_PrintElementBackgroundsOption).toBool());

			return;
		case SettingsManager::Cache_PagesInMemoryLimitOption:
			QWebSettings::setMaximumPagesInCache(SettingsManager::getOption(SettingsManager::Cache_PagesInMemoryLimitOption).toInt());

			return;
		case SettingsManager::Interface_EnableSmoothScrollingOption:
			QWebSettings::globalSettings()->setAttribute(QWebSettings::ScrollAnimatorEnabled, SettingsManager::getOption(SettingsManager::Interface_EnableSmoothScrollingOption).toBool());

			return;
		default:
			break;
	}

	if (SettingsManager::getOptionName(identifier).startsWith(QLatin1String("Content/")))
	{
		QWebSettings::globalSettings()->setAttribute(QWebSettings::ZoomTextOnly, SettingsManager::getOption(SettingsManager::Content_ZoomTextOnlyOption).toBool());
		QWebSettings::globalSettings()->setFontSize(QWebSettings::DefaultFontSize, SettingsManager::getOption(SettingsManager::Content_DefaultFontSizeOption).toInt());
		QWebSettings::globalSettings()->setFontSize(QWebSettings::DefaultFixedFontSize, SettingsManager::getOption(SettingsManager::Content_DefaultFixedFontSizeOption).toInt());
		QWebSettings::globalSettings()->setFontSize(QWebSettings::MinimumFontSize, SettingsManager::getOption(SettingsManager::Content_MinimumFontSizeOption).toInt());
		QWebSettings::globalSettings()->setFontFamily(QWebSettings::StandardFont, SettingsManager::getOption(SettingsManager::Content_StandardFontOption).toString());
		QWebSettings::globalSettings()->setFontFamily(QWebSettings::FixedFont, SettingsManager::getOption(SettingsManager::Content_FixedFontOption).toString());
		QWebSettings::globalSettings()->setFontFamily(QWebSettings::SerifFont, SettingsManager::getOption(SettingsManager::Content_SerifFontOption).toString());
		QWebSettings::globalSettings()->setFontFamily(QWebSettings::SansSerifFont, SettingsManager::getOption(SettingsManager::Content_SansSerifFontOption).toString());
		QWebSettings::globalSettings()->setFontFamily(QWebSettings::CursiveFont, SettingsManager::getOption(SettingsManager::Content_CursiveFontOption).toString());
		QWebSettings::globalSettings()->setFontFamily(QWebSettings::FantasyFont, SettingsManager::getOption(SettingsManager::Content_FantasyFontOption).toString());
	}
	else if (SettingsManager::getOptionName(identifier).startsWith(QLatin1String("Permissions/")))
	{
		const bool arePluginsEnabled(SettingsManager::getOption(SettingsManager::Permissions_EnablePluginsOption).toString() != QLatin1String("disabled"));

		QWebSettings::globalSettings()->setAttribute(QWebSettings::DnsPrefetchEnabled, true);
		QWebSettings::globalSettings()->setAttribute(QWebSettings::AutoLoadImages, (SettingsManager::getOption(SettingsManager::Permissions_EnableImagesOption).toString() != QLatin1String("onlyCached")));
		QWebSettings::globalSettings()->setAttribute(QWebSettings::PluginsEnabled, arePluginsEnabled);
		QWebSettings::globalSettings()->setAttribute(QWebSettings::JavaEnabled, arePluginsEnabled);
		QWebSettings::globalSettings()->setAttribute(QWebSettings::JavascriptEnabled, SettingsManager::getOption(SettingsManager::Permissions_EnableJavaScriptOption).toBool());
		QWebSettings::globalSettings()->setAttribute(QWebSettings::JavascriptCanAccessClipboard, SettingsManager::getOption(SettingsManager::Permissions_ScriptsCanAccessClipboardOption).toBool());
		QWebSettings::globalSettings()->setAttribute(QWebSettings::JavascriptCanOpenWindows, (SettingsManager::getOption(SettingsManager::Permissions_ScriptsCanOpenWindowsOption).toString() != QLatin1String("blockAll")));
		QWebSettings::globalSettings()->setAttribute(QWebSettings::WebGLEnabled, SettingsManager::getOption(SettingsManager::Permissions_EnableWebglOption).toBool());
		QWebSettings::globalSettings()->setAttribute(QWebSettings::LocalStorageEnabled, SettingsManager::getOption(SettingsManager::Permissions_EnableLocalStorageOption).toBool());
		QWebSettings::globalSettings()->setAttribute(QWebSettings::OfflineStorageDatabaseEnabled, SettingsManager::getOption(SettingsManager::Permissions_EnableOfflineStorageDatabaseOption).toBool());
		QWebSettings::globalSettings()->setAttribute(QWebSettings::OfflineWebApplicationCacheEnabled, SettingsManager::getOption(SettingsManager::Permissions_EnableOfflineWebApplicationCacheOption).toBool());
	}
}

void QtWebKitWebBackend::setActiveWidget(WebWidget *widget)
{
	m_activeWidget = widget;

	emit activeDictionaryChanged(getActiveDictionary());
}

WebWidget* QtWebKitWebBackend::createWidget(const QVariantMap &parameters, ContentsWidget *parent)
{
	if (!m_isInitialized)
	{
		m_isInitialized = true;

		QWebHistoryInterface::setDefaultInterface(new QtWebKitHistoryInterface(this));

#ifndef OTTER_ENABLE_QTWEBKIT_LEGACY
		QStringList pluginSearchPaths(QWebSettings::pluginSearchPaths());
		pluginSearchPaths.append(QDir::toNativeSeparators(QCoreApplication::applicationDirPath()));

		QWebSettings::setPluginSearchPaths(pluginSearchPaths);
#endif
		QWebSettings::setMaximumPagesInCache(SettingsManager::getOption(SettingsManager::Cache_PagesInMemoryLimitOption).toInt());
		QWebSettings::globalSettings()->setAttribute(QWebSettings::DnsPrefetchEnabled, true);
#ifndef OTTER_ENABLE_QTWEBKIT_LEGACY
		QWebSettings::globalSettings()->setAttribute(QWebSettings::FullScreenSupportEnabled, true);
#endif
		QWebSettings::globalSettings()->setAttribute(QWebSettings::JavascriptCanCloseWindows, true);
		QWebSettings::globalSettings()->setAttribute(QWebSettings::PrintElementBackgrounds, SettingsManager::getOption(SettingsManager::Browser_PrintElementBackgroundsOption).toBool());
		QWebSettings::globalSettings()->setAttribute(QWebSettings::ScrollAnimatorEnabled, SettingsManager::getOption(SettingsManager::Interface_EnableSmoothScrollingOption).toBool());
		QWebSettings::globalSettings()->setOfflineStorageDefaultQuota(SettingsManager::getOption(SettingsManager::Browser_OfflineStorageLimitOption).toInt() * 1024);
		QWebSettings::globalSettings()->setOfflineWebApplicationCacheQuota(SettingsManager::getOption(SettingsManager::Browser_OfflineWebApplicationCacheLimitOption).toInt() * 1024);

		handleOptionChanged(SettingsManager::Content_DefaultFontSizeOption);
		handleOptionChanged(SettingsManager::Permissions_EnableFullScreenOption);

		connect(SettingsManager::getInstance(), SIGNAL(optionChanged(int,QVariant)), this, SLOT(handleOptionChanged(int)));
	}

	QtWebKitWebWidget *widget(new QtWebKitWebWidget(parameters, this, nullptr, parent));

	connect(widget, SIGNAL(widgetActivated(WebWidget*)), this, SLOT(setActiveWidget(WebWidget*)));

	return widget;
}

QtWebKitWebBackend* QtWebKitWebBackend::getInstance()
{
	return m_instance;
}

QString QtWebKitWebBackend::getName() const
{
	return QLatin1String("qtwebkit");
}

QString QtWebKitWebBackend::getTitle() const
{
#ifdef OTTER_ENABLE_QTWEBKIT_LEGACY
	return tr("WebKit Backend (legacy)");
#else
	return tr("WebKit Backend");
#endif
}

QString QtWebKitWebBackend::getDescription() const
{
	return tr("Backend utilizing QtWebKit module");
}

QString QtWebKitWebBackend::getVersion() const
{
	return QCoreApplication::applicationVersion();
}

QString QtWebKitWebBackend::getEngineVersion() const
{
	return qWebKitVersion();
}

QString QtWebKitWebBackend::getSslVersion() const
{
	return (QSslSocket::supportsSsl() ? QSslSocket::sslLibraryVersionString() : QString());
}

QString QtWebKitWebBackend::getUserAgent(const QString &pattern) const
{
	if (!pattern.isEmpty())
	{
		if (m_userAgents.contains(pattern))
		{
			return (m_userAgents[pattern].isEmpty() ? pattern : m_userAgents[pattern]);
		}

		QString userAgent(pattern);
		QMap<QString, QString>::iterator iterator;

		for (iterator = m_userAgentComponents.begin(); iterator != m_userAgentComponents.end(); ++iterator)
		{
			userAgent = userAgent.replace(QStringLiteral("{%1}").arg(iterator.key()), iterator.value());
		}

		m_userAgents[pattern] = ((pattern == userAgent) ? QString() : userAgent);

		return userAgent;
	}

	const UserAgentDefinition userAgent(NetworkManagerFactory::getUserAgent(SettingsManager::getOption(SettingsManager::Network_UserAgentOption).toString()));

	return ((userAgent.value.isEmpty()) ? QString() : getUserAgent(userAgent.value));
}

QUrl QtWebKitWebBackend::getHomePage() const
{
	return QUrl(QLatin1String("https://otter-browser.org/"));
}

QString QtWebKitWebBackend::getActiveDictionary()
{
	if (m_activeWidget && m_activeWidget->getOption(SettingsManager::Browser_EnableSpellCheckOption, m_activeWidget->getUrl()).toBool())
	{
		const QString dictionary(m_activeWidget->getOption(SettingsManager::Browser_SpellCheckDictionaryOption, m_activeWidget->getUrl()).toString());

		return (dictionary.isEmpty() ? SpellCheckManager::getDefaultDictionary() : dictionary);
	}

	return QString();
}

WebBackend::BackendCapabilities QtWebKitWebBackend::getCapabilities() const
{
	return (CacheManagementCapability | CookiesManagementCapability | PasswordsManagementCapability | PluginsOnDemandCapability | UserScriptsCapability | UserStyleSheetsCapability | GlobalCookiesPolicyCapability | GlobalContentFilteringCapability | GlobalDoNotTrackCapability | GlobalProxyCapability | GlobalReferrerCapability | GlobalUserAgentCapability | TabCookiesPolicyCapability | TabContentFilteringCapability | TabDoNotTrackCapability | TabProxyCapability | TabReferrerCapability | TabUserAgentCapability | FindInPageHighlightAllCapability);
}

int QtWebKitWebBackend::getOptionIdentifier(QtWebKitWebBackend::OptionIdentifier identifier)
{
#ifndef OTTER_ENABLE_QTWEBKIT_LEGACY
	switch (identifier)
	{
		case QtWebKitBackend_EnableMediaOption:
			return m_enableMediaOption;
		case QtWebKitBackend_EnableMediaSourceOption:
			return m_enableMediaSourceOption;
		case QtWebKitBackend_EnableWebSecurityOption:
			return m_enableWebSecurityOption;
		default:
			return -1;
	}
#endif

	return -1;
}

bool QtWebKitWebBackend::requestThumbnail(const QUrl &url, const QSize &size)
{
	connect(new QtWebKitThumbnailFetchJob(url, size, this), SIGNAL(thumbnailAvailable(QUrl,QPixmap,QString)), this, SIGNAL(thumbnailAvailable(QUrl,QPixmap,QString)));

	return true;
}

QtWebKitThumbnailFetchJob::QtWebKitThumbnailFetchJob(const QUrl &url, const QSize &size, QObject *parent) : QObject(parent),
	m_page(new QtWebKitPage(url)),
	m_url(url),
	m_size(size)
{
	m_page->setParent(this);

	connect(m_page, SIGNAL(loadFinished(bool)), this, SLOT(handlePageLoadFinished(bool)));
}

void QtWebKitThumbnailFetchJob::handlePageLoadFinished(bool result)
{
	if (!result)
	{
		deleteLater();

		emit thumbnailAvailable(m_url, QPixmap(), QString());

		return;
	}

	QPixmap pixmap;
	QSize contentsSize(m_page->mainFrame()->contentsSize());

	if (contentsSize.isNull())
	{
		m_page->setViewportSize(QSize(1280, 760));

		contentsSize = m_page->mainFrame()->contentsSize();
	}

	if (!m_size.isNull() && !contentsSize.isNull())
	{
		if (contentsSize.width() < m_size.width())
		{
			contentsSize.setWidth(m_size.width());
		}
		else if (contentsSize.width() > 2000)
		{
			contentsSize.setWidth(2000);
		}

		contentsSize.setHeight(m_size.height() * (qreal(contentsSize.width()) / m_size.width()));

		m_page->setViewportSize(contentsSize);

		if (!contentsSize.isNull())
		{
			pixmap = QPixmap(contentsSize);
			pixmap.fill(Qt::white);

			QPainter painter(&pixmap);

			m_page->mainFrame()->render(&painter, QWebFrame::ContentsLayer, QRegion(QRect(QPoint(0, 0), contentsSize)));

			painter.end();

			if (pixmap.size() != m_size)
			{
				pixmap = pixmap.scaled(m_size, Qt::KeepAspectRatio, Qt::SmoothTransformation);
			}
		}
	}

	deleteLater();

	emit thumbnailAvailable(m_url, pixmap, m_page->mainFrame()->title());
}

}
