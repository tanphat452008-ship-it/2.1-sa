//////////////////////////////////////////
//  CROSS SYSTEM
//  Author: Cross
//  Telegram: https://t.me/taskJson
//  Date: 23.11.2025
//  Private Development
//////////////////////////////////////////

package com.russia.launcher.ui.activity

import android.content.Intent
import android.graphics.RenderEffect
import android.graphics.Shader
import android.net.Uri
import android.os.Build
import android.os.Bundle
import android.view.View
import android.widget.ImageView
import android.widget.TextView
import androidx.appcompat.app.AppCompatActivity
import androidx.constraintlayout.widget.ConstraintLayout
import com.russia.game.R
import com.russia.game.core.Samp
import com.russia.launcher.async.dto.response.FileInfo
import com.russia.launcher.config.Config.DONATE_URL
import com.russia.launcher.config.Config.FORUM_URL
import com.russia.launcher.domain.enums.DownloadType
import com.russia.launcher.domain.enums.StorageElements
import com.russia.launcher.service.impl.ActivityServiceImpl
import com.russia.launcher.storage.NativeStorage
import com.russia.launcher.storage.Storage
import com.russia.launcher.ui.dialogs.EnterLockedServerPasswordDialog
import com.russia.launcher.utils.MainUtils
import org.apache.commons.lang3.StringUtils
import java.io.File

class MainActivity : AppCompatActivity() {

    private lateinit var userNameTV: TextView
    private lateinit var serverNameTV: TextView
    private lateinit var playButtonLayout: ConstraintLayout
    private lateinit var settingButton: ImageView

    private lateinit var tgButton: ImageView
    private lateinit var ytButton: ImageView
    private lateinit var vkButton: ImageView
    private lateinit var dsButton: ImageView

    private lateinit var mainRoot: ConstraintLayout
    private lateinit var contentRoot: ConstraintLayout
    private lateinit var accountOverlayRoot: ConstraintLayout
    private var userAccountOverlay: UserAccountOverlay? = null

    private lateinit var loadingOverlayRoot: ConstraintLayout
    private lateinit var loadingText: TextView

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setTheme(R.style.AppBaseTheme)
        setContentView(R.layout.crosss_main)

        window.decorView.systemUiVisibility = (
                View.SYSTEM_UI_FLAG_IMMERSIVE
                        or View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
                        or View.SYSTEM_UI_FLAG_LAYOUT_STABLE
                        or View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
                        or View.SYSTEM_UI_FLAG_FULLSCREEN
                        or View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY
                )

