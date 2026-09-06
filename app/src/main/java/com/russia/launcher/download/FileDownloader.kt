//////////////////////////////////////////
//  CROSS SYSTEM
//  Author: Cross
//  Telegram: https://t.me/taskJson
//  Date: 23.11.2025
//  Private Development
//////////////////////////////////////////

package com.russia.launcher.download

import com.russia.launcher.NetworkService
import com.russia.launcher.async.dto.response.FileInfo
import com.russia.launcher.config.Config.APK_FILE_NAME
import com.russia.launcher.config.Config.ZIP_FILES_BASE_ADR
import com.russia.launcher.ui.activity.LoaderActivity
import com.russia.launcher.utils.BytesTo
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.GlobalScope
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import java.io.File
import java.io.FileInputStream
import java.io.FileOutputStream
import java.net.URL
import java.util.zip.ZipEntry
import java.util.zip.ZipInputStream

interface DownloadListener {
    fun onDownloadComplete()
    fun onDownloadFailed()
}

class FileDownloader(
    private val loaderActivity: LoaderActivity,
    private var filesList: MutableList<FileInfo>
) {

    private var lastTime: Long = System.currentTimeMillis()
    private var lastDownloaded: Long = 0
    private var curSpeed: Long = 0

    private var totalFilesSize: Long = 0
    private var totalDownloadedSize: Long = 0

    private var downloadListener: DownloadListener? = null

    private var leftFilesList: MutableList<FileInfo> = filesList.toMutableList()

    fun setDownloadListener(listener: DownloadListener) {
        downloadListener = listener
    }
    fun downloadAndUnzipFiles() {
        filesList = leftFilesList.toMutableList()
        totalFilesSize = filesList.sumOf { it.size }
        totalDownloadedSize = 0
        lastTime = System.currentTimeMillis()
        lastDownloaded = 0
        curSpeed = 0

        GlobalScope.launch(Dispatchers.Default) {
            for (file in filesList) {
                try {
                    downloadAndUnzipFile(file)
                    leftFilesList.remove(file)
                } catch (e: Exception) {
                    e.printStackTrace()
                    withContext(Dispatchers.Main) {
                        downloadListener?.onDownloadFailed()
                    }
                    return@launch
                }
            }
            withContext(Dispatchers.Main) {
                downloadListener?.onDownloadComplete()
            }
        }
    }
    fun downloadAndInstallFile() {
        totalFilesSize = filesList.sumOf { it.size }
        totalDownloadedSize = 0
        lastTime = System.currentTimeMillis()
        lastDownloaded = 0
        curSpeed = 0

        GlobalScope.launch(Dispatchers.Default) {
            try {
                val apkTarget = File(
                    loaderActivity.getExternalFilesDir(null),
                    APK_FILE_NAME
                )

                downloadFile(
                    NetworkService.APK_URL,
                    apkTarget
                )

                loaderActivity.installApk()

                withContext(Dispatchers.Main) {
                    downloadListener?.onDownloadComplete()
                }
            } catch (e: Exception) {
                e.printStackTrace()
                withContext(Dispatchers.Main) {
                    downloadListener?.onDownloadFailed()
                }
            }
        }
    }
    private suspend fun downloadAndUnzipFile(fileInfo: FileInfo) = withContext(Dispatchers.IO) {
        val baseDir = loaderActivity.getExternalFilesDir(null)
            ?: throw IllegalStateException("External files dir is null")

        val sourceUrl = if (fileInfo.path.startsWith("http"))
            fileInfo.path
        else
            ZIP_FILES_BASE_ADR + fileInfo.path + ".zip"
        val zipNameFromUrl = try {
            URL(sourceUrl).path.substringAfterLast('/').ifEmpty { "cache.zip" }
        } catch (e: Exception) {
            "cache.zip"
        }

        val zipFile = File(baseDir, zipNameFromUrl)

        downloadFile(sourceUrl, zipFile)

        println("Скачали файл ${fileInfo.path} -> ${zipFile.absolutePath}")

        unzipFile(zipFile, baseDir)
    }

    private fun downloadFile(from: String, outputFile: File) {
        val url = URL(from)
        val connection = url.openConnection().apply {
            connectTimeout = 15000
            readTimeout = 30000
        }

        if (totalFilesSize == 0L) {
            val len = connection.contentLengthLong
            if (len > 0) {
                totalFilesSize = len
            }
        }

        val inputStream = connection.getInputStream()

        outputFile.parentFile?.mkdirs()

        val outputStream = FileOutputStream(outputFile)
        val buffer = ByteArray(64 * 1024)
        var bytesRead: Int

        while (inputStream.read(buffer).also { bytesRead = it } != -1) {
            outputStream.write(buffer, 0, bytesRead)

            totalDownloadedSize += bytesRead.toLong()

            val percentDownloaded = if (totalFilesSize > 0)
                (totalDownloadedSize.toDouble() / totalFilesSize.toDouble() * 100).toInt()
            else
                0

            val currentTime = System.currentTimeMillis()
            val time = currentTime - lastTime
            if (time >= 1000) {
                curSpeed = totalDownloadedSize - lastDownloaded
                lastDownloaded = totalDownloadedSize
                lastTime = currentTime
            }

            val text = String.format(
                "%s из %s (%s / сек.)",
                BytesTo.convert(totalDownloadedSize),
                if (totalFilesSize > 0) BytesTo.convert(totalFilesSize) else "???",
                BytesTo.convert(curSpeed)
            )

            loaderActivity.updateProgress(
                percentDownloaded.coerceIn(0, 100),
                outputFile.name,
                text
            )
        }

        inputStream.close()
        outputStream.close()
    }

    private fun unzipFile(zipFile: File, targetDir: File) {
        if (!targetDir.exists()) targetDir.mkdirs()

        val zis = ZipInputStream(FileInputStream(zipFile))
        val buffer = ByteArray(64 * 1024)

        loaderActivity.runOnUiThread {
            loaderActivity.speedText.text = "Распаковка архива..."
        }

        var entry: ZipEntry? = zis.nextEntry
        while (entry != null) {
            val outFile = File(targetDir, entry.name)

            if (entry.isDirectory) {
                outFile.mkdirs()
            } else {
                outFile.parentFile?.mkdirs()
                val fos = FileOutputStream(outFile)
                var len: Int
                while (zis.read(buffer).also { len = it } > 0) {
                    fos.write(buffer, 0, len)
                }
                fos.close()
            }

            loaderActivity.updateProgress(
                100,
                "Распаковка: ${entry.name}",
                "Распаковка файлов..."
            )

            zis.closeEntry()
            entry = zis.nextEntry
        }

        zis.close()
        zipFile.delete()

        println("Распаковка завершена: ${zipFile.absolutePath}")
    }
}
//////////////////////////////////////////
//  CROSS SYSTEM
//  Author: Cross
//  Telegram: https://t.me/taskJson
//  Date: 23.11.2025
//  Private Development
//////////////////////////////////////////
