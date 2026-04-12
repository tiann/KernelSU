package me.weishu.kernelsu.ui.component.miuix.effect

internal object BgEffectConfig {

    internal class Config(
        val points: FloatArray,
        val colors: FloatArray,
        val lightOffset: Float,
        val saturateOffset: Float,
    )

    private val PHONE_LIGHT = Config(
        points = floatArrayOf(
            0.67f, 0.42f, 1.0f, 0.69f, 0.75f, 1.0f,
            0.14f, 0.71f, 0.95f, 0.14f, 0.27f, 0.8f,
        ),
        colors = floatArrayOf(
            0.57f, 0.76f, 0.98f, 1.0f,
            0.98f, 0.85f, 0.68f, 1.0f,
            0.98f, 0.75f, 0.93f, 1.0f,
            0.73f, 0.70f, 0.98f, 1.0f,
        ),
        lightOffset = 0.1f,
        saturateOffset = 0.2f,
    )

    private val PHONE_DARK = Config(
        points = floatArrayOf(
            0.63f, 0.50f, 0.88f, 0.69f, 0.75f, 0.80f,
            0.17f, 0.66f, 0.81f, 0.14f, 0.24f, 0.72f,
        ),
        colors = floatArrayOf(
            0.0f, 0.31f, 0.58f, 1.0f,
            0.53f, 0.29f, 0.15f, 1.0f,
            0.46f, 0.06f, 0.27f, 1.0f,
            0.16f, 0.12f, 0.45f, 1.0f,
        ),
        lightOffset = -0.1f,
        saturateOffset = 0.2f,
    )

    private val PAD_LIGHT = Config(
        points = floatArrayOf(
            0.67f, 0.37f, 0.88f, 0.54f, 0.66f, 1.0f,
            0.37f, 0.71f, 0.68f, 0.28f, 0.26f, 0.62f,
        ),
        colors = floatArrayOf(
            0.57f, 0.76f, 0.98f, 1.0f,
            0.98f, 0.85f, 0.68f, 1.0f,
            0.98f, 0.75f, 0.93f, 0.95f,
            0.73f, 0.70f, 0.98f, 0.90f,
        ),
        lightOffset = 0.1f,
        saturateOffset = 0f,
    )

    private val PAD_DARK = Config(
        points = floatArrayOf(
            0.55f, 0.42f, 1.0f, 0.56f, 0.75f, 1.0f,
            0.40f, 0.59f, 0.71f, 0.43f, 0.09f, 0.75f,
        ),
        colors = floatArrayOf(
            0.0f, 0.31f, 0.58f, 1.0f,
            0.53f, 0.29f, 0.15f, 1.0f,
            0.46f, 0.06f, 0.27f, 1.0f,
            0.16f, 0.12f, 0.45f, 1.0f,
        ),
        lightOffset = -0.1f,
        saturateOffset = 0.2f,
    )

    internal fun get(
        deviceType: DeviceType,
        isDark: Boolean,
    ): Config = when (deviceType) {
        DeviceType.PHONE if !isDark -> PHONE_LIGHT
        DeviceType.PHONE if isDark -> PHONE_DARK
        DeviceType.PAD if !isDark -> PAD_LIGHT
        else -> PAD_DARK
    }
}
