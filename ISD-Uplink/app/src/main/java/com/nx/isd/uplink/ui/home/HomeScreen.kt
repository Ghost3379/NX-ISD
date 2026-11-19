package com.nx.isd.uplink.ui.home

import android.widget.Toast
import androidx.activity.compose.BackHandler
import androidx.compose.animation.Crossfade
import androidx.compose.animation.core.tween
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.unit.dp
import com.nx.isd.uplink.ui.components.TacticalBackgroundGrid
import com.nx.isd.uplink.ui.submenu.SubMenuScreen
import com.nx.isd.uplink.ui.theme.*

@Composable
fun HomeScreen() {
    val context = LocalContext.current
    var activeSubMenu by remember { mutableStateOf<String?>(null) }
    var statusTitle by remember { mutableStateOf("Good") }
    var statusSubtitle by remember { mutableStateOf("No battery or sensor issues") }

    // Intercept physical/gesture back button to exit sub-menu back to hub
    BackHandler(enabled = activeSubMenu != null) {
        activeSubMenu = null
    }

    Surface(
        modifier = Modifier.fillMaxSize(),
        color = OledBlack
    ) {
        Crossfade(
            targetState = activeSubMenu,
            animationSpec = tween(350),
            label = "SubMenuNavigation"
        ) { currentSubMenu ->
            if (currentSubMenu != null) {
                // Dedicated Tactical Sub-Menu View (No popup dialog!)
                SubMenuScreen(
                    nodeId = currentSubMenu,
                    onBackClicked = { activeSubMenu = null }
                )
            } else {
                // Main WayneTech Hub Screen
                Box(
                    modifier = Modifier.fillMaxSize()
                ) {
                    // Subtle Arkham Origins Ambient Grid Layer
                    TacticalBackgroundGrid()

                    Column(
                        modifier = Modifier
                            .fillMaxSize()
                            .statusBarsPadding()
                            .padding(horizontal = 16.dp)
                            .verticalScroll(rememberScrollState()),
                        horizontalAlignment = Alignment.CenterHorizontally
                    ) {
                        Spacer(modifier = Modifier.height(48.dp))

                        // ZONE 1: Minimalist Health Care Overview (Monochrome Vector Smiley + Verdict + Optimise)
                        DeviceCareCard(
                            statusTitle = statusTitle,
                            statusSubtitle = statusSubtitle,
                            onOptimiseClicked = {
                                statusTitle = "Optimized"
                                statusSubtitle = "NX-SDS audit passed • RTC synced"
                                Toast.makeText(context, "Optimised watch state", Toast.LENGTH_SHORT).show()
                            }
                        )

                        Spacer(modifier = Modifier.height(36.dp))

                        // ZONE 2: Arkham Knight WayneTech 5-Hexagon Cluster
                        HexSubsystemHub(
                            onNodeSelected = { nodeKey ->
                                activeSubMenu = nodeKey // Open dedicated sub-menu view
                            }
                        )

                        Spacer(modifier = Modifier.height(40.dp))
                    }
                }
            }
        }
    }
}
