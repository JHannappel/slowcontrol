<?php
session_start();
include 'variables.php';
$dbconn = pg_connect($dbstring);
if (!$dbconn) {
	die('Could not connect: ' . pg_last_error());
};


if (isset($_GET['ext'])) {
  $externalUrl=$_GET['ext'];
 } else {
  die("i need an url");
 }

page_head($dbconn,"external $externalUrl");


echo "<iframe width=\"100%\" height=\"600\" title=\"external link $externalUrl\" sandbox src=\"$externalUrl\"></iframe>\n";


page_foot();
?>
