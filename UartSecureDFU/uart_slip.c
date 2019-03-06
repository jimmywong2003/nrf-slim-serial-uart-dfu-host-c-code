/**
* Copyright (c) 2017, Nordic Semiconductor ASA
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
#include "uart_slip.h"
#include "slip_enc.h"
#include "logging.h"

#define UART_SLIP_BUFF_SIZE		(UART_SLIP_SIZE_MAX * 2 + 1)

static uint8_t uart_slip_buff[UART_SLIP_BUFF_SIZE];

int uart_slip_open(uart_drv_t *p_uart)
{
	return uart_drv_open(p_uart);
}

int uart_slip_close(uart_drv_t *p_uart)
{
	return uart_drv_close(p_uart);
}

int uart_slip_send(uart_drv_t *p_uart, const uint8_t *pData, uint32_t nSize)
{
	int err_code = 0;
	uint32_t nSlipSize;

	if (nSize > UART_SLIP_SIZE_MAX)
	{
		logger_error("Cannot encode SLIP!");

		err_code = 1;
	}
	else
	{
		encode_slip(uart_slip_buff, &nSlipSize, pData, nSize);

		err_code = uart_drv_send(p_uart, uart_slip_buff, nSlipSize);
	}

	return err_code;
}

int uart_slip_receive(uart_drv_t *p_uart, uint8_t *pData, uint32_t nSize, uint32_t *pSize)
{
	int err_code = 0;
	uint32_t sizeBuffer;
	uint32_t length, slip_len = 0;

	do
	{
		sizeBuffer = sizeof(uart_slip_buff) - slip_len;
		if (!sizeBuffer)
		{
			logger_error("UART buffer overflow!");

			err_code = 1;

			break;
		}

		length = 0;
		err_code = uart_drv_receive(p_uart, uart_slip_buff + slip_len, sizeBuffer, &length);
		if (err_code)
			break;

		if (!length)
		{
			logger_error("Read no data from UART!");

			err_code = 1;

			break;
		}

		slip_len += length;

		if (!decode_slip(pData, pSize, uart_slip_buff, slip_len))
		{
			break;
		}
	} while (!err_code);

	return err_code;
}
