package com.nx.isd.uplink.ui.home

import androidx.compose.foundation.layout.*
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.*
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.nx.isd.uplink.ui.components.TacticalSmiley
import com.nx.isd.uplink.ui.theme.*

@Composable
fun DeviceCareCard(
    statusTitle: String = "Good",
    statusSubtitle: String = "No battery or sensor issues",
    isOptimal: Boolean = true,
    onOptimiseClicked: () -> Unit
) {
    Column(
        modifier = Modifier
            .fillMaxWidth()
            .padding(top = 8.dp, bottom = 4.dp),
        horizontalAlignment = Alignment.CenterHorizontally
    ) {
        // Clean Minimal Monochrome Vector Face (Matching Samsung One UI Style)
        TacticalSmiley(
            size = 92.dp,
            tintColor = AmberBright,
            isHappy = isOptimal
        )

        Spacer(modifier = Modifier.height(14.dp))

        // Large Status Verdict (Circuit Forem Cyberpunk Font)
        Text(
            text = statusTitle.uppercase(),
            color = TextPrimary,
            fontSize = 32.sp,
            fontWeight = FontWeight.Bold,
            fontFamily = CircuitForem,
            letterSpacing = 2.sp
        )

        Spacer(modifier = Modifier.height(4.dp))

        // Clean 1-Line Subtitle (Typewriter Font)
        Text(
            text = statusSubtitle,
            color = TextSecondary,
            fontSize = 13.sp,
            fontFamily = TypewriterFont,
            fontWeight = FontWeight.Normal
        )

        Spacer(modifier = Modifier.height(18.dp))

        // Amber Pill Button: "OPTIMISE NOW"
        Button(
            onClick = onOptimiseClicked,
            modifier = Modifier
                .fillMaxWidth(0.60f)
                .height(44.dp),
            shape = RoundedCornerShape(22.dp),
            colors = ButtonDefaults.buttonColors(
                containerColor = AmberBright,
                contentColor = OledBlack
            )
        ) {
            Text(
                text = "OPTIMISE NOW",
                fontSize = 14.sp,
                fontWeight = FontWeight.Bold,
                fontFamily = CircuitForem,
                letterSpacing = 1.sp
            )
        }
    }
}
