package com.russia.launcher;

import com.russia.launcher.async.dto.response.GameFileInfoDto;
import com.russia.launcher.async.dto.response.LatestVersionInfoDto;
import com.russia.launcher.async.dto.response.LoaderSliderInfoResponseDto;
import com.russia.launcher.async.dto.response.MonitoringData;

import retrofit2.Call;
import retrofit2.http.GET;
import retrofit2.http.Headers;

public interface NetworkService {

    // =========================================================
    // SARP DATA SERVER
    // =========================================================

    String FILES_BASE_ADR =
            "http://sa-rp.net:8642/data-mobile3/";

    // =========================================================
    // GAME FILE LIST
    // =========================================================

    String FILE_INFO_URL =
            "http://sa-rp.net:8642/data-mobile3/file_sort.json";


    // =========================================================
    // APK
    // =========================================================

    String APK_URL =
            "https://files.liverussia.online/apk/release/app-ver_release-release.apk";


    // =========================================================
    // MONITORING
    // =========================================================

    @Headers("Content-Type: application/json")
    @GET("http://sa-rp.net:8642/data-mobile/")
    Call<MonitoringData> getMonitoringData();

    @Headers("Content-Type: application/json")
    @GET(FILE_INFO_URL)
    Call<GameFileInfoDto> getFilesList();

    @Headers("Content-Type: application/json")
    @GET("https://sarphost.sa-rp.net/SARP-MB-nguyenvantinh/SARP-Ver2/texts.json")
    Call<LoaderSliderInfoResponseDto> getLoaderSliderInfo();

    @Headers("Content-Type: application/json")
    @GET("https://sarphost.sa-rp.net/SARP-MB-nguyenvantinh/SARP-Ver2/apk_info.json")
    Call<LatestVersionInfoDto> getLatestVersionInfoDto();
}
