<?php
session_start();
include 'variables.php';
$dbconn = pg_connect($dbstring);
if (!$dbconn) {
	die('Could not connect: ' . pg_last_error());
};

$uids=explode(",",$_GET["uid"]);
foreach ($uids as $uid) {
  $result = pg_query($dbconn,"SELECT uid, data_table, description, (SELECT value FROM uid_configs WHERE uid=$uid AND name='name') AS name FROM uid_list WHERE uid=$uid;");
  $row = pg_fetch_assoc($result);
  echo "<H1>Uid ${row['uid']} '${row['description']}' '${row['name']}'</H1>\n";
  echo "resides in ${row['data_table']}<br>\n";
  $result = pg_query($dbconn,"SELECT *, (SELECT COUNT(*) FROM uid_config_history WHERE uid_config_history.uid=uid_configs.uid and uid_config_history.name=uid_configs.name) AS history_items FROM uid_configs WHERE uid=$uid ORDER BY name COLLATE \"C\";");
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
  while ($row = pg_fetch_assoc($result)) {
    echo "<tr>\n";
    echo "<form action=\"configure_value.php?uid=$uid&name=${row['name']}\" method=\"post\">";
    echo "<td> ${row['name']} </td>\n";
    echo "<td> <input type=\"text\" name=\"value\" value=\"${row['value']}\"> </td>\n";
    echo "<td> <input type=\"text\" name=\"comment\" value=\"${row['comment']}\"> </td>\n";
    echo "<td><input type=\"submit\" value=\"Set\" ></td>\n";     
    echo "<td> ${row['last_change']} </td>\n";
    if ($row['history_items'] >0) {
      echo "<td><a href=\"valueconfig_history.php?uid=$uid&name=${row['name']}\"> ${row['history_items']}</a> </td>\n";
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
  echo "<td> ${row['last_change']} </td>\n";
  echo "</form>\n";
  echo "</tr>\n";
  echo "</table>\n";
}
?>
