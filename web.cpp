#include "web.h"
#include "config.h"
#include "mqtt.h"
#include "version.h"
#include "button.h"
#include "button2.h"
#include "utils.h"
#include "logger.h"
#include <Arduino.h>

#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include <Update.h>
#include <WiFi.h>
#include <esp_system.h>
#include <ArduinoJson.h>
#include <Ticker.h>

// ===================== Globals =====================
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");
Ticker rebooter;

// ===================== UI Text =====================
namespace UI {
  const char* title = "ESP32 Dimmer";
  const char* chip_id = "Chip ID";
  const char* fw_version = "Firmware";

  const char* status_title = "Status";
  const char* status_wifi = "Wi-Fi";
  const char* status_mqtt = "MQTT";
  const char* status_ota = "OTA";
  const char* status_debug = "Debug";
  const char* status_ip = "IP Address";
  const char* status_mdns = "mDNS Hostname";
  const char* status_rssi = "Signal (RSSI)";
  const char* status_uptime = "Uptime";
  const char* status_state = "Light State";
  const char* status_brightness = "Brightness";
  const char* status_heap = "Free Heap";
  const char* status_reset = "Last Reset";

  const char* controls_title = "Controls";
  const char* controls_power = "Power";
  const char* controls_hint_title = "Button Actions (for each channel)";
  const char* ch1_title = "Channel A";
  const char* ch2_title = "Channel B";
  
  const char* settings_light_dim_title = "Light & Dimming";
  const char* settings_hold_sps = "Hold speed (steps/sec)";
  const char* settings_dim_step = "Dim step size (% of range)";
  const char* settings_fade_delay = "Fade step delay (ms)";
  const char* settings_start_b = "Startup brightness";
  const char* settings_min_b = "Min brightness";
  const char* settings_max_b = "Max brightness";
  const char* settings_preset1 = "Preset 1 (2× click)";
  const char* settings_preset2 = "Preset 2 (3× click)";
  const char* settings_transition = "ON/OFF transition (ms)";
  const char* settings_transition_hint = "0 = instant; 100–300ms is pleasant";
  const char* settings_min_on = "Min ON-level";
  const char* settings_ease_fade = "Ease fade (smoother)";
  const char* settings_gamma_corr = "Gamma correction";
  const char* settings_gamma_val = "Gamma (1.2–3.5)";
  const char* settings_long_press = "Long-press threshold (ms)";

  const char* settings_behavior_title = "Behavior";
  const char* settings_start_on = "Start with light ON";
  const char* settings_restore_last = "Restore last brightness";
  const char* settings_reverse_dim = "Reverse dim direction";
  const char* settings_auto_off = "Auto-off timer (minutes, 0=off)";

  const char* settings_global_title = "Global Settings";
  const char* settings_debug_logs = "Enable debug logs";
  const char* settings_ota_web = "Enable OTA";
  
  const char* settings_ha_title = "Home Assistant";
  const char* settings_ha_prefix = "Discovery prefix";
  const char* settings_ha_name = "Device name";
  const char* settings_ha_enable = "Enable HA MQTT Discovery";
  const char* settings_ha_extras = "Include sensors & switches";
  const char* settings_ha_on_boot = "Publish discovery on boot";
  const char* settings_ha_publish = "Publish Now";
  const char* settings_ha_unpublish = "Unpublish Now";

  const char* settings_mqtt_title = "MQTT";
  const char* settings_mqtt_topic = "Base topic A (no /set)";
  const char* settings_mqtt_topic_b = "Base topic B (no /set)";
  const char* settings_mqtt_server = "Server";
  const char* settings_mqtt_port = "Port";
  const char* settings_mqtt_user = "User";
  const char* settings_mqtt_pass = "Password";
  const char* settings_mqtt_pass_unchanged = "(unchanged)";
  const char* settings_mqtt_keepalive = "Keepalive (s)";
  const char* settings_mqtt_backoff_min = "Backoff min (ms)";
  const char* settings_mqtt_backoff_max = "Backoff max (ms)";
  const char* settings_mqtt_telemetry = "Telemetry interval (s)";

  const char* settings_net_web_title = "Network & Web";
  const char* settings_mdns_host = "mDNS Hostname";
  const char* settings_web_user = "Web user";
  const char* settings_web_pass = "Web password";
  const char* settings_ota_pass = "OTA password";

  const char* tools_title = "Tools";
  const char* tools_backup = "Backup Config";
  const char* tools_test_mqtt = "Test MQTT";
  const char* tools_restore = "Restore Config";
  const char* tools_apply_restore = "Apply Restore";
  const char* tools_backup_hint = "Passwords are not included in backups.";

  const char* logs_title = "Live Logs";
  const char* logs_export = "Export Log";

  const char* actions_title = "Actions";
  const char* actions_save_reboot = "Save & Reboot";
  const char* actions_ota_upload = "Firmware Upload";
  const char* actions_wifi_reset = "WiFi Reset";
  const char* actions_factory_reset = "Factory Reset";
  const char* actions_wifi_confirm = "Reset WiFi settings? You will need to reconnect.";
  const char* actions_factory_confirm = "Perform a factory reset? All settings will be lost.";

  const char* ota_page_title = "Firmware Upload";
  const char* ota_label = "Upload firmware (.bin)";
  const char* ota_drop = "Drop file here or click to select";
  const char* ota_no_file = "No file chosen";
  const char* ota_uploading = "Uploading...";
  const char* ota_back = "Back";
  const char* ota_hint = "The device will reboot automatically after upload.";

  const char* toast_saved = "Saved. Rebooting...";
  const char* toast_saving = "Saving...";
  const char* toast_save_failed = "Save failed";
  const char* toast_restore_ok = "Config restored. Rebooting...";
  const char* toast_restore_fail = "Restore failed";
  const char* toast_mqtt_ok = "MQTT test sent";
  const char* toast_mqtt_fail = "MQTT test failed";
  const char* toast_file_chosen = "File selected";
  const char* toast_choose_json = "Please choose a JSON file";
  const char* toast_ha_pub = "Discovery published";
  const char* toast_ha_unpub = "Discovery removed";
  const char* toast_ha_fail = "HA command failed";
  const char* toast_power_on = "Light ON";
  const char* toast_power_off = "Light OFF";
  const char* toast_log_empty = "Log is empty, nothing to export.";
}

// Tooltip texts (full set)
namespace TIPS {
  const char* hold_sps = "Number of dimming adjustments per second while holding.";
  const char* dim_step = "Base perceptual dim step per hold tick. Lower = finer control.";
  const char* min_b = "Lowest brightness (prevents flicker/invisible output).";
  const char* max_b = "Highest brightness. Set lower if full power is undesired.";
  const char* preset1 = "Brightness applied on double-click.";
  const char* preset2 = "Brightness applied on triple-click.";
  const char* transition = "Soft fade duration when toggling power.";
  const char* min_on = "If turning ON below this, brightness jumps here.";
  const char* ease_fade = "Smooth exponential approach to target brightness.";
  const char* gamma_corr = "Adjust brightness curve to feel linear visually.";
  const char* gamma = "Typical 2.0–2.4. Higher = more low-end detail.";
  const char* reverse_dir = "Alternate initial dim direction each hold (edge-aware).";
  const char* restore_last = "Restore last saved brightness (not ON/OFF state).";
  const char* start_on = "If not restoring, start with light ON at boot.";
  const char* auto_off = "Turn OFF automatically after inactivity (0=disabled).";
  const char* keepalive = "MQTT TCP keepalive interval.";
  const char* backoff_min = "Initial reconnect delay (exponential backoff).";
  const char* backoff_max = "Maximum reconnect delay limit.";
  const char* telemetry = "RSSI, heap & uptime publish interval.";
  const char* ha_prefix = "MQTT discovery prefix (usually 'homeassistant').";
  const char* ha_extras = "Include debug/OTA switches & sensors.";
  const char* ha_onboot = "Publish discovery info after connecting.";
  const char* debug_logs = "Verbose serial & diagnostic printing.";
  const char* ota_enabled = "Enables OTA uploads.";
  const char* long_press = "How long you must hold before dimming starts.";
  const char* transition_hint = "0 = instant; 100–300 ms recommended.";
}

// ===================== Helpers =====================
static String chipIdHexLocal() { return chipIdHex(); }

static String fmtUptime() {
  unsigned long ms = millis(); unsigned long s = ms / 1000UL;
  unsigned long d = s / 86400UL; s %= 86400UL; unsigned long h = s / 3600UL; s %= 3600UL; unsigned long m = s / 60UL; s %= 60UL;
  char buf[48]; if (d > 0) snprintf(buf, sizeof(buf), "%lud %luh %lum %lus", d,h,m,s); else snprintf(buf, sizeof(buf), "%luh %lum %lus", h,m,s);
  return String(buf);
}

static String resetReasonStr(){ switch(esp_reset_reason()){
  case ESP_RST_POWERON: return "Power-on"; case ESP_RST_EXT: return "External pin"; case ESP_RST_SW: return "Software";
  case ESP_RST_PANIC: return "Panic"; case ESP_RST_INT_WDT: return "WDT (interrupt)"; case ESP_RST_TASK_WDT: return "WDT (task)";
  case ESP_RST_WDT: return "WDT"; case ESP_RST_DEEPSLEEP: return "Deep-sleep"; case ESP_RST_BROWNOUT: return "Brownout";
  case ESP_RST_SDIO: return "SDIO"; default: return "Unknown"; } }

