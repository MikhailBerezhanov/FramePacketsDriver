#include <stdio.h> 
#include <string.h>
#include <stdbool.h>

#include "crc.h"
#include "driver.h"

static protocol_PRB_t	PRB[MAX_PACKET_NUM];			// Буфер приема пакетов

// Для демонстрации используются кастомные функции отправки и передачи
static tx_rx_func_t custom_send_data = NULL;
static tx_rx_func_t custom_get_data = NULL;

// Отправка байтов по выбранному интерфейсу 
int driver_send_data(uint8_t* data, uint16_t data_len)
{
	#if (UART_INTERFACE)
		return uart_send_data(data, data_len);
	#elif (CAN_INTERFACE) 
		return can_send_data(data, data_len);
	#else
		return custom_send_data(data, data_len);
	#endif
}

// Прием байтов по выбранному интерфейсу 
// Прим. Предполагается что механизм приема сохраняет приходящие
// байты в fifo-буфере и чтение возможно порциями размером в data_len
int driver_get_data(uint8_t* data, uint16_t data_len)
{
	#if (UART_INTERFACE)
		return uart_get_data(data, data_len);
	#elif (CAN_INTERFACE) 
		return can_get_data(data, data_len);
	#else
		return custom_get_data(data, data_len);
	#endif
}

// Функции синхронизации доступа к общим данным (буферу приема пакетов)
int driver_mutex_lock(void)
{
	// Имплементация зависит от выбранной RTOS
	return 0;
}

int driver_mutex_unlock(void)
{
	// Имплементация зависит от выбранной RTOS
	return 0;
}

// Инициализация драйвера
driver_err_t driver_init(tx_rx_func_t send_func, tx_rx_func_t get_func)
{
	if(send_func && get_func) 
	{
		custom_send_data = send_func;
		custom_get_data = get_func;
		return OK;
	}

	return NO_DATA;
}

// Преобразование кода ошибки в строку с текстом ошибки
char* driver_err_text(driver_err_t err_code)
{
	switch(err_code)
	{
		case OK: return "Ок";
		case SEND_ERR: return "Ошибка отправки данных";
		case NO_DATA: return "Нет принятых данных";
		case NO_FREE_PBR: return "Нет свободных буферов сборки пакета";
		case IN_PROGRESS: return "Сборка пакета в процессе";
		case INVALID_ADDR: return "Неверный адрес назначения";
		case INVALID_FORMAT: return "Неверный формат пакета";
		case INVALID_CRC: return "Неверная контрольная сумма пакета";
		default: return "Неизвестная ошибка";
	}
}

/**
  * @описание  Формирование и отправка пакета 
  * @параметры
  *     Входные:
  *         *packet_id - указатель на идентификатор пакета
  *         *data - указатель на данные пакета (поток байтов)
  *					data_len - длина передаваемых данных в байтах
  * @возвращает результат выполнения
  *			OK							- Успех
  *			SEND_ERR				- Ошибка отправки
 */
driver_err_t driver_send_packet(protocol_id_t* packet_id, uint8_t* data, uint16_t data_len)
{
	protocol_frame_t tx_frame;								// фрейм для отправки
	memset(&tx_frame, 0, sizeof(tx_frame));
	uint8_t	frame_number = 0;									// номер фрейма
	uint16_t tx_len = sizeof(protocol_frame_header_t);
	uint8_t	packet_PM = packet_id->PM;				// сохраняем исходный параметр пакета
	uint8_t	packet_crc = 0;										// контрольная сумма пакета

	printf("\r\nОтправка пакета (");
	printf("FN: 0x%02X DA: 0x%02X SA: 0x%02X PM: 0x%02X data_len:%u)\r\n\n",
		packet_id->FN, packet_id->DA, packet_id->SA, packet_id->PM, data_len);

	// Отправка начала пакета
	tx_frame.header.id.FN = packet_id->FN;
	tx_frame.header.id.DA = packet_id->DA;
	tx_frame.header.id.SA = packet_id->SA;
	tx_frame.header.id.PM = PACKET_START;
	tx_frame.header.data_num = 0;
	if ( driver_send_data((uint8_t*)&tx_frame, tx_len) ) return SEND_ERR;

	// Отправка данных пакета
	while (data_len--)
	{
		// Формирование данных очередного сообщения
		tx_frame.data[tx_frame.header.data_num++] = *data++;
		// Проверка окончания формирования сообщения
		if ( !data_len || (tx_frame.header.data_num == 8) )
		{
			// Подсчет контрольной суммы данных
			packet_crc = CalcArrayCRC8(packet_crc, tx_frame.data, tx_frame.header.data_num);

			// Установка номера сообщения пакета
			tx_frame.header.id.PM = frame_number++;

			// Отправка очередного пакета с данными
			tx_len = sizeof(protocol_frame_t) - (FRAME_DATA_LEN - tx_frame.header.data_num);
			if ( driver_send_data((uint8_t*)&tx_frame, tx_len) ) return SEND_ERR;

			// Сброс размера данных для следующего сообщения
			tx_frame.header.data_num = 0;
		}
	}
	// Отправка окончания пакета
	tx_frame.header.id.PM = PACKET_END;
	tx_frame.header.data_num = 0;
	tx_frame.data[tx_frame.header.data_num++] = frame_number;
	tx_frame.data[tx_frame.header.data_num++] = packet_crc;
	tx_frame.data[tx_frame.header.data_num++] = packet_PM;
	tx_len = sizeof(protocol_frame_t) - (FRAME_DATA_LEN - tx_frame.header.data_num);
	if ( driver_send_data((uint8_t*)&tx_frame, tx_len) ) return SEND_ERR;

	return OK;
}


