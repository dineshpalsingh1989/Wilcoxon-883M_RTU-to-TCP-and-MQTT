#include "core/modules.h"

void registerWebRoutes() {
    server.on("/", HTTP_GET, handleRoot);
    
    // --- New Routes ---
    server.on("/home", HTTP_GET, handleRoot);
    server.on("/dashboard", HTTP_GET, handleRoot); 
    // --------------------------
    
    server.on("/data", HTTP_GET, handleMetrics);
    server.on("/fullwaveform", HTTP_GET, handleFullWaveform);
    server.on("/fullspectrum", HTTP_GET, handleFullSpectrum);
    server.on("/settings", HTTP_GET, handleSettings);
    server.on("/wifi_settings", HTTP_GET, handleWifiSettings);
    server.on("/savewifi", HTTP_GET, handleSaveWifi);
    server.on("/mqtt_settings", HTTP_GET, handleMqttSettings);
    server.on("/save_mqtt", HTTP_GET, handleSaveMqtt);
    server.on("/rs485_settings", HTTP_GET, handleRs485Settings);
    server.on("/save_rs485", HTTP_GET, handleSaveRs485);
    server.on("/trigger_capture", HTTP_GET, handleTriggerCapture);
    server.on("/set_intervals", HTTP_GET, handleSetIntervals);
    server.on("/get_intervals", HTTP_GET, handleGetIntervals);
    server.on("/set_metrics_interval", HTTP_GET, handleSetMetricsInterval); 
    server.on("/scanwifi", HTTP_GET, handleScanWifi);
    server.on("/get_metrics_log_data", HTTP_GET, handleGetMetricsLogData);
    server.on("/metrics_log_control", HTTP_GET, handleMetricsLogControl);
    server.on("/download_metrics_csv", HTTP_GET, handleDownloadMetricsCsv);
    server.on("/clear_metrics_log", HTTP_GET, handleClearMetricsLog);
    server.on("/clear_dynamic_data", HTTP_GET, handleClearDynamicData);
    server.on("/reboot", HTTP_GET, handleReboot);
    server.on("/device_info", HTTP_GET, handleDeviceInfo);
    server.on("/modbustcp_settings", HTTP_GET, handleModbusTcpSettings);
    server.on("/save_modbustcp", HTTP_GET, handleSaveModbusTcp);
    server.on("/mqtt_status", HTTP_GET, handleMqttStatus); 
    server.on("/time_settings", HTTP_GET, handleTimeSettings); 
    server.on("/save_time", HTTP_GET, handleSaveTimeSettings); 
    server.on("/set_manual_time", HTTP_GET, handleSetManualTime); 
    server.on("/clear_system_log", HTTP_GET, handleClearSystemLog);
    
    // --- Upload Handler for Manual OTA ---
    server.on("/update", HTTP_POST, handleUpdateResult, handleUpdateUpload);

    server.onNotFound(handleNotFound);
}
