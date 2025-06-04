#include <Arduino.h>
#include "f3f.h"

xQueueHandle keyPressQueue;

#define MS_TIMER_RESET (0)
#define MS_TIMER_RUNNING (1 << 0)
#define MS_TIMER_TIMEOUT (1 << 1)
#define MS_TIMER_STOPPED (1 << 2)

typedef void(*MsTimerIntervalCb)(uint32_t time_ms);
typedef void(*MsTimerTimeoutCb)(void);

void DefaultIntervalCb(uint32_t time_ms)
{
  (void)time_ms;
}

void DefaultTimeoutCb(void)
{
  return;
}

typedef struct _MsTimer {
  uint8_t status;
  uint32_t timeout;
  uint32_t startTick;
  uint32_t interval, intervalCount;
  MsTimerIntervalCb intervalCb;
  MsTimerTimeoutCb timeoutCb;
} MsTimer;

void MsTimer_Reset(MsTimer *mt);
void MsTimer_StartEx(MsTimer *mt, uint32_t ms, MsTimerTimeoutCb cb, uint32_t startTick);
void MsTimer_Start(MsTimer *mt, uint32_t ms, MsTimerTimeoutCb cb);
void MsTimer_SetupInterval(MsTimer *mt, uint32_t ms, MsTimerIntervalCb cb);
void MsTimer_Stop(MsTimer *mt);
uint32_t MsTimer_Duration(MsTimer *mt, uint32_t ms_tick);
uint8_t MsTimer_Status(MsTimer *mt) { return mt->status; }

void MsTimer_Reset(MsTimer *mt)
{
  memset(mt, 0, sizeof(MsTimer));
  mt->intervalCb = DefaultIntervalCb;
  mt->timeoutCb = DefaultTimeoutCb;
}

void MsTimer_StartEx(MsTimer *mt, uint32_t ms, MsTimerTimeoutCb cb, uint32_t startTick)
{
  if(mt->status & MS_TIMER_RUNNING)
    return;

  mt->startTick = startTick;
  mt->intervalCount = 0;

  mt->timeout = ms;
  mt->timeoutCb = cb;

  if(mt->interval > mt->timeout)
    mt->interval = 0;

  mt->status |= MS_TIMER_RUNNING;
}

void MsTimer_Start(MsTimer *mt, uint32_t ms, MsTimerTimeoutCb cb)
{
  MsTimer_StartEx(mt, ms, cb, millis());
}

void MsTimer_SetupInterval(MsTimer *mt, uint32_t ms, MsTimerIntervalCb cb)
{
  mt->interval = ms;
  mt->intervalCb = cb;
}

void MsTimer_Stop(MsTimer *mt)
{
  if((mt->status & MS_TIMER_RUNNING) == 0)
    return;

  mt->status &= (~MS_TIMER_RUNNING);
  mt->status |= MS_TIMER_STOPPED;
}

uint32_t MsTimer_Duration(MsTimer *mt, uint32_t ms_tick)
{
  return (ms_tick > mt->startTick) ? ms_tick - mt->startTick : 0;
}

void MsTimer_Loop(MsTimer *mt)
{
  if(!(mt->status & MS_TIMER_RUNNING))
    return;
  
  if(mt->status & MS_TIMER_TIMEOUT)
    return;

  uint32_t st = mt->startTick;
//Usart2_Printf("%s:%d\r\n", __FUNCTION__, __LINE__);
  if(mt->interval) {
    uint32_t ic = (millis() - st) / mt->interval;
    if(ic > mt->intervalCount) {
//Usart2_Printf("%s:%d\n", __FUNCTION__, __LINE__);
//Usart2_Printf("millis() - st = %d\r\n", millis() - st);
      mt->intervalCb(millis() - st);
      mt->intervalCount = ic;
    }
  }

  if(millis() - st >= mt->timeout) {
    mt->status |= MS_TIMER_TIMEOUT;
    mt->timeoutCb();
  }
}

/*
*
*/

