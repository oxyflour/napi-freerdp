const Connection = require('../'),
    fs = require('fs')

const server = require('http').createServer((req, res) => {
    res.setHeader('Content-Type', 'text/html')
    fs.readFile(__dirname + '/index.html', (err, html) => {
        res.setHeader('Content-Length', html.byteLength)
        res.end(html)
    })
})

const io = require('socket.io')(server)
io.on('connect', ws => {
    const { width, height, host, username } = ws.handshake.query
    const conn = new Connection({
        serverHostName: host,
        username: username || 'guest',
        password: '',
        ignoreCertificate: true,
        desktopWidth: width || 1024,
        desktopHeight: height || 768,
    })
    conn.on('paint', ({ x, y, w, h, d }) => {
        ws.compress(true).emit('paint', ({ x, y, w, h, d }))
    })
    conn.on('error', err => {
        console.error(err)
        ws.disconnect()
    })
    conn.on('connected', () => {
        ws.emit('connected')
    })
    conn.on('disconnected', () => {
        ws.disconnect()
    })
    ws.on('key', ({ key, down }) => {
        conn.sendKey(Connection.scancodes[key], down)
    })
    ws.on('mouse', ({ x, y, opts }) => {
        conn.sendMouse(x, y, opts)
    })
    ws.on('close', () => {
        conn.destroy()
    })
})

server.listen(5000, () => {
    console.log(`INFO: listening at port 5000`)
})
