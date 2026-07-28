#!/system/bin/sh
# FlutterTap installer script -- by Eduardo Lopes
# Works the same way when flashed through Magisk, KernelSU or APatch.

SKIPUNZIP=1

ui_print "- FlutterTap"
ui_print "  by Eduardo Lopes"
ui_print "- Extracting module files"

# Standard Magisk/KernelSU/APatch module layout extraction. With SKIPUNZIP=1
# this is the only thing that puts the payload in place, so a silent failure
# here would "install" an empty module that does nothing.
unzip -o "$ZIPFILE" -x 'META-INF/*' -d "$MODPATH" >&2 || abort "! Failed to extract module files"
[ -f "$MODPATH/zygisk/arm64-v8a.so" ] || abort "! Module payload is missing after extraction"

if [ -n "$KSU" ]; then
  ui_print "- Detected KernelSU"
elif [ -n "$APATCH" ]; then
  ui_print "- Detected APatch"
elif [ -n "$MAGISK_VER_CODE" ]; then
  ui_print "- Detected Magisk"
fi
ui_print "- Make sure Zygisk (or Zygisk Next, on KernelSU) is enabled"

# Persistent config directory, separate from $MODPATH so user settings
# configured through the FlutterTap manager app survive module updates
# and reinstalls.
CONFIG_DIR=/data/adb/fluttertap
CONFIG_FILE="$CONFIG_DIR/config.json"

mkdir -p "$CONFIG_DIR"
if [ ! -f "$CONFIG_FILE" ]; then
  ui_print "- Writing default configuration"
  cat > "$CONFIG_FILE" << 'EOF'
{
  "enabled": true,
  "proxy_ip": "203.0.113.1",
  "proxy_port": 8083,
  "target_packages": []
}
EOF
  ui_print "- No target apps are selected by default."
else
  ui_print "- Existing configuration preserved"
fi

chmod 755 "$CONFIG_DIR"
chmod 644 "$CONFIG_FILE"

ui_print "- Open the FlutterTap manager app to choose apps and proxy IP/port."

set_perm_recursive "$MODPATH" 0 0 0755 0644
# action.sh is invoked by the module manager's "Action" button; the recursive
# call above would otherwise leave it 0644.
set_perm "$MODPATH/action.sh" 0 0 0755
