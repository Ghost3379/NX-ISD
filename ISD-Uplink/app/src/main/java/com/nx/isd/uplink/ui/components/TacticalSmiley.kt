package com.nx.isd.uplink.ui.components

import androidx.compose.foundation.Canvas
import androidx.compose.foundation.layout.size
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.geometry.Size
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.Path
import androidx.compose.ui.graphics.StrokeCap
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.dp
import com.nx.isd.uplink.ui.theme.AmberBright

@Composable
fun TacticalSmiley(
    modifier: Modifier = Modifier,
    size: Dp = 92.dp,
    tintColor: Color = AmberBright,
    ringStroke: Dp = 3.5.dp,
    isHappy: Boolean = true
) {
    Canvas(modifier = modifier.size(size)) {
        val w = this.size.width
        val h = this.size.height
        val cx = w / 2f
        val cy = h / 2f
        val outerRadius = (w / 2f) - (ringStroke.toPx() / 2f)

        // 1. Outer Ring (Tactical Amber)
        drawCircle(
            color = tintColor,
            radius = outerRadius,
            center = Offset(cx, cy),
            style = Stroke(width = ringStroke.toPx())
        )

        // 2. Subtle Dark Glow Background inside the ring
        drawCircle(
            color = tintColor.copy(alpha = 0.08f),
            radius = outerRadius,
            center = Offset(cx, cy)
        )

        if (isHappy) {
            // Samsung Minimalist Vector "Great" Eyes: Two clean circular dots
            val eyeRadius = 4.5.dp.toPx()
            val eyeSpacing = 13.dp.toPx()
            val eyeY = cy - 8.dp.toPx()

            drawCircle(
                color = tintColor,
                radius = eyeRadius,
                center = Offset(cx - eyeSpacing, eyeY)
            )
            drawCircle(
                color = tintColor,
                radius = eyeRadius,
                center = Offset(cx + eyeSpacing, eyeY)
            )

            // Samsung Minimalist Crescent Smile (Open smooth curve)
            val mouthWidth = 24.dp.toPx()
            val mouthY = cy + 5.dp.toPx()
            val mouthDepth = 12.dp.toPx()

            val mouthPath = Path().apply {
                moveTo(cx - (mouthWidth / 2f), mouthY)
                quadraticBezierTo(
                    cx, mouthY + mouthDepth,
                    cx + (mouthWidth / 2f), mouthY
                )
            }

            drawPath(
                path = mouthPath,
                color = tintColor,
                style = Stroke(
                    width = 4.dp.toPx(),
                    cap = StrokeCap.Round
                )
            )
        } else {
            // Squeezed Warning Eyes (> <)
            val eyeOffset = 13.dp.toPx()
            val eyeY = cy - 6.dp.toPx()
            val arm = 6.dp.toPx()

            // Left Eye (>)
            drawLine(tintColor, Offset(cx - eyeOffset - arm, eyeY - arm), Offset(cx - eyeOffset, eyeY), 3.dp.toPx(), StrokeCap.Round)
            drawLine(tintColor, Offset(cx - eyeOffset, eyeY), Offset(cx - eyeOffset - arm, eyeY + arm), 3.dp.toPx(), StrokeCap.Round)

            // Right Eye (<)
            drawLine(tintColor, Offset(cx + eyeOffset + arm, eyeY - arm), Offset(cx + eyeOffset, eyeY), 3.dp.toPx(), StrokeCap.Round)
            drawLine(tintColor, Offset(cx + eyeOffset, eyeY), Offset(cx + eyeOffset + arm, eyeY + arm), 3.dp.toPx(), StrokeCap.Round)

            // Squiggly Warning Mouth
            val mouthY = cy + 10.dp.toPx()
            val mouthPath = Path().apply {
                moveTo(cx - 12.dp.toPx(), mouthY)
                lineTo(cx - 6.dp.toPx(), mouthY - 3.dp.toPx())
                lineTo(cx, mouthY + 3.dp.toPx())
                lineTo(cx + 6.dp.toPx(), mouthY - 3.dp.toPx())
                lineTo(cx + 12.dp.toPx(), mouthY)
            }
            drawPath(mouthPath, tintColor, style = Stroke(width = 3.dp.toPx(), cap = StrokeCap.Round))
        }
    }
}
