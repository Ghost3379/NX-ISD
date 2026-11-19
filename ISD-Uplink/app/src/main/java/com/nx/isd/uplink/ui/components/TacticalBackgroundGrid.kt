package com.nx.isd.uplink.ui.components

import androidx.compose.animation.core.*
import androidx.compose.foundation.Canvas
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.Path
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.unit.dp
import com.nx.isd.uplink.ui.theme.AmberBright
import com.nx.isd.uplink.ui.theme.AmberDim
import kotlin.math.abs
import kotlin.math.cos
import kotlin.math.sin
import kotlin.math.sqrt

@Composable
fun TacticalBackgroundGrid(
    modifier: Modifier = Modifier,
    enableScanPulse: Boolean = true
) {
    // Subtle looping ambient telemetry sweep (Arkham Origins style)
    val infiniteTransition = rememberInfiniteTransition(label = "HexScan")
    val sweepProgress by infiniteTransition.animateFloat(
        initialValue = 0f,
        targetValue = 1f,
        animationSpec = infiniteRepeatable(
            animation = tween(durationMillis = 7000, easing = LinearEasing),
            repeatMode = RepeatMode.Restart
        ),
        label = "SweepY"
    )

    Canvas(modifier = modifier.fillMaxSize()) {
        val w = size.width
        val h = size.height

        // Mathematical Honeycomb Tessellation Parameters
        val hexRadius = 22.dp.toPx()
        val horizSpacing = hexRadius * 1.5f
        val vertSpacing = hexRadius * sqrt(3f)

        val sweepY = h * sweepProgress
        val sweepRadius = 140.dp.toPx()

        val cols = (w / horizSpacing).toInt() + 2
        val rows = (h / vertSpacing).toInt() + 2

        for (c in -1..cols) {
            val cx = c * horizSpacing
            val yOffset = if (c % 2 != 0) vertSpacing / 2f else 0f

            for (r in -1..rows) {
                val cy = (r * vertSpacing) + yOffset

                // Check distance to ambient sweep wave for dynamic glow
                val distToSweep = abs(cy - sweepY)
                val isNearSweep = distToSweep < sweepRadius

                val alphaMultiplier = if (isNearSweep) {
                    val factor = 1f - (distToSweep / sweepRadius)
                    0.035f + (0.07f * factor)
                } else {
                    0.035f
                }

                val hexPath = Path()
                for (i in 0 until 6) {
                    val angleRad = Math.toRadians(60.0 * i)
                    val px = cx + (hexRadius * cos(angleRad)).toFloat()
                    val py = cy + (hexRadius * sin(angleRad)).toFloat()
                    if (i == 0) hexPath.moveTo(px, py) else hexPath.lineTo(px, py)
                }
                hexPath.close()

                drawPath(
                    path = hexPath,
                    color = AmberBright.copy(alpha = alphaMultiplier),
                    style = Stroke(width = 0.9.dp.toPx())
                )
            }
        }
    }
}
