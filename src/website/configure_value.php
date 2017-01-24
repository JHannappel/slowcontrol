<?php

session_start();
include 'variables.php';
$dbconn = pg_connect($dbstring);
if (!$dbconn) {
  die('Could not connect: ' . pg_last_error());
};

if (isset($_GET["uid"])) {
  $id = $_GET["uid"];
  $idcolumn="uid";
  $table="uid_configs";
 } else if (isset($_GET["nodeid"])) {
  $id = $_GET["nodeid"];
  $idcolumn="nodeid";
  $table="rule_configs";
 } else {
  die("uid or node id is required");
 }

if ($_SERVER['REQUEST_METHOD'] == 'POST') {
  if (isset($_GET["name"])) {
    $query="UPDATE $table SET ";
    $query.="value=".pg_escape_literal($_POST['value']);
    $query.=",comment=".pg_escape_literal($_POST['comment']);
    $query.=",last_change=now()";
    $query.="WHERE $idcolumn=$id AND name=".pg_escape_literal($_GET['name']).";";
  } else {
    $query="INSERT INTO $table ($idcolumn,name,value,comment) VALUES (";
    $query.="$id";
    $query.=",".pg_escape_literal($_POST['name']);
    $query.=",".pg_escape_literal($_POST['value']);
    $query.=",".pg_escape_literal($_POST['comment']);
    $query.=");";
  }
  pg_query($dbconn,$query);
  if (isset($_SERVER['HTTP_REFERER'])) {
    if ($_SERVER['SERVER_PROTOCOL'] == 'HTTP/1.1') {
      if (php_sapi_name() == 'cgi') {
	header('Status: 303 See Other');
      } else {
	header('HTTP/1.1 303 See Other');
      }
    }
    header("Location: ${_SERVER['HTTP_REFERER']}#$id");
  } else {
    http_redirect("valueconfig.php",array($idcolumn=>"$id"),true,HTTP_METH_GET);
  }
 }

?>
