#include "mgos.h"
#include "mgos_pwm.h"
#include "mgos_ir.h"
#include "mgos_time.h"


//------------------------------------------------------------------------------
// NEC receiver
//------------------------------------------------------------------------------

static IRAM void irrecv_nec_handler(int pin, void *arg)
{
  struct mgos_irrecv_nec_s *obj = (struct mgos_irrecv_nec_s *)arg;
  // get microseconds
  uint32_t t = 1000000 * mgos_uptime();
  // 0-1 transition?
  if (mgos_gpio_read(pin)) {
    // start counter
    obj->t = t;
    return;
  }
  // 1-0 transition. stop counter
  t -= obj->t;
  // derive bit from pulse width
  // 0
  if (t < 1000) {
    obj->code.dword <<= 1;
    obj->code.byte[0] &= ~0b00000001;
    ++obj->bit;
  // 1
  } else if (t < 2000) {
    obj->code.dword <<= 1;
    obj->code.byte[0] |= 0b00000001;
    ++obj->bit;
  // sequence start
  } else if (t >= 4000) {
    obj->bit = 0;
  // repeat
  } else {
    // FIXME: just signal if pulse circa 2250?
  }
  // sequence end?
  if (obj->bit == 32) {
    obj->bit = 0; // NB: do not auto-repeat
    // CRC ok?
#if MGOS_IRRECV_NEC_CHECK_ADDR_CRC
    if ((obj->code.byte[1] ^ obj->code.byte[0]) == 0xFF &&
        (obj->code.byte[3] ^ obj->code.byte[2]) == 0xFF)
#elif MGOS_IRRECV_NEC_CHECK_CODE_CRC
    if ((obj->code.byte[1] ^ obj->code.byte[0]) == 0xFF)
#endif
    {
      // report code
      // NO LOG or printf in ISR service routine, or in the handler
      // LOG(LL_DEBUG, ("IRRECV @ %d: %08X", pin, obj->code.dword));
      if (obj->handler) {
        obj->handler(obj->code.dword, obj->user_data);
      }
    }
  }
}

struct mgos_irrecv_nec_s *mgos_irrecv_nec_create(int pin, void (*handler)(int, void *), void *user_data)
{
  struct mgos_irrecv_nec_s *obj = calloc(1, sizeof(*obj));
  if (obj == NULL) return NULL;
  obj->pin = pin;
  obj->handler = handler;
  obj->user_data = user_data;
  obj->bit = 0;
  obj->t = 0;
  if (
    !mgos_gpio_set_mode(pin, MGOS_GPIO_MODE_INPUT) ||
    !mgos_gpio_set_pull(pin, MGOS_GPIO_PULL_UP) ||
    !mgos_gpio_set_int_handler_isr(pin, MGOS_GPIO_INT_EDGE_ANY, irrecv_nec_handler, (void *)obj) ||
    !mgos_gpio_enable_int(pin)
  ) {
    mgos_irrecv_nec_close(obj);
    return NULL;
  }
  return obj;
}

void mgos_irrecv_nec_close(struct mgos_irrecv_nec_s *obj)
{
  mgos_gpio_remove_int_handler(obj->pin, NULL, NULL);
  free(obj);
  obj = NULL;
}

//------------------------------------------------------------------------------
// NEC sender
//------------------------------------------------------------------------------

#define IRSEND_NEC_HDR_MARK   9000
#define IRSEND_NEC_HDR_SPACE  4500
#define IRSEND_NEC_BIT_MARK    560
#define IRSEND_NEC_ONE_SPACE  1600
#define IRSEND_NEC_ZERO_SPACE  560
#define IRSEND_NEC_RPT_SPACE  2250

#define IRSEND_NEC_PWM_CYCLE  (1000000 / 38000)

// TODO: mgos_bitbang_write_bits or non-delaying state machine?

// bit-bang by switching pin mode
#if 1

