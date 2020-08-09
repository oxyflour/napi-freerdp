const binding = require('./build/Release/binding.node'),
    { EventEmitter } = require('events')

class Connection extends EventEmitter {
    constructor(opts) {
        super()
        this.ptr = binding.connect(opts, (evt, msg) => this.emit(evt, msg))
    }
    sendKey(code, down) {
        binding.sendKey(this.ptr, code, down)
    }
    sendMouse(x, y, opts) {
        binding.sendMouse(this.ptr, x, y, opts)
    }
    destroy() {
        binding.disconnect(this.ptr)
    }
}

module.exports = Connection
