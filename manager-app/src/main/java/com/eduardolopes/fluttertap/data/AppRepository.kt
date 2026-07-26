// FlutterTap manager app -- by Eduardo Lopes
package com.eduardolopes.fluttertap.data

import android.content.Context
import android.content.Intent
import android.content.pm.PackageManager

data class AppInfo(val packageName: String, val label: String)

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
            .map { pkg ->
                val label = try {
                    pm.getApplicationLabel(pm.getApplicationInfo(pkg, 0)).toString()
                } catch (e: PackageManager.NameNotFoundException) {
                    pkg
                }
                AppInfo(packageName = pkg, label = label)
            }
            .sortedBy { it.label.lowercase() }
    }
}
