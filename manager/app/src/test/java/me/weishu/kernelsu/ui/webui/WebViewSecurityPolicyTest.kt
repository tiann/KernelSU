package me.weishu.kernelsu.ui.webui

import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

class WebViewSecurityPolicyTest {
    @Test
    fun isTrustedWebUiOrigin_requiresHttpsExactHostAndDefaultPort() {
        assertTrue(isTrustedWebUiOrigin("https", "mui.kernelsu.org", -1))
        assertTrue(isTrustedWebUiOrigin("HTTPS", "MUI.KERNELSU.ORG", 443))

        assertFalse(isTrustedWebUiOrigin("http", "mui.kernelsu.org", -1))
        assertFalse(isTrustedWebUiOrigin("https", "mui.kernelsu.org", 80))
        assertFalse(isTrustedWebUiOrigin("https", "mui.kernelsu.org", 8443))
        assertFalse(isTrustedWebUiOrigin("https", "mui.kernelsu.org.evil.example", -1))
        assertFalse(isTrustedWebUiOrigin("https", null, -1))
    }

    @Test
    fun isSupportedExternalScheme_allowsOnlyWebSchemes() {
        assertTrue(isSupportedExternalScheme("http"))
        assertTrue(isSupportedExternalScheme("HTTPS"))

        assertFalse(isSupportedExternalScheme("intent"))
        assertFalse(isSupportedExternalScheme("file"))
        assertFalse(isSupportedExternalScheme("content"))
        assertFalse(isSupportedExternalScheme(null))
    }
}
