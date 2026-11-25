#!/usr/bin/env bash
# Builds Afterlife-v<version>-macOS-Installer.pkg from the plug-ins CI just built. Run on macOS, from the repo root.
#   VST3 + AU      -> /Library/Audio/Plug-Ins/VST3 and /Components (every user, every DAW)
#   quick guides   -> /Applications/Castle Danger Music Company/Guides
# Before installing, copies installed by hand in your own plug-in folders are moved aside (so the DAW doesn't see
# two Afterlifes). After installing, the download flag is cleared and the AU cache reset: no Terminal needed.
set -euo pipefail
VER=$(sed -n 's/^project(AFTERLIFE VERSION \([0-9.]*\)).*/\1/p' CMakeLists.txt)
R=build/Afterlife_artefacts/Release
OUT="Afterlife-v$VER-macOS-Installer.pkg"
APPDIR="Applications/Castle Danger Music Company"
W=build-pkg
rm -rf ./build-pkg
mkdir -p "$W/root/Library/Audio/Plug-Ins/VST3" "$W/root/Library/Audio/Plug-Ins/Components" "$W/root/$APPDIR/Guides" "$W/scripts" "$W/resources"

cp -R "$R/VST3/Afterlife.vst3" "$W/root/Library/Audio/Plug-Ins/VST3/"
cp -R "$R/AU/Afterlife.component" "$W/root/Library/Audio/Plug-Ins/Components/"
cp controls.txt "$W/root/$APPDIR/Guides/Afterlife - controls.txt"
cp THIRD-PARTY-NOTICES.txt "$W/root/$APPDIR/Guides/Afterlife - third-party notices.txt"
xattr -cr "$W/root" 2>/dev/null || true

# the bundles must land where we say, not wherever an older copy happens to sit
pkgbuild --analyze --root "$W/root" "$W/component.plist" >/dev/null
i=0; while /usr/libexec/PlistBuddy -c "Print :$i" "$W/component.plist" >/dev/null 2>&1; do
  /usr/libexec/PlistBuddy -c "Set :$i:BundleIsRelocatable false" "$W/component.plist" 2>/dev/null \
    || /usr/libexec/PlistBuddy -c "Add :$i:BundleIsRelocatable bool false" "$W/component.plist"
  i=$((i+1)); done

cp installer/mac/scripts/preinstall installer/mac/scripts/postinstall "$W/scripts/"
chmod +x "$W/scripts/preinstall" "$W/scripts/postinstall"
pkgbuild --root "$W/root" --component-plist "$W/component.plist" --scripts "$W/scripts" \
         --identifier com.castledanger.afterlife --version "$VER" --install-location / "$W/afterlife.pkg" >/dev/null

cp installer/mac/resources/* "$W/resources/"
cp LICENSE.txt "$W/resources/license.txt"
sed -i '' "s/@VER@/$VER/g" "$W/resources/welcome.html"
cat > "$W/distribution.xml" <<XML
<?xml version="1.0" encoding="utf-8"?>
<installer-gui-script minSpecVersion="2">
    <title>Afterlife $VER (Castle Danger Music Company)</title>
    <organization>com.castledanger</organization>
    <welcome file="welcome.html" mime-type="text/html"/>
    <license file="license.txt"/>
    <conclusion file="conclusion.html" mime-type="text/html"/>
    <options customize="never" require-scripts="false" hostArchitectures="arm64,x86_64" rootVolumeOnly="true"/>
    <domains enable_localSystem="true"/>
    <volume-check><allowed-os-versions><os-version min="11.0"/></allowed-os-versions></volume-check>
    <choices-outline><line choice="afterlife"/></choices-outline>
    <choice id="afterlife" title="Afterlife" description="Convolution reverb + delay with section lanes. VST3 and AU."><pkg-ref id="com.castledanger.afterlife"/></choice>
    <pkg-ref id="com.castledanger.afterlife" version="$VER" onConclusion="none">afterlife.pkg</pkg-ref>
</installer-gui-script>
XML

SIGN=()
if [ -f build-pkg-identity ]; then SIGN=(--sign "$(cat build-pkg-identity)" --keychain "$(cat build-pkg-keychain)" --timestamp); fi
productbuild --distribution "$W/distribution.xml" --resources "$W/resources" --package-path "$W" ${SIGN[@]+"${SIGN[@]}"} "$OUT"
ls -la "$OUT"
