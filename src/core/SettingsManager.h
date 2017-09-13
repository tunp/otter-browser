/**************************************************************************
* Otter Browser: Web browser controlled by the user, not vice-versa.
* Copyright (C) 2013 - 2017 Michal Dutkiewicz aka Emdek <michal@emdek.pl>
* Copyright (C) 2014 - 2016 Piotr Wójcik <chocimier@tlen.pl>
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

#ifndef OTTER_SETTINGSMANAGER_H
#define OTTER_SETTINGSMANAGER_H

#include <QtCore/QObject>
#include <QtCore/QUrl>
#include <QtCore/QVariant>
#include <QtGui/QIcon>

namespace Otter
{

class SettingsManager final : public QObject
{
	Q_OBJECT
	Q_ENUMS(OptionIdentifier)
	Q_ENUMS(OptionType)

public:
	enum OptionIdentifier
	{
		AddressField_CompletionDisplayModeOption = 0,
		AddressField_CompletionModeOption,
		AddressField_DropActionOption,
		AddressField_HostLookupTimeoutOption,
		AddressField_LayoutOption,
		AddressField_PasteAndGoOnMiddleClickOption,
		AddressField_SelectAllOnFocusOption,
		AddressField_ShowCompletionCategoriesOption,
		AddressField_SuggestBookmarksOption,
		AddressField_SuggestHistoryOption,
		AddressField_SuggestLocalPathsOption,
		AddressField_SuggestSearchOption,
		AddressField_SuggestSpecialPagesOption,
		Backends_PasswordsOption,
		Backends_WebOption,
		Browser_AlwaysAskWhereToSaveDownloadOption,
		Browser_EnableMouseGesturesOption,
		Browser_EnableSingleKeyShortcutsOption,
		Browser_EnableSpellCheckOption,
		Browser_EnableTrayIconOption,
		Browser_HomePageOption,
		Browser_InactiveTabTimeUntilSuspendOption,
		Browser_KeyboardShortcutsProfilesOrderOption,
		Browser_LocaleOption,
		Browser_MigrationsOption,
		Browser_MouseProfilesOrderOption,
		Browser_OfflineStorageLimitOption,
		Browser_OfflineWebApplicationCacheLimitOption,
		Browser_OpenLinksInNewTabOption,
		Browser_PrintElementBackgroundsOption,
		Browser_PrivateModeOption,
		Browser_RememberPasswordsOption,
		Browser_ReuseCurrentTabOption,
		Browser_ShowSelectionContextMenuOnDoubleClickOption,
		Browser_SpellCheckDictionaryOption,
		Browser_StartupBehaviorOption,
		Browser_ToolTipsModeOption,
		Browser_TransferStartingActionOption,
		Browser_ValidatorsOrderOption,
		Cache_DiskCacheLimitOption,
		Cache_PagesInMemoryLimitOption,
		Choices_WarnFormResendOption,
		Choices_WarnLowDiskSpaceOption,
		Choices_WarnOpenBookmarkFolderOption,
		Choices_WarnOpenMultipleDroppedUrlsOption,
		Choices_WarnQuitOption,
		Choices_WarnQuitTransfersOption,
		Content_BackgroundColorOption,
		Content_CursiveFontOption,
		Content_DefaultCharacterEncodingOption,
		Content_DefaultFixedFontSizeOption,
		Content_DefaultFontSizeOption,
		Content_DefaultZoomOption,
		Content_FantasyFontOption,
		Content_FixedFontOption,
		Content_LinkColorOption,
		Content_MinimumFontSizeOption,
		Content_PageReloadTimeOption,
		Content_SansSerifFontOption,
		Content_SerifFontOption,
		Content_StandardFontOption,
		Content_TextColorOption,
		Content_UserStyleSheetOption,
		Content_VisitedLinkColorOption,
		Content_ZoomTextOnlyOption,
		ContentBlocking_CosmeticFiltersModeOption,
		ContentBlocking_EnableContentBlockingOption,
		ContentBlocking_EnableWildcardsOption,
		ContentBlocking_ProfilesOption,
		History_BrowsingLimitAmountGlobalOption,
		History_BrowsingLimitAmountWindowOption,
		History_BrowsingLimitPeriodOption,
		History_ClearOnCloseOption,
		History_DownloadsLimitPeriodOption,
		History_ExpandBranchesOption,
		History_ManualClearOptionsOption,
		History_ManualClearPeriodOption,
		History_RememberBrowsingOption,
		History_RememberClosedPrivateTabsOption,
		History_RememberDownloadsOption,
		History_StoreFaviconsOption,
		Interface_DateTimeFormatOption,
		Interface_EnableSmoothScrollingOption,
		Interface_IconThemePathOption,
		Interface_LastTabClosingActionOption,
		Interface_LockToolBarsOption,
		Interface_NewTabOpeningActionOption,
		Interface_NotificationVisibilityDurationOption,
		Interface_ShowScrollBarsOption,
		Interface_StyleSheetOption,
		Interface_TabCrashingActionOption,
		Interface_UseFancyDateTimeFormatOption,
		Interface_UseNativeNotificationsOption,
		Interface_UseSystemIconThemeOption,
		Interface_WidgetStyleOption,
		Network_AcceptLanguageOption,
		Network_CookiesKeepModeOption,
		Network_CookiesPolicyOption,
		Network_DoNotTrackPolicyOption,
		Network_EnableReferrerOption,
		Network_ProxyOption,
		Network_ThirdPartyCookiesAcceptedHostsOption,
		Network_ThirdPartyCookiesPolicyOption,
		Network_ThirdPartyCookiesRejectedHostsOption,
		Network_UserAgentOption,
		Network_WorkOfflineOption,
		Paths_DownloadsOption,
		Paths_OpenFileOption,
		Paths_SaveFileOption,
		Permissions_EnableFullScreenOption,
		Permissions_EnableGeolocationOption,
		Permissions_EnableImagesOption,
		Permissions_EnableJavaScriptOption,
		Permissions_EnableLocalStorageOption,
		Permissions_EnableMediaCaptureAudioOption,
		Permissions_EnableMediaCaptureVideoOption,
		Permissions_EnableMediaPlaybackAudioOption,
		Permissions_EnableNotificationsOption,
		Permissions_EnableOfflineStorageDatabaseOption,
		Permissions_EnableOfflineWebApplicationCacheOption,
		Permissions_EnablePluginsOption,
		Permissions_EnablePointerLockOption,
		Permissions_EnableWebglOption,
		Permissions_ScriptsCanAccessClipboardOption,
		Permissions_ScriptsCanChangeWindowGeometryOption,
		Permissions_ScriptsCanCloseSelfOpenedWindowsOption,
		Permissions_ScriptsCanCloseWindowsOption,
		Permissions_ScriptsCanOpenWindowsOption,
		Permissions_ScriptsCanReceiveRightClicksOption,
		Permissions_ScriptsCanShowStatusMessagesOption,
		Search_DefaultQuickSearchEngineOption,
		Search_DefaultSearchEngineOption,
		Search_EnableFindInPageAsYouTypeOption,
		Search_EnableFindInPageHighlightAllOption,
		Search_ReuseLastQuickFindQueryOption,
		Search_SearchEnginesOrderOption,
		Search_SearchEnginesSuggestionsOption,
		Security_AllowMixedContentOption,
		Security_CiphersOption,
		Security_IgnoreSslErrorsOption,
		Sessions_DeferTabsLoadingOption,
		Sessions_OpenInExistingWindowOption,
		Sessions_OptionsExludedFromInheritingOption,
		Sessions_OptionsExludedFromSavingOption,
		SourceViewer_ShowLineNumbersOption,
		SourceViewer_WrapLinesOption,
		StartPage_BackgroundColorOption,
		StartPage_BackgroundModeOption,
		StartPage_BackgroundPathOption,
		StartPage_BookmarksFolderOption,
		StartPage_EnableStartPageOption,
		StartPage_ShowAddTileOption,
		StartPage_ShowSearchFieldOption,
		StartPage_TileBackgroundModeOption,
		StartPage_TileHeightOption,
		StartPage_TileWidthOption,
		StartPage_TilesPerRowOption,
		StartPage_ZoomLevelOption,
		TabBar_EnablePreviewsOption,
		TabBar_EnableThumbnailsOption,
		TabBar_MaximumTabHeightOption,
		TabBar_MinimumTabHeightOption,
		TabBar_MaximumTabWidthOption,
		TabBar_MinimumTabWidthOption,
		TabBar_OpenNextToActiveOption,
		TabBar_PrependPinnedTabOption,
		TabBar_PreviewsAnimationDurationOption,
		TabBar_RequireModifierToSwitchTabOnScrollOption,
		TabBar_ShowCloseButtonOption,
		TabBar_ShowUrlIconOption,
		TabSwitcher_IgnoreMinimizedTabsOption,
		TabSwitcher_OrderByLastActivityOption,
		TabSwitcher_ShowListOption,
		Updates_ActiveChannelsOption,
		Updates_AutomaticInstallOption,
		Updates_CheckIntervalOption,
		Updates_LastCheckOption,
		Updates_ServerUrlOption
	};

	enum OptionType
	{
		UnknownType = 0,
		BooleanType,
		ColorType,
		EnumerationType,
		FontType,
		IconType,
		IntegerType,
		ListType,
		PathType,
		PasswordType,
		StringType
	};

	enum OptionFlag
	{
		NoFlags = 0,
		IsEnabledFlag = 1,
		IsVisibleFlag = 2,
		IsBuiltInFlag = 4
	};

	Q_DECLARE_FLAGS(OptionFlags, OptionFlag)

	struct OptionDefinition
	{
		struct ChoiceDefinition
		{
			QString title;
			QString value;
			QIcon icon;

			QString getTitle() const
			{
				return (title.isEmpty() ? value : title);
			}

			bool isValid() const
			{
				return (!title.isEmpty() || !value.isEmpty());
			}
		};

		QVariant defaultValue;
		QVector<ChoiceDefinition> choices;
		OptionType type = UnknownType;
		OptionFlags flags = static_cast<OptionFlags>(IsEnabledFlag | IsVisibleFlag);
		int identifier = -1;

		void setChoices(const QStringList &choicesValue)
		{
			choices.reserve(choicesValue.count());

			for (int i = 0; i < choicesValue.count(); ++i)
			{
				choices.append({QString(), choicesValue.at(i), QIcon()});
			}
		}

		bool hasIcons() const
		{
			for (int i = 0; i < choices.count(); ++i)
			{
				if (!choices.at(i).icon.isNull())
				{
					return true;
				}
			}

			return false;
		}
	};

	static void createInstance(const QString &path);
	static void removeOverride(const QUrl &url, const QString &key = {});
	static void updateOptionDefinition(int identifier, const OptionDefinition &definition);
	static void setOption(int identifier, const QVariant &value, const QUrl &url = {});
	static SettingsManager* getInstance();
	static QString createDisplayValue(int identifier, const QVariant &value);
	static QString createReport();
	static QString getGlobalPath();
	static QString getOverridePath();
	static QString getOptionName(int identifier);
	static QVariant getOption(int identifier, const QUrl &url = {});
	static QStringList getOptions();
	static OptionDefinition getOptionDefinition(int identifier);
	static int registerOption(const QString &name, OptionType type, const QVariant &defaultValue = {}, const QStringList &choices = {});
	static int getOptionIdentifier(const QString &name);
	static bool hasOverride(const QUrl &url, int identifier = -1);

protected:
	explicit SettingsManager(QObject *parent);

	static QString getHost(const QUrl &url);
	static void registerOption(int identifier, OptionType type, const QVariant &defaultValue = {}, const QStringList &choices = {});
	static void saveOption(const QString &path, const QString &key, const QVariant &value, OptionType type);

private:
	static SettingsManager *m_instance;
	static QString m_globalPath;
	static QString m_overridePath;
	static QVector<OptionDefinition> m_definitions;
	static QHash<QString, int> m_customOptions;
	static int m_identifierCounter;
	static int m_optionIdentifierEnumerator;
	static bool m_hasWildcardedOverrides;

signals:
	void optionChanged(int identifier, const QVariant &value);
	void optionChanged(int identifier, const QVariant &value, const QUrl &url);
};

}

Q_DECLARE_OPERATORS_FOR_FLAGS(Otter::SettingsManager::OptionFlags)

#endif
