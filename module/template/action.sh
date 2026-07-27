#!/system/bin/sh
# FlutterTap action script -- by Eduardo Lopes
#
# Runs when the user taps "Action" next to FlutterTap in the module list
# (Magisk/KernelSU/SukiSu Ultra/APatch all support this the same way: an
# optional action.sh in the module's root, executed on demand as root).
# Opens the FlutterTap manager app so proxy/target-app settings can be
# changed without hunting for its icon in the app drawer.
am start -n com.eduardolopes.fluttertap/.MainActivity >/dev/null 2>&1
