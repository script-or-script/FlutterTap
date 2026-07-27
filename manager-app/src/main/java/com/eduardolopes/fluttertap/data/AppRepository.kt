// FlutterTap manager app -- by Eduardo Lopes
package com.eduardolopes.fluttertap.data

import android.content.Context
import android.content.Intent
import android.content.pm.ApplicationInfo
import android.content.pm.PackageManager

data class AppInfo(val packageName: String, val label: String, val isSystemApp: Boolean = false)

object AppRepository {
    /** Every launchable (non-FlutterTap) app installed on the device, sorted by label. */
    fun listLaunchableApps(context: Context): List<AppInfo> {
        val pm = context.packageManager
        val intent = Intent(Intent.ACTION_MAIN).addCategory(Intent.CATEGORY_LAUNCHER)
        val resolved = pm.queryIntentActivities(intent, PackageManager.MATCH_ALL)

        return resolved
            .map { it.activityInfo.packageName }
            .distinct()
            .filterNot { it == context.packageName }
            .map { pkg -> appInfoFor(pm, pkg) }
            .sortedBy { it.label.lowercase() }
    }

    /**
     * System apps (ApplicationInfo.FLAG_SYSTEM) that don't already appear in
     * [listLaunchableApps] -- e.g. OEM/system services and components with no home-screen
     * icon, which can still embed a Flutter engine and make network calls worth intercepting.
     */
    fun listSystemOnlyApps(context: Context, excludingPackages: Set<String>): List<AppInfo> {
        val pm = context.packageManager
        @Suppress("DEPRECATION")
        val installed = pm.getInstalledApplications(PackageManager.MATCH_ALL)

        return installed
            .filter { it.flags and ApplicationInfo.FLAG_SYSTEM != 0 }
            .map { it.packageName }
            .filterNot { it == context.packageName || it in excludingPackages }
            .map { pkg -> appInfoFor(pm, pkg, forceSystem = true) }
            .sortedBy { it.label.lowercase() }
    }

    private fun appInfoFor(pm: PackageManager, pkg: String, forceSystem: Boolean = false): AppInfo {
        return try {
            val appInfo = pm.getApplicationInfo(pkg, 0)
            AppInfo(
                packageName = pkg,
                label = pm.getApplicationLabel(appInfo).toString(),
                isSystemApp = forceSystem || (appInfo.flags and ApplicationInfo.FLAG_SYSTEM != 0),
            )
        } catch (e: PackageManager.NameNotFoundException) {
            AppInfo(packageName = pkg, label = pkg, isSystemApp = forceSystem)
        }
    }
}
