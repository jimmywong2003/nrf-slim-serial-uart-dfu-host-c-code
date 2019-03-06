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

#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <string.h>
#include "uart_drv.h"
#include "logging.h"

int uart_drv_open(uart_drv_t *p_uart)
{
	int err_code = 0;
	int fd = -1;
	const char *tty_name = p_uart->p_PortName;
	char tty_path[20];
	struct termios options;

	strcpy(tty_path, "/dev/");
	if (strlen(tty_name) <= 14)
		strcat(tty_path, tty_name);
	else
	{
		logger_error("Invalid TTY port!");

		err_code = 1;
	}

	if (!err_code)
	{
		fd = open(tty_path, O_RDWR | O_NOCTTY);

		if (fd < 0)
		{
			logger_error("Cannot open TTY port!");

			err_code = 1;
		}
	}

	if (!err_code)
	{
		// clear all flags
		memset(&options, 0, sizeof(options));

		// 115200bps
		cfsetispeed(&options, B115200);
		cfsetospeed(&options, B115200);
		// 8N1
		options.c_cflag &= ~PARENB;
		options.c_cflag &= ~CSTOPB;
		options.c_cflag &= ~CSIZE;
		options.c_cflag |= CS8;
		// ignore DCD line
		options.c_cflag |= (CLOCAL | CREAD);
		// enable RTS/CTS handshake
		options.c_cflag |= CRTSCTS;
		// disabe XON/XOFF handshake
		options.c_iflag &= ~(IXON | IXOFF | IXANY);
		// disabe input mapping options
		options.c_iflag &= ~(ISTRIP | INLCR | IGNCR | ICRNL | IUCLC);
		// select RAW input
		options.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
		// select RAW output
		options.c_oflag &= ~OPOST;
		// disabe output mapping options
		options.c_oflag &= ~(OLCUC | ONLCR | OCRNL | ONOCR | ONLRET);
		// set read timeout
		options.c_cc[VMIN] = 0;
		options.c_cc[VTIME] = 5;  // 0.5 seconds

		if (tcsetattr(fd, TCSANOW, &options))
		{
			logger_error("Cannot set TTY options!");

			err_code = 1;
		}
	}

	if (!err_code)
	{
		if (tcflush(fd, TCIFLUSH))
		{
			logger_error("Cannot flush TTY RX buffer!");

			err_code = 1;
		}
	}

	if (err_code && fd >= 0)
	{
		p_uart->tty_fd = fd;
		uart_drv_close(p_uart);

		fd = -1;
	}

	p_uart->tty_fd = fd;

	return err_code;
}

int uart_drv_close(uart_drv_t *p_uart)
{
	int err_code = 0;
	int fd = p_uart->tty_fd;

	if (fd >= 0)
	{
		if (close(fd))
		{
			logger_error("Cannot close TTY port!");

			err_code = 1;
		}
	}
	else
		err_code = 1;
	
	return err_code;
}

int uart_drv_send(uart_drv_t *p_uart, const uint8_t *pData, uint32_t nSize)
{
	int err_code = 0;
	int32_t length;

	length = write(p_uart->tty_fd, pData, nSize);
	if (length != nSize)
	{
		logger_error("Cannot write TTY port!");

		err_code = 1;
	}
	else
	{
		if (tcdrain(p_uart->tty_fd))
		{
			logger_error("Cannot drain TTY TX buffer!");

			err_code = 1;
		}
	}
	
	return err_code;
}

int uart_drv_receive(uart_drv_t *p_uart, uint8_t *pData, uint32_t nSize, uint32_t *pSize)
{
	int err_code = 0;
	int32_t length;
	
	length = read(p_uart->tty_fd, pData, nSize);
	if (length < 0)
	{
		logger_error("Cannot read TTY port!");

		err_code = 1;
	}
	else
		*pSize = length;
	
	return err_code;
}
