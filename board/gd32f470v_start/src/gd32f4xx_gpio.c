/****************************************************************************
 * vendor/gigadevice/boards/gd32f4/gd32f470v_start/src/gd32f4xx_gpio.c
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.  The
 * ASF licenses this file to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance with the
 * License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.  See the
 * License for the specific language governing permissions and limitations
 * under the License.
 *
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#include <stdbool.h>
#include <assert.h>
#include <debug.h>

#include <nuttx/clock.h>
#include <nuttx/wdog.h>
#include <nuttx/ioexpander/gpio.h>

#include <arch/board/board.h>

#include "chip.h"

#include "gd32f4xx.h"
#include "gd32f470v_start.h"

#if defined(CONFIG_DEV_GPIO) && !defined(CONFIG_GPIO_LOWER_HALF)

/****************************************************************************
 * Private Types
 ****************************************************************************/

struct gd32gpio_dev_s
{
  struct gpio_dev_s gpio;
  uint8_t id;
  uint32_t pinset;         /* Physical pin configuration (port + pin + mode) */
  pin_interrupt_t callback; /* Interrupt callback, valid when interrupt type */
};

irqstate_t flags;

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

static int gpio_setpintype(struct gpio_dev_s *dev,
                           enum gpio_pintype_e gp_pintype);

#if BOARD_NGPIOIN > 0
static int gpin_read(struct gpio_dev_s *dev, bool *value);
#endif

#if BOARD_NGPIOOUT > 0
static int gpout_read(struct gpio_dev_s *dev, bool *value);
static int gpout_write(struct gpio_dev_s *dev, bool value);
#endif

#if BOARD_NGPIOINT > 0
static int gpint_read(struct gpio_dev_s *dev, bool *value);
static int gpint_attach(struct gpio_dev_s *dev,
                        pin_interrupt_t callback);
static int gpint_enable(struct gpio_dev_s *dev, bool enable);
#endif

/****************************************************************************
 * Private Data
 ****************************************************************************/

 #if BOARD_NGPIOIN > 0
static const struct gpio_operations_s gpin_ops =
{
  .go_read   = gpin_read,
  .go_write  = NULL,
  .go_attach = NULL,
  .go_enable = NULL,
  .go_setpintype = gpio_setpintype,
};

/* This array maps the GPIO pins used as INPUT */

static const uint32_t g_gpioinputs[BOARD_NGPIOIN] =
{
  GPIO_IN1,
};

static struct gd32gpio_dev_s g_gpin[BOARD_NGPIOIN];
#endif

#if BOARD_NGPIOOUT > 0

static const struct gpio_operations_s gpout_ops =
{
  .go_read   = gpout_read,
  .go_write  = gpout_write,
  .go_attach = NULL,
  .go_enable = NULL,
  .go_setpintype = gpio_setpintype,
};

/* This array maps the GPIO pins used as OUTPUT */

static const uint32_t g_gpiooutputs[BOARD_NGPIOOUT] =
{
  GPIO_OUT1,
  GPIO_OUT2,
  GPIO_OUT3,
  GPIO_OUT4,
  GPIO_OUT5,
};

static struct gd32gpio_dev_s g_gpout[BOARD_NGPIOOUT];
#endif

#if BOARD_NGPIOINT > 0

static const struct gpio_operations_s gpint_ops =
{
  .go_read   = gpint_read,
  .go_write  = NULL,
  .go_attach = gpint_attach,
  .go_enable = gpint_enable,
  .go_setpintype = gpio_setpintype,
};

/* This array maps the GPIO pins used as INTERRUPT INPUTS */

static const uint32_t g_gpiointinputs[BOARD_NGPIOINT] =
{
  GPIO_INT1,
};

static struct gd32gpio_dev_s g_gpint[BOARD_NGPIOINT];
#endif

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Name: gpio_setpintype
 *
 * Description:
 *   set gpio pintype.
 *
 ****************************************************************************/

static int gpio_setpintype(struct gpio_dev_s *dev,
                           enum gpio_pintype_e gpio_pintype)
{
  struct gd32gpio_dev_s *gd32gpio = (struct gd32gpio_dev_s *)dev;
  uint32_t base;
  uint32_t new_pinset;

  /* Extract port + pin bits, strip mode/speed/pupd */

  base = gd32gpio->pinset & (GPIO_CFG_PORT_MASK | GPIO_CFG_PIN_MASK);