// Проверка сборки текущего пакета возвращает
// Возвращает индекс на свободный буфер или на текущий буфер с принятой функцией
static bool driver_check_PRB (uint8_t* PRB_index, protocol_frame_t* frame)
{
	uint8_t index;
	// проверяем не идет ли сейчас сбор по принятой фукции
	driver_mutex_lock();
	for (index = 0; index < MAX_PACKET_NUM; index++)
	{
		// если идет сбор пакета
		if(PRB[index].flags.start)
		{
			// если принимаемая функция соответ текущему сбору
			if(PRB[index].packet.id.FN == frame->header.id.FN)
			{
				*PRB_index = index;
				driver_mutex_unlock();
				return(true);
			}
		}
	}

	// если текущего сбора по данной функции нет ищем первый свободный буфер
	for (index = 0; index < MAX_PACKET_NUM; index++)
	{
		// если идет сбор пакета
		if(!PRB[index].flags.start)
		{
			*PRB_index = index;
			driver_mutex_unlock();
			return(true);
		}
	}
	
	driver_mutex_unlock();
	return false;
}


// Сборка пакета из фреймов в буфере PRB
static driver_err_t driver_assemble_packet (protocol_PRB_t* pPRB, protocol_frame_t* frame)
{
	unsigned char	rx_frames_num;		// количество принятых фреймов с данными
	unsigned char	packet_crc;				// контрольная сумма данных принятого пакета

	printf("Обработка фрейма (FN: %02X DA: %02X SA: %02X PM: %02X)\r\n", 
		frame->header.id.FN, frame->header.id.DA, frame->header.id.SA, frame->header.id.PM);

	// Проверка признака фрейма
	switch (frame->header.id.PM)
	{
		case PACKET_START:
			printf("\r\nНачало приема пакета:\r\n");
			driver_mutex_lock();
			// Запуск приема пакета от устройства
			pPRB->flags.start = 1;
			pPRB->packet_data_len = 0;

			// Очистка буфера перед приемом данных
			memset((uint8_t*)&pPRB->packet, 0, sizeof(protocol_packet_t));
			// Заполнение заголовка принимаемого пакета
			pPRB->packet.id.FN = frame->header.id.FN;
			pPRB->packet.id.SA = frame->header.id.SA;
			pPRB->packet.id.DA = frame->header.id.DA;
			pPRB->packet.id.PM = 0;
			driver_mutex_unlock();
		break;

		case PACKET_END:
			driver_mutex_lock();
			pPRB->flags.start = 0;
			// Вычисление количества принятых фреймов с данными
			rx_frames_num = (pPRB->packet_data_len & 0x07) ? ((pPRB->packet_data_len >> 3) + 1) : (pPRB->packet_data_len >> 3);
			packet_crc = CalcArrayCRC8(0, pPRB->packet.data, pPRB->packet_data_len);

			// Проверка правильности принятого пакета
			if( rx_frames_num != frame->data[0] ){
				printf("Ошибка приема. Отправлено кадров: %u принято кадров: %u\r\n", frame->data[0], rx_frames_num);
				driver_mutex_unlock();
				return INVALID_FORMAT;
			}
			if ( packet_crc != frame->data[1] ){
				printf("Ошибка контрольной суммы пакета\r\n");
				printf("CRC отправленного пакета: 0x%02X CRC рассчитанное: 0x%02X\r\n", frame->data[1], packet_crc);
				driver_mutex_unlock();
				return INVALID_CRC;
			}
			// Извлечение параметра пакета
			pPRB->packet.id.PM = frame->data[2];

			// Пакет собран успешно
			printf("Пакет собран успешно (FN: 0x%02X SA: 0x%02X DA: 0x%02X PM:0x%02X кол-во фреймов: %u)\r\n",
				pPRB->packet.id.FN, pPRB->packet.id.SA, pPRB->packet.id.DA, pPRB->packet.id.PM, rx_frames_num);
			printf("Данные пакета:\r\n");
			for (int i = 0; i < pPRB->packet_data_len; i++){
				if ( (i != 0) && (i % 8 == 0) ) printf("  ");
				if ( (i != 0) && (i % 16 == 0)) printf("\r\n");
				printf("%02X ", pPRB->packet.data[i]);
			}
			printf("\r\n");

			driver_mutex_unlock();
			return OK;

		default:
			// Проверка сбора пакета (фрейм из текущего пакета)
			driver_mutex_lock();
			if (pPRB->flags.start && (pPRB->packet.id.FN == frame->header.id.FN))
			{
				// Проверка количества принятых данных пакета
				if (pPRB->packet_data_len >= sizeof(pPRB->packet.data))
				{
					pPRB->flags.start = 0;
					printf("Превышен максимально-допустимый размер пакета.\r\n");
					printf("Принято %u байт данных, Максимум: %zu\r\n", pPRB->packet_data_len, sizeof(pPRB->packet.data));
					driver_mutex_unlock();
					return INVALID_FORMAT;
				}
				// Сохранение фрейма в буфере
				memcpy(&pPRB->packet.data[pPRB->packet_data_len], frame->data, frame->header.data_num);
				pPRB->packet_data_len += frame->header.data_num;
			}
			driver_mutex_unlock();
	}
	// Идет сбор пакета
	return IN_PROGRESS;
}


