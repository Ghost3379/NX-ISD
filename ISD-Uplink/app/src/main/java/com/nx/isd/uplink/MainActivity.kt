package com.nx.isd.uplink

import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.compose.animation.Crossfade
import androidx.compose.animation.core.tween
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.material3.Surface
import androidx.compose.runtime.*
import androidx.compose.ui.Modifier
import com.nx.isd.uplink.ui.home.HomeScreen
import com.nx.isd.uplink.ui.splash.SplashScreen
import com.nx.isd.uplink.ui.theme.ISDUplinkTheme
import com.nx.isd.uplink.ui.theme.OledBlack

enum class AppDestination {
    SPLASH,
    HOME
}

class MainActivity : ComponentActivity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContent {
            ISDUplinkTheme {
                Surface(
                    modifier = Modifier.fillMaxSize(),
                    color = OledBlack
                ) {
                    var currentDestination by remember { mutableStateOf(AppDestination.SPLASH) }

                    Crossfade(
                        targetState = currentDestination,
                        animationSpec = tween(500),
                        label = "ScreenTransition"
                    ) { destination ->
                        when (destination) {
                            AppDestination.SPLASH -> {
                                SplashScreen(
                                    onSplashComplete = {
                                        currentDestination = AppDestination.HOME
                                    }
                                )
                            }
                            AppDestination.HOME -> {
                                HomeScreen()
                            }
                        }
                    }
                }
            }
        }
    }
}
