// FlutterTap manager app -- by Eduardo Lopes
package com.eduardolopes.fluttertap.data

import org.json.JSONArray
import org.json.JSONObject

/**
 * Mirrors the schema the native module reads at /data/adb/fluttertap/config.json
 * (see module_config.h/.cpp). Keep the two in sync.
 */
data class ConfigData(
    val enabled: Boolean = true,
    val proxyIp: String = "192.168.15.17",
    val proxyPort: Int = 8083,
    val targetPackages: Set<String> = emptySet(),
) {
    fun toJson(): String {
        val obj = JSONObject()
        obj.put("enabled", enabled)
        obj.put("proxy_ip", proxyIp)
        obj.put("proxy_port", proxyPort)
        obj.put("target_packages", JSONArray(targetPackages.sorted()))
        return obj.toString(2)
    }

    companion object {
        fun default() = ConfigData()

        fun fromJson(text: String): ConfigData? {
            if (text.isBlank()) return null
            return try {
                val obj = JSONObject(text)
                val pkgs = mutableSetOf<String>()
                obj.optJSONArray("target_packages")?.let { arr ->
                    for (i in 0 until arr.length()) pkgs.add(arr.getString(i))
                }
                ConfigData(
                    enabled = obj.optBoolean("enabled", true),
                    proxyIp = obj.optString("proxy_ip", "192.168.15.17"),
                    proxyPort = obj.optInt("proxy_port", 8083),
                    targetPackages = pkgs,
                )
            } catch (e: Exception) {
                null
            }
        }
    }
}
