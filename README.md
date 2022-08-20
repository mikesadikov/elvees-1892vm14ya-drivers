## Несколько драйверов для микропроцессора 1892ВМ14Я АО НПЦ "ЭЛВИС"

### На борту данного [процессора](https://elvees.ru/chip/processors-multicore/1892vm14ja):
два CPU ARM Cortex-A9, два DSP ядра ELcore-30M, кодек H.264, графический 3D акселератор Mali-300, навигационный коррелятор ГЛОНАСС/GPS/Beidou и встроенные порты ввода/вывода.

### rtc-mcom02.c: 
драйвер часов реального времени (RTC), 
путь: drivers/rtc/rtc-mcom02.c
В основной [ветке ядра Linux](https://github.com/elvees/linux/tree/mcom02-4.4.y) у "ЭЛВИС" данный драйвер отутствует. 
Сам "ЭЛВИС" на своих отладочных платах использует внешнюю микросхему RTC.

### mcom02-power.c:
путь: drivers/power/mcom02-power.c

демонстрационный драйвер, позволяющий отключать DSP ядра и графический 3D акселератор и тем самым сильно снижать потребление микропроцессора.

### elv-mipi-dsi.c, panel-hx8369a-spi.c:
пути: drivers/video/fbdev/vpoutfb/elv-mipi-dsi.c, drivers/video/backlight/panel-hx8369a-spi.c

драйверы для порта видео выхода c поддержкой формата MIPI DSI и для контроллера дисплея Himax HX8369 (480x864, RGB, 16.7M цветов, управление по SPI, картинка по MIPI-DSI)
