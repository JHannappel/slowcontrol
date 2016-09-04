<?php

session_start();
include 'variables.php';
$dbconn = pg_connect($dbstring);
if (!$dbconn) {
  die('Could not connect: ' . pg_last_error());
};

if (isset($_GET["uid"])) {
  $uid = $_GET["uid"];
 } else {
  die("uid is required");
 }

if ($_SERVER['REQUEST_METHOD'] == 'POST') {
	$query="INSERT INTO setvalue_requests (uid,request,comment) VALUES (";
	$query.="$uid,";
	$query.=pg_escape_literal($_POST['request']);
	$query.=",";
	$query.=pg_escape_literal($_POST['comment']);
	$query.=");";
  pg_query($dbconn,$query);
  if (isset($_SERVER['HTTP_REFERER'])) {
    if ($_SERVER['SERVER_PROTOCOL'] == 'HTTP/1.1') {
      if (php_sapi_name() == 'cgi') {
	header('Status: 303 See Other');
      } else {
	header('HTTP/1.1 303 See Other');
      }
    }
    header("Location: ${_SERVER['HTTP_REFERER']}#$uid");
  } else {
    http_redirect("valueconfig.php",array("uid"=>"$uid"),true,HTTP_METH_GET);
  }
 }

?>