static String buildHealthJson() {
  String mdns = config.mdns_hostname.length() ? config.mdns_hostname + ".local" : String("-");
  String j; j.reserve(768);
  j += "{";
  j += "\"wifi\":";  j += (WiFi.status() == WL_CONNECTED ? "true":"false"); j += ",";
  j += "\"mqtt\":";  j += (mqttIsConnected() ? "true":"false"); j += ",";
  j += "\"ota\":";   j += (config.otaEnabled ? "true":"false"); j += ",";
  j += "\"debug\":"; j += (config.debugEnabled ? "true":"false"); j += ",";
  j += "\"rssi\":";  j += String(WiFi.RSSI()); j += ",";
  j += "\"heap\":";  j += String(ESP.getFreeHeap()); j += ",";
  j += "\"ip\":\"";  j += WiFi.localIP().toString(); j += "\",";
  j += "\"mdns\":\"";j += mdns; j += "\",";
  j += "\"uptime\":\""; j += fmtUptime(); j += "\",";
  // Channel A
  j += "\"on1\":"; j += (lightOn ? "true":"false"); j += ",";
  j += "\"brightness1\":"; j += String(brightness); j += ",";
  j += "\"target1\":"; j += String(dimTargetBrightness); j += ",";
  // Channel B
  j += "\"on2\":"; j += (lightOn2 ? "true":"false"); j += ",";
  j += "\"brightness2\":"; j += String(brightness2); j += ",";
  j += "\"target2\":"; j += String(dimTargetBrightness2); j += ",";
  // Shared bounds + reset
  j += "\"min_b1\":"; j += String(config.ch[0].minBrightness); j += ",";
  j += "\"max_b1\":"; j += String(config.ch[0].maxBrightness); j += ",";
  j += "\"min_b2\":"; j += String(config.ch[1].minBrightness); j += ",";
  j += "\"max_b2\":"; j += String(config.ch[1].maxBrightness); j += ",";
  j += "\"reset\":\""; j += resetReasonStr(); j += "\"";
  j += "}";
  return j;
}

