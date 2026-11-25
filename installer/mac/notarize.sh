#!/usr/bin/env bash
# Sends the signed .pkg to Apple's notary service and staples the ticket, so macOS opens it with no warning.
#   NOTARY_KEY_P8 (base64 of the App Store Connect API key .p8), NOTARY_KEY_ID, NOTARY_ISSUER_ID
set -euo pipefail
PKG=$(ls Afterlife-v*-macOS-Installer.pkg | head -1)
KEY="${RUNNER_TEMP:?}/notary.p8"
echo "$NOTARY_KEY_P8" | base64 --decode > "$KEY"
xcrun notarytool submit "$PKG" --key "$KEY" --key-id "$NOTARY_KEY_ID" --issuer "$NOTARY_ISSUER_ID" --wait --timeout 30m --output-format json | tee notary.json
rm -f "${KEY:?}"
python3 -c "import json,sys; s=json.load(open('notary.json'))['status']; print('notary:', s); sys.exit(0 if s=='Accepted' else 1)"
xcrun stapler staple "$PKG"
xcrun stapler validate "$PKG"
spctl --assess --type install -vv "$PKG"
