<?php

session_start();
include 'variables.php';
$dbconn = pg_connect($dbstring);
if (!$dbconn) {
  die('Could not connect: ' . pg_last_error());
};

if (isset($_GET["uid"]) && isset($_GET["id"])) {
  $uid = $_GET["uid"];
  $id = $_GET["id"];
 } else {
  die("uid is required");
 }

if ($_SERVER['REQUEST_METHOD'] == 'POST') {
	$query="INSERT INTO setvalue_requests (uid,request,comment) VALUES (";
	$query.="$uid,";
	$query.=pg_escape_literal("set ${_POST['value']}");
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
    header("Location: ${_SERVER['HTTP_REFERER']}");
  } else {
    http_redirect("setLight.php",array("id"=>$id),true,HTTP_METH_GET);
  }
 }

?>
