<?php
session_start();
include 'variables.php';
$dbconn = pg_connect($dbstring);
if (!$dbconn) {
	die('Could not connect: ' . pg_last_error());
};

page_head($dbconn,"rules");

$graphurl="rulegraph.php";
echo "<a href=\"$graphurl\"><image src=\"$graphurl\"></a>\n";


page_foot();
?>
