package com.nx.isd.uplink.ui.device

import android.widget.Toast
import androidx.compose.animation.core.*
import androidx.compose.foundation.Canvas
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
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.nx.isd.uplink.ui.components.ConnectDeviceDialog
import com.nx.isd.uplink.ui.components.TacticalBackgroundGrid
import com.nx.isd.uplink.ui.components.TacticalHamburgerButton
import com.nx.isd.uplink.ui.theme.*

/**
 * Start screen for NX-Uplink.
 * Displays registered hardware fleet (NX-ISD simulated watch, NX-Edge blade)
 * and allows pairing new devices or launching telemetry dashboards.
 */
@Composable
fun DeviceListScreen(
    onOpenDrawer: () -> Unit,
    onSelectDevice: (String) -> Unit
) {
    val context = LocalContext.current
    var showScanDialog by remember { mutableStateOf(false) }
    var showEdgeInfoDialog by remember { mutableStateOf(false) }

    // Pulsing green beacon animation
    val infiniteTransition = rememberInfiniteTransition(label = "BeaconPulse")
    val beaconAlpha by infiniteTransition.animateFloat(
        initialValue = 0.4f,
        targetValue = 1.0f,
        animationSpec = infiniteRepeatable(
            animation = tween(1000, easing = LinearEasing),
            repeatMode = RepeatMode.Reverse
        ),
        label = "BeaconAlpha"
    )

    Box(
        modifier = Modifier
            .fillMaxSize()
            .background(OledBlack)
    ) {
        // Tactical Background Grid
        TacticalBackgroundGrid()

        Column(
            modifier = Modifier
                .fillMaxSize()
                .statusBarsPadding()
        ) {
            // TOP BAR: Hamburger (3 lines) on the left, Title in the center, Status on the right
            Row(
                modifier = Modifier
                    .fillMaxWidth()
                    .padding(horizontal = 12.dp, vertical = 8.dp),
                verticalAlignment = Alignment.CenterVertically,
                horizontalArrangement = Arrangement.SpaceBetween
            ) {
                // Top-Left 3-line Hamburger Menu
                TacticalHamburgerButton(
                    onClick = onOpenDrawer
                )

                // Center Title
                Text(
                    text = "NX-UPLINK",
                    fontFamily = CircuitForem,
                    fontSize = 22.sp,
                    fontWeight = FontWeight.Bold,
                    color = AmberBright,
                    letterSpacing = 2.sp
                )

                // Top-Right Signal Badge
                Row(
                    verticalAlignment = Alignment.CenterVertically,
                    modifier = Modifier
                        .background(TacticalSurfaceElevated, shape = RoundedCornerShape(4.dp))
                        .border(1.dp, TacticalBorder, shape = RoundedCornerShape(4.dp))
                        .padding(horizontal = 8.dp, vertical = 4.dp)
                ) {
                    Canvas(modifier = Modifier.size(8.dp)) {
                        drawCircle(color = NominalGreen.copy(alpha = beaconAlpha))
                    }
                    Spacer(modifier = Modifier.width(6.dp))
                    Text(
                        text = "FLEET",
                        fontFamily = TypewriterFont,
                        fontSize = 10.sp,
                        fontWeight = FontWeight.Bold,
                        color = NominalGreen
                    )
                }
            }

            // Scrollable Content
            Column(
                modifier = Modifier
                    .fillMaxSize()
                    .verticalScroll(rememberScrollState())
                    .padding(horizontal = 16.dp, vertical = 8.dp),
                verticalArrangement = Arrangement.spacedBy(16.dp)
            ) {
                // Screen Header
                Column {
                    Text(
                        text = "HARDWARE FLEET",
                        fontFamily = CircuitForem,
                        fontSize = 24.sp,
                        fontWeight = FontWeight.Bold,
                        color = AmberBright,
                        letterSpacing = 1.5.sp
                    )
                    Text(
                        text = "Select a hardware node to stream telemetry or pair new systems.",
                        fontFamily = TypewriterFont,
                        fontSize = 12.sp,
                        color = TextSecondary
                    )
                }

                Spacer(modifier = Modifier.height(4.dp))

                // CARD 1: NX-ISD (Simulated Watch)
                DeviceCard(
                    nodeTag = "NODE 01 // WEARABLE TACTICAL WATCH",
                    title = "NX-ISD",
                    subtitle = "Biometric Field Watch & 4x4 Matrix",
                    specs = listOf(
                        "ESP32-S3-WROOM-1 dual-core 240MHz",
                        "GC9A01 240x240 Circular IPS + 4x4 NeoPixels",
                        "MAX17048 Fuel Gauge [94% NOMINAL]",
                        "RV-3028 Ultra-Low Power RTC & MAX30102 PPG"
                    ),
                    badgeText = "SIMULATION [ONLINE]",
                    badgeColor = NominalGreen,
                    actionText = "LAUNCH DASHBOARD ->",
                    isAvailable = true,
                    onClick = {
                        onSelectDevice("NX-ISD")
                    }
                )

                // CARD 2: NX-Edge (M.2 Blade)
                DeviceCard(
                    nodeTag = "NODE 02 // M.2 2260 AUTONOMOUS BLADE",
                    title = "NX-Edge",
                    subtitle = "M.2 2260 Key B Autonomous Edge Blade",
                    specs = listOf(
                        "Bare ESP32-S3 IC rotated 45°",
                        "Dual RF (Ignion Chip + U.FL external)",
                        "BNO085 9-DOF IMU + RV-3028 RTC",
                        "MAX98357A I2S Class-D Amp + 4-bit SDMMC"
                    ),
                    badgeText = "IN DEVELOPMENT",
                    badgeColor = AmberMid,
                    actionText = "VIEW HARDWARE SPECS",
                    isAvailable = false,
                    onClick = {
                        showEdgeInfoDialog = true
                    }
                )

                // CARD 3: + PAIR NEW HARDWARE
                Box(
                    modifier = Modifier
                        .fillMaxWidth()
                        .background(TacticalSurface, shape = RoundedCornerShape(8.dp))
                        .border(
                            width = 1.5.dp,
                            color = AmberDim,
                            shape = RoundedCornerShape(8.dp)
                        )
                        .clickable { showScanDialog = true }
                        .padding(20.dp),
                    contentAlignment = Alignment.Center
                ) {
                    Column(
                        horizontalAlignment = Alignment.CenterHorizontally,
                        verticalArrangement = Arrangement.spacedBy(6.dp)
                    ) {
                        Text(
                            text = "+ PAIR NEW HARDWARE",
                            fontFamily = CircuitForem,
                            fontSize = 16.sp,
                            fontWeight = FontWeight.Bold,
                            color = AmberBright,
                            letterSpacing = 1.sp
                        )
                        Text(
                            text = "Scan 2.4GHz BLE GATT advertising beacons",
                            fontFamily = TypewriterFont,
                            fontSize = 11.sp,
                            color = TextDim
                        )
                    }
                }

                Spacer(modifier = Modifier.height(24.dp))
            }
        }
    }

    // Connect / Scan Dialog
    if (showScanDialog) {
        ConnectDeviceDialog(
            onDismiss = { showScanDialog = false },
            onSelectDevice = { device ->
                showScanDialog = false
                if (device == "NX-ISD") {
                    Toast.makeText(context, "Bridged to NX-ISD Simulator", Toast.LENGTH_SHORT).show()
                    onSelectDevice("NX-ISD")
                } else {
                    showEdgeInfoDialog = true
                }
            }
        )
    }

    // NX-Edge Preview / Info Dialog
    if (showEdgeInfoDialog) {
        AlertDialog(
            onDismissRequest = { showEdgeInfoDialog = false },
            containerColor = TacticalSurfaceElevated,
            titleContentColor = AmberBright,
            textContentColor = TextPrimary,
            title = {
                Text(
                    text = "NX-EDGE DEV BLADE",
                    fontFamily = CircuitForem,
                    fontSize = 18.sp,
                    fontWeight = FontWeight.Bold
                )
            },
            text = {
                Column(verticalArrangement = Arrangement.spacedBy(8.dp)) {
                    Text(
                        text = "STATUS: PCB LAYOUT VERIFIED (6-LAYER)",
                        fontFamily = TypewriterFont,
                        fontSize = 12.sp,
                        fontWeight = FontWeight.Bold,
                        color = AmberMid
                    )
                    Text(
                        text = "The NX-Edge M.2 2260 dev blade features a bare ESP32-S3 angled at 45°, dual RF front-end, BNO085 IMU, and microSD storage. Firmware telemetry bridge will activate upon hardware release.",
                        fontFamily = TypewriterFont,
                        fontSize = 11.sp,
                        color = TextSecondary
                    )
                }
            },
            confirmButton = {
                TextButton(onClick = { showEdgeInfoDialog = false }) {
                    Text(
                        text = "ACKNOWLEDGE",
                        fontFamily = CircuitForem,
                        color = AmberBright
                    )
                }
            }
        )
    }
}

