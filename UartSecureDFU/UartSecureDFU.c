// UartSecureDFU.c : Defines the entry point for the console application.
//

#include <stdio.h>
#include <string.h>
#include "uart_drv.h"
#include "uart_slip.h"
#include "dfu.h"
#include "logging.h"


static int is_argv_verbose(char *p_argv)
{
	int err_code;

	if (!strcmp(p_argv, "-v") || !strcmp(p_argv, "-V"))
		err_code = 0;
	else
		err_code = 1;

	return err_code;
}

int main(int argc, char *argv[])
{
	int err_code = 0;
	int show_usage = 0;
	char *portName = NULL;
	uart_drv_t uart_drv;
	char *zipName = NULL;
	int argn;
	int info_lvl = LOGGER_INFO_LVL_0;

	if (argc >= 2 && strlen(argv[1]) > 0)
		portName = argv[1];
	else
	{
		show_usage = 1;
		err_code = 1;
	}

	if (argc >= 3 && strlen(argv[2]) > 0)
		zipName = argv[2];
	else
	{
		show_usage = 1;
		err_code = 1;
	}

	for (argn = 3; argn < argc && !err_code; argn++)
	{
		err_code = is_argv_verbose(argv[argn]);
		if (!err_code)
		{
			info_lvl++;

			logger_set_info_level(info_lvl);

			if (info_lvl >= LOGGER_INFO_LVL_3)
				break;
		}
		else
		{
			show_usage = 1;
			err_code = 1;

			break;
		}
	}

	if (show_usage)
	{
		printf("Usage: UartSecureDFU serial_port package_name [-v] [-v] [-v]\n");
	}

	uart_drv.p_PortName = portName;

	if (!err_code)
	{
		err_code = uart_slip_open(&uart_drv);
	}

	if (!err_code)
	{
		dfu_param_t dfu_param;

		dfu_param.p_uart = &uart_drv;
		dfu_param.p_pkg_file = zipName;
		err_code = dfu_send_package(&dfu_param);
	}

	if (!show_usage)
	{
		int err_code2 = uart_slip_close(&uart_drv);
		
		if (!err_code)
			err_code = err_code2;
	}

	return err_code;
}
