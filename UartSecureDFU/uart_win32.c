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

#include <string.h>
#include "uart_drv.h"
#include "logging.h"

int uart_drv_open(uart_drv_t *p_uart)
{
	int err_code = 0;
	const char *portName = p_uart->p_PortName;
	CHAR portFileName[14];
	HANDLE handlePort_ = INVALID_HANDLE_VALUE;

	strcpy(portFileName, "\\\\.\\");
	if (strlen(portName) <= 6)
		strcat(portFileName, portName);
	else
	{
		logger_error("Invalid COM port!");

		err_code = 1;
	}

	if (!err_code)
	{
		handlePort_ = CreateFile(portFileName,  // Specify port device: default "COM1"
			GENERIC_READ | GENERIC_WRITE,       // Specify mode that open device.
			0,                                  // the devide isn't shared.
			NULL,                               // the object gets a default security.
			OPEN_EXISTING,                      // Specify which action to take on file. 
			0,                                  // default.
			NULL);                              // default.

		if (handlePort_ == INVALID_HANDLE_VALUE)
		{
			logger_error("Cannot open COM port!");

			err_code = 1;
		}
	}

	if (!err_code)
	{
		DCB config_;

		// Get current configuration of serial communication port.
		if (GetCommState(handlePort_, &config_) == FALSE)
		{
			logger_error("Cannot get COM configuration!");

			err_code = 1;
		}

		if (!err_code)
		{
			config_.BaudRate = 115200;		// Specify buad rate of communicaiton.
			config_.StopBits = 0;			// Specify stopbit of communication.
			config_.Parity = 0;				// Specify parity of communication.
			config_.ByteSize = 8;			// Specify byte of size of communication.
			config_.fDtrControl = DTR_CONTROL_DISABLE;
			config_.fDsrSensitivity = 0;
			config_.fRtsControl = RTS_CONTROL_HANDSHAKE;
			config_.fInX = 0;
			config_.fOutX = 0;
			config_.fBinary = 1;
			if (SetCommState(handlePort_, &config_) == FALSE)
			{
				logger_error("Cannot set COM configuration!");

				err_code = 1;
			}
		}
	}

	if (!err_code)
	{
		// instance an object of COMMTIMEOUTS.
		COMMTIMEOUTS comTimeOut;
		// Specify value is added to the product of the 
		// ReadTotalTimeoutMultiplier member
		comTimeOut.ReadTotalTimeoutConstant = 500;
		// Specify value that is multiplied 
		// by the requested number of bytes to be read. 
		comTimeOut.ReadTotalTimeoutMultiplier = 10;
		// Specify time-out between charactor for receiving.
		comTimeOut.ReadIntervalTimeout = 100;
		// Specify value that is multiplied 
		// by the requested number of bytes to be sent. 
		comTimeOut.WriteTotalTimeoutMultiplier = 15;
		// Specify value is added to the product of the 
		// WriteTotalTimeoutMultiplier member
		comTimeOut.WriteTotalTimeoutConstant = 300;
		// set the time-out parameter into device control.
		SetCommTimeouts(handlePort_, &comTimeOut);
	}

	if (!err_code)
	{
		if (PurgeComm(handlePort_, PURGE_RXCLEAR) == FALSE)
		{
			logger_error("Cannot purge COM RX buffer!");

			err_code = 1;
		}
	}

	if (err_code && handlePort_ != INVALID_HANDLE_VALUE)
	{
		p_uart->portHandle = handlePort_;
		uart_drv_close(p_uart);

		handlePort_ = INVALID_HANDLE_VALUE;
	}

	p_uart->portHandle = handlePort_;

	return err_code;
}

int uart_drv_close(uart_drv_t *p_uart)
{
	int err_code = 0;
	HANDLE portHandle = p_uart->portHandle;

	if (portHandle != INVALID_HANDLE_VALUE)
	{
		if (CloseHandle(portHandle) == FALSE)    // Call this function to close port.
		{
			logger_error("Cannot close COM port!");

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
	HANDLE portHandle = p_uart->portHandle;
	DWORD length;

	if (WriteFile(portHandle,           // handle to file to write to
		pData,                          // pointer to data to write to file
		nSize,                          // number of bytes to write
		&length, NULL) == FALSE ||      // pointer to number of bytes written
		length < nSize)
	{
		logger_error("Cannot write COM port!");

		err_code = 1;
	}

	return err_code;
}

int uart_drv_receive(uart_drv_t *p_uart, uint8_t *pData, uint32_t nSize, uint32_t *pSize)
{
	int err_code = 0;
	HANDLE portHandle = p_uart->portHandle;
	DWORD length = 0;

	if (ReadFile(portHandle,            // handle of file to read
		pData,                          // pointer to data to read from file
		nSize,                          // number of bytes to read
		&length,                        // pointer to number of bytes read
		NULL) == FALSE)                 // pointer to structure for data
	{
		logger_error("Cannot read COM port!");

		err_code = 1;
	}

	*pSize = length;

	return err_code;
}
