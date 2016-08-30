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


function page_head($dbconn,$name,$refreshable=true) {
	if (!isset($_SESSION)) {
		session_start();
	}
	echo "<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.01//EN\"\n";
	echo "                 \"http://www.w3.org/TR/html4/strict.dtd\">\n";
	echo "<HTML>\n";
	echo "<HEAD>\n";
	echo "<meta http-equiv=\"Content-Type\" content=\"text/html; charset=ISO-8859-1\"> \n";
	echo "<TITLE>$name</TITLE>\n";
	echo "<LINK REV=MADE HREF=\"mailto:juerge@juergen-hannappel.de\"> \n";
	echo "<link type=\"text/css\" rel=\"stylesheet\" href=\"style.css\">\n";
	echo "</HEAD>\n";
	echo "\n";
	echo "<BODY>\n";
	echo "<DIV id=\"globalstate\" class=\"fadeout\">\n";
	$result = pg_query($dbconn,"SELECT count(*) as n FROM uid_states WHERE type > 1;");
	$row = pg_fetch_assoc($result);
	echo "<a href=\"alarms.php\">${row['n']} alarms</a>\n";
	echo "</DIV>\n";
	echo "<DIV id=\"main\">\n";
	echo "<DIV id=\"content\">\n";

}	
function page_foot() {
	echo "</DIV>\n";
	echo "</DIV>\n";
	echo "</body>\n";
}


?>
