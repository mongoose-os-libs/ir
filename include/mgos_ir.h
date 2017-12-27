/*
 * Copyright 2017 Vladimir Dronnikov <dronnikov@gmail.com>
 * All rights reserved
 *
 * IP protocol driver API
 */

#ifndef CS_FW_SRC_MGOS_IR_H_
#define CS_FW_SRC_MGOS_IR_H_

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

struct mgos_irrecv_nec_s {
  int pin;
  void (*handler)(int, void *);
  void *user_data;
  union {
    uint8_t byte[4];
    uint32_t dword;
  } code;
  uint32_t t;
  uint8_t bit;
};

/*
 * Create an object instance of IR receiver for NEC protocol.
 * Return value: an object with the methods described below.
 */
struct mgos_irrecv_nec_s *mgos_irrecv_nec_create(int pin, void (*cb)(int, void *), void *userdata);

/*
 * Destroy an object instance of IR receiver for NEC protocol.
 */
void mgos_irrecv_nec_close(struct mgos_irrecv_nec_s *obj);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* CS_FW_SRC_MGOS_IR_H_ */