enum { KEY_0, KEY_1, KEY_2, KEY_3, 
    KEY_4, KEY_5, KEY_6, KEY_7, 
    KEY_8, KEY_9, KEY_A, KEY_B, 
    KEY_C, KEY_D, KEY_STAR, KEY_SHARP,
    KEY_START, KEY_STOP, KEY_BASE_A, KEY_BASE_B, KEY_MAX };
  
#define MUTEX_UNLOCK 0
#define MUTEX_LOCK 1
  
typedef struct _F3F_State {
    void (*OnEnter)(uint32_t ms_tick);
    void (*OnLoop)(void);
    void (*OnKey)(uint8_t key, uint32_t ms_tick);
    void (*OnTimeout)(void);
    void (*OnInterval)(uint32_t time_ms);
    unsigned int timeout;
} F3F_State;
  
static MsTimer stateTimer; 
  
static void IdleState_OnEnter(uint32_t ms_tick);
static void IdleState_OnLoop(void);
static void IdleState_OnKey(uint8_t key, uint32_t ms_tick);
static void IdleState_OnTimeout(void);
static void IdleState_OnInterval(uint32_t time_ms);
  
static void ThirtySecondState_OnEnter(uint32_t ms_tick);
static void ThirtySecondState_OnLoop(void);
static void ThirtySecondState_OnKey(uint8_t key, uint32_t ms_tick);
static void ThirtySecondState_OnTimeout(void);
static void ThirtySecondState_OnInterval(uint32_t time_ms);
  
static void CourseState_OnEnter(uint32_t ms_tick);
static void CourseState_OnLoop(void);
static void CourseState_OnKey(uint8_t key, uint32_t ms_tick);
static void CourseState_OnTimeout(void);
static void CourseState_OnInterval(uint32_t time_ms);
  
static void FinishState_OnEnter(uint32_t ms_tick);
static void FinishState_OnLoop(void);
static void FinishState_OnKey(uint8_t key, uint32_t ms_tick);
static void FinishState_OnTimeout(void);
static void FinishState_OnInterval(uint32_t time_ms);
  
/* State implement */
  
static F3F_State idleState = {
    IdleState_OnEnter,
    IdleState_OnLoop,
    IdleState_OnKey,
    IdleState_OnTimeout,
    IdleState_OnInterval,
    30
};
  
static F3F_State thirtySecondState = {
    ThirtySecondState_OnEnter,
    ThirtySecondState_OnLoop,
    ThirtySecondState_OnKey,
    ThirtySecondState_OnTimeout,
    ThirtySecondState_OnInterval,
    30
};
  
static F3F_State courseState = {
    CourseState_OnEnter,
    CourseState_OnLoop,
    CourseState_OnKey,
    CourseState_OnTimeout,
    CourseState_OnInterval,
    0
};
  
static F3F_State finishState = {
    FinishState_OnEnter,
    FinishState_OnLoop,
    FinishState_OnKey,
    FinishState_OnTimeout,
    FinishState_OnInterval,
    0
};
  
F3F_State *currentState = &idleState;

const char strBlank[] = "                    ";
static char strLastRecord[11] = "XXXXXXXXXX";
  
static uint8_t keypadMap[4][4] = {
    { KEY_1, KEY_2, KEY_3, KEY_A },
    { KEY_4, KEY_5, KEY_6, KEY_B },
    { KEY_7, KEY_8, KEY_9, KEY_C },
    { KEY_STAR, KEY_0, KEY_SHARP, KEY_D },
};
  
static void F3F_StateOnKey(uint8_t row, uint8_t col, uint32_t ms_tick)
{
    Serial.printf("KEY %d\r\n", keypadMap[col][row]);
    currentState->OnKey(keypadMap[col][row], ms_tick);
}
  
/*
*
*/

#include "player.h"
#include "lcd204.h"

static HeadLineType s_headLine = showCpuUsage;
static void (*s_headLineCb)(HeadLineType type) = nullptr;

/*
*
*/

//static uint8_t led_buf[32];
static uint32_t serNoA = 0;
static uint32_t serNoB = 0;

static bool canBeReFlight = false;

const char strPressStart[] = "Press Start";
const char strReady[] = "Ready";

