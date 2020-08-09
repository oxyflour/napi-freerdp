const Connection = require('../'),
    bmp = require('bmp-js'),
    fs = require('fs')

const canvas = { width: 800, height: 600, data: Buffer.alloc(0) }
canvas.data = Buffer.alloc(canvas.width * canvas.height * 4) // ARBG

const conn = new Connection({
    serverHostName: 'nuc7.yff.me',
    username: 'oxyflour',
    password: process.env.FREERDP_TEST_PASSWORD,
    ignoreCertificate: true,
    desktopWidth: canvas.width,
    desktopHeight: canvas.height,
})
conn.on('error', err => {
    console.error(err)
    process.exit(1)
})
conn.on('paint', ({ x, y, w, h, d }) => {
    const c = canvas.data,
        a = new Uint8Array(d)
    for (let i = 0; i < w; i ++) {
        for (let j = 0; j < h; j ++) {
            const n = (j * w + i) * 4,
                m = ((j + y) * canvas.width + (i + x)) * 4
            c[m    ] = a[n + 3] // A
            c[m + 1] = a[n    ] // B
            c[m + 2] = a[n + 1] // G
            c[m + 3] = a[n + 2] // R
        }
    }
    console.log(x, y, w, h, d.byteLength)
    fs.writeFileSync('build/test.bmp', bmp.encode(canvas).data)
})

conn.on('connected', () => {
    console.log('connected')
    //setTimeout(() => conn.sendKey(0x5B, true), 3000)
    //setTimeout(() => conn.sendKey(0x5B, false), 4000)
    setTimeout(() => conn.sendMouse(750, 550, { left: true }), 3000)
    setTimeout(() => conn.sendMouse(750, 550, { left: false }), 4000)
    setTimeout(() => conn.destroy(), 5000)
})
