/*
 * Copyright (c) Dustin Sallings <dustin@spy.net>
 *
 * $Id: commands.h,v 1.1 2000/07/13 23:44:26 dustin Exp $
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

#endif /* COMMANDS_H */