static void IdleState_OnEnter(uint32_t ms_tick)
{
  canBeReFlight = false;
  MsTimer_Reset(&stateTimer);

  lcdPrintRow(2, strPressStart);
  lcdPrintRow(3, strReady);
}

static void IdleState_OnLoop()
{
  MsTimer_Loop(&stateTimer);
}

static void IdleState_OnKey(uint8_t key, uint32_t ms_tick)
{
  switch(key)
  {
    case KEY_START:
        Mp3Player_Stop();
        Mp3Player_Reset();
        Mp3Player_Play("vocal/go.mp3");
        currentState = &thirtySecondState;
        currentState->OnEnter(ms_tick);
        //s_headLine = showWindData;
        //lcdPrintRow(0, "Wind speed & range  ");
        break;
    case KEY_STOP: {
        Mp3Player_Stop();
        Mp3Player_Reset();
        currentState->OnEnter(ms_tick);
        int i = s_headLine;
        s_headLine = static_cast<HeadLineType>(++i);
        if(s_headLine >= showMaximum)
        s_headLine = showCpuUsage;       
        if(s_headLineCb) {
          s_headLineCb(s_headLine);
        }
        }  break;
    case KEY_BASE_A: lcdPrintRow(0, "<A%04d>", serNoA);
        break;
    case KEY_BASE_B: lcdPrintRow(0, "<B%04d>", serNoB);
        break;
    case KEY_A:
        switch(s_headLine) {
          case showVolume:
            if(Mp3Player_GetVolume() < 0xff)
              Mp3Player_SetVolume(Mp3Player_GetVolume() + 1);
            lcdPrintRow(0, "Volume : %d dbm", Mp3Player_GetVolume());
            break;
          default:
            break;
        } break;
    case KEY_B:
        switch(s_headLine) {
          case showVolume:
            if(Mp3Player_GetVolume() > 0)
              Mp3Player_SetVolume(Mp3Player_GetVolume() - 1);
            lcdPrintRow(0, "Volume : %d dbm", Mp3Player_GetVolume());
            break;
          default: 
            break;
        } break;
    case KEY_C:
    case KEY_D:
        break;
    }
}

static void IdleState_OnTimeout()
{  
  Mp3Player_Stop();
  Mp3Player_Reset();
  Mp3Player_Play("music/smb_die.mp3");
}

static void IdleState_OnInterval(uint32_t time_ms)
{
}

/**/

const char strOutSide[] = "Out side";
const char strThirtySecond[] = "30 Seconds";
static bool thirtySecondOutSide = false;
static bool thirtySecondTimeOut = false;

static void ThirtySecondState_OnEnter(uint32_t ms_tick)
{
  thirtySecondOutSide = false;
  thirtySecondTimeOut = false;

  MsTimer_Reset(&stateTimer);
  MsTimer_SetupInterval(&stateTimer, 10, ThirtySecondState_OnInterval); /* 10 ms interval */
  MsTimer_Start(&stateTimer, 30000, ThirtySecondState_OnTimeout); /* 30 seconds */

  lcdPrintRow(2, strThirtySecond);
  lcdPrintRow(3, strBlank);
}

static void ThirtySecondState_OnLoop()
{
  MsTimer_Loop(&stateTimer);
}

static void ThirtySecondState_OnKey(uint8_t key, uint32_t ms_tick)
{
  switch(key)
  {
    case KEY_STOP:
      Mp3Player_Stop();
      Mp3Player_Reset();
      Mp3Player_Play("music/smb_die.mp3");
      currentState = &idleState;
      currentState->OnEnter(ms_tick);
      break;
    case KEY_BASE_A:
      if(thirtySecondOutSide) {
        currentState = &courseState;
        currentState->OnEnter(ms_tick);
      } else {
        lcdPrintRow(2, strOutSide);
        Mp3Player_PlayPriority("vocal/outside.mp3");
        thirtySecondOutSide = true;
      }
      break;
    case KEY_BASE_B:
      break;
  }
}

static void ThirtySecondState_OnTimeout()
{
  thirtySecondTimeOut = true;
  currentState = &courseState;
  currentState->OnEnter(millis());  
}

