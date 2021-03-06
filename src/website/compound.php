<?php
session_start();
include 'variables.php';
$dbconn = pg_connect($dbstring);
if (!$dbconn) {
	die('Could not connect: ' . pg_last_error());
};

if (isset($_GET['id'])) {
  $id=$_GET['id'];
 } else {
  die("i need an id");
 }

$result = pg_query($dbconn,"SELECT * FROM compound_list WHERE id = $id;");
$row = pg_fetch_assoc($result);
if ($row == NULL) {
  die("id $id not found");
 }
page_head($dbconn,"compound ${row['name']}");

echo "<H1>Compound '${row['name']}'</H1>\n";
echo "${row['description']}<br>\n";

echo "<H2>Parents</H2>";
$result = pg_query($dbconn,"select * from compound_families INNER JOIN compound_list ON compound_families.parent_id = compound_list.id WHERE child_id=$id;"); 
echo "<table>\n";
while ($row = pg_fetch_assoc($result)) {
  echo "<tr>\n";
  echo "<td> <a href=\"compound.php?id=${row['parent_id']}\">${row['name']}</a> </td>\n";
  echo "<td> ${row['child_name']} </td>\n";
  echo "<td> ${row['description']} </td>\n";
  echo "</tr>\n";
 }
echo "</table>\n";

echo "<H2>Children</H2>";
$result = pg_query($dbconn,"select * from compound_families INNER JOIN compound_list ON compound_families.child_id = compound_list.id WHERE parent_id = $id;"); 
echo "<table>\n";
while ($row = pg_fetch_assoc($result)) {
  echo "<tr>\n";
  echo "<td> <a href=\"compound.php?id=${row['child_id']}\">${row['child_name']}</a> </td>\n";
  echo "<td> ${row['name']} </td>\n";
  echo "<td> ${row['description']} </td>\n";
  echo "</tr>\n";
 }
echo "</table>\n";


echo "<H2>Values</H2>";
echo "<form action=dressed_graph.php method=get>\n";

$result = pg_query($dbconn,"SELECT *, (SELECT value FROM uid_configs WHERE name='unit' AND uid_configs.uid = compound_uids.uid) AS unit FROM compound_uids INNER JOIN uid_list USING (uid) WHERE id = $id;"); 
echo "<table>\n";

if (pg_num_rows($result) < 5) {
  $checkstate=" checked ";
}

while ($row = pg_fetch_assoc($result)) {
	$uid=$row['uid'];
	$result2 = pg_query($dbconn,"SELECT valid_from,reason,typename,explanation,class FROM uid_states INNER JOIN state_types USING (type) WHERE uid = $uid;");
  $state=pg_fetch_assoc($result2);
  echo "<tr>\n";
  echo "<td> <a href=\"valueconfig.php?uid=${row['uid']}\">${row['child_name']} </a></td>\n";
  $r2 = pg_query($dbconn,"SELECT * FROM ${row['data_table']} WHERE uid=${row['uid']} ORDER BY time desc limit 1;");
  $v=pg_fetch_assoc($r2);
  echo "<td class=\"${state['class']}\"> ${v['value']}</td>";
	echo "<td class=\"${state['class']}\"> ${row['unit']}</td>\n";
  echo "<td class=\"${state['class']}\"> <a href=\"dressed_graph.php?uid=${row['uid']}\">${v['time']}</a> </td>\n";
	echo "<td><input type=\"checkbox\" name=\"u$uid\" $checkstate></td>\n";
  echo "</tr>\n";
 }
echo "</table>\n";
echo "graph since <input type=\"submit\" value=\"yesterday\" name=\"starttime\">\n";
echo "</form>\n";

page_foot();

?>
