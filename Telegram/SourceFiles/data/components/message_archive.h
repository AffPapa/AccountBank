/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include "base/basic_types.h"
#include "base/timer.h"
#include "data/data_msg_id.h"
#include "rpl/lifetime.h"

struct sqlite3;

class History;
class HistoryItem;
class PeerData;

namespace Main {
class Session;
} // namespace Main

namespace Data {

class Session;

class MessageArchive final {
public:
	explicit MessageArchive(not_null<Session*> owner);
	~MessageArchive();

	void logMessage(not_null<HistoryItem*> item);
	void logEditSnapshot(not_null<HistoryItem*> item);
	void logDeleted(not_null<HistoryItem*> item);
	void logRead(not_null<History*> history, MsgId upTo);
	void logPresence(not_null<PeerData*> peer);
	[[nodiscard]] bool exportTo(const QString &path);

private:
	struct Pending {
		QByteArray key;
		QByteArray nonce;
		QByteArray tag;
		QByteArray data;
		qint64 ts = 0;
	};

	void enqueue(
		const QString &kind,
		const QString &peer,
		const QString &msg,
		const QJsonObject &object);
	void flush();
	[[nodiscard]] bool ensureOpened();

	Session *_owner = nullptr;
	sqlite3 *_db = nullptr;
	QByteArray _aesKey;
	QByteArray _macKey;
	std::vector<Pending> _pending;
	base::Timer _flushTimer;
	rpl::lifetime _lifetime;
	qint64 _rows = 0;
	bool _broken = false;
	bool _full = false;

	static constexpr auto kMaxRows = qint64(2'000'000);
	static constexpr auto kMaxPending = 10'000;
};

} // namespace Data
