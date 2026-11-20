package com.nx.isd.uplink.ui.components

import androidx.compose.foundation.Canvas
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Add
import androidx.compose.material.icons.filled.Check
import androidx.compose.material.icons.filled.GridView
import androidx.compose.material3.*
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.nx.isd.uplink.ui.theme.*

/**
 * Tactical sidepanel drawer containing the registered hardware fleet,
 * device switcher, and pairing actions.
 */
@Composable
fun TacticalDeviceDrawer(
    selectedDevice: String?,
    onSelectDevice: (String) -> Unit,
    onNavigateFleet: () -> Unit,
    onConnectNewDevice: () -> Unit,
    onCloseDrawer: () -> Unit
) {
    ModalDrawerSheet(
        drawerContainerColor = OledBlack,
        drawerContentColor = TextPrimary,
        modifier = Modifier
            .fillMaxHeight()
            .width(320.dp)
            .border(width = 1.dp, color = AmberDim)
    ) {
        Column(
            modifier = Modifier
                .fillMaxSize()
                .statusBarsPadding()
                .padding(20.dp)
                .verticalScroll(rememberScrollState()),
            verticalArrangement = Arrangement.SpaceBetween
        ) {
            Column {
                // Header: NX-UPLINK title + branding
                Row(
                    modifier = Modifier.fillMaxWidth(),
                    horizontalArrangement = Arrangement.SpaceBetween,
                    verticalAlignment = Alignment.CenterVertically
                ) {
                    Column {
                        Text(
                            text = "NX-UPLINK",
                            fontFamily = CircuitForem,
                            fontSize = 24.sp,
                            fontWeight = FontWeight.Bold,
                            color = AmberBright,
                            letterSpacing = 2.sp
                        )
                        Text(
                            text = "HARDWARE FLEET BRIDGE // v1.0",
                            fontFamily = TypewriterFont,
                            fontSize = 10.sp,
                            color = TextSecondary
                        )
                    }

                    // Tactical reticle indicator
                    Canvas(modifier = Modifier.size(24.dp)) {
                        drawCircle(
                            color = AmberBright,
                            radius = 6.dp.toPx(),
                            style = Stroke(width = 1.5.dp.toPx())
                        )
                        drawCircle(
                            color = AmberBright,
                            radius = 2.dp.toPx()
                        )
                    }
                }

                Spacer(modifier = Modifier.height(20.dp))
                HorizontalDivider(color = AmberDim, thickness = 1.dp)
                Spacer(modifier = Modifier.height(20.dp))

                // Section 1: Device List
                Text(
                    text = "REGISTERED HARDWARE",
                    fontFamily = CircuitForem,
                    fontSize = 12.sp,
                    fontWeight = FontWeight.Bold,
                    color = TextSecondary,
                    letterSpacing = 1.5.sp
                )

                Spacer(modifier = Modifier.height(12.dp))

                // Device 1: NX-ISD (Tactical Watch)
                DrawerDeviceItem(
                    title = "NX-ISD",
                    subtitle = "Tactical Biometric Watch",
                    telemetry = "ESP32-S3 • 240x240 • 4x4 MATRIX",
                    badge = if (selectedDevice == "NX-ISD") "ACTIVE" else "SIMULATED",
                    badgeColor = if (selectedDevice == "NX-ISD") NominalGreen else AmberBright,
                    isSelected = selectedDevice == "NX-ISD",
                    onClick = {
                        onSelectDevice("NX-ISD")
                        onCloseDrawer()
                    }
                )

                Spacer(modifier = Modifier.height(12.dp))

                // Device 2: NX-Edge (M.2 Blade)
                DrawerDeviceItem(
                    title = "NX-Edge",
                    subtitle = "M.2 2260 Dev Blade",
                    telemetry = "BARE ESP32-S3 • DUAL RF • BNO085",
                    badge = "IN DEV",
                    badgeColor = AmberDim,
                    isSelected = selectedDevice == "NX-Edge",
                    onClick = {
                        onSelectDevice("NX-Edge")
                        onCloseDrawer()
                    }
                )

                Spacer(modifier = Modifier.height(24.dp))
                HorizontalDivider(color = AmberDim.copy(alpha = 0.5f), thickness = 1.dp)
                Spacer(modifier = Modifier.height(20.dp))

                // Section 2: Fleet Management Actions
                Text(
                    text = "FLEET ACTIONS",
                    fontFamily = CircuitForem,
                    fontSize = 12.sp,
                    fontWeight = FontWeight.Bold,
                    color = TextSecondary,
                    letterSpacing = 1.5.sp
                )

                Spacer(modifier = Modifier.height(12.dp))

                // Button: Start Screen / Fleet Overview
                DrawerActionButton(
                    icon = Icons.Default.GridView,
                    label = "DEVICE FLEET OVERVIEW",
                    sublabel = "Return to start screen",
                    onClick = {
                        onNavigateFleet()
                        onCloseDrawer()
                    }
                )

                Spacer(modifier = Modifier.height(10.dp))

                // Button: Connect / Pair Device
                DrawerActionButton(
                    icon = Icons.Default.Add,
                    label = "+ PAIR NEW HARDWARE",
                    sublabel = "BLE 2.4GHz GATT scanner",
                    onClick = {
                        onConnectNewDevice()
                        onCloseDrawer()
                    }
                )
            }

            // Footer Diagnostics
            Column(
                modifier = Modifier
                    .fillMaxWidth()
                    .padding(top = 24.dp)
            ) {
                HorizontalDivider(color = AmberDim.copy(alpha = 0.3f), thickness = 1.dp)
                Spacer(modifier = Modifier.height(12.dp))
                Text(
                    text = "GATT STACK: ARMED // MTU 512",
                    fontFamily = TypewriterFont,
                    fontSize = 10.sp,
                    color = TextDim
                )
                Text(
                    text = "SECURITY: AES-128 AUTH READY",
                    fontFamily = TypewriterFont,
                    fontSize = 10.sp,
                    color = TextDim
                )
            }
        }
    }
}

