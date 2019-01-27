<?php
session_start();
include 'variables.php';
$dbconn = pg_connect($dbstring);
if (!$dbconn) {
	die('Could not connect: ' . pg_last_error());
};

page_head($dbconn,"paveview");

if (isset($_GET['uid'])) {
	$condition="WHERE uid IN (".get_int_list('uid').")";
 } else if (isset($_GET['unclaimed'])) {
	$condition="WHERE uid NOT IN (SELECT uid FROM compound_uids)";
 } else {
	$condition="";
 }
$result = pg_query($dbconn,"SELECT uid, data_table, description, is_write_value, (SELECT value FROM uid_configs WHERE uid_configs.uid=uid_list.uid AND name='name') AS name, (SELECT value FROM uid_configs WHERE uid_configs.uid=uid_list.uid AND name='unit') AS unit FROM uid_list $condition;");

if (pg_num_rows($result) < 5) {
  $checkstate=" checked ";
}
echo "<form action=dressed_graph.php method=get>\n";

echo "<div class=\"grid-container\">\n";


while ($row = pg_fetch_assoc($result)) {
	$uid=$row['uid'];
	$result2 = pg_query($dbconn,"SELECT * FROM ${row['data_table']} WHERE uid=$uid ORDER BY time desc limit 1;");
  $value=pg_fetch_assoc($result2);
	$result2 = pg_query($dbconn,"SELECT valid_from,reason,typename,explanation,class FROM uid_states INNER JOIN state_types USING (type) WHERE uid = $uid;");
  $state=pg_fetch_assoc($result2);
	echo "<div class=\"grid-item\">\n";
	echo "<span> <a href=\"valueconfig.php?uid=$uid\">";
	if ($row['name']=="") {
		echo $row['description'];
	}	else {
		echo $row['name'];
	}
	echo "</a></span></br>\n";
	echo "<span class=\"${state['class']}\"><a href=\"dressed_graph.php?uid=$uid\"> ${value['value']} ${row['unit']} </a></span>";
	echo "<span><input type=\"checkbox\" name=\"u$uid\" $checkstate></span>\n";
	echo "<span class=\"${state['class']}\">${state['typename']}</span>\n";
	
 echo "</div>\n";
 }
echo "</div>\n";
echo "<input type=\"submit\" value=\"yesterday\" name=\"starttime\">\n";
echo "</form>\n";
page_foot();
?>
