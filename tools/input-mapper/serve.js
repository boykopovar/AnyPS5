import {createServer} from 'node:http';
import {readFile} from 'node:fs/promises';

const files = new Set(['index.html', 'styles.css', 'app.js', 'mapping.js', 'keys.js', 'capture.js', 'validation.js', 'i18n.js', 'dropdown.js']);
const mime = {html: 'text/html', css: 'text/css', js: 'text/javascript'};
const port = Number(process.env.PORT || 4173);
const server = createServer(async (request, response) => {
    const path = new URL(request.url, 'http://127.0.0.1').pathname;
    const file = path === '/' ? 'index.html' : path.slice(1);
    if (!['GET', 'HEAD'].includes(request.method)) {
        response.writeHead(405, {Allow: 'GET, HEAD'}).end();
        return;
    }
    if (!files.has(file)) {
        response.writeHead(404).end('Not found');
        return;
    }
    try {
        const content = await readFile(new URL(file, import.meta.url));
        response.writeHead(200, {'Content-Type': `${mime[file.split('.').pop()]}; charset=utf-8`, 'Cache-Control': 'no-store'});
        response.end(request.method === 'HEAD' ? undefined : content);
    } catch {
        response.writeHead(500).end('Cannot read static file');
    }
});
server.on('error', error => {
    console.error(error.message);
    process.exitCode = 1;
});
server.listen(port, '127.0.0.1', () => console.log(`Input mapper: http://127.0.0.1:${port}`));
