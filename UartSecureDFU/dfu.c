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
#include <stdlib.h>
#include <string.h>
#include "dfu.h"
#include "dfu_serial.h"
#include "delay_connect.h"
#include "logging.h"
#include "zip.h"
#include "jsmn.h"

// maximum number of JSON tokens to process
#define JSON_TOKEN_NUM_MAX              30

// maximum number of DFU objects to process
#define DFU_OBJECT_NUM_MAX              3

typedef enum {
	DFU_IMG_NIL = 0,                    //!< DFU image invalid
	DFU_IMG_APP = 1,                    //!< DFU application image
	DFU_IMG_BL = 2,                     //!< DFU bootloader image
	DFU_IMG_SD = 3,                     //!< DFU SoftDevice image
	DFU_IMG_SD_BL = 4                   //!< DFU SoftDevice & bootloader image
} dfu_image_type_t;

typedef enum {
	STR_UNDEFINED = 0,
	STR_FILE_BIN = 1,
	STR_FILE_DAT = 2
} dfu_json_str_t;

typedef struct {
	jsmntype_t type;                    //!< Token type.
	int size;                           //!< Number of child tokens.
	char *str;                          //!< String value.
	dfu_json_str_t str_type;            //!< String usage type.
} jsmn_entity_t;

typedef struct {
	dfu_image_type_t img_type;          //!< DFU image type.
	const jsmn_entity_t *p_pattern;     //!< JSMN token pattern.
} dfu_image_jsmn_pattern;

typedef struct
{
	dfu_image_type_t img_type;          //!< DFU image type.

	char *file_bin;                     //!< BIN file name.
	char *file_dat;                     //!< DAT file name.
} dfu_json_object_t;

typedef struct
{
	uart_drv_t *p_uart;

	uint8_t *p_img_dat;                 //!< Image DAT pointer.
	uint32_t n_dat_size;                //!< Image DAT size.
	uint8_t *p_img_bin;                 //!< Image BIN pointer.
	uint32_t n_bin_size;                //!< Image BIN size.
} dfu_img_param_t;

// JSON tokens
static jsmntok_t json_tokens[JSON_TOKEN_NUM_MAX];

// DFU objects
static dfu_json_object_t dfu_objects[DFU_OBJECT_NUM_MAX];

// JSMN token pattern for Manifest
static const jsmn_entity_t dfu_mft_pattern[] =
{
	{ JSMN_OBJECT,    1, NULL,       STR_UNDEFINED },
	{ JSMN_STRING,    1, "manifest", STR_UNDEFINED },
	{ JSMN_UNDEFINED, 0, NULL,       STR_UNDEFINED }
};

// JSMN token pattern for 1 DFU image
static const jsmn_entity_t dfu_img_1_pattern[] =
{
	{ JSMN_OBJECT,    1, NULL,       STR_UNDEFINED },
	{ JSMN_UNDEFINED, 0, NULL,       STR_UNDEFINED }
};

// JSMN token pattern for 2 DFU images
static const jsmn_entity_t dfu_img_2_pattern[] =
{
	{ JSMN_OBJECT,    2, NULL,       STR_UNDEFINED },
	{ JSMN_UNDEFINED, 0, NULL,       STR_UNDEFINED }
};

// JSMN token pattern for DFU application
static const jsmn_entity_t dfu_app_pattern[] =
{
	{ JSMN_STRING,    1, "application", STR_UNDEFINED },
	{ JSMN_OBJECT,    2, NULL,          STR_UNDEFINED },
	{ JSMN_STRING,    1, "bin_file",    STR_UNDEFINED },
	{ JSMN_STRING,    0, NULL,          STR_FILE_BIN  },
	{ JSMN_STRING,    1, "dat_file",    STR_UNDEFINED },
	{ JSMN_STRING,    0, NULL,          STR_FILE_DAT  },
	{ JSMN_UNDEFINED, 0, NULL,          STR_UNDEFINED }
};

// JSMN token pattern for DFU bootloader
static const jsmn_entity_t dfu_bl_pattern[] =
{
	{ JSMN_STRING,    1, "bootloader",  STR_UNDEFINED },
	{ JSMN_OBJECT,    2, NULL,          STR_UNDEFINED },
	{ JSMN_STRING,    1, "bin_file",    STR_UNDEFINED },
	{ JSMN_STRING,    0, NULL,          STR_FILE_BIN  },
	{ JSMN_STRING,    1, "dat_file",    STR_UNDEFINED },
	{ JSMN_STRING,    0, NULL,          STR_FILE_DAT  },
	{ JSMN_UNDEFINED, 0, NULL,          STR_UNDEFINED }
};

