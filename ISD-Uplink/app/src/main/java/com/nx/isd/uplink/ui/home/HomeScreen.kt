package com.nx.isd.uplink.ui.home

import android.widget.Toast
import androidx.activity.compose.BackHandler
import androidx.compose.animation.Crossfade
import androidx.compose.animation.core.tween
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.nx.isd.uplink.ui.components.TacticalBackgroundGrid
import com.nx.isd.uplink.ui.components.TacticalHamburgerButton
import com.nx.isd.uplink.ui.submenu.SubMenuScreen
import com.nx.isd.uplink.ui.theme.*

@Composable
fun HomeScreen(
    onOpenDrawer: () -> Unit = {},
    onNavigateFleet: () -> Unit = {}
) {
    val context = LocalContext.current
    var activeSubMenu by remember { mutableStateOf<String?>(null) }
    var statusTitle by remember { mutableStateOf("Good") }
    var statusSubtitle by remember { mutableStateOf("No battery or sensor issues") }

    // Intercept physical/gesture back button:
    // If in sub-menu -> exit to hub
    // If in hub -> exit to Device Fleet start screen
    BackHandler(enabled = true) {
        if (activeSubMenu != null) {
            activeSubMenu = null
        } else {
            onNavigateFleet()
        }
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
                        // TOP BAR: Hamburger button (3 lines) on left, Title center, Fleet switcher on right
                        Row(
                            modifier = Modifier
                                .fillMaxWidth()
                                .padding(vertical = 4.dp),
                            verticalAlignment = Alignment.CenterVertically,
                            horizontalArrangement = Arrangement.SpaceBetween
                        ) {
                            TacticalHamburgerButton(
                                onClick = onOpenDrawer
                            )

                            Text(
                                text = "NX-ISD",
                                fontFamily = CircuitForem,
                                fontSize = 20.sp,
                                fontWeight = FontWeight.Bold,
                                color = AmberBright,
                                letterSpacing = 2.sp
                            )

                            // Device switcher button
                            Box(
                                modifier = Modifier
                                    .background(TacticalSurfaceElevated, shape = RoundedCornerShape(8.dp))
                                    .border(1.dp, TacticalBorder, shape = RoundedCornerShape(8.dp))
                                    .clickable { onNavigateFleet() }
                                    .padding(horizontal = 12.dp, vertical = 6.dp)
                            ) {
                                Text(
                                    text = "Devices",
                                    fontFamily = TypewriterFont,
                                    fontSize = 12.sp,
                                    fontWeight = FontWeight.Medium,
                                    color = AmberBright
                                )
                            }
                        }

                        Spacer(modifier = Modifier.height(16.dp))

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
