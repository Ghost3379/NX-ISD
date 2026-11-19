package com.nx.isd.uplink.ui.theme

import android.app.Activity
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.darkColorScheme
import androidx.compose.runtime.Composable
import androidx.compose.runtime.SideEffect
import androidx.compose.ui.graphics.toArgb
import androidx.compose.ui.platform.LocalView
import androidx.core.view.WindowCompat

private val TacticalColorScheme = darkColorScheme(
    primary = AmberBright,
    onPrimary = OledBlack,
    primaryContainer = AmberMid,
    onPrimaryContainer = TextPrimary,
    secondary = TacticalCyan,
    onSecondary = OledBlack,
    background = OledBlack,
    onBackground = TextPrimary,
    surface = TacticalSurface,
    onSurface = TextPrimary,
    surfaceVariant = TacticalSurfaceElevated,
    onSurfaceVariant = TextSecondary,
    outline = TacticalBorder
)

@Composable
fun ISDUplinkTheme(content: @Composable () -> Unit) {
    val view = LocalView.current
    if (!view.isInEditMode) {
        SideEffect {
            val window = (view.context as Activity).window
            window.statusBarColor = OledBlack.toArgb()
            window.navigationBarColor = OledBlack.toArgb()
            WindowCompat.getInsetsController(window, view).isAppearanceLightStatusBars = false
            WindowCompat.getInsetsController(window, view).isAppearanceLightNavigationBars = false
        }
    }

    MaterialTheme(
        colorScheme = TacticalColorScheme,
        content = content
    )
}
