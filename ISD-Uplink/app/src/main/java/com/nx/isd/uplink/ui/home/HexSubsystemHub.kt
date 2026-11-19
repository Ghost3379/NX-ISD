package com.nx.isd.uplink.ui.home

import androidx.compose.foundation.Canvas
import androidx.compose.foundation.gestures.detectTapGestures
import androidx.compose.foundation.layout.*
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.*
import androidx.compose.material3.Text
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.Path
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.graphics.drawscope.translate
import androidx.compose.ui.graphics.vector.ImageVector
import androidx.compose.ui.graphics.vector.rememberVectorPainter
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.nx.isd.uplink.ui.theme.*
import kotlin.math.cos
import kotlin.math.sin
import kotlin.math.sqrt

data class HexNode(
    val id: String,
    val title: String,
    val icon: ImageVector,
    val colOffset: Float,
    val rowOffset: Float
)

@Composable
fun HexSubsystemHub(
    onNodeSelected: (String) -> Unit
) {
    var selectedNode by remember { mutableStateOf<String?>(null) }

    // 5 Hexagons from PLAN.md: 1 Center (Field Manual) + 4 Surrounding Nodes
    val nodes = remember {
        listOf(
            // Center Node: Field Manual & Reference
            HexNode("FIELD_MANUAL", "MANUAL", Icons.Default.MenuBook, 0f, 0f),
            // Top-Left: General (System, QR, Battery, Storage)
            HexNode("GENERAL", "GENERAL", Icons.Default.Settings, -0.866f, -1.0f),
            // Top-Right: Peripherie (NX-SDS, Hardware, Matrix, Buzzer)
            HexNode("PERIPHERIE", "PERIPHERIE", Icons.Default.Memory, 0.866f, -1.0f),
            // Bottom-Left: Notifications (BLE Alert Rules)
            HexNode("NOTIFICATIONS", "NOTIFICATIONS", Icons.Default.NotificationsActive, -0.866f, 1.0f),
            // Bottom-Right: Health (Biometrics, PPG, SpO2, Stress)
            HexNode("HEALTH", "HEALTH", Icons.Default.Favorite, 0.866f, 1.0f)
        )
    }

    // Vector painters for icons
    val painters = nodes.associate { it.id to rememberVectorPainter(it.icon) }

    Column(
        modifier = Modifier
            .fillMaxWidth()
            .padding(vertical = 4.dp),
        horizontalAlignment = Alignment.CenterHorizontally
    ) {
        Box(
            modifier = Modifier
                .fillMaxWidth()
                .height(340.dp),
            contentAlignment = Alignment.Center
        ) {
            Canvas(
                modifier = Modifier
                    .fillMaxSize()
                    .pointerInput(Unit) {
                        detectTapGestures { tapOffset ->
                            val cx = size.width / 2f
                            val cy = size.height / 2f
                            val hexRadius = 52.dp.toPx()
                            val spacing = hexRadius * 1.76f

                            for (node in nodes) {
                                val nodeX = cx + node.colOffset * spacing
                                val nodeY = cy + node.rowOffset * (spacing * 0.866f)
                                val dx = tapOffset.x - nodeX
                                val dy = tapOffset.y - nodeY
                                if (sqrt(dx * dx + dy * dy) <= hexRadius) {
                                    selectedNode = node.id
                                    onNodeSelected(node.id)
                                    break
                                }
                            }
                        }
                    }
            ) {
                val cx = size.width / 2f
                val cy = size.height / 2f
                val hexRadius = 52.dp.toPx()
                val spacing = hexRadius * 1.76f // Tighter gap between hexagons

                // Draw connecting circuit traces between center and surrounding hexagons
                for (node in nodes) {
                    if (node.id != "FIELD_MANUAL") {
                        val nodeX = cx + node.colOffset * spacing
                        val nodeY = cy + node.rowOffset * (spacing * 0.866f)
                        drawLine(
                            color = AmberDim.copy(alpha = 0.45f),
                            start = Offset(cx, cy),
                            end = Offset(nodeX, nodeY),
                            strokeWidth = 1.5.dp.toPx()
                        )
                    }
                }

                // Draw each double-contour hexagon in unified Tactical Orange/Amber
                for (node in nodes) {
                    val nodeX = cx + node.colOffset * spacing
                    val nodeY = cy + node.rowOffset * (spacing * 0.866f)
                    val isSelected = selectedNode == node.id

                    // 1. Dark Hexagon Fill
                    val fillPath = createHexagonPath(nodeX, nodeY, hexRadius)
                    drawPath(
                        path = fillPath,
                        color = if (isSelected) TacticalSurfaceElevated else TacticalSurface
                    )

                    // 2. Outer Tactical Amber Hexagon Border
                    val outerPath = createHexagonPath(nodeX, nodeY, hexRadius)
                    drawPath(
                        path = outerPath,
                        color = if (isSelected) AmberBright else AmberBright.copy(alpha = 0.85f),
                        style = Stroke(width = if (isSelected) 2.5.dp.toPx() else 1.8.dp.toPx())
                    )

                    // 3. Inner Glowing Amber Contour
                    val innerRadius = hexRadius - 5.dp.toPx()
                    val innerPath = createHexagonPath(nodeX, nodeY, innerRadius)
                    drawPath(
                        path = innerPath,
                        color = if (isSelected) AmberBright else AmberMid.copy(alpha = 0.65f),
                        style = Stroke(width = 1.2.dp.toPx())
                    )

                    // 4. Center Icon in Pure Amber
                    val painter = painters[node.id]
                    if (painter != null) {
                        val iconSize = 28.dp.toPx()
                        val iconLeft = nodeX - (iconSize / 2f)
                        val iconTop = nodeY - (iconSize / 2f)

                        translate(left = iconLeft, top = iconTop) {
                            with(painter) {
                                draw(
                                    size = androidx.compose.ui.geometry.Size(iconSize, iconSize),
                                    colorFilter = androidx.compose.ui.graphics.ColorFilter.tint(AmberBright)
                                )
                            }
                        }
                    }
                }
            }
        }

        // Clean, minimal tactical title strip beneath the wheel (Zero bloat)
        Text(
            text = selectedNode?.let { "[ $it // READY ]" } ?: "[ SELECT SUBSYSTEM ]",
            color = AmberMid,
            fontSize = 12.sp,
            fontFamily = CircuitForem,
            fontWeight = FontWeight.Bold,
            letterSpacing = 2.sp
        )
    }
}

// Generates regular flat-topped hexagon path centered at (cx, cy)
private fun createHexagonPath(cx: Float, cy: Float, radius: Float): Path {
    val path = Path()
    for (i in 0 until 6) {
        val angleDeg = 60.0 * i - 30.0 // Flat top/bottom alignment
        val angleRad = Math.toRadians(angleDeg)
        val x = cx + (radius * cos(angleRad)).toFloat()
        val y = cy + (radius * sin(angleRad)).toFloat()
        if (i == 0) path.moveTo(x, y) else path.lineTo(x, y)
    }
    path.close()
    return path
}
