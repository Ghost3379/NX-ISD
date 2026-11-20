package com.nx.isd.uplink.ui.components

import androidx.compose.foundation.Canvas
import androidx.compose.foundation.clickable
import androidx.compose.foundation.interaction.MutableInteractionSource
import androidx.compose.foundation.layout.*
import androidx.compose.material.ripple.rememberRipple
import androidx.compose.runtime.Composable
import androidx.compose.runtime.remember
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.graphics.StrokeCap
import androidx.compose.ui.unit.dp
import com.nx.isd.uplink.ui.theme.AmberBright
import com.nx.isd.uplink.ui.theme.AmberGlow

/**
 * Tactical 3-line hamburger menu button for opening the hardware sidepanel.
 * Designed with mil-spec amber lines and clean touch feedback.
 */
@Composable
fun TacticalHamburgerButton(
    onClick: () -> Unit,
    modifier: Modifier = Modifier
) {
    Box(
        modifier = modifier
            .size(48.dp)
            .clickable(
                interactionSource = remember { MutableInteractionSource() },
                indication = rememberRipple(bounded = false, radius = 24.dp, color = AmberBright),
                onClick = onClick
            ),
        contentAlignment = Alignment.Center
    ) {
        Canvas(modifier = Modifier.size(24.dp)) {
            val strokeWidth = 2.2.dp.toPx()
            val w = size.width
            val h = size.height

            // 3 Clean horizontal lines
            val line1Y = h * 0.22f
            val line2Y = h * 0.50f
            val line3Y = h * 0.78f

            // Top line
            drawLine(
                color = AmberBright,
                start = Offset(0f, line1Y),
                end = Offset(w, line1Y),
                strokeWidth = strokeWidth,
                cap = StrokeCap.Round
            )

            // Middle line
            drawLine(
                color = AmberBright,
                start = Offset(0f, line2Y),
                end = Offset(w, line2Y),
                strokeWidth = strokeWidth,
                cap = StrokeCap.Round
            )

            // Bottom line
            drawLine(
                color = AmberBright,
                start = Offset(0f, line3Y),
                end = Offset(w, line3Y),
                strokeWidth = strokeWidth,
                cap = StrokeCap.Round
            )
        }
    }
}
