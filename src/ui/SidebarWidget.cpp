/**************************************************************************
* Otter Browser: Web browser controlled by the user, not vice-versa.
* Copyright (C) 2014 - 2015 Piotr Wójcik <chocimier@tlen.pl>
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

#include "SidebarWidget.h"
#include "MainWindow.h"
#include "ResizerWidget.h"
#include "ToolBarWidget.h"
#include "WidgetFactory.h"
#include "../core/ActionsManager.h"
#include "../core/AddonsManager.h"
#include "../core/HistoryManager.h"
#include "../core/ThemesManager.h"
#include "../modules/widgets/action/ActionWidget.h"
#include "../modules/widgets/panelChooser/PanelChooserWidget.h"

#include "ui_SidebarWidget.h"

#include <QtWidgets/QInputDialog>
#include <QtWidgets/QMenu>

namespace Otter
{

SidebarWidget::SidebarWidget(ToolBarWidget *parent) : QWidget(parent),
	m_toolBarWidget(parent),
	m_resizerWidget(nullptr),
	m_ui(new Ui::SidebarWidget)
{
	m_ui->setupUi(this);

	m_resizerWidget = new ResizerWidget(m_ui->containerWidget, this);

	ToolBarsManager::ToolBarDefinition::Entry definition;
	definition.parameters[QLatin1String("sidebar")] = m_toolBarWidget->getIdentifier();

	QToolBar *toolbar(new QToolBar(this));
	toolbar->setIconSize(QSize(16, 16));
	toolbar->addWidget(new PanelChooserWidget(definition, this));
	toolbar->addWidget(new ActionWidget(ActionsManager::OpenPanelAction, nullptr, definition, this));

	QWidget *spacer(new QWidget(toolbar));
	spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

	toolbar->addWidget(spacer);
	toolbar->addWidget(new ActionWidget(ActionsManager::ClosePanelAction, nullptr, definition, this));

	m_ui->panelLayout->addWidget(toolbar);
	m_ui->panelsButton->setPopupMode(QToolButton::InstantPopup);
	m_ui->panelsButton->setIcon(ThemesManager::createIcon(QLatin1String("list-add")));
	m_ui->horizontalLayout->addWidget(m_resizerWidget);

	const int panelSize(m_toolBarWidget->getDefinition().panelSize);

	if (panelSize > 0)
	{
		m_ui->containerWidget->setMaximumWidth(panelSize);
	}

	updateLayout();
	updatePanels();

	connect(parent, SIGNAL(toolBarModified()), this, SLOT(updateLayout()));
	connect(parent, SIGNAL(toolBarModified()), this, SLOT(updatePanels()));
	connect(m_resizerWidget, SIGNAL(finished()), this, SLOT(saveSize()));
}

SidebarWidget::~SidebarWidget()
{
	delete m_ui;
}

void SidebarWidget::changeEvent(QEvent *event)
{
	QWidget::changeEvent(event);

	switch (event->type())
	{
		case QEvent::LanguageChange:
			{
				m_ui->retranslateUi(this);

				QHash<QString, QToolButton*>::iterator iterator;

				for (iterator = m_buttons.begin(); iterator != m_buttons.end(); ++iterator)
				{
					iterator.value()->setToolTip(getPanelTitle(iterator.key()));
				}

				if (m_ui->panelsButton->menu())
				{
					const QList<QAction*> actions(m_ui->panelsButton->menu()->actions());

					for (int i = 0; i < actions.count(); ++i)
					{
						if (!actions[i]->data().toString().isEmpty())
						{
							actions[i]->setText(getPanelTitle(actions[i]->data().toString()));
						}
					}
				}
			}

			break;
		case QEvent::LayoutDirectionChange:
			updateLayout();

			break;
		default:
			break;
	}
}

void SidebarWidget::reload()
{
	const int panelSize(m_toolBarWidget->getDefinition().panelSize);

	if (panelSize > 0)
	{
		m_ui->containerWidget->setMaximumWidth(panelSize);
	}

	updateLayout();
	updatePanels();
}

void SidebarWidget::addWebPanel()
{
	const MainWindow *mainWindow(MainWindow::findMainWindow(this));
	QString url;

	if (mainWindow)
	{
		url = mainWindow->getUrl().toString(QUrl::RemovePassword);
	}

	url = QInputDialog::getText(this, tr("Add web panel"), tr("Input address of web page to be shown in panel:"), QLineEdit::Normal, url);

	if (!url.isEmpty())
	{
		ToolBarsManager::ToolBarDefinition definition(m_toolBarWidget->getDefinition());
		definition.panels.append(QLatin1String("web:") + url);

		ToolBarsManager::setToolBar(definition);
	}
}

void SidebarWidget::choosePanel(bool isChecked)
{
	QAction *action(qobject_cast<QAction*>(sender()));

	if (!action)
	{
		return;
	}

	ToolBarsManager::ToolBarDefinition definition(m_toolBarWidget->getDefinition());

	if (isChecked)
	{
		definition.panels.append(action->data().toString());
	}
	else
	{
		definition.panels.removeAll(action->data().toString());
	}

	ToolBarsManager::setToolBar(definition);
}

void SidebarWidget::selectPanel()
{
	QAction *action(qobject_cast<QAction*>(sender()));

	if (action)
	{
		selectPanel((action->data().toString() == m_currentPanel) ? QString() : action->data().toString());
	}
}

void SidebarWidget::selectPanel(const QString &identifier)
{
	ToolBarsManager::ToolBarDefinition definition(m_toolBarWidget->getDefinition());

	if (!identifier.isEmpty() && (identifier == m_currentPanel || !definition.panels.contains(identifier)))
	{
		return;
	}

	MainWindow *mainWindow(MainWindow::findMainWindow(parent()));
	QWidget *widget((m_panels.contains(identifier) && m_panels[identifier]) ? m_panels[identifier] : WidgetFactory::createSidebarPanel(identifier, mainWindow, m_toolBarWidget->getIdentifier()));

	if (widget && mainWindow)
	{
		connect(widget, SIGNAL(requestedOpenUrl(QUrl,SessionsManager::OpenHints)), mainWindow, SLOT(open(QUrl,SessionsManager::OpenHints)));
		connect(widget, SIGNAL(requestedSearch(QString,QString,SessionsManager::OpenHints)), mainWindow, SLOT(search(QString,QString,SessionsManager::OpenHints)));
		connect(widget, SIGNAL(requestedNewWindow(ContentsWidget*,SessionsManager::OpenHints)), mainWindow, SLOT(openWindow(ContentsWidget*,SessionsManager::OpenHints)));
	}

	if (m_panels.contains(m_currentPanel) && m_panels[m_currentPanel])
	{
		m_ui->panelLayout->removeWidget(m_panels[m_currentPanel]);

		m_panels[m_currentPanel]->hide();
	}

	if (m_buttons.contains(m_currentPanel) && m_buttons[m_currentPanel])
	{
		m_buttons[m_currentPanel]->setChecked(false);
	}

	if (widget)
	{
		m_ui->panelLayout->addWidget(widget);

		widget->show();

		m_panels[identifier] = widget;

		if (m_buttons.contains(identifier) && m_buttons[identifier])
		{
			m_buttons[identifier]->setChecked(true);
		}
	}

	m_ui->containerWidget->setVisible(widget != nullptr);

	m_resizerWidget->setVisible(widget != nullptr);

	m_currentPanel = identifier;

	if (identifier != definition.currentPanel)
	{
		definition.currentPanel = identifier;

		ToolBarsManager::setToolBar(definition);
	}
}

void SidebarWidget::saveSize()
{
	ToolBarsManager::ToolBarDefinition definition(m_toolBarWidget->getDefinition());
	int size(m_ui->containerWidget->maximumWidth());

	if (size == QWIDGETSIZE_MAX)
	{
		size = -1;
	}

	if (size != definition.panelSize)
	{
		definition.panelSize = size;

		ToolBarsManager::setToolBar(definition);
	}
}

void SidebarWidget::updateLayout()
{
	QToolBar *toolbar(findChild<QToolBar*>());
	const Qt::ToolBarArea area(m_toolBarWidget->getArea());
	QBoxLayout::Direction direction((area == Qt::RightToolBarArea) ? QBoxLayout::RightToLeft : QBoxLayout::LeftToRight);

	m_ui->toggleSpacer->changeSize((m_toolBarWidget->getDefinition().hasToggle ? 6 : 0), 0, QSizePolicy::Fixed);

	m_resizerWidget->setDirection((area == Qt::RightToolBarArea) ? ResizerWidget::RightToLeftDirection : ResizerWidget::LeftToRightDirection);

	if (QGuiApplication::isRightToLeft())
	{
		direction = ((direction == QBoxLayout::LeftToRight) ? QBoxLayout::RightToLeft : QBoxLayout::LeftToRight);
	}

	m_ui->horizontalLayout->setDirection(direction);

	if (!toolbar)
	{
		return;
	}

	toolbar->setLayoutDirection((area == Qt::LeftToolBarArea) ? Qt::LeftToRight : Qt::RightToLeft);

	const QList<QWidget*> widgets(toolbar->findChildren<QWidget*>());

	for (int i = 0; i < widgets.count(); ++i)
	{
		widgets[i]->setLayoutDirection(QGuiApplication::isLeftToRight() ? Qt::LeftToRight : Qt::RightToLeft);

		ActionWidget *widget(qobject_cast<ActionWidget*>(widgets.at(i)));

		if (widget && widget->getIdentifier() == ActionsManager::OpenPanelAction)
		{
			QVariantMap options(widget->getOptions());
			options[QLatin1String("icon")] = ((area == Qt::LeftToolBarArea) ? QLatin1String("arrow-right") : QLatin1String("arrow-left"));

			widget->setOptions(options);
		}
	}
}

void SidebarWidget::updatePanels()
{
	const ToolBarsManager::ToolBarDefinition definition(m_toolBarWidget->getDefinition());

	if (m_buttons.keys().toSet() == definition.panels.toSet())
	{
		selectPanel(definition.currentPanel);

		return;
	}

	const QStringList panels(definition.panels);

	qDeleteAll(m_buttons.begin(), m_buttons.end());

	m_buttons.clear();

	QHash<QString, QWidget*>::iterator iterator(m_panels.begin());

	while (iterator != m_panels.end())
	{
		if (!panels.contains(iterator.key()))
		{
			iterator.value()->hide();

			if (iterator.key() == m_currentPanel)
			{
				m_currentPanel = QString();

				m_ui->panelLayout->removeWidget(iterator.value());
				m_ui->containerWidget->hide();

				m_resizerWidget->hide();
			}

			iterator.value()->deleteLater();

			iterator = m_panels.erase(iterator);
		}
		else
		{
			++iterator;
		}
	}

	QMenu *menu(new QMenu(m_ui->panelsButton));
	const QStringList specialPages(AddonsManager::getSpecialPages());

	for (int i = 0; i < specialPages.count(); ++i)
	{
		QAction *action(menu->addAction(getPanelTitle(specialPages.at(i))));
		action->setCheckable(true);
		action->setChecked(panels.contains(specialPages.at(i)));
		action->setData(specialPages.at(i));

		connect(action, SIGNAL(toggled(bool)), this, SLOT(choosePanel(bool)));
	}

	menu->addSeparator();

	for (int i = 0; i < panels.count(); ++i)
	{
		QToolButton *button(new QToolButton(this));
		QAction *action(new QAction(button));
		action->setData(panels.at(i));
		action->setToolTip(getPanelTitle(panels.at(i)));

		button->setDefaultAction(action);
		button->setAutoRaise(true);
		button->setCheckable(true);

		if (specialPages.contains(panels.at(i)))
		{
			button->setIcon(AddonsManager::getSpecialPage(panels.at(i)).icon);
		}
		else if (panels.at(i).startsWith(QLatin1String("web:")))
		{
			button->setIcon(HistoryManager::getIcon(QUrl(panels.at(i).mid(4))));

			QAction *action(menu->addAction(getPanelTitle(panels.at(i))));
			action->setCheckable(true);
			action->setChecked(true);
			action->setData(panels.at(i));

			connect(action, SIGNAL(toggled(bool)), this, SLOT(choosePanel(bool)));
		}
		else
		{
			button->deleteLater();

			continue;
		}

		m_ui->buttonsLayout->insertWidget(qMax(0, (m_ui->buttonsLayout->count() - 2)), button);

		m_buttons[panels.at(i)] = button;

		connect(button->defaultAction(), SIGNAL(triggered()), this, SLOT(selectPanel()));
	}

	menu->addSeparator();
	menu->addAction(tr("Add Web Panel…"), this, SLOT(addWebPanel()));

	if (m_ui->panelsButton->menu())
	{
		m_ui->panelsButton->menu()->deleteLater();
	}

	m_ui->panelsButton->setMenu(menu);

	selectPanel(definition.currentPanel);
}

QString SidebarWidget::getPanelTitle(const QString &identifier)
{
	if (identifier.startsWith(QLatin1String("web:")))
	{
		return identifier.mid(4);
	}

	if (AddonsManager::getSpecialPages().contains(identifier))
	{
		return AddonsManager::getSpecialPage(identifier).getTitle();
	}

	return identifier;
}

QUrl SidebarWidget::getPanelUrl(const QString &identifier)
{
	if (identifier.startsWith(QLatin1String("web:")))
	{
		return QUrl(identifier.mid(4));
	}

	if (AddonsManager::getSpecialPages().contains(identifier))
	{
		return AddonsManager::getSpecialPage(identifier).url;
	}

	return QUrl();
}

QSize SidebarWidget::sizeHint() const
{
	if (m_currentPanel.isEmpty())
	{
		return m_ui->buttonsLayout->sizeHint();
	}

	return QSize((m_ui->buttonsLayout->sizeHint().width() + ((m_ui->containerWidget->maximumWidth() < QWIDGETSIZE_MAX) ? m_ui->containerWidget->maximumWidth() : 250)), m_ui->buttonsLayout->sizeHint().height());
}

}