// ===================== HTML / Assets (PROGMEM) =====================
static const char PROGMEM GLOBAL_CSS[] = R"CSS(
<style>
:root{--rem:16px;--font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',Roboto,Helvetica,Arial,sans-serif,'Apple Color Emoji','Segoe UI Emoji';--ease-out-quad:cubic-bezier(0.25,0.46,0.45,0.94);--ease-in-out-quad:cubic-bezier(0.455,0.03,0.515,0.955);--ease-out-quart:cubic-bezier(0.165,0.84,0.44,1);--border-radius:0.75rem;--primary-color:#039be5;--primary-text-color:#fff;--accent-color:#ff9800;--success-color:#43a047;--warn-color:#fdd835;--error-color:#e53935;--c-bg:#f7f8fa;--c-fg:#374151;--c-card-bg:#fff;--c-card-shadow:0 1px 2px 0 rgba(0,0,0,0.05);--c-input-bg:#f7f8fa;--c-input-border:#d1d5db;--c-input-focus-border:#039be5;--c-input-focus-shadow:rgba(3,155,229,0.2);--c-dim-fg:#6b7280;--c-icon-color:#9ca3af;--c-divider:#e5e7eb;--c-header-bg:rgba(255,255,255,0.7);--c-slider-track:#e5e7eb;}
html.dark{--c-bg:#111827;--c-fg:#d1d5db;--c-card-bg:#1f2937;--c-card-shadow:0 1px 2px 0 rgba(0,0,0,0.2);--c-input-bg:#374151;--c-input-border:#4b5563;--c-input-focus-border:#039be5;--c-input-focus-shadow:rgba(3,155,229,0.2);--c-dim-fg:#9ca3af;--c-icon-color:#6b7280;--c-divider:#374151;--c-header-bg:rgba(31,41,55,0.7);--c-slider-track:#4b5563;}
*,*::before,*::after{box-sizing:border-box;transition:background-color .2s,border-color .2s,color .2s,box-shadow .2s;}
body{margin:0;background:var(--c-bg);color:var(--c-fg);font-family:var(--font-family);line-height:1.5;font-size:var(--rem);}
.wrap{max-width:960px;margin:0 auto;padding:1.5rem 1rem;}
.header{display:flex;align-items:center;justify-content:space-between;padding:1rem 1.5rem;background:var(--c-header-bg);border-radius:var(--border-radius);margin-bottom:1.5rem;box-shadow:var(--c-card-shadow);position:sticky;top:1rem;z-index:10;backdrop-filter:blur(8px);-webkit-backdrop-filter:blur(8px);border:1px solid var(--c-divider);}
h1{font-size:1.25rem;margin:0;font-weight:600;color:var(--c-fg);}
.card{background:var(--c-card-bg);border-radius:var(--border-radius);padding:1.5rem;margin:1.5rem 0;box-shadow:var(--c-card-shadow);border:1px solid var(--c-divider);overflow:hidden;}
h2{font-size:1.125rem;margin:-1.5rem -1.5rem 1.5rem -1.5rem;padding:1rem 1.5rem;border-bottom:1px solid var(--c-divider);font-weight:600;display:flex;align-items:center;justify-content:space-between;}
.h2-title{display:flex;align-items:center;gap:0.75rem;}
.icon{width:1.25rem;height:1.25rem;color:var(--c-icon-color);}
.grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(280px,1fr));gap:1.5rem;}
.grid-2{display:grid;grid-template-columns:repeat(auto-fit,minmax(340px,1fr));gap:1.5rem;}
.kv{display:grid;grid-template-columns:minmax(140px,auto) 1fr;gap:0.75rem 1rem;font-size:0.9375rem;align-items:center;}
.kv>div:first-child{font-weight:500;color:var(--c-dim-fg);} .kv>div:last-child{font-weight:600;}
.pills{display:flex;gap:0.5rem;flex-wrap:wrap;}
.pill{font-size:0.75rem;padding:0.25rem 0.625rem;border-radius:9999px;font-weight:600;color:#fff;text-transform:uppercase;letter-spacing:0.05em;}
.pill.ok{background:var(--success-color);} .pill.bad{background:var(--error-color);}
.input-group{display:flex;flex-direction:column;gap:0.5rem;margin-bottom:0.75rem;}
label{font-size:0.875rem;font-weight:500;display:flex;align-items:center;gap:0.375rem;color:var(--c-dim-fg);}
input:not([type=checkbox]):not([type=radio]):not([type=range]),select{width:100%;padding:0.75rem;border-radius:0.5rem;border:1px solid var(--c-input-border);background:var(--c-input-bg);color:var(--c-fg);font-size:0.9375rem;outline:none;}
input:focus,select:focus{border-color:var(--c-input-focus-border);box-shadow:0 0 0 3px var(--c-input-focus-shadow);}
.input-wrapper{position:relative;}
.pass-toggle{position:absolute;right:0;top:0;bottom:0;border:none;background:transparent;padding:0 0.75rem;cursor:pointer;color:var(--c-icon-color);}
.btn{display:inline-flex;align-items:center;justify-content:center;gap:0.5rem;border:1px solid transparent;border-radius:0.5rem;padding:0.625rem 1.25rem;font-weight:600;cursor:pointer;text-decoration:none;font-size:0.875rem;transition:all .15s var(--ease-out-quad);}
.btn:active{transform:scale(0.97);} .btn:disabled{opacity:0.5;cursor:not-allowed;}
.btn-primary{background:var(--primary-color);color:var(--primary-text-color);border-color:var(--primary-color);}
.btn-secondary{background:var(--c-card-bg);color:var(--c-fg);border-color:var(--c-input-border);}
.btn-secondary:hover{background:var(--c-input-bg);}
.btn-warn{background:var(--accent-color);color:#fff;border-color:var(--accent-color);} .btn-danger{background:var(--error-color);color:#fff;border-color:var(--error-color);}
.btn-sm{padding:0.375rem 0.75rem;font-size:0.75rem;}
.row{display:flex;gap:0.75rem;flex-wrap:wrap;align-items:center;}
.btn-group{display:grid;gap:1rem;grid-template-columns:repeat(auto-fit,minmax(120px,1fr));}
.small{font-size:0.8125rem;color:var(--c-dim-fg);}
.range-wrap{display:flex;align-items:center;gap:1rem;}
.range-val{font-weight:600;font-size:1rem;min-width:3rem;text-align:right;}
input[type=range]{-webkit-appearance:none;width:100%;background:transparent;margin:0.5rem 0;}
input[type=range]:focus{outline:none;}
input[type=range]::-webkit-slider-runnable-track{width:100%;height:0.5rem;cursor:pointer;background:var(--c-slider-track);border-radius:0.25rem;}
input[type=range]::-webkit-slider-thumb{-webkit-appearance:none;height:1.5rem;width:1.5rem;border-radius:50%;background:var(--c-card-bg);border:2px solid var(--primary-color);box-shadow:0 1px 3px rgba(0,0,0,0.2);cursor:pointer;margin-top:-0.5rem;transition:transform .1s var(--ease-out-quad);}
input[type=range]:active::-webkit-slider-thumb{transform:scale(1.1);}
#toasts{position:fixed;bottom:1.5rem;left:50%;transform:translateX(-50%);display:flex;flex-direction:column;align-items:center;gap:0.5rem;z-index:9999;}
.toast{background:var(--c-card-bg);color:var(--c-fg);padding:0.75rem 1.25rem;border-radius:var(--border-radius);box-shadow:0 4px 12px rgba(0,0,0,0.15);opacity:0;transform:translateY(1rem);transition:all .3s var(--ease-out-quart);border:1px solid var(--c-divider);border-left-width:4px;min-width:280px;text-align:center;font-weight:500;}
.toast.show{opacity:1;transform:translateY(0);} .toast.ok{border-color:var(--success-color);} .toast.warn{border-color:var(--warn-color);} .toast.err{border-color:var(--error-color);}
.tooltip{position:relative;display:inline-flex;align-items:center;}
.tooltip .icon{cursor:help;color:var(--c-icon-color);width:1rem;height:1rem;}
.tooltip .tooltiptext{visibility:hidden;width:240px;background-color:#1f2937;color:#f9fafb;text-align:left;border-radius:0.375rem;padding:0.5rem 0.75rem;position:absolute;z-index:10;bottom:150%;left:50%;margin-left:-120px;opacity:0;transition:opacity .2s;font-weight:400;font-size:0.8125rem;box-shadow:0 4px 6px -1px rgba(0,0,0,0.1),0 2px 4px -1px rgba(0,0,0,0.06);}
.tooltip:hover .tooltiptext{visibility:visible;opacity:1;}
.theme-toggle{background:none;border:none;cursor:pointer;padding:0.25rem;color:var(--c-icon-color);display:flex;border-radius:0.375rem;}
.theme-toggle:hover{color:var(--c-fg);}
.theme-toggle .sun{display:none;} html.dark .theme-toggle .sun{display:block;} html.dark .theme-toggle .moon{display:none;}
.sub-card{border:1px solid var(--c-divider);border-radius:0.625rem;padding:1.25rem;margin-top:1.25rem;}
.sub-card h3{margin:-1.25rem -1.25rem 1.25rem -1.25rem;padding:0.75rem 1.25rem;border-bottom:1px solid var(--c-divider);font-size:1rem;font-weight:600;}
#log-viewer{background-color:var(--c-input-bg);border-radius:0.5rem;padding:1rem;height:300px;overflow-y:scroll;font-family:monospace;font-size:0.8125rem;white-space:pre-wrap;margin-top:1rem;border:1px solid var(--c-divider);}
.controls-hint-list{list-style:none;padding:0;margin:1.25rem 0 0 0;font-size:0.875rem;color:var(--c-dim-fg);}
.controls-hint-list li{margin-bottom:0.5rem;display:flex;align-items:center;}
.controls-hint-list strong{color:var(--c-fg);min-width:5rem;display:inline-block;}
.switch{position:relative;display:inline-block;width:44px;height:24px;}
.switch input{opacity:0;width:0;height:0;}
.switch .slider{position:absolute;cursor:pointer;top:0;left:0;right:0;bottom:0;background-color:var(--c-input-border);transition:.2s;border-radius:24px;}
.switch .slider:before{position:absolute;content:"";height:18px;width:18px;left:3px;bottom:3px;background-color:white;transition:.2s;border-radius:50%;}
.switch input:checked+.slider{background-color:var(--primary-color);}
.switch input:checked+.slider:before{transform:translateX(20px);}
.toggle-group{display:flex;justify-content:space-between;align-items:center;padding:0.25rem 0;}
</style>
)CSS";

static const char PROGMEM GLOBAL_JS[] = R"JS(
<script>
function getTheme(){try{return localStorage.getItem('theme')||'light';}catch(e){return 'light';}}
function setTheme(t){document.documentElement.classList.toggle('dark',t==='dark');try{localStorage.setItem('theme',t);}catch(e){}}
function loadTheme(){setTheme(getTheme());}
function toggleTheme(){setTheme(document.documentElement.classList.contains('dark')?'light':'dark');}
function showToast(msg,kind,timeout){const wrap=document.getElementById('toasts');if(!wrap)return;const el=document.createElement('div');el.className='toast '+(kind||'info');el.textContent=msg;wrap.appendChild(el);setTimeout(()=>el.classList.add('show'),10);setTimeout(()=>{el.classList.remove('show');setTimeout(()=>wrap.removeChild(el),300)},timeout||3000);}
function setBadge(el,ok,text){if(!el)return;el.classList.toggle('ok',ok);el.classList.toggle('bad',!ok);el.textContent=text;}
function handleWsMessage(j) {
    if (j.type === 'log') {
        const viewer = document.getElementById('log-viewer');
        if (viewer) {
            viewer.textContent += j.data;
            viewer.scrollTop = viewer.scrollHeight;
        }
        return;
    }
    applyState(j);
}
function applyState(j){
  setBadge(document.getElementById('wifiBadge'),j.wifi,'Wi-Fi');
  setBadge(document.getElementById('mqttBadge'),j.mqtt,'MQTT');
  setBadge(document.getElementById('otaBadge'),j.ota,'OTA');
  const dbgBadge = document.getElementById('dbgBadge');
  if(dbgBadge) setBadge(dbgBadge, j.debug, 'Debug');
  const s=(id,v)=>{const el=document.getElementById(id);if(el)el.textContent=v;};
  s('ipVal',j.ip||'-'); s('mdnsVal',j.mdns||'-'); s('rssiVal',j.rssi+' dBm'); s('upVal',j.uptime);
  const slider1=document.getElementById('bSlider1');
  if(slider1){
      slider1.min = j.min_b1; slider1.max = j.max_b1;
      if(!window.__userSliding1){s('stateVal1',j.on1?'ON':'OFF'); s('brightVal1',j.brightness1+' ('+Math.round(j.brightness1/2.54)+'%)');
        slider1.value=j.brightness1;updateSlider(slider1,1);slider1.disabled=!j.on1;
      }
  }
  const chk1=document.getElementById('powerChk1'); if(chk1)chk1.checked=j.on1;
  const slider2=document.getElementById('bSlider2');
  if(slider2){
      slider2.min = j.min_b2; slider2.max = j.max_b2;
      if(!window.__userSliding2){s('stateVal2',j.on2?'ON':'OFF'); s('brightVal2',j.brightness2+' ('+Math.round(j.brightness2/2.54)+'%)');
        slider2.value=j.brightness2;updateSlider(slider2,2);slider2.disabled=!j.on2;
      }
  }
  const chk2=document.getElementById('powerChk2'); if(chk2)chk2.checked=j.on2;
  s('heapVal',j.heap+' B'); s('resetVal',j.reset);
  const debugCheck = document.querySelector('input[name="debugEnabled"]'); if(debugCheck) debugCheck.checked = j.debug;
  const otaCheck = document.querySelector('input[name="otaEnabled"]'); if(otaCheck) otaCheck.checked = j.ota;
  const logContainer = document.getElementById('log-container');
  if (logContainer) logContainer.style.display = j.debug ? 'block' : 'none';
}
async function refreshHealth(){try{const r=await fetch('/api/health');if(!r.ok)return;const j=await r.json();applyState(j);}catch(e){}}
function initAutoRefresh(){refreshHealth();setInterval(refreshHealth,10000);
  try{const proto=(location.protocol==='https:')?'wss://':'ws://';const sock=new WebSocket(proto+location.host+'/ws');
  sock.onmessage=ev=>{try{const j=JSON.parse(ev.data);handleWsMessage(j);}catch(e){}};}catch(e){}}
function debounce(fn,ms){let t;return function(){const a=arguments;clearTimeout(t);t=setTimeout(()=>fn.apply(this,a),ms);};}
const sendSlider1=debounce(async v=>{try{const fd=new URLSearchParams();fd.append('ch','1');fd.append('b',v);if(Number(v)>0)fd.append('on','1');await fetch('/api/cmd',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:fd.toString()});}catch(e){}},120);
const sendSlider2=debounce(async v=>{try{const fd=new URLSearchParams();fd.append('ch','2');fd.append('b',v);if(Number(v)>0)fd.append('on','1');await fetch('/api/cmd',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:fd.toString()});}catch(e){}},120);
function onSliderStart(ch){window['__userSliding'+ch]=true;} function onSliderEnd(ch){setTimeout(()=>{window['__userSliding'+ch]=false;},300);}
function onSliderInput(el,ch){updateSlider(el,ch);(ch===1?sendSlider1:sendSlider2)(el.value);}
function updateSlider(el,ch){if(!el.max||!el.min)return;const pct=Math.round((el.value-el.min)*100/(el.max-el.min));const valEl=document.getElementById('rangeVal'+ch);if(valEl)valEl.textContent=pct+'%';const grad='linear-gradient(90deg,var(--primary-color) '+pct+'%,var(--c-slider-track) '+pct+'%)';el.style.background=grad;}
async function onPowerToggle(chk,ch){try{const fd=new URLSearchParams();fd.append('ch',String(ch));fd.append('on',chk.checked?'1':'0');await fetch('/api/cmd',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:fd.toString()});showToast(chk.checked?'Light ON':'Light OFF','ok',1200);}catch(e){}}
async function onTestMQTT(){try{const r=await fetch('/api/testmqtt');showToast(r.ok?'MQTT test sent':'MQTT test failed',r.ok?'ok':'err');}catch(e){showToast('MQTT test failed','err');}}
async function onBackup(){const a=document.createElement('a');a.href='/api/backup';a.download='esp32_dimmer_backup.json';document.body.appendChild(a);a.click();document.body.removeChild(a);}
async function onRestoreChoose(){const inp=document.getElementById('restoreFile');if(!inp||!inp.files||!inp.files.length){showToast('Please choose a JSON file','warn');return;}const file=inp.files[0];const txt=await file.text();try{const res=await fetch('/restore',{method:'POST',headers:{'Content-Type':'application/json'},body:txt});if(res.ok){showToast('Config restored. Rebooting...','ok');setTimeout(()=>location.reload(),3500);}else{showToast('Restore failed','err');}}catch(e){showToast('Restore failed','err');}}
function togglePassword(el){const inp=el.previousElementSibling;inp.type=inp.type==='password'?'text':'password';}
function exportLog(){const viewer=document.getElementById('log-viewer');if(!viewer||!viewer.textContent.trim()){showToast('Log is empty, nothing to export.','warn');return;}const blob=new Blob([viewer.textContent],{type:'text/plain'});const a=document.createElement('a');a.href=URL.createObjectURL(blob);const d=new Date();const ts=d.getFullYear()+'-'+(d.getMonth()+1).toString().padStart(2,'0')+'-'+d.getDate().toString().padStart(2,'0')+'_'+d.getHours().toString().padStart(2,'0')+'-'+d.getMinutes().toString().padStart(2,'0')+'-'+d.getSeconds().toString().padStart(2,'0');a.download='log-'+ts+'.txt';document.body.appendChild(a);a.click();document.body.removeChild(a);URL.revokeObjectURL(a.href);}
</script>
)JS";

