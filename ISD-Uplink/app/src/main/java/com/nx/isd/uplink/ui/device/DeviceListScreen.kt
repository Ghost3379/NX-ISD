package com.nx.isd.uplink.ui.device

import android.widget.Toast
import androidx.compose.foundation.Canvas
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Add
import androidx.compose.material.icons.filled.ChevronRight
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.nx.isd.uplink.ui.components.TacticalBackgroundGrid
import com.nx.isd.uplink.ui.components.TacticalHamburgerButton
import com.nx.isd.uplink.ui.theme.*

/**
 * Clean device selection start screen.
 * Shows registered devices (NX-ISD watch simulation, NX-Edge blade) and a connect button.
 */
@Composable
fun DeviceListScreen(
    onOpenDrawer: () -> Unit,
    onSelectDevice: (String) -> Unit
) {
    val context = LocalContext.current
    var showConnectDialog by remember { mutableStateOf(false) }

    Box(
        modifier = Modifier
            .fillMaxSize()
            .background(OledBlack)
    ) {
        TacticalBackgroundGrid()

        Column(
            modifier = Modifier
                .fillMaxSize()
                .statusBarsPadding()
        ) {
            // Top Bar
            Row(
                modifier = Modifier
                    .fillMaxWidth()
                    .padding(horizontal = 8.dp, vertical = 6.dp),
                verticalAlignment = Alignment.CenterVertically
            ) {
                TacticalHamburgerButton(onClick = onOpenDrawer)
                Spacer(modifier = Modifier.width(8.dp))
                Text(
                    text = "NX-Uplink",
                    fontFamily = CircuitForem,
                    fontSize = 22.sp,
                    fontWeight = FontWeight.Bold,
                    color = AmberBright,
                    letterSpacing = 1.sp
                )
            }

            // Body
            Column(
                modifier = Modifier
                    .fillMaxSize()
                    .verticalScroll(rememberScrollState())
                    .padding(horizontal = 20.dp, vertical = 12.dp),
                verticalArrangement = Arrangement.spacedBy(16.dp)
            ) {
                Text(
                    text = "Devices",
                    fontFamily = CircuitForem,
                    fontSize = 26.sp,
                    fontWeight = FontWeight.Bold,
                    color = TextPrimary
                )

                // Device 1: NX-ISD
                CleanDeviceCard(
                    name = "NX-ISD",
                    type = "Smartwatch",
                    status = "Connected (Simulation)",
                    statusColor = NominalGreen,
                    batteryText = "94%",
                    isWatch = true,
                    isEnabled = true,
                    onClick = { onSelectDevice("NX-ISD") }
                )

                // Device 2: NX-Edge
                CleanDeviceCard(
                    name = "NX-Edge",
                    type = "Dev Blade",
                    status = "Coming soon",
                    statusColor = TextDim,
                    batteryText = null,
                    isWatch = false,
                    isEnabled = false,
                    onClick = {
                        Toast.makeText(context, "NX-Edge: Coming when hardware releases", Toast.LENGTH_SHORT).show()
                    }
                )

                Spacer(modifier = Modifier.height(8.dp))

                // Connect Device Button
                Button(
                    onClick = { showConnectDialog = true },
                    colors = ButtonDefaults.buttonColors(
                        containerColor = TacticalSurfaceElevated,
                        contentColor = AmberBright
                    ),
                    shape = RoundedCornerShape(12.dp),
                    modifier = Modifier
                        .fillMaxWidth()
                        .height(54.dp)
                        .border(1.dp, AmberMid.copy(alpha = 0.5f), RoundedCornerShape(12.dp))
                ) {
                    Icon(
                        imageVector = Icons.Default.Add,
                        contentDescription = null,
                        modifier = Modifier.size(20.dp)
                    )
                    Spacer(modifier = Modifier.width(8.dp))
                    Text(
                        text = "Connect a device",
                        fontFamily = TypewriterFont,
                        fontSize = 14.sp,
                        fontWeight = FontWeight.Bold
                    )
                }
            }
        }
    }

    if (showConnectDialog) {
        AlertDialog(
            onDismissRequest = { showConnectDialog = false },
            containerColor = TacticalSurfaceElevated,
            shape = RoundedCornerShape(16.dp),
            title = {
                Text(
                    text = "Connect a device",
                    fontFamily = CircuitForem,
                    fontSize = 20.sp,
                    color = AmberBright
                )
            },
            text = {
                Column(verticalArrangement = Arrangement.spacedBy(12.dp)) {
                    Text(
                        text = "Select a device to connect:",
                        fontFamily = TypewriterFont,
                        fontSize = 13.sp,
                        color = TextSecondary
                    )

                    // Option: Simulated NX-ISD
                    Row(
                        modifier = Modifier
                            .fillMaxWidth()
                            .clip(RoundedCornerShape(8.dp))
                            .background(OledBlack)
                            .clickable {
                                showConnectDialog = false
                                Toast.makeText(context, "Connected to NX-ISD", Toast.LENGTH_SHORT).show()
                                onSelectDevice("NX-ISD")
                            }
                            .padding(14.dp),
                        verticalAlignment = Alignment.CenterVertically,
                        horizontalArrangement = Arrangement.SpaceBetween
                    ) {
                        Column {
                            Text(
                                text = "NX-ISD (Simulation)",
                                fontFamily = CircuitForem,
                                fontSize = 15.sp,
                                color = AmberBright
                            )
                            Text(
                                text = "Ready to connect",
                                fontFamily = TypewriterFont,
                                fontSize = 11.sp,
                                color = NominalGreen
                            )
                        }
                        Icon(
                            imageVector = Icons.Default.ChevronRight,
                            contentDescription = null,
                            tint = AmberBright
                        )
                    }

                    // Option: NX-Edge
                    Row(
                        modifier = Modifier
                            .fillMaxWidth()
                            .clip(RoundedCornerShape(8.dp))
                            .background(OledBlack)
                            .clickable {
                                showConnectDialog = false
                                Toast.makeText(context, "NX-Edge hardware release pending", Toast.LENGTH_SHORT).show()
                            }
                            .padding(14.dp),
                        verticalAlignment = Alignment.CenterVertically,
                        horizontalArrangement = Arrangement.SpaceBetween
                    ) {
                        Column {
                            Text(
                                text = "NX-Edge",
                                fontFamily = CircuitForem,
                                fontSize = 15.sp,
                                color = TextSecondary
                            )
                            Text(
                                text = "Releasing soon",
                                fontFamily = TypewriterFont,
                                fontSize = 11.sp,
                                color = TextDim
                            )
                        }
                        Icon(
                            imageVector = Icons.Default.ChevronRight,
                            contentDescription = null,
                            tint = TextDim
                        )
                    }
                }
            },
            confirmButton = {
                TextButton(onClick = { showConnectDialog = false }) {
                    Text("Close", fontFamily = TypewriterFont, color = TextSecondary)
                }
            }
        )
    }
}