static void ThirtySecondState_OnInterval(uint32_t time_ms)
{
  static uint32_t sec = 0;
  uint32_t t = 30000 - time_ms;
  uint32_t s = t / 1000;
  uint32_t ms = t % 1000;

  lcdPrintRow(3, "               %2u.%02u", s, ms / 10);
/*
  if(ms != 0)
    return;
*/
  if(sec != s) {
    if(s == 20)
      Mp3Player_Play("vocal/20.mp3");
    else if(s == 10)
      Mp3Player_Play("vocal/10.mp3");
    else if(s < 10) {
      if(s == 9)
        Mp3Player_Play("vocal/9.mp3");
      else if(s == 8)
        Mp3Player_Play("vocal/8.mp3");
      else if(s == 7)
        Mp3Player_Play("vocal/7.mp3");
      else if(s == 6)
        Mp3Player_Play("vocal/6.mp3");
      else if(s == 5)
        Mp3Player_Play("vocal/5.mp3");
      else if(s == 4)
        Mp3Player_Play("vocal/4.mp3");
      else if(s == 3)
        Mp3Player_Play("vocal/3.mp3");
      else if(s == 2)
        Mp3Player_Play("vocal/2.mp3");
      else if(s == 1)
        Mp3Player_Play("vocal/1.mp3");
      else if(s == 0)
        //Mp3Player_Play("vocal/0.mp3");
        Mp3Player_Play("music/smb_warning.mp3");
    }
    sec = s;
  }
}

/**/

char strBuf[16];
static uint8_t courseProgressCount = 0;

static void CourseState_OnEnter(uint32_t ms_tick)
{
  courseProgressCount = 0;

  MsTimer_Reset(&stateTimer);
  MsTimer_SetupInterval(&stateTimer, 10, CourseState_OnInterval);
  MsTimer_StartEx(&stateTimer, 999000, CourseState_OnTimeout, ms_tick); /* 999 seconds */
  
  if(thirtySecondTimeOut == false) {
    Mp3Player_PlayPriority("vocal/rA.mp3");
    courseProgressCount++;
  }

  snprintf(strBuf, 16, "Course %d", courseProgressCount);
  lcdPrintRow(2, strBuf);
  lcdPrintRow(3, strBlank);
}

static void CourseState_OnLoop()
{
  MsTimer_Loop(&stateTimer);
}

static void CourseState_OnKey(uint8_t key, uint32_t ms_tick)
{
  switch(key)
  {
    case KEY_STOP:
      Mp3Player_Stop();
      Mp3Player_Reset();
      Mp3Player_Play("music/smb_die.mp3");
      currentState = &idleState;
      currentState->OnEnter(ms_tick);
      break;
    case KEY_BASE_A:
      if(thirtySecondOutSide == false) {
        lcdPrintRow(2, strOutSide);
        Mp3Player_PlayPriority("vocal/outside.mp3");
        thirtySecondOutSide = true;
      } else {
        if(courseProgressCount % 2 == 0) {
          if(courseProgressCount == 10) {
            Mp3Player_PlayPriority("vocal/rE.mp3");
            MsTimer_Stop(&stateTimer);
            currentState = &finishState;
            currentState->OnEnter(MsTimer_Duration(&stateTimer, ms_tick));
            break;
          } else 
            Mp3Player_PlayPriority("vocal/rA.mp3");
          courseProgressCount++;
          snprintf(strBuf, 16, "Course %d", courseProgressCount);
          lcdPrintRow(2, strBuf);
        }
      }
      break;
    case KEY_BASE_B:
      if(courseProgressCount % 2 == 1) {
        if(courseProgressCount == 9)
          Mp3Player_PlayPriority("vocal/rFinal.mp3");
        else
          Mp3Player_PlayPriority("vocal/rB.mp3");
        courseProgressCount++;
        snprintf(strBuf, 16, "Course %d", courseProgressCount);
        lcdPrintRow(2, strBuf);
      }
      break;
  }
}

static void CourseState_OnTimeout()
{
}

static void CourseState_OnInterval(uint32_t time_ms)
{
  lcdPrintRow(3, "              %3u.%02u", time_ms / 1000, (time_ms % 1000) / 10);
}

