CC=gcc
CFLAGS=-Wall -Wno-format-security -Wno-unused-variable -Wno-unused-function

# Выбор интерфейса передачи осуществляется макроопределением
#DEFINES=-DUART_INTERFACE=1
#DEFINES=-DCAN_INTERFACE=1

EXECNAME=driver.exe

# PATHS
SRC=./
INC=-I./

SRC_FILES=	$(SRC)crc.c \
			$(SRC)driver.c \
			$(SRC)main.c \


.PHONY : clean install run

all: clean
	@LC_ALL=C $(CC) $(CFLAGS) -O0 $(DEFINES) \
	$(SRC_FILES) \
	-o $(EXECNAME) \
	$(INC) \
	#$(LLIBS) 

debug: clean
	@LC_ALL=C $(CC) $(CFLAGS) -O3 -g $(DEFINES) \
	$(SRC_FILES) \
	-o $(EXECNAME) \
	$(INC) \
	#$(LLIBS) 

run:
	@./$(EXECNAME)

clean:
	@rm -f ./$(EXECNAME)
