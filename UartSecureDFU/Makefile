CC = gcc
CFLAGS = -Wall -O2 -I.
BIN = UartSecureDFU

DEPS = crc32.h \
       delay_connect.h \
       dfu.h \
       dfu_serial.h \
       logging.h \
       slip_enc.h \
       uart_drv.h \
       uart_slip.h \
       zip.h \
       miniz.h \
       jsmn.h \
       Makefile

OBJS = crc32.o \
       delay_connect.o \
       dfu.o \
       dfu_serial.o \
       jsmn.o \
       logging.o \
       slip_enc.o \
       uart_linux.o \
       UartSecureDFU.o \
       uart_slip.o \
       zip.o

%.o: %.c $(DEPS)
	$(CC) $(CFLAGS) -c $< -o $@

$(BIN): $(OBJS)
	$(CC) $(OBJS) -o $(BIN)

clean: 
	rm -f $(BIN) $(OBJS)