static String tooltip(const char* text) {
  String s = "<div class='tooltip'><svg class='icon' fill='none' stroke='currentColor' viewBox='0 0 24 24'><path stroke-linecap='round' stroke-linejoin='round' stroke-width='2' d='M13 16h-1v-4h-1m1-4h.01M21 12a9 9 0 11-18 0 9 9 0 0118 0z'/></svg><span class='tooltiptext'>";
  s += text;
  s += "</span></div>";
  return s;
}

// Helper to generate a toggle switch
static String toggle(const char* name, bool checked, const char* label, const char* tip) {
    String s = "<div class='toggle-group'><label for='";
    s += name; s += "'>"; s += label; s += tooltip(tip); s += "</label><label class='switch'><input type='checkbox' id='";
    s += name; s += "' name='"; s += name; s += "' "; s += (checked ? "checked" : ""); s += "><span class='slider'></span></label></div>";
    return s;
}

// ===================== WebSocket push =====================
void wsPushState() {
  static unsigned long lastPush = 0, lastCleanup = 0;
  unsigned long now = millis();
  if (now - lastCleanup > 10000UL) { ws.cleanupClients(); lastCleanup = now; }
  const unsigned long MIN_PUSH_INTERVAL_MS = 150;
  if (now - lastPush < MIN_PUSH_INTERVAL_MS) return;
  lastPush = now;
  if (!ws.count()) return;
  String j = buildHealthJson();
  ws.textAll(j);
}

// ===================== OTA upload progress tracking =====================
static volatile uint32_t ota_prog = 0, ota_total = 0;
static volatile bool ota_error = false;

