#include "core/modules.h"
#include <HTTPClient.h>
#include <esp_ota_ops.h>
#include <esp_app_format.h>
#include <mbedtls/sha256.h>
#include "generated/github_roots.h"

namespace {
const char* REPO = "dineshpalsingh1989/Wilcoxon-883M_RTU-to-TCP-and-MQTT";
const char* ASSET = "firmware-adafruit_feather_esp32s3.bin";
char updateState[24] = "idle", updateMessage[160] = "No update running";
unsigned updateProgress = 0;
String requestedTag;

void status(const char* state, const char* message, unsigned progress = 0) {
    if (xSemaphoreTake(dataMutex, portMAX_DELAY) == pdTRUE) {
        snprintf(updateState, sizeof(updateState), "%s", state);
        snprintf(updateMessage, sizeof(updateMessage), "%s", message);
        updateProgress = progress;
        xSemaphoreGive(dataMutex);
    }
}
bool allowedUrl(const String& url) {
    return url.startsWith("https://api.github.com/") || url.startsWith("https://github.com/") ||
           url.startsWith("https://release-assets.githubusercontent.com/") || url.startsWith("https://objects.githubusercontent.com/");
}
int get(HTTPClient& http, WiFiClientSecure& client, String url) {
    client.setCACert(GITHUB_ROOTS);
    client.setTimeout(15); // WiFiClientSecure uses seconds, HTTPClient uses milliseconds.
    client.setHandshakeTimeout(15);
    for (unsigned redirect = 0; redirect < 5; redirect++) {
        if (!allowedUrl(url)) return -100;
        if (!http.begin(client, url)) return -101;
        http.setConnectTimeout(15000); http.setTimeout(15000); http.useHTTP10(true);
        http.setFollowRedirects(HTTPC_DISABLE_FOLLOW_REDIRECTS);
        http.setUserAgent("Wilcoxon-883M-Gateway");
        const char* headers[] = {"Location"}; http.collectHeaders(headers, 1);
        int code = http.GET();
        if (code == 301 || code == 302 || code == 303 || code == 307 || code == 308) {
            url = http.header("Location"); http.end(); continue;
        }
        return code;
    }
    return -102;
}
bool paused() {
    if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(50)) != pdTRUE) return false;
    bool ready = backgroundPausedForUpdate;
    xSemaphoreGive(dataMutex); return ready;
}
bool installRelease(String tag, String& error) {
    const uint32_t started = millis();
    while (!paused() && millis() - started < 15000) vTaskDelay(pdMS_TO_TICKS(50));
    if (!paused()) { error = "Unable to pause sensor task"; return false; }
    if (WiFi.status() != WL_CONNECTED) { error = "Connect gateway to Wi-Fi first"; return false; }
    if (time(nullptr) < 1700000000) { error = "Set gateway time before HTTPS verification"; return false; }
    status("checking", "Checking board firmware and SHA-256 metadata");
    WiFiClientSecure client;
    HTTPClient http;
    String api = String("https://api.github.com/repos/") + REPO + "/releases/tags/" + tag;
    if (get(http, client, api) != 200) { http.end(); error = "Cannot read public GitHub release (check network, TLS or API limit)"; return false; }
    // Parse only the metadata needed, so large release notes do not exhaust RAM.
    StaticJsonDocument<512> filter;
    filter["tag_name"] = true; filter["draft"] = true; filter["prerelease"] = true;
    filter["assets"][0]["name"] = true; filter["assets"][0]["size"] = true;
    filter["assets"][0]["digest"] = true; filter["assets"][0]["browser_download_url"] = true;
    DynamicJsonDocument metadata(12288);
    auto parsed = deserializeJson(metadata, http.getStream(), DeserializationOption::Filter(filter));
    http.end();
    if (parsed || metadata["draft"] == true || metadata["prerelease"] == true || metadata["tag_name"].as<String>() != tag) { error = "Invalid or non-stable release metadata"; return false; }
    String url, digest; size_t size = 0;
    for (JsonObject asset : metadata["assets"].as<JsonArray>()) {
        if (asset["name"].as<String>() == ASSET) { url = asset["browser_download_url"].as<String>(); digest = asset["digest"].as<String>(); size = asset["size"]; break; }
    }
    if (!url.startsWith(String("https://github.com/") + REPO + "/releases/download/" + tag + "/") || !digest.startsWith("sha256:") || digest.length() != 71 || !size) { error = "Matching board binary and GitHub SHA-256 digest are required"; return false; }
    for (unsigned i = 7; i < digest.length(); i++) if (!isxdigit(digest[i])) { error = "Invalid checksum metadata"; return false; }
    const esp_partition_t* partition = esp_ota_get_next_update_partition(nullptr);
    if (!partition || size > partition->size) { error = "Firmware does not fit the inactive OTA partition"; return false; }
    metadata.clear(); metadata.shrinkToFit();
    if (get(http, client, url) != 200 || http.getSize() != int(size)) { http.end(); error = "Firmware download failed or size differs from release"; return false; }
    WiFiClient* stream = http.getStreamPtr();
    stream->setTimeout(15);
    uint8_t buffer[2048];
    const size_t headerSize = sizeof(esp_image_header_t);
    if (stream->readBytes(buffer, headerSize) != headerSize) { http.end(); error = "Incomplete firmware header"; return false; }
    esp_image_header_t header; memcpy(&header, buffer, headerSize);
    if (header.magic != ESP_IMAGE_HEADER_MAGIC || header.chip_id != ESP_CHIP_ID_ESP32S3) { http.end(); error = "Firmware is not an ESP32-S3 application"; return false; }
    if (!Update.begin(size)) { http.end(); error = Update.errorString(); return false; }
    mbedtls_sha256_context sha; mbedtls_sha256_init(&sha); mbedtls_sha256_starts(&sha, 0);
    mbedtls_sha256_update(&sha, buffer, headerSize);
    size_t received = headerSize;
    bool written = Update.write(buffer, headerSize) == headerSize;
    uint32_t lastData = millis(), lastProgress = 0;
    while (written && received < size && millis() - started < 300000) {
        int available = stream->available();
        if (!available) {
            if (!http.connected() || millis() - lastData > 15000) break;
            vTaskDelay(pdMS_TO_TICKS(10)); continue;
        }
        size_t amount = std::min<size_t>(sizeof(buffer), std::min<size_t>(available, size - received));
        int count = stream->read(buffer, amount);
        if (count <= 0) break;
        lastData = millis();
        mbedtls_sha256_update(&sha, buffer, count);
        written = Update.write(buffer, count) == size_t(count);
        received += count;
        unsigned percent = received * 100 / size;
        if (percent != lastProgress) { status("downloading", "Downloading to inactive firmware partition", percent); lastProgress = percent; }
        vTaskDelay(1);
    }
    http.end();
    unsigned char hash[32]; mbedtls_sha256_finish(&sha, hash); mbedtls_sha256_free(&sha);
    char hex[65]; for (int i = 0; i < 32; i++) snprintf(hex + i * 2, 3, "%02x", hash[i]);
    String expected = digest.substring(7); expected.toLowerCase();
    if (!written || received != size || expected != hex) { Update.abort(); error = "Download incomplete or SHA-256 verification failed; current firmware retained"; return false; }
    status("verifying", "Checksum verified; validating firmware image", 100);
    if (!Update.end()) { error = Update.errorString(); Update.abort(); return false; }
    return true;
}
void updateTask(void*) {
    String error;
    if (installRelease(requestedTag, error)) {
        status("rebooting", "Firmware verified. Gateway restarting...", 100);
        vTaskDelay(pdMS_TO_TICKS(2000)); ESP.restart();
    }
    status("failed", error.c_str());
    if (xSemaphoreTake(dataMutex, portMAX_DELAY) == pdTRUE) { firmwareUpdateActive = false; xSemaphoreGive(dataMutex); }
    vTaskDelete(nullptr);
}
}

