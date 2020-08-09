import { EventEmitter } from 'events'

declare class Connection extends EventEmitter {
    constructor(opts: {
        serverHostName?: string
        username?: string
        password?: string
        ignoreCertificate?: boolean
        desktopWidth?: number
        desktopHeight?: number
    })
    sendKey(code: number, down: boolean): void
    sendMouse(x: number, y: number, opts: {
        left?: boolean
        right?: boolean
        middle?: boolean
    }): void
    destroy(): void
    on(evt: 'connected', listener: () => void): void
    on(evt: 'disconnected', listener: () => void): void
    on(evt: 'error', listener: (err: Error) => void): void
    on(evt: 'paint', listener: (img: { x: number, y: number, w: number, h: number, d: ArrayBuffer }) => void): void
}

export = Connection
