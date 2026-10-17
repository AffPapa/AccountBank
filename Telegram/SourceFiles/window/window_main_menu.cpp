/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "window/window_main_menu.h"

#include "apiwrap.h"
#include "base/event_filter.h"
#include "base/qt_signal_producer.h"
#include "boxes/about_box.h"
#include "core/update_channel.h"
#include "boxes/peer_list_controllers.h"
#include "boxes/premium_preview_box.h"
#include "calls/group/calls_group_common.h"
#include "calls/calls_box_controller.h"
#include "calls/calls_instance.h"
#include "core/application.h"
#include "core/file_utilities.h"
#include "core/click_handler_types.h"
#include "data/data_changes.h"
#include "data/data_channel.h"
#include "data/data_chat.h"
#include "data/data_document_media.h"
#include "data/data_folder.h"
#include "data/data_group_call.h"
#include "data/data_session.h"
#include "data/components/message_archive.h"
#include "data/data_stories.h"
#include "data/data_user.h"
#include "info/info_memento.h"
#include "info/profile/info_profile_badge.h"
#include "settings/settings_common.h"
#include "info/profile/info_profile_emoji_status_panel.h"
#include "info/profile/info_profile_icon.h"
#include "info/stories/info_stories_widget.h"
#include "lang/lang_keys.h"
#include "main/main_account.h"
#include "main/main_app_config.h"
#include "main/main_domain.h"
#include "main/main_session.h"
#include "main/main_session_settings.h"
#include "mtproto/mtproto_config.h"
#include "settings/sections/settings_advanced.h"
#include "settings/sections/settings_calls.h"
#include "settings/sections/settings_information.h"
#include "storage/localstorage.h"
#include "storage/storage_account.h"
#include "support/support_templates.h"
#include "tde2e/tde2e_api.h"
#include "tde2e/tde2e_integration.h"
#include "ui/boxes/confirm_box.h"
#include "ui/layers/generic_box.h"
#include "ui/chat/chat_theme.h"
#include "ui/controls/swipe_handler.h"
#include "ui/controls/userpic_button.h"
#include "ui/widgets/buttons.h"
#include "ui/effects/toggle_arrow.h"
#include "ui/new_badges.h"
#include "ui/painter.h"
#include "ui/text/text_options.h"
#include "ui/text/text_utilities.h"
#include "ui/ui_utility.h"
#include "ui/unread_badge_paint.h"
#include "ui/vertical_list.h"
#include "ui/widgets/menu/menu_add_action_callback_factory.h"
#include "ui/widgets/popup_menu.h"
#include "ui/widgets/scroll_area.h"
#include "ui/widgets/labels.h"
#include "chat_helpers/text_replacements.h"
#include "core/core_settings.h"
#include "ui/layers/box_content.h"
#include "ui/widgets/fields/input_field.h"
#include "ui/widgets/shadow.h"
#include "ui/wrap/slide_wrap.h"
#include "wallet/wallet_panel.h"
#include "window/themes/window_theme.h"
#include "window/window_controller.h"
#include "window/window_accountbank_actions.h"
#include "window/window_main_menu_helpers.h"
#include "window/window_peer_menu.h"
#include "window/window_session_controller.h"
#include "styles/style_chat.h" // popupMenuExpandedSeparator
#include "styles/style_boxes.h"
#include "styles/style_layers.h"
#include "styles/style_chat_helpers.h"
#include "styles/style_menu_icons.h"
#include "styles/style_settings.h"
#include "styles/style_wallet.h"
#include "styles/style_window.h"

#include <algorithm>
#include "styles/style_window_main_menu.h"

#include <QtGui/QWindow>
#include <QtGui/QScreen>

#include <QtGui/QGuiApplication>
#include <QtGui/QClipboard>
#include <QDateTime>
#include <QFileInfo>
#include <QLocale>

namespace {

void ShowMessageFilterBox(not_null<Ui::GenericBox*> box) {
	box->setTitle(tr::lng_accountbank_message_filter_title());
	box->addRow(
		object_ptr<Ui::FlatLabel>(
			box,
			tr::lng_accountbank_message_filter_about(),
			st::boxLabel),
		st::boxRowPadding);
	const auto field = box->addRow(object_ptr<Ui::InputField>(
		box.get(),
		st::textReplacementsField,
		Ui::InputField::Mode::MultiLine,
		rpl::producer<QString>(),
		TextWithTags{ QString::fromUtf8(
			Core::App().settings().messageFilter()) }));
	field->setSubmitSettings(Ui::InputField::SubmitSettings::CtrlEnter);
	box->setFocusCallback([=] {
		field->setFocusFast();
	});
	const auto save = [=] {
		Core::App().settings().setMessageFilter(
			field->getLastText().toUtf8());
		box->closeBox();
	};
	field->submits(
	) | rpl::on_next([=] {
		save();
	}, field->lifetime());
	box->addButton(tr::lng_settings_save(), save);
	box->addButton(tr::lng_cancel(), [=] {
		box->closeBox();
	});
}

class AccountBankAdminChats final : public PeerListController {
public:
	AccountBankAdminChats(
			not_null<Window::SessionController*> controller)
	: _controller(controller) {
	}

	Main::Session &session() const override {
		return _controller->session();
	}

