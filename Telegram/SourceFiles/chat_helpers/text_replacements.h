/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include "ui/text/text_entity.h"

namespace Ui {
class GenericBox;
} // namespace Ui

namespace ChatHelpers {

[[nodiscard]] TextWithEntities ApplyTextReplacements(
	TextWithEntities text);

void ShowTextReplacementsBox(not_null<Ui::GenericBox*> box);

} // namespace ChatHelpers
