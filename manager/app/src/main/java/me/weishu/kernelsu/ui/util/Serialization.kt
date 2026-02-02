package me.weishu.kernelsu.ui.util

import android.os.Parcel
import android.os.Parcelable
import kotlinx.serialization.KSerializer
import kotlinx.serialization.builtins.ByteArraySerializer
import kotlinx.serialization.encoding.Decoder
import kotlinx.serialization.encoding.Encoder
import me.weishu.kernelsu.ui.screen.FlashIt
import me.weishu.kernelsu.ui.screen.RepoModuleArg
import me.weishu.kernelsu.ui.viewmodel.TemplateViewModel

object FlashItSerializer : BaseParcelableSerializer<FlashIt>(FlashIt::class.java)
object RepoModuleArgSerializer : BaseParcelableSerializer<RepoModuleArg>(RepoModuleArg::class.java)
object TemplateInfoSerializer : BaseParcelableSerializer<TemplateViewModel.TemplateInfo>(TemplateViewModel.TemplateInfo::class.java)

open class BaseParcelableSerializer<T : Parcelable>(
    private val clazz: Class<T>
) : KSerializer<T> {

    private val delegate = ByteArraySerializer()
    override val descriptor = delegate.descriptor

    private val creator: Parcelable.Creator<T> by lazy {
        @Suppress("UNCHECKED_CAST")
        clazz.getField("CREATOR").get(null) as Parcelable.Creator<T>
    }

    override fun serialize(encoder: Encoder, value: T) {
        val parcel = Parcel.obtain()
        try {
            value.writeToParcel(parcel, 0)
            val bytes = parcel.marshall()
            encoder.encodeSerializableValue(delegate, bytes)
        } finally {
            parcel.recycle()
        }
    }

    override fun deserialize(decoder: Decoder): T {
        val bytes = decoder.decodeSerializableValue(delegate)
        val parcel = Parcel.obtain()
        try {
            parcel.unmarshall(bytes, 0, bytes.size)
            parcel.setDataPosition(0)
            return creator.createFromParcel(parcel)
        } finally {
            parcel.recycle()
        }
    }
}
