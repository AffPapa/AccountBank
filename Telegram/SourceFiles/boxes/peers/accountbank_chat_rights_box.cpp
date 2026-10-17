/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "boxes/peers/accountbank_chat_rights_box.h"

#include "boxes/peers/edit_peer_permissions_box.h"
#include "data/data_chat.h"
#include "data/data_channel.h"
#include "data/data_peer.h"
#include "lang/lang_keys.h"
#include "ui/layers/generic_box.h"
#include "ui/text/text_utilities.h"
#include "ui/vertical_list.h"
#include "ui/widgets/labels.h"
#include "ui/wrap/table_layout.h"
#include "styles/style_boxes.h"
#include "styles/style_giveaway.h"
#include "styles/style_layers.h"

namespace {

using TableRow = std::pair<QString, bool>;

void AddTableRows(
		not_null<Ui::TableLayout*> table,
		const std::vector<TableRow> &rows) {
	for (const auto &[label, allowed] : rows) {
		table->addRow(
			object_ptr<Ui::FlatLabel>(
				table,
				label,
				st::defaultTable.defaultLabel),
			object_ptr<Ui::FlatLabel>(
				table,
				(allowed
					? tr::lng_box_yes()
					: tr::lng_box_no()) | rpl::map(Ui::Text::Bold),
				st::defaultTable.defaultValue),
			st::giveawayGiftCodeLabelMargin,
			st::giveawayGiftCodeValueMargin);
	}
}

not_null<Ui::TableLayout*> AddRightsTable(
		not_null<Ui::VerticalLayout*> layout) {
	return layout->add(
		object_ptr<Ui::TableLayout>(layout, st::defaultTable));
}

} // namespace

void AccountBankChatRightsBox(
		not_null<Ui::GenericBox*> box,
		not_null<PeerData*> peer) {
	Expects(peer->isChat() || peer->isChannel());

	const auto chat = peer->asChat();
	const auto channel = peer->asChannel();

	box->setTitle(tr::lng_accountbank_chat_rights());
	box->setWidth(st::boxWideWidth);
	box->addButton(tr::lng_close(), [=] { box->closeBox(); });

	const auto layout = box->addRow(
		object_ptr<Ui::VerticalLayout>(box),
		st::boxRowPadding);

	const auto restrictions = chat
		? chat->defaultRestrictions()
		: channel->defaultRestrictions();
	auto memberRows = std::vector<TableRow>();
	for (const auto &entry : RestrictionLabels({ .isForum = peer->isForum() })) {
		memberRows.emplace_back(
			entry.label,
			!(restrictions & entry.flags));
	}
	if (!memberRows.empty()) {
		Ui::AddSubsectionTitle(layout, tr::lng_manage_peer_permissions());
		AddTableRows(AddRightsTable(layout), memberRows);
	}

	Ui::AddSkip(layout);
	Ui::AddDivider(layout);
	Ui::AddSkip(layout);

	const auto adminRights = chat
		? chat->adminRights()
		: channel->adminRights();
	auto myRows = std::vector<TableRow>();
	myRows.emplace_back(
		tr::lng_channel_admin_status_creator(tr::now),
		chat ? chat->amCreator() : channel->amCreator());
	const auto isGroup = chat || channel->isMegagroup();
	for (const auto &entry : AdminRightLabels(Data::AdminRightsSetOptions{
		.isGroup = isGroup,
		.isForum = peer->isForum(),
		.isCommunity = (channel && channel->isCommunity()),
		.anyoneCanAddMembers = chat
			? chat->anyoneCanAddMembers()
			: channel->anyoneCanAddMembers(),
		.canProcessJoinRequests = peer->isMegagroup(),
	})) {
		myRows.emplace_back(entry.label, (adminRights & entry.flags) != 0);
	}
	Ui::AddSubsectionTitle(layout, tr::lng_rights_edit_admin());
	AddTableRows(AddRightsTable(layout), myRows);
}
