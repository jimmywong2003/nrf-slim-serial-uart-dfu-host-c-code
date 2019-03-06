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
#include <string.h>
#include "dfu_serial.h"
#include "crc32.h"
#include "logging.h"

// SLIP data log buffer size
#define MAX_BUFF_SIZE           1024

/**
* @brief DFU protocol operation.
*/
typedef enum
{
	NRF_DFU_OP_PROTOCOL_VERSION  = 0x00,     //!< Retrieve protocol version.
	NRF_DFU_OP_OBJECT_CREATE     = 0x01,     //!< Create selected object.
	NRF_DFU_OP_RECEIPT_NOTIF_SET = 0x02,     //!< Set receipt notification.
	NRF_DFU_OP_CRC_GET           = 0x03,     //!< Request CRC of selected object.
	NRF_DFU_OP_OBJECT_EXECUTE    = 0x04,     //!< Execute selected object.
	NRF_DFU_OP_OBJECT_SELECT     = 0x06,     //!< Select object.
	NRF_DFU_OP_MTU_GET           = 0x07,     //!< Retrieve MTU size.
	NRF_DFU_OP_OBJECT_WRITE      = 0x08,     //!< Write selected object.
	NRF_DFU_OP_PING              = 0x09,     //!< Ping.
	NRF_DFU_OP_HARDWARE_VERSION  = 0x0A,     //!< Retrieve hardware version.
	NRF_DFU_OP_FIRMWARE_VERSION  = 0x0B,     //!< Retrieve firmware version.
	NRF_DFU_OP_ABORT             = 0x0C,     //!< Abort the DFU procedure.
	NRF_DFU_OP_RESPONSE          = 0x60,     //!< Response.
	NRF_DFU_OP_INVALID           = 0xFF
} nrf_dfu_op_t;

/**
* @brief DFU operation result code.
*/
typedef enum
{
	NRF_DFU_RES_CODE_INVALID                 = 0x00,    //!< Invalid opcode.
	NRF_DFU_RES_CODE_SUCCESS                 = 0x01,    //!< Operation successful.
	NRF_DFU_RES_CODE_OP_CODE_NOT_SUPPORTED   = 0x02,    //!< Opcode not supported.
	NRF_DFU_RES_CODE_INVALID_PARAMETER       = 0x03,    //!< Missing or invalid parameter value.
	NRF_DFU_RES_CODE_INSUFFICIENT_RESOURCES  = 0x04,    //!< Not enough memory for the data object.
	NRF_DFU_RES_CODE_INVALID_OBJECT          = 0x05,    //!< Data object does not match the firmware and hardware requirements, the signature is wrong, or parsing the command failed.
	NRF_DFU_RES_CODE_UNSUPPORTED_TYPE        = 0x07,    //!< Not a valid object type for a Create request.
	NRF_DFU_RES_CODE_OPERATION_NOT_PERMITTED = 0x08,    //!< The state of the DFU process does not allow this operation.
	NRF_DFU_RES_CODE_OPERATION_FAILED        = 0x0A,    //!< Operation failed.
	NRF_DFU_RES_CODE_EXT_ERROR               = 0x0B,    //!< Extended error. The next byte of the response contains the error code of the extended error (see @ref nrf_dfu_ext_error_code_t.
} nrf_dfu_result_t;

/**
* @brief @ref NRF_DFU_OP_OBJECT_SELECT response details.
*/
typedef struct
{
	uint32_t offset;                    //!< Current offset.
	uint32_t crc;                       //!< Current CRC.
	uint32_t max_size;                  //!< Maximum size of selected object.
} nrf_dfu_response_select_t;

/**
* @brief @ref NRF_DFU_OP_CRC_GET response details.
*/
typedef struct
{
	uint32_t offset;                    //!< Current offset.
	uint32_t crc;                       //!< Current CRC.
} nrf_dfu_response_crc_t;


#define MIN(a,b) (((a) < (b)) ? (a) : (b))


static uint8_t ping_id = 0;
static uint16_t prn = 0;
static uint16_t mtu = 0;

static uint8_t send_data[UART_SLIP_SIZE_MAX];
static uint8_t receive_data[UART_SLIP_SIZE_MAX];

static char logger_buff[MAX_BUFF_SIZE];