void handleGithubInstall() {
    DynamicJsonDocument body(256);
    if (deserializeJson(body, server.arg("plain"))) { server.send(400, "text/plain", "Invalid update request"); return; }
    String tag = body["tag"] | "";
    unsigned major, minor, patch, currentMajor, currentMinor, currentPatch; char extra; int versionEnd = 0;
    if (tag.length() > 40 || sscanf(tag.c_str(), "v%u.%u.%u%c", &major, &minor, &patch, &extra) != 3 ||
        sscanf(FIRMWARE_VERSION, "%u.%u.%u%n", &currentMajor, &currentMinor, &currentPatch, &versionEnd) != 3) { server.send(400, "text/plain", "A stable vMAJOR.MINOR.PATCH release is required"); return; }
    if (tag != String("v") + major + "." + minor + "." + patch) { server.send(400, "text/plain", "Invalid release version"); return; }
    // A beta such as 4.10.0-beta.1 may move to the stable 4.10.0 release.
    const bool currentIsBeta = FIRMWARE_VERSION[versionEnd] == '-';
    bool newer = major > currentMajor || (major == currentMajor && (minor > currentMinor || (minor == currentMinor &&
                 (patch > currentPatch || (patch == currentPatch && currentIsBeta)))));
    if (!newer) { server.send(400, "text/plain", "Release must be newer than installed firmware"); return; }
    if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(100)) != pdTRUE) { server.send(503, "text/plain", "Device busy"); return; }
    bool busy = firmwareUpdateActive || currentDataState != DD_IDLE || isReadingWaveform || isReadingSpectrum;
    if (!busy) { firmwareUpdateActive = true; requestedTag = tag; }
    xSemaphoreGive(dataMutex);
    if (busy) { server.send(409, "text/plain", "Wait for capture or update to finish"); return; }
    status("starting", "Pausing sensor monitoring");
    if (xTaskCreate(updateTask, "GithubUpdate", 16384, nullptr, 1, nullptr) != pdPASS) {
        if (xSemaphoreTake(dataMutex, portMAX_DELAY) == pdTRUE) { firmwareUpdateActive = false; xSemaphoreGive(dataMutex); }
        status("failed", "Not enough memory to start updater"); server.send(503, "text/plain", "Not enough memory"); return;
    }
    server.send(202, "application/json", "{\"accepted\":true}");
}

void handleGithubUpdateStatus() {
    DynamicJsonDocument doc(512);
    if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(100)) != pdTRUE) { server.send(503, "text/plain", "Device busy"); return; }
    doc["state"] = updateState; doc["message"] = updateMessage; doc["progress"] = updateProgress; doc["active"] = firmwareUpdateActive;
    xSemaphoreGive(dataMutex);
    String json; serializeJson(doc, json); server.send(200, "application/json", json);
}
