#!/bin/zsh
set -eu

folder=${0:A:h}
cd "$folder"

fail() {
  print -u2 -- "AccountBank: $1"
  exit 1
}

[[ -f AccountBank.app.zip && -f SHA256.txt ]] || fail 'архив приложения не найден.'
shasum -a 256 -c SHA256.txt || fail 'архив повреждён.'
archive_hash=$(awk 'NR == 1 { print $1 }' SHA256.txt)
[[ "$archive_hash" =~ '^[0-9a-f]{64}$' ]] || fail 'неверный SHA256.txt.'

cache_root="/private/tmp/accountbank-test-cache-${EUID}"
if [[ -e "$cache_root" ]]; then
  [[ -d "$cache_root" && ! -L "$cache_root" && -O "$cache_root" ]] \
    || fail 'небезопасная временная папка.'
else
  mkdir -m 700 "$cache_root" || fail 'не удалось создать временную папку.'
fi
chmod 700 "$cache_root"

cached="$cache_root/$archive_hash"
app="$cached/AccountBank.app"
if [[ ! -d "$app" ]]; then
  lock="$cache_root/.install-$archive_hash"
  if mkdir -m 700 "$lock" 2>/dev/null; then
    stage=$(mktemp -d "$cache_root/.stage.XXXXXX")
    cleanup() {
      [[ -n ${stage:-} ]] && rm -rf -- "$stage"
      rmdir "$lock" 2>/dev/null || true
    }
    trap cleanup EXIT
    ditto -x -k "$folder/AccountBank.app.zip" "$stage/AccountBank.app" \
      || fail 'не удалось распаковать приложение.'
    codesign --verify --deep --strict "$stage/AccountBank.app" \
      || fail 'подпись приложения не прошла проверку.'
    mv "$stage" "$cached" || fail 'не удалось сохранить проверенную копию.'
    stage=''
    cleanup
    trap - EXIT
  else
    for attempt in {1..60}; do
      [[ -d "$app" ]] && break
      sleep 0.5
    done
    [[ -d "$app" ]] || fail 'другая установка не завершилась.'
  fi
fi

codesign --verify --deep --strict "$app" \
  || fail 'проверенная копия приложения повреждена.'
[[ ${1:-} == --verify-only ]] && exit 0

data_folder="$HOME/Library/Application Support/AccountBank"
[[ -d "$data_folder" && ! -L "$data_folder" && -O "$data_folder" ]] \
  || fail 'папка данных AccountBank (Application Support) отсутствует или небезопасна.'
[[ -d "$data_folder/tdata" && ! -L "$data_folder/tdata" && -O "$data_folder/tdata" ]] \
  || fail 'ключ профиля не найден; запуск остановлен.'
domain_key_found=false
for key_suffix in s 0 1; do
  candidate="$data_folder/tdata/key_data$key_suffix"
  if [[ -f "$candidate" && ! -L "$candidate" ]]; then
    domain_key_found=true
    break
  fi
done
[[ "$domain_key_found" == true ]] \
  || fail 'ключ профиля не найден; запуск остановлен.'
set +e
"$app/Contents/MacOS/AccountBank" -workdir "$data_folder" -noupdate
result=$?
set -e
(( result == 0 )) || print -u2 -- "AccountBank завершился с кодом $result."
exit "$result"
