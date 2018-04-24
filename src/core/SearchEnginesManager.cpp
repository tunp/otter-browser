/**************************************************************************
* Otter Browser: Web browser controlled by the user, not vice-versa.
* Copyright (C) 2013 - 2018 Michal Dutkiewicz aka Emdek <michal@emdek.pl>
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

#include "SearchEnginesManager.h"
#include "ItemModel.h"
#include "SessionsManager.h"
#include "SettingsManager.h"
#include "ThemesManager.h"
#include "Utils.h"

#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QXmlStreamReader>
#include <QtCore/QXmlStreamWriter>
#include <QtNetwork/QNetworkRequest>

namespace Otter
{

SearchEnginesManager* SearchEnginesManager::m_instance(nullptr);
QStandardItemModel* SearchEnginesManager::m_searchEnginesModel(nullptr);
QStringList SearchEnginesManager::m_searchEnginesOrder;
QStringList SearchEnginesManager::m_searchKeywords;
QHash<QString, SearchEnginesManager::SearchEngineDefinition> SearchEnginesManager::m_searchEngines;
bool SearchEnginesManager::m_isInitialized(false);

SearchEnginesManager::SearchEnginesManager(QObject *parent) : QObject(parent)
{
}

void SearchEnginesManager::createInstance()
{
	if (!m_instance)
	{
		m_instance = new SearchEnginesManager(QCoreApplication::instance());
	}
}

void SearchEnginesManager::ensureInitialized()
{
	if (!m_isInitialized)
	{
		m_isInitialized = true;

		loadSearchEngines();

		connect(SettingsManager::getInstance(), &SettingsManager::optionChanged, m_instance, &SearchEnginesManager::handleOptionChanged);
	}
}

void SearchEnginesManager::loadSearchEngines()
{
	m_searchEnginesOrder = SettingsManager::getOption(SettingsManager::Search_SearchEnginesOrderOption).toStringList();

	m_searchEngines.clear();
	m_searchEngines.reserve(m_searchEnginesOrder.count());

	m_searchKeywords.clear();
	m_searchKeywords.reserve(m_searchEnginesOrder.count());

	const QStringList searchEnginesOrder(m_searchEnginesOrder);

	for (int i = 0; i < searchEnginesOrder.count(); ++i)
	{
		QFile file(SessionsManager::getReadableDataPath(QLatin1String("searchEngines/") + searchEnginesOrder.at(i) + QLatin1String(".xml")));

		if (!file.open(QIODevice::ReadOnly))
		{
			m_searchEnginesOrder.removeAll(searchEnginesOrder.at(i));

			continue;
		}

		const SearchEngineDefinition searchEngine(loadSearchEngine(&file, searchEnginesOrder.at(i), true));

		file.close();

		if (searchEngine.isValid())
		{
			m_searchEngines[searchEnginesOrder.at(i)] = searchEngine;
		}
		else
		{
			m_searchEnginesOrder.removeAll(searchEnginesOrder.at(i));
		}
	}

	m_searchEngines.squeeze();

	emit m_instance->searchEnginesModified();

	updateSearchEnginesModel();
	updateSearchEnginesOptions();
}

void SearchEnginesManager::addSearchEngine(const SearchEngineDefinition &searchEngine)
{
	if (!saveSearchEngine(searchEngine))
	{
		return;
	}

	if (m_searchEnginesOrder.contains(searchEngine.identifier))
	{
		emit m_instance->searchEnginesModified();

		updateSearchEnginesModel();
		updateSearchEnginesOptions();
	}
	else
	{
		m_searchEnginesOrder.append(searchEngine.identifier);

		SettingsManager::setOption(SettingsManager::Search_SearchEnginesOrderOption, m_searchEnginesOrder);
	}
}

void SearchEnginesManager::handleOptionChanged(int identifier)
{
	if (identifier == SettingsManager::Search_SearchEnginesOrderOption)
	{
		loadSearchEngines();
	}
}

void SearchEnginesManager::updateSearchEnginesModel()
{
	if (!m_searchEnginesModel)
	{
		return;
	}

	m_searchEnginesModel->clear();

	const QStringList searchEngines(getSearchEngines());

	for (int i = 0; i < searchEngines.count(); ++i)
	{
		const SearchEngineDefinition search(getSearchEngine(searchEngines.at(i)));

		if (search.isValid())
		{
			QStandardItem *item(new QStandardItem((search.icon.isNull() ? ThemesManager::createIcon(QLatin1String("edit-find")) : search.icon), {}));
			item->setData(search.title, TitleRole);
			item->setData(search.identifier, IdentifierRole);
			item->setData(search.keyword, KeywordRole);
			item->setFlags(item->flags() | Qt::ItemNeverHasChildren);

			m_searchEnginesModel->appendRow(item);
		}
	}

	if (searchEngines.count() > 0)
	{
		QStandardItem *manageItem(new QStandardItem(ThemesManager::createIcon(QLatin1String("configure")), tr("Manage Search Engines…")));
		manageItem->setData(QLatin1String("configure"), Qt::AccessibleDescriptionRole);
		manageItem->setFlags(manageItem->flags() | Qt::ItemNeverHasChildren);

		m_searchEnginesModel->appendRow(new ItemModel::Item(ItemModel::SeparatorType));
		m_searchEnginesModel->appendRow(manageItem);
	}

	emit m_instance->searchEnginesModelModified();
}

void SearchEnginesManager::updateSearchEnginesOptions()
{
	const QStringList searchEngines(m_searchEngines.keys());
	QVector<SettingsManager::OptionDefinition::Choice> searchEngineChoices;
	searchEngineChoices.reserve(searchEngines.count());

	for (int i = 0; i < searchEngines.count(); ++i)
	{
		const SearchEngineDefinition searchEngine(getSearchEngine(searchEngines.at(i)));

		searchEngineChoices.append({(searchEngine.title.isEmpty() ? tr("Unknown") : searchEngine.title), searchEngines.at(i), searchEngine.icon});
	}

	SettingsManager::OptionDefinition defaultQuickSearchEngineOption(SettingsManager::getOptionDefinition(SettingsManager::Search_DefaultQuickSearchEngineOption));
	defaultQuickSearchEngineOption.choices = searchEngineChoices;

	SettingsManager::OptionDefinition defaultSearchEngineOption(SettingsManager::getOptionDefinition(SettingsManager::Search_DefaultSearchEngineOption));
	defaultSearchEngineOption.choices = searchEngineChoices;

	SettingsManager::updateOptionDefinition(SettingsManager::Search_DefaultQuickSearchEngineOption, defaultQuickSearchEngineOption);
	SettingsManager::updateOptionDefinition(SettingsManager::Search_DefaultSearchEngineOption, defaultSearchEngineOption);
}

void SearchEnginesManager::setupQuery(const QString &query, const SearchUrl &searchUrl, QNetworkRequest *request, QNetworkAccessManager::Operation *method, QByteArray *body)
{
	if (searchUrl.url.isEmpty())
	{
		return;
	}

	QString urlString(searchUrl.url);
	const QHash<QString, QString> values({{QLatin1String("searchTerms"), query}, {QLatin1String("count"), QString()}, {QLatin1String("startIndex"), QString()}, {QLatin1String("startPage"), QString()}, {QLatin1String("language"), QLocale::system().name()}, {QLatin1String("inputEncoding"), QLatin1String("UTF-8")}, {QLatin1String("outputEncoding"), QLatin1String("UTF-8")}});
	QHash<QString, QString>::const_iterator iterator;

	for (iterator = values.constBegin(); iterator != values.constEnd(); ++iterator)
	{
		urlString = urlString.replace(QLatin1Char('{') + iterator.key() + QLatin1Char('}'), QUrl::toPercentEncoding(iterator.value()));
	}

	*method = ((searchUrl.method == QLatin1String("post")) ? QNetworkAccessManager::PostOperation : QNetworkAccessManager::GetOperation);

	QUrl url(urlString);
	QUrlQuery getQuery(url);
	QUrlQuery postQuery;
	const QList<QPair<QString, QString> > parameters(searchUrl.parameters.queryItems(QUrl::FullyDecoded));

	for (int i = 0; i < parameters.count(); ++i)
	{
		QString value(parameters.at(i).second);

		for (iterator = values.constBegin(); iterator != values.constEnd(); ++iterator)
		{
			value = value.replace(QLatin1Char('{') + iterator.key() + QLatin1Char('}'), iterator.value());
		}

		if (*method == QNetworkAccessManager::GetOperation)
		{
			getQuery.addQueryItem(parameters.at(i).first, QUrl::toPercentEncoding(value));
		}
		else
		{
			if (searchUrl.enctype == QLatin1String("application/x-www-form-urlencoded"))
			{
				postQuery.addQueryItem(parameters.at(i).first, QUrl::toPercentEncoding(value));
			}
			else if (searchUrl.enctype == QLatin1String("multipart/form-data"))
			{
				QString encodedValue;
				QByteArray plainValue(value.toUtf8());
				const char hex[] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'};

				for (int j = 0; j < plainValue.length(); ++j)
				{
					const char character(plainValue.at(j));

					if (character == 32 || (character >= 33 && character <= 126 && character != 61))
					{
						encodedValue.append(character);
					}
					else
					{
						encodedValue.append(QLatin1Char('='));
						encodedValue.append(hex[(character >> 4) & 0x0F]);
						encodedValue.append(hex[character & 0x0F]);
					}
				}

				body->append(QLatin1String("--AaB03x\r\ncontent-disposition: form-data; name=\"") + parameters.at(i).first + QLatin1String("\"\r\ncontent-type: text/plain;charset=UTF-8\r\ncontent-transfer-encoding: quoted-printable\r\n") + encodedValue + QLatin1String("\r\n--AaB03x\r\n"));
			}
		}
	}

	if (*method == QNetworkAccessManager::PostOperation)
	{
		if (searchUrl.enctype == QLatin1String("application/x-www-form-urlencoded"))
		{
			QUrl postUrl;
			postUrl.setQuery(postQuery);

			*body = postUrl.toString().mid(1).toUtf8();
		}
		else if (searchUrl.enctype == QLatin1String("multipart/form-data"))
		{
			request->setRawHeader(QStringLiteral("Content-Type").toLatin1(), QStringLiteral("multipart/form-data; boundary=AaB03x").toLatin1());
			request->setRawHeader(QStringLiteral("Content-Length").toLatin1(), QString::number(body->length()).toUtf8());
		}
	}

	url.setQuery(getQuery);

	request->setUrl(url);
	request->setAttribute(QNetworkRequest::CacheLoadControlAttribute, QNetworkRequest::AlwaysNetwork);
}

SearchEnginesManager::SearchEngineDefinition SearchEnginesManager::loadSearchEngine(QIODevice *device, const QString &identifier, bool checkKeyword)
{
	SearchEngineDefinition searchEngine;
	searchEngine.identifier = identifier;

	QXmlStreamReader reader(device->readAll());

	if (reader.readNextStartElement() && reader.name() == QLatin1String("OpenSearchDescription"))
	{
		SearchUrl *currentUrl(nullptr);

		while (!reader.atEnd())
		{
			reader.readNext();

			if (reader.isStartElement())
			{
				if (reader.name() == QLatin1String("Url"))
				{
					if (reader.attributes().value(QLatin1String("rel")) == QLatin1String("self") || reader.attributes().value(QLatin1String("type")) == QLatin1String("application/opensearchdescription+xml"))
					{
						searchEngine.selfUrl = QUrl(reader.attributes().value(QLatin1String("template")).toString());

						currentUrl = nullptr;
					}
					else if (reader.attributes().value(QLatin1String("rel")) == QLatin1String("suggestions") || reader.attributes().value(QLatin1String("type")) == QLatin1String("application/x-suggestions+json"))
					{
						currentUrl = &searchEngine.suggestionsUrl;
					}
					else if (!reader.attributes().hasAttribute(QLatin1String("rel")) || reader.attributes().value(QLatin1String("rel")) == QLatin1String("results"))
					{
						currentUrl = &searchEngine.resultsUrl;
					}
					else
					{
						currentUrl = nullptr;
					}

					if (currentUrl)
					{
						currentUrl->url = reader.attributes().value(QLatin1String("template")).toString();
						currentUrl->enctype = reader.attributes().value(QLatin1String("enctype")).toString().toLower();
						currentUrl->method = reader.attributes().value(QLatin1String("method")).toString().toLower();
					}
				}
				else if (currentUrl && (reader.name() == QLatin1String("Param") || reader.name() == QLatin1String("Parameter")))
				{
					currentUrl->parameters.addQueryItem(reader.attributes().value(QLatin1String("name")).toString(), reader.attributes().value(QLatin1String("value")).toString());
				}
				else if (reader.name() == QLatin1String("Shortcut"))
				{
					const QString keyword(reader.readElementText());

					if (!keyword.isEmpty())
					{
						if (!m_searchKeywords.contains(keyword))
						{
							searchEngine.keyword = keyword;

							m_searchKeywords.append(keyword);
						}
						else if (!checkKeyword)
						{
							searchEngine.keyword = keyword;
						}
					}
				}
				else if (reader.name() == QLatin1String("ShortName"))
				{
					searchEngine.title = reader.readElementText();
				}
				else if (reader.name() == QLatin1String("Description"))
				{
					searchEngine.description = reader.readElementText();
				}
				else if (reader.name() == QLatin1String("InputEncoding"))
				{
					searchEngine.encoding = reader.readElementText();
				}
				else if (reader.name() == QLatin1String("Image"))
				{
					const QString data(reader.readElementText());

					if (data.startsWith(QLatin1String("data:image/")))
					{
						searchEngine.icon = QIcon(Utils::loadPixmapFromDataUri(data));
					}
					else
					{
						searchEngine.iconUrl = QUrl(data);
					}
				}
				else if (reader.name() == QLatin1String("SearchForm"))
				{
					searchEngine.formUrl = QUrl(reader.readElementText());
				}
			}
		}
	}
	else
	{
		searchEngine.identifier.clear();
	}

	return searchEngine;
}

SearchEnginesManager* SearchEnginesManager::getInstance()
{
	return m_instance;
}

QStandardItemModel* SearchEnginesManager::getSearchEnginesModel()
{
	ensureInitialized();

	if (!m_searchEnginesModel)
	{
		m_searchEnginesModel = new QStandardItemModel(m_instance);

		updateSearchEnginesModel();
	}

	return m_searchEnginesModel;
}

SearchEnginesManager::SearchEngineDefinition SearchEnginesManager::getSearchEngine(const QString &identifier, bool byKeyword)
{
	ensureInitialized();

	if (byKeyword)
	{
		if (!identifier.isEmpty())
		{
			QHash<QString, SearchEngineDefinition>::iterator iterator;

			for (iterator = m_searchEngines.begin(); iterator != m_searchEngines.end(); ++iterator)
			{
				if (iterator.value().keyword == identifier)
				{
					return iterator.value();
				}
			}
		}

		return SearchEngineDefinition();
	}

	if (identifier.isEmpty())
	{
		return m_searchEngines.value(SettingsManager::getOption(SettingsManager::Search_DefaultSearchEngineOption).toString(), SearchEngineDefinition());
	}

	if (!m_searchEngines.contains(identifier))
	{
		QFile file(SessionsManager::getReadableDataPath(QLatin1String("searchEngines/") + identifier + QLatin1String(".xml")));

		if (file.open(QIODevice::ReadOnly))
		{
			const SearchEnginesManager::SearchEngineDefinition searchEngine(SearchEnginesManager::loadSearchEngine(&file, identifier));

			if (searchEngine.isValid())
			{
				m_searchEngines[identifier] = searchEngine;
			}

			file.close();
		}
	}

	return m_searchEngines.value(identifier, SearchEngineDefinition());
}

QStringList SearchEnginesManager::getSearchEngines()
{
	ensureInitialized();

	return m_searchEnginesOrder;
}

QStringList SearchEnginesManager::getSearchKeywords()
{
	ensureInitialized();

	return m_searchKeywords;
}

bool SearchEnginesManager::hasSearchEngine(const QUrl &url)
{
	if (!url.isValid())
	{
		return false;
	}

	ensureInitialized();

	QHash<QString, SearchEngineDefinition>::iterator iterator;

	for (iterator = m_searchEngines.begin(); iterator != m_searchEngines.end(); ++iterator)
	{
		if (iterator.value().selfUrl == url)
		{
			return true;
		}
	}

	return false;
}

bool SearchEnginesManager::saveSearchEngine(const SearchEngineDefinition &searchEngine)
{
	if (SessionsManager::isReadOnly() || !searchEngine.isValid())
	{
		return false;
	}

	QDir().mkpath(SessionsManager::getWritableDataPath(QLatin1String("searchEngines")));

	QFile file(SessionsManager::getWritableDataPath(QLatin1String("searchEngines/") + searchEngine.identifier + QLatin1String(".xml")));

	if (!file.open(QIODevice::WriteOnly))
	{
		return false;
	}

	QXmlStreamWriter writer(&file);
	writer.setAutoFormatting(true);
	writer.setAutoFormattingIndent(-1);
	writer.writeStartDocument();
	writer.writeStartElement(QLatin1String("OpenSearchDescription"));
	writer.writeAttribute(QLatin1String("xmlns"), QLatin1String("http://a9.com/-/spec/opensearch/1.1/"));

	if (!searchEngine.formUrl.isEmpty())
	{
		writer.writeAttribute(QLatin1String("xmlns:moz"), QLatin1String("http://www.mozilla.org/2006/browser/search/"));
	}

	writer.writeTextElement(QLatin1String("Shortcut"), searchEngine.keyword);
	writer.writeTextElement(QLatin1String("ShortName"), searchEngine.title);
	writer.writeTextElement(QLatin1String("Description"), searchEngine.description);
	writer.writeTextElement(QLatin1String("InputEncoding"), searchEngine.encoding);

	if (!searchEngine.icon.isNull())
	{
		const QSize size(searchEngine.icon.availableSizes().value(0, QSize(16, 16)));

		writer.writeStartElement(QLatin1String("Image"));
		writer.writeAttribute(QLatin1String("width"), QString::number(size.width()));
		writer.writeAttribute(QLatin1String("height"), QString::number(size.height()));
		writer.writeAttribute(QLatin1String("type"), QLatin1String("image/png"));
		writer.writeCharacters(Utils::savePixmapAsDataUri(searchEngine.icon.pixmap(size)));
		writer.writeEndElement();
	}

	if (!searchEngine.selfUrl.isEmpty())
	{
		writer.writeStartElement(QLatin1String("Url"));
		writer.writeAttribute(QLatin1String("rel"), QLatin1String("self"));
		writer.writeAttribute(QLatin1String("type"), QLatin1String("application/opensearchdescription+xml"));
		writer.writeAttribute(QLatin1String("template"), searchEngine.selfUrl.toString());
		writer.writeEndElement();
	}

	if (!searchEngine.resultsUrl.url.isEmpty())
	{
		writer.writeStartElement(QLatin1String("Url"));
		writer.writeAttribute(QLatin1String("rel"), QLatin1String("results"));
		writer.writeAttribute(QLatin1String("type"), QLatin1String("text/html"));
		writer.writeAttribute(QLatin1String("method"), searchEngine.resultsUrl.method.toUpper());
		writer.writeAttribute(QLatin1String("enctype"), searchEngine.resultsUrl.enctype.toLower());
		writer.writeAttribute(QLatin1String("template"), searchEngine.resultsUrl.url);

		const QList<QPair<QString, QString> > parameters(searchEngine.resultsUrl.parameters.queryItems());

		for (int i = 0; i < parameters.count(); ++i)
		{
			writer.writeStartElement(QLatin1String("Param"));
			writer.writeAttribute(QLatin1String("name"), parameters.at(i).first);
			writer.writeAttribute(QLatin1String("value"), parameters.at(i).second);
			writer.writeEndElement();
		}

		writer.writeEndElement();
	}

	if (!searchEngine.suggestionsUrl.url.isEmpty())
	{
		writer.writeStartElement(QLatin1String("Url"));
		writer.writeAttribute(QLatin1String("rel"), QLatin1String("suggestions"));
		writer.writeAttribute(QLatin1String("type"), QLatin1String("application/x-suggestions+json"));
		writer.writeAttribute(QLatin1String("method"), searchEngine.suggestionsUrl.method.toUpper());
		writer.writeAttribute(QLatin1String("enctype"), searchEngine.suggestionsUrl.enctype.toLower());
		writer.writeAttribute(QLatin1String("template"), searchEngine.suggestionsUrl.url);

		const QList<QPair<QString, QString> > parameters(searchEngine.suggestionsUrl.parameters.queryItems());

		for (int i = 0; i < parameters.count(); ++i)
		{
			writer.writeStartElement(QLatin1String("Param"));
			writer.writeAttribute(QLatin1String("name"), parameters.at(i).first);
			writer.writeAttribute(QLatin1String("value"), parameters.at(i).second);
			writer.writeEndElement();
		}

		writer.writeEndElement();
	}

	if (!searchEngine.formUrl.isEmpty())
	{
		writer.writeTextElement(QLatin1String("moz:SearchForm"), searchEngine.formUrl.toString());
	}

	writer.writeEndElement();
	writer.writeEndDocument();

	return true;
}

bool SearchEnginesManager::setupSearchQuery(const QString &query, const QString &identifier, QNetworkRequest *request, QNetworkAccessManager::Operation *method, QByteArray *body)
{
	const SearchEngineDefinition searchEngine(getSearchEngine(identifier));

	if (searchEngine.isValid())
	{
		setupQuery(query, searchEngine.resultsUrl, request, method, body);

		return true;
	}

	return false;
}

}