	void prepare() override {
		delegate()->peerListSetTitle(tr::lng_accountbank_admin_chats());
		delegate()->peerListSetSearchMode(PeerListSearchMode::Enabled);
		auto peers = std::vector<not_null<PeerData*>>();
		const auto append = [&](not_null<PeerData*> peer) {
			if (const auto chat = peer->asChat()) {
				if (chat->amIn()
					&& (chat->amCreator() || chat->hasAdminRights())) {
					peers.push_back(peer);
				}
			} else if (const auto channel = peer->asChannel()) {
				if (channel->amIn()
					&& (channel->amCreator() || channel->hasAdminRights())) {
					peers.push_back(peer);
				}
			}
		};
		session().data().enumerateGroups(append);
		session().data().enumerateBroadcasts(append);
		std::sort(peers.begin(), peers.end(), [](auto a, auto b) {
			return a->name().localeAwareCompare(b->name()) < 0;
		});
		for (const auto peer : peers) {
			auto row = std::make_unique<PeerListRow>(peer);
			const auto creator = peer->isChat()
				? peer->asChat()->amCreator()
				: peer->asChannel()->amCreator();
			row->setCustomStatus(creator
				? tr::lng_accountbank_chat_owner(tr::now)
				: tr::lng_accountbank_chat_admin(tr::now));
			delegate()->peerListAppendRow(std::move(row));
		}
		delegate()->peerListRefreshRows();
	}

