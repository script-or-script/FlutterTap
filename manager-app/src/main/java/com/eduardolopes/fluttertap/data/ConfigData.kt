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
    // 203.0.113.0/24 is reserved for documentation (RFC 5737) and can never be
    // a real LAN address -- unlike a 192.168.x.x-looking default, a user can't
    // mistake this for their own IP and leave it unchanged by accident.
    val proxyIp: String = "203.0.113.1",
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
                    proxyIp = obj.optString("proxy_ip", "203.0.113.1"),
                    proxyPort = obj.optInt("proxy_port", 8083),
                    targetPackages = pkgs,
                )
            } catch (e: Exception) {
                null
            }
        }
    }
}