  if (gpio_pintype >= GPIO_INTERRUPT_PIN && gpio_pintype < GPIO_NPINTYPES)
    {
      /* Interrupt pin: reconfigure hardware as input, EXTI is set up
       * later in go_attach() which reads dev->gp_pintype for the
       * trigger type.
       */

      new_pinset = base | GPIO_CFG_MODE_INPUT | GPIO_CFG_PUPD_NONE;
      gd32_gpio_config(new_pinset);
      dev->gp_ops = &gpint_ops;
    }
  else
    {
      switch (gpio_pintype)
        {
          case GPIO_INPUT_PIN:
            new_pinset = base | GPIO_CFG_MODE_INPUT | GPIO_CFG_PUPD_NONE;
            dev->gp_ops = &gpin_ops;
            break;

          case GPIO_INPUT_PIN_PULLUP:
            new_pinset = base | GPIO_CFG_MODE_INPUT | GPIO_CFG_PUPD_PULLUP;
            dev->gp_ops = &gpin_ops;
            break;

          case GPIO_INPUT_PIN_PULLDOWN:
            new_pinset = base | GPIO_CFG_MODE_INPUT |
                         GPIO_CFG_PUPD_PULLDOWN;
            dev->gp_ops = &gpin_ops;
            break;

          case GPIO_OUTPUT_PIN:
          case GPIO_OUTPUT_PIN_OPENDRAIN:
            new_pinset = base | GPIO_CFG_MODE_OUTPUT |
                         GPIO_CFG_SPEED_50MHZ;
            dev->gp_ops = &gpout_ops;
            break;

          default:
            return -EINVAL;
        }

      gd32_gpio_config(new_pinset);
    }

  gd32gpio->pinset    = new_pinset;
  dev->gp_pintype     = gpio_pintype;
  return OK;
}

#if BOARD_NGPIOIN > 0
static int gpin_read(struct gpio_dev_s *dev, bool *value)
{
  struct gd32gpio_dev_s *gd32gpio = (struct gd32gpio_dev_s *)dev;

  DEBUGASSERT(gd32gpio != NULL && value != NULL);
  gpioinfo("Reading...\n");

  *value = gd32_gpio_read(gd32gpio->pinset);
  return OK;
}
#endif

#if BOARD_NGPIOOUT > 0
static int gpout_read(struct gpio_dev_s *dev, bool *value)
{
  struct gd32gpio_dev_s *gd32gpio = (struct gd32gpio_dev_s *)dev;

  DEBUGASSERT(gd32gpio != NULL && value != NULL);
  gpioinfo("Reading...\n");

  *value = gd32_gpio_read(gd32gpio->pinset);
  return OK;
}

static int gpout_write(struct gpio_dev_s *dev, bool value)
{
  struct gd32gpio_dev_s *gd32gpio = (struct gd32gpio_dev_s *)dev;

  DEBUGASSERT(gd32gpio != NULL);
  gpioinfo("Writing %d\n", (int)value);

  gd32_gpio_write(gd32gpio->pinset, value);
  return OK;
}
#endif

#if BOARD_NGPIOINT > 0

static int gd32gpio_interrupt(int irq, void *context, void *arg)
{
  struct gd32gpio_dev_s *gd32gpio = (struct gd32gpio_dev_s *)arg;

  DEBUGASSERT(gd32gpio != NULL && gd32gpio->callback != NULL);
  gpioinfo("Interrupt! callback=%p\n", gd32gpio->callback);

  gd32gpio->callback(&gd32gpio->gpio, gd32gpio->id);
  return OK;
}

static int gpint_read(struct gpio_dev_s *dev, bool *value)
{
  struct gd32gpio_dev_s *gd32gpio = (struct gd32gpio_dev_s *)dev;

  DEBUGASSERT(gd32gpio != NULL && value != NULL);
  gpioinfo("Reading int pin...\n");

  *value = gd32_gpio_read(gd32gpio->pinset);
  return OK;
}

