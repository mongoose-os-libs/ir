# IR protocol library

## Usage

Either

### src/main.c

```c
#include "mgos.h"
#include "mgos_ir.h"

static void irrecv_cb(int code, void *arg)
{
  LOG(LL_INFO, ("IR: %08X", code));
  (void) arg;
}

enum mgos_app_init_result mgos_app_init(void)
{
  // TSOP on pin 14, NEC protocol
  mgos_irrecv_nec_create(14, irrecv_cb, NULL);

  return MGOS_APP_INIT_SUCCESS;
}
```

or

### fs/init.js

```js
load("api_ir.js");

// TSOP on pin 14, NEC protocol
let ir = IR.Receiver.NEC.create(14, function(code) {
    print("IR", code);
}, null);
```
