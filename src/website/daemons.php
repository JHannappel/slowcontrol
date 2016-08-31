<?php
session_start();
include 'variables.php';
$dbconn = pg_connect($dbstring);
if (!$dbconn) {
	die('Could not connect: ' . pg_last_error());
};

page_head($dbconn,"daemons");

if (isset($_GET['daemonid'])) {
	$condition="WHERE daemonid IN (".get_int_list('daemonid').")";
 } else {
	$condition="";
 }
$result = pg_query($dbconn,"SELECT *,(SELECT string_agg(format('%s',uid_daemon_connection.uid),',') FROM uid_daemon_connection WHERE uid_daemon_connection.daemonid = daemon_list.daemonid) AS uids,  now()<next_beat+(next_beat-daemon_time) AS alive FROM daemon_list INNER JOIN daemon_heartbeat USING (daemonid) $condition ORDER BY description;");
echo "<table>\n";
while ($row = pg_fetch_assoc($result)) {
	$daemonid=$row['daemonid'];
	echo "<tr>\n";
	if ($row['alive']=='t') {
		$class="good";
	} else {
		$class="bad";
	}
	echo "<td class=\"$class\">${row['description']}</td>\n";
	echo "<td class=\"$class\">${row['name']}</td>\n";
	echo "<td class=\"$class\">${row['host']}</td>\n";
	echo "<td class=\"$class\">${row['daemon_time']}</td>\n";
	echo "<td><a href=\"valueconfig.php?uid=${row['uids']}\">values</a></td>\n";
	echo "</tr>\n";
 }
echo "</table>\n";
page_foot();
?>
