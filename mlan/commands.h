/*
 * Copyright (c) Dustin Sallings <dustin@spy.net>
 *
 * $Id: commands.h,v 1.4 2001/08/29 09:27:50 dustin Exp $
 */

#ifndef COMMANDS_H
#define COMMANDS_H 1

#define READ_ROM 0x33
#define MATCH_ROM 0x55
#define SEARCH_ROM 0xf0
#define SKIP_ROM 0xcc
#define OVERDRIVE_SKIP_ROM 0x3c
#define OVERDRIVE_MATCH_ROM 0x69
#define CONDITIONAL_SEARCH_ROM 0xec

#define WRITE_SCRATCHPAD 0x0f
#define READ_SCRATCHPAD 0xaa
#define COPY_SCRATCHPAD 0x55
#define READ_MEMORY 0xf0
#define READ_MEMORY_CRC 0xa5
#define CLEAR_MEMORY 0x3c
#define CONVERT_TEMPERATURE 0x44
#define RECALL 0xb8

/* Special stuff the 1920 uses */
#define DS1920WRITE_SCRACTHPAD 0x4e
#define DS1920READ_SCRATCHPAD  0xbe
#define DS1920COPY_SCRATCHPAD  0x48

/* Special 2406/2407 stuff */
#define DS2406CHANNEL_ACCESS 0xf5
#define DS2406READ_STATUS 0xaa
#define DS2406WRITE_STATUS 0x55

/* 2438 stuff */
/* Convert voltage */
#define DS2438CONVERTV 0xb4
/* Convert temperature */
#define DS2438CONVERTT CONVERT_TEMPERATURE
#define DS2438WRITE_SCRATCHPAD 0x4e
#define DS2438READ_SCRATCHPAD 0xbe

#endif /* COMMANDS_H */

/*
 * arch-tag: 200C3FEE-13E5-11D8-9303-000393CFE6B8
 */
