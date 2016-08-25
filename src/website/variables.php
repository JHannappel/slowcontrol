<?php

include apache_getenv('CONTEXT_DOCUMENT_ROOT')."/database.php";

if (!function_exists("http_redirect")) {
        function http_redirect($url, $params, $session = false , $status = 0 ) {
                $hostname = $_SERVER['HTTP_HOST'];
                $path = dirname($_SERVER['PHP_SELF']);
                if ($_SERVER['SERVER_PROTOCOL'] == 'HTTP/1.1') {
                        if (php_sapi_name() == 'cgi') {
                                header('Status: 303 See Other');
                        } else {
                                header('HTTP/1.1 303 See Other');
                        }
                }
                if ($params==NULL) {
                        header('Location: http://'.$hostname.($path == '/' ? '' : $path)."/".$url);
                } else {
                        header('Location: http://'.$hostname.($path == '/' ? '' : $path)."/".$url."?".http_build_query($params));
                }
        }
 }
defined('HTTP_METH_GET') or define('HTTP_METH_GET','GET');

?>
