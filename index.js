const binding = require('./build/Release/binding.node'),
    { EventEmitter } = require('events')

const scancodes = { },
// TODO: check keycodes
// https://github.com/FreeRDP/FreeRDP/blob/master/include/freerdp/scancode.h
    keys =
        '|Escape|1|2|3|4|5|6|7|8|9|0|-|+|Backspace' +
        '|Tab|q|w|e|r|t|y|u|i|o|p|oem4|oem6|Enter' +
        '|Control1|a|s|d|f|g|h|j|k|l|oem1|oem7|oem3' +
        '|Shift1|oem5|z|x|c|v|b|n|m|,|.|oem2|Shift2|x-MULTIPLY' +
        '|ContextMenu1| |CapsLock|F1|F2|F3|F4|F5|F6|F7|F8|F9|F10|x-NUMLOCK'
for (const [idx, key] of keys.split('|').entries()) {
    scancodes[key] = idx
}

class Connection extends EventEmitter {
    constructor(opts) {
        super()
        this.ptr = binding.connect(opts, (evt, msg) => this.emit(evt, msg))
    }
    sendKey(code, down) {
        binding.sendKey(this.ptr, code, down)
    }
    sendMouse(x, y, opts) {
        binding.sendMouse(this.ptr, x, y, opts || { })
    }
    destroy() {
        binding.disconnect(this.ptr)
    }
    static scancodes = scancodes
}

module.exports = Connection