/**/

static void SaveResult(const char *id, const char *result)
{
#if 0    
  char filePath[128];
  FRESULT res;
  DIR dir;

  sprintf(filePath, "f3f");
  res = f_opendir(&dir, filePath); /* Open the directory */
  if(res == FR_NO_PATH)
    f_mkdir("f3f");
  res = f_opendir(&dir, filePath);
  if(res != FR_OK) {
    Usart2_Printf("Fail open %s\r\n", filePath);
    return;
  }

  snprintf(filePath, 128, "/f3f/%s.dat", id);

  FIL fp;
  res = f_open(&fp, filePath, FA_OPEN_ALWAYS | FA_WRITE);
  if(res != FR_OK) {
    Usart2_Printf("Fail open %s\r\n", filePath);
    return;   
  }

  res = f_lseek(&fp, fp.fsize);
  if(res != FR_OK) {
    f_close(&fp);
    Usart2_Printf("Fail seek %s\r\n", filePath);
    return;   
  }

  Usart2_Printf("Open %s successful\r\n", filePath);

  f_puts(result, &fp);
  f_puts("\n", &fp);

  f_close(&fp);  
#endif
}

static const char *itov(uint32_t i)
{
  switch(i) {
    case 0: return "vocal/0.mp3";
    case 1: return "vocal/1.mp3";
    case 2: return "vocal/2.mp3";
    case 3: return "vocal/3.mp3";
    case 4: return "vocal/4.mp3";
    case 5: return "vocal/5.mp3";
    case 6: return "vocal/6.mp3";
    case 7: return "vocal/7.mp3";
    case 8: return "vocal/8.mp3";
    case 9: return "vocal/9.mp3";
    case 10: return "vocal/10.mp3";
    case 11: return "vocal/11.mp3";
    case 12: return "vocal/12.mp3";
    case 13: return "vocal/13.mp3";
    case 14: return "vocal/14.mp3";
    case 15: return "vocal/15.mp3";
    case 16: return "vocal/16.mp3";
    case 17: return "vocal/17.mp3";
    case 18: return "vocal/18.mp3";
    case 19: return "vocal/19.mp3";
    case 20: return "vocal/20.mp3";
    case 30: return "vocal/30.mp3";
    case 40: return "vocal/40.mp3";
    case 50: return "vocal/50.mp3";
    case 60: return "vocal/60.mp3";
    case 70: return "vocal/70.mp3";
    case 80: return "vocal/80.mp3";
    case 90: return "vocal/90.mp3";
    case 100: return "vocal/100.mp3";
    default:
      return "NULL";
  } 

  return "NULL";
}

const char strFinish[] = "Finish";
const char strReFlight[] = "Re-flight";

static void FinishState_OnEnter(uint32_t ms_tick)
{
  MsTimer_Reset(&stateTimer);

  uint32_t cs = (ms_tick % 1000) / 10;
  uint32_t s = ms_tick / 1000;
  lcdPrintRow(2, strFinish);
  lcdPrintRow(3, "              %3lu.%02lu", s, cs);
  snprintf(strLastRecord, 10, "%lu.%02lu", s, cs);

  if(s < 20) {
    Mp3Player_Play(itov(s));
  } else if(s < 100) {
    Mp3Player_Play(itov(s - (s % 10)));
    if((s % 10) != 0)
      Mp3Player_Play(itov(s % 10));
  }
  Mp3Player_Play("vocal/rPoint.mp3");
  if(cs < 10) {
    Mp3Player_Play(itov(0));
    Mp3Player_Play(itov(cs));
  } else {
    Mp3Player_Play(itov(cs / 10));
    Mp3Player_Play(itov(cs % 10));
  }

  if(s < 30) {
    Mp3Player_Play("music/smb_world_clear.mp3");
  }

  if(canBeReFlight) {
    lcdPrintRow(2, strReFlight);
    Mp3Player_Play("vocal/re-flight.mp3");
  }
}

static void FinishState_OnLoop()
{
}

