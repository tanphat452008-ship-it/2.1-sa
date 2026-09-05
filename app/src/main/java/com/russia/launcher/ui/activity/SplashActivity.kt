package com.russia.launcher.ui.activity

import android.Manifest
import android.animation.Animator
import android.animation.AnimatorListenerAdapter
import android.app.AlertDialog
import android.content.DialogInterface
import android.content.Intent
import android.content.pm.PackageManager
import android.net.ConnectivityManager
import android.os.Bundle
import android.util.Log
import android.view.View
import androidx.appcompat.app.AppCompatActivity
import androidx.core.app.ActivityCompat
import androidx.core.content.ContextCompat
import com.google.firebase.crashlytics.FirebaseCrashlytics
import com.google.gson.GsonBuilder
import com.russia.game.databinding.ActivitySplashBinding
import com.russia.launcher.NetworkService
import com.russia.launcher.async.dto.response.GameFileInfoDto
import com.russia.launcher.async.dto.response.LatestVersionInfoDto
import com.russia.launcher.async.dto.response.MonitoringDataLoaderListener
import com.russia.launcher.async.dto.response.ServersList
import com.russia.launcher.async.task.CacheChecker
import com.russia.launcher.config.Config
import com.russia.launcher.domain.enums.DownloadType
import com.russia.launcher.utils.MainUtils
import okhttp3.OkHttpClient
import retrofit2.Call
import retrofit2.Callback
import retrofit2.Response
import retrofit2.Retrofit
import retrofit2.converter.gson.GsonConverterFactory
import java.util.concurrent.TimeUnit
import kotlin.system.exitProcess

class SplashActivity : AppCompatActivity() {

    
    private val IS_API_ENABLED = true
    private var permissionsGranded = true
    private var apkVersionChecked = false
    private var monitoringDataLoaded = true
    private var filesListLoaded = true
    private var animationEnded = true

    private val REQUEST_ID = 228
    private val permissionList = arrayOf(
        Manifest.permission.RECORD_AUDIO,
    )

    private var networkService: NetworkService
    private lateinit var binding: ActivitySplashBinding

    init {
        val gson = GsonBuilder().setLenient().create()

        val okHttpClient = OkHttpClient.Builder()
            .connectTimeout(10, TimeUnit.SECONDS)
            .readTimeout(10, TimeUnit.SECONDS)
            .writeTimeout(10, TimeUnit.SECONDS)
            .build()

        val retrofit = Retrofit.Builder()
            .baseUrl(Config.LIVE_RUSSIA_RESOURCE_SERVER_URL)
            .addConverterFactory(GsonConverterFactory.create(gson))
            .client(okHttpClient)
            .build()

        networkService = retrofit.create(NetworkService::class.java)
    }

    private val isOnline: Boolean
        get() {
            val cm = getSystemService(CONNECTIVITY_SERVICE) as ConnectivityManager
            return cm.activeNetworkInfo != null
        }

    override fun onCreate(savedInstanceState: Bundle?) {
        FirebaseCrashlytics.getInstance().deleteUnsentReports()
        FirebaseCrashlytics.getInstance().setCrashlyticsCollectionEnabled(true)

        super.onCreate(savedInstanceState)
        binding = ActivitySplashBinding.inflate(layoutInflater)
        setContentView(binding.root)

        window.decorView.systemUiVisibility = (View.SYSTEM_UI_FLAG_IMMERSIVE
                or View.SYSTEM_UI_FLAG_LAYOUT_STABLE
                or View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
                or View.SYSTEM_UI_FLAG_FULLSCREEN
                or View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY)

        binding.lottieLogo.addAnimatorListener(object : AnimatorListenerAdapter() {
            override fun onAnimationEnd(animation: Animator) {
                animationEnded = true
                startIfReady()
            }
        })

        if (!isOnline && IS_API_ENABLED) {
            val builder = AlertDialog.Builder(this)
            builder.setTitle("Ошибка!")
                .setMessage("Нет соединения с интернетом")
                .setPositiveButton("Закрыть") { dialog: DialogInterface, _: Int ->
                    dialog.cancel()
                    finishAffinity()
                }
            runOnUiThread { builder.create().show() }
        }

        if (IS_API_ENABLED) {
            ServersList.load(
                this,
                networkService,
                object : MonitoringDataLoaderListener {
                    override fun monitoringDataLoadedSuccess() {
                        monitoringDataLoaded = true
                        startIfReady()
                    }
                }
            )
            loadFilesList()
            checkVersion()
        } else {
            // Tắt API: Đánh dấu thành công toàn bộ bước kiểm tra
            monitoringDataLoaded = true
            filesListLoaded = true
            apkVersionChecked = true
        }

        checkPermissions()
    }

