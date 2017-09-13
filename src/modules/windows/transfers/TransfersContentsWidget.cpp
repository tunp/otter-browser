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

#include "TransfersContentsWidget.h"
#include "../../../core/ThemesManager.h"
#include "../../../core/TransfersManager.h"
#include "../../../core/Utils.h"
#include "../../../ui/Menu.h"

#include "ui_TransfersContentsWidget.h"

#include <QtCore/QDir>
#include <QtCore/QFileInfo>
#include <QtCore/QtMath>
#include <QtCore/QQueue>
#include <QtGui/QClipboard>
#include <QtGui/QKeyEvent>
#include <QtWidgets/QApplication>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QProgressBar>

namespace Otter
{

ProgressBarDelegate::ProgressBarDelegate(QObject *parent) : ItemDelegate(parent)
{
}

void ProgressBarDelegate::setEditorData(QWidget *editor, const QModelIndex &index) const
{
	QProgressBar *progressBar(qobject_cast<QProgressBar*>(editor));

	if (progressBar)
	{
		const Transfer::TransferState state(static_cast<Transfer::TransferState>(index.data(TransfersContentsWidget::StateRole).toInt()));
		const qint64 bytesTotal(index.data(TransfersContentsWidget::BytesTotalRole).toLongLong());
		const bool isIndeterminate(bytesTotal <= 0);
		const bool hasError(state == Transfer::UnknownState || state == Transfer::ErrorState);

		progressBar->setRange(0, ((isIndeterminate && !hasError) ? 0 : 100));
		progressBar->setValue(isIndeterminate ? (hasError ? 0 : -1) : qFloor((static_cast<qreal>(index.data(TransfersContentsWidget::BytesReceivedRole).toLongLong()) / bytesTotal) * 100));
		progressBar->setFormat(isIndeterminate ? tr("Unknown") : QLatin1String("%p%"));
	}
}

QWidget* ProgressBarDelegate::createEditor(QWidget *parent, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
	Q_UNUSED(option)

	QProgressBar *editor(new QProgressBar(parent));
	editor->setAlignment(Qt::AlignCenter);

	setEditorData(editor, index);

	return editor;
}

TransfersContentsWidget::TransfersContentsWidget(const QVariantMap &parameters, Window *window) : ContentsWidget(parameters, window),
	m_model(new QStandardItemModel(this)),
	m_isLoading(false),
	m_ui(new Ui::TransfersContentsWidget)
{
	m_ui->setupUi(this);

	m_model->setHorizontalHeaderLabels(QStringList({QString(), tr("Filename"), tr("Size"), tr("Progress"), tr("Time"), tr("Speed"), tr("Started"), tr("Finished")}));

	m_ui->transfersViewWidget->setModel(m_model);
	m_ui->transfersViewWidget->header()->resizeSection(0, 30);
	m_ui->transfersViewWidget->header()->setSectionResizeMode(0, QHeaderView::Fixed);
	m_ui->transfersViewWidget->header()->setSectionResizeMode(1, QHeaderView::Stretch);
	m_ui->transfersViewWidget->setItemDelegateForColumn(3, new ProgressBarDelegate(this));
	m_ui->transfersViewWidget->installEventFilter(this);
	m_ui->stopResumeButton->setIcon(ThemesManager::createIcon(QLatin1String("task-ongoing")));
	m_ui->redownloadButton->setIcon(ThemesManager::createIcon(QLatin1String("view-refresh")));
	m_ui->downloadLineEdit->installEventFilter(this);

	const QVector<Transfer*> transfers(TransfersManager::getTransfers());

	for (int i = 0; i < transfers.count(); ++i)
	{
		addTransfer(transfers.at(i));
	}

	if (isSidebarPanel())
	{
		m_ui->detailsWidget->hide();
	}

	connect(TransfersManager::getInstance(), SIGNAL(transferStarted(Transfer*)), this, SLOT(addTransfer(Transfer*)));
	connect(TransfersManager::getInstance(), SIGNAL(transferRemoved(Transfer*)), this, SLOT(removeTransfer(Transfer*)));
	connect(TransfersManager::getInstance(), SIGNAL(transferChanged(Transfer*)), this, SLOT(updateTransfer(Transfer*)));
	connect(m_model, SIGNAL(modelReset()), this, SLOT(updateActions()));
	connect(m_ui->transfersViewWidget, SIGNAL(doubleClicked(QModelIndex)), this, SLOT(openTransfer(QModelIndex)));
	connect(m_ui->transfersViewWidget, SIGNAL(customContextMenuRequested(QPoint)), this, SLOT(showContextMenu(QPoint)));
	connect(m_ui->transfersViewWidget, SIGNAL(needsActionsUpdate()), this, SLOT(updateActions()));
	connect(m_ui->downloadLineEdit, SIGNAL(returnPressed()), this, SLOT(startQuickTransfer()));
	connect(m_ui->stopResumeButton, SIGNAL(clicked()), this, SLOT(stopResumeTransfer()));
	connect(m_ui->redownloadButton, SIGNAL(clicked()), this, SLOT(redownloadTransfer()));
}

TransfersContentsWidget::~TransfersContentsWidget()
{
	delete m_ui;
}

void TransfersContentsWidget::changeEvent(QEvent *event)
{
	ContentsWidget::changeEvent(event);

	if (event->type() == QEvent::LanguageChange)
	{
		m_ui->retranslateUi(this);
	}
}

void TransfersContentsWidget::addTransfer(Transfer *transfer)
{
	QList<QStandardItem*> items({new QStandardItem(), new QStandardItem(QFileInfo(transfer->getTarget()).fileName())});
	items[0]->setData(qVariantFromValue(static_cast<void*>(transfer)), Qt::UserRole);
	items[0]->setFlags(items[0]->flags() | Qt::ItemNeverHasChildren);
	items[1]->setFlags(items[1]->flags() | Qt::ItemNeverHasChildren);

	for (int i = 2; i < m_model->columnCount(); ++i)
	{
		QStandardItem *item(new QStandardItem());
		item->setFlags(item->flags() | Qt::ItemNeverHasChildren);

		items.append(item);
	}

	m_model->appendRow(items);

	m_ui->transfersViewWidget->openPersistentEditor(items[3]->index());

	if (transfer->getState() == Transfer::RunningState)
	{
		m_speeds[transfer] = QQueue<qint64>();
	}

	updateTransfer(transfer);
}

void TransfersContentsWidget::removeTransfer(Transfer *transfer)
{
	const int row(findTransfer(transfer));

	if (row >= 0)
	{
		m_model->removeRow(row);
	}

	m_speeds.remove(transfer);
}

void TransfersContentsWidget::removeTransfer()
{
	Transfer *transfer(getTransfer(m_ui->transfersViewWidget->currentIndex()));

	if (transfer)
	{
		if (transfer->getState() == Transfer::RunningState && QMessageBox::warning(this, tr("Warning"), tr("This transfer is still running.\nDo you really want to remove it?"), (QMessageBox::Yes | QMessageBox::Cancel)) == QMessageBox::Cancel)
		{
			return;
		}

		m_speeds.remove(transfer);

		m_model->removeRow(m_ui->transfersViewWidget->currentIndex().row());

		TransfersManager::removeTransfer(transfer);
	}
}

void TransfersContentsWidget::updateTransfer(Transfer *transfer)
{
	const int row(findTransfer(transfer));

	if (row < 0)
	{
		return;
	}

	QString remainingTime;
	const bool isIndeterminate(transfer->getBytesTotal() <= 0);

	if (transfer->getState() == Transfer::RunningState)
	{
		if (!m_speeds.contains(transfer))
		{
			m_speeds[transfer] = QQueue<qint64>();
		}

		m_speeds[transfer].enqueue(transfer->getSpeed());

		if (m_speeds[transfer].count() > 10)
		{
			m_speeds[transfer].dequeue();
		}

		if (!isIndeterminate)
		{
			qint64 speedSum(0);
			const QQueue<qint64> speeds(m_speeds[transfer]);

			for (int i = 0; i < speeds.count(); ++i)
			{
				speedSum += speeds.at(i);
			}

			speedSum /= (speeds.count());

			remainingTime = Utils::formatElapsedTime(qreal(transfer->getBytesTotal() - transfer->getBytesReceived()) / speedSum);
		}
	}
	else
	{
		m_speeds.remove(transfer);
	}

	QIcon icon;

	switch (transfer->getState())
	{
		case Transfer::RunningState:
			icon = ThemesManager::createIcon(QLatin1String("task-ongoing"));

			break;
		case Transfer::FinishedState:
			icon = ThemesManager::createIcon(QLatin1String("task-complete"));

			break;
		default:
			icon = ThemesManager::createIcon(QLatin1String("task-reject"));

			break;
	}

	const QString toolTip(tr("<div style=\"white-space:pre;\">Source: %1\nTarget: %2\nSize: %3\nDownloaded: %4\nProgress: %5</div>").arg(transfer->getSource().toDisplayString().toHtmlEscaped()).arg(transfer->getTarget().toHtmlEscaped()).arg(isIndeterminate ? tr("Unknown") : Utils::formatUnit(transfer->getBytesTotal(), false, 1, true)).arg(Utils::formatUnit(transfer->getBytesReceived(), false, 1, true)).arg(isIndeterminate ? tr("Unknown") : QStringLiteral("%1%").arg(((static_cast<qreal>(transfer->getBytesReceived()) / transfer->getBytesTotal()) * 100), 0, 'f', 1)));

	for (int i = 0; i < m_model->columnCount(); ++i)
	{
		const QModelIndex &index(m_model->index(row, i));

		m_model->setData(index, toolTip, Qt::ToolTipRole);

		switch (i)
		{
			case 1:
				m_model->setData(index, QFileInfo(transfer->getTarget()).fileName(), Qt::DisplayRole);

				break;
			case 2:
				m_model->setData(index, Utils::formatUnit(transfer->getBytesTotal(), false, 1), Qt::DisplayRole);

				break;
			case 3:
				m_model->setData(index, transfer->getBytesReceived(), BytesReceivedRole);
				m_model->setData(index, transfer->getBytesTotal(), BytesTotalRole);
				m_model->setData(index, transfer->getState(), StateRole);

				break;
			case 4:
				m_model->setData(index, remainingTime, Qt::DisplayRole);

				break;
			case 5:
				m_model->setData(index, ((transfer->getState() == Transfer::RunningState) ? Utils::formatUnit(transfer->getSpeed(), true, 1) : QString()), Qt::DisplayRole);

				break;
			case 6:
				m_model->setData(index, Utils::formatDateTime(transfer->getTimeStarted()), Qt::DisplayRole);

				break;
			case 7:
				m_model->setData(index, Utils::formatDateTime(transfer->getTimeFinished()), Qt::DisplayRole);

				break;
			default:
				m_model->setData(index, icon, Qt::DecorationRole);

				break;
		}
	}

	if (m_ui->transfersViewWidget->selectionModel()->hasSelection())
	{
		updateActions();
	}

	const bool isRunning(transfer && transfer->getState() == Transfer::RunningState);

	if (isRunning != m_isLoading)
	{
		if (isRunning)
		{
			m_isLoading = true;

			emit loadingStateChanged(WebWidget::OngoingLoadingState);
		}
		else
		{
			const QVector<Transfer*> transfers(TransfersManager::getTransfers());
			bool hasRunning(false);

			for (int i = 0; i < transfers.count(); ++i)
			{
				if (transfers.at(i) && transfers.at(i)->getState() == Transfer::RunningState)
				{
					hasRunning = true;

					break;
				}
			}

			if (!hasRunning)
			{
				m_isLoading = false;

				emit loadingStateChanged(WebWidget::FinishedLoadingState);
			}
		}
	}
}

void TransfersContentsWidget::openTransfer(const QModelIndex &index)
{
	const Transfer *transfer(getTransfer(index.isValid() ? index : m_ui->transfersViewWidget->currentIndex()));

	if (transfer)
	{
		transfer->openTarget();
	}
}

void TransfersContentsWidget::openTransferFolder(const QModelIndex &index)
{
	const Transfer *transfer(getTransfer(index.isValid() ? index : m_ui->transfersViewWidget->currentIndex()));

	if (transfer)
	{
		Utils::runApplication(QString(), QUrl::fromLocalFile(QFileInfo(transfer->getTarget()).dir().canonicalPath()));
	}
}

void TransfersContentsWidget::copyTransferInformation()
{
	const QStandardItem *item(m_model->itemFromIndex(m_ui->transfersViewWidget->currentIndex()));

	if (item)
	{
		QApplication::clipboard()->setText(item->toolTip().remove(QRegularExpression(QLatin1String("<[^>]*>"))));
	}
}

void TransfersContentsWidget::stopResumeTransfer()
{
	Transfer *transfer(getTransfer(m_ui->transfersViewWidget->getCurrentIndex()));

	if (transfer)
	{
		if (transfer->getState() == Transfer::RunningState)
		{
			transfer->stop();
		}
		else if (transfer->getState() == Transfer::ErrorState)
		{
			transfer->resume();
		}

		updateActions();
	}
}

void TransfersContentsWidget::redownloadTransfer()
{
	Transfer *transfer(getTransfer(m_ui->transfersViewWidget->getCurrentIndex()));

	if (transfer)
	{
		transfer->restart();
	}
}

void TransfersContentsWidget::startQuickTransfer()
{
	TransfersManager::startTransfer(m_ui->downloadLineEdit->text(), QString(), (Transfer::CanNotifyOption | Transfer::IsQuickTransferOption | (SessionsManager::isPrivate() ? Transfer::IsPrivateOption : Transfer::NoOption)));

	m_ui->downloadLineEdit->clear();
}

void TransfersContentsWidget::clearFinishedTransfers()
{
	TransfersManager::clearTransfers();
}

void TransfersContentsWidget::showContextMenu(const QPoint &position)
{
	const Transfer *transfer(getTransfer(m_ui->transfersViewWidget->indexAt(position)));
	QMenu menu(this);

	if (transfer)
	{
		const bool canOpen(QFileInfo(transfer->getTarget()).exists());

		menu.addAction(tr("Open"), this, SLOT(openTransfer()))->setEnabled(canOpen);

		Menu *openWithMenu(new Menu(Menu::OpenInApplicationMenuRole, this));
		openWithMenu->setEnabled(canOpen);
		openWithMenu->setExecutor(ActionExecutor::Object(this, this));
		openWithMenu->setActionParameters({{QLatin1String("url"), transfer->getTarget()}});
		openWithMenu->setMenuOptions({{QLatin1String("mimeType"), transfer->getMimeType().name()}});

		menu.addMenu(openWithMenu);
		menu.addAction(tr("Open Folder"), this, SLOT(openTransferFolder()))->setEnabled(canOpen || QFileInfo(transfer->getTarget()).dir().exists());
		menu.addSeparator();
		menu.addAction(((transfer->getState() == Transfer::ErrorState) ? tr("Resume") : tr("Stop")), this, SLOT(stopResumeTransfer()))->setEnabled(transfer->getState() == Transfer::RunningState || transfer->getState() == Transfer::ErrorState);
		menu.addAction(tr("Redownload"), this, SLOT(redownloadTransfer()));
		menu.addSeparator();
		menu.addAction(tr("Copy Transfer Information"), this, SLOT(copyTransferInformation()));
		menu.addSeparator();
		menu.addAction(tr("Remove"), this, SLOT(removeTransfer()));
	}

	const QVector<Transfer*> transfers(TransfersManager::getTransfers());
	int finishedTransfers(0);

	for (int i = 0; i < transfers.count(); ++i)
	{
		if (transfers.at(i)->getState() == Transfer::FinishedState)
		{
			++finishedTransfers;
		}
	}

	menu.addAction(tr("Clear Finished Transfers"), this, SLOT(clearFinishedTransfers()))->setEnabled(finishedTransfers > 0);
	menu.exec(m_ui->transfersViewWidget->mapToGlobal(position));
}

void TransfersContentsWidget::updateActions()
{
	const Transfer *transfer(getTransfer(m_ui->transfersViewWidget->getCurrentIndex()));

	if (transfer && transfer->getState() == Transfer::ErrorState)
	{
		m_ui->stopResumeButton->setText(tr("Resume"));
		m_ui->stopResumeButton->setIcon(ThemesManager::createIcon(QLatin1String("task-ongoing")));
	}
	else
	{
		m_ui->stopResumeButton->setText(tr("Stop"));
		m_ui->stopResumeButton->setIcon(ThemesManager::createIcon(QLatin1String("task-reject")));
	}

	m_ui->stopResumeButton->setEnabled(transfer && (transfer->getState() == Transfer::RunningState || transfer->getState() == Transfer::ErrorState));
	m_ui->redownloadButton->setEnabled(transfer);

	if (transfer)
	{
		const bool isIndeterminate(transfer->getBytesTotal() <= 0);

		m_ui->sourceLabelWidget->setText(transfer->getSource().toDisplayString());
		m_ui->sourceLabelWidget->setUrl(transfer->getSource());
		m_ui->targetLabelWidget->setText(transfer->getTarget());
		m_ui->targetLabelWidget->setUrl(QUrl(transfer->getTarget()));
		m_ui->sizeLabelWidget->setText(isIndeterminate ? tr("Unknown") : Utils::formatUnit(transfer->getBytesTotal(), false, 1, true));
		m_ui->downloadedLabelWidget->setText(Utils::formatUnit(transfer->getBytesReceived(), false, 1, true));
		m_ui->progressLabelWidget->setText(isIndeterminate ? tr("Unknown") : QStringLiteral("%1%").arg(((static_cast<qreal>(transfer->getBytesReceived()) / transfer->getBytesTotal()) * 100), 0, 'f', 1));
	}
	else
	{
		m_ui->sourceLabelWidget->clear();
		m_ui->targetLabelWidget->clear();
		m_ui->sizeLabelWidget->clear();
		m_ui->downloadedLabelWidget->clear();
		m_ui->progressLabelWidget->clear();
	}

	emit actionsStateChanged(ActionsManager::ActionDefinition::EditingCategory);
}

void TransfersContentsWidget::print(QPrinter *printer)
{
	m_ui->transfersViewWidget->render(printer);
}

void TransfersContentsWidget::triggerAction(int identifier, const QVariantMap &parameters)
{
	switch (identifier)
	{
		case ActionsManager::CopyAction:
			if (m_ui->transfersViewWidget->hasFocus() && m_ui->transfersViewWidget->currentIndex().isValid())
			{
				copyTransferInformation();
			}
			else
			{
				const QWidget *widget(focusWidget());

				if (widget->metaObject()->className() == QLatin1String("Otter::TextLabelWidget"))
				{
					const TextLabelWidget *label(qobject_cast<const TextLabelWidget*>(widget));

					if (label)
					{
						label->copy();
					}
				}
			}

			break;
		case ActionsManager::DeleteAction:
			removeTransfer();

			break;
		case ActionsManager::FindAction:
		case ActionsManager::QuickFindAction:
			m_ui->downloadLineEdit->setFocus();
			m_ui->downloadLineEdit->selectAll();

			break;
		case ActionsManager::ActivateContentAction:
			m_ui->transfersViewWidget->setFocus();

			break;
		default:
			ContentsWidget::triggerAction(identifier, parameters);

			break;
	}
}

Transfer* TransfersContentsWidget::getTransfer(const QModelIndex &index) const
{
	if (index.isValid())
	{
		return static_cast<Transfer*>(index.sibling(index.row(), 0).data(Qt::UserRole).value<void*>());
	}

	return nullptr;
}

QString TransfersContentsWidget::getTitle() const
{
	return tr("Transfers");
}

QLatin1String TransfersContentsWidget::getType() const
{
	return QLatin1String("transfers");
}

QUrl TransfersContentsWidget::getUrl() const
{
	return QUrl(QLatin1String("about:transfers"));
}

QIcon TransfersContentsWidget::getIcon() const
{
	return ThemesManager::createIcon(QLatin1String("transfers"), false);
}

ActionsManager::ActionDefinition::State TransfersContentsWidget::getActionState(int identifier, const QVariantMap &parameters) const
{
	switch (identifier)
	{
		case ActionsManager::CopyAction:
		case ActionsManager::DeleteAction:
			{
				const Transfer *transfer(getTransfer(m_ui->transfersViewWidget->getCurrentIndex()));
				ActionsManager::ActionDefinition::State state(ActionsManager::getActionDefinition(identifier).getDefaultState());
				state.isEnabled = (transfer != nullptr);

				return state;
			}

			break;
		default:
			break;
	}

	return ContentsWidget::getActionState(identifier, parameters);
}

WebWidget::LoadingState TransfersContentsWidget::getLoadingState() const
{
	return (m_isLoading ? WebWidget::OngoingLoadingState : WebWidget::FinishedLoadingState);
}

int TransfersContentsWidget::findTransfer(Transfer *transfer) const
{
	if (!transfer)
	{
		return -1;
	}

	for (int i = 0; i < m_model->rowCount(); ++i)
	{
		if (transfer == static_cast<Transfer*>(m_model->item(i, 0)->data(Qt::UserRole).value<void*>()))
		{
			return i;
		}
	}

	return -1;
}

bool TransfersContentsWidget::eventFilter(QObject *object, QEvent *event)
{
	if (object == m_ui->transfersViewWidget && event->type() == QEvent::KeyPress)
	{
		const QKeyEvent *keyEvent(static_cast<QKeyEvent*>(event));

		if (keyEvent && (keyEvent->key() == Qt::Key_Enter || keyEvent->key() == Qt::Key_Return))
		{
			openTransfer();

			return true;
		}

		if (keyEvent && keyEvent->key() == Qt::Key_Delete)
		{
			removeTransfer();

			return true;
		}
	}
	else if (object == m_ui->downloadLineEdit && event->type() == QEvent::KeyPress)
	{
		const QKeyEvent *keyEvent(static_cast<QKeyEvent*>(event));

		if (keyEvent->key() == Qt::Key_Escape)
		{
			m_ui->downloadLineEdit->clear();
		}
	}

	return ContentsWidget::eventFilter(object, event);
}

}