@Composable
private fun CleanDeviceCard(
    name: String,
    type: String,
    status: String,
    statusColor: Color,
    batteryText: String?,
    isWatch: Boolean,
    isEnabled: Boolean,
    onClick: () -> Unit
) {
    Box(
        modifier = Modifier
            .fillMaxWidth()
            .clip(RoundedCornerShape(16.dp))
            .background(TacticalSurfaceElevated)
            .border(
                width = if (isEnabled) 1.2.dp else 1.dp,
                color = if (isEnabled) AmberMid.copy(alpha = 0.6f) else TacticalBorder,
                shape = RoundedCornerShape(16.dp)
            )
            .clickable { onClick() }
            .padding(18.dp)
    ) {
        Row(
            modifier = Modifier.fillMaxWidth(),
            verticalAlignment = Alignment.CenterVertically
        ) {
            // Minimal device icon
            Box(
                modifier = Modifier
                    .size(52.dp)
                    .background(OledBlack, CircleShape)
                    .border(1.dp, if (isEnabled) AmberDim else TacticalBorder, CircleShape),
                contentAlignment = Alignment.Center
            ) {
                if (isWatch) {
                    Canvas(modifier = Modifier.size(28.dp)) {
                        val stroke = 2.dp.toPx()
                        val color = if (isEnabled) AmberBright else TextDim
                        // Round dial
                        drawCircle(color = color, radius = size.width * 0.38f, style = Stroke(stroke))
                        // Top strap hint
                        drawLine(color, Offset(size.width * 0.35f, 0f), Offset(size.width * 0.35f, size.height * 0.14f), stroke)
                        drawLine(color, Offset(size.width * 0.65f, 0f), Offset(size.width * 0.65f, size.height * 0.14f), stroke)
                        // Bottom strap hint
                        drawLine(color, Offset(size.width * 0.35f, size.height * 0.86f), Offset(size.width * 0.35f, size.height), stroke)
                        drawLine(color, Offset(size.width * 0.65f, size.height * 0.86f), Offset(size.width * 0.65f, size.height), stroke)
                    }
                } else {
                    Canvas(modifier = Modifier.size(26.dp)) {
                        val stroke = 2.dp.toPx()
                        val color = if (isEnabled) AmberBright else TextDim
                        // Blade rectangle
                        drawRect(
                            color = color,
                            topLeft = Offset(size.width * 0.15f, size.height * 0.1f),
                            size = androidx.compose.ui.geometry.Size(size.width * 0.7f, size.height * 0.8f),
                            style = Stroke(stroke)
                        )
                        // M.2 notch hint
                        drawLine(color, Offset(size.width * 0.35f, size.height * 0.9f), Offset(size.width * 0.35f, size.height), stroke)
                    }
                }
            }

            Spacer(modifier = Modifier.width(16.dp))

            // Details
            Column(modifier = Modifier.weight(1f)) {
                Row(
                    verticalAlignment = Alignment.CenterVertically,
                    horizontalArrangement = Arrangement.spacedBy(8.dp)
                ) {
                    Text(
                        text = name,
                        fontFamily = CircuitForem,
                        fontSize = 20.sp,
                        fontWeight = FontWeight.Bold,
                        color = if (isEnabled) AmberBright else TextPrimary
                    )
                    Text(
                        text = "• $type",
                        fontFamily = TypewriterFont,
                        fontSize = 12.sp,
                        color = TextSecondary
                    )
                }

                Spacer(modifier = Modifier.height(2.dp))

                Row(
                    verticalAlignment = Alignment.CenterVertically,
                    horizontalArrangement = Arrangement.spacedBy(6.dp)
                ) {
                    Canvas(modifier = Modifier.size(7.dp)) {
                        drawCircle(color = statusColor)
                    }
                    Text(
                        text = status,
                        fontFamily = TypewriterFont,
                        fontSize = 12.sp,
                        color = statusColor
                    )
                    if (batteryText != null) {
                        Text(
                            text = "• $batteryText",
                            fontFamily = TypewriterFont,
                            fontSize = 12.sp,
                            color = TextSecondary
                        )
                    }
                }
            }

            Icon(
                imageVector = Icons.Default.ChevronRight,
                contentDescription = null,
                tint = if (isEnabled) AmberBright else TextDim,
                modifier = Modifier.size(24.dp)
            )
        }
    }
}
