/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "chat_helpers/text_replacements.h"

#include "core/application.h"
#include "core/core_settings.h"
#include "lang/lang_keys.h"
#include "ui/layers/generic_box.h"
#include "ui/widgets/fields/input_field.h"
#include "ui/widgets/labels.h"
#include "styles/style_chat_helpers.h"
#include "styles/style_layers.h"

namespace ChatHelpers {
namespace {

struct ReplacementRule {
	QString shortcut;
	QString replacement;
};

[[nodiscard]] std::vector<ReplacementRule> ParseRules(
		const QByteArray &serialized) {
	auto unique = std::map<QString, QString>();
	for (const auto &line : serialized.split('\n')) {
		const auto equal = line.indexOf('=');
		if (equal <= 0) {
			continue;
		}
		unique.insert_or_assign(
			QString::fromUtf8(line.left(equal)).trimmed(),
			QString::fromUtf8(line.mid(equal + 1)).trimmed());
	}
	auto result = std::vector<ReplacementRule>();
	result.reserve(unique.size());
	for (const auto &[shortcut, replacement] : unique) {
		if (!shortcut.isEmpty()) {
			result.push_back({ shortcut, replacement });
		}
	}
	std::stable_sort(result.begin(), result.end(), [](
			const ReplacementRule &a,
			const ReplacementRule &b) {
		return (a.shortcut.size() > b.shortcut.size());
	});
	return result;
}

[[nodiscard]] bool AllowedBefore(QChar previous) {
	return !previous.isLetterOrNumber()
		&& (previous != u'_')
		&& (previous != u'@')
		&& (previous != u'#')
		&& (previous != u'/');
}

[[nodiscard]] bool AllowedAfter(QChar next) {
	return !next.isLetterOrNumber() && (next != u'_') && (next != u'@');
}

[[nodiscard]] bool InsideLinkOrMention(const QString &text, int position) {
	auto start = position;
	while (start > 0 && !text[start - 1].isSpace()) {
		--start;
	}
	auto end = start;
	while (end < text.size() && !text[end].isSpace()) {
		++end;
	}
	const auto token = text.mid(start, end - start).toLower();
	return token.contains(u"t.me"_q) || token.contains(u"://"_q);
}

[[nodiscard]] bool MatchesAt(
		const QString &text,
		QStringView view,
		int position,
		const ReplacementRule &rule) {
	const auto length = int(rule.shortcut.size());
	if ((text.size() - position) < length
		|| view.mid(position, length) != QStringView(rule.shortcut)) {
		return false;
	}
	if ((position > 0 && !AllowedBefore(text[position - 1]))
		|| ((position + length) < text.size()
			&& !AllowedAfter(text[position + length]))) {
		return false;
	}
	return !InsideLinkOrMention(text, position);
}

[[nodiscard]] std::optional<QString> ApplyRules(
		const QString &text,
		const std::vector<ReplacementRule> &rules) {
	const auto view = QStringView(text);
	auto result = QString();
	result.reserve(text.size());
	auto changed = false;
	auto position = 0;
	while (position < text.size()) {
		const auto rule = ranges::find_if(rules, [&](const ReplacementRule &r) {
			return MatchesAt(text, view, position, r);
		});
		if (rule == rules.end()) {
			result.append(text[position]);
			++position;
			continue;
		}
		result.append(rule->replacement);
		position += int(rule->shortcut.size());
		changed = true;
	}
	return changed ? std::optional<QString>(std::move(result)) : std::nullopt;
}

} // namespace

TextWithEntities ApplyTextReplacements(TextWithEntities text) {
	const auto rules = ParseRules(Core::App().settings().textReplacements());
	if (rules.empty() || text.text.isEmpty()) {
		return text;
	}
	const auto replaced = ApplyRules(text.text, rules);
	if (!replaced) {
		return text;
	}
	return TextWithEntities{ .text = std::move(*replaced) };
}

void ShowTextReplacementsBox(not_null<Ui::GenericBox*> box) {
	box->setTitle(tr::lng_accountbank_text_replacements_title());

	box->addRow(
		object_ptr<Ui::FlatLabel>(
			box,
			tr::lng_accountbank_text_replacements_about(),
			st::boxLabel),
		st::boxRowPadding);

	const auto field = box->addRow(object_ptr<Ui::InputField>(
		box.get(),
		st::textReplacementsField,
		Ui::InputField::Mode::MultiLine,
		rpl::producer<QString>(),
		TextWithTags{ QString::fromUtf8(
			Core::App().settings().textReplacements()) }));
	field->setSubmitSettings(Ui::InputField::SubmitSettings::CtrlEnter);
	box->setFocusCallback([=] {
		field->setFocusFast();
	});

	const auto save = [=] {
		Core::App().settings().setTextReplacements(
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

} // namespace ChatHelpers
