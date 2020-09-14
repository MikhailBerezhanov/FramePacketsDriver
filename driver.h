
#ifndef __DRIVER_H__
#define __DRIVER_H__

#include <stdint.h>

#define PACKET_START				0xFB 		// Признак начала пакета
#define PACKET_END					0xFE 		// Признак окончания пакета

#define MAX_PACKET_LEN			256			// Максимальное Кол-во байт данных в одном пакете
																		// Примечание: при увеличении выше 256 потребуется
																		// расширирение формата стопового фрейма. 256 выбрано для примера
#define MAX_PACKET_NUM			2				// Кол-во буферов для сборки пакетов

// Коды ошибок драйвера
typedef enum
{
	OK = 0,
	SEND_ERR,
	NO_DATA,
	NO_FREE_PBR,
	IN_PROGRESS,
	INVALID_ADDR,
	INVALID_FORMAT,
	INVALID_CRC,
}driver_err_t;

// Прототип функции работы с данными для инициализации драйвера
typedef int (*tx_rx_func_t)(uint8_t* data, uint16_t data_len);

/* ПРИМЕЧАНИЯ:
		1. Формат фрейма пакета выбран исходя из совместимости с форматом CAN-кадра
		2. Т.к. шина CAN в общем случаем подразумевает несколько устройств на линии,
			 в отличие от UART, в идентификатор фрейма пакета включены адрес источника 
			 и адрес назначения для возможности программной фильтрации поступающих данных.
*/

// Структура идентификатора протокола
typedef struct 
{
		uint8_t	 PM;									// параметр сообщения
		unsigned SA:		7;						// адрес отправителя сообщения
		unsigned DA:		7;						// адрес назначения сообщения
		unsigned FN:		7;						// функция сообщения
		// Идентификатор занимает 29 бит (расширенный формат CAN-кадра)
		unsigned unused:	3;					// неиспользуемые	
}__attribute__((__packed__)) protocol_id_t;

// Заголовок фрейма протокола
typedef struct
{
		unsigned data_num:	4;					// количество данных в фрейме
		unsigned unused:	2;						// резерв		
		unsigned req:		1;							// признак запроса
		unsigned ext:		1;							// тип id фрейма (1 - расширенный(29 бит)/0 - стандартный(11 бит))
		protocol_id_t id;								// идентификатор фрейма
}__attribute__((__packed__)) protocol_frame_header_t;

#define FRAME_DATA_LEN			8				// Для совместимости с форматом CAN-кадра 0-8 байт

// Структура фрейма протокола
typedef struct 
{
	protocol_frame_header_t header;		// заголовок фрейма		
	uint8_t	data[FRAME_DATA_LEN];			// данные фрейма
}__attribute__((__packed__)) protocol_frame_t;

// Структура пакета протокола
typedef struct
{
	protocol_id_t id;										// идентификатор пакета
	uint8_t	data[MAX_PACKET_LEN];				// данные пакета
}__attribute__((__packed__)) protocol_packet_t;


// Структура буфера приема пакетов (Packet Receive Buffer)
typedef struct
{
	struct
	{
		unsigned start:	1;								// признак начала сбора пакета
		unsigned unused: 7;								// резерв
	}__attribute__((__packed__)) flags;
	uint16_t	packet_data_len;					// количество принятых данных пакета
	protocol_packet_t	packet;						// пакет
}__attribute__((__packed__)) protocol_PRB_t;


// Инициализация драйвера
driver_err_t driver_init(tx_rx_func_t send_func, tx_rx_func_t get_func);

// Преобразование кода ошибки в строку с текстом ошибки
char* driver_err_text(driver_err_t err_code);

// Формирование и отправка пакета
driver_err_t driver_send_packet (protocol_id_t* packet_id, uint8_t* data, uint16_t data_len);

// Получение пакета данных (сборка из фреймов)
driver_err_t driver_get_packet (uint8_t my_addr, protocol_packet_t *packet);


#endif
