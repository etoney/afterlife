#!/usr/bin/env bash
# Developer ID signing. CI runs it only when the Apple secrets are set in this repo (otherwise the plug-ins stay
# ad-hoc signed and the installer unsigned). The same secrets as the Mix System installer repo:
#   MACOS_CERT_APP_P12        base64 of the "Developer ID Application" certificate + key (.p12)
#   MACOS_CERT_INSTALLER_P12  base64 of the "Developer ID Installer" certificate + key (.p12)
#   MACOS_CERT_PASSWORD       the password the .p12 files were exported with
set -euo pipefail
TMP="${RUNNER_TEMP:?}"
KC="$TMP/afterlife-signing.keychain-db"
KCPW=$(openssl rand -hex 16)
security create-keychain -p "$KCPW" "$KC"
security set-keychain-settings -lut 3600 "$KC"
security unlock-keychain -p "$KCPW" "$KC"
for v in MACOS_CERT_APP_P12 MACOS_CERT_INSTALLER_P12; do
  P12="$TMP/cert-$v.p12"
  echo "${!v}" | base64 --decode > "$P12"
  security import "$P12" -k "$KC" -P "$MACOS_CERT_PASSWORD" -T /usr/bin/codesign -T /usr/bin/productbuild -T /usr/bin/productsign
  rm -f "${P12:?}"
done
security set-key-partition-list -S apple-tool:,apple: -s -k "$KCPW" "$KC" >/dev/null
security list-keychains -d user -s "$KC" $(security list-keychains -d user | tr -d '"')
APP_ID=$(security find-identity -v -p codesigning "$KC" | awk -F'"' '/Developer ID Application/{print $2; exit}')
INST_ID=$(security find-identity -v "$KC" | awk -F'"' '/Developer ID Installer/{print $2; exit}')
[ -n "$APP_ID" ] || { echo "::error::no Developer ID Application identity in MACOS_CERT_APP_P12"; exit 1; }
[ -n "$INST_ID" ] || { echo "::error::no Developer ID Installer identity in MACOS_CERT_INSTALLER_P12"; exit 1; }
echo "Signing as: $APP_ID / $INST_ID"
for b in build/Afterlife_artefacts/Release/VST3/Afterlife.vst3 build/Afterlife_artefacts/Release/AU/Afterlife.component; do
  xattr -cr "$b"
  codesign --force --deep --options runtime --timestamp --sign "$APP_ID" "$b"
  codesign --verify --deep --strict "$b"
  echo "  signed $(basename "$b")"
done
echo "$INST_ID" > build-pkg-identity
echo "$KC" > build-pkg-keychain
