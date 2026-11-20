package com.nx.isd.uplink

import android.os.Bundle
import android.widget.Toast
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.compose.animation.Crossfade
import androidx.compose.animation.core.tween
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Modifier
import com.nx.isd.uplink.ui.components.TacticalDeviceDrawer
import com.nx.isd.uplink.ui.device.DeviceListScreen
import com.nx.isd.uplink.ui.home.HomeScreen
import com.nx.isd.uplink.ui.splash.SplashScreen
import com.nx.isd.uplink.ui.theme.ISDUplinkTheme
import com.nx.isd.uplink.ui.theme.OledBlack
import kotlinx.coroutines.launch

enum class AppDestination {
    SPLASH,
    DEVICE_LIST,
    NX_ISD_DASHBOARD
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
                    val drawerState = rememberDrawerState(initialValue = DrawerValue.Closed)
                    val scope = rememberCoroutineScope()

                    ModalNavigationDrawer(
                        drawerState = drawerState,
                        gesturesEnabled = currentDestination != AppDestination.SPLASH,
                        drawerContent = {
                            TacticalDeviceDrawer(
                                selectedDevice = when (currentDestination) {
                                    AppDestination.NX_ISD_DASHBOARD -> "NX-ISD"
                                    else -> null
                                },
                                onSelectDevice = { device ->
                                    if (device == "NX-ISD") {
                                        currentDestination = AppDestination.NX_ISD_DASHBOARD
                                    } else {
                                        Toast.makeText(
                                            this@MainActivity,
                                            "NX-Edge: Hardware release pending",
                                            Toast.LENGTH_SHORT
                                        ).show()
                                    }
                                    scope.launch { drawerState.close() }
                                },
                                onNavigateFleet = {
                                    currentDestination = AppDestination.DEVICE_LIST
                                    scope.launch { drawerState.close() }
                                },
                                onConnectNewDevice = {
                                    currentDestination = AppDestination.DEVICE_LIST
                                    scope.launch { drawerState.close() }
                                },
                                onCloseDrawer = {
                                    scope.launch { drawerState.close() }
                                }
                            )
                        }
                    ) {
                        Crossfade(
                            targetState = currentDestination,
                            animationSpec = tween(400),
                            label = "MainScreenNavigation"
                        ) { destination ->
                            when (destination) {
                                AppDestination.SPLASH -> {
                                    SplashScreen(
                                        onSplashComplete = {
                                            currentDestination = AppDestination.DEVICE_LIST
                                        }
                                    )
                                }
                                AppDestination.DEVICE_LIST -> {
                                    DeviceListScreen(
                                        onOpenDrawer = {
                                            scope.launch { drawerState.open() }
                                        },
                                        onSelectDevice = { device ->
                                            if (device == "NX-ISD") {
                                                currentDestination = AppDestination.NX_ISD_DASHBOARD
                                            }
                                        }
                                    )
                                }
                                AppDestination.NX_ISD_DASHBOARD -> {
                                    HomeScreen(
                                        onOpenDrawer = {
                                            scope.launch { drawerState.open() }
                                        },
                                        onNavigateFleet = {
                                            currentDestination = AppDestination.DEVICE_LIST
                                        }
                                    )
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
