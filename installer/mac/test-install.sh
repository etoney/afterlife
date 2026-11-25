#!/usr/bin/env bash
# Installs the .pkg on the CI Mac the way a user would (downloaded, so quarantined) and checks: both formats
# land in /Library, universal, with no quarantine flag, and the guide is in place.
set -euo pipefail
PKG=$(ls Afterlife-v*-macOS-Installer.pkg | head -1)
xattr -w com.apple.quarantine "0081;00000000;Safari;" "$PKG"
sudo installer -pkg "$PKG" -target / -verboseR
fail=0
for b in /Library/Audio/Plug-Ins/VST3/Afterlife.vst3 /Library/Audio/Plug-Ins/Components/Afterlife.component; do
  if [ ! -d "$b" ]; then echo "::error::$b missing"; fail=1; continue; fi
  archs=$(lipo -archs "$b/Contents/MacOS/Afterlife")
  echo "$(basename "$b"): $archs"
  if [[ "$archs" != *arm64* || "$archs" != *x86_64* ]]; then echo "::error::$b is not universal"; fail=1; fi
  if xattr -lr "$b" | grep -q quarantine; then echo "::error::$b still quarantined"; fail=1; fi
done
[ -f "/Applications/Castle Danger Music Company/Guides/Afterlife - controls.txt" ] || { echo "::error::guide missing"; fail=1; }
pkgutil --pkg-info com.castledanger.afterlife
ls -la "/Applications/Castle Danger Music Company/Guides"
exit $fail
