<?php
session_start();
include 'variables.php';
$dbconn = pg_connect($dbstring);
if (!$dbconn) {
	die('Could not connect: ' . pg_last_error());
};

if (! isset($_GET['nodeid'])) {
	die('node id not given');
 }
$nodeid=$_GET['nodeid'];

$result = pg_query($dbconn,"SELECT *, (SELECT value FROM uid_list INNER JOIN uid_configs USING (uid) WHERE description = rule_nodes.nodename AND name = 'name') AS name, (SELECT uid FROM uid_list WHERE description = rule_nodes.nodename) as uid FROM rule_nodes WHERE nodeid=$nodeid;");
$nodedata = pg_fetch_assoc($result);

page_head($dbconn,"rule node $nodeid ${nodedata['nodename']} ${nodedata['name']}");

echo "<h1>node $nodeid ${nodedata['nodename']} ${nodedata['name']}</h1>\n";

echo "This node is a ${nodedata['nodetype']}\n";

if ($nodedata['uid']!="") {
	echo "<a href=\"valueconfig.php?uid=${nodedata['uid']}\">";
	echo "Associated with value ${nodedata['nodename']} ${nodedata['name']}";
	echo "</a>\n";
 }


echo "<h2>Parents</h2>\n";
$result = pg_query($dbconn,"SELECT parent,slot,nodetype,nodename,(SELECT value FROM uid_list INNER JOIN uid_configs USING (uid) WHERE description = rule_nodes.nodename AND name = 'name') AS name FROM rule_node_parents INNER JOIN rule_nodes ON rule_node_parents.parent = rule_nodes.nodeid WHERE rule_node_parents.nodeid=$nodeid;");
echo "<table>\n";
while ($row = pg_fetch_assoc($result)) {
	echo "<tr>\n";
	$h="<a href=\"rulenode.php?nodeid=${row['parent']}\">";
	echo "<td>${row['slot']}</td>\n";
	echo "<td>${row['nodetype']}</td>\n";
	echo "<td>$h${row['nodename']}</a></td>\n";
	echo "<td>$h${row['name']}</a></td>\n";
	echo "</tr>\n";
 }
echo "</table>\n";

echo "<h2>Children</h2>\n";
$result = pg_query($dbconn,"SELECT rule_node_parents.nodeid,slot,nodetype,nodename,(SELECT value FROM uid_list INNER JOIN uid_configs USING (uid) WHERE description = rule_nodes.nodename AND name = 'name') AS name FROM rule_node_parents INNER JOIN rule_nodes ON rule_node_parents.nodeid = rule_nodes.nodeid WHERE rule_node_parents.parent=$nodeid;");
echo "<table>\n";
while ($row = pg_fetch_assoc($result)) {
	echo "<tr>\n";
	$h="<a href=\"rulenode.php?nodeid=${row['nodeid']}\">";
	echo "<td>${row['slot']}</td>\n";
	echo "<td>${row['nodetype']}</td>\n";
	echo "<td>$h${row['nodename']}</a></td>\n";
	echo "<td>$h${row['name']}</a></td>\n";
	echo "</tr>\n";
 }
echo "</table>\n";

echo "<h2>Parameters</h2>\n";
$result = pg_query($dbconn,"SELECT *, (SELECT COUNT(*) FROM rule_config_history WHERE rule_config_history.nodeid=rule_configs.nodeid and rule_config_history.name=rule_configs.name) AS history_items FROM rule_configs WHERE nodeid=$nodeid ORDER BY name COLLATE \"C\";");
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
    echo "<form action=\"configure_value.php?nodeid=$nodeid&name=${row['name']}\" method=\"post\">";
    echo "<td> ${row['name']} </td>\n";
    echo "<td> <input type=\"text\" name=\"value\" value=\"${row['value']}\"> </td>\n";
    echo "<td> <input type=\"text\" name=\"comment\" value=\"${row['comment']}\"> </td>\n";
    echo "<td><input type=\"submit\" value=\"Set\" ></td>\n";     
    echo "<td> ${row['last_change']} </td>\n";
    if ($row['history_items'] >0) {
      echo "<td><a href=\"valueconfig_history.php?nodeid=$nodeid&name=${row['name']}\"> ${row['history_items']}</a> </td>\n";
    } else {
      echo "<td> --- </td>\n";
    }
    echo "</form>\n";
    echo "</tr>\n";
  }
  echo "<tr>\n";	
  echo "<form action=\"configure_value.php?nodeid=$nodeid\" method=\"post\">";
  echo "<td> <input type=\"text\" name=\"name\" value=\"\"> </td>\n";
  echo "<td> <input type=\"text\" name=\"value\" value=\"\"> </td>\n";
  echo "<td> <input type=\"text\" name=\"comment\" value=\"\"> </td>\n";
  echo "<td><input type=\"submit\" value=\"Add\" ></td>\n";     
	echo "<td> --- </td>\n";
	echo "<td> \m/ </td>\n";
  echo "</form>\n";
  echo "</tr>\n";
  echo "</table>\n";
page_foot();
?>
