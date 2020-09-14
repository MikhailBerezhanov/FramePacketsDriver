#include <stdio.h> 
#include <string.h>

#include "driver.h"

#include "fifo.h"	// для отладочных функций отправки и приема 

FIFO_CREATE(dbg_buf, uint8_t, 4*MAX_PACKET_LEN);	// Буфер для хранения данных пакета для демонстрации

// Отладочная функция передачи
int dbg_put_data(uint8_t* data, uint16_t data_len)
{
	printf("Отправка '%u' байт:\t", data_len);

	for(int i = 0; i < data_len; i++){
		FIFO_PUSH(dbg_buf, data[i]);
		printf("%02X ", data[i]);
	}

	puts("\r");
	return 0;
}

// Отладочная функция приема
int dbg_get_data(uint8_t* data, uint16_t data_len)
{
	printf("Получение '%u' байт:\t", data_len);

	if(FIFO_AVAIL_COUNT(dbg_buf) < data_len){
		printf("Ошибка. Доступно '%u' байт:\t", FIFO_AVAIL_COUNT(dbg_buf));
		return -1;
	}

	for(int i = 0; i < data_len; i++){
		FIFO_POP(dbg_buf, data[i])
		printf("%02X ", data[i]);
	}

	puts("\r");

	return 0;
}

#define TEST_DATA_LEN	50		// В текущей версии поддерживается максимум 256


int main(int argc, char* argv[]) 
{ 
	driver_err_t ret = IN_PROGRESS;
	protocol_id_t tx_packet_id;
	protocol_packet_t rx_packet;
	uint8_t msg_data[TEST_DATA_LEN];

	memset(&rx_packet, 0, sizeof(rx_packet));

	// Тестовый набор данных
	for(uint16_t i = 0; i < TEST_DATA_LEN; i++){
		msg_data[i] = (uint8_t)(0x00FF & i);
	}

	memset(&tx_packet_id, 0, sizeof(tx_packet_id));
	tx_packet_id.FN = 0x43;
	tx_packet_id.PM = 0x12;
	tx_packet_id.SA = 0x01;
	tx_packet_id.DA = 0x02;

	// Для демонстрации используются отладочные функции приема и передачи данных
	driver_init(dbg_put_data, dbg_get_data);

	driver_send_packet(&tx_packet_id, msg_data, sizeof(msg_data));

	for(;;)
	{
		// Основной цикл МК или потока

		if( (ret = driver_get_packet(tx_packet_id.DA, &rx_packet)) != IN_PROGRESS)
		{
			if(ret == OK)
			{

				// Обрабока полученной информации

				// Для выхода из теста
				break;
			}

			// Обработка возникших ошибок сборки пакета
			else 
			{
				printf("Ошибка сборки пакета: %s\r\n", driver_err_text(ret));
				break;
			}
		}
	}

	return 0;
}
