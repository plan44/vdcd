# JSON API forwarder

This little php script can be installed on a local webserver to forward http POST requests with JSON payload to a vdcd daemon which has a TCP socket JSON API.

To simplify testing, simple JSON requests that only consist of an object containing some fields (and no nested arrays or objects) can also be created as GET request with CGI style parameters in the URI.

Just install it on a webserver and make sure line 9 and 10 point to a running vdcd instance.