// FlutterTap manager app -- by Eduardo Lopes
package com.eduardolopes.fluttertap.data

import android.util.Base64
import com.topjohnwu.superuser.Shell

enum class RootBackend { MAGISK, KERNELSU, APATCH, UNKNOWN }

data class RootStatus(
    val granted: Boolean,
    val backend: RootBackend,
    val moduleInstalled: Boolean,
    /** Only meaningful when [moduleInstalled] is true: absence of the `disable` marker file. */
    val moduleEnabled: Boolean,
)

/** All root shell I/O for FlutterTap: root/backend detection and config.json read/write. */
object RootManager {
    private const val CONFIG_DIR = "/data/adb/fluttertap"
    private const val CONFIG_FILE = "$CONFIG_DIR/config.json"
    private const val MODULE_DIR = "/data/adb/modules/fluttertap"

    // Runs on a background thread; callers are expected to launch it off the main thread.
    fun queryStatus(): RootStatus {
        val granted = Shell.getShell().isRoot
        if (!granted) {
            return RootStatus(granted = false, backend = RootBackend.UNKNOWN, moduleInstalled = false, moduleEnabled = false)
        }

        val backend = when {
            exists("/data/adb/ksu") || Shell.cmd("ksud --version").exec().isSuccess -> RootBackend.KERNELSU
            exists("/data/adb/ap") -> RootBackend.APATCH
            Shell.cmd("magisk -v").exec().isSuccess -> RootBackend.MAGISK
            else -> RootBackend.UNKNOWN
        }
        val moduleInstalled = exists(MODULE_DIR)
        val moduleEnabled = moduleInstalled && !exists("$MODULE_DIR/disable")
        return RootStatus(granted = true, backend = backend, moduleInstalled = moduleInstalled, moduleEnabled = moduleEnabled)
    }

    /**
     * Closes the cached shell if it's not root and asks again -- covers the case where the
     * user dismissed or denied the automatic prompt on first launch and now wants to retry
     * (e.g. after fixing the app's permission in their root manager). A prior hard "deny"
     * recorded by the root manager itself still won't re-prompt; only it can undo that.
     */
    fun requestRoot(): RootStatus {
        Shell.getCachedShell()?.let { if (!it.isRoot) it.close() }
        return queryStatus()
    }

    fun readConfig(): ConfigData {
        val text = Shell.cmd("cat $CONFIG_FILE 2>/dev/null").exec().out.joinToString("\n")
        return ConfigData.fromJson(text) ?: ConfigData.default()
    }

    fun writeConfig(config: ConfigData): Boolean {
        val json = config.toJson()
        // Written via base64 so no part of the JSON (proxy IP, package names) ever
        // needs shell-escaping.
        val encoded = Base64.encodeToString(json.toByteArray(Charsets.UTF_8), Base64.NO_WRAP)
        val cmd = "mkdir -p $CONFIG_DIR && echo '$encoded' | base64 -d > $CONFIG_FILE && chmod 644 $CONFIG_FILE"
        return Shell.cmd(cmd).exec().isSuccess
    }

    private fun exists(path: String): Boolean =
        Shell.cmd("[ -e $path ] && echo yes").exec().out.any { it.trim() == "yes" }
}
