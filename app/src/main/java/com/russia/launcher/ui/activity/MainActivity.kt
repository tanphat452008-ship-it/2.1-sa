package com.russia.launcher.ui.activity

import android.content.Intent
import android.graphics.PorterDuff
import android.net.Uri
import android.os.Bundle
import android.view.View
import android.widget.FrameLayout
import android.widget.ImageView
import android.widget.LinearLayout
import android.widget.TextView
import androidx.appcompat.app.AppCompatActivity
import androidx.core.content.ContextCompat
import androidx.core.view.WindowCompat
import androidx.core.view.WindowInsetsCompat
import androidx.core.view.WindowInsetsControllerCompat
import androidx.fragment.app.Fragment
import com.russia.game.R
import com.russia.game.core.Samp
import com.russia.launcher.config.Config.DONATE_URL
import com.russia.launcher.config.Config.FORUM_URL
import com.russia.launcher.domain.enums.StorageElements
import com.russia.launcher.service.impl.ActivityServiceImpl
import com.russia.launcher.storage.NativeStorage
import com.russia.launcher.storage.Storage
import com.russia.launcher.ui.dialogs.EnterLockedServerPasswordDialog
import com.russia.launcher.ui.fragment.MonitoringFragment
import com.russia.launcher.ui.fragment.SettingsFragment
import org.apache.commons.lang3.StringUtils
import java.io.File

class MainActivity : AppCompatActivity() {

    private lateinit var donateButton: LinearLayout
    private lateinit var donateImage: ImageView
    private lateinit var donateTV: TextView
    private lateinit var monitoringButton: LinearLayout
    private lateinit var monitoringImage: ImageView
    private lateinit var monitoringTV: TextView
    private lateinit var playButton: LinearLayout
    private lateinit var playImage: ImageView
    private lateinit var rouletteButton: LinearLayout
    private lateinit var rouletteImage: ImageView
    private lateinit var rouletteTV: TextView
    private lateinit var settingsButton: LinearLayout
    private lateinit var settingsImage: ImageView
    private lateinit var settingsTV: TextView
    private lateinit var containerLayout: FrameLayout

    private val monitoringFragment by lazy { MonitoringFragment() }
    private val settingsFragment by lazy { SettingsFragment() }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setTheme(R.style.AppBaseTheme)
        setContentView(R.layout.activity_main)

        hideSystemUI()
        initViews()

        if (savedInstanceState != null && savedInstanceState.getBoolean(IS_AFTER_LOADING_KEY)) {
            replaceFragment(settingsFragment)
        } else if (savedInstanceState == null && intent.extras?.getBoolean(IS_AFTER_LOADING_KEY) == true) {
            onClickSettings()
        } else {
            replaceFragment(monitoringFragment)
        }

