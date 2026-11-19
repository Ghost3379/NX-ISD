package com.nx.isd.uplink.ui.submenu

import android.widget.Toast
import androidx.compose.foundation.border
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.automirrored.filled.ArrowBack
import androidx.compose.material.icons.filled.*
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.vector.ImageVector
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.nx.isd.uplink.ui.components.TacticalBackgroundGrid
import com.nx.isd.uplink.ui.theme.*

@Composable
fun SubMenuScreen(
    nodeId: String,
    onBackClicked: () -> Unit
) {
    val context = LocalContext.current

    Box(
        modifier = Modifier
            .fillMaxSize()
            .statusBarsPadding()
    ) {
        // Background Tactical Grid
        TacticalBackgroundGrid()

        Column(
            modifier = Modifier
                .fillMaxSize()
                .padding(horizontal = 16.dp)
                .verticalScroll(rememberScrollState())
        ) {
            Spacer(modifier = Modifier.height(16.dp))

            // Sub-Menu Navigation Header
            Row(
                modifier = Modifier.fillMaxWidth(),
                verticalAlignment = Alignment.CenterVertically,
                horizontalArrangement = Arrangement.SpaceBetween
            ) {
                Row(verticalAlignment = Alignment.CenterVertically) {
                    IconButton(
                        onClick = onBackClicked,
                        modifier = Modifier.size(36.dp)
                    ) {
                        Icon(
                            imageVector = Icons.AutoMirrored.Filled.ArrowBack,
                            contentDescription = "Back",
                            tint = AmberBright
                        )
                    }
                    Spacer(modifier = Modifier.width(8.dp))
                    Text(
                        text = when (nodeId) {
                            "FIELD_MANUAL" -> "FIELD MANUAL"
                            "GENERAL" -> "SYSTEM & GENERAL"
                            "PERIPHERIE" -> "PERIPHERIE // SDS"
                            "NOTIFICATIONS" -> "NOTIFICATION MATRIX"
                            "HEALTH" -> "HEALTH & VITALS"
                            else -> nodeId
                        },
                        color = TextPrimary,
                        fontSize = 18.sp,
                        fontWeight = FontWeight.Bold,
                        fontFamily = CircuitForem,
                        letterSpacing = 1.5.sp
                    )
                }

                Surface(
                    color = TacticalSurfaceElevated,
                    shape = RoundedCornerShape(6.dp),
                    modifier = Modifier.border(1.dp, AmberMid, RoundedCornerShape(6.dp))
                ) {
                    Text(
                        text = "ONLINE",
                        color = AmberBright,
                        fontSize = 10.sp,
                        fontWeight = FontWeight.Bold,
                        fontFamily = FontFamily.Monospace,
                        modifier = Modifier.padding(horizontal = 8.dp, vertical = 4.dp)
                    )
                }
            }

            Spacer(modifier = Modifier.height(24.dp))

            // Dynamic Content Per Node
            when (nodeId) {
                "FIELD_MANUAL" -> FieldManualContent()
                "GENERAL" -> GeneralContent(context)
                "PERIPHERIE" -> PeripherieContent(context)
                "NOTIFICATIONS" -> NotificationsContent()
                "HEALTH" -> HealthContent()
                else -> {
                    Text(
                        text = "Subsystem node initialized.",
                        color = TextSecondary,
                        fontFamily = FontFamily.Monospace
                    )
                }
            }

            Spacer(modifier = Modifier.height(32.dp))
        }
    }
}

@Composable
fun FieldManualContent() {
    SubMenuCard(title = "PHYSICAL CONTROLS", icon = Icons.Default.Tune) {
        Text("• 3-Way Lever (Left/Right): Cycle application decks", color = TextPrimary, fontSize = 13.sp, fontFamily = TypewriterFont)
        Text("• 3-Way Lever (Push): Select / Confirm deck action", color = TextPrimary, fontSize = 13.sp, fontFamily = TypewriterFont)
        Text("• Back Button: Return to primary HUD watchface", color = TextPrimary, fontSize = 13.sp, fontFamily = TypewriterFont)
    }

    Spacer(modifier = Modifier.height(16.dp))

    SubMenuCard(title = "ESTIMATED RUNTIMES", icon = Icons.Default.BatteryChargingFull) {
        Text("• Active Sensing Deck: ~14.2 Hours (100Hz PPG)", color = TextPrimary, fontSize = 13.sp, fontFamily = TypewriterFont)
        Text("• Standby Watchface HUD: ~28.0 Hours", color = TextPrimary, fontSize = 13.sp, fontFamily = TypewriterFont)
        Text("• Deep Sleep + RTC Backup: ~120+ Hours (45nA)", color = TextPrimary, fontSize = 13.sp, fontFamily = TypewriterFont)
    }

    Spacer(modifier = Modifier.height(16.dp))

    SubMenuCard(title = "HARDWARE PLATFORM", icon = Icons.Default.DeveloperBoard) {
        Text("• Revision: NX-ISD v1.4 (CPH3225A Supercap)", color = TextSecondary, fontSize = 12.sp, fontFamily = TypewriterFont)
        Text("• Display Bus: 40 MHz SPI Direct 3.3V (ST7789)", color = TextSecondary, fontSize = 12.sp, fontFamily = TypewriterFont)
        Text("• Mass Storage: 4-Bit Hardware SDMMC (Molex)", color = TextSecondary, fontSize = 12.sp, fontFamily = TypewriterFont)
    }
}

