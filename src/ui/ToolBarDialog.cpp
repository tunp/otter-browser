/**************************************************************************
* Otter Browser: Web browser controlled by the user, not vice-versa.
* Copyright (C) 2015 - 2017 Michal Dutkiewicz aka Emdek <michal@emdek.pl>
* Copyright (C) 2016 - 2017 Piotr Wójcik <chocimier@tlen.pl>
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

#include "ToolBarDialog.h"
#include "Menu.h"
#include "OptionWidget.h"
#include "SidebarWidget.h"
#include "../core/ActionsManager.h"
#include "../core/AddonsManager.h"
#include "../core/BookmarksManager.h"
#include "../core/BookmarksModel.h"
#include "../core/SearchEnginesManager.h"
#include "../core/ThemesManager.h"
#include "../core/TreeModel.h"
#include "../core/Utils.h"

#include "ui_ToolBarDialog.h"

namespace Otter
{

ToolBarDialog::ToolBarDialog(const ToolBarsManager::ToolBarDefinition &definition, QWidget *parent) : Dialog(parent),
	m_definition(definition),
	m_ui(new Ui::ToolBarDialog)
{
	if (definition.title.isEmpty())
	{
		switch (definition.type)
		{
			case ToolBarsManager::BookmarksBarType:
				m_definition.title = tr("Bookmarks Bar");

				break;
			case ToolBarsManager::SideBarType:
				m_definition.title = tr("Sidebar");

				break;
			default:
				m_definition.title = tr("Toolbar");

				break;
		}
	}

	m_ui->setupUi(this);
	m_ui->stackedWidget->setCurrentIndex(definition.type);
	m_ui->titleLineEdit->setText(m_definition.getTitle());
	m_ui->iconSizeSpinBox->setValue(qMax(0, m_definition.iconSize));
	m_ui->maximumButtonSizeSpinBox->setValue(qMax(0, m_definition.maximumButtonSize));
	m_ui->normalVisibilityComboBox->addItem(tr("Always visible"), ToolBarsManager::AlwaysVisibleToolBar);
	m_ui->normalVisibilityComboBox->addItem(tr("Always hidden"), ToolBarsManager::AlwaysHiddenToolBar);
	m_ui->normalVisibilityComboBox->addItem(tr("Visible only when needed"), ToolBarsManager::AutoVisibilityToolBar);

	const int normalVisibilityIndex(m_ui->normalVisibilityComboBox->findData(m_definition.normalVisibility));

	m_ui->normalVisibilityComboBox->setCurrentIndex((normalVisibilityIndex < 0) ? 0 : normalVisibilityIndex);
	m_ui->fullScreenVisibilityComboBox->addItem(tr("Always visible"), ToolBarsManager::AlwaysVisibleToolBar);
	m_ui->fullScreenVisibilityComboBox->addItem(tr("Always hidden"), ToolBarsManager::AlwaysHiddenToolBar);
	m_ui->fullScreenVisibilityComboBox->addItem(tr("Visible only when needed"), ToolBarsManager::AutoVisibilityToolBar);
	m_ui->fullScreenVisibilityComboBox->addItem(tr("Visible only when cursor is close to screen edge"), ToolBarsManager::OnHoverVisibleToolBar);

	const int fullScreenVisibilityIndex(m_ui->fullScreenVisibilityComboBox->findData(m_definition.fullScreenVisibility));

	m_ui->fullScreenVisibilityComboBox->setCurrentIndex((fullScreenVisibilityIndex < 0) ? 1 : fullScreenVisibilityIndex);
	m_ui->buttonStyleComboBox->addItem(tr("Follow style"), Qt::ToolButtonFollowStyle);
	m_ui->buttonStyleComboBox->addItem(tr("Icon only"), Qt::ToolButtonIconOnly);
	m_ui->buttonStyleComboBox->addItem(tr("Text only"), Qt::ToolButtonTextOnly);
	m_ui->buttonStyleComboBox->addItem(tr("Text beside icon"), Qt::ToolButtonTextBesideIcon);
	m_ui->buttonStyleComboBox->addItem(tr("Text under icon"), Qt::ToolButtonTextUnderIcon);

	const int buttonStyleIndex(m_ui->buttonStyleComboBox->findData(m_definition.buttonStyle));

	m_ui->buttonStyleComboBox->setCurrentIndex((buttonStyleIndex < 0) ? 1 : buttonStyleIndex);
	m_ui->hasToggleCheckBox->setChecked(definition.hasToggle);
	m_ui->buttonBox->button(QDialogButtonBox::RestoreDefaults)->setEnabled(m_definition.canReset);

	if (definition.identifier == ToolBarsManager::MenuBar || definition.identifier == ToolBarsManager::ProgressBar || definition.identifier == ToolBarsManager::StatusBar)
	{
		m_ui->hasToggleCheckBox->hide();
	}

	connect(m_ui->buttonBox->button(QDialogButtonBox::RestoreDefaults), SIGNAL(clicked()), this, SLOT(restoreDefaults()));

	switch (definition.type)
	{
		case ToolBarsManager::BookmarksBarType:
			m_ui->folderComboBox->setCurrentFolder(BookmarksManager::getModel()->getItem(definition.bookmarksPath));

			return;
		case ToolBarsManager::SideBarType:
			{
				TreeModel *panelsModel(new TreeModel(this));
				const QStringList specialPages(AddonsManager::getSpecialPages());

				for (int i = 0; i < definition.panels.count(); ++i)
				{
					QStandardItem *item(new QStandardItem(SidebarWidget::getPanelTitle(definition.panels.at(i))));
					item->setCheckable(true);
					item->setCheckState(Qt::Checked);
					item->setData(definition.panels.at(i), TreeModel::UserRole);
					item->setFlags(item->flags() | Qt::ItemNeverHasChildren);

					panelsModel->insertRow(item);
				}

				for (int i = 0; i < specialPages.count(); ++i)
				{
					if (!definition.panels.contains(specialPages.at(i)))
					{
						QStandardItem *item(new QStandardItem(SidebarWidget::getPanelTitle(specialPages.at(i))));
						item->setCheckable(true);
						item->setData(specialPages.at(i), TreeModel::UserRole);
						item->setFlags(item->flags() | Qt::ItemNeverHasChildren);

						panelsModel->insertRow(item);
					}
				}

				m_ui->panelsViewWidget->setModel(panelsModel);
			}

			return;
		default:
			break;
	}

	m_ui->removeButton->setIcon(ThemesManager::createIcon(QGuiApplication::isLeftToRight() ? QLatin1String("go-previous") : QLatin1String("go-next")));
	m_ui->addButton->setIcon(ThemesManager::createIcon(QGuiApplication::isLeftToRight() ? QLatin1String("go-next") : QLatin1String("go-previous")));
	m_ui->moveUpButton->setIcon(ThemesManager::createIcon(QLatin1String("go-up")));
	m_ui->moveDownButton->setIcon(ThemesManager::createIcon(QLatin1String("go-down")));
	m_ui->addEntryButton->setMenu(new QMenu(m_ui->addEntryButton));

	QStandardItemModel *availableEntriesModel(new QStandardItemModel(this));
	availableEntriesModel->appendRow(createEntry(QLatin1String("separator")));
	availableEntriesModel->appendRow(createEntry(QLatin1String("spacer")));

	const QStringList widgets({QLatin1String("CustomMenu"), QLatin1String("ClosedWindowsMenu"), QLatin1String("AddressWidget"), QLatin1String("ConfigurationOptionWidget"), QLatin1String("ContentBlockingInformationWidget"), QLatin1String("MenuButtonWidget"), QLatin1String("PanelChooserWidget"), QLatin1String("PrivateWindowIndicatorWidget"), QLatin1String("SearchWidget"), QLatin1String("SizeGripWidget"), QLatin1String("StatusMessageWidget"), QLatin1String("ZoomWidget")});

	for (int i = 0; i < widgets.count(); ++i)
	{
		availableEntriesModel->appendRow(createEntry(widgets.at(i)));

		if (widgets.at(i) == QLatin1String("SearchWidget"))
		{
			const QStringList searchEngines(SearchEnginesManager::getSearchEngines());

			for (int j = 0; j < searchEngines.count(); ++j)
			{
				QVariantMap options;
				options[QLatin1String("searchEngine")] = searchEngines.at(j);

				availableEntriesModel->appendRow(createEntry(widgets.at(i), options));
			}
		}
	}

	const QVector<ActionsManager::ActionDefinition> actions(ActionsManager::getActionDefinitions());

	for (int i = 0; i < actions.count(); ++i)
	{
		if (actions.at(i).flags.testFlag(ActionsManager::ActionDefinition::IsDeprecatedFlag) || actions.at(i).flags.testFlag(ActionsManager::ActionDefinition::RequiresParameters))
		{
			continue;
		}

		QStandardItem *item(new QStandardItem(actions.at(i).getText(true)));
		item->setData(QColor(Qt::transparent), Qt::DecorationRole);
		item->setData(ActionsManager::getActionName(actions.at(i).identifier) + QLatin1String("Action"), IdentifierRole);
		item->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled | Qt::ItemIsDragEnabled | Qt::ItemNeverHasChildren);
		item->setToolTip(item->text());

		if (!actions.at(i).defaultState.icon.isNull())
		{
			item->setIcon(actions.at(i).defaultState.icon);
		}

		availableEntriesModel->appendRow(item);
	}

	m_ui->availableEntriesItemView->setModel(availableEntriesModel);
	m_ui->availableEntriesItemView->viewport()->installEventFilter(this);
	m_ui->currentEntriesItemView->setModel(new QStandardItemModel(this));
	m_ui->currentEntriesItemView->setViewMode(ItemViewWidget::TreeViewMode);
	m_ui->currentEntriesItemView->setRootIsDecorated(false);

	for (int i = 0; i < m_definition.entries.count(); ++i)
	{
		addEntry(m_definition.entries.at(i));
	}

	m_definition.entries.clear();

	Menu *bookmarksMenu(new Menu(Menu::BookmarkSelectorMenuRole, m_ui->addEntryButton));

	m_ui->addEntryButton->setMenu(bookmarksMenu);
	m_ui->addEntryButton->setEnabled(BookmarksManager::getModel()->getRootItem()->rowCount() > 0);

	connect(bookmarksMenu, SIGNAL(triggered(QAction*)), this, SLOT(addBookmark(QAction*)));
	connect(m_ui->addButton, SIGNAL(clicked()), this, SLOT(addEntry()));
	connect(m_ui->removeButton, SIGNAL(clicked()), m_ui->currentEntriesItemView, SLOT(removeRow()));
	connect(m_ui->moveDownButton, SIGNAL(clicked()), m_ui->currentEntriesItemView, SLOT(moveDownRow()));
	connect(m_ui->moveUpButton, SIGNAL(clicked()), m_ui->currentEntriesItemView, SLOT(moveUpRow()));
	connect(m_ui->availableEntriesItemView, SIGNAL(needsActionsUpdate()), this, SLOT(updateActions()));
	connect(m_ui->currentEntriesItemView, SIGNAL(needsActionsUpdate()), this, SLOT(updateActions()));
	connect(m_ui->currentEntriesItemView, SIGNAL(canMoveDownChanged(bool)), m_ui->moveDownButton, SLOT(setEnabled(bool)));
	connect(m_ui->currentEntriesItemView, SIGNAL(canMoveUpChanged(bool)), m_ui->moveUpButton, SLOT(setEnabled(bool)));
	connect(m_ui->availableEntriesFilterLineEdit, SIGNAL(textChanged(QString)), m_ui->availableEntriesItemView, SLOT(setFilterString(QString)));
	connect(m_ui->currentEntriesFilterLineEdit, SIGNAL(textChanged(QString)), m_ui->currentEntriesItemView, SLOT(setFilterString(QString)));
	connect(m_ui->editEntryButton, SIGNAL(clicked(bool)), this, SLOT(editEntry()));
}

ToolBarDialog::~ToolBarDialog()
{
	delete m_ui;
}

void ToolBarDialog::changeEvent(QEvent *event)
{
	QDialog::changeEvent(event);

	if (event->type() == QEvent::LanguageChange)
	{
		m_ui->retranslateUi(this);
	}
}

void ToolBarDialog::addEntry(const ToolBarsManager::ToolBarDefinition::Entry &entry, QStandardItem *parent)
{
	QStandardItem *item(createEntry(entry.action, entry.options, entry.parameters));

	if (parent)
	{
		parent->appendRow(item);
	}
	else
	{
		m_ui->currentEntriesItemView->getSourceModel()->appendRow(item);
	}

	if (entry.action == QLatin1String("CustomMenu"))
	{
		for (int i = 0; i < entry.entries.count(); ++i)
		{
			addEntry(entry.entries.at(i), item);
		}

		m_ui->currentEntriesItemView->expand(item->index());
	}
}

void ToolBarDialog::addEntry()
{
	const QStandardItem *sourceItem(m_ui->availableEntriesItemView->getItem(m_ui->availableEntriesItemView->getCurrentRow()));

	if (sourceItem)
	{
		QStandardItem *newItem(sourceItem->clone());
		QStandardItem *targetItem(m_ui->currentEntriesItemView->getItem(m_ui->currentEntriesItemView->currentIndex()));

		if (targetItem && targetItem->data(IdentifierRole).toString() != QLatin1String("CustomMenu"))
		{
			targetItem = targetItem->parent();
		}

		if (targetItem && targetItem->data(IdentifierRole).toString() == QLatin1String("CustomMenu"))
		{
			int row(-1);

			if (targetItem->index() != m_ui->currentEntriesItemView->currentIndex())
			{
				row = m_ui->currentEntriesItemView->currentIndex().row();
			}

			if (row >= 0)
			{
				targetItem->insertRow((row + 1), newItem);
			}
			else
			{
				targetItem->appendRow(newItem);
			}

			m_ui->currentEntriesItemView->setCurrentIndex(newItem->index());
			m_ui->currentEntriesItemView->expand(targetItem->index());
		}
		else
		{
			m_ui->currentEntriesItemView->insertRow(newItem);
		}
	}
}

void ToolBarDialog::editEntry()
{
	const QModelIndex index(m_ui->currentEntriesItemView->currentIndex());
	const QString identifier(index.data(IdentifierRole).toString());
	QVariantMap options(index.data(OptionsRole).toMap());
	QVector<OptionEntry> entries;

	if (identifier == QLatin1String("SearchWidget"))
	{
		const QStringList searchEngines(SearchEnginesManager::getSearchEngines());
		QVector<SettingsManager::OptionDefinition::ChoiceDefinition> searchEngineChoices{{tr("All"), QString(), QIcon()}, SettingsManager::OptionDefinition::ChoiceDefinition()};
		OptionEntry searchEngineEntry;
		searchEngineEntry.widget = new OptionWidget(options.value(QLatin1String("searchEngine")), SettingsManager::EnumerationType, this);
		searchEngineEntry.name = QLatin1String("searchEngine");
		searchEngineEntry.label = tr("Show search engine:");

		for (int i = 0; i < searchEngines.count(); ++i)
		{
			const SearchEnginesManager::SearchEngineDefinition searchEngine(SearchEnginesManager::getSearchEngine(searchEngines.at(i)));

			searchEngineChoices.append({(searchEngine.title.isEmpty() ? tr("Unknown") : searchEngine.title), searchEngines.at(i), searchEngine.icon});
		}

		searchEngineEntry.widget->setChoices(searchEngineChoices);

		OptionEntry showSearchButtonEntry;
		showSearchButtonEntry.widget = new OptionWidget(options.value(QLatin1String("showSearchButton"), true), SettingsManager::BooleanType, this);
		showSearchButtonEntry.name = QLatin1String("showSearchButton");
		showSearchButtonEntry.label = tr("Show search button:");

		entries = {searchEngineEntry, showSearchButtonEntry};
	}
	else if (identifier == QLatin1String("ConfigurationOptionWidget") || identifier == QLatin1String("ContentBlockingInformationWidget") || identifier == QLatin1String("MenuButtonWidget") || identifier.startsWith(QLatin1String("bookmarks:")) || identifier.endsWith(QLatin1String("Action")) || identifier.endsWith(QLatin1String("Menu")))
	{
		OptionEntry iconEntry;
		iconEntry.widget = new OptionWidget(QVariant(), SettingsManager::IconType, this);
		iconEntry.name = QLatin1String("icon");
		iconEntry.label = tr("Icon:");

		OptionEntry textEntry;
		textEntry.widget = new OptionWidget(QVariant(), SettingsManager::StringType, this);
		textEntry.name = QLatin1String("text");
		textEntry.label = tr("Text:");

		entries = {iconEntry, textEntry};

		if (identifier == QLatin1String("ClosedWindowsMenu"))
		{
			iconEntry.widget->setDefaultValue(ThemesManager::createIcon(QLatin1String("user-trash")));
		}
		else if (identifier == QLatin1String("ConfigurationOptionWidget"))
		{
			const QStringList choices(SettingsManager::getOptions());
			OptionEntry optionNameEntry;
			optionNameEntry.widget = new OptionWidget(options.value(QLatin1String("optionName")), SettingsManager::EnumerationType, this);
			optionNameEntry.widget->setChoices(choices);
			optionNameEntry.name = QLatin1String("optionName");
			optionNameEntry.label = tr("Option:");

			OptionEntry scopeEntry;
			scopeEntry.widget = new OptionWidget(options.value(QLatin1String("scope")), SettingsManager::EnumerationType, this);
			scopeEntry.widget->setChoices(QVector<SettingsManager::OptionDefinition::ChoiceDefinition>{{tr("Global"), QLatin1String("global"), QIcon()}, {tr("Tab"), QLatin1String("window"), QIcon()}});
			scopeEntry.widget->setDefaultValue(QLatin1String("window"));
			scopeEntry.name = QLatin1String("scope");
			scopeEntry.label = tr("Scope:");

			entries.append(optionNameEntry);
			entries.append(scopeEntry);

			textEntry.widget->setDefaultValue(options.value(QLatin1String("optionName"), choices.first()).toString().section(QLatin1Char('/'), -1));

			connect(optionNameEntry.widget, &OptionWidget::commitData, [&]()
			{
				const bool needsReset(textEntry.widget->getDefaultValue() == textEntry.widget->getValue());

				textEntry.widget->setDefaultValue(optionNameEntry.widget->getValue().toString().section(QLatin1Char('/'), -1));

				if (needsReset)
				{
					textEntry.widget->setValue(textEntry.widget->getDefaultValue());
				}
			});
		}
		else if (identifier == QLatin1String("ContentBlockingInformationWidget"))
		{
			iconEntry.widget->setDefaultValue(ThemesManager::createIcon(QLatin1String("content-blocking")));
			textEntry.widget->setDefaultValue(tr("Blocked Elements: {amount}"));
		}
		else if (identifier == QLatin1String("MenuButtonWidget"))
		{
			iconEntry.widget->setDefaultValue(ThemesManager::createIcon(QLatin1String("otter-browser"), false));
			textEntry.widget->setDefaultValue(tr("Menu"));
		}
		else if (identifier.startsWith(QLatin1String("bookmarks:")))
		{
			const BookmarksItem *bookmark(identifier.startsWith(QLatin1String("bookmarks:/")) ? BookmarksManager::getModel()->getItem(identifier.mid(11)) : BookmarksManager::getBookmark(identifier.mid(10).toULongLong()));

			if (bookmark)
			{
				iconEntry.widget->setDefaultValue(bookmark->data(Qt::DecorationRole).value<QIcon>());
				textEntry.widget->setDefaultValue(bookmark->data(BookmarksModel::TitleRole).isValid() ? bookmark->data(BookmarksModel::TitleRole).toString() : tr("(Untitled)"));
			}
		}
		else if (identifier.endsWith(QLatin1String("Action")))
		{
			const int actionIdentifier(ActionsManager::getActionIdentifier(identifier.left(identifier.length() - 6)));

			if (actionIdentifier >= 0)
			{
				const ActionsManager::ActionDefinition definition(ActionsManager::getActionDefinition(actionIdentifier));

				iconEntry.widget->setDefaultValue(definition.defaultState.icon);
				textEntry.widget->setDefaultValue(definition.getText(true));
			}
		}

		iconEntry.widget->setValue(options.value(QLatin1String("icon"), iconEntry.widget->getDefaultValue()));
		textEntry.widget->setValue(options.value(QLatin1String("text"), textEntry.widget->getDefaultValue()));
	}

	if (entries.isEmpty())
	{
		return;
	}

	QDialog dialog(this);
	dialog.setWindowTitle(tr("Edit Entry"));

	QDialogButtonBox *buttonBox(new QDialogButtonBox(&dialog));
	buttonBox->addButton(QDialogButtonBox::Cancel);
	buttonBox->addButton(QDialogButtonBox::Ok);

	QFormLayout *formLayout(new QFormLayout());
	QVBoxLayout *mainLayout(new QVBoxLayout(&dialog));
	mainLayout->addLayout(formLayout);
	mainLayout->addWidget(buttonBox);

	for (int i = 0; i < entries.count(); ++i)
	{
		formLayout->addRow(entries.at(i).label, entries.at(i).widget);

		entries.at(i).widget->setParent(&dialog);
	}

	connect(buttonBox, SIGNAL(accepted()), &dialog, SLOT(accept()));
	connect(buttonBox, SIGNAL(rejected()), &dialog, SLOT(reject()));

	if (dialog.exec() == QDialog::Rejected)
	{
		return;
	}

	for (int i = 0; i < entries.count(); ++i)
	{
		options[entries.at(i).name] = entries.at(i).widget->getValue();
	}

	QStandardItem *item(createEntry(identifier, options));

	m_ui->currentEntriesItemView->setData(index, item->text(), Qt::DisplayRole);
	m_ui->currentEntriesItemView->setData(index, item->text(), Qt::ToolTipRole);
	m_ui->currentEntriesItemView->setData(index, item->data(Qt::DecorationRole), Qt::DecorationRole);
	m_ui->currentEntriesItemView->setData(index, options, OptionsRole);

	delete item;
}

void ToolBarDialog::addBookmark(QAction *action)
{
	if (action && action->data().type() == QVariant::ULongLong)
	{
		m_ui->currentEntriesItemView->insertRow(createEntry(QLatin1String("bookmarks:") + QString::number(action->data().toULongLong())));
	}
}

void ToolBarDialog::restoreDefaults()
{
	ToolBarsManager::getInstance()->resetToolBar(m_definition.identifier);

	reject();
}

void ToolBarDialog::updateActions()
{
	const QString sourceIdentifier(m_ui->availableEntriesItemView->currentIndex().data(IdentifierRole).toString());
	const QString targetIdentifier(m_ui->currentEntriesItemView->currentIndex().data(IdentifierRole).toString());

	m_ui->addButton->setEnabled(!sourceIdentifier.isEmpty() && (!(m_ui->currentEntriesItemView->currentIndex().data(IdentifierRole).toString() == QLatin1String("CustomMenu") || m_ui->currentEntriesItemView->currentIndex().parent().data(IdentifierRole).toString() == QLatin1String("CustomMenu")) || (sourceIdentifier == QLatin1String("separator") || sourceIdentifier.endsWith(QLatin1String("Action")) || sourceIdentifier.endsWith(QLatin1String("Menu")))));
	m_ui->removeButton->setEnabled(m_ui->currentEntriesItemView->currentIndex().isValid() && targetIdentifier != QLatin1String("MenuBarWidget") && targetIdentifier != QLatin1String("TabBarWidget"));
	m_ui->editEntryButton->setEnabled(targetIdentifier == QLatin1String("ConfigurationOptionWidget") || targetIdentifier == QLatin1String("ContentBlockingInformationWidget") || targetIdentifier == QLatin1String("MenuButtonWidget") || targetIdentifier == QLatin1String("PanelChooserWidget") || targetIdentifier == QLatin1String("SearchWidget") || targetIdentifier.startsWith(QLatin1String("bookmarks:")) || targetIdentifier.endsWith(QLatin1String("Action")) || targetIdentifier.endsWith(QLatin1String("Menu")));
}

QStandardItem* ToolBarDialog::createEntry(const QString &identifier, const QVariantMap &options, const QVariantMap &parameters) const
{
	QStandardItem *item(new QStandardItem());
	item->setData(QColor(Qt::transparent), Qt::DecorationRole);
	item->setData(identifier, IdentifierRole);
	item->setData(parameters, ParametersRole);
	item->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled | Qt::ItemIsDragEnabled | Qt::ItemNeverHasChildren);

	if (identifier == QLatin1String("separator"))
	{
		item->setText(tr("--- separator ---"));
	}
	else if (identifier == QLatin1String("spacer"))
	{
		item->setText(tr("--- spacer ---"));
	}
	else if (identifier == QLatin1String("CustomMenu"))
	{
		item->setText(tr("Arbitrary List of Actions"));
		item->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled | Qt::ItemIsDragEnabled);

		m_ui->currentEntriesItemView->setRootIsDecorated(true);
	}
	else if (identifier == QLatin1String("ClosedWindowsMenu"))
	{
		item->setText(tr("List of Closed Tabs and Windows"));
		item->setIcon(ThemesManager::createIcon(QLatin1String("user-trash")));
	}
	else if (identifier == QLatin1String("AddressWidget"))
	{
		item->setText(tr("Address Field"));
		item->setIcon(ThemesManager::createIcon(QLatin1String("document-open")));
	}
	else if (identifier == QLatin1String("ConfigurationOptionWidget"))
	{
		if (options.contains(QLatin1String("optionName")) && !options.value("optionName").toString().isEmpty())
		{
			item->setText(tr("Configuration Widget (%1)").arg(options.value("optionName").toString()));
		}
		else
		{
			item->setText(tr("Configuration Widget"));
		}
	}
	else if (identifier == QLatin1String("ContentBlockingInformationWidget"))
	{
		item->setText(tr("Content Blocking Details"));
		item->setIcon(ThemesManager::createIcon(QLatin1String("content-blocking")));
	}
	else if (identifier == QLatin1String("ErrorConsoleWidget"))
	{
		item->setText(tr("Error Console"));
	}
	else if (identifier == QLatin1String("MenuBarWidget"))
	{
		item->setText(tr("Menu Bar"));
	}
	else if (identifier == QLatin1String("MenuButtonWidget"))
	{
		item->setText(tr("Menu Button"));
	}
	else if (identifier == QLatin1String("PanelChooserWidget"))
	{
		item->setText(tr("Sidebar Panel Chooser"));
	}
	else if (identifier == QLatin1String("PrivateWindowIndicatorWidget"))
	{
		item->setText(tr("Private Window Indicator"));
		item->setIcon(ThemesManager::createIcon(QLatin1String("window-private")));
	}
	else if (identifier == QLatin1String("ProgressInformationDocumentProgressWidget"))
	{
		item->setText(tr("Progress Information (Document Progress)"));
	}
	else if (identifier == QLatin1String("ProgressInformationTotalSizeWidget"))
	{
		item->setText(tr("Progress Information (Total Progress)"));
	}
	else if (identifier == QLatin1String("ProgressInformationElementsWidget"))
	{
		item->setText(tr("Progress Information (Loaded Elements)"));
	}
	else if (identifier == QLatin1String("ProgressInformationSpeedWidget"))
	{
		item->setText(tr("Progress Information (Loading Speed)"));
	}
	else if (identifier == QLatin1String("ProgressInformationElapsedTimeWidget"))
	{
		item->setText(tr("Progress Information (Elapsed Time)"));
	}
	else if (identifier == QLatin1String("ProgressInformationMessageWidget"))
	{
		item->setText(tr("Progress Information (Status Message)"));
	}
	else if (identifier == QLatin1String("SearchWidget"))
	{
		if (options.contains(QLatin1String("searchEngine")))
		{
			const SearchEnginesManager::SearchEngineDefinition definition(SearchEnginesManager::getSearchEngine(options[QLatin1String("searchEngine")].toString()));

			item->setText(tr("Search Field (%1)").arg(definition.title.isEmpty() ? tr("Unknown") : definition.title));
			item->setIcon(definition.icon.isNull() ? ThemesManager::createIcon(QLatin1String("edit-find")) : definition.icon);
		}
		else
		{
			item->setText(tr("Search Field"));
			item->setIcon(ThemesManager::createIcon(QLatin1String("edit-find")));
		}
	}
	else if (identifier == QLatin1String("SizeGripWidget"))
	{
		item->setText(tr("Window Resize Handle"));
	}
	else if (identifier == QLatin1String("StatusMessageWidget"))
	{
		item->setText(tr("Status Message Field"));
	}
	else if (identifier == QLatin1String("TabBarWidget"))
	{
		item->setText(tr("Tab Bar"));
	}
	else if (identifier == QLatin1String("ZoomWidget"))
	{
		item->setText(tr("Zoom Slider"));
	}
	else if (identifier.startsWith(QLatin1String("bookmarks:")))
	{
		const BookmarksItem *bookmark(identifier.startsWith(QLatin1String("bookmarks:/")) ? BookmarksManager::getModel()->getItem(identifier.mid(11)) : BookmarksManager::getBookmark(identifier.mid(10).toULongLong()));

		if (bookmark)
		{
			const QIcon icon(bookmark->data(Qt::DecorationRole).value<QIcon>());

			item->setText(bookmark->data(BookmarksModel::TitleRole).isValid() ? bookmark->data(BookmarksModel::TitleRole).toString() : tr("(Untitled)"));

			if (!icon.isNull())
			{
				item->setIcon(icon);
			}
		}
		else
		{
			item->setText(tr("Invalid Bookmark"));
		}
	}
	else if (identifier.endsWith(QLatin1String("Action")))
	{
		const int actionIdentifier(ActionsManager::getActionIdentifier(identifier.left(identifier.length() - 6)));

		if (actionIdentifier < 0)
		{
			item->setText(tr("Invalid Entry"));
		}
		else
		{
			const ActionsManager::ActionDefinition definition(ActionsManager::getActionDefinition(actionIdentifier));

			item->setText(definition.getText(true));

			if (!definition.defaultState.icon.isNull())
			{
				item->setIcon(definition.defaultState.icon);
			}
		}
	}
	else
	{
		item->setText(tr("Invalid Entry"));
	}

	if (!options.isEmpty())
	{
		item->setData(options, OptionsRole);

		if (options.contains(QLatin1String("icon")))
		{
			const QString data(options[QLatin1String("icon")].toString());
			QIcon icon;

			if (data.startsWith(QLatin1String("data:image/")))
			{
				icon = QIcon(Utils::loadPixmapFromDataUri(data));
			}
			else
			{
				icon = ThemesManager::createIcon(data);
			}

			if (data.isEmpty())
			{
				item->setData(QColor(Qt::transparent), Qt::DecorationRole);
			}
			else if (!icon.isNull())
			{
				item->setIcon(icon);
			}
		}

		if (options.contains(QLatin1String("text")))
		{
			item->setText(options[QLatin1String("text")].toString());
		}
	}

	item->setToolTip(item->text());

	return item;
}

ToolBarsManager::ToolBarDefinition::Entry ToolBarDialog::getEntry(QStandardItem *item) const
{
	ToolBarsManager::ToolBarDefinition::Entry definition;

	if (!item)
	{
		return definition;
	}

	definition.action = item->data(IdentifierRole).toString();
	definition.options = item->data(OptionsRole).toMap();
	definition.parameters = item->data(ParametersRole).toMap();

	if (definition.action == QLatin1String("CustomMenu"))
	{
		definition.entries.reserve(item->rowCount());

		for (int i = 0; i < item->rowCount(); ++i)
		{
			definition.entries.append(getEntry(item->child(i, 0)));
		}
	}

	return definition;
}

ToolBarsManager::ToolBarDefinition ToolBarDialog::getDefinition() const
{
	ToolBarsManager::ToolBarDefinition definition(m_definition);
	definition.title = m_ui->titleLineEdit->text();
	definition.normalVisibility = static_cast<ToolBarsManager::ToolBarVisibility>(m_ui->normalVisibilityComboBox->currentData().toInt());
	definition.fullScreenVisibility = static_cast<ToolBarsManager::ToolBarVisibility>(m_ui->fullScreenVisibilityComboBox->currentData().toInt());
	definition.buttonStyle = static_cast<Qt::ToolButtonStyle>(m_ui->buttonStyleComboBox->currentData().toInt());
	definition.iconSize = m_ui->iconSizeSpinBox->value();
	definition.maximumButtonSize = m_ui->maximumButtonSizeSpinBox->value();
	definition.hasToggle = m_ui->hasToggleCheckBox->isChecked();

	switch (definition.type)
	{
		case ToolBarsManager::BookmarksBarType:
			definition.bookmarksPath = QLatin1Char('#') + QString::number(m_ui->folderComboBox->getCurrentFolder()->data(BookmarksModel::IdentifierRole).toULongLong());

			break;
		case ToolBarsManager::SideBarType:
			definition.panels.clear();

			for (int i = 0; i < m_ui->panelsViewWidget->model()->rowCount(); ++i)
			{
				const QStandardItem *item(m_ui->panelsViewWidget->getItem(i));

				if (item->data(Qt::CheckStateRole).toInt() == Qt::Checked)
				{
					definition.panels.append(item->data(TreeModel::UserRole).toString());
				}
			}

			if (!definition.panels.contains(definition.currentPanel))
			{
				definition.currentPanel.clear();
			}

			break;
		default:
			definition.entries.reserve(m_ui->currentEntriesItemView->model()->rowCount());

			for (int i = 0; i < m_ui->currentEntriesItemView->model()->rowCount(); ++i)
			{
				definition.entries.append(getEntry(m_ui->currentEntriesItemView->getItem(i)));
			}

			break;
	}

	return definition;
}

bool ToolBarDialog::eventFilter(QObject *object, QEvent *event)
{
	if (object == m_ui->availableEntriesItemView->viewport() && event->type() == QEvent::Drop)
	{
		event->accept();

		return true;
	}

	return Dialog::eventFilter(object, event);;
}

}
