// FlutterTap manager app -- by Eduardo Lopes
package com.eduardolopes.fluttertap

import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.appcompat.app.AppCompatDelegate
import androidx.core.os.LocaleListCompat
import com.eduardolopes.fluttertap.ui.FlutterTapTheme
import com.eduardolopes.fluttertap.ui.HomeScreen
import com.topjohnwu.superuser.Shell

class MainActivity : ComponentActivity() {

    init {
        Shell.enableVerboseLogging = false
        Shell.setDefaultBuilder(Shell.Builder.create().setFlags(Shell.FLAG_MOUNT_MASTER))
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContent {
            FlutterTapTheme {
                HomeScreen(onLanguageSelected = ::applyLanguage)
            }
        }
    }

    private fun applyLanguage(languageTag: String?) {
        val locales =
            if (languageTag == null) LocaleListCompat.getEmptyLocaleList()
            else LocaleListCompat.forLanguageTags(languageTag)
        AppCompatDelegate.setApplicationLocales(locales)
    }
}