@Composable
fun GeneralContent(context: android.content.Context) {
    SubMenuCard(title = "HARDWARE PROVENANCE", icon = Icons.Default.QrCodeScanner) {
        Text("Scan the 5x5mm 2D DataMatrix code on PCB bottom to register board serial and revision.", color = TextSecondary, fontSize = 12.sp, fontFamily = TypewriterFont)
        Spacer(modifier = Modifier.height(10.dp))
        Button(
            onClick = { Toast.makeText(context, "Camera scanner ready", Toast.LENGTH_SHORT).show() },
            colors = ButtonDefaults.buttonColors(containerColor = AmberBright, contentColor = OledBlack),
            shape = RoundedCornerShape(8.dp)
        ) {
            Text("SCAN PCB CODE", fontWeight = FontWeight.Bold, fontFamily = CircuitForem)
        }
    }

    Spacer(modifier = Modifier.height(16.dp))

    SubMenuCard(title = "POWER & BATTERY (MAX17048)", icon = Icons.Default.Bolt) {
        Text("• Cell Voltage: 3.94 V", color = TextPrimary, fontSize = 13.sp, fontFamily = TypewriterFont)
        Text("• State of Charge: 82.4%", color = TextPrimary, fontSize = 13.sp, fontFamily = TypewriterFont)
        Text("• Charge Current: 600 mA (BQ25170 Active)", color = TextSecondary, fontSize = 12.sp, fontFamily = TypewriterFont)
    }

    Spacer(modifier = Modifier.height(16.dp))

    SubMenuCard(title = "STORAGE EXPLORER (MICROSD)", icon = Icons.Default.SdCard) {
        Text("Capacity: 14.8 GB Free / 16.0 GB Total", color = TextPrimary, fontSize = 13.sp, fontFamily = TypewriterFont)
        Text("Active Logs: /sds/audit_log.txt", color = TextSecondary, fontSize = 12.sp, fontFamily = TypewriterFont)
    }
}

@Composable
fun PeripherieContent(context: android.content.Context) {
    SubMenuCard(title = "NX-SDS SILICON AUDIT", icon = Icons.Default.Memory) {
        Text("Run in-situ self-diagnostic across all I2C/SPI buses.", color = TextSecondary, fontSize = 12.sp, fontFamily = TypewriterFont)
        Spacer(modifier = Modifier.height(10.dp))
        Button(
            onClick = { Toast.makeText(context, "Executing NX-SDS Audit...", Toast.LENGTH_SHORT).show() },
            colors = ButtonDefaults.buttonColors(containerColor = AmberBright, contentColor = OledBlack),
            shape = RoundedCornerShape(8.dp)
        ) {
            Text("RUN SILICON AUDIT", fontWeight = FontWeight.Bold, fontFamily = CircuitForem)
        }
    }

    Spacer(modifier = Modifier.height(16.dp))

    SubMenuCard(title = "RTC TIMEKEEPER (RV-3028)", icon = Icons.Default.AccessTime) {
        Text("Synchronize high-accuracy RTC to phone time.", color = TextSecondary, fontSize = 12.sp, fontFamily = TypewriterFont)
        Spacer(modifier = Modifier.height(10.dp))
        Button(
            onClick = { Toast.makeText(context, "RTC Synced to Phone Time", Toast.LENGTH_SHORT).show() },
            colors = ButtonDefaults.buttonColors(containerColor = AmberBright, contentColor = OledBlack),
            shape = RoundedCornerShape(8.dp)
        ) {
            Text("SYNC RTC CLOCK", fontWeight = FontWeight.Bold, fontFamily = CircuitForem)
        }
    }

    Spacer(modifier = Modifier.height(16.dp))

    SubMenuCard(title = "4x4 NEOPIXEL MATRIX", icon = Icons.Default.Lightbulb) {
        Text("Trigger hardware patterns on the 16-LED array.", color = TextSecondary, fontSize = 12.sp, fontFamily = TypewriterFont)
        Spacer(modifier = Modifier.height(10.dp))
        Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            Button(
                onClick = { Toast.makeText(context, "Pattern: RADAR_SWEEP", Toast.LENGTH_SHORT).show() },
                colors = ButtonDefaults.buttonColors(containerColor = TacticalSurfaceElevated, contentColor = AmberBright),
                shape = RoundedCornerShape(8.dp),
                modifier = Modifier.border(1.dp, AmberMid, RoundedCornerShape(8.dp))
            ) {
                Text("SWEEP", fontSize = 11.sp, fontFamily = CircuitForem)
            }
            Button(
                onClick = { Toast.makeText(context, "Pattern: PULSE_BEAT", Toast.LENGTH_SHORT).show() },
                colors = ButtonDefaults.buttonColors(containerColor = TacticalSurfaceElevated, contentColor = AmberBright),
                shape = RoundedCornerShape(8.dp),
                modifier = Modifier.border(1.dp, AmberMid, RoundedCornerShape(8.dp))
            ) {
                Text("PULSE", fontSize = 11.sp, fontFamily = CircuitForem)
            }
        }
    }
}

