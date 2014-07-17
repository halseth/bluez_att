HTTP Proxy Server
==============================================

Let a BLE device send HTTP request to a web server.

### Instructions:
- Include "curl_proxy.h".
- Initialize the proxy by calling initialize() with preferred parameters.
- The HTTP requests are sent by filling a request_t struct, and calling add_server_request() with this struct and a callback function.
- The callback will be fired when an answer from the server are received, passing on a response_t struct.

TODO:
-----
  - HTTP Header in request supporting more than one line.
