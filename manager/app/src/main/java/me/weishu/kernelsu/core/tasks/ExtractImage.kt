package me.weishu.kernelsu.core.tasks

import me.weishu.kernelsu.core.utils.DataSourceChannel
import org.apache.commons.compress.archivers.zip.ZipArchiveEntry
import org.apache.commons.compress.archivers.zip.ZipFile
import org.apache.commons.compress.archivers.zip.ZipMethod
import java.io.File
import java.io.FileOutputStream
import java.io.IOException
import java.nio.channels.FileChannel
import java.nio.file.StandardOpenOption
import java.util.zip.Inflater
import java.util.zip.InflaterInputStream

data class ProbeResult(
    val partitions: List<String>,
    val kmi: String?,
)

class ExtractImage(
    private val outFile: File,
    private val onConsole: (String) -> Unit,
) {
    @Throws(IOException::class)
    fun consume(channel: DataSourceChannel, target: String) {
        ZipFile.builder()
            .setSeekableByteChannel(channel)
            .setIgnoreLocalFileHeader(true)
            .get().use { zipFile ->
                val payload = zipFile.getEntry("payload.bin")
                if (payload != null) {
                    onConsole("- Processing as OTA package")

                    zipFile.getEntry("META-INF/com/android/metadata")?.let { entry ->
                        zipFile.getInputStream(entry).use {
                            val meta = it.bufferedReader().readText()

                            onConsole("- OTA metadata:")
                            meta.lines().forEach { line ->
                                if (line.startsWith("post-")) {
                                    onConsole("  ${line.substringAfter('-')}")
                                }
                            }
                        }
                    }
                    zipFile.getRawInputStream(payload)
                    extractFromOTAPackage(payload, channel, target)
                } else {
                    extractFromFactoryImage(zipFile, channel, target)
                }
            }
    }

    @Throws(IOException::class)
    fun consumePayload(channel: DataSourceChannel, target: String) {
        onConsole("- Processing as raw payload.bin")
        Payload(channel).extract(
            outFile,
            target,
            onConsole,
            onConsole,
        )
    }

    @Throws(IOException::class)
    private fun extractFromOTAPackage(
        payload: ZipArchiveEntry,
        channel: DataSourceChannel,
        target: String,
    ) {
        if (payload.method != ZipMethod.STORED.code) {
            throw IOException("payload.bin is compressed, expected STORED method")
        }

        channel.slice(payload.dataOffset, payload.size).use { payloadChannel ->
            Payload(payloadChannel).extract(
                outFile,
                target,
                onConsole,
                onConsole,
            )
        }
    }

    @Throws(IOException::class)
    private fun extractFromFactoryImage(
        zipFile: ZipFile,
        channel: DataSourceChannel,
        target: String,
    ) {
        onConsole("- Processing as factory image package")

        findBootImageEntry(zipFile, target)?.let { entry ->
            return extractImageFile(zipFile, entry, channel)
        }

        val imageZipEntry = zipFile.entries.asSequence().find { entry ->
            val fileName = entry.name.substringAfterLast('/')
            fileName.startsWith("image-") && fileName.endsWith(".zip")
        }
        if (imageZipEntry != null) {
            zipFile.getRawInputStream(imageZipEntry)
            return extractFromInnerImageZip(imageZipEntry, channel, target)
        }

        throw IOException("inner image ZIP not found in factory image package")
    }

    private fun findBootImageEntry(zipFile: ZipFile, target: String): ZipArchiveEntry? {
        val imageName = BOOT_IMAGE_NAMES[target] ?: return null
        return zipFile.entries.asSequence().find {
            it.name.substringAfterLast('/') == imageName
        }
    }

    @Throws(IOException::class)
    private fun extractFromInnerImageZip(
        entry: ZipArchiveEntry,
        channel: DataSourceChannel,
        target: String,
    ) {
        onConsole("Found inner image ZIP: ${entry.name}")

        if (entry.method != ZipMethod.STORED.code) {
            throw IOException("image ZIP is compressed, expected STORED method")
        }

        channel.slice(entry.dataOffset, entry.size).use { innerZipChannel ->
            ZipFile.builder()
                .setSeekableByteChannel(innerZipChannel)
                .setIgnoreLocalFileHeader(true)
                .get().use { innerZipFile ->
                    val targetEntry = findBootImageEntry(innerZipFile, target)
                        ?: throw IOException("boot image not found in inner image ZIP")
                    return extractImageFile(innerZipFile, targetEntry, innerZipChannel)
                }
        }
    }

    @Throws(IOException::class)
    private fun extractImageFile(
        zipFile: ZipFile,
        entry: ZipArchiveEntry,
        channel: DataSourceChannel,
    ) {
        onConsole("- Found boot image entry: ${entry.name} (${entry.size} bytes)")
        onConsole("- Downloading")

        zipFile.getRawInputStream(entry)
        when (entry.method) {
            ZipMethod.STORED.code -> {
                FileChannel.open(
                    outFile.toPath(),
                    StandardOpenOption.CREATE,
                    StandardOpenOption.WRITE,
                    StandardOpenOption.READ,
                    StandardOpenOption.TRUNCATE_EXISTING
                ).use { fileChannel ->
                    val mapped = fileChannel.map(FileChannel.MapMode.READ_WRITE, 0, entry.size)
                    val sourceChannel = channel.slice(entry.dataOffset, entry.size)
                    sourceChannel.read(mapped)
                }
            }

            ZipMethod.DEFLATED.code -> {
                InflaterInputStream(
                    channel.streamRead(entry.dataOffset, entry.compressedSize),
                    Inflater(true),
                    16 * 1024
                ).use { input ->
                    FileOutputStream(outFile).use { out ->
                        input.copyTo(out)
                    }
                }
            }

            else -> throw IOException("unsupported method: ${entry.method}")
        }
    }

    companion object {
        private val BOOT_IMAGE_NAMES = mapOf(
            "boot" to "boot.img",
            "init_boot" to "init_boot.img",
            "vendor_boot" to "vendor_boot.img",
        )

        @Throws(IOException::class)
        fun probe(
            channel: DataSourceChannel,
            withKmi: Boolean = true,
            onProgress: ((String) -> Unit)? = null,
        ): ProbeResult {
            return probeEntries(channel, withKmi, onProgress)
        }

        @Throws(IOException::class)
        fun probePayload(
            channel: DataSourceChannel,
            withKmi: Boolean = true,
            onProgress: ((String) -> Unit)? = null,
        ): ProbeResult {
            val payload = Payload(channel)
            return ProbeResult(
                partitions = filterBootPartitions(payload.partitionNames()),
                kmi = if (withKmi) payload.kernelKmi(onProgress) else null,
            )
        }

        @Throws(IOException::class)
        private fun probeEntries(
            channel: DataSourceChannel,
            withKmi: Boolean,
            onProgress: ((String) -> Unit)?,
        ): ProbeResult {
            ZipFile.builder()
                .setSeekableByteChannel(channel)
                .setIgnoreLocalFileHeader(true)
                .get().use { zipFile ->
                    val payload = zipFile.getEntry("payload.bin")
                    if (payload != null) {
                        if (payload.method != ZipMethod.STORED.code) {
                            throw IOException("payload.bin is compressed, expected STORED method")
                        }
                        zipFile.getRawInputStream(payload)
                        channel.slice(payload.dataOffset, payload.size).use { payloadChannel ->
                            val payload = Payload(payloadChannel)
                            return ProbeResult(
                                partitions = filterBootPartitions(payload.partitionNames()),
                                kmi = if (withKmi) payload.kernelKmi(onProgress) else null,
                            )
                        }
                    }

                    val bootParts = BOOT_IMAGE_NAMES.entries
                        .filter { (_, imageName) ->
                            zipFile.entries.asSequence().any {
                                it.name.substringAfterLast('/') == imageName
                            }
                        }
                        .map { it.key }
                    if (bootParts.isNotEmpty()) {
                        val kmi = if (withKmi) {
                            KERNEL_IMAGE_NAMES.firstNotNullOfOrNull { imageName ->
                                zipFile.entries.asSequence()
                                    .firstOrNull { it.name.substringAfterLast('/') == imageName }
                                    ?.let { readBootImageKmi(channel, it) }
                            }
                        } else {
                            null
                        }
                        return ProbeResult(bootParts, kmi)
                    }

                    val imageZip = zipFile.entries.asSequence().find { entry ->
                        val fileName = entry.name.substringAfterLast('/')
                        fileName.startsWith("image-") && fileName.endsWith(".zip")
                    }
                    if (imageZip != null) {
                        if (imageZip.method != ZipMethod.STORED.code) {
                            throw IOException("image ZIP is compressed, expected STORED method")
                        }
                        zipFile.getRawInputStream(imageZip)
                        channel.slice(imageZip.dataOffset, imageZip.size).use { innerChannel ->
                            return probeEntries(innerChannel, withKmi, onProgress)
                        }
                    }

                    throw IOException("boot image not found in package")
                }
        }

        private fun readBootImageKmi(channel: DataSourceChannel, entry: ZipArchiveEntry): String? {
            if (entry.method != ZipMethod.STORED.code) return null
            return channel.slice(entry.dataOffset, entry.size).use { entryChannel ->
                BootKernelVersion.parseKmiFromBoot(entryChannel)
            }
        }

        private fun filterBootPartitions(names: List<String>): List<String> {
            return BOOT_IMAGE_NAMES.keys.filter { it in names }
        }

        private val KERNEL_IMAGE_NAMES = listOf("boot.img")
    }
}
