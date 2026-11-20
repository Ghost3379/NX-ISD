package com.nx.isd.uplink.ui.components

import androidx.compose.animation.core.*
import androidx.compose.foundation.Canvas
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.Text
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.compose.ui.window.Dialog
import com.nx.isd.uplink.ui.theme.*
import kotlin.math.cos
import kotlin.math.sin

/**
 * Tactical BLE discovery modal dialog.
 * Scans 2.4GHz channels and lets the user connect to NX-ISD (Simulation) or detect NX-Edge.
 */
@Composable
fun ConnectDeviceDialog(
    onDismiss: () -> Unit,
    onSelectDevice: (String) -> Unit
) {
    val infiniteTransition = rememberInfiniteTransition(label = "RadarSweep")
    val angle by infiniteTransition.animateFloat(
        initialValue = 0f,
        targetValue = 360f,
        animationSpec = infiniteRepeatable(
            animation = tween(2200, easing = LinearEasing)
        ),
        label = "RadarAngle"
    )

    Dialog(onDismissRequest = onDismiss) {
        Box(
            modifier = Modifier
                .fillMaxWidth()
                .background(TacticalSurfaceElevated, shape = RoundedCornerShape(8.dp))
                .border(1.5.dp, AmberMid, shape = RoundedCornerShape(8.dp))
                .padding(20.dp)
        ) {
            Column(
                horizontalAlignment = Alignment.CenterHorizontally,
                verticalArrangement = Arrangement.spacedBy(16.dp)
            ) {
                // Header
                Text(
                    text = "PERIPHERAL SCANNER",
                    fontFamily = CircuitForem,
                    fontSize = 18.sp,
                    fontWeight = FontWeight.Bold,
                    color = AmberBright,
                    letterSpacing = 1.5.sp
                )

                // Animated Radar Reticle
                Box(
                    modifier = Modifier.size(100.dp),
                    contentAlignment = Alignment.Center
                ) {
                    Canvas(modifier = Modifier.fillMaxSize()) {
                        val center = Offset(size.width / 2, size.height / 2)
                        val radius = size.width / 2

                        // Outer ring
                        drawCircle(
                            color = AmberDim,
                            radius = radius,
                            style = Stroke(width = 1.5.dp.toPx())
                        )
                        // Inner ring
                        drawCircle(
                            color = AmberDim.copy(alpha = 0.5f),
                            radius = radius * 0.5f,
                            style = Stroke(width = 1.dp.toPx())
                        )
                        // Crosshairs
                        drawLine(
                            color = AmberDim.copy(alpha = 0.4f),
                            start = Offset(center.x, 0f),
                            end = Offset(center.x, size.height),
                            strokeWidth = 1.dp.toPx()
                        )
                        drawLine(
                            color = AmberDim.copy(alpha = 0.4f),
                            start = Offset(0f, center.y),
                            end = Offset(size.width, center.y),
                            strokeWidth = 1.dp.toPx()
                        )

                        // Rotating Radar Line
                        val rad = Math.toRadians(angle.toDouble())
                        val endX = center.x + radius * cos(rad).toFloat()
                        val endY = center.y + radius * sin(rad).toFloat()
                        drawLine(
                            color = AmberBright,
                            start = center,
                            end = Offset(endX, endY),
                            strokeWidth = 2.dp.toPx()
                        )
                    }
                }

                Text(
                    text = "SCANNING 2.4GHz GATT SPECTRUM...",
                    fontFamily = TypewriterFont,
                    fontSize = 11.sp,
                    color = TextSecondary
                )

                // Discovered Devices List
                Column(
                    modifier = Modifier.fillMaxWidth(),
                    verticalArrangement = Arrangement.spacedBy(10.dp)
                ) {
                    // Device Item 1: Simulated NX-ISD
                    DiscoveredDeviceItem(
                        name = "NX-ISD [SIMULATOR]",
                        mac = "SIM:E4:65:B8:21:40",
                        rssi = "-36 dBm",
                        badge = "READY TO BRIDGE",
                        badgeColor = NominalGreen,
                        onClick = {
                            onSelectDevice("NX-ISD")
                            onDismiss()
                        }
                    )

                    // Device Item 2: NX-Edge Blade
                    DiscoveredDeviceItem(
                        name = "NX-EDGE [BLADE TESTBENCH]",
                        mac = "SIM:30:AE:A4:77:92",
                        rssi = "-68 dBm",
                        badge = "DEV STANDBY",
                        badgeColor = AmberMid,
                        onClick = {
                            onSelectDevice("NX-Edge")
                            onDismiss()
                        }
                    )
                }

                Spacer(modifier = Modifier.height(4.dp))

                // Cancel Button
                Box(
                    modifier = Modifier
                        .fillMaxWidth()
                        .background(OledBlack, shape = RoundedCornerShape(4.dp))
                        .border(1.dp, TacticalBorder, shape = RoundedCornerShape(4.dp))
                        .clickable { onDismiss() }
                        .padding(vertical = 10.dp),
                    contentAlignment = Alignment.Center
                ) {
                    Text(
                        text = "ABORT SCAN",
                        fontFamily = TypewriterFont,
                        fontSize = 12.sp,
                        fontWeight = FontWeight.Bold,
                        color = TextDim
                    )
                }
            }
        }
    }
}

@Composable
private fun DiscoveredDeviceItem(
    name: String,
    mac: String,
    rssi: String,
    badge: String,
    badgeColor: Color,
    onClick: () -> Unit
) {
    Box(
        modifier = Modifier
            .fillMaxWidth()
            .background(OledBlack, shape = RoundedCornerShape(6.dp))
            .border(1.dp, AmberMid.copy(alpha = 0.5f), shape = RoundedCornerShape(6.dp))
            .clickable { onClick() }
            .padding(12.dp)
    ) {
        Row(
            modifier = Modifier.fillMaxWidth(),
            horizontalArrangement = Arrangement.SpaceBetween,
            verticalAlignment = Alignment.CenterVertically
        ) {
            Column(modifier = Modifier.weight(1f)) {
                Text(
                    text = name,
                    fontFamily = CircuitForem,
                    fontSize = 14.sp,
                    fontWeight = FontWeight.Bold,
                    color = AmberBright
                )
                Text(
                    text = "$mac • RSSI $rssi",
                    fontFamily = TypewriterFont,
                    fontSize = 11.sp,
                    color = TextDim
                )
            }

            Box(
                modifier = Modifier
                    .background(badgeColor.copy(alpha = 0.15f), shape = RoundedCornerShape(4.dp))
                    .border(1.dp, badgeColor, shape = RoundedCornerShape(4.dp))
                    .padding(horizontal = 8.dp, vertical = 4.dp)
            ) {
                Text(
                    text = badge,
                    fontFamily = TypewriterFont,
                    fontSize = 10.sp,
                    fontWeight = FontWeight.Bold,
                    color = badgeColor
                )
            }
        }
    }
}