@Composable
private fun DeviceCard(
    nodeTag: String,
    title: String,
    subtitle: String,
    specs: List<String>,
    badgeText: String,
    badgeColor: Color,
    actionText: String,
    isAvailable: Boolean,
    onClick: () -> Unit
) {
    Box(
        modifier = Modifier
            .fillMaxWidth()
            .clip(RoundedCornerShape(8.dp))
            .background(TacticalSurfaceElevated)
            .border(
                width = if (isAvailable) 1.5.dp else 1.dp,
                color = if (isAvailable) AmberBright else TacticalBorder,
                shape = RoundedCornerShape(8.dp)
            )
            .clickable { onClick() }
            .padding(18.dp)
    ) {
        Column(
            modifier = Modifier.fillMaxWidth(),
            verticalArrangement = Arrangement.spacedBy(10.dp)
        ) {
            // Top Row: Tag + Status Badge
            Row(
                modifier = Modifier.fillMaxWidth(),
                horizontalArrangement = Arrangement.SpaceBetween,
                verticalAlignment = Alignment.CenterVertically
            ) {
                Text(
                    text = nodeTag,
                    fontFamily = TypewriterFont,
                    fontSize = 10.sp,
                    fontWeight = FontWeight.Bold,
                    color = TextDim
                )

                Box(
                    modifier = Modifier
                        .background(badgeColor.copy(alpha = 0.15f), shape = RoundedCornerShape(4.dp))
                        .border(1.dp, badgeColor, shape = RoundedCornerShape(4.dp))
                        .padding(horizontal = 8.dp, vertical = 3.dp)
                ) {
                    Text(
                        text = badgeText,
                        fontFamily = TypewriterFont,
                        fontSize = 10.sp,
                        fontWeight = FontWeight.Bold,
                        color = badgeColor
                    )
                }
            }

            // Title & Subtitle
            Column {
                Text(
                    text = title,
                    fontFamily = CircuitForem,
                    fontSize = 28.sp,
                    fontWeight = FontWeight.Bold,
                    color = if (isAvailable) AmberBright else TextPrimary,
                    letterSpacing = 1.sp
                )
                Text(
                    text = subtitle,
                    fontFamily = TypewriterFont,
                    fontSize = 12.sp,
                    color = TextSecondary
                )
            }

            // Hardware Specs List
            Column(
                verticalArrangement = Arrangement.spacedBy(3.dp),
                modifier = Modifier
                    .fillMaxWidth()
                    .background(OledBlack, shape = RoundedCornerShape(4.dp))
                    .padding(10.dp)
            ) {
                for (spec in specs) {
                    Text(
                        text = "• $spec",
                        fontFamily = TypewriterFont,
                        fontSize = 11.sp,
                        color = if (isAvailable) TextPrimary else TextDim
                    )
                }
            }

            // Action Button
            Box(
                modifier = Modifier
                    .fillMaxWidth()
                    .background(
                        if (isAvailable) AmberBright.copy(alpha = 0.15f) else TacticalSurface,
                        shape = RoundedCornerShape(4.dp)
                    )
                    .border(
                        1.dp,
                        if (isAvailable) AmberBright else TacticalBorder,
                        shape = RoundedCornerShape(4.dp)
                    )
                    .padding(vertical = 10.dp),
                contentAlignment = Alignment.Center
            ) {
                Text(
                    text = actionText,
                    fontFamily = CircuitForem,
                    fontSize = 13.sp,
                    fontWeight = FontWeight.Bold,
                    color = if (isAvailable) AmberBright else TextSecondary,
                    letterSpacing = 1.sp
                )
            }
        }
    }
}
