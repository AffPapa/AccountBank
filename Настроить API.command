#!/bin/zsh
set -eu
umask 077

folder=${0:A:h}
api_id=$(osascript -e 'text returned of (display dialog "Telegram api_id (только цифры):" default answer "")')
api_hash=$(osascript -e 'text returned of (display dialog "Telegram api_hash (32 символа):" default answer "" with hidden answer)')

if [[ ! "$api_id" =~ '^[0-9]{1,10}$' ]] || [[ ! "$api_hash" =~ '^[0-9A-Fa-f]{32}$' ]]; then
	print -u2 'Неверный формат API-данных. Файл не изменён.'
	exit 1
fi

temporary=$(mktemp "$folder/.accountbank-api.local.cmake.XXXXXX")
trap 'rm -f "$temporary"' EXIT
printf 'set(TDESKTOP_API_ID "%s")\nset(TDESKTOP_API_HASH "%s")\n' \
	"$api_id" "$api_hash" > "$temporary"
mv -f "$temporary" "$folder/accountbank-api.local.cmake"
touch "$folder/CMakeLists.txt"
print 'Готово: локальный файл API создан. Значения не выводились в Терминал.'
