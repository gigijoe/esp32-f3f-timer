#ifndef PLAYER_H
#define PLAYER_H

#include <stdint.h>

// I2S Connections
#define I2S_DOUT      22
#define I2S_BCLK      26
#define I2S_LRC       25

void Mp3Player_Init(void);
void Mp3Player_Loop(void);

void Mp3Player_Play(const char *filePath);
void Mp3Player_PlayPriority(const char *filePath);
void Mp3Player_Stop(void);
void Mp3Player_Reset(void);
bool Mp3Player_IsPlaying(void);
const char *Mp3Player_CurrentPlayFile();

uint8_t Mp3Player_GetVolume();
void Mp3Player_SetVolume(uint8_t volume);

#endif