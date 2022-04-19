# Frame Packets Driver
Драйвер для отправки и приема сообщений (байтовых потоков) переменной длины в формате пакетов.

## Примечания:
1. Формат фрейма пакета выбран исходя из совместимости с форматом CAN-кадра.
2. Т.к. шина CAN в общем случаем подразумевает несколько устройств на линии,
	 в отличие от UART, в идентификатор фрейма пакета включены адрес источника 
	 и адрес назначения для возможности программной фильтрации поступающих данных.
3. Выбор физического интерфейса для приемопередачи сообщений осуществляется 
   макроопределениями UART_INTERFACE и CAN_INTERFACE при компиляции (см. Makefile).

## Форматы фрейма и пакета

Исходное сообщение разбивается на фреймы заданного формата,
формирующие пакет. Пакет имеет идентификатор, в котором:
* PM - параметр сообщения
* SA - адрес источника сообщения
* DA - адрес назначения сообщения
* FN - функция сообщения

![packet](/packet_and_frame.bmp)
data_num - кол-во байт данных в фрейме,
N - количество фреймов в пакете
