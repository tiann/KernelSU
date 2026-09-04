package me.weishu.kernelsu.ui.webui

internal const val WEBUI_DOMAIN = "mui.kernelsu.org"
internal const val WEBUI_ORIGIN = "https://$WEBUI_DOMAIN"

internal fun isTrustedWebUiOrigin(
    scheme: String?,
    host: String?,
    port: Int,
): Boolean =
    scheme?.equals("https", ignoreCase = true) == true &&
        host?.equals(WEBUI_DOMAIN, ignoreCase = true) == true &&
        (port == -1 || port == 443)

internal fun isSupportedExternalScheme(scheme: String?): Boolean =
    scheme?.equals("https", ignoreCase = true) == true ||
        scheme?.equals("http", ignoreCase = true) == true