        bindViews()
        initOverlay()
        setLoadingState(false)
        fillUserInfo()
        setupClicks()
    }

    private fun bindViews() {
        mainRoot = findViewById(R.id.mainRoot)
        contentRoot = findViewById(R.id.contentRoot)

        userNameTV = findViewById(R.id.userName)
        serverNameTV = findViewById(R.id.serverName)
        playButtonLayout = findViewById(R.id.playButt)
        settingButton = findViewById(R.id.settingButton)

        tgButton = findViewById(R.id.tgButton)
        ytButton = findViewById(R.id.ytButton)
        vkButton = findViewById(R.id.vkButton)
        dsButton = findViewById(R.id.dsButton)

        accountOverlayRoot = findViewById(R.id.accountOverlayRoot)
        loadingOverlayRoot = findViewById(R.id.loadingOverlayRoot)
        loadingText = findViewById(R.id.loadingText)
    }

    private fun initOverlay() {
        accountOverlayRoot.visibility = View.GONE
        userAccountOverlay = UserAccountOverlay(this, accountOverlayRoot, contentRoot)
    }

    private fun fillUserInfo() {
        val nickname = NativeStorage.getClientProperty("name", this)
        if (!nickname.isNullOrBlank()) userNameTV.text = nickname

        val serverName = NativeStorage.getClientProperty("serverName", this)
        val serverCode = NativeStorage.getClientProperty("server", this)

        serverNameTV.text = when {
            !serverName.isNullOrBlank() -> serverName
            !serverCode.isNullOrBlank() -> serverCode
            else -> getString(R.string.select_server_)
        }
    }

    private fun setupClicks() {
        playButtonLayout.setOnClickListener { onClickPlay() }

        settingButton.setOnClickListener {
            ActivityServiceImpl.showErrorMessage("Раздел настроек пока не подключен", this)
        }

        findViewById<ConstraintLayout>(R.id.userLayout)?.setOnClickListener { showUserAccountOverlay() }
        userNameTV.setOnClickListener { showUserAccountOverlay() }

        tgButton.setOnClickListener { openLink(FORUM_URL) }
        ytButton.setOnClickListener { openLink(FORUM_URL) }
        vkButton.setOnClickListener { openLink(FORUM_URL) }
        dsButton.setOnClickListener { openLink(DONATE_URL) }
    }

    private fun showUserAccountOverlay() {
        userAccountOverlay?.show()
    }

    fun onNicknameChanged(newNick: String) {
        userNameTV.text = newNick
    }

    private fun openLink(url: String) {
        startActivity(Intent(Intent.ACTION_VIEW, Uri.parse(url)))
    }

    private fun onClickPlay() {
        if (!validateBeforeStart()) return
    /*
        if (!isCheckSkipping && !isCachePresent()) {
            MainUtils.type = DownloadType.RELOAD_GAME_FILES
            startActivity(Intent(this, LoaderActivity::class.java))
            return
        }

     */

        loadingText.text = "Запуск игры..."
        setLoadingState(true)

        mainRoot.postDelayed({ startGameInternal() }, 300)
    }

    private fun validateBeforeStart(): Boolean {
        val nickname = NativeStorage.getClientProperty("name", this)
        val selectedServer = NativeStorage.getClientProperty("server", this)

//        if (StringUtils.isBlank(nickname)) {
//            ActivityServiceImpl.showErrorMessage("Укажите ник!", this)
//            return false
//        }

//        if (StringUtils.isBlank(selectedServer)) {
//            ActivityServiceImpl.showErrorMessage("Выберите сервер", this)
//            return false
//        }

        val tmp = Storage.getProperty(StorageElements.SERVER_LOCKED, this)
        val serverLockedValue = tmp?.toIntOrNull() ?: 0

        if (SERVER_LOCKED_VALUE == serverLockedValue) {
            val dialog = EnterLockedServerPasswordDialog(this)
            dialog.setOnDialogCloseListener { saveServerPassword(it) }
            dialog.createDialog()
            return false
        }

        NativeStorage.addClientProperty("password", StringUtils.EMPTY, this)
        return true
    }

    private fun isCachePresent(): Boolean {
        val baseDir = getExternalFilesDir(null) ?: return false
        val gta3Img = File(baseDir, "texdb/gta3.img")
        return gta3Img.exists()
    }

    private fun startGameInternal() {
        File(getExternalFilesDir(null), "log.txt").delete()
        File(getExternalFilesDir(null), "CINFO.BIN").delete()
        File(getExternalFilesDir(null), "models/MINFO.BIN").delete()

        val intent = Intent(this, Samp::class.java)
        intent.flags = Intent.FLAG_ACTIVITY_CLEAR_TASK or Intent.FLAG_ACTIVITY_CLEAR_TOP
        startActivity(intent)
        finish()
    }

    private fun saveServerPassword(password: String) {
        NativeStorage.addClientProperty("password", password, this)
        startGameInternal()
    }

    private fun setLoadingState(isLoading: Boolean) {
        if (isLoading) {
            loadingOverlayRoot.visibility = View.VISIBLE

            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
                contentRoot.setRenderEffect(
                    RenderEffect.createBlurEffect(30f, 30f, Shader.TileMode.CLAMP)
                )
            } else {
                contentRoot.alpha = 0.4f
            }
        } else {
            loadingOverlayRoot.visibility = View.GONE
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
                contentRoot.setRenderEffect(null)
            } else {
                contentRoot.alpha = 1f
            }
        }
    }

    private val isCheckSkipping: Boolean
        get() = NativeStorage.getClientProperty("test", this) == TEST_MODE_ON_VALUE

    private fun doAfterCacheChecked(fileToReload: MutableList<FileInfo>) {
        if (fileToReload.isEmpty()) startGameInternal()
        else {
            MainUtils.FILES_TO_RELOAD = fileToReload
            MainUtils.type = DownloadType.RELOAD_GAME_FILES
            startActivity(Intent(this, LoaderActivity::class.java))
        }
    }

    companion object {
        private const val SERVER_LOCKED_VALUE = 1
        private const val TEST_MODE_ON_VALUE = "1"
    }
}
