// FlutterTap manager app -- by Eduardo Lopes
package com.eduardolopes.fluttertap

import android.app.Application
import com.topjohnwu.superuser.Shell

class FlutterTapApplication : Application() {
    override fun onCreate() {
        super.onCreate()
        // libsu's shell builder can only be set once per process. It must live here
        // (Application.onCreate runs exactly once per process) rather than in
        // MainActivity's init block, which re-runs every time the activity is
        // recreated -- e.g. when AppCompatDelegate.setApplicationLocales() applies a
        // new locale on API <33 and crashes with "The main shell was already created".
        Shell.enableVerboseLogging = false
        Shell.setDefaultBuilder(Shell.Builder.create().setFlags(Shell.FLAG_MOUNT_MASTER))
    }
}
