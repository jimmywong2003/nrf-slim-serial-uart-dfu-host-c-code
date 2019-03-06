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

#include <stdbool.h>
#include "slip_enc.h"

#define	SLIP_END				0300
#define	SLIP_ESC				0333
#define	SLIP_ESC_END			0334
#define	SLIP_ESC_ESC			0335

void encode_slip(uint8_t *pDestData, uint32_t *pDestSize, const uint8_t *pSrcData, uint32_t nSrcSize)
{
	uint32_t n, nDestSize;

	nDestSize = 0;

	for (n = 0; n < nSrcSize; n++)
	{
		uint8_t nSrcByte = *(pSrcData + n);

		if (nSrcByte == SLIP_END)
		{
			*pDestData++ = SLIP_ESC;
			*pDestData++ = SLIP_ESC_END;
			nDestSize += 2;
		}
		else if (nSrcByte == SLIP_ESC)
		{
			*pDestData++ = SLIP_ESC;
			*pDestData++ = SLIP_ESC_ESC;
			nDestSize += 2;
		}
		else
		{
			*pDestData++ = nSrcByte;
			nDestSize++;
		}
	}

	*pDestData = SLIP_END;
	nDestSize++;

	*pDestSize = nDestSize;
}

int decode_slip(uint8_t *pDestData, uint32_t *pDestSize, const uint8_t *pSrcData, uint32_t nSrcSize)
{
	int err_code = 1;
	uint32_t n, nDestSize = 0;
	bool is_escaped = false;

	for (n = 0; n < nSrcSize; n++)
	{
		uint8_t nSrcByte = *(pSrcData + n);

		if (nSrcByte == SLIP_END)
		{
			if (!is_escaped)
				err_code = 0;  // Done. OK

			break;
		}
		else if (nSrcByte == SLIP_ESC)
		{
			if (is_escaped)
			{
				// should not get SLIP_ESC twice...
				err_code = 1;
				break;
			}
			else
				is_escaped = true;
		}
		else if (nSrcByte == SLIP_ESC_END)
		{
			if (is_escaped)
			{
				is_escaped = false;

				*pDestData++ = SLIP_END;
			}
			else
				*pDestData++ = nSrcByte;

			nDestSize++;
		}
		else if (nSrcByte == SLIP_ESC_ESC)
		{
			if (is_escaped)
			{
				is_escaped = false;

				*pDestData++ = SLIP_ESC;
			}
			else
				*pDestData++ = nSrcByte;

			nDestSize++;
		}
		else
		{
			*pDestData++ = nSrcByte;
			nDestSize++;
		}
	}

	*pDestSize = nDestSize;

	return err_code;
}