static void FinishState_OnKey(uint8_t key, uint32_t ms_tick)
{
  switch(key)
  {
    case KEY_STOP:
      Mp3Player_Stop();
      Mp3Player_Reset();
      currentState = &idleState;
      currentState->OnEnter(ms_tick);
      break;
  }
}

static void FinishState_OnTimeout()
{
}

static void FinishState_OnInterval(uint32_t time_ms)
{
}

void F3F_Init(void (*headLineCb)(HeadLineType type))
{
  s_headLineCb = headLineCb;
  keyPressQueue = xQueueCreate(36, sizeof(uint8_t));

  MsTimer_Reset(&stateTimer);
#if 0
  Keypad_Register(0, 0, F3F_StateOnKey);
  Keypad_Register(0, 1, F3F_StateOnKey);
  Keypad_Register(0, 2, F3F_StateOnKey);
  Keypad_Register(0, 3, F3F_StateOnKey);
  Keypad_Register(1, 0, F3F_StateOnKey);
  Keypad_Register(1, 1, F3F_StateOnKey);
  Keypad_Register(1, 2, F3F_StateOnKey);
  Keypad_Register(1, 3, F3F_StateOnKey);
  Keypad_Register(2, 0, F3F_StateOnKey);
  Keypad_Register(2, 1, F3F_StateOnKey);
  Keypad_Register(2, 2, F3F_StateOnKey);
  Keypad_Register(2, 3, F3F_StateOnKey);
  Keypad_Register(3, 0, F3F_StateOnKey);
  Keypad_Register(3, 1, F3F_StateOnKey);
  Keypad_Register(3, 2, F3F_StateOnKey);
  Keypad_Register(3, 3, F3F_StateOnKey);
#endif
  lcdNoCursor();
  lcdClear();

  currentState->OnEnter(millis());
}

void F3F_KeyStart()
{
    static uint32_t latestTick = 0;

    if(millis() - latestTick > 200) { /* 200ms debounce */
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        uint8_t r = KEY_START;
        xQueueSendFromISR(keyPressQueue, &r, &xHigherPriorityTaskWoken);
    }
    latestTick = millis();
}

void F3F_KeyA()
{
    static uint32_t latestTick = 0;

    if(millis() - latestTick > 200) { /* 200ms debounce */
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        uint8_t r = KEY_A;
        xQueueSendFromISR(keyPressQueue, &r, &xHigherPriorityTaskWoken);
    }
    latestTick = millis();
}

void F3F_KeyB()
{
    static uint32_t latestTick = 0;

    if(millis() - latestTick > 200) { /* 200ms debounce */
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        uint8_t r = KEY_B;
        xQueueSendFromISR(keyPressQueue, &r, &xHigherPriorityTaskWoken);
    }
    latestTick = millis();
}

void F3F_KeyStop()
{
    static uint32_t latestTick = 0;

    if(millis() - latestTick > 200) { /* 200ms debounce */
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        uint8_t r = KEY_STOP;
        xQueueSendFromISR(keyPressQueue, &r, &xHigherPriorityTaskWoken);
    }
    latestTick = millis();
}

void F3F_KeyBaseA()
{
    static uint32_t latestTick = 0;

    if(millis() - latestTick > 200) { /* 200ms debounce */
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        uint8_t r = KEY_BASE_A;
        xQueueSendFromISR(keyPressQueue, &r, &xHigherPriorityTaskWoken);
    }
    latestTick = millis();
}

void F3F_KeyBaseB()
{
    static uint32_t latestTick = 0;

    if(millis() - latestTick > 200) { /* 200ms debounce */
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        uint8_t r = KEY_BASE_B;
        xQueueSendFromISR(keyPressQueue, &r, &xHigherPriorityTaskWoken);
    }
    latestTick = millis();
}

/*
 * 
 */
  
static unsigned char atohex8(unsigned char *s)
{
  unsigned char value = 0;
  if(!s)
    return 0;

  if(*s >= '0' && *s <= '9')
    value = (*s - '0') << 4;
  else if(*s >= 'A' && *s <= 'F')
    value = ((*s - 'A') + 10) << 4;

  s++;

  if(*s >= '0' && *s <= '9')
    value |= (*s - '0');
  else
    value |= ((*s - 'A') + 10);

  return value;
}

