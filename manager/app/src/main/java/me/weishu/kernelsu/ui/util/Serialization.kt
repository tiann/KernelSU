package me.weishu.kernelsu.ui.util

import android.content.pm.ApplicationInfo
import android.content.pm.PackageInfo
import android.net.Uri
import androidx.core.net.toUri
import kotlinx.serialization.KSerializer
import kotlinx.serialization.descriptors.PrimitiveKind
import kotlinx.serialization.descriptors.PrimitiveSerialDescriptor
import kotlinx.serialization.descriptors.SerialDescriptor
import kotlinx.serialization.descriptors.buildClassSerialDescriptor
import kotlinx.serialization.descriptors.element
import kotlinx.serialization.encoding.CompositeDecoder
import kotlinx.serialization.encoding.Decoder
import kotlinx.serialization.encoding.Encoder
import kotlinx.serialization.encoding.decodeStructure
import kotlinx.serialization.encoding.encodeStructure
import me.weishu.kernelsu.ksuApp


object PackageInfoSerializer : KSerializer<PackageInfo> {
    override val descriptor: SerialDescriptor = buildClassSerialDescriptor("PackageInfo") {
        element<String>("packageName")
        element<Int>("uid")
    }

    override fun serialize(encoder: Encoder, value: PackageInfo) {
        encoder.encodeStructure(descriptor) {
            encodeStringElement(descriptor, 0, value.packageName)
            encodeIntElement(descriptor, 1, value.applicationInfo?.uid ?: 0)
        }
    }

    override fun deserialize(decoder: Decoder): PackageInfo {
        return decoder.decodeStructure(descriptor) {
            var packageName = ""
            var uid = 0
            while (true) {
                when (val index = decodeElementIndex(descriptor)) {
                    0 -> packageName = decodeStringElement(descriptor, 0)
                    1 -> uid = decodeIntElement(descriptor, 1)
                    CompositeDecoder.DECODE_DONE -> break
                    else -> error("Unexpected index: $index")
                }
            }
            runCatching {
                ksuApp.packageManager.getPackageInfo(packageName, 0)
            }.getOrElse {
                PackageInfo().apply {
                    this.packageName = packageName
                    this.applicationInfo = ApplicationInfo().apply {
                        this.uid = uid
                        this.packageName = packageName
                        this.enabled = true
                    }
                }
            }
        }
    }
}

object UriSerializer : KSerializer<Uri> {
    override val descriptor: SerialDescriptor = PrimitiveSerialDescriptor("Uri", PrimitiveKind.STRING)

    override fun serialize(encoder: Encoder, value: Uri) = encoder.encodeString(value.toString())

    override fun deserialize(decoder: Decoder): Uri = decoder.decodeString().toUri()
}
