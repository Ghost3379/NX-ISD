package com.nx.isd.uplink.ui.theme

import androidx.compose.material3.Typography
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.text.font.Font
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.sp
import com.nx.isd.uplink.R

// Custom Cyberpunk / PCB Silkscreen Typography (Headlines & Badges)
val CircuitForem = FontFamily(
    Font(R.font.circuit_forem)
)

// Mechanical Typewriter Typography (Menus, Options & Field Logs)
val TypewriterFont = FontFamily(
    Font(R.font.typewriter)
)

val TacticalTypography = Typography(
    headlineLarge = TextStyle(
        fontFamily = CircuitForem,
        fontWeight = FontWeight.Bold,
        fontSize = 28.sp,
        letterSpacing = 2.sp
    ),
    titleLarge = TextStyle(
        fontFamily = CircuitForem,
        fontWeight = FontWeight.Bold,
        fontSize = 20.sp,
        letterSpacing = 1.5.sp
    ),
    titleMedium = TextStyle(
        fontFamily = CircuitForem,
        fontWeight = FontWeight.Medium,
        fontSize = 15.sp,
        letterSpacing = 1.sp
    ),
    bodyMedium = TextStyle(
        fontFamily = TypewriterFont,
        fontWeight = FontWeight.Normal,
        fontSize = 13.sp,
        letterSpacing = 0.5.sp
    )
)