/**
  * @описание  Получение пакета данных (сборка из фреймов)
  * @справка   Принадлежность фрейма к пакету определяется по полю функции FN в заголовке
  *						 Параллельно могут собираться несколько разных пакетов:
  *						 кол-во задается макросом MAX_PACKET_NUM
  * @параметры
  *     Входные:
  *         my_addr - собственный адрес устройства 
	*			Выходные:
	*					*packet - указатель на буфер для помещения собранного пакета
  * @возвращает результат выполнения
  *			OK							- Успех
  *			NO_DATA					- Нет данных для чтения
  *			INVALID_ADDR		- Фрейм не для данного устройства
  *			NO_FREE_PBR			- Нет свободных буферов для сборки (MAX_PACKET_NUM следует увеличить)
  *			IN_PROGRESS			- Сборка пакета еще не закончена
  *			INVALID_FORMAT	- Неверный формат пакета
  *			INVALID_CRC			-	Неверная контрольная сумма собранного пакета
 */
driver_err_t driver_get_packet (uint8_t my_addr, protocol_packet_t* packet)
{
	driver_err_t ret = OK;
	uint8_t PRB_index = 0;				// index of parallel packet assembling  buffer (max MAX_CAN_PACK) 
	protocol_frame_t rx_frame;  	// buf for received CAN frame
	memset(&rx_frame, 0, sizeof(rx_frame));

	// Получение заголовка фрейма
	if( driver_get_data((uint8_t*)&rx_frame, sizeof(protocol_frame_header_t)) ) return NO_DATA;

	// Дочитать данные фрейма
	if(rx_frame.header.data_num){
		if( driver_get_data((uint8_t*)&rx_frame.data, rx_frame.header.data_num) ) return NO_DATA;
	}

	// Проверка адреса назначения фрейма 
	if( rx_frame.header.id.DA != my_addr ) return INVALID_ADDR;

	// Получение индекса свободного или текущего буфера для сборки
	if( !driver_check_PRB(&PRB_index, &rx_frame) ) return NO_FREE_PBR;
			
	// Сборка пакета из фреймов
	if( (ret = driver_assemble_packet(&PRB[PRB_index], &rx_frame)) != OK ) return ret;

	// Помещение собранного пакета в выходной буфер
	if(packet) memcpy(packet, &PRB[PRB_index].packet, PRB[PRB_index].packet_data_len);			
  
	return OK;         
}