// JSMN token pattern for DFU SoftDevice
static const jsmn_entity_t dfu_sd_pattern[] =
{
	{ JSMN_STRING,    1, "softdevice",  STR_UNDEFINED },
	{ JSMN_OBJECT,    2, NULL,          STR_UNDEFINED },
	{ JSMN_STRING,    1, "bin_file",    STR_UNDEFINED },
	{ JSMN_STRING,    0, NULL,          STR_FILE_BIN  },
	{ JSMN_STRING,    1, "dat_file",    STR_UNDEFINED },
	{ JSMN_STRING,    0, NULL,          STR_FILE_DAT  },
	{ JSMN_UNDEFINED, 0, NULL,          STR_UNDEFINED }
};

// JSMN token pattern for DFU SoftDevice & bootloader
static const jsmn_entity_t dfu_sd_bl_pattern[] =
{
	{ JSMN_STRING,    1, "softdevice_bootloader",   STR_UNDEFINED },
	{ JSMN_OBJECT,    3, NULL,                      STR_UNDEFINED },
	{ JSMN_STRING,    1, "bin_file",                STR_UNDEFINED },
	{ JSMN_STRING,    0, NULL,                      STR_FILE_BIN  },
	{ JSMN_STRING,    1, "dat_file",                STR_UNDEFINED },
	{ JSMN_STRING,    0, NULL,                      STR_FILE_DAT  },
	{ JSMN_STRING,    1, "info_read_only_metadata", STR_UNDEFINED },
	{ JSMN_OBJECT,    2, NULL,                      STR_UNDEFINED },
	{ JSMN_STRING,    1, "bl_size",                 STR_UNDEFINED },
	{ JSMN_PRIMITIVE, 0, NULL,                      STR_UNDEFINED },
	{ JSMN_STRING,    1, "sd_size",                 STR_UNDEFINED },
	{ JSMN_PRIMITIVE, 0, NULL,                      STR_UNDEFINED },
	{ JSMN_UNDEFINED, 0, NULL,                      STR_UNDEFINED }
};

// JSMN token pattern table
static const dfu_image_jsmn_pattern dfu_pattern_tbl[] =
{
	{ DFU_IMG_APP,   dfu_app_pattern   },
	{ DFU_IMG_BL,    dfu_bl_pattern    },
	{ DFU_IMG_SD,    dfu_sd_pattern    },
	{ DFU_IMG_SD_BL, dfu_sd_bl_pattern },
	{ DFU_IMG_NIL,   NULL              }
};

static void free_dfu_json_obj(dfu_json_object_t *p_dfu_obj);

// allocate memory to store a JSON string
static char *put_json_to_string(const uint8_t *p_data, int len)
{
	char *p_str;

	if (len >= 0)
		p_str = (char *)malloc(len + 1);
	else
		p_str = NULL;

	if (p_str != NULL)
	{
		memcpy(p_str, p_data, len);
		*(p_str + len) = '\0';
	}

	return p_str;
}

// map a JSMN token to a DFU JSON pattern
static int map_jsmn_token_to_pattern(dfu_json_object_t *p_dfu_obj, const jsmn_entity_t *p_pattern, const jsmntok_t *p_tokens, int num_tokens, const uint8_t *p_data)
{
	int i, j = 0;
	jsmntype_t jsmn_type;

	// compare the JSMN tokens one by one
	for (i = 0; (jsmn_type = (p_pattern + i)->type) != JSMN_UNDEFINED; i++)
	{
		if (i >= num_tokens || 
			jsmn_type != (p_tokens + i)->type || 
			(p_pattern + i)->size != (p_tokens + i)->size)
		{
			// type or size mismatch...
			j = -1;
			break;
		}

		if (jsmn_type == JSMN_STRING)
		{
			int len = (p_tokens + i)->end - (p_tokens + i)->start;
			char *p_str = put_json_to_string(p_data + (p_tokens + i)->start, len);

			if (p_str == NULL)
			{
				// cannot allocate memory...
				j = -1;
				break;
			}

			// check the pattern string
			if ((p_pattern + i)->str != NULL && strcmp(p_str, (p_pattern + i)->str))
			{
				free(p_str);

				// mismatch...
				j = -1;
				break;
			}

			// store string target, if any
			switch ((p_pattern + i)->str_type)
			{
			case STR_FILE_DAT:
				if (p_dfu_obj != NULL)
					p_dfu_obj->file_dat = p_str;
				break;
			case STR_FILE_BIN:
				if (p_dfu_obj != NULL)
					p_dfu_obj->file_bin = p_str;
				break;
			default:
				free(p_str);
				break;
			}
		}
	}

	if (j < 0)
	{
		i = j;

		if (p_dfu_obj != NULL)
			free_dfu_json_obj(p_dfu_obj);
	}

	return i;
}

