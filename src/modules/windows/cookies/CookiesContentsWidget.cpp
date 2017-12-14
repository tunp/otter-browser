/**************************************************************************
* Otter Browser: Web browser controlled by the user, not vice-versa.
* Copyright (C) 2013 - 2017 Michal Dutkiewicz aka Emdek <michal@emdek.pl>
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

#include "CookiesContentsWidget.h"
#include "../../../core/Application.h"
#include "../../../core/CookieJar.h"
#include "../../../core/HistoryManager.h"
#include "../../../core/NetworkManagerFactory.h"
#include "../../../core/ThemesManager.h"
#include "../../../ui/Action.h"
#include "../../../ui/CookiePropertiesDialog.h"
#include "../../../ui/MainWindow.h"

#include "ui_CookiesContentsWidget.h"

#include <QtCore/QTimer>
#include <QtGui/QKeyEvent>
#include <QtWidgets/QMenu>
#include <QtWidgets/QMessageBox>

namespace Otter
{

CookiesContentsWidget::CookiesContentsWidget(const QVariantMap &parameters, Window *window, QWidget *parent) : ContentsWidget(parameters, window, parent),
	m_model(new QStandardItemModel(this)),
	m_isLoading(true),
	m_ui(new Ui::CookiesContentsWidget)
{
	m_ui->setupUi(this);
	m_ui->filterLineEditWidget->setClearOnEscape(true);
	m_ui->cookiesViewWidget->installEventFilter(this);

	if (isSidebarPanel())
	{
		m_ui->detailsWidget->hide();
	}

	QTimer::singleShot(100, this, &CookiesContentsWidget::populateCookies);

	connect(m_ui->filterLineEditWidget, &LineEditWidget::textChanged, m_ui->cookiesViewWidget, &ItemViewWidget::setFilterString);
	connect(m_ui->cookiesViewWidget, &ItemViewWidget::customContextMenuRequested, this, &CookiesContentsWidget::showContextMenu);
	connect(m_ui->propertiesButton, &QPushButton::clicked, this, &CookiesContentsWidget::cookieProperties);
	connect(m_ui->deleteButton, &QPushButton::clicked, this, &CookiesContentsWidget::removeCookies);
	connect(m_ui->addButton, &QPushButton::clicked, this, &CookiesContentsWidget::addCookie);
}

CookiesContentsWidget::~CookiesContentsWidget()
{
	delete m_ui;
}

void CookiesContentsWidget::changeEvent(QEvent *event)
{
	ContentsWidget::changeEvent(event);

	if (event->type() == QEvent::LanguageChange)
	{
		m_ui->retranslateUi(this);
	}
}

void CookiesContentsWidget::populateCookies()
{
	const CookieJar *cookieJar(NetworkManagerFactory::getCookieJar());
	const QVector<QNetworkCookie> cookies(cookieJar->getCookies());

	for (int i = 0; i < cookies.count(); ++i)
	{
		handleCookieAdded(cookies.at(i));
	}

	m_model->sort(0);

	m_ui->cookiesViewWidget->setViewMode(ItemViewWidget::TreeViewMode);
	m_ui->cookiesViewWidget->setModel(m_model);
	m_ui->cookiesViewWidget->setLayoutDirection(Qt::LeftToRight);

	m_isLoading = false;

	emit loadingStateChanged(WebWidget::FinishedLoadingState);

	connect(cookieJar, &CookieJar::cookieAdded, this, &CookiesContentsWidget::handleCookieAdded);
	connect(cookieJar, &CookieJar::cookieRemoved, this, &CookiesContentsWidget::handleCookieRemoved);
	connect(m_model, &QStandardItemModel::modelReset, this, &CookiesContentsWidget::updateActions);
	connect(m_ui->cookiesViewWidget, &ItemViewWidget::needsActionsUpdate, this, &CookiesContentsWidget::updateActions);
}

void CookiesContentsWidget::addCookie()
{
	CookiePropertiesDialog dialog(QNetworkCookie(), this);

	if (dialog.exec() == QDialog::Accepted)
	{
		NetworkManagerFactory::getCookieJar()->forceInsertCookie(dialog.getModifiedCookie());
	}
}

void CookiesContentsWidget::removeCookies()
{
	const QModelIndexList indexes(m_ui->cookiesViewWidget->selectionModel()->selectedIndexes());

	if (indexes.isEmpty())
	{
		return;
	}

	CookieJar *cookieJar(NetworkManagerFactory::getCookieJar());
	QVector<QNetworkCookie> cookies;
	cookies.reserve(indexes.count());

	for (int i = 0; i < indexes.count(); ++i)
	{
		if (!indexes.at(i).isValid())
		{
			continue;
		}

		if (indexes.at(i).parent() == m_model->invisibleRootItem()->index())
		{
			const QStandardItem *domainItem(m_model->itemFromIndex(indexes.at(i)));

			if (!domainItem)
			{
				continue;
			}

			for (int j = 0; j < domainItem->rowCount(); ++j)
			{
				cookies.append(getCookie(domainItem->index().child(j, 0)));
			}
		}
		else
		{
			const QStandardItem *cookieItem(m_model->itemFromIndex(indexes.at(i)));

			if (cookieItem)
			{
				cookies.append(getCookie(cookieItem->index()));
			}
		}
	}

	for (int i = 0; i < cookies.count(); ++i)
	{
		cookieJar->forceDeleteCookie(cookies.at(i));
	}
}

void CookiesContentsWidget::removeDomainCookies()
{
	const QModelIndexList indexes(m_ui->cookiesViewWidget->selectionModel()->selectedIndexes());

	if (indexes.isEmpty())
	{
		return;
	}

	QVector<QNetworkCookie> cookies;
	cookies.reserve(indexes.count());

	for (int i = 0; i < indexes.count(); ++i)
	{
		const QStandardItem *domainItem((indexes.at(i).isValid() && indexes.at(i).parent() == m_model->invisibleRootItem()->index()) ? findDomainItem(indexes.at(i).sibling(indexes.at(i).row(), 0).data(Qt::ToolTipRole).toString()) : m_model->itemFromIndex(indexes.at(i).parent()));

		if (domainItem)
		{
			for (int j = 0; j < domainItem->rowCount(); ++j)
			{
				const QNetworkCookie cookie(getCookie(domainItem->index().child(j, 0)));

				if (!cookies.contains(cookie))
				{
					cookies.append(cookie);
				}
			}
		}
	}

	if (cookies.isEmpty())
	{
		return;
	}

	QMessageBox messageBox;
	messageBox.setWindowTitle(tr("Question"));
	messageBox.setText(tr("You are about to delete %n cookie(s).", "", cookies.count()));
	messageBox.setInformativeText(tr("Do you want to continue?"));
	messageBox.setIcon(QMessageBox::Question);
	messageBox.setStandardButtons(QMessageBox::Yes | QMessageBox::Cancel);
	messageBox.setDefaultButton(QMessageBox::Yes);

	if (messageBox.exec() == QMessageBox::Yes)
	{
		CookieJar *cookieJar(NetworkManagerFactory::getCookieJar());

		for (int i = 0; i < cookies.count(); ++i)
		{
			cookieJar->forceDeleteCookie(cookies.at(i));
		}
	}
}

void CookiesContentsWidget::removeAllCookies()
{
	QMessageBox messageBox;
	messageBox.setWindowTitle(tr("Question"));
	messageBox.setText(tr("You are about to delete all cookies."));
	messageBox.setInformativeText(tr("Do you want to continue?"));
	messageBox.setIcon(QMessageBox::Question);
	messageBox.setStandardButtons(QMessageBox::Yes | QMessageBox::Cancel);
	messageBox.setDefaultButton(QMessageBox::Yes);

	if (messageBox.exec() == QMessageBox::Yes)
	{
		NetworkManagerFactory::getCookieJar()->clearCookies();
	}
}

void CookiesContentsWidget::cookieProperties()
{
	CookiePropertiesDialog dialog(getCookie(m_ui->cookiesViewWidget->currentIndex()), this);

	if (dialog.exec() == QDialog::Accepted && dialog.isModified())
	{
		NetworkManagerFactory::getCookieJar()->forceDeleteCookie(dialog.getOriginalCookie());
		NetworkManagerFactory::getCookieJar()->forceInsertCookie(dialog.getModifiedCookie());
	}
}

void CookiesContentsWidget::handleCookieAdded(const QNetworkCookie &cookie)
{
	const QString domain(cookie.domain().startsWith(QLatin1Char('.')) ? cookie.domain().mid(1) : cookie.domain());
	QStandardItem *domainItem(findDomainItem(domain));

	if (domainItem)
	{
		for (int i = 0; i < domainItem->rowCount(); ++i)
		{
			QStandardItem *childItem(domainItem->child(i, 0));

			if (childItem && cookie.hasSameIdentifier(getCookie(childItem->index())))
			{
				childItem->setData(cookie.toRawForm());

				updateActions();

				return;
			}
		}
	}
	else
	{
		domainItem = new QStandardItem(HistoryManager::getIcon(QUrl(QStringLiteral("http://%1/").arg(domain))), domain);
		domainItem->setToolTip(domain);

		m_model->appendRow(domainItem);

		if (sender())
		{
			m_model->sort(0);
		}
	}

	QStandardItem *cookieItem(new QStandardItem(QString(cookie.name())));
	cookieItem->setData(cookie.toRawForm(), Qt::UserRole);
	cookieItem->setToolTip(cookie.name());
	cookieItem->setFlags(cookieItem->flags() | Qt::ItemNeverHasChildren);

	domainItem->appendRow(cookieItem);
	domainItem->setText(QStringLiteral("%1 (%2)").arg(domain).arg(domainItem->rowCount()));
}

void CookiesContentsWidget::handleCookieRemoved(const QNetworkCookie &cookie)
{
	const QString domain(cookie.domain().startsWith(QLatin1Char('.')) ? cookie.domain().mid(1) : cookie.domain());
	QStandardItem *domainItem(findDomainItem(domain));

	if (domainItem)
	{
		QPoint point;

		for (int j = 0; j < domainItem->rowCount(); ++j)
		{
			if (cookie.hasSameIdentifier(getCookie(domainItem->index().child(j, 0))))
			{
				point = m_ui->cookiesViewWidget->visualRect(domainItem->index().child(j, 0)).center();

				domainItem->removeRow(j);

				break;
			}
		}

		if (domainItem->rowCount() == 0)
		{
			m_model->invisibleRootItem()->removeRow(domainItem->row());
		}
		else
		{
			domainItem->setText(QStringLiteral("%1 (%2)").arg(domain).arg(domainItem->rowCount()));
		}

		if (!point.isNull())
		{
			const QModelIndex index(m_ui->cookiesViewWidget->indexAt(point));

			m_ui->cookiesViewWidget->setCurrentIndex(index);
			m_ui->cookiesViewWidget->selectionModel()->select(index, QItemSelectionModel::Select);
		}
	}
}

void CookiesContentsWidget::showContextMenu(const QPoint &position)
{
	MainWindow *mainWindow(MainWindow::findMainWindow(this));
	const QModelIndex index(m_ui->cookiesViewWidget->indexAt(position));
	QMenu menu(this);
	menu.addAction(tr("Add Cookie…"), this, SLOT(addCookie()));
	menu.addSeparator();

	if (index.isValid())
	{
		if (index.parent() != m_model->invisibleRootItem()->index())
		{
			menu.addAction(new Action(ActionsManager::DeleteAction, {}, ActionExecutor::Object(this, this), &menu));
		}

		menu.addAction(tr("Remove All Cookies from This Domain…"), this, SLOT(removeDomainCookies()));
	}

	menu.addAction(tr("Remove All Cookies…"), this, SLOT(removeAllCookies()))->setEnabled(m_ui->cookiesViewWidget->model()->rowCount() > 0);
	menu.addSeparator();
	menu.addAction(new Action(ActionsManager::ClearHistoryAction, {}, ActionExecutor::Object(mainWindow, mainWindow), &menu));

	if (index.parent() != m_model->invisibleRootItem()->index())
	{
		menu.addSeparator();
		menu.addAction(tr("Properties…"), this, SLOT(cookieProperties()));
	}

	menu.exec(m_ui->cookiesViewWidget->mapToGlobal(position));
}

void CookiesContentsWidget::print(QPrinter *printer)
{
	m_ui->cookiesViewWidget->render(printer);
}

void CookiesContentsWidget::triggerAction(int identifier, const QVariantMap &parameters)
{
	switch (identifier)
	{
		case ActionsManager::SelectAllAction:
			m_ui->cookiesViewWidget->selectAll();

			break;
		case ActionsManager::DeleteAction:
			removeCookies();

			break;
		case ActionsManager::FindAction:
		case ActionsManager::QuickFindAction:
			m_ui->filterLineEditWidget->setFocus();

			break;
		case ActionsManager::ActivateContentAction:
			m_ui->cookiesViewWidget->setFocus();

			break;
		default:
			ContentsWidget::triggerAction(identifier, parameters);

			break;
	}
}

void CookiesContentsWidget::updateActions()
{
	const QModelIndexList indexes(m_ui->cookiesViewWidget->selectionModel()->selectedIndexes());

	m_ui->propertiesButton->setEnabled(false);
	m_ui->deleteButton->setEnabled(!indexes.isEmpty());
	m_ui->nameLabelWidget->clear();
	m_ui->valueLabelWidget->clear();
	m_ui->domainLabelWidget->clear();
	m_ui->pathLabelWidget->clear();
	m_ui->expiresLabelWidget->clear();

	if (indexes.count() == 1)
	{
		const QNetworkCookie cookie(getCookie(indexes.first()));

		if (!cookie.name().isEmpty())
		{
			m_ui->propertiesButton->setEnabled(true);
			m_ui->nameLabelWidget->setText(QString(cookie.name()));
			m_ui->valueLabelWidget->setText(QString(cookie.value()));
			m_ui->domainLabelWidget->setText(cookie.domain());
			m_ui->pathLabelWidget->setText(cookie.path());
			m_ui->expiresLabelWidget->setText(cookie.expirationDate().isValid() ? Utils::formatDateTime(cookie.expirationDate()) : tr("this session only"));
		}
	}

	emit categorizedActionsStateChanged({ActionsManager::ActionDefinition::EditingCategory});
}

QStandardItem* CookiesContentsWidget::findDomainItem(const QString &domain)
{
	for (int i = 0; i < m_model->rowCount(); ++i)
	{
		QStandardItem *item(m_model->item(i, 0));

		if (item && domain == item->toolTip())
		{
			return item;
		}
	}

	return nullptr;
}

QString CookiesContentsWidget::getTitle() const
{
	return tr("Cookies");
}

QLatin1String CookiesContentsWidget::getType() const
{
	return QLatin1String("cookies");
}

QUrl CookiesContentsWidget::getUrl() const
{
	return QUrl(QLatin1String("about:cookies"));
}

QIcon CookiesContentsWidget::getIcon() const
{
	return ThemesManager::createIcon(QLatin1String("cookies"), false);
}

QNetworkCookie CookiesContentsWidget::getCookie(const QModelIndex &index) const
{
	const QList<QNetworkCookie> cookies(QNetworkCookie::parseCookies(index.data(Qt::UserRole).toByteArray()));

	return (cookies.isEmpty() ? QNetworkCookie() : cookies.first());
}

ActionsManager::ActionDefinition::State CookiesContentsWidget::getActionState(int identifier, const QVariantMap &parameters) const
{
	ActionsManager::ActionDefinition::State state(ActionsManager::getActionDefinition(identifier).getDefaultState());

	switch (identifier)
	{
		case ActionsManager::DeleteAction:
			state.text = QCoreApplication::translate("actions", "Remove Cookie");
			state.isEnabled = m_ui->deleteButton->isEnabled();

			return state;
		case ActionsManager::SelectAllAction:
			state.isEnabled = true;

			return state;
		default:
			break;
	}

	return ContentsWidget::getActionState(identifier, parameters);
}

WebWidget::LoadingState CookiesContentsWidget::getLoadingState() const
{
	return (m_isLoading ? WebWidget::OngoingLoadingState : WebWidget::FinishedLoadingState);
}

bool CookiesContentsWidget::eventFilter(QObject *object, QEvent *event)
{
	if (object == m_ui->cookiesViewWidget && event->type() == QEvent::KeyPress)
	{
		const QKeyEvent *keyEvent(static_cast<QKeyEvent*>(event));

		if (keyEvent && keyEvent->key() == Qt::Key_Delete)
		{
			removeCookies();

			return true;
		}
	}

	return ContentsWidget::eventFilter(object, event);
}

}
