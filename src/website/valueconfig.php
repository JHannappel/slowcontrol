<?php
session_start();
include 'variables.php';
$dbconn = pg_connect($dbstring);
if (!$dbconn) {
	die('Could not connect: ' . pg_last_error());
};
if (isset($_GET['uid'])) {
	$condition="WHERE uid in (${_GET['uid']})";
 } else {
	$condition="";
 }
$result = pg_query($dbconn,"SELECT uid, data_table, description, (SELECT value FROM uid_configs WHERE uid_configs.uid=uid_list.uid AND name='name') AS name FROM uid_list $condition;");

while ($row = pg_fetch_assoc($result)) {
	$uid=$row['uid'];
  echo "<H1>Uid ${row['uid']} '${row['description']}' '${row['name']}'</H1>\n";
  echo "resides in ${row['data_table']}<br>\n";
  $result2 = pg_query($dbconn,"SELECT *, (SELECT COUNT(*) FROM uid_config_history WHERE uid_config_history.uid=uid_configs.uid and uid_config_history.name=uid_configs.name) AS history_items FROM uid_configs WHERE uid=$uid ORDER BY name COLLATE \"C\";");
  echo "<table>\n";
  echo "<thead>\n";
  echo "<tr>\n";
  echo "<th> cfg variable name </th>\n";
  echo "<th> value </th>\n";
  echo "<th> comment </th>\n";
  echo "<th> action </th>\n";     
  echo "<th> last change </th>\n";
  echo "<th> history </th>\n";
  echo "</tr>\n";
  echo "<tbody>\n";
  while ($row2 = pg_fetch_assoc($result2)) {
    echo "<tr>\n";
    echo "<form action=\"configure_value.php?uid=$uid&name=${row2['name']}\" method=\"post\">";
    echo "<td> ${row2['name']} </td>\n";
    echo "<td> <input type=\"text\" name=\"value\" value=\"${row2['value']}\"> </td>\n";
    echo "<td> <input type=\"text\" name=\"comment\" value=\"${row2['comment']}\"> </td>\n";
    echo "<td><input type=\"submit\" value=\"Set\" ></td>\n";     
    echo "<td> ${row2['last_change']} </td>\n";
    if ($row2['history_items'] >0) {
      echo "<td><a href=\"valueconfig_history.php?uid=$uid&name=${row2['name']}\"> ${row2['history_items']}</a> </td>\n";
    } else {
      echo "<td> --- </td>\n";
    }
    echo "</form>\n";
    echo "</tr>\n";
  }
  echo "<tr>\n";	
  echo "<form action=\"configure_value.php?uid=$uid\" method=\"post\">";
  echo "<td> <input type=\"text\" name=\"name\" value=\"\"> </td>\n";
  echo "<td> <input type=\"text\" name=\"value\" value=\"\"> </td>\n";
  echo "<td> <input type=\"text\" name=\"comment\" value=\"\"> </td>\n";
  echo "<td><input type=\"submit\" value=\"Add\" ></td>\n";     
  echo "<td> ${row2['last_change']} </td>\n";
  echo "</form>\n";
  echo "</tr>\n";
  echo "</table>\n";

	echo "<h3>Compounds containing this measurement</h3>\n";
	$result2 = pg_query($dbconn,"SELECT * FROM compound_list INNER JOIN compound_uids USING (id) WHERE uid=$uid;");
	while ($row2 = pg_fetch_assoc($result2)) {
		echo "<a href=\"compound.php?id=${row2['id']}\">${row2['name']}";
		if ($row2['description'] != $row2['name']) {
			echo "${row2['description']}";
		}
		echo "</a></br>\n";
	}
	echo "<h3>Current value</h3>\n";
	$result2 = pg_query($dbconn,"SELECT * FROM ${row['data_table']} WHERE uid=${row['uid']} ORDER BY time desc limit 1;");
  $row2=pg_fetch_assoc($result2);
  echo "${row2['value']} measured at ${row2['time']}</br>";
	echo "<a href=\"graph.php?uid=${row['uid']}\">graph</a>\n";
	echo "<a href=\"graph.php?uid=${row['uid']}&starttime='today'\">graph since today</a>\n";
	echo "<a href=\"graph.php?uid=${row['uid']}&starttime='yesterday'\">graph since yesterday</a>\n";
 }
?>