// free allocated strings
static void free_dfu_json_obj(dfu_json_object_t *p_dfu_obj)
{
	if (p_dfu_obj->file_dat != NULL)
	{
		free(p_dfu_obj->file_dat);
		p_dfu_obj->file_dat = NULL;
	}
	if (p_dfu_obj->file_bin != NULL)
	{
		free(p_dfu_obj->file_bin);
		p_dfu_obj->file_bin = NULL;
	}
}

static int dfu_send_image(dfu_img_param_t *p_dfu_img)
{
	int err_code;

	err_code = dfu_serial_open(p_dfu_img->p_uart);

	if (!err_code)
	{
		err_code = dfu_serial_send_init_packet(p_dfu_img->p_uart, p_dfu_img->p_img_dat, p_dfu_img->n_dat_size);
	}

	if (!err_code)
	{
		err_code = dfu_serial_send_firmware(p_dfu_img->p_uart, p_dfu_img->p_img_bin, p_dfu_img->n_bin_size);
	}

	if (!err_code)
	{
		err_code = dfu_serial_close(p_dfu_img->p_uart);
	}

	return err_code;
}

static int dfu_send_object(uart_drv_t *p_uart, dfu_json_object_t *p_dfu_obj, struct zip_t *p_zip_pkg)
{
	int err_code = 0;
	uint8_t *buf_dat = NULL;
	size_t buf_dat_size;
	uint8_t *buf_bin = NULL;
	size_t buf_bin_size;
	dfu_img_param_t dfu_img;

	if (zip_entry_open(p_zip_pkg, p_dfu_obj->file_dat))
	{
		logger_error("Cannot open package DAT file!");

		err_code = 1;
	}
	else
	{
		if (zip_entry_read(p_zip_pkg, (void **)&buf_dat, &buf_dat_size))
		{
			logger_error("Cannot read package DAT file!");

			err_code = 1;
		}

		zip_entry_close(p_zip_pkg);
	}

	if (!err_code)
	{
		if (zip_entry_open(p_zip_pkg, p_dfu_obj->file_bin))
		{
			logger_error("Cannot open package BIN file!");

			err_code = 1;
		}
		else
		{
			if (zip_entry_read(p_zip_pkg, (void **)&buf_bin, &buf_bin_size))
			{
				logger_error("Cannot read package BIN file!");

				err_code = 1;
			}

			zip_entry_close(p_zip_pkg);
		}
	}

	if (!err_code)
	{
		dfu_img.p_uart = p_uart;
		dfu_img.p_img_dat = buf_dat;
		dfu_img.n_dat_size = buf_dat_size;
		dfu_img.p_img_bin = buf_bin;
		dfu_img.n_bin_size = buf_bin_size;
		err_code = dfu_send_image(&dfu_img);
	}

	if (buf_dat != NULL)
		free(buf_dat);

	if (buf_bin != NULL)
		free(buf_bin);

	return err_code;
}

static dfu_json_object_t *find_dfu_object(dfu_json_object_t *p_dfu_obj, int num_obj, dfu_image_type_t img_type)
{
	dfu_json_object_t *p_obj = NULL;
	int i;

	for (i = 0; i < num_obj; i++)
	{
		if ((p_dfu_obj + i)->img_type == img_type)
		{
			p_obj = p_dfu_obj + i;
			break;
		}
	}

	return p_obj;
}

