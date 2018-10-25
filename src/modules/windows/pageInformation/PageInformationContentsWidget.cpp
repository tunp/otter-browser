/**************************************************************************
* Otter Browser: Web browser controlled by the user, not vice-versa.
* Copyright (C) 2017 - 2018 Michal Dutkiewicz aka Emdek <michal@emdek.pl>
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

#include "PageInformationContentsWidget.h"
#include "../../../core/ThemesManager.h"
#include "../../../ui/Action.h"
#include "../../../ui/MainWindow.h"
#include "../../../ui/Window.h"

#include "ui_PageInformationContentsWidget.h"

#include <QtGui/QClipboard>

namespace Otter
{

PageInformationContentsWidget::PageInformationContentsWidget(const QVariantMap &parameters, QWidget *parent) : ContentsWidget(parameters, nullptr, parent),
	m_window(nullptr),
	m_ui(new Ui::PageInformationContentsWidget)
{
	m_ui->setupUi(this);
	m_ui->filterLineEditWidget->setClearOnEscape(true);

	const QVector<SectionName> sections({GeneralSection, SecuritySection, PermissionsSection, MetaSection, HeadersSection});
	QStandardItemModel *model(new QStandardItemModel(this));
	model->setHorizontalHeaderLabels({tr("Name"), tr("Value")});

	for (int i = 0; i < sections.count(); ++i)
	{
		QStandardItem *item(new QStandardItem());
		item->setData(sections.at(i), Qt::UserRole);

		model->appendRow({item, new QStandardItem()});
	}

	m_ui->informationViewWidget->setViewMode(ItemViewWidget::TreeView);
	m_ui->informationViewWidget->setModel(model);
	m_ui->informationViewWidget->expandAll();

	const MainWindow *mainWindow(MainWindow::findMainWindow(parentWidget()));

	if (mainWindow)
	{
		m_window = mainWindow->getActiveWindow();

		connect(mainWindow, &MainWindow::currentWindowChanged, this, [=]()
		{
			Window *window(mainWindow->getActiveWindow());

			if (window != m_window)
			{
				if (m_window)
				{
					disconnect(m_window, &Window::loadingStateChanged, this, &PageInformationContentsWidget::updateSections);

					if (m_window->getWebWidget())
					{
						m_window->getWebWidget()->stopWatchingChanges(this, WebWidget::MetaDataWatcher);

						disconnect(m_window->getWebWidget(), &WebWidget::watchedDataChanged, this, &PageInformationContentsWidget::handleWatchedDataChanged);
					}
				}

				m_window = window;

				if (window)
				{
					connect(window, &Window::loadingStateChanged, this, &PageInformationContentsWidget::updateSections);

					if (window->getWebWidget())
					{
						window->getWebWidget()->stopWatchingChanges(this, WebWidget::MetaDataWatcher);

						connect(window->getWebWidget(), &WebWidget::watchedDataChanged, this, &PageInformationContentsWidget::handleWatchedDataChanged);
					}
				}
			}

			updateSections();
		});
	}

	updateSections();

	connect(m_ui->filterLineEditWidget, &LineEditWidget::textChanged, m_ui->informationViewWidget, &ItemViewWidget::setFilterString);
	connect(m_ui->informationViewWidget, &ItemViewWidget::customContextMenuRequested, this, &PageInformationContentsWidget::showContextMenu);
	connect(m_ui->informationViewWidget, &ItemViewWidget::needsActionsUpdate, this, [&]()
	{
		emit arbitraryActionsStateChanged({ActionsManager::CopyAction});
	});
}

PageInformationContentsWidget::~PageInformationContentsWidget()
{
	delete m_ui;
}

void PageInformationContentsWidget::changeEvent(QEvent *event)
{
	ContentsWidget::changeEvent(event);

	if (event->type() == QEvent::LanguageChange)
	{
		m_ui->retranslateUi(this);

		if (m_ui->informationViewWidget->getSourceModel())
		{
			m_ui->informationViewWidget->getSourceModel()->setHorizontalHeaderLabels({tr("Name"), tr("Value")});
		}

		updateSections();
	}
}

void PageInformationContentsWidget::triggerAction(int identifier, const QVariantMap &parameters, ActionsManager::TriggerType trigger)
{
	if (identifier == ActionsManager::CopyAction)
	{
		const QModelIndex index(m_ui->informationViewWidget->getCurrentIndex());

		if (index.isValid() && index.parent().isValid())
		{
			QGuiApplication::clipboard()->setText(index.data(Qt::ToolTipRole).toString());
		}
	}
	else
	{
		ContentsWidget::triggerAction(identifier, parameters, trigger);
	}
}

void PageInformationContentsWidget::addEntry(QStandardItem *parent, const QString &label, const QString &value)
{
	const QString toolTip(label + QLatin1String(": ") + (value.isEmpty() ? tr("<empty>") : value));
	QList<QStandardItem*> items({new QStandardItem(label), new QStandardItem(value.isEmpty() ? tr("<empty>") : value)});
	items[0]->setToolTip(toolTip);
	items[0]->setFlags(items[0]->flags() | Qt::ItemNeverHasChildren);
	items[1]->setToolTip(toolTip);
	items[1]->setFlags(items[1]->flags() | Qt::ItemNeverHasChildren);

	parent->appendRow(items);
}

void PageInformationContentsWidget::updateSections()
{
	for (int i = 0; i < m_ui->informationViewWidget->getRowCount(); ++i)
	{
		const QModelIndex index(m_ui->informationViewWidget->getIndex(i));
		QStandardItem *sectionItem(m_ui->informationViewWidget->getItem(index));
		QStandardItemModel *model(m_ui->informationViewWidget->getSourceModel());
		model->removeRows(0, model->rowCount(index), index);

		switch (static_cast<SectionName>(index.data(Qt::UserRole).toInt()))
		{
			case GeneralSection:
				m_ui->informationViewWidget->setData(index, tr("General"), Qt::DisplayRole);

				if (sectionItem)
				{
					const bool canGetPageInformation(m_window && m_window->getWebWidget() && m_window->getLoadingState() == WebWidget::FinishedLoadingState);

					addEntry(sectionItem, tr("Title"), (m_window ? m_window->getTitle() : QString()));

					if (!m_window || m_window->getUrl().scheme() != QLatin1String("about"))
					{
						addEntry(sectionItem, tr("MIME type"), (canGetPageInformation ? m_window->getWebWidget()->getPageInformation(WebWidget::DocumentMimeTypeInformation).toString() : QString()));
						addEntry(sectionItem, tr("Document size"), (canGetPageInformation ? Utils::formatUnit(m_window->getWebWidget()->getPageInformation(WebWidget::DocumentBytesTotalInformation).toLongLong(), false, 1, true) : QString()));
						addEntry(sectionItem, tr("Total size"), (canGetPageInformation ? Utils::formatUnit(m_window->getWebWidget()->getPageInformation(WebWidget::TotalBytesTotalInformation).toLongLong(), false, 1, true) : QString()));

						if (canGetPageInformation && m_window->getWebWidget()->getPageInformation(WebWidget::RequestsBlockedInformation).toInt() > 0)
						{
							addEntry(sectionItem, tr("Number of requests"), tr("%1 (%n blocked)", "", m_window->getWebWidget()->getPageInformation(WebWidget::RequestsBlockedInformation).toInt()).arg(m_window->getWebWidget()->getPageInformation(WebWidget::RequestsFinishedInformation).toInt()));
						}
						else
						{
							addEntry(sectionItem, tr("Number of requests"), (canGetPageInformation ? QString::number(m_window->getWebWidget()->getPageInformation(WebWidget::RequestsFinishedInformation).toInt()) : QString()));
						}

						addEntry(sectionItem, tr("Downloaded"), (canGetPageInformation ? Utils::formatDateTime(m_window->getWebWidget()->getPageInformation(WebWidget::LoadingFinishedInformation).toDateTime(), {}, false) : QString()));
					}
				}

				break;
			case HeadersSection:
				m_ui->informationViewWidget->setData(index, tr("Headers"), Qt::DisplayRole);

				if (sectionItem && m_window && m_window->getWebWidget())
				{
					const QMap<QByteArray, QByteArray> headers(m_window->getWebWidget()->getHeaders());
					QMap<QByteArray, QByteArray>::const_iterator iterator;

					for (iterator = headers.constBegin(); iterator != headers.constEnd(); ++iterator)
					{
						addEntry(sectionItem, QString(iterator.key()), QString(iterator.value()));
					}
				}

				break;
			case MetaSection:
				m_ui->informationViewWidget->setData(index, tr("Meta"), Qt::DisplayRole);

				if (sectionItem && m_window && m_window->getWebWidget())
				{
					const QMultiMap<QString, QString> metaData(m_window->getWebWidget()->getMetaData());
					QMultiMap<QString, QString>::const_iterator iterator;

					for (iterator = metaData.constBegin(); iterator != metaData.constEnd(); ++iterator)
					{
						if (!iterator.key().isEmpty())
						{
							addEntry(sectionItem, iterator.key(), iterator.value());
						}
					}
				}

				break;
			case PermissionsSection:
				m_ui->informationViewWidget->setData(index, tr("Permissions"), Qt::DisplayRole);

				break;
			case SecuritySection:
				m_ui->informationViewWidget->setData(index, tr("Security"), Qt::DisplayRole);

				if (sectionItem && m_window && m_window->getWebWidget())
				{
					const QSslCipher cipher(m_window->getWebWidget()->getSslInformation().cipher);

					if (!cipher.isNull())
					{
						addEntry(sectionItem, tr("Cipher protocol"), cipher.protocolString());
						addEntry(sectionItem, tr("Cipher authentication method"), cipher.authenticationMethod());
						addEntry(sectionItem, tr("Cipher encryption method"), cipher.encryptionMethod());
						addEntry(sectionItem, tr("Cipher key exchange method"), cipher.keyExchangeMethod());
					}
				}

				break;
			default:
				break;
		}

		m_ui->informationViewWidget->setRowHidden(i, model->invisibleRootItem()->index(), (model->rowCount(index) == 0));
	}
}

void PageInformationContentsWidget::handleWatchedDataChanged(WebWidget::ChangeWatcher watcher)
{
	if (watcher == WebWidget::MetaDataWatcher)
	{
		updateSections();
	}
}

void PageInformationContentsWidget::showContextMenu(const QPoint &position)
{
	const QModelIndex index(m_ui->informationViewWidget->indexAt(position));

	if (index.isValid())
	{
		QMenu menu(this);
		menu.addAction(new Action(ActionsManager::CopyAction, {}, ActionExecutor::Object(this, this), &menu));
		menu.exec(m_ui->informationViewWidget->mapToGlobal(position));
	}
}

QString PageInformationContentsWidget::getTitle() const
{
	return tr("Page Information");
}

QLatin1String PageInformationContentsWidget::getType() const
{
	return QLatin1String("pageInformation");
}

QUrl PageInformationContentsWidget::getUrl() const
{
	return {};
}

QIcon PageInformationContentsWidget::getIcon() const
{
	return ThemesManager::createIcon(QLatin1String("view-information"), false);
}

ActionsManager::ActionDefinition::State PageInformationContentsWidget::getActionState(int identifier, const QVariantMap &parameters) const
{
	if (identifier == ActionsManager::CopyAction)
	{
		const QModelIndex index(m_ui->informationViewWidget->getCurrentIndex());
		ActionsManager::ActionDefinition::State state(ActionsManager::getActionDefinition(ActionsManager::CopyAction).getDefaultState());
		state.isEnabled = (index.isValid() && index.parent().isValid());

		return state;
	}

	return ContentsWidget::getActionState(identifier, parameters);
}

}