	void rowClicked(not_null<PeerListRow*> row) override {
		_controller->showPeerHistory(row->peer()->id);
	}

private:
	const not_null<Window::SessionController*> _controller;
};

QString AccountBankElapsed(qint64 sent, qint64 observed) {
	if (sent <= 0 || observed < sent) {
		return u"?"_q;
	}
	const auto minutes = (observed - sent) / 60000;
	if (minutes >= 1440) {
		return tr::lng_days_tiny(tr::now, lt_count, int(minutes / 1440))
			+ u" "_q
			+ tr::lng_hours_tiny(tr::now, lt_count, int((minutes % 1440) / 60));
	} else if (minutes >= 60) {
		return tr::lng_hours_tiny(tr::now, lt_count, int(minutes / 60))
			+ u" "_q
			+ tr::lng_minutes_tiny(tr::now, lt_count, int(minutes % 60));
	}
	return tr::lng_minutes_tiny(tr::now, lt_count, int(minutes));
}

void ShowAccountBankArchiveBox(
		not_null<Window::SessionController*> controller,
		const QString &kind,
		const QString &peerFilter = QString(),
		const QString &peerTitle = QString()) {
	controller->show(Box([=](not_null<Ui::GenericBox*> box) {
		const auto title = [](const QString &value) {
			return value == u"delete"_q
				? tr::lng_accountbank_deleted_archive(tr::now)
				: value == u"edit"_q
					? tr::lng_accountbank_edited_archive(tr::now)
					: value == u"saved_ttl_photo"_q
						? tr::lng_accountbank_ttl_archive(tr::now)
						: tr::lng_accountbank_stories_archive(tr::now);
		};
		box->setTitle(rpl::single(peerFilter.isEmpty()
			? title(kind)
			: title(kind) + u" · "_q
				+ (peerTitle.isEmpty() ? peerFilter : peerTitle)));
		box->setWidth(st::boxWideWidth);
		box->setMaxHeight(st::boxMaxListHeight);
		if (!peerFilter.isEmpty()) {
			const auto all = box->addRow(object_ptr<Ui::LinkButton>(
				box,
				tr::lng_accountbank_archive_all_chats(tr::now),
				st::aboutVersionLink));
			all->setClickedCallback([=] {
				box->closeBox();
				ShowAccountBankArchiveBox(controller, kind);
			});
		}
		for (const auto &category : {
			u"delete"_q,
			u"edit"_q,
			u"saved_ttl_photo"_q,
			u"saved_story"_q,
		}) {
			if (category == kind) {
				continue;
			}
			const auto link = box->addRow(object_ptr<Ui::LinkButton>(
				box,
				title(category),
				st::aboutVersionLink));
			link->setClickedCallback([=] {
				box->closeBox();
				ShowAccountBankArchiveBox(controller, category, peerFilter, peerTitle);
			});
		}
		box->addSkip(st::boxLittleSkip);
		box->addRow(object_ptr<Ui::FlatLabel>(
			box,
			tr::lng_accountbank_archive_about(tr::now),
			st::aboutLabel));
		const auto status = box->addRow(object_ptr<Ui::FlatLabel>(
			box,
			tr::lng_accountbank_archive_loading(tr::now),
			st::aboutLabel));
		box->addButton(tr::lng_close(), [=] { box->closeBox(); });
		const auto weak = base::make_weak(box);
		controller->session().data().archive().loadRecent(
			kind,
			peerFilter,
			[weak, status, kind, peerFilter, controller](
					Data::MessageArchive::RecentResult result) {
				if (!weak) {
					return;
				}
				weak->setMaxHeight(st::boxMaxListHeight);
				if (!result.ok) {
					status->setText(tr::lng_accountbank_archive_read_failed(tr::now));
					return;
				}
				if (result.rows.empty()) {
					status->setText(
						tr::lng_accountbank_archive_empty(tr::now)
						+ (controller->session().data().archive().atCapacity()
							? u"\n"_q + tr::lng_accountbank_archive_full(tr::now)
							: QString()));
					return;
				}
				status->setText((kind == u"delete"_q || kind == u"edit"_q
					? tr::lng_accountbank_archive_recent(tr::now)
					: tr::lng_accountbank_archive_saved_note(tr::now))
					+ (controller->session().data().archive().atCapacity()
						? u"\n"_q + tr::lng_accountbank_archive_full(tr::now)
						: QString()));
				for (const auto &row : result.rows) {
					if (kind == u"saved_ttl_photo"_q
						|| kind == u"saved_story"_q) {
						const auto path = row.value(u"path"_q).toString();
						const auto when = QLocale().toString(
							QDateTime::fromMSecsSinceEpoch(
								qint64(row.value(u"ts"_q).toDouble())),
							QLocale::ShortFormat);
						const auto sender = row.value(u"from_name"_q).toString();
						const auto peer = row.value(u"peer_name"_q).toString();
						const auto names = sender.isEmpty()
							? peer
							: (peer.isEmpty() || sender == peer
								? sender
								: sender + u" · "_q + peer);
						const auto exists = QFileInfo::exists(path);
						const auto label = weak->addRow(
							object_ptr<Ui::FlatLabel>(
								weak.get(),
								when + (names.isEmpty()
									? QString()
									: u" · "_q + names) + u"\n"_q + path
									+ (exists
										? QString()
										: u" ["_q
											+ tr::lng_accountbank_archive_missing(tr::now)
											+ u"]"_q),
								st::aboutLabel));
						label->setSelectable(true);
						if (exists) {
							const auto reveal = weak->addRow(
								object_ptr<Ui::LinkButton>(
									weak.get(),
									tr::lng_accountbank_archive_reveal(tr::now),
									st::aboutVersionLink));
							reveal->setClickedCallback([path] {
								File::ShowInFolder(path);
							});
						}
						const auto peerId = row.value(u"peer"_q).toString();
						if (peerFilter.isEmpty() && !peerId.isEmpty()) {
							const auto dialog = weak->addRow(
								object_ptr<Ui::LinkButton>(
									weak.get(),
									tr::lng_accountbank_archive_dialog(tr::now),
									st::aboutVersionLink));
							dialog->setClickedCallback(
								[weak, controller, kind, peerId, peer] {
									if (weak) weak->closeBox();
									ShowAccountBankArchiveBox(controller, kind, peerId, peer);
								});
						}
						continue;
					}
					const auto from = row.value(u"from_name"_q).toString();
					const auto fromId = row.value(u"from"_q).toString();
					const auto peer = row.value(u"peer_name"_q).toString();
					const auto peerId = row.value(u"peer"_q).toString();
					const auto observed = qint64(row.value(u"ts"_q).toDouble());
					const auto sent = qint64(row.value(u"date"_q).toDouble()) * 1000;
					const auto elapsed = AccountBankElapsed(sent, observed);
					const auto when = QLocale().toString(
						QDateTime::fromMSecsSinceEpoch(observed),
						QLocale::ShortFormat);
					const auto text = row.value(u"text"_q).toString().left(800);
					const auto media = row.contains(u"photo"_q)
						? u" ["_q
							+ tr::lng_accountbank_archive_photo_metadata(tr::now)
							+ u"]"_q
						: row.contains(u"document"_q)
							? u" ["_q
								+ tr::lng_accountbank_archive_file_metadata(tr::now)
								+ u"]"_q
							: QString();
					const auto timeInfo = when + ((sent > 0 && observed >= sent)
						? u" · ~"_q + elapsed + u" "_q
							+ tr::lng_accountbank_archive_after_sent(tr::now)
						: QString());
					const auto beforeKnown = row.value(u"before_known"_q).toBool(true);
					const auto before = beforeKnown
						? (text.isEmpty()
							? tr::lng_accountbank_archive_no_text(tr::now)
							: text)
						: tr::lng_accountbank_archive_unknown_body(tr::now);
					const auto beforeNote = row.value(u"before_may_be_stale"_q).toBool()
						? u" "_q + tr::lng_accountbank_archive_old_version(tr::now)
						: QString();
					const auto afterText = row.value(u"after_text"_q).toString().left(800);
					const auto afterPhoto = row.value(u"after_photo"_q).toString();
					const auto afterDocument = row.value(u"after_document"_q).toString();
					const auto afterMedia = !afterPhoto.isEmpty()
						? u" ["_q
							+ tr::lng_accountbank_archive_photo_metadata(tr::now)
							+ u" #"_q + afterPhoto + u"]"_q
						: !afterDocument.isEmpty()
							? u" ["_q
								+ tr::lng_accountbank_archive_file_metadata(tr::now)
								+ u" #"_q + afterDocument + u"]"_q
							: QString();
					const auto after = (kind == u"edit"_q)
						? u"\n"_q + tr::lng_accountbank_archive_after(tr::now)
							+ u" "_q + (afterText.isEmpty()
								? tr::lng_accountbank_archive_no_text(tr::now)
								: afterText) + afterMedia
						: QString();
					const auto summary = QString::fromUtf8("%1 · %2\n%3\n%4 %5%6%7")
						.arg(from.isEmpty()
							? (fromId.isEmpty()
								? tr::lng_accountbank_archive_unknown(tr::now)
								: fromId)
							: from)
						.arg(peer.isEmpty() ? peerId : peer)
						.arg(timeInfo)
						.arg(tr::lng_accountbank_archive_before(tr::now))
						.arg(before + beforeNote)
						.arg(media)
						.arg(after);
					const auto label = weak->addRow(
						object_ptr<Ui::FlatLabel>(weak.get(), summary, st::aboutLabel));
					label->setSelectable(true);
					if (peerFilter.isEmpty() && !peerId.isEmpty()) {
						const auto dialog = weak->addRow(
							object_ptr<Ui::LinkButton>(
								weak.get(),
								tr::lng_accountbank_archive_dialog(tr::now),
								st::aboutVersionLink));
						dialog->setClickedCallback(
							[weak, controller, kind, peerId, peer] {
								if (weak) weak->closeBox();
								ShowAccountBankArchiveBox(controller, kind, peerId, peer);
							});
					}
				}
			});
	}));
}

} // namespace

