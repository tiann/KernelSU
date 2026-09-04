package me.weishu.kernelsu.core.utils

import okhttp3.OkHttpClient
import okhttp3.Request
import org.apache.commons.io.input.BoundedInputStream
import java.io.IOException
import java.io.InputStream
import java.nio.ByteBuffer
import java.nio.channels.Channels
import java.nio.channels.ClosedChannelException
import java.nio.channels.FileChannel
import java.nio.channels.NonWritableChannelException
import java.nio.channels.SeekableByteChannel

class DataSourceChannel private constructor(
    private val client: OkHttpClient?,
    private val url: String?,
    private val fileChannel: FileChannel?,
    private val startOffset: Long,
    private val totalSize: Long,
) : SeekableByteChannel {

    constructor(client: OkHttpClient, url: String) : this(client, url, null, 0, fetchTotalSize(client, url))

    private var pos = 0L
    private var open = true
    private var cache: ByteArray? = null
    private var cacheStart = -1L

    override fun isOpen(): Boolean = open

    override fun size(): Long = totalSize

    override fun position(): Long = pos

    override fun position(newPosition: Long): DataSourceChannel {
        if (!open) throw ClosedChannelException()
        if (newPosition < 0) throw IllegalArgumentException("Position out of bounds: $newPosition")
        pos = newPosition
        return this
    }

    override fun close() {
        open = false
        cache = null
        fileChannel?.close()
    }

    override fun read(dst: ByteBuffer): Int {
        val bytesRead = read(dst, pos)
        if (bytesRead > 0) pos += bytesRead
        return bytesRead
    }

    fun read(dst: ByteBuffer, position: Long): Int {
        if (!open) throw ClosedChannelException()
        if (position < 0) throw IllegalArgumentException("Position out of bounds: $position")
        if (position >= totalSize) return -1

        val requestSize = dst.remaining()
        if (requestSize == 0) return 0

        if (requestSize > DIRECT_READ_THRESHOLD) {
            return handleLargeRead(dst, position)
        }

        var totalBytesRead = 0
        var currentPos = position
        if (isCacheHit(currentPos, 1)) {
            val bytesFromCache = readFromCache(dst, currentPos)
            totalBytesRead += bytesFromCache
            currentPos += bytesFromCache
        }

        if (dst.hasRemaining() && currentPos < totalSize) {
            loadCache(currentPos, requestSize)
            totalBytesRead += if (isCacheHit(currentPos, dst.remaining())) {
                readFromCache(dst, currentPos)
            } else {
                readDirectly(dst, currentPos)
            }
        }

        return totalBytesRead
    }

    fun slice(offset: Long, sliceSize: Long): DataSourceChannel {
        if (offset == 0L && sliceSize == totalSize) return this
        if (offset < 0 || sliceSize <= 0 || offset + sliceSize > totalSize) {
            throw IllegalArgumentException("Invalid slice parameters")
        }
        return DataSourceChannel(client, url, fileChannel, startOffset + offset, sliceSize)
    }

    /** Reads up to [length] bytes at [position] into a fresh array. */
    fun readFully(position: Long, length: Long): ByteArray? {
        if (length > Int.MAX_VALUE) return null
        val buffer = ByteBuffer.allocate(length.toInt())
        val bytesRead = read(buffer, position)
        if (bytesRead <= 0) return null
        buffer.flip()
        val data = ByteArray(bytesRead)
        buffer.get(data)
        return data
    }

    fun streamRead(position: Long, length: Long): InputStream {
        val endPosition = minOf(position + length, totalSize) + startOffset
        val startPosition = startOffset + position
        val readLength = endPosition - startPosition

        if (fileChannel != null) {
            fileChannel.position(startPosition)
            return BoundedInputStream.builder()
                .setInputStream(Channels.newInputStream(fileChannel))
                .setMaxCount(readLength)
                .setPropagateClose(false)
                .get()
        }

        val request = Request.Builder()
            .url(url!!)
            .header("Range", "bytes=$startPosition-${endPosition - 1}")
            .build()

        val response = client!!.newCall(request).execute()
        if (response.code != 206) {
            response.close()
            throw IOException("Unexpected response code ${response.code}")
        }
        return response.body.byteStream()
    }

    private fun handleLargeRead(dst: ByteBuffer, position: Long): Int {
        var bytesFromCache = 0
        var currentPos = position
        if (isCacheHit(currentPos, 1)) {
            bytesFromCache = readFromCache(dst, currentPos)
            currentPos += bytesFromCache
        }

        return if (dst.hasRemaining() && currentPos < totalSize) {
            bytesFromCache + readDirectly(dst, currentPos)
        } else {
            bytesFromCache
        }
    }

    private fun loadCache(requestPos: Long, requestSize: Int) {
        val lastCacheEnd = cache?.let { cacheStart + it.size } ?: -1L
        val newCacheSize: Int
        val newCacheStart: Long

        if (requestSize > SEQ_READ_THRESHOLD || lastCacheEnd == requestPos) {
            newCacheSize = SEQ_READ_CACHE_SIZE
            newCacheStart = requestPos
        } else {
            newCacheSize = RANDOM_READ_CACHE_SIZE
            newCacheStart = maxOf(0L, requestPos - newCacheSize / 2)
        }

        loadCacheAt(newCacheStart, newCacheSize)
    }

    private fun loadCacheAt(cacheStart: Long, cacheSize: Int) {
        val maxEnd = minOf(cacheStart + cacheSize, totalSize)
        val start = maxOf(0L, maxEnd - cacheSize)

        val buffer = ByteBuffer.allocate((maxEnd - start).toInt())
        val bytesRead = readDirectly(buffer, start)
        if (bytesRead != buffer.capacity()) {
            throw IOException("Failed to fill cache.")
        }

        cache = buffer.array()
        this.cacheStart = start
    }

    private fun isCacheHit(pos: Long, bytesToRead: Int): Boolean {
        val cache = cache ?: return false
        val cacheEnd = cacheStart + cache.size
        val readEnd = minOf(pos + bytesToRead, totalSize)
        return pos >= cacheStart && readEnd <= cacheEnd
    }

    private fun readFromCache(dst: ByteBuffer, position: Long): Int {
        val cache = cache ?: return 0
        val relativePos = position - cacheStart
        val available = minOf(dst.remaining().toLong(), cache.size - relativePos).toInt()
        dst.put(cache, relativePos.toInt(), available)
        return available
    }

    private fun readDirectly(dst: ByteBuffer, position: Long): Int {
        Channels.newChannel(streamRead(position, dst.remaining().toLong())).use { channel ->
            var totalBytesRead = 0
            while (true) {
                val bytesRead = channel.read(dst)
                if (bytesRead <= 0) break
                totalBytesRead += bytesRead
            }
            return totalBytesRead
        }
    }

    override fun write(src: ByteBuffer): Int = throw NonWritableChannelException()

    override fun truncate(size: Long): DataSourceChannel = throw NonWritableChannelException()

    companion object {
        private const val RANDOM_READ_CACHE_SIZE = 16 * 1024
        private const val SEQ_READ_CACHE_SIZE = 1024 * 1024
        private const val SEQ_READ_THRESHOLD = 1024
        private const val DIRECT_READ_THRESHOLD = 512 * 1024

        private fun fetchTotalSize(client: OkHttpClient, url: String): Long {
            val request = Request.Builder().url(url).head().build()
            client.newCall(request).execute().use { response ->
                if (!response.isSuccessful) {
                    throw IOException("Failed to connect to URL: $response")
                }
                val contentLength = response.header("Content-Length")
                    ?: throw IOException("Could not determine file size.")
                val acceptRanges = response.header("Accept-Ranges")
                if (acceptRanges == null || !acceptRanges.equals("bytes", ignoreCase = true)) {
                    throw IOException("Server does not support byte ranges: $response")
                }
                return contentLength.toLong()
            }
        }
    }
}
