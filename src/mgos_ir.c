#include "mgos.h"
#include "mgos_ir.h"

static void irrecv_nec_handler(int pin, void *arg)
{
  struct mgos_irrecv_nec_s *obj = (struct mgos_irrecv_nec_s *)arg;
  // 0-1 transition?
  uint32_t t = 1000000 * mgos_uptime();
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
      LOG(LL_DEBUG, ("IR: %08X", obj->code.dword));
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
  mgos_gpio_disable_int(obj->pin);
  mgos_gpio_remove_int_handler(obj->pin, NULL, NULL);
  free(obj);
  obj = NULL;
}

bool mgos_ir_init(void) {
  return true;
}