@Composable
private fun DrawerDeviceItem(
    title: String,
    subtitle: String,
    telemetry: String,
    badge: String,
    badgeColor: Color,
    isSelected: Boolean,
    onClick: () -> Unit
) {
    Box(
        modifier = Modifier
            .fillMaxWidth()
            .background(
                color = if (isSelected) TacticalSurfaceElevated else TacticalSurface,
                shape = RoundedCornerShape(6.dp)
            )
            .border(
                width = if (isSelected) 1.5.dp else 1.dp,
                color = if (isSelected) AmberBright else TacticalBorder,
                shape = RoundedCornerShape(6.dp)
            )
            .clickable { onClick() }
            .padding(14.dp)
    ) {
        Column(modifier = Modifier.fillMaxWidth()) {
            Row(
                modifier = Modifier.fillMaxWidth(),
                horizontalArrangement = Arrangement.SpaceBetween,
                verticalAlignment = Alignment.CenterVertically
            ) {
                Text(
                    text = title,
                    fontFamily = CircuitForem,
                    fontSize = 16.sp,
                    fontWeight = FontWeight.Bold,
                    color = if (isSelected) AmberBright else TextPrimary
                )

                Box(
                    modifier = Modifier
                        .background(badgeColor.copy(alpha = 0.15f), shape = RoundedCornerShape(3.dp))
                        .border(1.dp, badgeColor, shape = RoundedCornerShape(3.dp))
                        .padding(horizontal = 6.dp, vertical = 2.dp)
                ) {
                    Text(
                        text = badge,
                        fontFamily = TypewriterFont,
                        fontSize = 9.sp,
                        fontWeight = FontWeight.Bold,
                        color = badgeColor
                    )
                }
            }

            Spacer(modifier = Modifier.height(4.dp))

            Text(
                text = subtitle,
                fontFamily = TypewriterFont,
                fontSize = 11.sp,
                color = TextSecondary
            )

            Spacer(modifier = Modifier.height(4.dp))

            Text(
                text = telemetry,
                fontFamily = TypewriterFont,
                fontSize = 9.sp,
                color = TextDim
            )
        }
    }
}

@Composable
private fun DrawerActionButton(
    icon: androidx.compose.ui.graphics.vector.ImageVector,
    label: String,
    sublabel: String,
    onClick: () -> Unit
) {
    Box(
        modifier = Modifier
            .fillMaxWidth()
            .background(TacticalSurface, shape = RoundedCornerShape(6.dp))
            .border(1.dp, TacticalBorder, shape = RoundedCornerShape(6.dp))
            .clickable { onClick() }
            .padding(12.dp)
    ) {
        Row(
            modifier = Modifier.fillMaxWidth(),
            verticalAlignment = Alignment.CenterVertically
        ) {
            Icon(
                imageVector = icon,
                contentDescription = null,
                tint = AmberBright,
                modifier = Modifier.size(20.dp)
            )

            Spacer(modifier = Modifier.width(12.dp))

            Column {
                Text(
                    text = label,
                    fontFamily = CircuitForem,
                    fontSize = 13.sp,
                    fontWeight = FontWeight.Bold,
                    color = AmberBright
                )
                Text(
                    text = sublabel,
                    fontFamily = TypewriterFont,
                    fontSize = 10.sp,
                    color = TextDim
                )
            }
        }
    }
}
