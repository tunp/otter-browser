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

#ifndef OTTER_HANDLERSMANAGER_H
#define OTTER_HANDLERSMANAGER_H

#include <QtCore/QObject>

namespace Otter
{

class HandlersManager final : public QObject
{
	Q_OBJECT

public:
	struct HandlerDefinition final
	{
		enum TransferMode
		{
			IgnoreTransfer = 0,
			AskTransfer,
			OpenTransfer,
			SaveTransfer,
			SaveAsTransfer
		};

		QString openCommand;
		QString downloadsPath;
		TransferMode transferMode = IgnoreTransfer;
		bool isExplicit = true;
	};

	static void createInstance();
	static HandlersManager* getInstance();
	static HandlerDefinition getHandler(const QString &type);
	static void setHandler(const QString &type, const HandlerDefinition &definition);

protected:
	explicit HandlersManager(QObject *parent);

private:
	static HandlersManager *m_instance;
};

}

#endif
