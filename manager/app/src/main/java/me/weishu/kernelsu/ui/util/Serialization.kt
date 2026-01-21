package me.weishu.kernelsu.ui.util

import android.content.pm.PackageInfo
import android.net.Uri
import android.os.Parcel
import android.os.Parcelable
import android.util.Base64
import kotlinx.serialization.KSerializer
import kotlinx.serialization.builtins.ListSerializer
import kotlinx.serialization.descriptors.PrimitiveKind
import kotlinx.serialization.descriptors.PrimitiveSerialDescriptor
import kotlinx.serialization.descriptors.SerialDescriptor
import kotlinx.serialization.encoding.Decoder
import kotlinx.serialization.encoding.Encoder

open class ParcelableSerializer<T : Parcelable>(
    private val creator: Parcelable.Creator<T>
) : KSerializer<T> {
    override val descriptor: SerialDescriptor = PrimitiveSerialDescriptor("Parcelable", PrimitiveKind.STRING)

    override fun serialize(encoder: Encoder, value: T) {
        Parcel.obtain().use { parcel ->
            value.writeToParcel(parcel, 0)
            val bytes = parcel.marshall()
            val string = Base64.encodeToString(bytes, Base64.NO_WRAP)
            encoder.encodeString(string)
        }
    }

    override fun deserialize(decoder: Decoder): T {
        val string = decoder.decodeString()
        val bytes = Base64.decode(string, Base64.NO_WRAP)
        return Parcel.obtain().use { parcel ->
            parcel.unmarshall(bytes, 0, bytes.size)
            parcel.setDataPosition(0)
            creator.createFromParcel(parcel)
        }
    }
}

object UriSerializer : ParcelableSerializer<Uri>(Uri.CREATOR)

object UriListSerializer : KSerializer<List<Uri>> by ListSerializer(UriSerializer)

object PackageInfoSerializer : ParcelableSerializer<PackageInfo>(PackageInfo.CREATOR)