        setupClickListeners()
    }

    private fun initViews() {
        containerLayout = findViewById(R.id.container)
        monitoringTV = findViewById(R.id.monitoringTV)
        settingsTV = findViewById(R.id.settingsTV)
        rouletteTV = findViewById(R.id.forumTV)
        donateTV = findViewById(R.id.donateTV)

        monitoringImage = findViewById(R.id.monitoringImage)
        settingsImage = findViewById(R.id.settingsImage)
        rouletteImage = findViewById(R.id.forumImage)
        donateImage = findViewById(R.id.donateImage)
        playImage = findViewById(R.id.playImage)

        monitoringButton = findViewById(R.id.monitoringButton)
        settingsButton = findViewById(R.id.settingsButton)
        rouletteButton = findViewById(R.id.rouletteButton)
        donateButton = findViewById(R.id.donateButton)
        playButton = findViewById(R.id.playButton)
    }

    private fun setupClickListeners() {
        monitoringButton.setOnClickListener { onClickMonitoring() }
        settingsButton.setOnClickListener { onClickSettings() }
        
        rouletteButton.setOnClickListener {
            openUrl(FORUM_URL)
        }
        
        donateButton.setOnClickListener { onClickDonate() }
        playButton.setOnClickListener { onClickPlay() }
    }

    private fun hideSystemUI() {
        WindowCompat.setDecorFitsSystemWindows(window, false)
        WindowInsetsControllerCompat(window, window.decorView).let { controller ->
            controller.hide(WindowInsetsCompat.Type.systemBars())
            controller.systemBarsBehavior = WindowInsetsControllerCompat.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE
        }
    }

    private fun onClickPlay() {
        startGame()
    }

    private fun startGame() {
        // Dọn dẹp cache / log
        val filesToDelete = listOf("/log.txt", "/CINFO.BIN", "/models/MINFO.BIN")
        filesToDelete.forEach { path ->
            File(getExternalFilesDir(null).toString() + path).delete()
        }

        val nickname = NativeStorage.getClientProperty("name", this)
        val selectedServer = NativeStorage.getClientProperty("server", this)

        if (StringUtils.isBlank(nickname)) {
            ActivityServiceImpl.showErrorMessage("Укажите ник!", this)
            onClickSettings()
            return
        }

        if (StringUtils.isBlank(selectedServer)) {
            ActivityServiceImpl.showErrorMessage("Выберите сервер", this)
            onClickMonitoring()
            return
        }

        val tmp = Storage.getProperty(StorageElements.SERVER_LOCKED, this)
        val serverLockedValue = tmp?.toIntOrNull() ?: 0

        if (SERVER_LOCKED_VALUE == serverLockedValue) {
            val dialog = EnterLockedServerPasswordDialog(this)
            dialog.setOnDialogCloseListener { password -> saveServerPassword(password) }
            dialog.createDialog()
            return
        } else {
            NativeStorage.addClientProperty("password", StringUtils.EMPTY, this)
        }

        launchGame()
    }

    private fun saveServerPassword(password: String) {
        NativeStorage.addClientProperty("password", password, this)
        launchGame()
    }

    private fun launchGame() {
        val intent = Intent(this, Samp::class.java).apply {
            flags = Intent.FLAG_ACTIVITY_CLEAR_TASK or Intent.FLAG_ACTIVITY_NEW_TASK
        }
        startActivity(intent)
        finish()
    }

    private fun onClickSettings() {
        setTextColor(settingsButton, settingsTV, settingsImage)
        replaceFragment(settingsFragment)
    }

    private fun onClickDonate() {
        openUrl(DONATE_URL)
    }

    private fun onClickMonitoring() {
        setTextColor(monitoringButton, monitoringTV, monitoringImage)
        replaceFragment(monitoringFragment)
    }

    private fun openUrl(url: String) {
        runCatching {
            val intent = Intent(Intent.ACTION_VIEW, Uri.parse(url))
            startActivity(intent)
        }
    }

    private fun setTextColor(activeLayout: LinearLayout, activeTextView: TextView, activeImageView: ImageView) {
        val disableColor = ContextCompat.getColor(this, R.color.menuTextDisable)
        val enableColor = ContextCompat.getColor(this, R.color.menuTextEnable)

        listOf(monitoringButton, settingsButton, rouletteButton, donateButton).forEach {
            it.alpha = 0.45f
        }
        listOf(monitoringTV, settingsTV, rouletteTV, donateTV).forEach {
            it.setTextColor(disableColor)
        }
        listOf(monitoringImage, settingsImage, rouletteImage, donateImage).forEach {
            it.setColorFilter(disableColor, PorterDuff.Mode.SRC_IN)
        }

        activeLayout.alpha = 1.0f
        activeTextView.setTextColor(enableColor)
        activeImageView.setColorFilter(enableColor, PorterDuff.Mode.SRC_IN)
    }

    private fun replaceFragment(fragment: Fragment) {
        supportFragmentManager.beginTransaction()
            .replace(R.id.container, fragment)
            .commitAllowingStateLoss()
    }

    companion object {
        private const val IS_AFTER_LOADING_KEY = "isAfterLoading"
        private const val SERVER_LOCKED_VALUE = 1
        private const val TEST_MODE_ON_VALUE = "1"
    }
}
