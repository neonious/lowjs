Simple webserver with chat functionality via WebSocket made for low.js/Node.JS

Run with bin/low examples/chat_ws_webserver/index.js from lowjs directory
and then open the links given with the web browser.

The https link will give a certificate warning, which is correct, because
the certificate is self-signed.

In node_modules you can find the following modules, transpiled to ES5 via
TypeScript:

ws:		6.1.0
async_limiter:	1.0.0

The package.json is NOT included, so a npm install does not replace the
transpiled files with the non-transpiled versions.
