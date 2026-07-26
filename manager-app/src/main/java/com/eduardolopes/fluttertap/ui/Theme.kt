// FlutterTap manager app -- by Eduardo Lopes
package com.eduardolopes.fluttertap.ui

import android.os.Build
import androidx.compose.foundation.isSystemInDarkTheme
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.darkColorScheme
import androidx.compose.material3.dynamicDarkColorScheme
import androidx.compose.material3.dynamicLightColorScheme
import androidx.compose.material3.lightColorScheme
import androidx.compose.runtime.Composable
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.platform.LocalContext

private val BluePrimaryLight = Color(0xFF0D47A1)
private val BluePrimaryDark = Color(0xFF90CAF9)

private val LightColors = lightColorScheme(primary = BluePrimaryLight)
private val DarkColors = darkColorScheme(primary = BluePrimaryDark)

/**
 * Always derives its color scheme from [isSystemInDarkTheme]. This is what
 * keeps the manager's background in sync with the phone's system theme
 * (fixing the "white background while the phone is in dark mode" bug of
 * flat/hardcoded-light themes).
 */
@Composable
fun FlutterTapTheme(content: @Composable () -> Unit) {
    val useDark = isSystemInDarkTheme()
    val context = LocalContext.current

    val colorScheme = when {
        Build.VERSION.SDK_INT >= Build.VERSION_CODES.S ->
            if (useDark) dynamicDarkColorScheme(context) else dynamicLightColorScheme(context)
        useDark -> DarkColors
        else -> LightColors
    }

    MaterialTheme(colorScheme = colorScheme, content = content)
}