namespace Window {

void ShowAccountBankArchive(
		not_null<SessionController*> controller,
		const QString &kind) {
	ShowAccountBankArchiveBox(controller, kind);
}

void ShowAccountBankAdminChats(not_null<SessionController*> controller) {
	controller->show(Box<PeerListBox>(
		std::make_unique<AccountBankAdminChats>(controller),
		[](not_null<PeerListBox*>) {}));
}

void ShowAccountBankMessageFilterBox(
		not_null<SessionController*> controller) {
	controller->show(Box(::ShowMessageFilterBox));
}

void ExportAccountBankArchive(not_null<SessionController*> controller) {
	auto &session = controller->session();
	auto &archive = session.data().archive();
	if (archive.exportInProgress()) {
		controller->showToast(
			tr::lng_accountbank_archive_export_running(tr::now));
		return;
	}
	const auto path = File::DefaultDownloadPath(&session)
		+ u"AccountBank archive %1.json"_q.arg(session.userId().bare);
	const auto weak = base::make_weak(controller);
	archive.exportAsync(path, [weak, path](bool ok) {
		if (!weak) {
			return;
		}
		weak->showToast(ok
			? tr::lng_accountbank_archive_exported(tr::now, lt_path, path)
			: tr::lng_accountbank_archive_failed(tr::now));
	});
}

void OpenAccountBankCloudForum(
		not_null<SessionController*> controller,
		bool legacy) {
	auto &session = controller->session();
	auto &archive = session.data().archive();
	if (legacy) {
		const auto id = archive.legacyCloudChannelId();
		if (!id) {
			return;
		}
		const auto channel = session.data().channel(ChannelId(id));
		channel->setAccessHash(archive.legacyCloudAccessHash());
		session.api().requestFullPeer(channel);
		controller->showPeerHistory(channel);
	} else if (!archive.ensureCloudForum()) {
		controller->showToast(tr::lng_accountbank_cloud_failed(tr::now));
	} else if (const auto id = archive.cloudForumId()) {
		const auto channel = session.data().channel(ChannelId(id));
		channel->setAccessHash(archive.cloudForumAccessHash());
		session.api().requestFullPeer(channel);
		controller->showPeerHistory(channel);
	} else {
		controller->showToast(tr::lng_accountbank_cloud_creating(tr::now));
	}
}

class MainMenu::ToggleAccountsButton final : public Ui::AbstractButton {
public:
	ToggleAccountsButton(QWidget *parent, not_null<Main::Account*> current);

	[[nodiscard]] int rightSkip() const {
		return _rightSkip.current();
	}
	[[nodiscard]] rpl::producer<int> rightSkipValue() const {
		return _rightSkip.value();
	}

private:
	void paintEvent(QPaintEvent *e) override;
	void paintUnreadBadge(Painter &p);

	void validateUnreadBadge();
	[[nodiscard]] QString computeUnreadBadge() const;

	const not_null<Main::Account*> _current;
	rpl::variable<int> _rightSkip = 0;
	Ui::Animations::Simple _toggledAnimation;
	bool _toggled = false;

	QString _unreadBadge;
	bool _unreadBadgeStale = false;

};

class MainMenu::ResetScaleButton final : public Ui::AbstractButton {
public:
	ResetScaleButton(QWidget *parent);

protected:
	void paintEvent(QPaintEvent *e) override;

