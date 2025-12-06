package me.weishu.kernelsu.ui.util

import kotlin.math.pow

fun cssColorFromArgb(argb: Int): String {
    val a = ((argb ushr 24) and 0xFF) / 255f
    val r = (argb ushr 16) and 0xFF
    val g = (argb ushr 8) and 0xFF
    val b = argb and 0xFF
    return "rgba(${r},${g},${b},${"%.3f".format(a)})"
}

fun mixArgb(c1: Int, c2: Int, ratio: Float): Int {
    val r1 = (c1 ushr 16) and 0xFF
    val g1 = (c1 ushr 8) and 0xFF
    val b1 = c1 and 0xFF
    val a1 = (c1 ushr 24) and 0xFF

    val r2 = (c2 ushr 16) and 0xFF
    val g2 = (c2 ushr 8) and 0xFF
    val b2 = c2 and 0xFF
    val a2 = (c2 ushr 24) and 0xFF

    val r = (r1 * (1 - ratio) + r2 * ratio).toInt().coerceIn(0, 255)
    val g = (g1 * (1 - ratio) + g2 * ratio).toInt().coerceIn(0, 255)
    val b = (b1 * (1 - ratio) + b2 * ratio).toInt().coerceIn(0, 255)
    val a = (a1 * (1 - ratio) + a2 * ratio).toInt().coerceIn(0, 255)

    return (a shl 24) or (r shl 16) or (g shl 8) or b
}

fun relativeLuminance(argb: Int): Double {
    fun linearize(c: Int): Double {
        val s = c / 255.0
        return if (s <= 0.03928) s / 12.92 else ((s + 0.055) / 1.055).pow(2.4)
    }

    val r = linearize((argb ushr 16) and 0xFF)
    val g = linearize((argb ushr 8) and 0xFF)
    val b = linearize(argb and 0xFF)
    return 0.2126 * r + 0.7152 * g + 0.0722 * b
}

fun contrastRatio(a: Int, b: Int): Double {
    val l1 = relativeLuminance(a)
    val l2 = relativeLuminance(b)
    val (hi, lo) = if (l1 >= l2) Pair(l1, l2) else Pair(l2, l1)
    return (hi + 0.05) / (lo + 0.05)
}

fun argbToHsl(argb: Int): Triple<Float, Float, Float> {
    val r = ((argb ushr 16) and 0xFF) / 255f
    val g = ((argb ushr 8) and 0xFF) / 255f
    val b = (argb and 0xFF) / 255f
    val max = maxOf(r, g, b)
    val min = minOf(r, g, b)
    val l = (max + min) / 2f
    val d = max - min
    val s = if (d == 0f) 0f else d / (1f - kotlin.math.abs(2f * l - 1f))
    val h = when {
        d == 0f -> 0f
        max == r -> ((g - b) / d % 6f) * 60f
        max == g -> ((b - r) / d + 2f) * 60f
        else -> ((r - g) / d + 4f) * 60f
    }.let { if (it < 0f) it + 360f else it }
    return Triple(h, s, l)
}

fun hslToArgb(h: Float, s: Float, l: Float, alpha: Int = 0xFF): Int {
    val c = (1f - kotlin.math.abs(2f * l - 1f)) * s
    val x = c * (1f - kotlin.math.abs((h / 60f) % 2f - 1f))
    val m = l - c / 2f
    val (r1, g1, b1) = when {
        h < 60f -> Triple(c, x, 0f)
        h < 120f -> Triple(x, c, 0f)
        h < 180f -> Triple(0f, c, x)
        h < 240f -> Triple(0f, x, c)
        h < 300f -> Triple(x, 0f, c)
        else -> Triple(c, 0f, x)
    }
    val r = ((r1 + m) * 255f).toInt().coerceIn(0, 255)
    val g = ((g1 + m) * 255f).toInt().coerceIn(0, 255)
    val b = ((b1 + m) * 255f).toInt().coerceIn(0, 255)
    return (alpha shl 24) or (r shl 16) or (g shl 8) or b
}

fun adjustLightnessArgb(argb: Int, delta: Float): Int {
    val (h, s, l) = argbToHsl(argb)
    val nl = (l + delta).coerceIn(0f, 1f)
    val alpha = (argb ushr 24) and 0xFF
    return hslToArgb(h, s, nl, alpha)
}

fun ensureVisibleByMix(original: Int, candidate: Int, minRatio: Double, mixWithWhiteIfLighter: Boolean): Int {
    if (contrastRatio(original, candidate) >= minRatio) return candidate
    val target = if (mixWithWhiteIfLighter) 0xFFFFFFFF.toInt() else 0xFF000000.toInt()
    var lo = 0f
    var hi = 1f
    var best = candidate
    for (i in 0 until 12) {
        val mid = (lo + hi) / 2f
        val mixed = mixArgb(candidate, target, mid)
        if (contrastRatio(original, mixed) >= minRatio) {
            best = mixed
            hi = mid
        } else {
            lo = mid
        }
    }
    return best
}
