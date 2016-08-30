<?php
session_start();
include 'variables.php';
$dbconn = pg_connect($dbstring);
if (!$dbconn) {
	die('Could not connect: ' . pg_last_error());
};

page_head($dbconn,"alarms");

$result = pg_query($dbconn,"SELECT * FROM uid_states INNER JOIN uid_list USING (uid) INNER JOIN state_types USING (type) WHERE type > 1 ORDER BY valid_from DESC;");
echo "<table>\n";
while ($row = pg_fetch_assoc($result)) {
	$uid=$row['uid'];
	echo "<tr>\n";
	echo "<td> <a href=\"valueconfig.php?uid=${row['uid']}\">${row['description']}</a> </td>\n";
	echo "<td> ${row['typename']} </td>\n";
	echo "<td> ${row['reason']} </td>\n";
	echo "<td> ${row['valid_from']} </td>\n";
	echo "<td> ${row['explanation']} </td>\n";
  echo "<tr>\n";	
 }
echo "</table>\n";

page_foot();
?>
