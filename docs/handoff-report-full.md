# AccountBank: полный отчёт о реализованных функциях (передача в соседний чат)

Этот документ — полная карта работы, сделанной «старшим» ИИ в параллельном чате
(модель по токенам). Цель передачи: другая ИИ (по подписке) дорабатывает
внешний вид, полирует UI и продолжает волны. Своя работа соседнего чата
(облачный форум-архив v27–v33, просмотр архива v30+, секунды и т.д.) здесь НЕ
переописывается — она и так известна соседу.

Все коммиты в ветке `accountbank/bootstrap`. Сборка: `out-size-oz` Release.
Проверка запуска: бинарь с временным `-workdir`. Упаковка:
`ACCOUNTBANK_BUILD_TREE=out-size-oz tools/package-test-release.command v40` (v39 занята).

## 1. Мои коммиты (хронологически)

| Коммит | Что |
|---|---|
| `785b3e4` | Автосейв view-once/TTL фото при просмотре |
| `d031e1f` | Усиление автосейва + тост с полным путём и открытием в Finder |
| `8305d44` | Автосейв всех просмотренных сторис (фото+видео) |
| `1734f25` | Роадмап нестандартных фич (docs/unofficial-features-roadmap.md) |
| (v14/v15, без коммита — правки лаунчера) | Единый профиль `~/Library/Application Support/AccountBank`, миграция, архив старых копий |
| `73750bd` | Чекпойнт: ghost mode, скрытие спонсорских, view-once видео/войс, last seen в списке, streamer mode, F13 |
| `4ec0c1e` | Исследование фич альтернативных клиентов (docs/alternative-clients-features-list.md) |
| `6771ddc` | Объединённый план C/[Я] (docs/merged-features-plan.md) |
| `ac350e5` | План волн (docs/development-waves-plan.md) |
| `4d6e6fb` | Волна 1 (12 функций) |
| `5d3c70f` | Волна 2 (автозамены, админ-папка, матрица прав, peek, фильтры) |
| `372f989` | QA-чеклист волн 1–2 (docs/qa-checklist-v38-v39.md) |

Ранние коммиты (v14–v25 эпоха) могли перебазироваться — сверять по git log/messages.

## 2. Ядро: автосейв сгорающего контента

**Файл:** `Telegram/SourceFiles/media/view/media_view_overlay_widget.{h,cpp}`.

- `maybeAutoSaveTtlPhoto()` — view-once/TTL фото. Условия: `_photo`, без видео,
  `_photoMedia->loaded()`, `_message->media()->ttlSeconds() > 0`,
  `media->photo() == _photo`. Путь: `<загрузки>/ttl/ttl_photo_<PhotoId>.jpg`
  (база — настроенный downloadPath, при ask/Temp/пустом — `File::DefaultDownloadPath`).
  Запись `_photoMedia->saveToFile`; дедуп `QFileInfo(toName).size() > 0`; при ошибке
  файл удаляется и пишется LOG. Тост: `showSaveMsgToastWith` с ключом
  `lng_mediaview_ttl_saved_to` (полный путь — кликабельная ссылка
  `internal:show_saved_message` → `File::ShowInFolder` → Finder), 5 с
  (`kTtlSavedToastDuration`). Вызовы: конец `displayPhoto()`, начало
  `checkForSaveLoaded()`, `clearBeforeHide()` (ДО `checkSingleViewMediaBurn()`),
  отдельная подписка на `downloaderTaskFinished` БЕЗ `isHidden`-гарда.
- `maybeAutoSaveTtlDocument()` — view-once ВИДЕО: `<ttl>/ttl_doc_<DocumentId>.<ext>`;
  если файл уже локален — копия из location, иначе `_document->save(fileOrigin(), toName)`
  (фоновая докачка прямо в целевой файл, живёт на DocumentData — переживает закрытие вьюера).
