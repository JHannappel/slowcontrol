<?php
session_start();
include 'variables.php';
$uids=get_int_list("uid");
$uids=get_uid_list($uids);
$starttime="yesterday";
if (isset($_GET['starttime'])) {
   $starttime=$_GET['starttime'];
}
$dbconn = pg_connect($dbstring);
if (!$dbconn) {
	die('Could not connect: ' . pg_last_error());
};

page_head($dbconn,"dressed graph");

$graphurl="graph.php?uid=$uids&starttime='$starttime'";
echo "<a href=\"$graphurl\"><image src=\"$graphurl\"></a>\n";

$result = pg_query($dbconn,"SELECT TIMESTAMP '$starttime' + tdiff AS t, name FROM time_intervals WHERE TIMESTAMP '$starttime' + tdiff < now() ORDER BY tdiff;");
while ($row = pg_fetch_assoc($result)) {
      echo "<a href=\"dressed_graph.php?uid=$uids&starttime=${row['t']}\">start ${row['name']}</a>\n";
}  

page_foot();
?>