static int gpint_attach(struct gpio_dev_s *dev, pin_interrupt_t callback)
{
  struct gd32gpio_dev_s *gd32gpio = (struct gd32gpio_dev_s *)dev;
  uint8_t trig;
  int ret;
  uint8_t gpio_irq;
  uint8_t gpio_irqnum;

  gpioinfo("Attaching the callback\n");

  /* Map NuttX pin type to GD32 EXTI trigger type */

  switch (dev->gp_pintype)
    {
      case GPIO_INTERRUPT_FALLING_PIN:
      case GPIO_INTERRUPT_LOW_PIN:
        trig = EXTI_TRIG_FALLING;
        break;
      case GPIO_INTERRUPT_BOTH_PIN:
        trig = EXTI_TRIG_BOTH;
        break;
      case GPIO_INTERRUPT_PIN:
      case GPIO_INTERRUPT_RISING_PIN:
      case GPIO_INTERRUPT_HIGH_PIN:
      default:
        trig = EXTI_TRIG_RISING;
        break;
    }

  flags = enter_critical_section();

  /* Configure EXTI for this pin with the requested trigger */

  ret = gd32_exti_gpioirq_init(gd32gpio->pinset,
                               EXTI_INTERRUPT, trig, &gpio_irq);
  if (ret < 0)
    {
      leave_critical_section(flags);
      return ret;
    }

  gd32_gpio_exti_irqnum_get(gd32gpio->pinset, &gpio_irqnum);

  /* Attach ISR and leave interrupt disabled until go_enable(true) */

  gd32_exti_gpio_irq_attach(gpio_irq, gd32gpio_interrupt, gd32gpio);
  up_disable_irq(gpio_irqnum);

  leave_critical_section(flags);
  gpioinfo("Attach %p\n", callback);
  gd32gpio->callback = callback;
  return OK;
}

static int gpint_enable(struct gpio_dev_s *dev, bool enable)
{
  struct gd32gpio_dev_s *gd32gpio = (struct gd32gpio_dev_s *)dev;
  int ret;
  uint8_t gpio_irqnum;

  flags = enter_critical_section();

  ret = gd32_gpio_exti_irqnum_get(gd32gpio->pinset, &gpio_irqnum);
  if (ret < 0)
    {
      leave_critical_section(flags);
      return ret;
    }

  if (enable)
    {
      if (gd32gpio->callback != NULL)
        {
          gpioinfo("Enabling the interrupt\n");
          up_enable_irq(gpio_irqnum);
        }
    }
  else
    {
      up_disable_irq(gpio_irqnum);
      gpioinfo("Disable the interrupt\n");
    }

  leave_critical_section(flags);
  return OK;
}
#endif

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: gd32_gpio_initialize
 *
 * Description:
 *   Initialize GPIO drivers for use with /apps/examples/gpio
 *
 ****************************************************************************/

int gd32_gpio_initialize(void)
{
  int pincount = 0;
  int i;

#if BOARD_NGPIOIN > 0
  for (i = 0; i < BOARD_NGPIOIN; i++)
    {
      /* Setup and register the GPIO pin */

      g_gpin[i].gpio.gp_pintype = GPIO_INPUT_PIN;
      g_gpin[i].gpio.gp_ops     = &gpin_ops;
      g_gpin[i].id              = i;
      g_gpin[i].pinset          = g_gpioinputs[i];
      g_gpin[i].callback        = NULL;

      gpio_pin_register(&g_gpin[i].gpio, pincount);

      /* Configure the pin that will be used as input */

      gd32_gpio_config(g_gpioinputs[i]);

      pincount++;
    }
#endif

#if BOARD_NGPIOOUT > 0
  for (i = 0; i < BOARD_NGPIOOUT; i++)
    {
      /* Setup and register the GPIO pin */

      g_gpout[i].gpio.gp_pintype = GPIO_OUTPUT_PIN;
      g_gpout[i].gpio.gp_ops     = &gpout_ops;
      g_gpout[i].id              = i;
      g_gpout[i].pinset          = g_gpiooutputs[i];
      g_gpout[i].callback        = NULL;

      gpio_pin_register(&g_gpout[i].gpio, pincount);

      /* Configure the pin that will be used as output */

      gd32_gpio_write(g_gpiooutputs[i], 0);
      gd32_gpio_config(g_gpiooutputs[i]);

      pincount++;
    }
#endif

#if BOARD_NGPIOINT > 0
  for (i = 0; i < BOARD_NGPIOINT; i++)
    {
      /* Setup and register the GPIO pin */

      g_gpint[i].gpio.gp_pintype = GPIO_INTERRUPT_PIN;
      g_gpint[i].gpio.gp_ops     = &gpint_ops;
      g_gpint[i].id              = i;
      g_gpint[i].pinset          = g_gpiointinputs[i];
      g_gpint[i].callback        = NULL;
      (void)gpio_pin_register(&g_gpint[i].gpio, pincount);

      /* Configure the pin that will be used as interrupt input */

      gd32_gpio_config(g_gpiointinputs[i]);

      pincount++;
    }
#endif

  return 0;
}
#endif /* CONFIG_DEV_GPIO && !CONFIG_GPIO_LOWER_HALF */
