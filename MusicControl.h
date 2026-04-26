#ifndef __MUSIC_CONTROL_H__
#define __MUSIC_CONTROL_H__

#include "mcp_server.h"
#include "led_strip.h"
#include "math.h"
#include <driver/uart.h>
void uart_send_str(const char* str) {
    uart_write_bytes(UART_NUM_1, str, strlen(str));
}


class MusicControl {
private:
    gpio_num_t rx;
    gpio_num_t tx;
    bool audio_playing = false;
public:
    MusicControl(gpio_num_t rx_pin, gpio_num_t tx_pin) : rx(rx_pin), tx(tx_pin) {
        // 初始化 UART
        uart_config_t uart_config = {
            .baud_rate = 115200,
            .data_bits = UART_DATA_8_BITS,
            .parity = UART_PARITY_DISABLE,
            .stop_bits = UART_STOP_BITS_1,
            .flow_ctrl = UART_HW_FLOWCTRL_DISABLE
        };
        uart_driver_install(UART_NUM_1, 1024 * 2, 0, 0, nullptr, 0);
        uart_param_config(UART_NUM_1, &uart_config);
        uart_set_pin(UART_NUM_1, tx, rx, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
        auto& mcp_server = McpServer::GetInstance();

        mcp_server.AddTool("self.music.play", 
            "当用户要播放音乐时，使用这个mcp来让跟uart连接的音箱播放音乐\n"
            "song_name: 要播放的歌曲名称\n"
            "music_quality: 音乐质量，可以是 standard 或 high\n"
                        "", 
            PropertyList({
                Property("song_name", kPropertyTypeString),
                Property("music_quality", kPropertyTypeString, "standard")
            }), 
            [this](const PropertyList& properties) -> ReturnValue {
                std::string song_name = properties["song_name"].value<std::string>();
                std::string music_quality = properties["music_quality"].value<std::string>();
                // 这里可以根据 song_name 和 music_quality 来控制 UART 连接的音箱播放音乐
                uart_send_str((song_name+"+"+music_quality+"\n").c_str());
                audio_playing = true;
                return true;
            }
        );

        mcp_server.AddTool("self.music.stop", 
            "当用户要停止播放音乐时，使用这个mcp来让跟uart连接的音箱停止播放音乐\n"
                        "", 
            PropertyList(), 
            [this](const PropertyList& properties) -> ReturnValue {
                uart_send_str("STOP\n");
                audio_playing = false;
                return true;
            }
        );

        mcp_server.AddTool("self.music.volume", 
            "当用户要调整音乐音量时，使用这个mcp来让跟uart连接的音箱调整音量\n"
                        "", 
            PropertyList({
                Property("volume", kPropertyTypeInteger, 10)
            }), 
            [this](const PropertyList& properties) -> ReturnValue {
                int volume = properties["volume"].value<int>();
                uart_send_str(("VOL" + std::to_string(volume) + "\n").c_str());
                return true;
            }
        );
    }
};


#endif 