union Uint32 {
  float f;
  unsigned char u[4];
};

static float atof32(unsigned char *s)
{
  union Uint32 u32;

  u32.u[1] = atohex8(s);
  s+=2;
  u32.u[0] = atohex8(s);
  s+=2;
  u32.u[3] = atohex8(s);
  s+=2;
  u32.u[2] = atohex8(s);

  return u32.f;
}

#include <string.h>

void F3F_AnemometerDecode(unsigned char *data, unsigned int len)
{
  unsigned char i;
  unsigned char *p = (unsigned char *)data;
  unsigned short direction = 0;
  float speed = 0;

  for(i=0;i<len;i++) {
    if(*p == ':') /* Start of MODBUS packet */
      break;
    else
      p++;
  }

  if(i == len) /* illegal data */
    return;

  if(memcmp(p, ":OOR:1", 6) == 0) { /* Wind speed out of range */
    lcdPrintRow(1, "Wind Speed !!!");
    return;
  } else if(memcmp(p, ":OOR:2", 6) == 0) { /* Wind direction out of range */
    lcdPrintRow(1, "Wind Dir !!!");
    return;
  } else
    lcdPrintRow(1, strBlank);

  if(s_headLine != showWindData)
    return;

  p++; /* Ignore token ':' */
  i = atohex8(p); /* addr */
  if(i == 1) { /* addr 1 - Wind speed*/
    p+=2;
    i = atohex8(p); /* Func */
    if(i != 3)
      return;
    p+=2;
    i = atohex8(p); /* Data length */
    p+=2;
    i = atohex8(p); /* Wind direction */
    direction = i << 8;
    p+=2;
    i = atohex8(p);
    direction |= i;
    p+=2;
    speed = atof32(p);
    //Usart2_Printf("%f m/s %s %3d Deg\r\n", speed, (direction < 180) ? "<<" : ">>", direction);
    //lcdPrintRow(0, "%.2f m/s %s %3d Deg", (double)speed, (direction < 180) ? "<<" : ">>", direction);

    int32_t t = (int32_t)(speed * 100);
    uint32_t s = t / 100;
    uint32_t cs = t % 100;

    lcdPrintRow(0, "%d.%d m/s %s %3d Deg", s, cs, (direction < 180) ? "<<" : ">>", direction);
  }
}

void F3F_Task(void * pvParameters)
{
  Mp3Player_Init();

  while(1) {
    uint8_t r;
    if(xQueueReceive(keyPressQueue, &r, pdMS_TO_TICKS(1))) {
    //if(xQueueReceive(keyPressQueue, &r, 0)) {
      F3F_State *s = currentState;
      switch(r) {
        case KEY_START: s->OnKey(KEY_START, millis());
          break;
        case KEY_A: s->OnKey(KEY_A, millis());
          break;
        case KEY_B: s->OnKey(KEY_B, millis());
          break;
        case KEY_STOP: s->OnKey(KEY_STOP, millis());
          break;
        case KEY_BASE_A: s->OnKey(KEY_BASE_A, millis());
          break;
        case KEY_BASE_B: s->OnKey(KEY_BASE_B, millis());
          break;
      }
    }
    currentState->OnLoop();

    Mp3Player_Loop();
  }
  vTaskDelete(NULL);
}

void F3F_TiggleBaseA(uint32_t serNo)
{
/*  
  serNoA = serNo;
  currentState->OnKey(KEY_BASE_A, millis());
  //extiKeyMux &= (~(1 << 5));

  currentState->OnLoop();
*/
  serNoA = serNo;
  F3F_KeyBaseA();
}

void F3F_TiggleBaseB(uint32_t serNo)
{
/*  
  serNoB = serNo;
  currentState->OnKey(KEY_BASE_B, millis());
  //extiKeyMux &= (~(1 << 6));

  currentState->OnLoop();
*/
  serNoB = serNo;
  F3F_KeyBaseB();
}

const char *F3F_LastRecord()
{
  return strLastRecord;
}