/**************************************************************************
* Otter Browser: Web browser controlled by the user, not vice-versa.
* Copyright (C) 2016 - 2017 Michal Dutkiewicz aka Emdek <michal@emdek.pl>
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

#ifndef OTTER_PASSWORDSCONTENTSWIDGET_H
#define OTTER_PASSWORDSCONTENTSWIDGET_H

#include "../../../ui/ContentsWidget.h"

#include <QtGui/QStandardItemModel>

namespace Otter
{

namespace Ui
{
	class PasswordsContentsWidget;
}

class Window;

class PasswordsContentsWidget final : public ContentsWidget
{
	Q_OBJECT

public:
	enum DataRole
	{
		HostRole = Qt::UserRole,
		UrlRole,
		AuthTypeRole,
		FieldTypeRole
	};

	explicit PasswordsContentsWidget(const QVariantMap &parameters, Window *window, QWidget *parent);
	~PasswordsContentsWidget();

	void print(QPrinter *printer) override;
	QString getTitle() const override;
	QLatin1String getType() const override;
	QUrl getUrl() const override;
	QIcon getIcon() const override;
	ActionsManager::ActionDefinition::State getActionState(int identifier, const QVariantMap &parameters = {}) const override;
	WebWidget::LoadingState getLoadingState() const override;
	bool eventFilter(QObject *object, QEvent *event) override;

public slots:
	void triggerAction(int identifier, const QVariantMap &parameters = {}) override;

protected:
	void changeEvent(QEvent *event) override;
	PasswordsManager::PasswordInformation getPassword(const QModelIndex &index) const;

protected slots:
	void populatePasswords();
	void filterPasswords(const QString &filter);
	void removePasswords();
	void removeHostPasswords();
	void removeAllPasswords();
	void showContextMenu(const QPoint &position);

private:
	QStandardItemModel *m_model;
	bool m_isLoading;
	Ui::PasswordsContentsWidget *m_ui;
};

}

#endif
