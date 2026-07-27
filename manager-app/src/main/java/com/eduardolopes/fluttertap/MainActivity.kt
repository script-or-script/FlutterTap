// FlutterTap manager app -- by Eduardo Lopes
package com.eduardolopes.fluttertap

import android.os.Bundle
import androidx.activity.compose.setContent
import androidx.appcompat.app.AppCompatActivity
import androidx.appcompat.app.AppCompatDelegate
import androidx.core.os.LocaleListCompat
import com.eduardolopes.fluttertap.ui.FlutterTapTheme
import com.eduardolopes.fluttertap.ui.HomeScreen

// AppCompatActivity (not the plain ComponentActivity) is required here: AppCompatDelegate's
// per-app language backport only auto-recreates the activity with the new locale on API <33
// when the activity extends AppCompatActivity.
class MainActivity : AppCompatActivity() {

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