- `maybeAutoSaveShownStory()` — просмотренные СТОРИС (фото и видео, включая свои):
  `<загрузки>/stories/story_<peerId>_<storyId>.<ext>`; фото — `saveToFile`,
  видео — `document->save(_stories->fileOrigin(), toName)`. Пропуск
  `unsupported()/call()`. Вызовы: `displayPhoto`, `displayDocument`,
  `clearBeforeHide`, та же подписка.
- **TTL войс:** `SaveTtlVoice()` в
  `Telegram/SourceFiles/history/view/media/history_view_document.cpp` — хук в обеих
  ветках `TTLVoiceStops` (контекст `Context::TTLViewer` и обычный пузырь), ДО
  `clearMediaAsExpired()`. Путь `<ttl>/ttl_voice_<DocumentId>.ogg`; байты из
  `media->bytes()`, при пустых — `data->save(origin, toName)`.

## 3. Ghost mode (pref `ghost-mode`, по умолчанию ВКЛ)

Точки глушения СЕТИ при включённом префе (локальные пометки остаются):
- `data/data_histories.cpp` `Histories::sendReadRequest` — не отправляет
  readHistory; state помечается done локально (unread-счётчики у себя гаснут).
- `apiwrap.cpp` `ApiWrap::markContentsRead` (оба оверлоада) — `item->markContentsRead(true)`
  выполняется (view-once сгорает локально), MTP-запросы не уходят.
- `data/data_stories.cpp` `Stories::markAsRead` — `bumpReadTill` локально, без
  `MTPstories_ReadStories`/`IncrementStoryViews`.
- `api/api_updates.cpp` `Updates::updateOnline` — `reported = isOnline && !ghost`,
  серверу всегда шлётся offline.
- `api/api_send_progress.cpp` `SendProgressManager::send` — typing/record/upload
  глушатся, КРОМЕ `Speaking` (групповые звонки).
- Самоисцеление unread: `History::applyInboxReadUpdate` игнорирует серверные
  отставания, когда локальный read-till выше (`_inboxReadBefore <= upTo`).

## 4. Скрытие спонсорских (pref `hide-sponsored`, ВКЛ)

`data/components/sponsored_messages.cpp`: ранний return в `request()` и
`requestForVideo()` (поиск-спонсорские в `api_peer_search` не тронуты).

## 5. Anti-recall: удалённые остаются в чате (pref `keep-deleted`, ВКЛ)

- `data/data_session.cpp` `processMessagesDeleted` + `processNonChannelMessagesDeleted`:
  ВСЕГДА `_archive->logDeleted(item)` (снимок в зашифрованный архив); при префе —
  `item->markServerDeleted()` вместо `destroy()`, иначе штатное уничтожение.
- `history/history_item.{h,cpp}`: `markServerDeleted()` / `isServerDeleted()` /
  `_serverDeleted`. Реализация: флаг, `setText(маркер "deleted" + '\n' + старый
  текст)` (ключ `lng_accountbank_deleted_marker`), `markContentsRead(false)`,
  `requestItemResize`, `updateChatListEntry`.
- Визуал: `history/view/history_view_message.cpp` `Message::draw` —
  полупрозрачность 0.55 для `isServerDeleted()` через RAII-восстановление
  `p.setOpacity` (`gsl::finally`), независимо от ранних return.
- Повторные пачки удалений идемпотентны (флаг), «призрак» не ломает chat-list.

## 6. Зашифрованный SQLite-архив (локальное ядро банка)

**Файл:** `data/components/message_archive.{h,cpp}` — `Data::MessageArchive`,
владеет `Data::Session` (создаётся в конструкторе, accessor `Session::archive()`).
- БД: `<папка аккаунта>/archive.ab` (новый accessor
  `Storage::Account::accountFolder()` → `_basePath`); системный sqlite3
  (`-lsqlite3` в `Telegram/CMakeLists.txt`); WAL, busy_timeout 5000,
  `records(key BLOB PK, nonce, tag, data, ts) WITHOUT ROWID`.
