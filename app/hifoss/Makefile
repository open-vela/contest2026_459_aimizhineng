############################################################################
# apps/examples/hifoss/Makefile
############################################################################

include $(APPDIR)/Make.defs

# HiFOSS WiFi OLED built-in application info

PROGNAME  = $(CONFIG_EXAMPLES_HIFOSS_PROGNAME)
PRIORITY  = $(CONFIG_EXAMPLES_HIFOSS_PRIORITY)
STACKSIZE = $(CONFIG_EXAMPLES_HIFOSS_STACKSIZE)
MODULE    = $(CONFIG_EXAMPLES_HIFOSS)

# HiFOSS demo: OLED(SSD1306/SPI0) + ESP8266(AT/ESP-MQTT over USART1)

MAINSRC = hifoss_main.c

CSRCS = oled.c esp8266_at.c

include $(APPDIR)/Application.mk
