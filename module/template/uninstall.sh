#!/system/bin/sh
# FlutterTap uninstaller -- by Eduardo Lopes
#
# Deliberately NOT removing /data/adb/fluttertap/config.json: if the user
# reinstalls the module later, their proxy IP/port and selected apps come
# back exactly as they left them. Nothing else was changed outside the
# module's own directory, so there is nothing else to clean up here.
