package com.nx.isd.uplink.ui.components

import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Add
import androidx.compose.material.icons.filled.GridView
import androidx.compose.material3.*
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.nx.isd.uplink.ui.theme.*

/**
 * Clean sidepanel drawer showing registered devices and quick actions.
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
            .width(300.dp)
            .border(width = 1.dp, color = TacticalBorder)
    ) {
        Column(
            modifier = Modifier
                .fillMaxSize()
                .statusBarsPadding()
                .padding(20.dp)
        ) {
            // Header
            Text(
                text = "NX-Uplink",
                fontFamily = CircuitForem,
                fontSize = 24.sp,
                fontWeight = FontWeight.Bold,
                color = AmberBright,
                letterSpacing = 1.sp
            )

            Spacer(modifier = Modifier.height(24.dp))
            HorizontalDivider(color = TacticalBorder, thickness = 1.dp)
            Spacer(modifier = Modifier.height(20.dp))

            Text(
                text = "MY DEVICES",
                fontFamily = CircuitForem,
                fontSize = 12.sp,
                fontWeight = FontWeight.Bold,
                color = TextSecondary,
                letterSpacing = 1.sp
            )

            Spacer(modifier = Modifier.height(12.dp))

            // Device 1: NX-ISD
            DrawerDeviceRow(
                name = "NX-ISD",
                type = "Smartwatch",
                status = "Connected (Simulation)",
                statusColor = NominalGreen,
                isSelected = selectedDevice == "NX-ISD",
                onClick = {
                    onSelectDevice("NX-ISD")
                    onCloseDrawer()
                }
            )

            Spacer(modifier = Modifier.height(10.dp))

            // Device 2: NX-Edge
            DrawerDeviceRow(
                name = "NX-Edge",
                type = "Dev Blade",
                status = "Coming soon",
                statusColor = TextDim,
                isSelected = selectedDevice == "NX-Edge",
                onClick = {
                    onSelectDevice("NX-Edge")
                    onCloseDrawer()
                }
            )

            Spacer(modifier = Modifier.height(24.dp))
            HorizontalDivider(color = TacticalBorder, thickness = 1.dp)
            Spacer(modifier = Modifier.height(20.dp))

            // Navigation Actions
            Row(
                modifier = Modifier
                    .fillMaxWidth()
                    .clip(RoundedCornerShape(8.dp))
                    .clickable {
                        onNavigateFleet()
                        onCloseDrawer()
                    }
                    .padding(vertical = 12.dp, horizontal = 8.dp),
                verticalAlignment = Alignment.CenterVertically
            ) {
                Icon(
                    imageVector = Icons.Default.GridView,
                    contentDescription = null,
                    tint = AmberBright,
                    modifier = Modifier.size(20.dp)
                )
                Spacer(modifier = Modifier.width(12.dp))
                Text(
                    text = "All Devices",
                    fontFamily = TypewriterFont,
                    fontSize = 14.sp,
                    fontWeight = FontWeight.Medium,
                    color = TextPrimary
                )
            }

            Row(
                modifier = Modifier
                    .fillMaxWidth()
                    .clip(RoundedCornerShape(8.dp))
                    .clickable {
                        onConnectNewDevice()
                        onCloseDrawer()
                    }
                    .padding(vertical = 12.dp, horizontal = 8.dp),
                verticalAlignment = Alignment.CenterVertically
            ) {
                Icon(
                    imageVector = Icons.Default.Add,
                    contentDescription = null,
                    tint = AmberBright,
                    modifier = Modifier.size(20.dp)
                )
                Spacer(modifier = Modifier.width(12.dp))
                Text(
                    text = "Connect a device",
                    fontFamily = TypewriterFont,
                    fontSize = 14.sp,
                    fontWeight = FontWeight.Medium,
                    color = TextPrimary
                )
            }
        }
    }
}

@Composable
private fun DrawerDeviceRow(
    name: String,
    type: String,
    status: String,
    statusColor: Color,
    isSelected: Boolean,
    onClick: () -> Unit
) {
    Box(
        modifier = Modifier
            .fillMaxWidth()
            .clip(RoundedCornerShape(10.dp))
            .background(if (isSelected) TacticalSurfaceElevated else TacticalSurface)
            .border(
                width = 1.dp,
                color = if (isSelected) AmberBright else TacticalBorder,
                shape = RoundedCornerShape(10.dp)
            )
            .clickable { onClick() }
            .padding(14.dp)
    ) {
        Column {
            Row(
                modifier = Modifier.fillMaxWidth(),
                horizontalArrangement = Arrangement.SpaceBetween,
                verticalAlignment = Alignment.CenterVertically
            ) {
                Text(
                    text = name,
                    fontFamily = CircuitForem,
                    fontSize = 16.sp,
                    fontWeight = FontWeight.Bold,
                    color = if (isSelected) AmberBright else TextPrimary
                )
                Text(
                    text = type,
                    fontFamily = TypewriterFont,
                    fontSize = 11.sp,
                    color = TextSecondary
                )
            }

            Spacer(modifier = Modifier.height(4.dp))

            Row(
                verticalAlignment = Alignment.CenterVertically,
                horizontalArrangement = Arrangement.spacedBy(6.dp)
            ) {
                Box(
                    modifier = Modifier
                        .size(6.dp)
                        .background(statusColor, CircleShape)
                )
                Text(
                    text = status,
                    fontFamily = TypewriterFont,
                    fontSize = 11.sp,
                    color = statusColor
                )
            }
        }
    }
}
