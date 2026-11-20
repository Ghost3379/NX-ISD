package com.nx.isd.uplink.ui.splash

import androidx.compose.animation.core.*
import androidx.compose.foundation.Canvas
import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.*
import androidx.compose.material3.Text
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.nx.isd.uplink.ui.theme.*
import kotlinx.coroutines.delay

@Composable
fun SplashScreen(
    onSplashComplete: () -> Unit
) {
    var progress by remember { mutableStateOf(0f) }
    var bootStepText by remember { mutableStateOf("INITIALIZING CORE...") }

    // Pulsing reticle animation
    val infiniteTransition = rememberInfiniteTransition(label = "ReticlePulse")
    val pulseAlpha by infiniteTransition.animateFloat(
        initialValue = 0.3f,
        targetValue = 1.0f,
        animationSpec = infiniteRepeatable(
            animation = tween(800, easing = LinearEasing),
            repeatMode = RepeatMode.Reverse
        ),
        label = "PulseAlpha"
    )

    LaunchedEffect(Unit) {
        val steps = listOf(
            0.15f to "DISCOVERING LOCAL STACK...",
            0.35f to "CALIBRATING GATT PROTOCOL ENGINE...",
            0.60f to "VERIFYING CRYPTO & REGISTRY STATE...",
            0.85f to "PREPARING NX-SDS TELEMETRY LINK...",
            1.00f to "ALL SUBSYSTEMS NOMINAL"
        )
        for ((targetProg, text) in steps) {
            bootStepText = text
            while (progress < targetProg) {
                progress = (progress + 0.05f).coerceAtMost(targetProg)
                delay(35)
            }
            delay(120)
        }
        delay(350)
        onSplashComplete()
    }

    Box(
        modifier = Modifier
            .fillMaxSize()
            .background(OledBlack)
            .clickable { onSplashComplete() }
            .padding(24.dp),
        contentAlignment = Alignment.Center
    ) {
        // Background Tactical Crosshairs & Grid Lines
        Canvas(modifier = Modifier.fillMaxSize()) {
            val w = size.width
            val h = size.height

            // Corner decorative brackets
            val bracketLen = 32.dp.toPx()
            val bracketStroke = 1.5.dp.toPx()
            val bracketColor = AmberDim.copy(alpha = 0.6f)

            // Top-Left
            drawLine(bracketColor, Offset(0f, 0f), Offset(bracketLen, 0f), bracketStroke)
            drawLine(bracketColor, Offset(0f, 0f), Offset(0f, bracketLen), bracketStroke)

            // Top-Right
            drawLine(bracketColor, Offset(w - bracketLen, 0f), Offset(w, 0f), bracketStroke)
            drawLine(bracketColor, Offset(w, 0f), Offset(w, bracketLen), bracketStroke)

            // Bottom-Left
            drawLine(bracketColor, Offset(0f, h), Offset(bracketLen, h), bracketStroke)
            drawLine(bracketColor, Offset(0f, h), Offset(0f, h - bracketLen), bracketStroke)

            // Bottom-Right
            drawLine(bracketColor, Offset(w - bracketLen, h), Offset(w, h), bracketStroke)
            drawLine(bracketColor, Offset(w, h), Offset(w, h - bracketLen), bracketStroke)

            // Subtle center radar ring
            drawCircle(
                color = AmberMid.copy(alpha = 0.15f * pulseAlpha),
                radius = 120.dp.toPx(),
                style = Stroke(width = 1.dp.toPx())
            )
        }

        Column(
            horizontalAlignment = Alignment.CenterHorizontally,
            verticalArrangement = Arrangement.Center
        ) {
            // Tactical Title Header (Circuit Forem)
            Text(
                text = "NX-UPLINK",
                color = AmberBright,
                fontSize = 42.sp,
                fontWeight = FontWeight.Bold,
                fontFamily = CircuitForem,
                letterSpacing = 4.sp
            )
            Text(
                text = "COMMAND UPLINK // HARDWARE BRIDGE",
                color = TextSecondary,
                fontSize = 11.sp,
                fontWeight = FontWeight.Bold,
                fontFamily = CircuitForem,
                letterSpacing = 2.sp
            )

            Spacer(modifier = Modifier.height(48.dp))

            // Diagnostic Segmented Progress Bar
            Box(
                modifier = Modifier
                    .fillMaxWidth(0.85f)
                    .height(8.dp)
                    .background(TacticalSurfaceElevated)
            ) {
                Box(
                    modifier = Modifier
                        .fillMaxWidth(progress)
                        .fillMaxHeight()
                        .background(AmberBright)
                )
            }

            Spacer(modifier = Modifier.height(16.dp))

            // Telemetry Ticker Text
            Text(
                text = bootStepText,
                color = TextPrimary,
                fontSize = 12.sp,
                fontFamily = FontFamily.Monospace,
                fontWeight = FontWeight.Medium,
                textAlign = TextAlign.Center
            )

            Spacer(modifier = Modifier.height(8.dp))

            Text(
                text = "[ ${(progress * 100).toInt()}% ]",
                color = AmberMid,
                fontSize = 14.sp,
                fontFamily = FontFamily.Monospace,
                fontWeight = FontWeight.Bold
            )
        }

        // Footer Tap To Skip hint
        Text(
            text = "TAP SCREEN TO BYPASS DIAGNOSTIC",
            color = TextDim,
            fontSize = 10.sp,
            fontFamily = FontFamily.Monospace,
            modifier = Modifier.align(Alignment.BottomCenter)
        )
    }
}