- Крипто: ключи = HMAC-SHA256 от локального ключа аккаунта
  (`peekLegacyLocalKey()` → 256 байт) с метками `AccountBank archive v1` /
  `…index v1`; записи AES-256-GCM (nonce 12 байт, tag 16) поверх JSON
  (OpenSSL EVP, уже в зависимостях); ключ записи = HMAC-индекс
  `kind:peer:msg[:ts]` (16 байт) — метаданные не plaintext.
- Записи: `message` (дедуп peer:msg), `edit` (снимок ДО applyEdition — хук в
  `Session::updateEditedMessage`), `delete` (до destroy), `read`
  (`History::outboxRead`), `presence` (подписка `peerUpdates(OnlineStatus)`),
  `profile` (подписка `peerUpdates(Name|Username|Photo|PhoneNumber|About)` —
  имя/юзернейм/телефон/about).
- Поток: буфер в памяти, flush транзакцией раз в 2 с или при 10k записей;
  лимит 2M строк; ЛЮБАЯ ошибка → LOG + `_broken` (архив отключается, вход не
  блокируется). Деструктор — flush + close.
- Экспорт: `MessageArchive::exportTo(path)` — SELECT + decrypt → JSON-массив.
  Пункт меню «Export message archive» (window_main_menu.cpp) →
  `~/Downloads/AccountBank/AccountBank archive <userId>.json` + тост.
  ВНИМАНИЕ: соседний чат развил это (`exportAsync`/`exportInProgress`,
  просмотрщик `ShowAccountBankArchive` с категориями, облачный форум-архив) —
  моё ядро остаётся локальным зашифрованным слоем под ними.

## 7. Волна 1 (`4d6e6fb`)

- **Компактный список** (pref `compact-chat-list`): стиль `compactDialogRow`
  (48px) в `dialogs/dialogs.style`; `DefaultChatListRowSt()` в
  `dialogs_row.{h,cpp}`; применение после перезапуска; тумблер в ☰.
- **Непрочитанные сверху** (pref `unread-first`): `Session::applyUnreadFirstPref()`
  поверх апстримового механизма `_dialogsUnreadOnTop`; мгновенно; тумблер в ☰.
- **Alt+клик превью чата**: существующий путь `showChatPreview` подключён к
  Alt в `dialogs_inner_widget.cpp` (прочтения не происходит).
- **Локальные закрепы сверх лимита** (pref `local-pinned`, QByteArray пар
  userId+peerId): `Entry::isLocalPinnedDialog/LocalPinnedStorage`,
  `LocalPinnedDialogPos` (ниже серверных, выше unread-top);
  `window_peer_menu.cpp` `PinnedLimitReached` → локальный закреп + тост;
  иконка через `PinnedIconShown` (dialogs_layout).
- **Копирование callback-данных ботов**: `AddBotButtonCopyAction`
  (context_menu + classic menu) — title/url/callback_data кнопки в буфер.
- **Повтор сообщения**: `AddRepeatMessageAction` — текст в поле ввода, медиа —
  форвард в тот же чат/топик (`forwardMessages`, topicRootId сохраняется).
- **Форвард в Избранное**: `FastShareMessagesToSelf` (boxes/share_box.cpp) +
  `AddForwardSavedAction`; скрыт в Saved и noforwards.
- **Выбор 10 000**: `config.h` `MaxSelectedItems` 100 → 10000.
- **Перезапись ссылок при вставке**: `RewritePastedLinks`/`InsertWithRewrittenLinks`
  (chat_helpers/message_field.cpp): x.com/twitter.com → fixupx.com (только
  plain-text paste), подключено в compose_controls и history_widget.
- **Обход noforwards** (pref `allow-copy-restricted`, ВКЛ): три точки —
  `CopyRestrictionTypeFor`/`CopyMediaRestrictionTypeFor`
  (history_view_list_widget.cpp), `HistoryInner::hasCopyRestriction/
  hasCopyMediaRestriction` (history_inner_widget.cpp),
  `OverlayWidget::hasCopyMediaRestriction` (ветка сторис не тронута).
  Копирование/выделение/сохранение в защищённых чатах разблокированы.

## 8. Волна 2 (`5d3c70f`)

