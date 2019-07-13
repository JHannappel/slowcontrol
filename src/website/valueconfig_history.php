<?php
session_start();
include 'variables.php';
$dbconn = pg_connect($dbstring);
if (!$dbconn) {
	die('Could not connect: ' . pg_last_error());
};

page_head($dbconn,"value config history");

if (isset($_GET["uid"])) {
	$ids=explode(",",$_GET["uid"]);
	$table="uid_config_history";
	$idcolumn="uid";
 }	else if (isset($_GET["nodeid"])) {
	$ids=explode(",",$_GET["nodeid"]);
	$table="rule_config_history";
	$idcolumn="nodeid";
 }	else {
	die("uid/nodeid required");
 }
$name=$_GET['name'];
foreach ($ids as $id) {
  echo "<H1>$idcolumn $id config history of property $name</H1>\n";
  echo "<table>\n";
  echo "<thead>\n";
  echo "<tr>\n";
  echo "<th> value </th>\n";
  echo "<th> comment </th>\n";
  echo "<th> valid from </th>\n";     
  echo "<th> valid to </th>\n";
  echo "</tr>\n";
  echo "<tbody>\n";
  $result = pg_query($dbconn,"SELECT * FROM $table WHERE $idcolumn=$id AND name='$name' ORDER BY valid_from DESC;");
  while ($row = pg_fetch_assoc($result)) {
    echo "<tr>\n";
    echo "<td> ${row['value']} </td>\n";
    echo "<td> ${row['comment']} </td>\n";
    echo "<td> ${row['valid_from']} </td>\n";
    echo "<td> ${row['valid_to']} </td>\n";
    echo "</tr>\n";
  }
  echo "<tr>\n";	
  echo "</table>\n";
}
page_foot();
?>