static uint16_t get_uint16_le(const uint8_t *p_data)
{
	uint16_t data;

	data  = ((uint16_t)*(p_data + 0) << 0);
	data += ((uint16_t)*(p_data + 1) << 8);

	return data;
}

static void put_uint16_le(uint8_t *p_data, uint16_t data)
{
	*(p_data + 0) = (uint8_t)(data >> 0);
	*(p_data + 1) = (uint8_t)(data >> 8);
}

static uint32_t get_uint32_le(const uint8_t *p_data)
{
	uint32_t data;

	data  = ((uint32_t)*(p_data + 0) <<  0);
	data += ((uint32_t)*(p_data + 1) <<  8);
	data += ((uint32_t)*(p_data + 2) << 16);
	data += ((uint32_t)*(p_data + 3) << 24);

	return data;
}

static void put_uint32_le(uint8_t *p_data, uint32_t data)
{
	*(p_data + 0) = (uint8_t)(data >>  0);
	*(p_data + 1) = (uint8_t)(data >>  8);
	*(p_data + 2) = (uint8_t)(data >> 16);
	*(p_data + 3) = (uint8_t)(data >> 24);
}

static void uart_data_to_buff(const uint8_t *pData, uint32_t nSize)
{
	uint32_t n;
	char data_buff[6];
	int len, pos;

	logger_buff[0] = '\0';
	pos = 0;

	for (n = 0; n < nSize; n++)
	{
		if (!n)
			len = sprintf(data_buff, "%u", *(pData + n));
		else
			len = sprintf(data_buff, ", %u", *(pData + n));

		if ((size_t)len + 1 < sizeof(logger_buff) - pos)
		{
			strcat(logger_buff, data_buff);

			pos += len;
		}
		else
		{
			// not enough data buffer...
			break;
		}
	}
}

static int dfu_serial_send(uart_drv_t *p_uart, const uint8_t *pData, uint32_t nSize)
{
	int info_lvl = logger_get_info_level();

	if (info_lvl >= LOGGER_INFO_LVL_3)
	{
		uart_data_to_buff(pData, nSize);
		logger_info_3("SLIP: --> [%s]", logger_buff);
	}

	return uart_slip_send(p_uart, pData, nSize);
}

static int dfu_serial_get_rsp(uart_drv_t *p_uart, nrf_dfu_op_t oper, uint32_t *p_data_cnt)
{
	int err_code;

	err_code = uart_slip_receive(p_uart, receive_data, sizeof(receive_data), p_data_cnt);

	if (!err_code)
	{
		int info_lvl = logger_get_info_level();

		if (info_lvl >= LOGGER_INFO_LVL_3)
		{
			uart_data_to_buff(receive_data, *p_data_cnt);
			logger_info_3("SLIP: <-- [%s]", logger_buff);
		}

		if (*p_data_cnt >= 3 &&
			receive_data[0] == NRF_DFU_OP_RESPONSE &&
			receive_data[1] == oper)
		{
			if (receive_data[2] != NRF_DFU_RES_CODE_SUCCESS)
			{
				uint16_t rsp_error = receive_data[2];

				// get 2-byte error code, if applicable
				if (*p_data_cnt >= 4)
					rsp_error = (rsp_error << 8) + receive_data[3];

				logger_error("Bad result code (0x%X)!", rsp_error);

				err_code = 1;
			}
		}
		else
		{
			logger_error("Invalid response!");

			err_code = 1;
		}
	}

	return err_code;
}

static int dfu_serial_ping(uart_drv_t *p_uart, uint8_t id)
{
	int err_code;
	uint8_t send_data[2] = { NRF_DFU_OP_PING };

	send_data[1] = id;
	err_code = dfu_serial_send(p_uart, send_data, sizeof(send_data));

	if (!err_code)
	{
		uint32_t data_cnt;

		err_code = dfu_serial_get_rsp(p_uart, NRF_DFU_OP_PING, &data_cnt);

		if (!err_code)
		{
			if (data_cnt != 4 ||
				receive_data[3] != id)
			{
				logger_error("Bad ping id!");

				err_code = 1;
			}
		}
	}

	return err_code;
}

