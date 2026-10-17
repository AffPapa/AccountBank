/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "settings/sections/settings_accountbank.h"

#include "chat_helpers/text_replacements.h"
#include "core/application.h"
#include "core/core_settings.h"
#include "data/components/message_archive.h"
#include "data/data_session.h"
#include "lang/lang_keys.h"
#include "main/main_session.h"
#include "main/main_account.h"
#include "main/main_domain.h"
#include "settings/sections/settings_main.h"
#include "settings/settings_builder.h"
#include "ui/layers/generic_box.h"
#include "ui/vertical_list.h"
#include "ui/widgets/checkbox.h"
#include "window/window_accountbank_actions.h"
#include "window/window_session_controller.h"
#include "styles/style_menu_icons.h"
#include "styles/style_settings.h"

namespace Settings {
namespace {

using namespace Builder;

void AddPreference(
		SectionBuilder &builder,
		QString id,
		rpl::producer<QString> title,
		const char *key,
		bool defaultValue = false,
		bool refreshUnread = false) {
	const auto checked = Core::App().settings().readPref<bool>(
		key,
		defaultValue);
	const auto checkbox = builder.addCheckbox({
		.id = std::move(id),
		.title = std::move(title),
		.checked = checked,
	});
	if (!checkbox) {
		return;
	}
	checkbox->checkedChanges(
	) | rpl::filter([=](bool value) {
		return value != Core::App().settings().readPref<bool>(
			key,
			defaultValue);
	}) | rpl::on_next([=](bool value) {
		Core::App().settings().writePref<bool>(key, value);
		Core::App().saveSettingsDelayed();
		if (refreshUnread) {
			for (const auto &[index, account]
					: Core::App().domain().accounts()) {
				if (const auto session = account->maybeSession()) {
					session->data().applyUnreadFirstPref();
				}
			}
		}
	}, checkbox->lifetime());
}

void BuildAccountBankContent(SectionBuilder &builder) {
	const auto controller = builder.controller();
	builder.addSkip(st::settingsCheckboxesSkip);
	builder.addDividerText(tr::lng_accountbank_settings_scope());

	builder.addSkip(st::settingsCheckboxesSkip);
	builder.addSubsectionTitle({
		.id = u"accountbank/privacy"_q,
		.title = tr::lng_accountbank_settings_privacy(),
	});
	AddPreference(builder, u"accountbank/ghost"_q,
		tr::lng_accountbank_settings_ghost(), "ghost-mode", true);
	AddPreference(builder, u"accountbank/copy"_q,
		tr::lng_accountbank_settings_copy(), "allow-copy-restricted", true);
	AddPreference(builder, u"accountbank/streamer"_q,
		tr::lng_accountbank_settings_streamer(), "streamer-mode");
	builder.addDividerText(tr::lng_accountbank_settings_privacy_help());

	builder.addSkip(st::settingsCheckboxesSkip);
	builder.addSubsectionTitle({
		.id = u"accountbank/archive"_q,
		.title = tr::lng_accountbank_settings_archive(),
	});
	builder.addDividerText(tr::lng_accountbank_settings_archive_scope());
	AddPreference(builder, u"accountbank/keep-deleted"_q,
		tr::lng_accountbank_settings_keep_deleted(), "keep-deleted", true);
	builder.addDividerText(tr::lng_accountbank_keep_deleted_help());
	builder.addButton({
		.id = u"accountbank/deleted-archive"_q,
		.title = tr::lng_accountbank_deleted_archive(),
		.icon = { &st::menuIconSavedMessages },
		.onClick = [=] {
			Window::ShowAccountBankArchive(controller, u"delete"_q);
		},
	});
	builder.addButton({
		.id = u"accountbank/edited-archive"_q,
		.title = tr::lng_accountbank_edited_archive(),
		.icon = { &st::menuIconSavedMessages },
		.onClick = [=] {
			Window::ShowAccountBankArchive(controller, u"edit"_q);
		},
	});
	builder.addButton({
		.id = u"accountbank/ttl-archive"_q,
		.title = tr::lng_accountbank_ttl_archive(),
		.icon = { &st::menuIconSavedMessages },
		.onClick = [=] {
			Window::ShowAccountBankArchive(controller, u"saved_ttl_photo"_q);
		},
	});
	builder.addButton({
		.id = u"accountbank/stories-archive"_q,
		.title = tr::lng_accountbank_stories_archive(),
		.icon = { &st::menuIconSavedMessages },
		.onClick = [=] {
			Window::ShowAccountBankArchive(controller, u"saved_story"_q);
		},
	});
	builder.addButton({
		.id = u"accountbank/cloud-archive"_q,
		.title = tr::lng_accountbank_cloud_archive(),
		.icon = { &st::menuIconSavedMessages },
		.onClick = [=] {
			Window::OpenAccountBankCloudForum(controller);
		},
	});
	if (controller && controller->session().data().archive()
			.legacyCloudChannelId()) {
		builder.addButton({
			.id = u"accountbank/legacy-cloud-archive"_q,
			.title = tr::lng_accountbank_cloud_legacy(),
			.icon = { &st::menuIconSavedMessages },
			.onClick = [=] {
				Window::OpenAccountBankCloudForum(controller, true);
			},
		});
	}
	builder.addButton({
		.id = u"accountbank/export"_q,
		.title = tr::lng_accountbank_export_archive(),
		.icon = { &st::menuIconExport },
		.onClick = [=] {
			Window::ExportAccountBankArchive(controller);
		},
	});

	builder.addDivider();
	builder.addSkip(st::settingsCheckboxesSkip);
	builder.addSubsectionTitle({
		.id = u"accountbank/chats"_q,
		.title = tr::lng_accountbank_settings_chats(),
	});
	AddPreference(builder, u"accountbank/compact"_q,
		tr::lng_accountbank_compact_list(), "compact-chat-list");
	AddPreference(builder, u"accountbank/unread-first"_q,
		tr::lng_accountbank_unread_first(), "unread-first", false, true);
	AddPreference(builder, u"accountbank/seconds"_q,
		tr::lng_accountbank_time_seconds(), "accountbank-time-seconds");
	AddPreference(builder, u"accountbank/service-time"_q,
		tr::lng_accountbank_service_time(), "accountbank-service-time");
	builder.addDividerText(tr::lng_accountbank_settings_restart_help());
	builder.addButton({
		.id = u"accountbank/admin-chats"_q,
		.title = tr::lng_accountbank_admin_chats(),
		.icon = { &st::menuIconChatBubble },
		.onClick = [=] {
			Window::ShowAccountBankAdminChats(controller);
		},
	});

	builder.addDivider();
	builder.addSkip(st::settingsCheckboxesSkip);
	builder.addSubsectionTitle({
		.id = u"accountbank/notifications"_q,
		.title = tr::lng_accountbank_settings_notifications(),
	});
	AddPreference(builder, u"accountbank/mentions"_q,
		tr::lng_accountbank_ignore_group_mentions(),
		"accountbank-ignore-group-mentions");
	AddPreference(builder, u"accountbank/sponsored"_q,
		tr::lng_accountbank_settings_hide_sponsored(), "hide-sponsored", true);
	builder.addDividerText(tr::lng_accountbank_ignore_group_mentions_help());

	builder.addSkip(st::settingsCheckboxesSkip);
	builder.addSubsectionTitle({
		.id = u"accountbank/appearance"_q,
		.title = tr::lng_accountbank_settings_appearance(),
	});
	AddPreference(builder, u"accountbank/birthdays"_q,
		tr::lng_accountbank_hide_birthdays(),
		"accountbank-hide-birthday-suggestions");
	AddPreference(builder, u"accountbank/stories-strip"_q,
		tr::lng_accountbank_hide_stories_strip(),
		"accountbank-hide-stories-strip");
	AddPreference(builder, u"accountbank/similar"_q,
		tr::lng_accountbank_hide_similar_peers(),
		"accountbank-hide-similar-peers");
	builder.addDividerText(tr::lng_accountbank_settings_restart_help());

	builder.addDivider();
	builder.addSkip(st::settingsCheckboxesSkip);
	builder.addSubsectionTitle({
		.id = u"accountbank/tools"_q,
		.title = tr::lng_accountbank_settings_tools(),
	});
	builder.addButton({
		.id = u"accountbank/text-replacements"_q,
		.title = tr::lng_accountbank_text_replacements_title(),
		.icon = { &st::menuIconSettings },
		.onClick = [=] {
			controller->show(Box(ChatHelpers::ShowTextReplacementsBox));
		},
	});
	builder.addDividerText(tr::lng_accountbank_settings_replacements_help());
	builder.addButton({
		.id = u"accountbank/message-filter"_q,
		.title = tr::lng_accountbank_message_filter_menu(),
		.icon = { &st::menuIconSettings },
		.onClick = [=] {
			Window::ShowAccountBankMessageFilterBox(controller);
		},
	});
}

const auto kMeta = BuildHelper({
	.id = AccountBank::Id(),
	.parentId = MainId(),
	.title = &tr::lng_accountbank_settings_title,
	.icon = &st::menuIconSettings,
}, [](SectionBuilder &builder) {
	BuildAccountBankContent(builder);
});

const SectionBuildMethod kAccountBankSection = kMeta.build;

} // namespace

AccountBank::AccountBank(
		QWidget *parent,
		not_null<Window::SessionController*> controller)
: Section(parent, controller) {
	setupContent();
}

rpl::producer<QString> AccountBank::title() {
	return tr::lng_accountbank_settings_title();
}

void AccountBank::setupContent() {
	const auto content = Ui::CreateChild<Ui::VerticalLayout>(this);
	build(content, kAccountBankSection);
	Ui::ResizeFitChild(this, content);
}

Type AccountBankId() {
	return AccountBank::Id();
}

} // namespace Settings
