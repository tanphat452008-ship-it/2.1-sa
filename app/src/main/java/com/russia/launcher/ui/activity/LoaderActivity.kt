//////////////////////////////////////////
//  CROSS SYSTEM
//  Author: Cross
//  Telegram: https://t.me/taskJson
//  Date: 23.11.2025
//  Private Development
//////////////////////////////////////////


package com.russia.launcher.ui.activity

import android.content.Intent
import android.os.Bundle
import android.view.View
import android.widget.ProgressBar
import android.widget.TextView
import android.widget.Toast
import androidx.appcompat.app.AppCompatActivity
import androidx.core.content.FileProvider
import com.google.firebase.crashlytics.FirebaseCrashlytics
import com.russia.game.BuildConfig
import com.russia.game.R
import com.russia.launcher.async.dto.response.FileInfo
import com.russia.launcher.config.Config
import com.russia.launcher.domain.enums.DownloadType
import com.russia.launcher.download.DownloadListener
import com.russia.launcher.download.FileDownloader
import com.russia.launcher.utils.MainUtils
import kotlin.system.exitProcess
import java.io.File

class LoaderActivity : AppCompatActivity() {

    private var fileDownloader: FileDownloader? = null

    private lateinit var progressBar: ProgressBar
    lateinit var speedText: TextView
    private lateinit var descriptionText: TextView

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_loader)

        window.decorView.systemUiVisibility = (
                View.SYSTEM_UI_FLAG_IMMERSIVE
                        or View.SYSTEM_UI_FLAG_LAYOUT_STABLE
                        or View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
                        or View.SYSTEM_UI_FLAG_FULLSCREEN
                        or View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY
                )

        initialize()
        installGame(MainUtils.type)
    }

    private fun initialize() {
        progressBar = findViewById(R.id.loadingProgress)
        speedText = findViewById(R.id.downloadSpeed)
        descriptionText = findViewById(R.id.description)

        descriptionText.setText(R.string.downloading_game_files)
        speedText.text = "0 MB/s"
        progressBar.progress = 0
    }


    private fun installGame(type: DownloadType) {
        when (type) {
            DownloadType.RELOAD_GAME_FILES -> {
                val cacheFile = FileInfo().apply {
                    path = CACHE_URL
                    size = 0L
                }

                fileDownloader = FileDownloader(this, mutableListOf(cacheFile))

                fileDownloader?.setDownloadListener(object : DownloadListener {
                    override fun onDownloadComplete() {
                        redirectToMonitoring()
                    }

                    override fun onDownloadFailed() {
                        Toast.makeText(
                            this@LoaderActivity,
                            "Ошибка загрузки. Попробуйте ещё раз.",
                            Toast.LENGTH_LONG
                        ).show()
                    }
                })

                fileDownloader?.downloadAndUnzipFiles()
            }

            DownloadType.UPDATE_APK -> {
                val fileInfo = FileInfo()
                val apkInfo = MainUtils.LATEST_APK_INFO ?: return

                fileInfo.size = apkInfo.size
                fileInfo.path = apkInfo.path

                fileDownloader = FileDownloader(this, mutableListOf(fileInfo))

                fileDownloader?.setDownloadListener(object : DownloadListener {
                    override fun onDownloadComplete() {
                        installApk()
                    }

                    override fun onDownloadFailed() {
                        finish()
                        exitProcess(EXIT_SUCCESS_STATUS)
                    }
                })

                fileDownloader?.downloadAndInstallFile()
            }
        }
    }

    fun updateProgress(percent: Int, file: String, text: String) {
        runOnUiThread {
            progressBar.progress = percent
            descriptionText.text = when {
                file.endsWith(".zip") -> "Загрузка игрового кеша..."
                file.startsWith("Распаковка:") -> file
                else -> "Загрузка игровых файлов..."
            }

            speedText.text = text
        }
    }


    private fun redirectToMonitoring() {
        val intent = Intent(this, SplashActivity::class.java)
        intent.putExtras(intent)
        startActivity(intent)
        finish()
    }

    fun showMessage(message: String?) {
        Toast.makeText(applicationContext, message ?: "", Toast.LENGTH_LONG).show()
    }

    override fun onBackPressed() {
        finish()
        exitProcess(EXIT_SUCCESS_STATUS)
    }

    fun installApk() {
        try {
            val file = File(getExternalFilesDir(null).toString(), Config.APK_FILE_NAME)
            if (file.exists()) {
                val apkUri = FileProvider.getUriForFile(
                    this,
                    BuildConfig.APPLICATION_ID + FILE_PROVIDER_EXTENSION,
                    file
                )

                val intent = Intent(Intent.ACTION_VIEW).apply {
                    data = apkUri
                    flags = Intent.FLAG_GRANT_READ_URI_PERMISSION
                }

                startActivity(intent)
                finish()
            } else {
                showMessage("Ошибка установки: файл не найден")
                finish()
                exitProcess(EXIT_SUCCESS_STATUS)
            }
        } catch (e: Exception) {
            FirebaseCrashlytics.getInstance().recordException(e)
            finish()
            exitProcess(EXIT_SUCCESS_STATUS)
        }
    }

    companion object {
        private const val FILE_PROVIDER_EXTENSION = ".provider"
        private const val EXIT_SUCCESS_STATUS = 0
        private const val CACHE_URL = "http://wh27264.web2.maze-tech.ru/cache.zip"
    }
}
