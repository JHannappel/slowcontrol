<?php

$dbstring="host=localhost dbname=testdb user=hannappe";

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


function get_int_list($varname) {
	if (isset($_GET[$varname])) {
		foreach (explode(",",$_GET[$varname]) as $item) {
			if (!is_numeric($item)) {
				die("bad paramaeter value '$item'in '${_GET[$varname]}'");
			}
		};
		return $_GET[$varname];
	}
	return "";
}
function get_uid_list($initial_list="") {
  $uidlist=$initial_list;
  foreach (array_keys($_GET) as $key) {
    if ($key[0]=='u') {
      $numpart=substr($key,1);
      if (strspn($numpart,"0123456789") != strlen($numpart)) {
	continue;
      }
      if ($uidlist != "") {
	$uidlist.=",";
      }
      $uidlist.=$numpart;
    }
  }
  return $uidlist;
}
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
	echo "<DIV id=\"globalstate\" class=\"fadeout\"> Global status\n";
	echo date("Y-M-d H:i:s");
	$result = pg_query($dbconn,"SELECT(SELECT count(*) as bad_states FROM uid_states WHERE type > 1),(SELECT count(*) AS dead_daemons FROM daemon_heartbeat WHERE CASE WHEN next_beat='infinity' THEN false ELSE now()>next_beat+(next_beat-daemon_time) END);");
	$row = pg_fetch_assoc($result);
	if ($row['bad_states']>0) {
		$class="bad";
	} else {
		$class="good";
	}
	echo "<span class=\"$class\"><a href=\"alarms.php\"> ${row['bad_states']} alarms</a></span>\n";
	if ($row['dead_daemons']>0) {
		$class="bad";
	} else {
		$class="good";
	}
	echo "<span class=\"$class\"><a href=\"daemons.php\"> ${row['dead_daemons']} dead daemons</a></span>\n";
	echo "</DIV>\n";
	echo "<DIV id=\"main\">\n";
	echo "<NAV id=\"navigation\">\n";
	echo "<ul>\n";
	echo "<li><a href=\"alarms.php\">Alarms</a>\n";
	echo "<li><a href=\"compounds.php\">Compounds</a>\n";
	echo "<li><a href=\"daemons.php\">Daemons</a>\n";
	echo "<li><a href=\"valueconfig.php?unclaimed\">Unclaimed measurements</a>\n";
	echo "<li><a href=\"videos.php\">Videos</a>\n";
	echo "<li><a href=\"rules.php\">Rules</a>\n";
	$result = pg_query($dbconn,"SELECT * FROM site_links WHERE context='navigation' ORDER BY number;");
	while ($row = pg_fetch_assoc($result)) {
	  echo "<li><a href=\"${row['url']}\">${row['name']}</a>\n";
	}
       	echo "</ul>\n";

	if ((include 'suncalc-php/suncalc.php')==TRUE) {
	   include 'heredef.php';
	   $sc=new AurorasLive\SunCalc(new DateTime(), $latitude, $longitude);
	   $sunTimes = $sc->getSunTimes();
       echo 'True noon: ';echo $sunTimes['solarNoon']->format('H:i');
       echo "<br>\n";
	   echo '<img src="sun.svg" class="fontsized"/>';	
       echo ' &uarr;'; echo  $sunTimes['sunrise']->format('H:i');				
	   echo ' &darr;'; echo  $sunTimes['sunset']->format('H:i');
	   $moonTimes = $sc->getMoonTimes();
       echo "<br>\n";
       $moonIllumination=$sc->getMoonIllumination();
       $pom=$moonIllumination['phase'];
	   echo "<img src=\"moon.php?phase=$pom\" class=\"fontsized\"/>";	
	   echo ' &uarr;'; echo $moonTimes['moonrise']->format('H:i');
	   echo ' &darr;'; echo $moonTimes['moonset']->format('H:i');
       echo "<br>\n";
	}

	echo "</NAV>\n";
	echo "<DIV id=\"content\">\n";

}	
function page_foot() {
	echo "</DIV>\n";
	echo "</DIV>\n";
	echo "</body>\n";
}


?>