static int dfu_serial_set_prn(uart_drv_t *p_uart, uint16_t prn)
{
	int err_code;
	uint8_t send_data[3] = { NRF_DFU_OP_RECEIPT_NOTIF_SET };
	
	logger_info_2("Set Packet Receipt Notification %u", prn);

	put_uint16_le(send_data + 1, prn);
	err_code = dfu_serial_send(p_uart, send_data, sizeof(send_data));

	if (!err_code)
	{
		uint32_t data_cnt;

		err_code = dfu_serial_get_rsp(p_uart, NRF_DFU_OP_RECEIPT_NOTIF_SET, &data_cnt);
	}

	return err_code;
}

static int dfu_serial_get_mtu(uart_drv_t *p_uart, uint16_t *p_mtu)
{
	int err_code;
	uint8_t send_data[1] = { NRF_DFU_OP_MTU_GET };

	err_code = dfu_serial_send(p_uart, send_data, sizeof(send_data));

	if (!err_code)
	{
		uint32_t data_cnt;

		err_code = dfu_serial_get_rsp(p_uart, NRF_DFU_OP_MTU_GET, &data_cnt);

		if (!err_code)
		{
			if (data_cnt == 5)
			{
				uint16_t mtu = get_uint16_le(receive_data + 3);

				*p_mtu = mtu;
			}
			else
			{
				logger_error("Invalid MTU!");

				err_code = 1;
			}
		}
	}

	return err_code;
}

static int dfu_serial_select_obj(uart_drv_t *p_uart, uint8_t obj_type, nrf_dfu_response_select_t *p_select_rsp)
{
	int err_code;
	uint8_t send_data[2] = { NRF_DFU_OP_OBJECT_SELECT };

	logger_info_2("Selecting Object: type:%u", obj_type);

	send_data[1] = obj_type;
	err_code = dfu_serial_send(p_uart, send_data, sizeof(send_data));

	if (!err_code)
	{
		uint32_t data_cnt;

		err_code = dfu_serial_get_rsp(p_uart, NRF_DFU_OP_OBJECT_SELECT, &data_cnt);

		if (!err_code)
		{
			if (data_cnt == 15)
			{
				p_select_rsp->max_size = get_uint32_le(receive_data + 3);
				p_select_rsp->offset   = get_uint32_le(receive_data + 7);
				p_select_rsp->crc      = get_uint32_le(receive_data + 11);

				logger_info_2("Object selected:  max_size:%u offset:%u crc:0x%08X", p_select_rsp->max_size, p_select_rsp->offset, p_select_rsp->crc);
			}
			else
			{
				logger_error("Invalid object response!");

				err_code = 1;
			}
		}
	}

	return err_code;
}

static int dfu_serial_create_obj(uart_drv_t *p_uart, uint8_t obj_type, uint32_t obj_size)
{
	int err_code;
	uint8_t send_data[6] = { NRF_DFU_OP_OBJECT_CREATE };

	send_data[1] = obj_type;
	put_uint32_le(send_data + 2, obj_size);
	err_code = dfu_serial_send(p_uart, send_data, sizeof(send_data));

	if (!err_code)
	{
		uint32_t data_cnt;

		err_code = dfu_serial_get_rsp(p_uart, NRF_DFU_OP_OBJECT_CREATE, &data_cnt);
	}

	return err_code;
}

static int dfu_serial_stream_data(uart_drv_t *p_uart, const uint8_t *p_data, uint32_t data_size)
{
	int err_code = 0;
	uint32_t pos, stp, stp_max;

	if (p_data == NULL || !data_size)
	{
		err_code = 1;
	}

	if (!err_code)
	{
		if (mtu >= 5)
		{
			stp_max = (mtu - 1) / 2 - 1;
		}
		else
		{
			logger_error("MTU is too small to send data!");

			err_code = 1;
		}
	}

	for (pos = 0; !err_code && pos < data_size; pos += stp)
	{
		send_data[0] = NRF_DFU_OP_OBJECT_WRITE;
		stp = MIN((data_size - pos), stp_max);
		memcpy(send_data + 1, p_data + pos, stp);
		err_code = dfu_serial_send(p_uart, send_data, stp + 1);
	}

	return err_code;
}

