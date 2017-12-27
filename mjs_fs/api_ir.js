// IR protocol library API. Source C API is defined at:
// [mgos_ir.h](https://github.com/dvv/mongoose-os-libs-ir/blob/master/include/mgos_ir.h)

let IR = {

  Receiver: {

    NEC: {
      _crt: ffi('void *mgos_irrecv_nec_create(int, void (*)(int, userdata), userdata)'),
      _cls: ffi('void mgos_irrecv_nec_close(void *)'),

      // ## **`IR.Receiver.NEC.create(pin, callback, userdata)`**
      // Create an object instance of IR receiver for NEC protocol.
      // Return value: an object with the methods described below.
      create: function(pin, cb, ud) {
        let self = Object.create(IR.Receiver.NEC._proto);
        self.obj = IR.Receiver.NEC._crt(pin, cb, ud);
        return self;
      },
      _proto: {
        // ## **`myIR.close()`**
        // Close receiver handle. Return value: none.
        close: function() {
          return IR.Receiver.NEC._cls(this.obj);
        }
      }
    }

  }

};
