/**************************************************************************
* Otter Browser: Web browser controlled by the user, not vice-versa.
* Copyright (C) 2016 - 2017 Michal Dutkiewicz aka Emdek <michal@emdek.pl>
* Copyright (C) 2016 Jan Bajer aka bajasoft <jbajer@gmail.com>
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

#include "ContentBlockingInformationWidget.h"
#include "../../../core/Console.h"
#include "../../../core/Application.h"
#include "../../../core/ContentBlockingManager.h"
#include "../../../core/ContentBlockingProfile.h"
#include "../../../core/ThemesManager.h"
#include "../../../core/Utils.h"
#include "../../../ui/ContentsWidget.h"
#include "../../../ui/ToolBarWidget.h"
#include "../../../ui/Window.h"

#include <QtWidgets/QStyleOptionToolButton>
#include <QtWidgets/QStylePainter>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QWidgetAction>
#include <QDir>

namespace Otter
{

ContentBlockingInformationWidget::ContentBlockingInformationWidget(Window *window, const ToolBarsManager::ToolBarDefinition::Entry &definition, QWidget *parent) : ToolButtonWidget(definition, parent),
	m_window(window),
	m_elementsMenu(nullptr),
	m_profilesMenu(nullptr),
	m_hostsMenu(nullptr),
	m_amount(0),
	m_isContentBlockingEnabled(false)
{
	QMenu *menu(new QMenu(this));

	m_profilesMenu = menu->addMenu(tr("Active Profiles"));
	m_elementsMenu = menu->addMenu(tr("Blocked Elements"));
	m_hostsMenu = menu->addMenu(tr("Allowed 3rd-party Hosts"));

	setMenu(menu);
	setPopupMode(QToolButton::MenuButtonPopup);
	setIcon(ThemesManager::createIcon(QLatin1String("content-blocking")));
	setDefaultAction(new QAction(this));
	setWindow(window);

	const ToolBarWidget *toolBar(qobject_cast<ToolBarWidget*>(parent));

	if (toolBar && toolBar->getIdentifier() != ToolBarsManager::AddressBar)
	{
		connect(toolBar, &ToolBarWidget::windowChanged, this, &ContentBlockingInformationWidget::setWindow);
	}

	connect(m_elementsMenu, &QMenu::aboutToShow, this, &ContentBlockingInformationWidget::populateElementsMenu);
	connect(m_elementsMenu, &QMenu::triggered, this, &ContentBlockingInformationWidget::openElement);
	connect(m_profilesMenu, &QMenu::aboutToShow, this, &ContentBlockingInformationWidget::populateProfilesMenu);
	connect(m_profilesMenu, &QMenu::triggered, this, &ContentBlockingInformationWidget::toggleOption);
	connect(menu, &QMenu::aboutToShow, this, &ContentBlockingInformationWidget::populateHostsMenu);
	connect(menu, &QMenu::aboutToHide, this, &ContentBlockingInformationWidget::saveHosts);
	connect(defaultAction(), &QAction::triggered, this, &ContentBlockingInformationWidget::toggleContentBlocking);
}

void ContentBlockingInformationWidget::resizeEvent(QResizeEvent *event)
{
	ToolButtonWidget::resizeEvent(event);

	updateState();
}

void ContentBlockingInformationWidget::clear()
{
	m_amount = 0;

	updateState();
}

void ContentBlockingInformationWidget::openElement(QAction *action)
{
	if (action)
	{
		Application::triggerAction(ActionsManager::OpenUrlAction, {{QLatin1String("url"), QUrl(action->statusTip())}}, parentWidget());
	}
}

void ContentBlockingInformationWidget::toggleContentBlocking()
{
	if (m_window && !m_window->isAboutToClose())
	{
		m_isContentBlockingEnabled = !m_window->getOption(SettingsManager::ContentBlocking_EnableContentBlockingOption).toBool();

		m_window->setOption(SettingsManager::ContentBlocking_EnableContentBlockingOption, m_isContentBlockingEnabled);

		updateState();
	}
}

void ContentBlockingInformationWidget::toggleOption(QAction *action)
{
	if (action && m_window && !action->data().isNull())
	{
		const QString profile(action->data().toString());
		QStringList profiles(m_window->getOption(SettingsManager::ContentBlocking_ProfilesOption).toStringList());

		if (!action->isChecked())
		{
			profiles.removeAll(profile);
		}
		else if (!profiles.contains(profile))
		{
			profiles.append(profile);
		}

		m_window->setOption(SettingsManager::ContentBlocking_ProfilesOption, profiles);
	}
}

void ContentBlockingInformationWidget::populateElementsMenu()
{
	m_elementsMenu->clear();

	if (!m_window || !m_window->getWebWidget())
	{
		return;
	}

	const QVector<NetworkManager::ResourceInformation> requests(m_window->getWebWidget()->getBlockedRequests().mid(m_amount - 50));

	for (int i = 0; i < requests.count(); ++i)
	{
		QString type;

		switch (requests.at(i).resourceType)
		{
			case NetworkManager::MainFrameType:
				type = tr("main frame");

				break;
			case NetworkManager::SubFrameType:
				type = tr("subframe");

				break;
			case NetworkManager::PopupType:
				type = tr("pop-up");

				break;
			case NetworkManager::StyleSheetType:
				type = tr("stylesheet");

				break;
			case NetworkManager::ScriptType:
				type = tr("script");

				break;
			case NetworkManager::ImageType:
				type = tr("image");

				break;
			case NetworkManager::ObjectType:
				type = tr("object");

				break;
			case NetworkManager::ObjectSubrequestType:
				type = tr("object subrequest");

				break;
			case NetworkManager::XmlHttpRequestType:
				type = tr("XHR");

				break;
			case NetworkManager::WebSocketType:
				type = tr("WebSocket");

				break;
			default:
				type = tr("other");

				break;
		}

		QAction *action(m_elementsMenu->addAction(QStringLiteral("%1\t [%2]").arg(Utils::elideText(requests.at(i).url.toString(), m_elementsMenu)).arg(type)));
		action->setStatusTip(requests.at(i).url.toString());
	}
}

void ContentBlockingInformationWidget::populateProfilesMenu()
{
	m_profilesMenu->clear();

	if (!m_window || !m_window->getWebWidget())
	{
		return;
	}

	QAction *enableContentBlockingAction(m_profilesMenu->addAction(tr("Enable Content Blocking")));
	enableContentBlockingAction->setCheckable(true);
	enableContentBlockingAction->setChecked(m_window->getOption(SettingsManager::ContentBlocking_EnableContentBlockingOption).toBool());

	m_profilesMenu->addSeparator();

	const QVector<NetworkManager::ResourceInformation> requests(m_window->getWebWidget()->getBlockedRequests());
	QHash<QString, int> amounts;

	for (int i = 0; i < requests.count(); ++i)
	{
		const QString profile(requests.at(i).metaData.value(NetworkManager::ContentBlockingProfileMetaData).toString());

		if (amounts.contains(profile))
		{
			++amounts[profile];
		}
		else
		{
			amounts[profile] = 1;
		}
	}

	const QVector<ContentBlockingProfile*> profiles(ContentBlockingManager::getProfiles());
	const QStringList enabledProfiles(m_window->getOption(SettingsManager::ContentBlocking_ProfilesOption).toStringList());

	for (int i = 0; i < profiles.count(); ++i)
	{
		if (profiles.at(i))
		{
			const int amount(amounts.value(profiles.at(i)->getName()));
			const QString title(Utils::elideText(profiles.at(i)->getTitle(), m_profilesMenu));
			QAction *profileAction(m_profilesMenu->addAction((amount > 0) ? QStringLiteral("%1 (%2)").arg(title).arg(amount) : title));
			profileAction->setData(profiles.at(i)->getName());
			profileAction->setCheckable(true);
			profileAction->setChecked(enabledProfiles.contains(profiles.at(i)->getName()));
		}
	}

	connect(enableContentBlockingAction, &QAction::triggered, this, &ContentBlockingInformationWidget::toggleContentBlocking);
}

void ContentBlockingInformationWidget::populateHostsMenu()
{
	m_hostsMenu->clear();
	m_tempHosts.clear();
	m_hostsMenu->setEnabled(0);

	if (!m_window || !m_window->getContentsWidget()->getWebWidget())
	{
		return;
	}
	if (m_window->getOption(SettingsManager::ContentBlocking_ProfilesOption).toStringList().contains(QLatin1String("3rdpartyblock")))
	{
		const QString mainHost = m_window->getContentsWidget()->getWebWidget()->getUrl().host();
		QFile file(SessionsManager::getWritableDataPath(QLatin1String("contentBlocking/3rdpartyblock.txt")));
		QHash<QString, bool> hostsEnabled;
		QHash<QString, unsigned int> hosts;
		QHash<QString, bool> secondLevelsEnabled;
		QHash<QString, unsigned int> secondLevels;

		if (file.open(QIODevice::ReadOnly | QIODevice::Text))
		{
			QTextStream stream(&file);
			while (!stream.atEnd())
			{
				const QString line(stream.readLine().trimmed());
				if (line.startsWith(QLatin1String("@@||")))
				{
					const QStringList parts(line.split(QLatin1String("^$domain=")));
					if (parts.count() == 2)
					{
						if (parts[1] == mainHost)
						{
							const QString host(parts[0].mid(4));
							QStringList parts(host.split(QLatin1Char('.')));
							if (parts.count() > 2)
							{
								hostsEnabled[host] = 1;
								hosts[host] = 0;
							}
							else
							{
								secondLevelsEnabled[host] = 1;
								secondLevels[host] = 0;
							}
						}
						else
						{
							m_tempHosts.append(line);
						}
					}
				}
			}
			file.close();
		}

		const QVector<NetworkManager::ResourceInformation> requests(m_window->getContentsWidget()->getWebWidget()->getBlockedRequests());

		for (int i = 0; i < requests.count(); ++i)
		{
			unsigned int count = 0;
			const QString host = requests.at(i).url.host();
			QStringList parts(host.split(QLatin1Char('.')));
			if (hosts.contains(host))
			{
				count = hosts[host];
			}
			hosts[host] = ++count;
			count = 0;
			if (parts.count() > 2)
			{
				QString secondLevel = parts.takeLast();
				if (parts.count() > 0)
				{
					secondLevel = QStringLiteral("%1.%2").arg(parts.takeLast()).arg(secondLevel);
				}
				if (secondLevels.contains(secondLevel))
				{
					count = secondLevels[secondLevel];
				}
				secondLevels[secondLevel] = ++count;
			}
		}
		if (secondLevels.count() || hosts.count())
		{
			populateHostsPart(secondLevelsEnabled, secondLevels, 1);
			m_hostsMenu->addSeparator();
			populateHostsPart(hostsEnabled, hosts, 0);
			setHostsDisabledState();
			m_hostsMenu->setEnabled(1);
		}
	}
}

void ContentBlockingInformationWidget::populateHostsPart(const QHash<QString, bool> &hostsEnabled, const QHash<QString, unsigned int> &hosts, const bool isSecondLevel)
{
	QMap <unsigned int, QVector<QString> > orderedHosts;
	QHashIterator<QString, unsigned int> hostsIt(hosts);
	while (hostsIt.hasNext())
	{
		hostsIt.next();
		orderedHosts[hostsIt.value()].push_back(hostsIt.key());
	}
	QMapIterator<unsigned int, QVector<QString> > orderedHostsIt(orderedHosts);
	while (orderedHostsIt.hasNext())
	{
		orderedHostsIt.next();
		for (int i = 0; i < orderedHostsIt.value().count(); ++i)
		{
			const QString host(orderedHostsIt.value().at(i));
			QCheckBox *checkbox = new QCheckBox(m_hostsMenu);
			checkbox->setText(QStringLiteral("%1\t [%2]").arg(Utils::elideText(host, m_hostsMenu)).arg(orderedHostsIt.key()));
			bool isEnabled = hostsEnabled.contains(host);
			checkbox->setChecked(isEnabled);
			checkbox->setProperty("origIsChecked", isEnabled);
			if (isSecondLevel)
			{
				connect(checkbox, SIGNAL(stateChanged(int)), this, SLOT(setHostsDisabledState()));
			}
			QWidgetAction *action = new QWidgetAction(m_hostsMenu);
			action->setProperty("isSecondLevel", isSecondLevel);
			action->setDefaultWidget(checkbox);
			action->setData(host);
			m_hostsMenu->addAction(action);
		}
	}
}

void ContentBlockingInformationWidget::saveHosts()
{
	if (!m_window || !m_window->getContentsWidget()->getWebWidget())
	{
		return;
	}

	QStringList profiles(m_window->getOption(SettingsManager::ContentBlocking_ProfilesOption).toStringList());
	if (profiles.contains(QLatin1String("3rdpartyblock")))
	{
		const QString mainHost = m_window->getContentsWidget()->getWebWidget()->getUrl().host();
		QStringList newLines;
		bool hasChanges = 0;
		QList<QAction *> actions = m_hostsMenu->actions();
		for (int i = 0; i < actions.size(); i++)
		{
			QWidgetAction *action = qobject_cast<QWidgetAction *>(actions.at(i));
			if (action)
			{
				QCheckBox *checkbox = qobject_cast<QCheckBox *>(action->defaultWidget());
				if (checkbox)
				{
					const bool isChecked = checkbox->isEnabled() && checkbox->isChecked();
					if (isChecked)
					{
						const QString remoteHost(action->data().toString());
						newLines.append(QStringLiteral("@@||%1^$domain=%2").arg(remoteHost).arg(mainHost));
					}
					if (checkbox->property("origIsChecked").toBool() != isChecked)
					{
						hasChanges = 1;
					}
				}
			}
		}

		if (hasChanges) {
			QDir().mkpath(SessionsManager::getWritableDataPath(QLatin1String("contentBlocking")));

			QFile file(SessionsManager::getWritableDataPath(QLatin1String("contentBlocking/3rdpartyblock.txt")));

			if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text))
			{
				Console::addMessage(QCoreApplication::translate("main", "Failed to create 3rd party rules file: %1").arg(file.errorString()), Console::OtherCategory, Console::ErrorLevel, file.fileName());
			}
			else
			{
				file.write(QStringLiteral("[AdBlock Plus 2.0]\n").toUtf8());
				file.write(QStringLiteral("*$third-party\n").toUtf8());

				for (int i = 0; i < m_tempHosts.size(); i++)
				{
					file.write(QString(m_tempHosts.at(i) + "\n").toUtf8());
				}
				for (int i = 0; i < newLines.size(); i++)
				{
					file.write(QString(newLines.at(i) + "\n").toUtf8());
				}

				ContentBlockingProfile *profile(ContentBlockingManager::getProfile(QLatin1String("3rdpartyblock")));

				// clear profile so that ContentBlockingManager will notice that its empty and reloads it
				if (profile)
				{
					profile->clear();
				}
				else
				{
					profile = new ContentBlockingProfile(QLatin1String("3rdpartyblock"), tr("3rd-party block"), QUrl(), QDateTime(), QStringList(), 0, ContentBlockingProfile::OtherCategory, ContentBlockingProfile::NoFlags);

					ContentBlockingManager::addProfile(profile);
				}
			}
		}
	}
}

void ContentBlockingInformationWidget::setHostsDisabledState()
{
	QHash<QString, int> secondLevels;
	const QList<QAction *> actions(m_hostsMenu->actions());
	for (int i = 0; i < actions.size(); i++)
	{
		QWidgetAction *action = qobject_cast<QWidgetAction *>(actions.at(i));
		if (action)
		{
			QCheckBox *checkbox = qobject_cast<QCheckBox *>(action->defaultWidget());
			if (checkbox)
			{
				const QString host = action->data().toString();
				if (action->property("isSecondLevel").toBool())
				{
					secondLevels[host] = checkbox->isChecked();
				}
				else
				{
					QStringList parts(host.split(QLatin1Char('.')));
					QString secondLevel(parts.takeLast());
					if (parts.count() > 0)
					{
						secondLevel = QStringLiteral("%1.%2").arg(parts.takeLast()).arg(secondLevel);
					}
					// if secondLevel is checked disable the full host checkbox
					if (secondLevels.contains(secondLevel))
					{
						checkbox->setEnabled(!secondLevels[secondLevel]);
					}
					else
					{
						checkbox->setEnabled(true);
					}
				}
			}
		}
	}
}

void ContentBlockingInformationWidget::handleRequest(const NetworkManager::ResourceInformation &request)
{
	Q_UNUSED(request)

	++m_amount;

	updateState();
}

void ContentBlockingInformationWidget::updateState()
{
	m_icon = (isCustomized() ? getOptions().value(QLatin1String("icon")).value<QIcon>() : QIcon());

	if (m_icon.isNull())
	{
		m_icon = ThemesManager::createIcon(QLatin1String("content-blocking"));
	}

	const int iconSize(this->iconSize().width());
	const int fontSize(qMax((iconSize / 2), 12));
	QFont font(this->font());
	font.setBold(true);
	font.setPixelSize(fontSize);

	QString label;

	if (m_amount > 999999)
	{
		label = QString::number(m_amount / 1000000) + QLatin1Char('M');
	}
	else if (m_amount > 999)
	{
		label = QString::number(m_amount / 1000) + QLatin1Char('K');
	}
	else
	{
		label = QString::number(m_amount);
	}

	const qreal labelWidth(QFontMetricsF(font).width(label));

	font.setPixelSize(fontSize * 0.8);

	const QRectF rectangle((iconSize - labelWidth), (iconSize - fontSize), labelWidth, fontSize);
	QPixmap pixmap(m_icon.pixmap(iconSize, iconSize, (m_isContentBlockingEnabled ? QIcon::Normal : QIcon::Disabled)));
	QPainter iconPainter(&pixmap);
	iconPainter.fillRect(rectangle, (m_isContentBlockingEnabled ? Qt::darkRed : Qt::darkGray));
	iconPainter.setFont(font);
	iconPainter.setPen(QColor(255, 255, 255, 230));
	iconPainter.drawText(rectangle, Qt::AlignCenter, label);

	m_icon = QIcon(pixmap);

	setText(getText());
	setToolTip(text());
	setIcon(m_icon);

	m_elementsMenu->setEnabled(m_amount > 0);
}

void ContentBlockingInformationWidget::setWindow(Window *window)
{
	if (m_window && !m_window->isAboutToClose())
	{
		disconnect(m_window, &Window::aboutToNavigate, this, &ContentBlockingInformationWidget::clear);
		disconnect(m_window, &Window::requestBlocked, this, &ContentBlockingInformationWidget::handleRequest);
	}

	m_window = window;
	m_amount = 0;

	if (window && window->getWebWidget())
	{
		m_amount = window->getWebWidget()->getBlockedRequests().count();
		m_isContentBlockingEnabled = (m_window->getOption(SettingsManager::ContentBlocking_EnableContentBlockingOption).toBool());

		connect(m_window, &Window::aboutToNavigate, this, &ContentBlockingInformationWidget::clear);
		connect(m_window, &Window::requestBlocked, this, &ContentBlockingInformationWidget::handleRequest);
	}
	else
	{
		m_isContentBlockingEnabled = false;
	}

	updateState();
	setEnabled(m_window);
}

QString ContentBlockingInformationWidget::getText() const
{
	QString text(tr("Blocked Elements: {amount}"));

	if (isCustomized())
	{
		const QVariantMap options(getOptions());

		if (options.contains(QLatin1String("text")))
		{
			text = options[QLatin1String("text")].toString();
		}
	}

	return text.replace(QLatin1String("{amount}"), QString::number(m_amount));
}

QIcon ContentBlockingInformationWidget::getIcon() const
{
	return m_icon;
}

}
