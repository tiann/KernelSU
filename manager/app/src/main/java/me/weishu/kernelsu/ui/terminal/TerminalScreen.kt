// Ported from Magisk's terminal display.
package me.weishu.kernelsu.ui.terminal

import android.graphics.Typeface
import androidx.compose.foundation.background
import androidx.compose.foundation.gestures.Orientation
import androidx.compose.foundation.gestures.rememberScrollableState
import androidx.compose.foundation.gestures.scrollable
import androidx.compose.foundation.layout.BoxWithConstraints
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.runtime.Composable
import androidx.compose.runtime.DisposableEffect
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableFloatStateOf
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clipToBounds
import androidx.compose.ui.draw.drawBehind
import androidx.compose.ui.geometry.CornerRadius
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.geometry.Size
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.drawscope.drawIntoCanvas
import androidx.compose.ui.graphics.nativeCanvas
import androidx.compose.ui.platform.LocalDensity
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.core.graphics.withTranslation
import kotlin.math.max
import me.weishu.kernelsu.terminal.TerminalSession

@Composable
fun TerminalScreen(terminal: TerminalSession, modifier: Modifier = Modifier) {
    val density = LocalDensity.current
    val renderer = remember(density) {
        TerminalRenderer(with(density) { 12.sp.toPx().toInt() }, Typeface.MONOSPACE)
    }
    val emulator = terminal.emulator
    var updateTick by remember(terminal) { mutableIntStateOf(0) }
    var topRow by remember(terminal) { mutableIntStateOf(0) }
    var scrollX by remember(terminal) { mutableFloatStateOf(0f) }
    var verticalRemainder by remember(terminal) { mutableFloatStateOf(0f) }

    DisposableEffect(emulator) {
        emulator.onScreenUpdate = {
            // Keep the viewed transcript anchored while new output scrolls in.
            topRow = if (topRow == 0) 0 else {
                (topRow - emulator.scrollCounter).coerceIn(-emulator.screen.activeTranscriptRows, 0)
            }
            emulator.clearScrollCounter()
            updateTick++
        }
        onDispose { emulator.onScreenUpdate = null }
    }

    BoxWithConstraints(modifier) {
        val width = constraints.maxWidth.toFloat()
        val height = constraints.maxHeight.toFloat()
        val columns = max(256, (width / renderer.fontWidth).toInt())
        val rows = max(4, ((height - renderer.fontLineSpacingAndAscent) / renderer.fontLineSpacing).toInt())
        val lineHeight = renderer.fontLineSpacing.toFloat()
        @Suppress("UNUSED_EXPRESSION")
        updateTick
        val transcriptRows = emulator.screen.activeTranscriptRows
        val contentWidth = max(width, (emulator.maxUsedColumn + 2) * renderer.fontWidth)
        val maxScrollX = max(0f, contentWidth - width)

        LaunchedEffect(columns, rows, renderer) {
            terminal.resize(columns, rows, renderer.fontWidth.toInt(), renderer.fontLineSpacing)
        }
        LaunchedEffect(maxScrollX) { scrollX = scrollX.coerceIn(0f, maxScrollX) }

        val vertical = rememberScrollableState { delta ->
            verticalRemainder -= delta / lineHeight
            val wholeRows = verticalRemainder.toInt()
            verticalRemainder -= wholeRows
            topRow = (topRow + wholeRows).coerceIn(-emulator.screen.activeTranscriptRows, 0)
            delta
        }
        val horizontal = rememberScrollableState { delta ->
            val previous = scrollX
            scrollX = (scrollX - delta).coerceIn(0f, maxScrollX)
            previous - scrollX
        }
        Spacer(
            Modifier.fillMaxSize().clipToBounds().background(Color.Black)
                .scrollable(vertical, Orientation.Vertical)
                .scrollable(horizontal, Orientation.Horizontal)
                .drawBehind {
                    @Suppress("UNUSED_EXPRESSION")
                    updateTick
                    drawIntoCanvas { canvas ->
                        canvas.nativeCanvas.withTranslation(-scrollX, 0f) {
                            renderer.render(emulator, this, topRow, -1, -1, -1, -1)
                        }
                    }
                    val thickness = 3.dp.toPx()
                    val thumbColor = Color.White.copy(alpha = 0.5f)
                    if (transcriptRows > 0) {
                        val thumbHeight = max(24.dp.toPx(), size.height * rows / (rows + transcriptRows))
                            .coerceAtMost(size.height)
                        val y = (size.height - thumbHeight) * (topRow + transcriptRows) / transcriptRows
                        drawRoundRect(thumbColor, Offset(size.width - thickness, y), Size(thickness, thumbHeight), CornerRadius(thickness))
                    }
                    if (maxScrollX > 0f) {
                        val thumbWidth = max(24.dp.toPx(), size.width * width / contentWidth).coerceAtMost(size.width)
                        val x = (size.width - thumbWidth) * scrollX / maxScrollX
                        drawRoundRect(thumbColor, Offset(x, size.height - thickness), Size(thumbWidth, thickness), CornerRadius(thickness))
                    }
                }
        )
    }
}
