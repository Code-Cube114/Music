#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "AudioGeneratorMP3.h"
#include "AudioFileSourceHTTPStream.h"
#include "AudioOutputI2S.h"

// ==================== 配置 ====================
const char *WIFI_SSID = "5-203";
const char *WIFI_PASSWORD = "18939516850";
const char *SERVER_SEARCH = "http://sj.frp.one:45000/search";
const char *SERVER_SONG = "http://sj.frp.one:45000/song";

// 加在全局变量区
enum PlayStatus { STOPPED, PLAYING, FINISHED };
PlayStatus playStatus = STOPPED;

// ==================== Arduino Serial1 UART1 配置 ====================
#define UART1_TX_PIN 22
#define UART1_RX_PIN 23
#define UART1_BAUD 115200

// I2S 引脚
#define I2S_BCK_PIN GPIO_NUM_20
#define I2S_DIN_PIN GPIO_NUM_19
#define I2S_LCK_PIN GPIO_NUM_17

// 播放器
AudioGeneratorMP3 *mp3 = NULL;
AudioFileSourceHTTPStream *file = NULL;
AudioOutputI2S *out = NULL;

int volume = 10;

// ==================== WiFi ====================
void connectWiFi() {
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("WiFi 连接中...");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi 已连接: " + WiFi.localIP().toString());
}

// 设置音量
void setVolume(int vol) {
  if (vol < 0) vol = 0;
  if (vol > 100) vol = 100;
  volume = vol;
  if (out) out->SetGain(volume / 100.0);
  Serial.println("音量已设置为: " + String(vol) + "%");
}

void stopPlay() {
  if (!mp3) return;

  out->stop();
  mp3->stop();
  delay(5);

  delete mp3;
  delete file;
  delete out;
  mp3 = NULL;
  file = NULL;
  out = NULL;
  playStatus = STOPPED;

  Serial.println("完全静音停止");
}

// ==================== 搜索歌曲 ====================
String searchSongByName(String name) {
  HTTPClient http;
  http.begin(SERVER_SEARCH);
  http.addHeader("Content-Type", "application/json");

  JsonDocument req;
  req["keywords"] = name;
  req["limit"] = 1;

  String postData;
  serializeJson(req, postData);
  int code = http.POST(postData);
  String songId = "";

  if (code == 200) {
    String resp = http.getString();
    Serial.println("搜索结果: " + resp);
    JsonDocument doc;
    deserializeJson(doc, resp);
    songId = doc["data"][0]["id"].as<String>();
  }

  http.end();
  return songId;
}

// 获取播放 URL
String getSongUrl(String songId, String level) {
  HTTPClient http;
  http.begin(SERVER_SONG);
  http.addHeader("Content-Type", "application/json");

  JsonDocument req;
  req["id"] = songId;
  req["level"] = level;

  String postData;
  serializeJson(req, postData);

  int code = http.POST(postData);
  String playUrl = "";

  if (code == 200) {
    String resp = http.getString();
    JsonDocument doc;
    deserializeJson(doc, resp);
    playUrl = doc["data"]["url"].as<String>();
  }

  http.end();
  return playUrl;
}

// 播放
void playSongByNameAndLevel(String name, String level) {
  stopPlay();
  Serial.println("搜索: " + name + " | 音质: " + level);

  String songId = searchSongByName(name);
  if (songId == "") {
    Serial.println("未找到歌曲");
    return;
  }

  String url = getSongUrl(songId, level);
  if (url == "") {
    Serial.println("获取播放地址失败");
    return;
  }

  out = new AudioOutputI2S();
  out->SetPinout(I2S_BCK_PIN, I2S_LCK_PIN, I2S_DIN_PIN);
  out->SetChannels(2);
  setVolume(volume);

  file = new AudioFileSourceHTTPStream(url.c_str());
  mp3 = new AudioGeneratorMP3();
  mp3->begin(file, out);
  playStatus = PLAYING;

  Serial.println("开始播放");
}

// 解析指令
void parseUartData(String input) {
  input.trim();
  if (input == "") return;

  if (input.equalsIgnoreCase("STOP")) {
    stopPlay();
    return;
  }

  if (input.startsWith("VOL")) {
    int vol = input.substring(3).toInt();
    setVolume(vol);
    return;
  }

  int plusIndex = input.indexOf('+');
  if (plusIndex == -1) {
    Serial.println("格式错误: 必须是 歌名+质量");
    return;
  }

  String name = input.substring(0, plusIndex);
  String level = input.substring(plusIndex + 1);
  playSongByNameAndLevel(name, level);
}

// ==================== setup ====================
void setup() {
  Serial.begin(115200);
  Serial1.begin(UART1_BAUD, SERIAL_8N1, UART1_RX_PIN, UART1_TX_PIN);

  delay(1000);
  pinMode(WIFI_ENABLE, OUTPUT);
  digitalWrite(WIFI_ENABLE, LOW);

  delay(100);

  pinMode(WIFI_ANT_CONFIG, OUTPUT);
  digitalWrite(WIFI_ANT_CONFIG, HIGH);

  connectWiFi();

  Serial.println("\n=====================================");
  Serial.println("ESP32 播放器已启动 (UART1 接收)");
  Serial.println("接收格式: 歌名+质量 (例: 七里香+高)");
  Serial.println("STOP = 停止");
  Serial.println("VOL80 = 音量");
  Serial.println("=====================================\n");
}
String uartBuffer = "";
// ==================== loop ====================
void loop() {
  if (mp3 && mp3->isRunning()) {
    if (!mp3->loop()) {
      stopPlay();
      Serial.println("播放完成");
      playStatus = FINISHED;
    }
  }

  if (Serial1.available()) {
    // 只读取 1 个字节，不等待、不阻塞
    char c = Serial1.read();
    
    // 如果是换行符，表示一行接收完成
    if (c == '\n') {
      // 去除空格、回车等无用字符
      uartBuffer.trim();
      
      // 只有非空才处理
      if (uartBuffer.length() > 0) {
        Serial.println("收到 UART 指令: " + uartBuffer);
        // 处理指令
        parseUartData(uartBuffer);
      }
      
      // 清空缓冲区，准备接收下一行
      uartBuffer = "";
    }
    // 如果不是换行，就拼接到缓冲区
    else {
      uartBuffer += c;
    }
  }

  delay(1);
}