static int dfu_serial_get_crc(uart_drv_t *p_uart, nrf_dfu_response_crc_t *p_crc_rsp)
{
	int err_code;
	uint8_t send_data[1] = { NRF_DFU_OP_CRC_GET };

	err_code = dfu_serial_send(p_uart, send_data, sizeof(send_data));

	if (!err_code)
	{
		uint32_t data_cnt;

		err_code = dfu_serial_get_rsp(p_uart, NRF_DFU_OP_CRC_GET, &data_cnt);

		if (!err_code)
		{
			if (data_cnt == 11)
			{
				p_crc_rsp->offset = get_uint32_le(receive_data + 3);
				p_crc_rsp->crc    = get_uint32_le(receive_data + 7);
			}
			else
			{
				logger_error("Invalid CRC response!");

				err_code = 1;
			}
		}
	}

	return err_code;
}

static int dfu_serial_execute_obj(uart_drv_t *p_uart)
{
	int err_code;
	uint8_t send_data[1] = { NRF_DFU_OP_OBJECT_EXECUTE };

	err_code = dfu_serial_send(p_uart, send_data, sizeof(send_data));

	if (!err_code)
	{
		uint32_t data_cnt;

		err_code = dfu_serial_get_rsp(p_uart, NRF_DFU_OP_OBJECT_EXECUTE, &data_cnt);
	}

	return err_code;
}

static int dfu_serial_stream_data_crc(uart_drv_t *p_uart, const uint8_t *p_data, uint32_t data_size, uint32_t pos, uint32_t *p_crc)
{
	int err_code;
	nrf_dfu_response_crc_t rsp_crc;

	logger_info_2("Streaming Data: len:%u offset:%u crc:0x%08X", data_size, pos, *p_crc);

	err_code = dfu_serial_stream_data(p_uart, p_data, data_size);

	if (!err_code)
	{
		*p_crc = crc32_compute(p_data, data_size, p_crc);

		err_code = dfu_serial_get_crc(p_uart, &rsp_crc);
	}

	if (!err_code)
	{
		if (rsp_crc.offset != pos + data_size)
		{
			logger_error("Invalid offset (%u -> %u)!", pos + data_size, rsp_crc.offset);

			err_code = 2;
		}
		if (rsp_crc.crc != *p_crc)
		{
			logger_error("Invalid CRC (0x%08X -> 0x%08X)!", *p_crc, rsp_crc.crc);

			err_code = 2;
		}
	}

	return err_code;
}

static int dfu_serial_try_to_recover_ip(uart_drv_t *p_uart, const uint8_t *p_data, uint32_t data_size,
										nrf_dfu_response_select_t *p_rsp_recover,
										const nrf_dfu_response_select_t *p_rsp_select)
{
	int err_code = 0;
	uint32_t pos_start, len_remain;
	uint32_t crc_32;

	*p_rsp_recover = *p_rsp_select;

	pos_start = p_rsp_recover->offset;

	if (pos_start > 0 && pos_start <= data_size)
	{
		crc_32 = crc32_compute(p_data, pos_start, NULL);

		if (p_rsp_select->crc != crc_32)
		{
			pos_start = 0;
		}
	}
	else
	{
		pos_start = 0;
	}

	if (pos_start > 0 && pos_start < data_size)
	{
		len_remain = data_size - pos_start;
		err_code = dfu_serial_stream_data_crc(p_uart, p_data + pos_start, len_remain, pos_start, &crc_32);
		if (!err_code)
		{
			pos_start += len_remain;
		}
		else if (err_code == 2)
		{
			// when there is a CRC error, discard previous init packet
			err_code = 0;
			pos_start = 0;
		}
	}

	if (!err_code && pos_start == data_size)
	{
		err_code = dfu_serial_execute_obj(p_uart);
	}

	p_rsp_recover->offset = pos_start;

	return err_code;
}