    private fun loadFilesList() {
        if (!IS_API_ENABLED) {
            filesListLoaded = true
            startIfReady()
            return
        }

        val call = networkService.filesList

        call?.enqueue(object : Callback<GameFileInfoDto> {
            override fun onResponse(call: Call<GameFileInfoDto>, response: Response<GameFileInfoDto>) {
                if (response.isSuccessful) {
                    response.body()?.let { CacheChecker.setFilesList(this@SplashActivity, it) }
                }
                filesListLoaded = true
                startIfReady()
            }

            override fun onFailure(call: Call<GameFileInfoDto>, t: Throwable) {
                Log.d("tag", "onFailure = " + t.message)
                filesListLoaded = true
                startIfReady()
            }
        })
    }

    private fun checkPermissions() {
        val permissionsToRequest = mutableListOf<String>()

        for (permission in permissionList) {
            if (ContextCompat.checkSelfPermission(this, permission) != PackageManager.PERMISSION_GRANTED) {
                permissionsToRequest.add(permission)
            }
        }

        if (permissionsToRequest.isNotEmpty()) {
            ActivityCompat.requestPermissions(this, permissionsToRequest.toTypedArray(), REQUEST_ID)
        } else {
            permissionsGranded = true
            startIfReady()
        }
    }

    override fun onRequestPermissionsResult(
        requestCode: Int,
        permissions: Array<out String>,
        grantResults: IntArray
    ) {
        super.onRequestPermissionsResult(requestCode, permissions, grantResults)

        if (requestCode == REQUEST_ID) {
            for (i in grantResults.indices) {
                if (grantResults[i] == PackageManager.PERMISSION_GRANTED) {
                    permissionsGranded = true
                    startIfReady()
                }
            }
        }
    }

    fun startIfReady() {
        if (permissionsGranded && apkVersionChecked && filesListLoaded && monitoringDataLoaded && animationEnded) {
            startActivity(Intent(this, MainActivity::class.java))
            finish()
        }
    }

    private fun checkVersion() {
        if (!IS_API_ENABLED) {
            apkVersionChecked = true
            startIfReady()
            return
        }

        val latestVersionInfoCall = networkService.latestVersionInfoDto
        latestVersionInfoCall?.enqueue(object : Callback<LatestVersionInfoDto?> {
            override fun onResponse(call: Call<LatestVersionInfoDto?>, response: Response<LatestVersionInfoDto?>) {
                if (!response.isSuccessful) {
                    finish()
                    exitProcess(0)
                }
                val currentVersion = currentVersion
                val latestVersion: Int = response.body()?.version?.toInt() ?: 0
                MainUtils.LATEST_APK_INFO = response.body()
                if (currentVersion >= latestVersion) {
                    apkVersionChecked = true
                    startIfReady()
                    return
                }
                MainUtils.type = DownloadType.UPDATE_APK
                startActivity(Intent(this@SplashActivity, LoaderActivity::class.java))
            }

            override fun onFailure(call: Call<LatestVersionInfoDto?>, t: Throwable) {
                finish()
                exitProcess(0)
            }
        })
    }

    private val currentVersion: Int
        get() {
            val pm = this.packageManager
            try {
                val pInfo = pm.getPackageInfo(this.packageName, 0)
                return pInfo.versionCode
            } catch (e1: PackageManager.NameNotFoundException) {
                e1.printStackTrace()
            }
            finish()
            exitProcess(0)
        }

    public override fun onDestroy() {
        super.onDestroy()
    }
}
