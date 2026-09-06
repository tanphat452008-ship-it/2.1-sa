Lỗi ứng dụng bị treo ở màn hình "Получение данных..." (như trong hình 1000186000.jpg) xuất phát từ 3 lỗ hổng logic trong quá trình xử lý luồng (flow) của mã nguồn:
 * Lỗi kẹt ở hộp thoại xin quyền (Permissions): Trong hàm onRequestPermissionsResult, biến permissionsGranded chỉ được chuyển thành true và gọi startIfReady() nếu người dùng "Cho phép". Nếu người dùng "Từ chối", hàm không làm gì cả, khiến ứng dụng kẹt vĩnh viễn ở Splash Screen.
 * Không ngắt luồng khi mất mạng: Ở hàm onCreate, khi phát hiện !isOnline, ứng dụng hiện thông báo lỗi nhưng lại thiếu lệnh return. Mã nguồn vẫn tiếp tục chạy xuống dưới để gọi API ServersList.load(), gây kẹt luồng mạng.
 * Thiếu cơ chế phòng hờ (Timeout) cho API: Callback của ServersList.load chỉ định nghĩa hàm monitoringDataLoadedSuccess(). Nếu máy chủ lỗi, không phản hồi hoặc phản hồi sai định dạng, sự kiện "Success" sẽ không bao giờ được kích hoạt. Biến monitoringDataLoaded mãi mãi bằng false, chặn hàm startIfReady().
Dưới đây là toàn bộ tệp SplashActivity.kt đã được khắc phục. Các lỗi trên được giải quyết bằng cách thêm lệnh return khi mất mạng, thêm bộ đếm giờ Failsafe dự phòng API bị nghẽn, và đảm bảo ứng dụng luôn đi tiếp dù quyền bị từ chối.
package com.russia.launcher.ui.activity

import android.Manifest
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
import com.airbnb.lottie.LottieDrawable
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

    private var permissionsGranded = false
    private var apkVersionChecked = false
    private var monitoringDataLoaded = false
    private var filesListLoaded = false

    private val REQUEST_ID = 228
    private val permissionList = arrayOf(
        Manifest.permission.RECORD_AUDIO,
    )

    private var networkService: NetworkService
    private lateinit var binding: ActivitySplashBinding

    private val MIN_SPLASH_TIME = 1500L
    private var splashStartTime: Long = 0

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

        splashStartTime = System.currentTimeMillis()

        window.decorView.systemUiVisibility = (
                View.SYSTEM_UI_FLAG_IMMERSIVE
                        or View.SYSTEM_UI_FLAG_LAYOUT_STABLE
                        or View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
                        or View.SYSTEM_UI_FLAG_FULLSCREEN
                        or View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY
                )

        binding.splashAnim.apply {
            repeatCount = LottieDrawable.INFINITE
            if (!isAnimating) {
                playAnimation()
            }
        }

        binding.tvStatus.text = "Получение данных..."

        if (!isOnline) {
            val builder = AlertDialog.Builder(this)
            builder.setTitle("Ошибка!")
                .setMessage("Нет соединения с интернетом")
                .setCancelable(false) // Không cho tắt dialog khi ấn ra ngoài
                .setPositiveButton("Закрыть") { dialog: DialogInterface, _: Int ->
                    dialog.cancel()
                    finishAffinity()
                }
            runOnUiThread { builder.create().show() }
            return // [SỬA LỖI] Ngăn luồng tiếp tục gọi ServersList.load khi mất mạng
        }

        // [SỬA LỖI] Cài đặt Timeout (7 giây) đề phòng API treo không trả dữ liệu
        binding.root.postDelayed({
            if (!monitoringDataLoaded) {
                Log.w("SplashActivity", "Monitoring API Timeout. Forcing continue.")
                monitoringDataLoaded = true
                startIfReady()
            }
        }, 7000)

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
        
        checkPermissions()
        startIfReady()
    }

    private fun loadFilesList() {
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
            // [SỬA LỖI] Bỏ điều kiện bắt buộc "GRANTED". 
            // Nếu người dùng "Từ chối", ứng dụng vẫn được phép vào Menu (tới MainActivity).
            permissionsGranded = true 
            startIfReady()
        }
    }

    private fun goNext() {
        startActivity(Intent(this, MainActivity::class.java))
        finish()
    }

    fun startIfReady() {
        if (permissionsGranded && monitoringDataLoaded /* && apkVersionChecked && filesListLoaded */) {
            val elapsed = System.currentTimeMillis() - splashStartTime
            if (elapsed >= MIN_SPLASH_TIME) {
                goNext()
            } else {
                binding.root.postDelayed(
                    { goNext() },
                    MIN_SPLASH_TIME - elapsed
                )
            }
        }
    }

    private fun checkVersion() {
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
