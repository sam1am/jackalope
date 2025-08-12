#include "globals.h"
#include "display_handler.h"
#include "driver/i2s.h"

void init_audio()
{
    i2s_config_t i2s_config = {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
        .sample_rate = SAMPLE_RATE,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT,
        .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = 8,
        .dma_buf_len = 64,
        .use_apll = false};
    if (i2s_driver_install(I2S_NUM_1, &i2s_config, 0, NULL) != ESP_OK)
    {
        update_display(1, "Mic FAIL");
        Serial.println("Mic Init Failed!");
        return;
    }
    i2s_pin_config_t pin_config = {
        .bck_io_num = I2S_SCK,
        .ws_io_num = I2S_WS,
        .data_out_num = I2S_PIN_NO_CHANGE,
        .data_in_num = I2S_SD};
    i2s_set_pin(I2S_NUM_1, &pin_config);
    update_display(1, "Mic Init OK");
    Serial.println("Mic Initialized.");
}

void create_wav_header(byte *header, int data_size, int bitsPerSample)
{
    int32_t sampleRate = SAMPLE_RATE;
    int16_t numChannels = 1;
    int16_t blockAlign = numChannels * bitsPerSample / 8;
    int32_t byteRate = sampleRate * blockAlign;
    int32_t subchunk2Size = data_size;
    int32_t chunkSize = 36 + subchunk2Size;

    header[0] = 'R';
    header[1] = 'I';
    header[2] = 'F';
    header[3] = 'F';
    *((int32_t *)(header + 4)) = chunkSize;
    header[8] = 'W';
    header[9] = 'A';
    header[10] = 'V';
    header[11] = 'E';
    header[12] = 'f';
    header[13] = 'm';
    header[14] = 't';
    header[15] = ' ';
    *((int32_t *)(header + 16)) = 16;
    *((int16_t *)(header + 20)) = 1; // PCM
    *((int16_t *)(header + 22)) = numChannels;
    *((int32_t *)(header + 24)) = sampleRate;
    *((int32_t *)(header + 28)) = byteRate;
    *((int16_t *)(header + 32)) = blockAlign;
    *((int16_t *)(header + 34)) = bitsPerSample;
    header[36] = 'd';
    header[37] = 'a';
    header[38] = 't';
    header[39] = 'a';
    *((int32_t *)(header + 40)) = subchunk2Size;
}