@Composable
fun NotificationsContent() {
    var allowBuzzer by remember { mutableStateOf(true) }
    var allowMatrix by remember { mutableStateOf(true) }

    SubMenuCard(title = "HARDWARE ALERT RULES", icon = Icons.Default.NotificationsActive) {
        Row(
            modifier = Modifier.fillMaxWidth(),
            horizontalArrangement = Arrangement.SpaceBetween,
            verticalAlignment = Alignment.CenterVertically
        ) {
            Text("Acoustic Buzzer Alert", color = TextPrimary, fontSize = 13.sp, fontFamily = TypewriterFont)
            Switch(
                checked = allowBuzzer,
                onCheckedChange = { allowBuzzer = it },
                colors = SwitchDefaults.colors(checkedThumbColor = AmberBright, checkedTrackColor = AmberDim)
            )
        }
        Spacer(modifier = Modifier.height(8.dp))
        Row(
            modifier = Modifier.fillMaxWidth(),
            horizontalArrangement = Arrangement.SpaceBetween,
            verticalAlignment = Alignment.CenterVertically
        ) {
            Text("4x4 Matrix Visual Pulse", color = TextPrimary, fontSize = 13.sp, fontFamily = TypewriterFont)
            Switch(
                checked = allowMatrix,
                onCheckedChange = { allowMatrix = it },
                colors = SwitchDefaults.colors(checkedThumbColor = AmberBright, checkedTrackColor = AmberDim)
            )
        }
    }
}

@Composable
fun HealthContent() {
    SubMenuCard(title = "MAX30102 OPTICAL PPG", icon = Icons.Default.Favorite) {
        Text("• Heart Rate: 72 BPM (Calibrated Spot)", color = TextPrimary, fontSize = 13.sp, fontFamily = TypewriterFont)
        Text("• Oxygenation: 98% SpO2", color = TextPrimary, fontSize = 13.sp, fontFamily = TypewriterFont)
        Text("• RMSSD Stress Score: 24 / 100 (Optimal)", color = TextPrimary, fontSize = 13.sp, fontFamily = TypewriterFont)
    }

    Spacer(modifier = Modifier.height(16.dp))

    SubMenuCard(title = "MEASUREMENT SCHEDULE", icon = Icons.Default.HourglassBottom) {
        Text("Scheduled measurement reminders: Every 1 Hour", color = TextSecondary, fontSize = 12.sp, fontFamily = TypewriterFont)
    }
}

@Composable
fun SubMenuCard(
    title: String,
    icon: ImageVector,
    content: @Composable ColumnScope.() -> Unit
) {
    Card(
        modifier = Modifier
            .fillMaxWidth()
            .border(1.dp, TacticalBorderBright, RoundedCornerShape(12.dp)),
        colors = CardDefaults.cardColors(containerColor = TacticalSurfaceElevated),
        shape = RoundedCornerShape(12.dp)
    ) {
        Column(
            modifier = Modifier
                .fillMaxWidth()
                .padding(16.dp)
        ) {
            Row(verticalAlignment = Alignment.CenterVertically) {
                Icon(
                    imageVector = icon,
                    contentDescription = title,
                    tint = AmberBright,
                    modifier = Modifier.size(18.dp)
                )
                Spacer(modifier = Modifier.width(8.dp))
                Text(
                    text = title,
                    color = AmberBright,
                    fontSize = 13.sp,
                    fontWeight = FontWeight.Bold,
                    fontFamily = CircuitForem,
                    letterSpacing = 1.5.sp
                )
            }
            Spacer(modifier = Modifier.height(12.dp))
            content()
        }
    }
}
