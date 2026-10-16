#!/bin/zsh
set -eu

repo=${0:A:h:h}
workspace=${repo:h}
version=${1:-}
[[ "$version" =~ '^v[0-9]+$' ]] || {
  print -u2 -- 'Укажите номер вида v7.'
  exit 2
}

build_tree=${ACCOUNTBANK_BUILD_TREE:-out-release}
[[ "$build_tree" =~ '^out-[a-z0-9-]+$' ]] || {
  print -u2 -- 'Неверное имя папки сборки.'
  exit 2
}
[[ ! -L "$repo/$build_tree" ]] || {
  print -u2 -- 'Папка сборки не должна быть символьной ссылкой.'
  exit 2
}
source_app="$repo/$build_tree/Release/AccountBank.app"
[[ -f "$source_app/Contents/MacOS/AccountBank" ]] || {
  print -u2 -- 'Сначала соберите Release AccountBank.'
  exit 1
}

package="$workspace/Builds.nosync/AccountBank-Minimal-Release-${version}-macOS-arm64-Test"
[[ ! -e "$package" ]] || {
  print -u2 -- 'Такая тестовая папка уже существует; она сохранена без изменений.'
  exit 1
}

staging=$(mktemp -d /private/tmp/accountbank-package.XXXXXX)
package_stage=$(mktemp -d "$workspace/Builds.nosync/.accountbank-package.XXXXXX")
cleanup() {
  rm -rf -- "$staging"
  [[ -n ${package_stage:-} ]] && rm -rf -- "$package_stage"
  return 0
}
trap cleanup EXIT
mkdir -p "$staging/source"
ditto --norsrc --noextattr "$source_app" "$staging/source/AccountBank.app"
strip "$staging/source/AccountBank.app/Contents/MacOS/AccountBank"
xattr -cr "$staging/source/AccountBank.app"
codesign --force --deep --sign - "$staging/source/AccountBank.app"
codesign --verify --deep --strict "$staging/source/AccountBank.app"

ditto -c -k --norsrc --noextattr --zlibCompressionLevel 9 \
  "$staging/source/AccountBank.app" "$package_stage/AccountBank.app.zip"
cd "$package_stage"
shasum -a 256 AccountBank.app.zip > SHA256.txt
cp "$repo/tools/test-release-launcher.command" \
  "$package_stage/Запустить AccountBank.command"
cp "$repo/tools/test-release-readme.txt" "$package_stage/ПРОЧИТАТЬ.txt"
chmod 755 "$package_stage/Запустить AccountBank.command"
zsh -n "$package_stage/Запустить AccountBank.command"
"$package_stage/Запустить AccountBank.command" --verify-only
mv "$package_stage" "$package"
package_stage=''
print -- "$package"
