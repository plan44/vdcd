<?php

// this converts HTTP GET and POST requests to TCP JSON request in the same way
// the plan44 mg44 embedded web server does. So this php script can be used on
// any PHP enabled web server to simulate a mg44-like JSON API forwarder

// Configuration:
// - TCP JSON server socket
$json_api_host = 'localhost';
$json_api_port = 8090;


// HTTP requests will be converted into pure JSON as follows:
// { "method" : "GET", "uri" : "/myuri" ["uri_params":{ "param1":"value1" [, "param2":"value2",...] }] }
// { "method" : "POST"|"PUT", "uri" : "/myuri", ["uri_params":{ ... }] "data" : <{ JSON payload from PUT or POST }>}

$method = $_SERVER['REQUEST_METHOD'];
$uri = '';
if (isset($_SERVER['PATH_INFO'])) $uri = $_SERVER['PATH_INFO'];
$uri_params = false;
$data = false;
// if there are CGI params in the URI, put them into uri_params
if (sizeof($_REQUEST)>0) {
  $uri_params = $_REQUEST;
}
// in case of PUT and POST, get JSON from post data
if ($method=='PUT' || $method=='POST') {
  $json = file_get_contents('php://input');
  $data = json_decode($json);
}

// wrap in mg44-compatible JSON
if (substr($uri,0,1)=='/')
  $uri = substr($uri,1);
$wrappedcall = array(
  'method' => $method,
  'uri' => $uri,
);
if ($uri_params!==false) {
  $wrappedcall['uri_params'] = $uri_params;
}
if ($data!==false) {
  $wrappedcall['data'] = $data;
}

// now call
$request = json_encode($wrappedcall);
$fp = fsockopen($json_api_host, $json_api_port, $errno, $errstr, 10);
if (!$fp) {
  $result = array('error' => 'cannot open TCP connection to ' . $json_api_host . ':' . $json_api_port);
} else {
  fwrite($fp, $request);
  while (!feof($fp)) {
    $data = fgets($fp, 128);
    echo $data;
    if (strstr($data,"\n"))
      break;
  }
  fclose($fp);
}

?>