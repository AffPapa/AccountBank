/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include "base/object_ptr.h"

namespace Window {

class SessionController;

void ShowAccountBankArchiveHome(not_null<SessionController*> controller);
void ShowAccountBankArchive(
	not_null<SessionController*> controller,
	const QString &kind);
void ShowAccountBankAdminChats(not_null<SessionController*> controller);
void ShowAccountBankMessageFilterBox(
	not_null<SessionController*> controller);
void ExportAccountBankArchive(not_null<SessionController*> controller);
void OpenAccountBankCloudForum(
	not_null<SessionController*> controller,
	bool legacy = false);

} // namespace Window