- **Локальная вкладка «Admin»**: `data_chat_filters.{h,cpp}` —
  `ChatFilter::Flag::LocalAdmins (1<<13)`, id `kAdminFilterId = 999'999'999`,
  `contains()` = amCreator/hasAdminRights; фильтр дописывается в конец списка,
  `peerUpdates(Admins|Rights)` → `refreshHistory`; исключён из серверного
  синка/порядка/лимитов в 8 файлах (chat_filters_tabs_strip,
  window_filters_menu, choose_filter_box, settings_folders,
  premium_limits_box, api_chat_filters, dialogs_inner_widget).
  Заголовок фиксированный «Admin».
- **Матрица прав**: новые `boxes/peers/accountbank_chat_rights_box.{h,cpp}`
  (Ui::TableLayout, две таблицы: ограничения участников lng_rights_chat_* +
  мои права lng_rights_group_*/lng_rights_channel_*, Yes/No); вход из меню
  шапки чата «Chat rights» (`window_peer_menu.cpp`).
- **Бан из заявки**: `edit_peer_requests_box` — кнопка Ban (`requestsBanButton`)
  → `chatParticipants().kick` + отклонение заявки; только при canBanMembers.
- **Владелец стикерпака**: `AddStickerPackOwnerAction` — только для сетов с
  флагом AmCreator (сервер не отдаёт creator id чужих паков).
- **Текстовые автозамены**: `chat_helpers/text_replacements.{h,cpp}` —
  `ApplyTextReplacements` в ЕДИНОЙ точке `ApiWrap::sendMessage` (все виды чатов
  + отложенные); правила `сокращение=замена` построчно, целые слова, не в
  @/#/ссылках/t.me; при сработке — сообщение уходит plain (entities теряются,
  v1); редактор `ShowTextReplacementsBox` (MultiLine, Ctrl+Enter — Save);
  pref `text-replacements`; стиль `textReplacementsField`.
- **Peek last seen**: `info/profile/info_profile_actions.cpp` — кнопка
  «Показать точный статус» при `user->lastseen().isHidden()`; механика:
  снапшот правил LastSeen → `userPrivacy().save(LastSeen, +user в always)` →
  ожидание `peerUpdates(OnlineStatus)` c `onlineTill > 0` (take(1), таймер
  3 с) → тост с точным временем (`langDateTimeFull`) → `finish()` ВСЕГДА
  восстанавливает сохранённые правила. Использует `Api::UserPrivacy`
  (api_user_privacy). Lang-ключи `lng_accountbank_peek_lastseen*` — русские.
- **Regex-фильтр сообщений** (pref `message-filter`, QByteArray): бокс
  «Message filter» (window_main_menu.cpp, `ShowMessageFilterBox`, поле
  MultiLine); движок в `history_view_message.cpp`: `MessageFilterRegex()`
  (статический кэш скомпилированного QRegularExpression), `MessageDimmedByFilter`
  → затемнение до 0.15 в `Message::draw`.
- **Ignore users** (pref `ignored-users`, QByteArray id через запятую):
  `AddIgnoreUserAction` в контекстном меню («Ignore user»/«Stop ignoring»,
  иконка menuIconBlock) — тогл префа + `requestItemRepaint`;
  `IgnoredUserIds()` (кэш) в message.cpp → те же 0.15.
- **Меню ☰**: пункты «Text replacements» и «Message filter» (обе иконки
  menuIconSettings — ЗАГЛУШКА, подобрать нормальные).

## 9. Стабильность сессий (v14-эпоха)

- Лаунчер `tools/test-release-launcher.command` → единый workdir
  `~/Library/Application Support/AccountBank` (совпадает с прямым запуском .app);
  миграция `AccountBank-Data.nosync` → канонический путь, старые копии
  переименованы `.migrated-20261016`; single-instance сокет закрывает дубли.
- `tools/test-release-readme.txt` переписан.

## 10. Полный список kv-префов (core_settings.h)

