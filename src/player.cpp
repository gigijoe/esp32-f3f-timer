#include <Arduino.h>
#include <Audio.h>

#include "player.h"

Audio audio;

#define MP3_EVENT_PLAY 0x30
#define MP3_EVENT_STOP 0x31
#define MP3_EVENT_PAUSE 0x32
#define MP3_EVENT_RESUME 0x33
#define MP3_EVENT_PRIORITY_PLAY 0x34

typedef enum { mp3Idle, mp3OutOfData, mp3Stopped, mp3Playing, mp3PriorityPlaying, mp3Paused } Mp3State;

static Mp3State mp3State = mp3Idle;

#define FILE_PATH_SIZE 64

typedef struct _Mp3Context {
	char filePath[FILE_PATH_SIZE];
} Mp3Context;

static Mp3Context *Mp3Context_Create(const char *filePath)
{
	Mp3Context *c = (Mp3Context *)pvPortMalloc(sizeof(Mp3Context));
	if(c) {
		memset(c, 0, sizeof(Mp3Context));
		if(filePath)
			strncpy(c->filePath, filePath, FILE_PATH_SIZE);
	} else
		Serial.printf("pvPortMalloc fali\r\n");

	return c;
}

static void Mp3Context_Destroy(Mp3Context *c)
{
	if(c)
		vPortFree(c);
}

static String currentFilePath; 

static bool Mp3Context_Play(Mp3Context *c)
{
    currentFilePath = c->filePath;
    return audio.connecttoFS(SD, c->filePath);
}

static const char *Mp3Context_CurrentPlayFile()
{
    if(audio.isRunning())
        return currentFilePath.c_str();
    else
        return nullptr; 
}

static Mp3Context *pMp3Context = 0;
static Mp3Context *pMp3PriorityContext = 0;

static xQueueHandle eventQueue;
static xQueueHandle mp3ContextQueue;
static xQueueHandle mp3PriorityContextQueue;

void Mp3Player_Init(void)
{
    // Setup I2S 
    audio.setPinout(I2S_BCLK, I2S_LRC, I2S_DOUT);
  
    // Set Volume
    audio.setVolume(21); // default 0...21
  
	eventQueue = xQueueCreate(32, sizeof(uint8_t));
	mp3ContextQueue = xQueueCreate(16, sizeof(Mp3Context *));
	mp3PriorityContextQueue = xQueueCreate(16, sizeof(Mp3Context *));
}