// ===================== Routes =====================
void setupWebServer() {
  ws.onEvent([](AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len){
    if (type == WS_EVT_CONNECT) {
      client->text(buildHealthJson());
    }
  });
  server.addHandler(&ws);

  // ------------- Root (Main UI) -------------
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *r) {
    if (!r->authenticate(config.web_user.c_str(), config.web_pass.c_str())) return r->requestAuthentication();
    
    String html;
    html.reserve(32000);
    html += F("<!DOCTYPE html><html lang='en'><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width,initial-scale=1'>"
              "<title>ESP32 Dimmer</title>");
    html += FPSTR(GLOBAL_CSS);
    html += FPSTR(GLOBAL_JS);
    html += F("</head><body onload='loadTheme();initAutoRefresh()'><div class='wrap'><div id='toasts'></div>");

    html += "<header class='header'><h1>"; html += UI::title; html += "</h1><div class='row'>";
    html += "<div class='pills'><span id='wifiBadge' class='pill'></span><span id='mqttBadge' class='pill'></span><span id='otaBadge' class='pill'></span><span id='dbgBadge' class='pill'></span></div>";
    html += "<button id='themeBtn' onclick='toggleTheme()' class='theme-toggle' aria-label='Toggle theme'><svg class='icon moon' fill='none' stroke='currentColor' viewBox='0 0 24 24'><path stroke-linecap='round' stroke-linejoin='round' stroke-width='2' d='M20.354 15.354A9 9 0 018.646 3.646 9.003 9.003 0 0012 21a9.003 9.003 0 008.354-5.646z'/></svg><svg class='icon sun' fill='none' stroke='currentColor' viewBox='0 0 24 24'><path stroke-linecap='round' stroke-linejoin='round' stroke-width='2' d='M12 3v1m0 16v1m9-9h-1M4 12H3m15.364 6.364l-.707-.707M6.343 6.343l-.707-.707m12.728 0l-.707.707M6.343 17.657l-.707.707M12 12a5 5 0 100-10 5 5 0 000 10z'/></svg></button>";
    html += "</div></header>";

    // Controls
    html += "<div class='card'><h2><div class='h2-title'><svg class='icon' fill='none' stroke='currentColor' viewBox='0 0 24 24'><path stroke-linecap='round' stroke-linejoin='round' stroke-width='2' d='M12 18h.01M8 21h8a2 2 0 002-2V5a2 2 0 00-2-2H8a2 2 0 00-2 2v14a2 2 0 002 2z'/></svg><span>";
    html += UI::controls_title; html += "</span></div></h2>";
    html += "<div class='row' style='align-items:center;gap:1rem;'><label class='switch' style='flex-shrink:0;'><input id='powerChk1' type='checkbox' onchange='onPowerToggle(this,1)'><span class='slider'></span></label>";
    html += "<div class='range-wrap' style='flex:1;'><input id='bSlider1' type='range' min='";
    html += String(config.ch[0].minBrightness); html += "' max='"; html += String(config.ch[0].maxBrightness);
    html += "' oninput='onSliderInput(this,1)' onmousedown='onSliderStart(1)' ontouchstart='onSliderStart(1)' onmouseup='onSliderEnd(1)' ontouchend='onSliderEnd(1)'><span id='rangeVal1' class='range-val'></span></div></div>";
    html += "<hr style='border:none;border-top:1px solid var(--c-divider);margin:1.25rem 0'>";
    html += "<div class='row' style='align-items:center;gap:1rem;'><label class='switch' style='flex-shrink:0;'><input id='powerChk2' type='checkbox' onchange='onPowerToggle(this,2)'><span class='slider'></span></label>";
    html += "<div class='range-wrap' style='flex:1;'><input id='bSlider2' type='range' min='";
    html += String(config.ch[1].minBrightness); html += "' max='"; html += String(config.ch[1].maxBrightness);
    html += "' oninput='onSliderInput(this,2)' onmousedown='onSliderStart(2)' ontouchstart='onSliderStart(2)' onmouseup='onSliderEnd(2)' ontouchend='onSliderEnd(2)'><span id='rangeVal2' class='range-val'></span></div></div>";
    html += "<div class='sub-card' style='margin-top:2rem;'><h3>"; html += UI::controls_hint_title; html += "</h3>";
    html += "<ul class='controls-hint-list'><li><strong>1&times; Click</strong> &rarr; Toggle On/Off</li><li><strong>2&times; Click</strong> &rarr; Apply Preset 1</li><li><strong>3&times; Click</strong> &rarr; Apply Preset 2</li><li><strong>Hold</strong> &rarr; Start Dimming</li><li><strong>10&times; Click</strong> &rarr; Factory Reset (within 12s)</li></ul></div></div>";

    // Status
    html += "<div class='card'><h2><div class='h2-title'><svg class='icon' fill='none' stroke='currentColor' viewBox='0 0 24 24'><path stroke-linecap='round' stroke-linejoin='round' stroke-width='2' d='M13 16h-1v-4h-1m1-4h.01M21 12a9 9 0 11-18 0 9 9 0 0118 0z'/></svg><span>";
    html += UI::status_title; html += "</span></div></h2><div class='kv'>";
    html += "<div>"; html += UI::status_ip; html += "</div><div id='ipVal'>-</div>";
    html += "<div>"; html += UI::status_mdns; html += "</div><div id='mdnsVal'>-</div>";
    html += "<div>"; html += UI::fw_version; html += "</div><div>"; html += FW_VERSION; html += "</div>";
    html += "<div>"; html += UI::status_rssi; html += "</div><div id='rssiVal'>-</div>";
    html += "<div>"; html += UI::status_uptime; html += "</div><div id='upVal'>-</div>";
    html += "<div>State A</div><div id='stateVal1'>-</div>";
    html += "<div>Brightness A</div><div id='brightVal1'>-</div>";
    html += "<div>State B</div><div id='stateVal2'>-</div>";
    html += "<div>Brightness B</div><div id='brightVal2'>-</div>";
    html += "<div>"; html += UI::status_heap; html += "</div><div id='heapVal'>-</div>";
    html += "<div>"; html += UI::status_reset; html += "</div><div id='resetVal'>-</div></div></div>";

    // Logs
    html += "<div id='log-container' class='card' style='display:none;'><h2><div class='h2-title'><svg class='icon' fill='none' stroke='currentColor' viewBox='0 0 24 24'><path stroke-linecap='round' stroke-linejoin='round' stroke-width='2' d='M9 12h6m-6 4h6m2 5H7a2 2 0 01-2-2V5a2 2 0 012-2h5.586a1 1 0 01.707.293l5.414 5.414a1 1 0 01.293.707V19a2 2 0 01-2 2z'/></svg><span>";
    html += UI::logs_title; html += "</span></div><button type='button' class='btn btn-secondary btn-sm' onclick='exportLog()'>";
    html += UI::logs_export; html += "</button></h2><pre id='log-viewer'></pre></div>";

    // Settings form start
    html += "<form id='settingsForm' method='POST' action='/save'>";
    html += "<div class='grid-2'>";
    for (int i=0; i<2; i++) {
        String p = "ch" + String(i+1) + "_";
        html += "<div class='card'><h2><div class='h2-title'><span>"; html += (i==0 ? UI::ch1_title : UI::ch2_title); html += "</span></div></h2>";
        html += "<div class='sub-card'><h3>"; html += UI::settings_light_dim_title; html += "</h3><div class='grid'>";
        html += "<div class='input-group'><label>"; html += UI::settings_hold_sps; html += tooltip(TIPS::hold_sps); html += "</label><input type='number' name='"; html += p; html += "holdSPS' min='1' max='50' step='1' value='"; html += String(config.ch[i].holdSPS); html += "'></div>";
        html += "<div class='input-group'><label>"; html += UI::settings_dim_step; html += tooltip(TIPS::dim_step); html += "</label><input type='number' name='"; html += p; html += "dimStep' min='1' max='100' step='1' value='"; html += String(config.ch[i].dimStep); html += "'></div>";
        html += "<div class='input-group'><label>"; html += UI::settings_fade_delay; html += tooltip("Delay between fade steps."); html += "</label><input type='number' name='"; html += p; html += "fadeDelay' min='5' max='200' value='"; html += String(config.ch[i].fadeStepDelayMs); html += "'></div>";
        html += "<div class='input-group'><label>"; html += UI::settings_min_b; html += tooltip(TIPS::min_b); html += "</label><input type='number' name='"; html += p; html += "minBright' min='0' max='254' value='"; html += String(config.ch[i].minBrightness); html += "'></div>";
        html += "<div class='input-group'><label>"; html += UI::settings_max_b; html += tooltip(TIPS::max_b); html += "</label><input type='number' name='"; html += p; html += "maxBright' min='0' max='254' value='"; html += String(config.ch[i].maxBrightness); html += "'></div>";
        html += "<div class='input-group'><label>"; html += UI::settings_transition; html += tooltip(TIPS::transition_hint); html += "</label><input type='number' name='"; html += p; html += "transMs' min='0' max='2000' value='"; html += String(config.ch[i].transitionMs); html += "'></div>";
        html += "<div class='input-group'><label>"; html += UI::settings_min_on; html += tooltip(TIPS::min_on); html += "</label><input type='number' name='"; html += p; html += "minOnLvl' min='0' max='254' value='"; html += String(config.ch[i].minOnLevel); html += "'></div>";
        html += "<div class='input-group'><label>"; html += UI::settings_gamma_val; html += tooltip(TIPS::gamma); html += "</label><input type='number' step='0.1' min='1.2' max='3.5' name='"; html += p; html += "gamma' value='"; html += String(config.ch[i].gamma,1); html += "'></div>";
        html += "<div class='input-group'><label>"; html += UI::settings_long_press; html += tooltip(TIPS::long_press); html += "</label><input type='number' name='"; html += p; html += "longPress' min='150' max='2000' value='"; html += String(config.ch[i].longPressMs); html += "'></div>";
        html += "</div><div style='margin-top:1rem;'>";
        html += toggle((p+"ease").c_str(), config.ch[i].easeFade, UI::settings_ease_fade, TIPS::ease_fade);
        html += toggle((p+"gammaEn").c_str(), config.ch[i].gammaEnabled, UI::settings_gamma_corr, TIPS::gamma_corr);
        html += "</div></div>";
        html += "<div class='sub-card'><h3>"; html += UI::settings_behavior_title; html += "</h3>";
        html += "<div class='input-group'><label>"; html += UI::settings_start_b; html += tooltip("Brightness at boot (if not restoring)."); html += "</label><input type='number' name='"; html += p; html += "startB' min='0' max='254' value='"; html += String(config.ch[i].startBrightness); html += "'></div>";
        html += "<div>";
        html += toggle((p+"startOn").c_str(), config.ch[i].startOn, UI::settings_start_on, TIPS::start_on);
        html += toggle((p+"restore").c_str(), config.ch[i].restoreLast, UI::settings_restore_last, TIPS::restore_last);
        html += toggle((p+"reverse").c_str(), config.ch[i].reverseDirection, UI::settings_reverse_dim, TIPS::reverse_dir);
        html += "</div><div class='input-group' style='margin-top:1.25rem;'><label>"; html += UI::settings_auto_off; html += tooltip(TIPS::auto_off); html += "</label><input type='number' name='"; html += p; html += "autoOff' min='0' max='1440' value='"; html += String(config.ch[i].autoOffMinutes); html += "'></div></div>";
        html += "<div class='sub-card'><h3>Presets</h3>";
        html += "<div class='input-group'><label>"; html += UI::settings_preset1; html += tooltip(TIPS::preset1); html += "</label><input type='number' name='"; html += p; html += "preset1' min='0' max='254' value='"; html += String(config.ch[i].preset1Brightness); html += "'></div>";
        html += "<div class='input-group'><label>"; html += UI::settings_preset2; html += tooltip(TIPS::preset2); html += "</label><input type='number' name='"; html += p; html += "preset2' min='0' max='254' value='"; html += String(config.ch[i].preset2Brightness); html += "'></div>";
        html += "</div></div>";
    }
    html += "</div>";

    html += "<div class='card'><h2><div class='h2-title'><svg class='icon' fill='none' stroke='currentColor' viewBox='0 0 24 24'><path stroke-linecap='round' stroke-linejoin='round' stroke-width='2' d='M10.325 4.317c.426-1.756 2.924-1.756 3.35 0a1.724 1.724 0 002.573 1.066c1.543-.94 3.31.826 2.37 2.37a1.724 1.724 0 001.065 2.572c1.756.426 1.756 2.924 0 3.35a1.724 1.724 0 00-1.066 2.573c.94 1.543-.826 3.31-2.37 2.37a1.724 1.724 0 00-2.572 1.065c-.426 1.756-2.924 1.756-3.35 0a1.724 1.724 0 00-2.573-1.066c-1.543.94-3.31-.826-2.37-2.37a1.724 1.724 0 00-1.065-2.572c-1.756-.426-1.756-2.924 0-3.35a1.724 1.724 0 001.066-2.573c-.94-1.543.826-3.31 2.37-2.37.996.608 2.296.07 2.572-1.065z'></path><path stroke-linecap='round' stroke-linejoin='round' stroke-width='2' d='M15 12a3 3 0 11-6 0 3 3 0 016 0z'></path></svg><span>";
    html += UI::settings_global_title; html += "</span></div></h2><div>";
    html += toggle("debugEnabled", config.debugEnabled, UI::settings_debug_logs, TIPS::debug_logs);
    html += toggle("otaEnabled", config.otaEnabled, UI::settings_ota_web, TIPS::ota_enabled);
    html += "</div></div>";

    html += "<div class='card'><h2><div class='h2-title'><svg class='icon' fill='none' stroke='currentColor' viewBox='0 0 24 24'><path stroke-linecap='round' stroke-linejoin='round' stroke-width='2' d='M3 12l2-2m0 0l7-7 7 7M5 10v10a1 1 0 001 1h3m10-11l2 2m-2-2v10a1 1 0 01-1 1h-3m-6 0a1 1 0 001-1v-4a1 1 0 011-1h2a1 1 0 011 1v4a1 1 0 001 1m-6 0h6'/></svg><span>";
    html += UI::settings_ha_title; html += "</span></div></h2><div class='grid'>";
    html += "<div class='input-group'><label>"; html += UI::settings_ha_prefix; html += tooltip(TIPS::ha_prefix); html += "</label><input type='text' name='haPrefix' value='"; html += config.haPrefix; html += "'></div>";
    html += "<div class='input-group'><label>"; html += UI::settings_ha_name; html += "</label><input type='text' name='haName' value='"; html += config.haName; html += "'></div></div>";
    html += "<div style='margin-top:1rem;'>";
    html += toggle("haEnabled", config.haEnabled, UI::settings_ha_enable, "Enable or disable Home Assistant MQTT discovery.");
    html += toggle("haExtras", config.haExtras, UI::settings_ha_extras, TIPS::ha_extras);
    html += toggle("haOnBoot", config.haOnBoot, UI::settings_ha_on_boot, TIPS::ha_onboot);
    html += "</div><div class='row' style='margin-top:1.5rem; gap:1rem;'>";
    html += "<button type='button' class='btn btn-secondary' onclick=\"fetch('/api/hadiscovery?op=publish').then(r=>showToast(r.ok?'Discovery published':'HA command failed',r.ok?'ok':'err')).catch(()=>showToast('HA command failed','err'))\">";
    html += UI::settings_ha_publish; html += "</button>";
    html += "<button type='button' class='btn btn-secondary' onclick=\"fetch('/api/hadiscovery?op=unpublish').then(r=>showToast(r.ok?'Discovery removed':'HA command failed',r.ok?'ok':'err')).catch(()=>showToast('HA command failed','err'))\">";
    html += UI::settings_ha_unpublish; html += "</button></div></div>";

    html += "<div class='card'><h2><div class='h2-title'><svg class='icon' fill='none' stroke='currentColor' viewBox='0 0 24 24'><path stroke-linecap='round' stroke-linejoin='round' stroke-width='2' d='M21 12a9 9 0 01-9 9m9-9a9 9 0 00-9-9m9 9H3m9 9a9 9 0 01-9-9m9 9V3m0 18a9 9 0 009-9m-9 9a9 9 0 00-9-9'/></svg><span>";
    html += UI::settings_mqtt_title; html += "</span></div></h2><div class='grid'>";
    html += "<div class='input-group'><label>"; html += UI::settings_mqtt_topic; html += "</label><input type='text' name='mqtt_topic' value='"; html += config.mqtt_topic; html += "'></div>";
    html += "<div class='input-group'><label>"; html += UI::settings_mqtt_topic_b; html += "</label><input type='text' name='mqtt_topic2' value='"; html += config.mqtt_topic2; html += "'></div>";
    html += "<div class='input-group'><label>"; html += UI::settings_mqtt_server; html += "</label><input type='text' name='mqtt_server' value='"; html += config.mqtt_server; html += "'></div>";
    html += "<div class='input-group'><label>"; html += UI::settings_mqtt_port; html += "</label><input type='number' name='mqtt_port' value='"; html += String(config.mqtt_port); html += "'></div>";
    html += "<div class='input-group'><label>"; html += UI::settings_mqtt_user; html += "</label><input type='text' name='mqtt_user' value='"; html += config.mqtt_user; html += "'></div>";
    html += "<div class='input-group'><label>"; html += UI::settings_mqtt_pass; html += "</label><div class='input-wrapper'><input type='password' name='mqtt_pass' placeholder='";
    html += UI::settings_mqtt_pass_unchanged; html += "'><button type='button' class='pass-toggle' onclick='togglePassword(this)'>👁️</button></div></div>";
    html += "<div class='input-group'><label>"; html += UI::settings_mqtt_keepalive; html += tooltip(TIPS::keepalive); html += "</label><input type='number' name='mqttKeepalive' min='5' max='120' value='"; html += String(config.mqttKeepalive); html += "'></div>";
    html += "<div class='input-group'><label>"; html += UI::settings_mqtt_backoff_min; html += tooltip(TIPS::backoff_min); html += "</label><input type='number' name='backoffMinMs' min='200' max='60000' value='"; html += String(config.backoffMinMs); html += "'></div>";
    html += "<div class='input-group'><label>"; html += UI::settings_mqtt_backoff_max; html += tooltip(TIPS::backoff_max); html += "</label><input type='number' name='backoffMaxMs' min='200' max='60000' value='"; html += String(config.backoffMaxMs); html += "'></div>";
    html += "<div class='input-group'><label>"; html += UI::settings_mqtt_telemetry; html += tooltip(TIPS::telemetry); html += "</label><input type='number' name='telemetrySec' min='5' max='3600' value='"; html += String(config.telemetrySec); html += "'></div></div></div>";

    html += "<div class='card'><h2><div class='h2-title'><svg class='icon' fill='none' stroke='currentColor' viewBox='0 0 24 24'><path stroke-linecap='round' stroke-linejoin='round' stroke-width='2' d='M12 15v2m-6 4h12a2 2 0 002-2v-6a2 2 0 00-2-2H6a2 2 0 00-2 2v6a2 2 0 002 2zm10-10V7a4 4 0 00-8 0v4h8z'/></svg><span>";
    html += UI::settings_net_web_title; html += "</span></div></h2><div class='grid'>";
    html += "<div class='input-group'><label>"; html += UI::settings_mdns_host; html += "</label><input type='text' name='mdns_hostname' value='"; html += config.mdns_hostname; html += "'></div>";
    html += "<div class='input-group'><label>"; html += UI::settings_web_user; html += "</label><input type='text' name='web_user' value='"; html += config.web_user; html += "'></div>";
    html += "<div class='input-group'><label>"; html += UI::settings_web_pass; html += "</label><div class='input-wrapper'><input type='password' name='web_pass' placeholder='"; html += UI::settings_mqtt_pass_unchanged;
    html += "'><button type='button' class='pass-toggle' onclick='togglePassword(this)'>👁️</button></div></div>";
    html += "<div class='input-group'><label>"; html += UI::settings_ota_pass; html += "</label><div class='input-wrapper'><input type='password' name='ota_password' placeholder='"; html += UI::settings_mqtt_pass_unchanged;
    html += "'><button type='button' class='pass-toggle' onclick='togglePassword(this)'>👁️</button></div></div></div></div>";
    html += "</form>";

    html += "<div class='card'><h2><div class='h2-title'><svg class='icon' fill='none' stroke='currentColor' viewBox='0 0 24 24'><path stroke-linecap='round' stroke-linejoin='round' stroke-width='2' d='M12 6V4m0 2a2 2 0 100 4m0-4a2 2 0 110 4m-6 8a2 2 0 100-4m0 4a2 2 0 110-4m0 4v2m0-6V4m6 6v10m6-2a2 2 0 100-4m0 4a2 2 0 110-4m0 4v2m0-6V4'/></svg><span>";
    html += UI::tools_title; html += "</span></div></h2><div class='btn-group'>";
    html += "<button type='button' class='btn btn-secondary' onclick='onBackup()'>"; html += UI::tools_backup; html += "</button>";
    html += "<button type='button' class='btn btn-secondary' onclick='onTestMQTT()'>"; html += UI::tools_test_mqtt; html += "</button>";
    html += "<label class='btn btn-secondary' style='position:relative;overflow:hidden'><span>"; html += UI::tools_restore; html += "</span><input id='restoreFile' type='file' accept='application/json' style='position:absolute;inset:0;opacity:0;cursor:pointer' onchange=\"showToast('";
    html += UI::toast_file_chosen; html += "','ok',1000)\"></label>";
    html += "<button type='button' class='btn btn-primary' onclick='onRestoreChoose()'>"; html += UI::tools_apply_restore; html += "</button></div><div class='small' style='margin-top:1rem'>";
    html += UI::tools_backup_hint; html += "</div></div>";

    html += "<div class='card'><h2><div class='h2-title'><svg class='icon' fill='none' stroke='currentColor' viewBox='0 0 24 24'><path stroke-linecap='round' stroke-linejoin='round' stroke-width='2' d='M12 9v2m0 4h.01m-6.938 4h13.856c1.54 0 2.502-1.667 1.732-3L13.732 4c-.77-1.333-2.694-1.333-3.464 0L3.34 16c-.77 1.333.192 3 1.732 3z'/></svg><span>";
    html += UI::actions_title; html += "</span></div></h2><div class='btn-group'>";
    html += "<button id='saveBtn' class='btn btn-primary' type='submit' form='settingsForm'>"; html += UI::actions_save_reboot; html += "</button>";
    html += "<a class='btn btn-secondary' href='/update'><span>"; html += UI::actions_ota_upload; html += "</span></a>";
    html += "<button class='btn btn-warn' type='button' onclick=\"if(confirm('"; html += UI::actions_wifi_confirm; html += "')) location='/wifireset'\">"; html += UI::actions_wifi_reset; html += "</button>";
    html += "<button class='btn btn-danger' type='button' onclick=\"if(confirm('"; html += UI::actions_factory_confirm; html += "')) location='/factoryreset'\">"; html += UI::actions_factory_reset; html += "</button>";
    html += "</div></div>";

    html += R"JS(<script>
document.getElementById('settingsForm').addEventListener('submit', e => {
  e.preventDefault();
  const btn = document.getElementById('saveBtn');
  btn.disabled = true;
  const form = e.target;
  const formData = new FormData(form);
  form.querySelectorAll('input[type="checkbox"]').forEach(cb => { formData.set(cb.name, cb.checked ? 'on' : 'off'); });
  if (formData.get('mqtt_pass') === '') formData.delete('mqtt_pass');
  if (formData.get('web_pass') === '') formData.delete('web_pass');
  if (formData.get('ota_password') === '') formData.delete('ota_password');
  const body = new URLSearchParams(formData).toString();
  fetch('/save', {
    method: 'POST',
    headers: {'Content-Type': 'application/x-www-form-urlencoded'},
    body: body
  }).then(response => {
    if (response.ok) {
      showToast('Saved. Rebooting...', 'ok', 4000);
      setTimeout(() => { window.location.href = '/'; }, 4500);
    } else {
      return response.text().then(text => { throw new Error('Save failed: ' + response.status + ' ' + text) });
    }
  }).catch(err => {
    console.error('Fetch Error:', err);
    showToast('Save exception: ' + err.message, 'err', 6000);
    btn.disabled = false;
  });
});
document.addEventListener('DOMContentLoaded',()=>{
  const s1=document.getElementById('bSlider1'); if(s1)updateSlider(s1,1);
  const s2=document.getElementById('bSlider2'); if(s2)updateSlider(s2,2);
});
</script>)JS";

    html += "</div></body></html>";
    r->send(200, "text/html", html);
  });

  // ------------- OTA Page -------------
  server.on("/update", HTTP_GET, [](AsyncWebServerRequest *r){
    if (!r->authenticate(config.web_user.c_str(), config.web_pass.c_str())) return r->requestAuthentication();
    String html;
    html.reserve(8000);
    html += "<!DOCTYPE html><html lang='en'><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width,initial-scale=1'><title>";
    html += UI::ota_page_title; html += "</title>";
    html += FPSTR(GLOBAL_CSS);
    html += FPSTR(GLOBAL_JS);
    html += "</head><body onload='loadTheme()'><div class='wrap'><div id='toasts'></div>";
    html += "<header class='header'><h1>"; html += UI::ota_page_title; html += "</h1><a class='btn btn-secondary' href='/'>";
    html += UI::ota_back; html += "</a></header>";
    html += "<div class='card'><form id='f' method='POST' action='/update' enctype='multipart/form-data'>";
    html += "<div id='drop' style='border:2px dashed var(--c-input-border);padding:3rem;text-align:center;border-radius:var(--border-radius);cursor:pointer;font-weight:500;color:var(--c-dim-fg);'>";
    html += UI::ota_drop; html += "</div>";
    html += "<input id='file' type='file' name='update' style='display:none' accept='.bin'>";
    html += "<div id='chosen' class='small' style='margin-top:1rem;'>"; html += UI::ota_no_file; html += "</div>";
    html += "<progress id='bar' max='100' value='0' style='width:100%;height:0.5rem;margin:0.5rem 0;border-radius:99px;overflow:hidden;'></progress> <span id='pct' class='small'>0%</span>";
    html += "<div class='row' style='margin-top:1.5rem;'><button id='btn' class='btn btn-primary' type='submit'>";
    html += UI::actions_ota_upload; html += "</button></div><span class='small' id='hint' style='display:block;margin-top:1rem;'>";
    html += UI::ota_hint; html += "</span></form></div></div>";
    html += R"JS(<script>
const drop=document.getElementById('drop'),file=document.getElementById('file'),form=document.getElementById('f'),chosen=document.getElementById('chosen'),btn=document.getElementById('btn'),bar=document.getElementById('bar'),pct=document.getElementById('pct');
drop.addEventListener('click',()=>file.click());
drop.addEventListener('dragover',e=>{e.preventDefault();drop.style.borderColor='var(--primary-color)';});
drop.addEventListener('dragleave',()=>drop.style.borderColor='var(--c-input-border)');
drop.addEventListener('drop',e=>{e.preventDefault();drop.style.borderColor='var(--c-input-border)';if(e.dataTransfer.files.length){file.files=e.dataTransfer.files;chosen.textContent=file.files[0].name;}});
file.addEventListener('change',()=>{if(file.files.length)chosen.textContent=file.files[0].name;});
let pollTimer=0, lastProgress=0, stagnantCount=0;
async function pollProgress(){
  try{
    const r=await fetch('/api/otaprogress'); if(!r.ok) return;
    const j=await r.json();
    if(j.error){ showToast('OTA error','err',4000); clearInterval(pollTimer); btn.disabled=false; return; }
    let pctVal=j.total?Math.floor(j.progress*100/j.total):0;
    if(pctVal>100)pctVal=100;
    bar.value=pctVal; pct.textContent=pctVal+'%';
    if(pctVal===100){ showToast('Upload complete. Rebooting...','ok',3500); clearInterval(pollTimer); }
    if(j.progress===lastProgress) stagnantCount++; else stagnantCount=0;
    lastProgress=j.progress;
    if(stagnantCount>40){ showToast('OTA stalled','err',4000); clearInterval(pollTimer); btn.disabled=false; }
  }catch(e){}
}
form.addEventListener('submit',e=>{
  if(!file.files.length){e.preventDefault();showToast('Select a file first','warn',2000);return;}
  btn.disabled=true; btn.textContent=')JS";
    html += UI::ota_uploading; html += R"JS('; pollTimer=setInterval(pollProgress,300); });</script>)JS";
    html += "</div></body></html>";
    r->send(200,"text/html",html);
  });

  // OTA POST
  server.on("/update", HTTP_POST,
    [](AsyncWebServerRequest *r) { r->send(200, "text/plain", "OK"); rebooter.once(1, [](){ ESP.restart(); }); },
    [](AsyncWebServerRequest *r, String filename, size_t index, uint8_t *data, size_t len, bool final) {
      if (!r->authenticate(config.web_user.c_str(), config.web_pass.c_str())) return;
      if (!config.otaEnabled) { ota_error = true; return; }
      if (!index) {
        ota_prog = ota_total = 0; ota_error = false;
        Log.printf("[OTA] Start: %s\n", filename.c_str());
        if (!Update.begin(UPDATE_SIZE_UNKNOWN)) { Update.printError(Log); ota_error = true; return; }
      }
      ota_prog += len; ota_total += len;
      if (Update.write(data, len) != len) { Update.printError(Log); ota_error = true; }
      if (final) {
        if (!Update.end(true)) { Update.printError(Log); ota_error = true; } 
        else { Log.println("[OTA] Updated."); }
      }
    }
  );

  server.on("/api/otaprogress", HTTP_GET, [](AsyncWebServerRequest *r){
    if (!r->authenticate(config.web_user.c_str(), config.web_pass.c_str())) return r->requestAuthentication();
    char buf[96];
    snprintf(buf, sizeof(buf), "{\"progress\":%u,\"total\":%u,\"error\":%s}", (unsigned)ota_prog, (unsigned)ota_total, ota_error ? "true":"false");
    r->send(200, "application/json", buf);
  });

  // ------------- Command API -------------
  server.on("/api/cmd", HTTP_POST, [](AsyncWebServerRequest *r) {
    int ch_idx = 0;
    if (r->hasParam("ch", true)) { ch_idx = r->getParam("ch", true)->value().toInt() - 1; }
    if (ch_idx != 0 && ch_idx != 1) ch_idx = 0;
    const ChannelConfig& ch_config = config.ch[ch_idx];
    bool haveOn = r->hasParam("on", true);
    bool haveB  = r->hasParam("b", true);
    if (ch_idx == 0) {
      if (haveOn) {
        bool newOn = (r->getParam("on", true)->value() == "1"); bool prev = lightOn; lightOn = newOn;
        if (newOn) {
          if (dimTargetBrightness < max(ch_config.minOnLevel, ch_config.minBrightness)) { applyTargetBrightness(max(ch_config.minOnLevel, ch_config.minBrightness), true, false); } 
          else { applyTargetBrightness(dimTargetBrightness, true, false); }
        }
        if (prev != lightOn) dimmerBeginPowerTransition();
      }
      if (haveB) { int b = constrain(r->getParam("b", true)->value().toInt(), ch_config.minBrightness, ch_config.maxBrightness); applyTargetBrightness(b, (b > 0), false); }
      sendStateCh(0, true);
    } else {
      if (haveOn) {
        bool newOn = (r->getParam("on", true)->value() == "1"); bool prev = lightOn2; lightOn2 = newOn;
        if (newOn) {
          if (dimTargetBrightness2 < max(ch_config.minOnLevel, ch_config.minBrightness)) { applyTargetBrightness2(max(ch_config.minOnLevel, ch_config.minBrightness), true, false); } 
          else { applyTargetBrightness2(dimTargetBrightness2, true, false); }
        }
        if (prev != lightOn2) dimmerBeginPowerTransition2();
      }
      if (haveB) { int b = constrain(r->getParam("b", true)->value().toInt(), ch_config.minBrightness, ch_config.maxBrightness); applyTargetBrightness2(b, (b > 0), false); }
      sendStateCh(1, true);
    }
    r->send(200, "application/json", "{\"ok\":true}");
  });

  // ------------- Health API -------------
  server.on("/api/health", HTTP_GET, [](AsyncWebServerRequest *r) {
    if (!r->authenticate(config.web_user.c_str(), config.web_pass.c_str())) return r->requestAuthentication();
    r->send(200, "application/json", buildHealthJson());
  });

  // ------------- Backup -------------
  server.on("/api/backup", HTTP_GET, [](AsyncWebServerRequest *r) {
    if (!r->authenticate(config.web_user.c_str(), config.web_pass.c_str())) return r->requestAuthentication();
    DynamicJsonDocument doc(4096);
    doc["fw"] = FW_VERSION; doc["mqtt_server"] = config.mqtt_server; doc["mqtt_port"] = config.mqtt_port;
    doc["mqtt_user"] = config.mqtt_user; doc["mqtt_pass"] = "***"; doc["mqtt_topic"] = config.mqtt_topic;
    doc["mqtt_topic2"] = config.mqtt_topic2; doc["mdns_hostname"] = config.mdns_hostname; doc["web_user"] = config.web_user;
    doc["web_pass"] = "***"; doc["ota_password"] = "***";
    for (int i=0; i<2; i++) {
        String p = "ch" + String(i+1) + "_";
        doc[p+"holdSPS"] = config.ch[i].holdSPS; doc[p+"dimStep"] = config.ch[i].dimStep;
        doc[p+"minBright"] = config.ch[i].minBrightness; doc[p+"maxBright"] = config.ch[i].maxBrightness;
        doc[p+"fadeDelay"] = config.ch[i].fadeStepDelayMs; doc[p+"longPress"] = config.ch[i].longPressMs;
        doc[p+"startB"] = config.ch[i].startBrightness; doc[p+"startOn"] = config.ch[i].startOn;
        doc[p+"restore"] = config.ch[i].restoreLast; doc[p+"reverse"] = config.ch[i].reverseDirection;
        doc[p+"autoOff"] = config.ch[i].autoOffMinutes; doc[p+"preset1"] = config.ch[i].preset1Brightness;
        doc[p+"preset2"] = config.ch[i].preset2Brightness; doc[p+"ease"] = config.ch[i].easeFade;
        doc[p+"gammaEn"] = config.ch[i].gammaEnabled; doc[p+"gamma"] = config.ch[i].gamma;
        doc[p+"transMs"] = config.ch[i].transitionMs; doc[p+"minOnLvl"] = config.ch[i].minOnLevel;
    }
    doc["debugEnabled"] = config.debugEnabled; doc["otaEnabled"] = config.otaEnabled; doc["haEnabled"] = config.haEnabled;
    doc["haExtras"] = config.haExtras; doc["haOnBoot"] = config.haOnBoot; doc["haPrefix"] = config.haPrefix;
    doc["haName"] = config.haName; doc["mqttKeepalive"] = config.mqttKeepalive; doc["backoffMinMs"] = config.backoffMinMs;
    doc["backoffMaxMs"] = config.backoffMaxMs; doc["telemetrySec"] = config.telemetrySec;
    String j; serializeJson(doc, j);
    AsyncWebServerResponse *res = r->beginResponse(200, "application/json", j);
    res->addHeader("Content-Disposition", "attachment; filename=\"esp32_dimmer_backup.json\"");
    r->send(res);
  });

  // ------------- Restore -------------
  server.on("/restore", HTTP_POST,
    [](AsyncWebServerRequest *r){ if (!r->authenticate(config.web_user.c_str(), config.web_pass.c_str())) { r->send(401); return; } r->send(200); }, NULL,
    [](AsyncWebServerRequest *r, uint8_t *data, size_t len, size_t index, size_t total) {
      if (!r->authenticate(config.web_user.c_str(), config.web_pass.c_str())) return;
      if (index == 0) {
        DynamicJsonDocument doc(total + 256);
        if (deserializeJson(doc, (const char*)data, len)) { r->send(400); return; }
        config.mqtt_server = doc["mqtt_server"] | config.mqtt_server; config.mqtt_port = doc["mqtt_port"] | config.mqtt_port;
        config.mqtt_user = doc["mqtt_user"] | config.mqtt_user; if (doc["mqtt_pass"] != "***") config.mqtt_pass = doc["mqtt_pass"] | config.mqtt_pass;
        config.mqtt_topic = doc["mqtt_topic"] | config.mqtt_topic; config.mqtt_topic2 = doc["mqtt_topic2"] | config.mqtt_topic2;
        config.mdns_hostname = doc["mdns_hostname"] | config.mdns_hostname; config.web_user = doc["web_user"] | config.web_user;
        if (doc["web_pass"] != "***") config.web_pass = doc["web_pass"] | config.web_pass; if (doc["ota_password"] != "***") config.ota_password = doc["ota_password"] | config.ota_password;
        for (int i=0; i<2; i++) {
            String p = "ch" + String(i+1) + "_";
            config.ch[i].holdSPS = doc[p+"holdSPS"] | config.ch[i].holdSPS; config.ch[i].dimStep = doc[p+"dimStep"] | config.ch[i].dimStep;
            config.ch[i].minBrightness = doc[p+"minBright"] | config.ch[i].minBrightness; config.ch[i].maxBrightness = doc[p+"maxBright"] | config.ch[i].maxBrightness;
            config.ch[i].fadeStepDelayMs = doc[p+"fadeDelay"] | config.ch[i].fadeStepDelayMs; config.ch[i].longPressMs = doc[p+"longPress"] | config.ch[i].longPressMs;
            config.ch[i].startBrightness = doc[p+"startB"] | config.ch[i].startBrightness; config.ch[i].startOn = doc[p+"startOn"] | config.ch[i].startOn;
            config.ch[i].restoreLast = doc[p+"restore"] | config.ch[i].restoreLast; config.ch[i].reverseDirection = doc[p+"reverse"] | config.ch[i].reverseDirection;
            config.ch[i].autoOffMinutes = doc[p+"autoOff"] | config.ch[i].autoOffMinutes; config.ch[i].preset1Brightness = doc[p+"preset1"] | config.ch[i].preset1Brightness;
            config.ch[i].preset2Brightness = doc[p+"preset2"] | config.ch[i].preset2Brightness; config.ch[i].easeFade = doc[p+"ease"] | config.ch[i].easeFade;
            config.ch[i].gammaEnabled = doc[p+"gammaEn"] | config.ch[i].gammaEnabled; config.ch[i].gamma = doc[p+"gamma"] | config.ch[i].gamma;
            config.ch[i].transitionMs = doc[p+"transMs"] | config.ch[i].transitionMs; config.ch[i].minOnLevel = doc[p+"minOnLvl"] | config.ch[i].minOnLevel;
        }
        config.debugEnabled = doc["debugEnabled"] | config.debugEnabled; config.otaEnabled = doc["otaEnabled"] | config.otaEnabled;
        config.haEnabled = doc["haEnabled"] | config.haEnabled; config.haExtras = doc["haExtras"] | config.haExtras;
        config.haOnBoot = doc["haOnBoot"] | config.haOnBoot; config.haPrefix = doc["haPrefix"] | config.haPrefix;
        config.haName = doc["haName"] | config.haName; config.mqttKeepalive = doc["mqttKeepalive"] | config.mqttKeepalive;
        config.backoffMinMs = doc["backoffMinMs"] | config.backoffMinMs; config.backoffMaxMs = doc["backoffMaxMs"] | config.backoffMaxMs;
        config.telemetrySec = doc["telemetrySec"] | config.telemetrySec;
        if (config.mqtt_topic.endsWith("/set")) config.mqtt_topic.remove(config.mqtt_topic.length()-4);
        if (config.mqtt_topic2.endsWith("/set")) config.mqtt_topic2.remove(config.mqtt_topic2.length()-4);
        clampConfig(); saveConfig(); rebooter.once(1, [](){ ESP.restart(); });
      }
    }
  );

  // ------------- Save Settings -------------
  server.on("/save", HTTP_POST, [](AsyncWebServerRequest *r) {
    auto param = [&](const String& name, bool isCheckbox = false) {
      if (!r->hasParam(name.c_str(), true)) return String();
      if (isCheckbox) return r->getParam(name.c_str(), true)->value() == "on" ? String("on") : String("off");
      return r->getParam(name.c_str(), true)->value();
    };
    if (r->hasParam("mqtt_topic", true)) { config.mqtt_topic = param("mqtt_topic"); if (config.mqtt_topic.endsWith("/set")) config.mqtt_topic.remove(config.mqtt_topic.length()-4); }
    if (r->hasParam("mqtt_topic2", true)) { config.mqtt_topic2 = param("mqtt_topic2"); if (config.mqtt_topic2.endsWith("/set")) config.mqtt_topic2.remove(config.mqtt_topic2.length()-4); }
    if (r->hasParam("mqtt_server", true)) config.mqtt_server = param("mqtt_server"); if (r->hasParam("mqtt_port", true)) config.mqtt_port = param("mqtt_port").toInt();
    if (r->hasParam("mqtt_user", true)) config.mqtt_user = param("mqtt_user"); if (r->hasParam("mqtt_pass", true)) { String p = param("mqtt_pass"); if (p.length()) config.mqtt_pass = p; }
    if (r->hasParam("mqttKeepalive", true)) config.mqttKeepalive = param("mqttKeepalive").toInt(); if (r->hasParam("backoffMinMs", true)) config.backoffMinMs = param("backoffMinMs").toInt();
    if (r->hasParam("backoffMaxMs", true)) config.backoffMaxMs = param("backoffMaxMs").toInt(); if (r->hasParam("telemetrySec", true)) config.telemetrySec = param("telemetrySec").toInt();
    if (r->hasParam("mdns_hostname", true)) config.mdns_hostname = param("mdns_hostname"); if (r->hasParam("web_user", true)) config.web_user = param("web_user");
    if (r->hasParam("web_pass", true)) { String wp = param("web_pass"); if (wp.length()) config.web_pass = wp; } if (r->hasParam("ota_password", true)) { String op = param("ota_password"); if (op.length()) config.ota_password = op; }
    for (int i=0; i<2; i++) {
        String p = "ch" + String(i+1) + "_";
        if (r->hasParam((p+"holdSPS").c_str(), true)) config.ch[i].holdSPS = param(p+"holdSPS").toInt(); if (r->hasParam((p+"dimStep").c_str(), true)) config.ch[i].dimStep = param(p+"dimStep").toInt();
        if (r->hasParam((p+"fadeDelay").c_str(), true)) config.ch[i].fadeStepDelayMs = param(p+"fadeDelay").toInt(); if (r->hasParam((p+"minBright").c_str(), true)) config.ch[i].minBrightness = param(p+"minBright").toInt();
        if (r->hasParam((p+"maxBright").c_str(), true)) config.ch[i].maxBrightness = param(p+"maxBright").toInt(); if (r->hasParam((p+"startB").c_str(), true)) config.ch[i].startBrightness = param(p+"startB").toInt();
        if (r->hasParam((p+"autoOff").c_str(), true)) config.ch[i].autoOffMinutes = param(p+"autoOff").toInt(); if (r->hasParam((p+"preset1").c_str(), true)) config.ch[i].preset1Brightness = param(p+"preset1").toInt();
        if (r->hasParam((p+"preset2").c_str(), true)) config.ch[i].preset2Brightness = param(p+"preset2").toInt(); if (r->hasParam((p+"gamma").c_str(), true)) config.ch[i].gamma = param(p+"gamma").toFloat();
        if (r->hasParam((p+"transMs").c_str(), true)) config.ch[i].transitionMs = param(p+"transMs").toInt(); if (r->hasParam((p+"minOnLvl").c_str(), true)) config.ch[i].minOnLevel = param(p+"minOnLvl").toInt();
        if (r->hasParam((p+"longPress").c_str(), true)) config.ch[i].longPressMs = param(p+"longPress").toInt();
        config.ch[i].startOn = param(p+"startOn", true) == "on"; config.ch[i].restoreLast = param(p+"restore", true) == "on";
        config.ch[i].reverseDirection = param(p+"reverse", true) == "on"; config.ch[i].easeFade = param(p+"ease", true) == "on";
        config.ch[i].gammaEnabled = param(p+"gammaEn", true) == "on";
    }
    config.debugEnabled = param("debugEnabled", true) == "on"; config.otaEnabled = param("otaEnabled", true) == "on";
    config.haEnabled = param("haEnabled", true) == "on"; config.haExtras = param("haExtras", true) == "on";
    config.haOnBoot = param("haOnBoot", true) == "on"; if (r->hasParam("haPrefix", true)) config.haPrefix = param("haPrefix");
    if (r->hasParam("haName", true)) config.haName = param("haName");
    clampConfig();
    if (saveConfig()) { r->send(200, "text/plain", "OK"); rebooter.once(0.5, [](){ ESP.restart(); }); } 
    else { r->send(500, "text/plain", "Failed to save config to NVS"); }
  });

  // ------------- Actions -------------
  server.on("/factoryreset", HTTP_GET, [](AsyncWebServerRequest *r) { if (!r->authenticate(config.web_user.c_str(), config.web_pass.c_str())) return r->requestAuthentication(); r->send(200, "text/plain", "OK"); rebooter.once(0.5, [](){ performWiFiReset(true); }); });
  server.on("/wifireset", HTTP_GET, [](AsyncWebServerRequest *r) { if (!r->authenticate(config.web_user.c_str(), config.web_pass.c_str())) return r->requestAuthentication(); r->send(200, "text/plain", "OK"); rebooter.once(0.5, [](){ performWiFiReset(false); }); });
  server.on("/api/hadiscovery", HTTP_GET, [](AsyncWebServerRequest *r){ if (!r->authenticate(config.web_user.c_str(), config.web_pass.c_str())) return r->requestAuthentication(); String op = r->getParam("op")->value(); if (op == "publish") haDiscoveryPublish(false); else if (op == "unpublish") haDiscoveryPublish(true); r->send(200, "text/plain", "OK"); });
  server.on("/api/testmqtt", HTTP_GET, [](AsyncWebServerRequest *r) { if (!r->authenticate(config.web_user.c_str(), config.web_pass.c_str())) return r->requestAuthentication(); bool ok = mqttPublishTest(); r->send(ok?200:500, "text/plain", ok?"OK":"FAIL"); });
  server.on("/ping", HTTP_GET, [](AsyncWebServerRequest* r){ r->send(200, "text/plain", "pong"); });

  server.begin();
}