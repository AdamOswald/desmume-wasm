/*
	Copyright (C) 2006 Normmatt
	Copyright (C) 2007 Pascal Giard
	Copyright (C) 2007-2017 DeSmuME team

	This file is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 2 of the License, or
	(at your option) any later version.

	This file is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with the this software.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef _SRAM_H
#define _SRAM_H

#include "types.h"
#ifdef HAVE_LIBZ
#include "zlib.h"
#endif
#define NB_STATES 10

extern int lastSaveFile;

typedef struct 
{
  BOOL exists;
  char date[40];
} savefiles_t;


struct SFORMAT
{
	//a string description of the element
	const char *desc;

	//the size of each element
	u32 size;

	//the number of each element
	u32 count;

	//a void* to the data or a void** to the data
	void *v;
};

extern savefiles_t savefiles[NB_Files];

void clear_savefiles();
void scan_savefiles();
u8 sram_read (u32 address);
void sram_write (u32 address, u8 value);
int sram_load (const char *file_name);
int sram_save (const char *file_name);

bool savefile_load (const char *file_name);
bool savefile_save (const char *file_name);

void savefile_slot(int num);
void loadfile_slot(int num);

bool savefile_load(class EMUFILE &is);
bool savefile_save(class EMUFILE &outstream, int compressionLevel = -1);

#endif