void Mp3Player_Loop(void)
{
    audio.loop(); 

	if(mp3State == mp3OutOfData) {
		audio.stopSong();
        mp3State = mp3Stopped;
	}

	if(mp3State == mp3Stopped) {
		mp3State = mp3Idle;

		//pMp3PriorityContext = Mp3PriorityContextList_PopFront();				
		if(xQueueReceive(mp3PriorityContextQueue, &pMp3PriorityContext, 0)) { /* Play next mp3 */
			if(Mp3Context_Play(pMp3PriorityContext) == true)
				mp3State = mp3PriorityPlaying;
			else {
				Mp3Context_Destroy(pMp3PriorityContext);
				pMp3PriorityContext = 0;
			}
		}

		if(pMp3Context) {
			Mp3Context_Destroy(pMp3Context);
			pMp3Context = 0;
		}

		if(mp3State != mp3PriorityPlaying) {
			//pMp3Context = Mp3ContextList_PopFront();
			if(xQueueReceive(mp3ContextQueue, &pMp3Context, 0)) { /* Play next mp3 */
				if(Mp3Context_Play(pMp3Context) == true)
					mp3State = mp3Playing;
				else {
					Mp3Context_Destroy(pMp3Context);
					pMp3Context = 0;
				}
			}
		}
	}

	uint8_t mp3Event = 0;
	//xQueueReceive(eventQueue, &mp3Event, pdMS_TO_TICKS(1));
	xQueueReceive(eventQueue, &mp3Event, 0);

	switch(mp3State) {
		case mp3Stopped:
//Usart2_Puts("mp3Stopped\r\n");
			break;
		case mp3Idle:
//Usart2_Puts("mp3Idle\r\n");
			if(mp3Event == MP3_EVENT_PRIORITY_PLAY) {
//Usart2_Puts("MP3_EVENT_PRIORITY_PLAY\r\n");
				//pMp3PriorityContext = Mp3PriorityContextList_PopFront();
				if(xQueueReceive(mp3PriorityContextQueue, &pMp3PriorityContext, 0)) {
					if(Mp3Context_Play(pMp3PriorityContext) == true)
						mp3State = mp3PriorityPlaying;
					else {
						Mp3Context_Destroy(pMp3PriorityContext);
						pMp3PriorityContext = 0;
					}					
				}
			} else if(mp3Event == MP3_EVENT_PLAY) {
//Usart2_Puts("MP3_EVENT_PLAY\r\n");
				//pMp3Context = Mp3ContextList_PopFront();								
				if(xQueueReceive(mp3ContextQueue, &pMp3Context, 0)) {
					if(Mp3Context_Play(pMp3Context) == true)
						mp3State = mp3Playing;
					else {
						Mp3Context_Destroy(pMp3Context);
						pMp3Context = 0;
					}
				}
			}
			break;
		case mp3Playing:
//Usart2_Puts("mp3Playing\r\n");
			if(mp3Event == MP3_EVENT_STOP) {
//Usart2_Puts("MP3_EVENT_STOP\r\n");
				if(pMp3Context) {
					audio.stopSong();		
					mp3State = mp3Stopped;
				}
			} else if(mp3Event == MP3_EVENT_PRIORITY_PLAY) {
//Usart2_Puts("MP3_EVENT_PRIORITY_PLAY\r\n");				
				if(pMp3Context) {
					audio.stopSong();		
					mp3State = mp3Stopped;
				}

				//pMp3PriorityContext = Mp3PriorityContextList_PopFront();				
				if(xQueueReceive(mp3PriorityContextQueue, &pMp3PriorityContext, 0)) {					
					if(Mp3Context_Play(pMp3PriorityContext) == true) {
						if(pMp3Context) {
							Mp3Context_Destroy(pMp3Context);
							pMp3Context = 0;
						}
						mp3State = mp3PriorityPlaying;
					} else {
						Mp3Context_Destroy(pMp3PriorityContext);
						pMp3PriorityContext = 0;
					}					
				}				
			}
			break;
		case mp3PriorityPlaying:
			if(mp3Event == MP3_EVENT_STOP) {
//Usart2_Puts("MP3_EVENT_STOP\r\n");
				if(pMp3PriorityContext){
					audio.stopSong();		
					mp3State = mp3Stopped;
				}
			} else { /* Normal loop */
				if(audio.isRunning() == false) { /* EOF */
					audio.stopSong();
 					mp3State = mp3Stopped;
				}
			}
			break;
		case mp3Paused:
//Usart2_Puts("mp3Paused\r\n");
			break;
		case mp3OutOfData:
//Usart2_Puts("mp3OutOfData\r\n");
			break;
		default:
//Usart2_Puts("mp3Unknown\r\n");
			break;		
	}

    if(audio.isRunning() == false)
        mp3State = mp3OutOfData;
}

void Mp3Player_Play(const char *filePath)
{
	Mp3Context *c = Mp3Context_Create(filePath);
	if(c == 0)
		return;

    Serial.printf("%s:%d - %s\r\n", __FUNCTION__, __LINE__, c->filePath);
	xQueueSend(mp3ContextQueue, &c, portMAX_DELAY);
	uint8_t r = MP3_EVENT_PLAY;
	xQueueSend(eventQueue, &r, 0);
	yield();
}

void Mp3Player_PlayPriority(const char *filePath)
{
	Mp3Context *c = Mp3Context_Create(filePath);
	if(c == 0)
		return;

    Serial.printf("%s:%d - %s\r\n", __FUNCTION__, __LINE__, c->filePath);
	xQueueSend(mp3PriorityContextQueue, &c, portMAX_DELAY);
	uint8_t r = MP3_EVENT_PRIORITY_PLAY;
	xQueueSend(eventQueue, &r, 0);
	yield();
}

void Mp3Player_Stop(void)
{
	if(mp3State == mp3Playing || mp3State == mp3PriorityPlaying) {
		uint8_t r = MP3_EVENT_STOP;
		xQueueSend(eventQueue, &r, 0);
		yield();
	}
}

void Mp3Player_Reset(void)
{	
	while(1) {
		Mp3Context *c = 0;
		xQueueReceive(mp3ContextQueue, &c, 0);
		if(c == 0)
			break;
		Mp3Context_Destroy(c);
	}

	while(1) {
		Mp3Context *c = 0;
		xQueueReceive(mp3PriorityContextQueue, &c, 0);
		if(c == 0)
			break;
		Mp3Context_Destroy(c);
	}
}

bool Mp3Player_IsPlaying(void)
{
    return audio.isRunning();
}

const char *Mp3Player_CurrentPlayFile()
{
    return Mp3Context_CurrentPlayFile();
}

uint8_t Mp3Player_GetVolume()
{
Serial.printf("audio.getVolume() = %d\r\n", audio.getVolume());
    return audio.getVolume();
}

void Mp3Player_SetVolume(uint8_t volume)
{
Serial.printf("volume = %d\r\n", volume);
    if(volume > audio.maxVolume())
        volume = audio.maxVolume();
    audio.setVolume(volume);
}