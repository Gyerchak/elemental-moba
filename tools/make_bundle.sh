#!/usr/bin/env bash
# Builds the install folder for a pendrive from the current commit:  bash tools/make_bundle.sh <folder>
# Contains the game (git archive of HEAD), the local-only champions, the Windows programs of that commit (GitHub Actions)
# and the install scripts. Never contains settings.cfg, server.cfg, logs or replays.
set -euo pipefail
cd "$(dirname "$0")/.."
DEST="${1:?usage: make_bundle.sh <folder>}"
HEAD="$(git rev-parse HEAD)"
[ -z "$(git status --porcelain)" ] || { echo "Commit first: the bundle is made from HEAD."; exit 1; }
RUN="$(gh run list --commit "$HEAD" --status success --json databaseId -q '.[0].databaseId')"
[ -n "$RUN" ] || { echo "No finished build of $HEAD on GitHub yet."; exit 1; }

mkdir -p "$DEST"
git archive --format=tar.gz --prefix=elemental-moba/ HEAD -o "$DEST/elemental-moba-source.tar.gz"
LOCAL=()
for p in custom_champions/aatrox.champ custom_champions/taliyah.champ custom_champions/_server/aatrox.champ custom_champions/_server/taliyah.champ \
         resourcepacks/default/champions/aatrox resourcepacks/default/champions/taliyah \
         resourcepacks/default/models/aatrox resourcepacks/default/models/taliyah \
         resourcepacks/default/models_hd/aatrox resourcepacks/default/models_hd/taliyah; do
    [ -e "$p" ] && LOCAL+=("$p")
done
[ ${#LOCAL[@]} -eq 0 ] || tar czf "$DEST/elemental-moba-local.tar.gz" "${LOCAL[@]}"
TMP="$(mktemp -d)"
gh run download "$RUN" -n elemental-moba-windows-programs -D "$TMP"
mkdir -p "$DEST/windows-programs"
cp "$TMP"/* "$DEST/windows-programs/"
gio trash "$TMP"
cp install/bundle/README.txt install/bundle/install-linux.sh install/bundle/install-macos.sh install/bundle/install-windows.ps1 install/bundle/install-windows.cmd SERVER.txt "$DEST/"
( cd "$DEST" && find . -type f ! -name SHA256SUMS.txt -print0 | sort -z | xargs -0 sha256sum > SHA256SUMS.txt )
sync
( cd "$DEST" && sha256sum -c --quiet SHA256SUMS.txt ) && echo "Bundle OK: $DEST ($(du -sh "$DEST" | cut -f1)) from $(git rev-parse --short HEAD)"