int dfu_send_package(dfu_param_t *p_dfu)
{
	int err_code = 0;
	struct zip_t *zip_pkg;
	uint8_t *buf_json = NULL;
	size_t bufsize;
	jsmn_parser parser;
	int num_tokens;
	int num_images, img_n = 0;
	dfu_json_object_t *p_dfu_object;
	int i, n;

	zip_pkg = zip_open(p_dfu->p_pkg_file, 0, 'r');
	if (zip_pkg == NULL)
	{
		logger_error("Cannot open ZIP package file!");

		err_code = 1;
	}
	else
	{
		if (zip_entry_open(zip_pkg, "manifest.json"))
		{
			logger_error("Cannot open package manifest file!");

			err_code = 1;
		}
		else
		{
			if (zip_entry_read(zip_pkg, (void **)&buf_json, &bufsize))
			{
				logger_error("Cannot read package manifest file!");

				err_code = 1;
			}
			else
			{
				zip_entry_close(zip_pkg);
			}
		}
	}

	if (!err_code)
	{
		jsmn_init(&parser);

		num_tokens = jsmn_parse(&parser, (char *)buf_json, bufsize, json_tokens, JSON_TOKEN_NUM_MAX);

		if (num_tokens < 0)
		{
			logger_error("Cannot parse package manifest json (%d)!", num_tokens);

			err_code = 1;
		}
	}

	for (i = 0; i < DFU_OBJECT_NUM_MAX; i++)
	{
		dfu_objects[i].file_bin = NULL;
		dfu_objects[i].file_dat = NULL;
	}

	if (!err_code)
	{
		// check that JSON starts with a manifest object
		i = map_jsmn_token_to_pattern(NULL, dfu_mft_pattern, json_tokens, num_tokens, buf_json);
		if (i < 0)
		{
			logger_error("Cannot get json manifest object!");

			err_code = 1;
		}
		n = i;

		if (!err_code)
		{
			// check whether there are 1 or 2 DFU images
			i = map_jsmn_token_to_pattern(NULL, dfu_img_1_pattern, json_tokens + n, num_tokens - n, buf_json);
			if (i > 0)
			{
				num_images = 1;
			}
			else
			{
				i = map_jsmn_token_to_pattern(NULL, dfu_img_2_pattern, json_tokens + n, num_tokens - n, buf_json);
				if (i > 0)
				{
					num_images = 2;
				}
			}

			if (i <= 0)
			{
				logger_error("Cannot get json number of DFU images!");

				err_code = 1;
			}
		}
		n += i;

		while (!err_code && n < num_tokens)
		{
			int t;

			if (img_n >= DFU_OBJECT_NUM_MAX)
			{
				logger_error("No data storage for json DFU image object!");

				err_code = 1;
				break;
			}

			// determine the DFU image type
			for (t = 0; dfu_pattern_tbl[t].img_type != DFU_IMG_NIL; t++)
			{
				i = map_jsmn_token_to_pattern(dfu_objects + img_n, dfu_pattern_tbl[t].p_pattern, json_tokens + n, num_tokens - n, buf_json);

				if (i > 0)
				{
					dfu_objects[img_n].img_type = dfu_pattern_tbl[t].img_type;
					break;
				}
			}

			if (i <= 0)
			{
				logger_error("Cannot find json DFU image object!");

				err_code = 1;
				break;
			}
			else
			{
				n += i;

				img_n++;
			}
		}

		if (!err_code && (n != num_tokens || img_n != num_images))
		{
			logger_error("Incoherent json object structure detected!");

			err_code = 1;
		}
	}

	if (!err_code)
	{
		// send SoftDevice & bootloader image, if any
		p_dfu_object = find_dfu_object(dfu_objects, num_images, DFU_IMG_SD_BL);
		if (p_dfu_object != NULL)
		{
			logger_info_1("Sending SoftDevice+Bootloader image.");

			err_code = dfu_send_object(p_dfu->p_uart, p_dfu_object, zip_pkg);

			if (!err_code && num_images > 1)
				err_code = delay_connect();
		}
	}

	if (!err_code)
	{
		// send SoftDevice image, if any
		p_dfu_object = find_dfu_object(dfu_objects, num_images, DFU_IMG_SD);
		if (p_dfu_object != NULL)
		{
			logger_info_1("Sending SoftDevice image.");

			err_code = dfu_send_object(p_dfu->p_uart, p_dfu_object, zip_pkg);

			if (!err_code && num_images > 1)
				err_code = delay_connect();
		}
	}

	if (!err_code)
	{
		// send bootloader image, if any
		p_dfu_object = find_dfu_object(dfu_objects, num_images, DFU_IMG_BL);
		if (p_dfu_object != NULL)
		{
			logger_info_1("Sending Bootloader image.");

			err_code = dfu_send_object(p_dfu->p_uart, p_dfu_object, zip_pkg);

			if (!err_code && num_images > 1)
				err_code = delay_connect();
		}
	}

	if (!err_code)
	{
		// send application image, if any
		p_dfu_object = find_dfu_object(dfu_objects, num_images, DFU_IMG_APP);
		if (p_dfu_object != NULL)
		{
			logger_info_1("Sending Application image.");

			err_code = dfu_send_object(p_dfu->p_uart, p_dfu_object, zip_pkg);
		}
	}

	for (i = 0; i < DFU_OBJECT_NUM_MAX; i++)
		free_dfu_json_obj(dfu_objects + i);

	if (buf_json != NULL)
		free(buf_json);

	if (zip_pkg != NULL)
		zip_close(zip_pkg);

	return err_code;
}
