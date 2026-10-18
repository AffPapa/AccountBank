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
#include "data/data_user.h"
#include "lang/lang_keys.h"
#include "main/main_session.h"
#include "main/main_account.h"
#include "main/main_domain.h"
#include "settings/sections/settings_main.h"
#include "settings/settings_builder.h"
#include "ui/layers/generic_box.h"
#include "ui/vertical_list.h"
#include "ui/widgets/buttons.h"
#include "ui/widgets/checkbox.h"
#include "ui/widgets/labels.h"
#include "window/window_accountbank_actions.h"
#include "window/window_session_controller.h"
#include "styles/style_boxes.h"
#include "styles/style_layers.h"
#include "styles/style_menu_icons.h"
#include "styles/style_settings.h"

#include <QSet>

namespace Settings {
namespace {

using namespace Builder;

void ShowIgnoredUsersBox(
		not_null<Ui::GenericBox*> box,
		not_null<Window::SessionController*> controller) {
	box->setTitle(tr::lng_accountbank_settings_ignored_users());
	box->setWidth(st::boxWideWidth);
	box->setMaxHeight(st::boxMaxListHeight);
	box->addRow(object_ptr<Ui::FlatLabel>(
		box,
		tr::lng_accountbank_settings_ignored_help(),
		st::aboutLabel));
	box->addSkip(st::boxLittleSkip);
	const auto raw = Core::App().settings().ignoredUsers();
	auto shown = QSet<quint64>();
	for (const auto &part : raw.split(',')) {
		auto ok = false;
		const auto id = part.toULongLong(&ok);
		if (!ok || !id || shown.contains(id)) {
			continue;
		}
		shown.insert(id);
		const auto user = controller->session().data().userLoaded(UserId(id));
		const auto loadedName = user ? user->name() : QString();
		const auto name = !loadedName.isEmpty()
			? loadedName
			: tr::lng_accountbank_settings_ignored_unnamed(tr::now);
		box->addRow(object_ptr<Ui::FlatLabel>(
			box,
			name + u"\n"_q
				+ tr::lng_accountbank_peer_id_user(tr::now)
				+ QString::number(id),
			st::boxLabel));
		const auto remove = box->addRow(object_ptr<Ui::LinkButton>(
			box,
			tr::lng_accountbank_settings_ignored_remove(tr::now),
			st::aboutVersionLink));
		remove->setClickedCallback([=] {
			auto parts = Core::App().settings().ignoredUsers().split(',');
			parts.removeAll(QByteArray::number(id));
			Core::App().settings().setIgnoredUsers(parts.join(','));
			box->closeBox();
			controller->show(Box([=](not_null<Ui::GenericBox*> next) {
				ShowIgnoredUsersBox(next, controller);
			}));
		});
		box->addSkip(st::boxLittleSkip);
	}
	if (shown.isEmpty()) {
		box->addRow(object_ptr<Ui::FlatLabel>(
			box,
			tr::lng_accountbank_settings_ignored_empty(),
			st::aboutLabel));
	}
	box->addButton(tr::lng_close(), [=] { box->closeBox(); });
}

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
		.id = u"accountbank/local-archive"_q,
		.title = tr::lng_accountbank_settings_local_archive(),
		.icon = { &st::menuIconArchiveOpen },
		.onClick = [=] {
			Window::ShowAccountBankArchiveHome(controller);
		},
	});
	builder.addSubsectionTitle({
		.id = u"accountbank/cloud-and-export"_q,
		.title = tr::lng_accountbank_settings_cloud_export(),
	});
	builder.addButton({
		.id = u"accountbank/cloud-archive"_q,
		.title = tr::lng_accountbank_cloud_archive(),
		.icon = { &st::menuIconArchive },
		.onClick = [=] {
			Window::OpenAccountBankCloudForum(controller);
		},
	});
	if (controller && controller->session().data().archive()
			.legacyCloudChannelId()) {
		builder.addButton({
			.id = u"accountbank/legacy-cloud-archive"_q,
			.title = tr::lng_accountbank_cloud_legacy(),
			.icon = { &st::menuIconArchiveOpen },
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
	AddPreference(builder, u"accountbank/sponsored"_q,
		tr::lng_accountbank_settings_hide_sponsored(), "hide-sponsored", true);
	builder.addDividerText(tr::lng_accountbank_settings_chat_refresh());
	builder.addDividerText(tr::lng_accountbank_role_folders_help());

	builder.addDivider();
	builder.addSkip(st::settingsCheckboxesSkip);
	builder.addSubsectionTitle({
		.id = u"accountbank/notifications"_q,
		.title = tr::lng_accountbank_settings_notifications(),
	});
	AddPreference(builder, u"accountbank/mentions"_q,
		tr::lng_accountbank_ignore_group_mentions(),
		"accountbank-ignore-group-mentions");
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
	builder.addDividerText(tr::lng_accountbank_settings_appearance_refresh());

	builder.addDivider();
	builder.addSkip(st::settingsCheckboxesSkip);
	builder.addSubsectionTitle({
		.id = u"accountbank/tools"_q,
		.title = tr::lng_accountbank_settings_tools(),
	});
	builder.addButton({
		.id = u"accountbank/text-replacements"_q,
		.title = tr::lng_accountbank_text_replacements_title(),
		.icon = { &st::menuIconEdit },
		.onClick = [=] {
			controller->show(Box(ChatHelpers::ShowTextReplacementsBox));
		},
	});
	builder.addDividerText(tr::lng_accountbank_settings_replacements_help());
	builder.addButton({
		.id = u"accountbank/message-filter"_q,
		.title = tr::lng_accountbank_message_filter_menu(),
		.icon = { &st::menuIconSearch },
		.onClick = [=] {
			Window::ShowAccountBankMessageFilterBox(controller);
		},
	});
	builder.addButton({
		.id = u"accountbank/ignored-users"_q,
		.title = tr::lng_accountbank_settings_ignored_users(),
		.icon = { &st::menuIconBlock },
		.onClick = [=] {
			controller->show(Box([=](not_null<Ui::GenericBox*> box) {
				ShowIgnoredUsersBox(box, controller);
			}));
		},
	});
}

const auto kMeta = BuildHelper({
	.id = AccountBank::Id(),
	.parentId = MainId(),
	.title = &tr::lng_accountbank_settings_title,
	.icon = &st::menuIconManage,
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
