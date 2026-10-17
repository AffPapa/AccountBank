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
#include <QJsonObject>

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
	struct RecentResult {
		std::vector<QJsonObject> rows;
		bool ok = false;
	};

	explicit MessageArchive(not_null<Session*> owner);
	~MessageArchive();

	void logMessage(not_null<HistoryItem*> item);
	void logEditTransition(
		HistoryItem *existing,
		const QString &peer,
		const QString &msg,
		const QString &afterText,
		const QString &afterPhoto,
		const QString &afterDocument,
		bool nonChannel);
	void logDeleted(not_null<HistoryItem*> item);
	void logDeletedBatch(const std::vector<not_null<HistoryItem*>> &items);
	void logDeletedMissing(
		const QString &peer,
		const std::vector<int> &ids);
	void loadRecent(
		const QString &kind,
		const QString &peer,
		Fn<void(RecentResult)> done);
	void logRead(not_null<History*> history, MsgId upTo);
	void logPresence(not_null<PeerData*> peer);
	void logSavedMedia(
		const QString &kind,
		const QString &path,
		const QString &peer,
		const QString &msg,
		const QString &senderName,
		const QString &peerName);
	[[nodiscard]] bool exportTo(const QString &path);
	void exportAsync(const QString &path, Fn<void(bool)> done);
	[[nodiscard]] bool exportInProgress() const { return _exportActive; }
	[[nodiscard]] bool atCapacity() const { return _full; }
	[[nodiscard]] uint64 cloudChannelId() const { return _cloudChannelId; }
	[[nodiscard]] uint64 cloudAccessHash() const { return _cloudAccessHash; }
	[[nodiscard]] bool cloudMirrorUnavailable() const { return _mirrorDisabled; }
	[[nodiscard]] bool ensureCloudChannel();

private:
	struct Pending {
		QByteArray key;
		QByteArray nonce;
		QByteArray tag;
		QByteArray data;
		qint64 ts = 0;
		bool cloudMirror = false;
		QByteArray stateKey;
		QByteArray lookupKey;
		bool stateReplace = false;
	};

	void enqueue(
		const QString &kind,
		const QString &peer,
		const QString &msg,
		const QJsonObject &object,
		bool cloudMirror = false);
	bool flush();
	[[nodiscard]] bool ensureOpened();
	[[nodiscard]] QJsonObject currentSnapshot(
		const QString &peer,
		const QString &msg);
	void processCloudMirror();
	void createCloudChannel();
	void sendCloudSnapshot();
	void markCloudSent(const QByteArray &key);
	[[nodiscard]] bool saveCloudChannel(uint64 id, uint64 accessHash);
	void loadCloudChannel();

	Session *_owner = nullptr;
	sqlite3 *_db = nullptr;
	QByteArray _aesKey;
	QByteArray _macKey;
	std::vector<Pending> _pending;
	base::Timer _flushTimer;
	base::Timer _mirrorTimer;
	rpl::lifetime _lifetime;
	std::shared_ptr<bool> _alive = std::make_shared<bool>(true);
	qint64 _rows = 0;
	qint64 _eventCounter = 0;
	uint64 _cloudChannelId = 0;
	uint64 _cloudAccessHash = 0;
	bool _mirrorRequestActive = false;
	bool _mirrorDisabled = false;
	bool _broken = false;
	bool _full = false;
	bool _exportActive = false;

	static constexpr auto kMaxRows = qint64(2'000'000);
	static constexpr auto kMaxPending = 10'000;
};

} // namespace Data