static void irsend_nec_tsop(int pin, int code)
{
  // setup

  // header
  mgos_gpio_set_mode(pin, MGOS_GPIO_MODE_OUTPUT);
  mgos_usleep(IRSEND_NEC_HDR_MARK);
  mgos_gpio_set_mode(pin, MGOS_GPIO_MODE_INPUT);
  mgos_usleep(IRSEND_NEC_HDR_SPACE);

  // sequence
  for (uint32_t i = 0x80000000; i; i >>= 1) {
    mgos_gpio_set_mode(pin, MGOS_GPIO_MODE_OUTPUT);
    mgos_usleep(IRSEND_NEC_BIT_MARK);
    mgos_gpio_set_mode(pin, MGOS_GPIO_MODE_INPUT);
    if (code & i) {
      mgos_usleep(IRSEND_NEC_ONE_SPACE);
    } else {
      mgos_usleep(IRSEND_NEC_ZERO_SPACE);
    }
  }

  // footer
  mgos_gpio_set_mode(pin, MGOS_GPIO_MODE_OUTPUT);
  mgos_usleep(IRSEND_NEC_BIT_MARK);
  mgos_gpio_set_mode(pin, MGOS_GPIO_MODE_INPUT);

  // cleanup
}

#else

//------------------------------------------------------------------------------

static void irsend_nec_tsop(int pin, int code)
{
  // setup
  mgos_gpio_set_mode(pin, MGOS_GPIO_MODE_OUTPUT);
  mgos_gpio_write(pin, 1);
  mgos_gpio_set_pull(pin, MGOS_GPIO_PULL_NONE);
  mgos_usleep(IRSEND_NEC_HDR_MARK);

  // header
  mgos_gpio_write(pin, 0);
  mgos_usleep(IRSEND_NEC_HDR_MARK);
  mgos_gpio_write(pin, 1);
  mgos_usleep(IRSEND_NEC_HDR_SPACE);

  // sequence
  for (uint32_t i = 0x80000000; i; i >>= 1) {
    mgos_gpio_write(pin, 0);
    mgos_usleep(IRSEND_NEC_BIT_MARK);
    mgos_gpio_write(pin, 1);
    if (code & i) {
      mgos_usleep(IRSEND_NEC_ONE_SPACE);
    } else {
      mgos_usleep(IRSEND_NEC_ZERO_SPACE);
    }
  }

  // footer
  mgos_gpio_write(pin, 0);
  mgos_usleep(IRSEND_NEC_BIT_MARK);
  mgos_gpio_write(pin, 1);

  // cleanup
  mgos_gpio_set_mode(pin, MGOS_GPIO_MODE_INPUT);
  mgos_gpio_set_pull(pin, MGOS_GPIO_PULL_UP);
}

#endif

static void irsend_carrier_38kHz(int pin, int n)
{
  // make signal of circa 38 kHz of circa 1/3 duty
  for (; n >= 0; --n) {
    mgos_gpio_write(pin, 1);
    mgos_usleep(9);
    mgos_gpio_write(pin, 0);
    mgos_usleep(16);
  }
}

static void irsend_nec_pwm(int pin, int code)
{
  // setup
  mgos_gpio_set_mode(pin, MGOS_GPIO_MODE_OUTPUT);

  // header
  irsend_carrier_38kHz(pin, IRSEND_NEC_HDR_MARK / IRSEND_NEC_PWM_CYCLE);
  mgos_usleep(IRSEND_NEC_HDR_SPACE);

  // sequence
  for (uint32_t i = 0x80000000; i; i >>= 1) {
    irsend_carrier_38kHz(pin, IRSEND_NEC_BIT_MARK / IRSEND_NEC_PWM_CYCLE);
    if (code & i) {
      mgos_usleep(IRSEND_NEC_ONE_SPACE);
    } else {
      mgos_usleep(IRSEND_NEC_ZERO_SPACE);
    }
  }

  // footer
  irsend_carrier_38kHz(pin, IRSEND_NEC_BIT_MARK / IRSEND_NEC_PWM_CYCLE);

  // cleanup
  mgos_gpio_set_mode(pin, MGOS_GPIO_MODE_INPUT);
}

void mgos_irsend_nec(int pin, int code, bool tsop)
{
  LOG(LL_DEBUG, ("IRSEND @ %d: %08X", pin, code));

  if (tsop) {
    irsend_nec_tsop(pin, code);
  } else {
    irsend_nec_pwm(pin, code);
  }
}

//------------------------------------------------------------------------------

bool mgos_ir_init(void) {
  return true;
}

//------------------------------------------------------------------------------