`ghost-mode` (true), `hide-sponsored` (true), `keep-deleted` (true),
`allow-copy-restricted` (true), `streamer-mode` (false — работа соседа),
`compact-chat-list` (false), `unread-first` (false), `local-pinned` (QByteArray),
`text-replacements` (QByteArray), `message-filter` (QByteArray),
`ignored-users` (QByteArray). Все через `readPref/writePref`, UI-тумблеры у
первых семи в ☰ (window_main_menu.cpp).

## 11. Что полировать по внешнему виду (задача соседнего чата)

1. Иконки меню: «Text replacements»/«Message filter» — обе `menuIconSettings`,
   «Ignore user» — `menuIconBlock`; подобрать осмысленные (в menu_icons.style
   есть запас). Главное меню перегружено — сгруппировать фичи AccountBank в
   подменю/секцию.
2. Язык строк: мои ключи EN, peek — RU. Унифицировать под язык интерфейса.
3. Маркер удалённых: текст «deleted» над сообщением — заменить на иконку
   корзины/бейдж; подобрать opacity 0.55 (удалённые) / 0.15 (фильтр/игнор);
   проверить отрисовку ВЫДЕЛЕНИЯ затемнённых сообщений.
4. Вкладка Admin: фиксированный «Admin» без иконки вкладки; локализовать,
   добавить иконку; ПКМ-меню вкладки минимально (осознанно).
5. Компактный список: forum/tagged-строки и глобальные результаты поиска людей
   остаются высокими (72/96/62px) — унифицировать при желании.
6. Локальные закрепы: без секции-заголовка и drag-reorder; значок тот же, что
   серверный — визуально отличить.
7. Chat rights box: read-only Yes/No — стилизовать под текущий дизайн-систему.
8. Тост автосейва TTL: 5 с, ссылка-путь — проверить читаемость длинных путей.
9. Peek-кнопка в профиле без иконки — добавить.
10. Регексп-бокс и бокс автозамен — одинаковый стиль поля (общий
    `textReplacementsField`) — ок, но подписки/подсказки можно улучшить.

## 12. Известные ограничения (не баги)

- Автозамены: теряется форматирование при сработке (v1); не покрывают
  editMessage/подписи к медиа/ephemeral.
- Peek: работает только если статус скрыт из-за МОЕЙ приватности; при сбое
  сети — тост об ошибке, правила восстанавливаются (кроме офлайн-сбоя restore).
- Матрица прав: без прав топиков, read-only.
- Стикер-владелец: только свои сеты.
- Локальные закрепы: без drag, только основной список+архив.
- Непрочитанные сверху: только незаглушенные чаты (семантика апстрима).
- Regex-фильтр: только визуальное затемнение — уведомления/счётчики не гасятся.
- View-once при ghost: отправитель не видит «просмотрено» — осознанно;
  копия у нас в ttl/.
- «Закрыл вьюер до окончания загрузки фото» — узкое окно потери автосейва
  (префетч обычно качает заранее).

## 13. Документация для продолжения

- `docs/development-waves-plan.md` — все волны дальше (В3: локальные
  категории, массовые действия, favorites, recent bar; В4 composer; В5 медиа;
  В6 большие C6; [Я]-В3: уведомления «контакт в сети», центр событий,
  скриншоты секретных, duress PIN; [Я]-В4: синк, автозагрузка медиа).
- `docs/merged-features-plan.md` — полный каталог с номерами.
- `docs/qa-checklist-v38-v39.md` — ручная проверка волн 1–2 (45 пунктов).
- `docs/accountbank-v37-backlog.md` — сверка соседнего чата.

## 14. Правила для продолжающего ИИ

- Сборка `out-size-oz` Release зелёная после каждого пакета; smoke — запуск с
  временным `-workdir`, живой профиль не копировать.
- Стиль: AGENTS.md + REVIEW.md (табы, `u"..."_q`, kv-префы, строки — конец
  lang.strings, комментарии rationed).
- «Запретные» фичи (ghost/anti-recall/noforwards/peek/фильтры) НЕ откатывать
  и не «чинить по ToS» — это осознанные решения владельца.
- Коммиты маленькие, один пакет — один коммит.
