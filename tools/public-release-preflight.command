#!/bin/zsh
set -eu

repo=${0:A:h:h}
app=${1:-}
[[ -n "$app" && -d "$app" && ! -L "$app" ]] || {
  print -u2 -- 'Укажите готовый .app из out-public/Release.'
  exit 2
}
app=${app:A}
[[ "$app" == "$repo"/out-public/Release/*.app ||
   "$app" == "${repo:h}"/Builds.nosync/NeTelegram-Public-Release-Candidate/mount/*.app ]] || {
  print -u2 -- 'Приложение должно быть в out-public или в каталоге подписываемой копии.'
  exit 1
}
cache="$repo/out-public/CMakeCache.txt"
[[ -f "$cache" ]] && rg -q '^ACCOUNTBANK_PUBLIC_RELEASE:BOOL=ON$' "$cache" || {
  print -u2 -- 'Сборка не использовала режим публичного выпуска.'
  exit 1
}
[[ -f "$repo/accountbank-api.local.cmake" ]] || {
  print -u2 -- 'Нет локальной конфигурации API для сборки.'
  exit 1
}
[[ -z "$(git -C "$repo" status --porcelain --untracked-files=all --ignore-submodules=untracked)" ]] || {
  print -u2 -- 'Рабочее дерево и подмодули должны быть чистыми.'
  exit 1
}
[[ -f "$app/Contents/MacOS/AccountBank" ]] || {
  print -u2 -- 'Исполняемый файл не найден.'
  exit 1
}
commit_time=$(git -C "$repo" log -1 --format=%ct)
binary_time=$(stat -f %m "$app/Contents/MacOS/AccountBank")
(( binary_time >= commit_time )) || {
  print -u2 -- 'Приложение собрано раньше текущего коммита.'
  exit 1
}
[[ "$(lipo -archs "$app/Contents/MacOS/AccountBank")" == 'arm64' ]] || {
  print -u2 -- 'Проверена только сборка arm64.'
  exit 1
}
if find "$app" -iname 'tdata' -o -iname 'archive.ab' -o \
    -iname 'key_data*' -o -iname '*.local.cmake' -o -iname '.env*' | rg -q .; then
  print -u2 -- 'В пакете найдены локальные данные или конфигурация.'
  exit 1
fi
codesign --verify --deep --strict "$app" >/dev/null 2>&1 || {
  print -u2 -- 'Подпись Developer ID не прошла проверку.'
  exit 1
}
signature=$(codesign -dv --verbose=4 "$app" 2>&1)
print -r -- "$signature" | rg -q '^Authority=Developer ID Application:' || {
  print -u2 -- 'Нужна подпись Developer ID Application.'
  exit 1
}
print -r -- "$signature" | rg -q '^TeamIdentifier=[A-Z0-9]+' || {
  print -u2 -- 'В подписи нет идентификатора команды.'
  exit 1
}
print -r -- "$signature" | rg -q 'flags=.*runtime' || {
  print -u2 -- 'Для выпуска требуется hardened runtime.'
  exit 1
}
xcrun stapler validate "$app" >/dev/null 2>&1 || {
  print -u2 -- 'Приложение не прошло проверку нотариализации.'
  exit 1
}
print -- 'Проверки локального публичного .app пройдены.'