static int dfu_serial_try_to_recover_fw(uart_drv_t *p_uart, const uint8_t *p_data, uint32_t data_size,
										nrf_dfu_response_select_t *p_rsp_recover,
										const nrf_dfu_response_select_t *p_rsp_select)
{
	int err_code = 0;
	uint32_t max_size, stp_size;
	uint32_t pos_start, len_remain;
	uint32_t crc_32;
	int obj_exec = 1;

	*p_rsp_recover = *p_rsp_select;

	pos_start = p_rsp_recover->offset;

	if (pos_start > data_size)
	{
		logger_error("Invalid firmware offset reported!");

		err_code = 1;
	}
	else if (pos_start > 0)
	{
		max_size = p_rsp_select->max_size;
		crc_32 = crc32_compute(p_data, pos_start, NULL);
		len_remain = pos_start % max_size;

		if (p_rsp_select->crc != crc_32)
		{
			pos_start -= ((len_remain > 0) ? len_remain : max_size);
			p_rsp_recover->offset = pos_start;

			return err_code;
		}

		if (len_remain > 0)
		{
			stp_size = max_size - len_remain;

			err_code = dfu_serial_stream_data_crc(p_uart, p_data + pos_start, stp_size, pos_start, &crc_32);
			if (!err_code)
			{
				pos_start += stp_size;
			}
			else if (err_code == 2)
			{
				err_code = 0;

				pos_start -= len_remain;

				obj_exec = 0;
			}

			p_rsp_recover->offset = pos_start;
		}

		if (!err_code && obj_exec)
		{
			err_code = dfu_serial_execute_obj(p_uart);
		}
	}

	return err_code;
}

int dfu_serial_open(uart_drv_t *p_uart)
{
	int err_code;

	ping_id++;

	err_code = dfu_serial_ping(p_uart, ping_id);

	if (!err_code)
	{
		err_code = dfu_serial_set_prn(p_uart, prn);
	}

	if (!err_code)
	{
		err_code = dfu_serial_get_mtu(p_uart, &mtu);
	}

	return err_code;
}

int dfu_serial_close(uart_drv_t *p_uart)
{
	return 0;
}

int dfu_serial_send_init_packet(uart_drv_t *p_uart, const uint8_t *p_data, uint32_t data_size)
{
	int err_code = 0;
	uint32_t crc_32 = 0;
	nrf_dfu_response_select_t rsp_select;
	nrf_dfu_response_select_t rsp_recover;

	logger_info_1("Sending init packet...");

	if (p_data == NULL || !data_size)
	{
		logger_error("Invalid init packet!");

		err_code = 1;
	}

	if (!err_code)
	{
		err_code = dfu_serial_select_obj(p_uart, 0x01, &rsp_select);
	}

	if (!err_code)
	{
		err_code = dfu_serial_try_to_recover_ip(p_uart, p_data, data_size, &rsp_recover, &rsp_select);

		if (!err_code && rsp_recover.offset == data_size)
			return err_code;
	}

	if (!err_code)
	{
		if (data_size > rsp_select.max_size)
		{
			logger_error("Init packet too big!");

			err_code = 1;
		}
	}

	if (!err_code)
	{
		err_code = dfu_serial_create_obj(p_uart, 0x01, data_size);
	}

	if (!err_code)
	{
		err_code = dfu_serial_stream_data_crc(p_uart, p_data, data_size, 0, &crc_32);
	}

	if (!err_code)
	{
		err_code = dfu_serial_execute_obj(p_uart);
	}

	return err_code;
}

int dfu_serial_send_firmware(uart_drv_t *p_uart, const uint8_t *p_data, uint32_t data_size)
{
	int err_code = 0;
	uint32_t max_size, stp_size, pos;
	uint32_t crc_32 = 0;
	nrf_dfu_response_select_t rsp_select;
	nrf_dfu_response_select_t rsp_recover;
	uint32_t pos_start;

	logger_info_1("Sending firmware file...");

	if (p_data == NULL || !data_size)
	{
		logger_error("Invalid firmware data!");

		err_code = 1;
	}

	if (!err_code)
	{
		err_code = dfu_serial_select_obj(p_uart, 0x02, &rsp_select);
	}

	if (!err_code)
	{
		err_code = dfu_serial_try_to_recover_fw(p_uart, p_data, data_size, &rsp_recover, &rsp_select);
	}

	if (!err_code)
	{
		max_size = rsp_select.max_size;

		pos_start = rsp_recover.offset;
		crc_32 = crc32_compute(p_data, pos_start, &crc_32);

		for (pos = pos_start; pos < data_size; pos += stp_size)
		{
			stp_size = MIN((data_size - pos), max_size);

			err_code = dfu_serial_create_obj(p_uart, 0x02, stp_size);

			if (!err_code)
			{
				err_code = dfu_serial_stream_data_crc(p_uart, p_data + pos, stp_size, pos, &crc_32);
			}

			if (!err_code)
			{
				err_code = dfu_serial_execute_obj(p_uart);
			}

			if (err_code)
				break;
		}
	}

	return err_code;
}
