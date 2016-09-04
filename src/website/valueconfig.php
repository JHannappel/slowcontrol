<?php
session_start();
include 'variables.php';
$dbconn = pg_connect($dbstring);
if (!$dbconn) {
	die('Could not connect: ' . pg_last_error());
};

page_head($dbconn,"valueconfig");

if (isset($_GET['uid'])) {
	$condition="WHERE uid IN (".get_int_list('uid').")";
 } else if (isset($_GET['unclaimed'])) {
	$condition="WHERE uid NOT IN (SELECT uid FROM compound_uids)";
 } else {
	$condition="";
 }
$result = pg_query($dbconn,"SELECT uid, data_table, description, is_write_value, (SELECT value FROM uid_configs WHERE uid_configs.uid=uid_list.uid AND name='name') AS name FROM uid_list $condition;");

while ($row = pg_fetch_assoc($result)) {
	$uid=$row['uid'];
  echo "<H1 id=$uid>Uid ${row['uid']} '${row['description']}' '${row['name']}'</H1>\n";
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
	$unit = "";
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
		if ($row2['name']=="unit") {
			$unit = $row2['value'];
		}
  }
  echo "<tr>\n";	
  echo "<form action=\"configure_value.php?uid=$uid\" method=\"post\">";
  echo "<td> <input type=\"text\" name=\"name\" value=\"\"> </td>\n";
  echo "<td> <input type=\"text\" name=\"value\" value=\"\"> </td>\n";
  echo "<td> <input type=\"text\" name=\"comment\" value=\"\"> </td>\n";
  echo "<td><input type=\"submit\" value=\"Add\" ></td>\n";     
	echo "<td> --- </td>\n";
	echo "<td> \m/ </td>\n";
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
  echo "${row2['value']} $unit measured at ${row2['time']}</br>";
	echo "<a href=\"graph.php?uid=${row['uid']}\">graph</a>\n";
	echo "<a href=\"graph.php?uid=${row['uid']}&starttime='today'\">graph since today</a>\n";
	echo "<a href=\"graph.php?uid=${row['uid']}&starttime='yesterday'\">graph since yesterday</a>\n";
	
	echo "<table>\n";
	$result2 = pg_query($dbconn,"SELECT uid,type,valid_from,timestamp 'infinity' AS valid_to,reason,typename,explanation FROM uid_states INNER JOIN state_types USING (type) WHERE uid = $uid UNION SELECT uid,type,valid_from,valid_to,reason,typename,explanation FROM uid_state_history INNER JOIN state_types USING (type) WHERE uid = $uid ORDER BY valid_from DESC LIMIT 5;");
	while ($row2=pg_fetch_assoc($result2)) {
		echo "<tr>\n";
		echo "<td>${row2['typename']}</td>\n";
		echo "<td>${row2['explanation']}</td>\n";
		echo "<td>${row2['reason']}</td>\n";
		echo "<td>${row2['valid_from']}</td>\n";
		echo "<td>${row2['valid_to']}</td>\n";
		echo "</tr>\n";
	}
	echo "</table>\n";

	if ($row['is_write_value'] == "t") {
		echo "<form action=\"set_value.php?uid=$uid\" method=\"post\">";
		echo " request: <input type=\"text\" name=\"request\" value=\"\">\n";
		echo " comment: <input type=\"text\" name=\"comment\" value=\"\">\n";
		echo "<input type=\"submit\" value=\"Set\" >\n";     
		echo "</form>\n";
	}

	$result2 = pg_query($dbconn,"SELECT *, CASE WHEN response_time IS NOT NULL THEN extract('epoch' from response_time - request_time) ELSE 0 END AS delay FROM setvalue_requests WHERE uid = $uid ORDER BY request_time DESC limit 5;");
	echo "<table>\n";
	while ($row2=pg_fetch_assoc($result2)) {
		echo "<tr>\n";
		echo "<td>${row2['request']}</td>\n";
		echo "<td>${row2['response']}</td>\n";
		echo "<td>${row2['comment']}</td>\n";
		echo "<td>${row2['request_time']}</td>\n";
		echo "<td>${row2['response_time']}</td>\n";
		echo "<td>${row2['delay']}</td>\n";
		echo "</tr>\n";
	}
	echo "</table>\n";
 }

page_foot();
?>
