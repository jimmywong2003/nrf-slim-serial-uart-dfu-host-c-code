/**
* Copyright (c) 2018, Nordic Semiconductor ASA
*
* All rights reserved.
*
* Redistribution and use in source and binary forms, with or without modification,
* are permitted provided that the following conditions are met:
*
* 1. Redistributions of source code must retain the above copyright notice, this
*    list of conditions and the following disclaimer.
*
* 2. Redistributions in binary form, except as embedded into a Nordic
*    Semiconductor ASA integrated circuit in a product or a software update for
*    such product, must reproduce the above copyright notice, this list of
*    conditions and the following disclaimer in the documentation and/or other
*    materials provided with the distribution.
*
* 3. Neither the name of Nordic Semiconductor ASA nor the names of its
*    contributors may be used to endorse or promote products derived from this
*    software without specific prior written permission.
*
* 4. This software, with or without modification, must only be used with a
*    Nordic Semiconductor ASA integrated circuit.
*
* 5. Any software provided in binary form under this license must not be reverse
*    engineered, decompiled, modified and/or disassembled.
*
* THIS SOFTWARE IS PROVIDED BY NORDIC SEMICONDUCTOR ASA "AS IS" AND ANY EXPRESS
* OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
* OF MERCHANTABILITY, NONINFRINGEMENT, AND FITNESS FOR A PARTICULAR PURPOSE ARE
* DISCLAIMED. IN NO EVENT SHALL NORDIC SEMICONDUCTOR ASA OR CONTRIBUTORS BE
* LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
* CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
* GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
* HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
* LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
* OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*
*/

#include <stdio.h>
#include <stdarg.h>
#include "logging.h"

int m_level = LOGGER_INFO_LVL_0;

void logger_error(const char* format, ...)
{
    va_list argptr;
    va_start(argptr, format);
    vfprintf(stderr, format, argptr);
    va_end(argptr);
	putchar('\n');
}

void logger_info(const char* format, va_list arg_list)
{
	vfprintf(stdout, format, arg_list);
	putchar('\n');
}

void logger_info_1(const char* format, ...)
{
	if (m_level >= LOGGER_INFO_LVL_1)
	{
		va_list argptr;
		va_start(argptr, format);
		logger_info(format, argptr);
		va_end(argptr);
	}
}

void logger_info_2(const char* format, ...)
{
	if (m_level >= LOGGER_INFO_LVL_2)
	{
		va_list argptr;
		va_start(argptr, format);
		logger_info(format, argptr);
		va_end(argptr);
	}
}

void logger_info_3(const char* format, ...)
{
	if (m_level >= LOGGER_INFO_LVL_3)
	{
		va_list argptr;
		va_start(argptr, format);
		logger_info(format, argptr);
		va_end(argptr);
	}
}

void logger_set_info_level(int level)
{
	m_level = level;
}

int logger_get_info_level(void)
{
	return m_level;
}
