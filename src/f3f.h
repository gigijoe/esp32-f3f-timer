/*
 * f3f.h
 *
 *  Created on: 25 Feb 2015
 *      Author: Steve Chang
 *
 */
#ifndef F3F_H_
#define F3F_H_

typedef enum { showCpuUsage, showWindData, showLastRecord, showVolume, showMaximum } HeadLineType;

void F3F_Init(void (*headLineCb)(HeadLineType type));
void F3F_AnemometerDecode(unsigned char *data, unsigned int len);

void F3F_TiggleBaseA(uint32_t serNo);
void F3F_TiggleBaseB(uint32_t serNo);

void F3F_KeyStart();
void F3F_KeyA();
void F3F_KeyB();
void F3F_KeyStop();

void F3F_KeyBaseA();
void F3F_KeyBaseB();

void F3F_Task(void * pvParameters);
const char *F3F_LastRecord();

#endif