	static constexpr auto kText = "100%";

};

MainMenu::ToggleAccountsButton::ToggleAccountsButton(
	QWidget *parent,
	not_null<Main::Account*> current)
: AbstractButton(parent)
, _current(current) {
	rpl::single(rpl::empty) | rpl::then(
		Core::App().unreadBadgeChanges()
	) | rpl::on_next([=] {
		_unreadBadgeStale = true;
		if (!_toggled) {
			validateUnreadBadge();
			update();
		}
	}, lifetime());

	auto &settings = Core::App().settings();
	if (Core::App().domain().accounts().size() < 2
		&& settings.mainMenuAccountsShown()) {
		settings.setMainMenuAccountsShown(false);
	}
	settings.mainMenuAccountsShownValue(
	) | rpl::filter([=](bool value) {
		return (_toggled != value);
	}) | rpl::on_next([=](bool value) {
		_toggled = value;
		_toggledAnimation.start(
			[=] { update(); },
			_toggled ? 0. : 1.,
			_toggled ? 1. : 0.,
			st::slideWrapDuration);
		validateUnreadBadge();
	}, lifetime());
	_toggledAnimation.stop();
}

void MainMenu::ToggleAccountsButton::paintEvent(QPaintEvent *e) {
	auto p = Painter(this);

	const auto path = Ui::ToggleUpDownArrowPath(
		0. + width() - st::mainMenuTogglePosition.x(),
		0. + height() - st::mainMenuTogglePosition.y(),
		st::mainMenuToggleSize,
		st::mainMenuToggleFourStrokes,
		_toggledAnimation.value(_toggled ? 1. : 0.));

	auto hq = PainterHighQualityEnabler(p);
	p.fillPath(path, st::windowSubTextFg);

	paintUnreadBadge(p);
}

void MainMenu::ToggleAccountsButton::paintUnreadBadge(Painter &p) {
	const auto progress = 1. - _toggledAnimation.value(_toggled ? 1. : 0.);
	if (!progress) {
		return;
	}
	validateUnreadBadge();
	if (_unreadBadge.isEmpty()) {
		return;
	}

	auto st = Settings::Badge::Style();
	const auto right = width()
		- st::mainMenuTogglePosition.x()
		- st::mainMenuToggleSize * 3;
	const auto top = height()
		- st::mainMenuTogglePosition.y()
		- st::mainMenuBadgeSize / 2;
	p.setOpacity(progress);
	Ui::PaintUnreadBadge(p, _unreadBadge, right, top, st);
}

void MainMenu::ToggleAccountsButton::validateUnreadBadge() {
	const auto base = st::mainMenuTogglePosition.x()
		+ 2 * st::mainMenuToggleSize;
	if (_toggled) {
		_rightSkip = base;
		return;
	} else if (!_unreadBadgeStale) {
		return;
	}
	_unreadBadge = computeUnreadBadge();

	auto skip = base;
	if (!_unreadBadge.isEmpty()) {
		const auto st = Settings::Badge::Style();
		skip += 2 * st::mainMenuToggleSize
			+ Ui::CountUnreadBadgeSize(_unreadBadge, st).width();
	}
	_rightSkip = skip;
}

QString MainMenu::ToggleAccountsButton::computeUnreadBadge() const {
	const auto state = OtherAccountsUnreadStateCurrent(_current);
	return state.allMuted
		? QString()
		: (state.count > 0)
		? Lang::FormatCountToShort(state.count).string
		: QString();
}

MainMenu::ResetScaleButton::ResetScaleButton(QWidget *parent)
: AbstractButton(parent) {
	const auto margin = st::mainMenuCloudButton.height
		- st::mainMenuCloudSize;
	const auto textWidth = st::mainMenuResetScaleFont->width(kText);
	const auto innerWidth = st::mainMenuResetScaleLeft
		+ textWidth
		+ st::mainMenuResetScaleRight;
	const auto width = margin + innerWidth;
	resize(width, st::mainMenuCloudButton.height);
}

void MainMenu::ResetScaleButton::paintEvent(QPaintEvent *e) {
	Painter p(this);

	const auto innerHeight = st::mainMenuCloudSize;
	const auto radius = innerHeight / 2;
	const auto margin = st::mainMenuCloudButton.height
		- st::mainMenuCloudSize;
	const auto textWidth = st::mainMenuResetScaleFont->width(kText);
	const auto innerWidth = st::mainMenuResetScaleLeft
		+ textWidth
		+ st::mainMenuResetScaleRight;
	const auto left = margin / 2;
	const auto top = margin / 2;
	p.setPen(Qt::NoPen);
	p.setBrush(st::mainMenuCloudBg);
	p.drawRoundedRect(left, top, innerWidth, innerHeight, radius, radius);

	st::settingsIconInterfaceScale.paint(
		p,
		left + st::mainMenuResetScaleIconLeft,
		top + ((innerHeight - st::settingsIconInterfaceScale.height()) / 2),
		width(),
		st::mainMenuCloudFg->c);

	p.setFont(st::mainMenuResetScaleFont);
	p.setPen(st::mainMenuCloudFg);
	p.drawText(
		left + st::mainMenuResetScaleLeft,
		top + st::mainMenuResetScaleTop + st::mainMenuResetScaleFont->ascent,
		kText);
}

MainMenu::MainMenu(
	QWidget *parent,
	not_null<SessionController*> controller)
: LayerWidget(parent)
, _controller(controller)
, _userpicButton(
	this,
	_controller->session().user(),
	st::mainMenuUserpic)
, _toggleAccounts(this, &controller->session().account())
, _scroll(this, st::defaultSolidScroll)
, _inner(_scroll->setOwnedWidget(
	object_ptr<Ui::VerticalLayout>(_scroll.data())))
, _topShadowSkip(_inner->add(
	object_ptr<Ui::FixedHeightWidget>(_inner.get(), st::lineWidth)))
, _accounts(_inner->add(object_ptr<Ui::SlideWrap<Ui::VerticalLayout>>(
	_inner.get(),
	object_ptr<Ui::VerticalLayout>(_inner.get()))))
, _shadow(_inner->add(object_ptr<Ui::SlideWrap<Ui::PlainShadow>>(
	_inner.get(),
	object_ptr<Ui::PlainShadow>(_inner.get()))))
, _menu(_inner->add(
	object_ptr<Ui::VerticalLayout>(_inner.get()),
	{ 0, st::mainMenuSkip, 0, 0 }))
, _footer(_inner->add(object_ptr<Ui::RpWidget>(_inner.get())))
, _telegram(
	Ui::CreateChild<Ui::FlatLabel>(_footer.get(), st::mainMenuTelegramLabel))
, _version(AddVersionLabel(_footer)) {
	setAttribute(Qt::WA_OpaquePaintEvent);

	setupUserpicButton();
	setupAccountsToggle();
	setupAccounts();
	setupArchive();
	setupMenu();

	const auto shadow = Ui::CreateChild<Ui::PlainShadow>(this);
	widthValue(
	) | rpl::on_next([=](int width) {
		const auto line = st::lineWidth;
		shadow->setGeometry(0, st::mainMenuCoverHeight - line, width, line);
	}, shadow->lifetime());

	_nightThemeSwitch.setCallback([this] {
		Expects(_nightThemeToggle != nullptr);

		const auto nightMode = Window::Theme::IsNightMode();
		if (_nightThemeToggle->toggled() != nightMode) {
			Window::Theme::ToggleNightMode();
			Window::Theme::KeepApplied();
		}
	});

	_footer->heightValue(
	) | rpl::on_next([=] {
		_telegram->moveToLeft(st::mainMenuFooterLeft, _footer->height() - st::mainMenuTelegramBottom - _telegram->height());
		_version->moveToLeft(st::mainMenuFooterLeft, _footer->height() - st::mainMenuVersionBottom - _version->height());
	}, _footer->lifetime());

	rpl::combine(
		heightValue(),
		_inner->heightValue()
	) | rpl::on_next([=] {
		updateInnerControlsGeometry();
	}, _inner->lifetime());

	parentResized();

	_telegram->setMarkedText(tr::link(
		u"AccountBank"_q,
		u"https://github.com/AffPapa/AccountBank"_q));
	_telegram->setLinksTrusted();
	// The canary version is too long for the "Version {version}" form.
	_version->setMarkedText(
		tr::link(
			Core::BuildIsCanary
				? currentVersionShortText()
				: tr::lng_settings_current_version(
					tr::now,
					lt_version,
					currentVersionShortText()),
			1) // Link 1.
		.append(QChar(' '))
		.append(QChar(8211))
		.append(QChar(' '))
		.append(tr::link(tr::lng_menu_about(tr::now), 2))); // Link 2.
	_version->setLink(
		1,
		std::make_shared<UrlClickHandler>(
			u"https://github.com/AffPapa/AccountBank"_q));
	_version->setLink(
		2,
		std::make_shared<LambdaClickHandler>([=] {
			controller->show(Box(AboutBox));
		}));

	_controller->session().downloaderTaskFinished(
	) | rpl::on_next([=] {
		update();
	}, lifetime());

	initResetScaleButton();

	setupSwipe();
}

MainMenu::~MainMenu() = default;

void MainMenu::setupArchive() {
	using namespace Settings;

	const auto controller = _controller;
	const auto folder = [=] {
		return controller->session().data().folderLoaded(Data::Folder::kId);
	};
	const auto showArchive = [=](Qt::KeyboardModifiers modifiers) {
		if (const auto f = folder()) {
			if (modifiers & Qt::ControlModifier) {
				controller->showInNewWindow(Window::SeparateId(
					Window::SeparateType::Archive,
					&controller->session()));
			} else {
				controller->openFolder(f);
			}
			controller->window().hideSettingsAndLayer();
			controller->removeLayerBlackout();
		}
	};
	const auto checkArchive = [=] {
		const auto f = folder();
		return f
			&& (!f->chatsList()->empty() || f->storiesCount() > 0)
			&& controller->session().settings().archiveInMainMenu();
	};

	const auto wrap = _menu->add(
		object_ptr<Ui::SlideWrap<Ui::VerticalLayout>>(
			_menu,
			object_ptr<Ui::VerticalLayout>(_menu)));
	const auto inner = wrap->entity();
	wrap->toggle(checkArchive(), anim::type::instant);

	const auto button = AddButtonWithIcon(
		inner,
		tr::lng_archived_name(),
		st::mainMenuButton,
		{ &st::menuIconArchiveOpen });
	inner->add(
		object_ptr<Ui::PlainShadow>(inner),
		{ 0, st::mainMenuSkip, 0, st::mainMenuSkip });
	button->setAcceptBoth(true);
	button->clicks(
	) | rpl::on_next([=](Qt::MouseButton which) {
		if (which == Qt::LeftButton) {
			showArchive(button->clickModifiers());
			return;
		} else if (which != Qt::RightButton) {
			return;
		}
		_contextMenu = base::make_unique_q<Ui::PopupMenu>(
			this,
			st::popupMenuExpandedSeparator);
		Window::FillDialogsEntryMenu(
			_controller,
			Dialogs::EntryState{
				.key = folder(),
				.section = Dialogs::EntryState::Section::ContextMenu,
			},
			Ui::Menu::CreateAddActionCallback(_contextMenu));
		_contextMenu->popup(QCursor::pos());
	}, button->lifetime());

	const auto now = folder();
	auto folderValue = now
		? (rpl::single(now) | rpl::type_erased)
		: controller->session().data().chatsListChanges(
		) | rpl::filter([](Data::Folder *folder) {
			return folder && (folder->id() == Data::Folder::kId);
		}) | rpl::take(1);

	using namespace Settings;
	Badge::AddUnread(button, rpl::single(rpl::empty) | rpl::then(std::move(
		folderValue
	) | rpl::map([=](not_null<Data::Folder*> folder) {
		return folder->owner().chatsList(folder)->unreadStateChanges();
	}) | rpl::flatten_latest() | rpl::to_empty) | rpl::map([=] {
		const auto loaded = folder();
		const auto state = loaded
			? loaded->chatListBadgesState()
			: Dialogs::BadgesState();
		return Badge::UnreadBadge{ state.unreadCounter, true };
	}));

	rpl::merge(
		controller->session().data().chatsListChanges(
		) | rpl::filter([](Data::Folder *folder) {
			return folder && (folder->id() == Data::Folder::kId);
		}) | rpl::to_empty,
		controller->session().data().stories().sourcesChanged(
			Data::StorySourcesList::Hidden
		)
	) | rpl::on_next([=] {
		const auto isArchiveVisible = checkArchive();
		wrap->toggle(isArchiveVisible, anim::type::normal);
		if (!isArchiveVisible) {
			_contextMenu = nullptr;
		}
		update();
	}, lifetime());
}

void MainMenu::setupUserpicButton() {
	_userpicButton->setClickedCallback([=] { toggleAccounts(); });
	_userpicButton->show();
}

void MainMenu::toggleAccounts() {
	auto &settings = Core::App().settings();
	const auto shown = !settings.mainMenuAccountsShown();
	settings.setMainMenuAccountsShown(shown);
	Core::App().saveSettingsDelayed();
}

void MainMenu::setupAccounts() {
	const auto inner = _accounts->entity();

	inner->add(object_ptr<Ui::FixedHeightWidget>(inner, st::mainMenuSkip));
	auto events = Settings::SetupAccounts(inner, _controller);
	inner->add(object_ptr<Ui::FixedHeightWidget>(inner, st::mainMenuSkip));

	std::move(
		events.closeRequests
	) | rpl::on_next([=] {
		closeLayer();
	}, inner->lifetime());

	_accounts->toggleOn(Core::App().settings().mainMenuAccountsShownValue());
	_accounts->finishAnimating();

	_shadow->setDuration(0)->toggleOn(_accounts->shownValue());
}

void MainMenu::setupAccountsToggle() {
	_toggleAccounts->show();
	_toggleAccounts->setAcceptBoth();
	_toggleAccounts->addClickHandler([=](Qt::MouseButton button) {
		if (button == Qt::LeftButton) {
			toggleAccounts();
		}
	});
}

void MainMenu::parentResized() {
	resize(st::mainMenuWidth, parentWidget()->height());
}

void MainMenu::showFinished() {
	_controller->checkHighlightControl(
		u"main-menu/night-mode"_q,
		_nightThemeToggle);
}

void MainMenu::setupMenu() {
	using namespace Settings;

	const auto controller = _controller;
	const auto addAction = [&](
			rpl::producer<QString> text,
			IconDescriptor &&descriptor) {
		return AddButtonWithIcon(
			_menu,
			std::move(text),
			st::mainMenuButton,
			std::move(descriptor));
	};
	_menu->add(
		CreateButtonWithIcon(
			_menu,
			tr::lng_menu_my_profile(),
			st::mainMenuButton,
			{ &st::menuIconProfile })
	)->setClickedCallback([=] {
		controller->showPeerInfo(controller->session().user());
	});

	addAction(
		tr::lng_saved_messages(),
		{ &st::menuIconSavedMessages }
	)->setClickedCallback([=] {
		controller->showPeerHistory(controller->session().user());
	});
	addAction(
		tr::lng_menu_settings(),
		{ &st::menuIconSettings }
	)->setClickedCallback([=] {
		controller->showSettings();
	});

	_nightThemeToggle = addAction(
		tr::lng_menu_night_mode(),
		{ &st::menuIconNightMode }
	)->toggleOn(_nightThemeSwitches.events_starting_with(
		Window::Theme::IsNightMode()
	));
	_nightThemeToggle->toggledChanges(
	) | rpl::filter([=](bool night) {
		return (night != Window::Theme::IsNightMode());
	}) | rpl::on_next([=](bool night) {
		if (Window::Theme::Background()->editingTheme()) {
			_nightThemeSwitches.fire(!night);
			controller->show(Ui::MakeInformBox(
				tr::lng_theme_editor_cant_change_theme()));
			return;
		}
		const auto weak = base::make_weak(this);
		const auto toggle = [=] {
			if (!weak) {
				Window::Theme::ToggleNightMode();
				Window::Theme::KeepApplied();
			} else {
				_nightThemeSwitch.callOnce(st::mainMenu.itemToggle.duration);
			}
		};
		Window::Theme::ToggleNightModeWithConfirmation(
			&_controller->window(),
			toggle);
	}, _nightThemeToggle->lifetime());

	Core::App().settings().systemDarkModeValue(
	) | rpl::on_next([=](std::optional<bool> darkMode) {
		const auto darkModeEnabled
			= Core::App().settings().systemDarkModeEnabled();
		if (darkModeEnabled && darkMode.has_value()) {
			_nightThemeSwitches.fire_copy(*darkMode);
		}
	}, _nightThemeToggle->lifetime());
}

void MainMenu::resizeEvent(QResizeEvent *e) {
	_inner->resizeToWidth(width());
	updateControlsGeometry();
}

void MainMenu::updateControlsGeometry() {
	_userpicButton->moveToLeft(
		st::mainMenuUserpicLeft,
		st::mainMenuUserpicTop);
	if (_resetScaleButton) {
		_resetScaleButton->moveToRight(0, 0);
	}
	_toggleAccounts->setGeometry(
		0,
		st::mainMenuCoverNameTop,
		width(),
		st::mainMenuCoverHeight - st::mainMenuCoverNameTop);
	// Allow cover shadow over the scrolled content.
	const auto top = st::mainMenuCoverHeight - st::lineWidth;
	_scroll->setGeometry(0, top, width(), height() - top);
	updateInnerControlsGeometry();
}

void MainMenu::updateInnerControlsGeometry() {
	const auto contentHeight = _accounts->height()
		+ _shadow->height()
		+ st::mainMenuSkip
		+ _menu->height();
	const auto available = height() - st::mainMenuCoverHeight - contentHeight;
	const auto footerHeight = std::max(
		available,
		st::mainMenuFooterHeightMin);
	if (_footer->height() != footerHeight) {
		_footer->resize(_footer->width(), footerHeight);
	}
}

bool MainMenu::eventHook(QEvent *event) {
	const auto type = event->type();
	if (type == QEvent::TouchBegin
		|| type == QEvent::TouchUpdate
		|| type == QEvent::TouchEnd
		|| type == QEvent::TouchCancel) {
		QGuiApplication::sendEvent(_inner, event);
	}
	return RpWidget::eventHook(event);
}

void MainMenu::paintEvent(QPaintEvent *e) {
	auto p = Painter(this);
	const auto clip = e->rect();
	const auto cover = QRect(0, 0, width(), st::mainMenuCoverHeight);

	p.fillRect(clip, st::mainMenuBg);
	if (cover.intersects(clip)) {
		drawName(p);
	}
}

void MainMenu::drawName(Painter &p) {
	const auto widthText = width()
		- st::mainMenuCoverNameLeft
		- _toggleAccounts->rightSkip();

	const auto user = _controller->session().user();
	if (_nameVersion < user->nameVersion()) {
		_nameVersion = user->nameVersion();
		_name.setText(
			st::semiboldTextStyle,
			user->name(),
			Ui::NameTextOptions());
	}
	p.setFont(st::semiboldFont);
	p.setPen(st::windowBoldFg);
	_name.drawLeftElided(
		p,
		st::mainMenuCoverNameLeft,
		st::mainMenuCoverNameTop,
		widthText,
		width());
}

void MainMenu::initResetScaleButton() {
	_controller->widget()->screenValue(
	) | rpl::map([](not_null<QScreen*> screen) {
		return rpl::single(
			screen->availableGeometry()
		) | rpl::then(
			base::qt_signal_producer(
				screen.get(),
				&QScreen::availableGeometryChanged
			)
		);
	}) | rpl::flatten_latest(
	) | rpl::map([](QRect available) {
		return (available.width() >= st::windowMinWidth)
			&& (available.height() >= st::windowMinHeight);
	}) | rpl::distinct_until_changed(
	) | rpl::on_next([=](bool good) {
		if (good) {
			_resetScaleButton.destroy();
		} else {
			_resetScaleButton.create(this);
			_resetScaleButton->addClickHandler([] {
				cSetConfigScale(style::kScaleDefault);
				Local::writeSettings();
				Core::Restart();
			});
			_resetScaleButton->show();
			updateControlsGeometry();
		}
	}, lifetime());
}

OthersUnreadState OtherAccountsUnreadStateCurrent(
		not_null<Main::Account*> current) {
	const auto &domain = Core::App().domain();
	auto counter = 0;
	auto allMuted = true;
	for (const auto &[index, account] : domain.accounts()) {
		if (account.get() == current) {
			continue;
		} else if (const auto session = account->maybeSession()) {
			counter += session->data().unreadWithMentionsBadge();
			if (!session->data().unreadWithMentionsBadgeMuted()) {
				allMuted = false;
			}
		}
	}
	return {
		.count = counter,
		.allMuted = allMuted,
	};
}

rpl::producer<OthersUnreadState> OtherAccountsUnreadState(
		not_null<Main::Account*> current) {
	return rpl::single(rpl::empty) | rpl::then(
		Core::App().unreadBadgeChanges()
	) | rpl::map([=] {
		return OtherAccountsUnreadStateCurrent(current);
	});
}

base::EventFilterResult MainMenu::redirectToInnerChecked(not_null<QEvent*> e) {
	if (_insideEventRedirect) {
		return base::EventFilterResult::Continue;
	}
	const auto weak = base::make_weak(this);
	_insideEventRedirect = true;
	QGuiApplication::sendEvent(_inner, e);
	if (weak) {
		_insideEventRedirect = false;
	}
	return base::EventFilterResult::Cancel;
}

void MainMenu::setupSwipe() {
	const auto outer = _controller->widget()->body();
	base::install_event_filter(this, outer, [=](not_null<QEvent*> e) {
		const auto type = e->type();
		if (type == QEvent::TouchBegin
			|| type == QEvent::TouchUpdate
			|| type == QEvent::TouchEnd
			|| type == QEvent::TouchCancel) {
			return redirectToInnerChecked(e);
		} else if (type == QEvent::Wheel) {
			const auto w = static_cast<QWheelEvent*>(e.get());
			const auto d = Ui::ScrollDeltaF(w);
			if (std::abs(d.x()) > std::abs(d.y())) {
				return redirectToInnerChecked(e);
			}
		}
		return base::EventFilterResult::Continue;
	});
	const auto handles = outer->testAttribute(Qt::WA_AcceptTouchEvents);
	if (!handles) {
		outer->setAttribute(Qt::WA_AcceptTouchEvents);
		lifetime().add([=] {
			outer->setAttribute(Qt::WA_AcceptTouchEvents, false);
		});
	}

	auto update = [=](Ui::Controls::SwipeContextData data) {
		if (data.translation < 0) {
			if (!_swipeBackData.callback) {
				_swipeBackData = Ui::Controls::SetupSwipeBack(
					this,
					[=]() -> std::pair<QColor, QColor> {
						return {
							st::historyForwardChooseBg->c,
							st::historyForwardChooseFg->c,
						};
					});
			}
			_swipeBackData.callback(data);
			return;
		} else if (_swipeBackData.lifetime) {
			_swipeBackData = {};
		}
	};

	auto init = [=](Ui::Controls::SwipeHandlerInitData data) {
		if (data.direction != Qt::LeftToRight) {
			return Ui::Controls::SwipeHandlerFinishData();
		}
		return Ui::Controls::DefaultSwipeBackHandlerFinishData([=] {
			closeLayer();
		});
	};

	Ui::Controls::SetupSwipeHandler({
		.widget = _inner,
		.scroll = _scroll.data(),
		.update = std::move(update),
		.init = std::move(init),
	});
}

} // namespace Window
