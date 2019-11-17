<?php
session_start();
include 'variables.php';
$dbconn = pg_connect($dbstring);
if (!$dbconn) {
	die('Could not connect: ' . pg_last_error());
};

page_head($dbconn,"compounds");

if (isset($_GET['id'])) {
  $ids=$_GET['id'];
 } else {
  $result = pg_query($dbconn,"select string_agg(format('%s',id),',') as ids from compound_list where id not in (select child_id from compound_families);");
  $row = pg_fetch_assoc($result);
  $ids=$row['ids'];
}

echo "<H1>Compounds</H1>\n";

echo "<table>\n";
echo "<thead>\n";
echo "<tr>\n";
echo "<th> name </th>\n";
echo "<th> description </th>\n";
echo "<th> uids </th>\n";
echo "<th> children </th>\n";     
echo "<th> parents </th>\n";     
echo "</tr>\n";
echo "<tbody>\n";

$result = pg_query($dbconn,"SELECT *,(SELECT string_agg(format('%s',compound_uids.uid),',') FROM compound_uids WHERE compound_uids.id=compound_list.id) AS uids, (SELECT string_agg(format('%s',compound_families.child_id),',') FROM compound_families WHERE compound_families.parent_id=compound_list.id) AS children, (SELECT string_agg(format('%s',compound_families.parent_id),',') FROM compound_families WHERE compound_families.child_id=compound_list.id) AS parents FROM compound_list WHERE id IN ($ids);");
while ($row = pg_fetch_assoc($result)) {
  echo "<tr>\n";
  echo "<td><a href=\"compound.php?id=${row['id']}\"> ${row['name']} </a></td>\n";
  echo "<td> ${row['description']} </td>\n";
  if ($row['uids'] != '') {
      echo "<td><a href=\"valuelist.php?uid=${row['uids']}\"> ${row['uids']}</a> </td>\n";
  } else {
    echo "<td> --- </td>\n";
  }
  if ($row['children'] != '') {
      echo "<td><a href=\"compounds.php?id=${row['children']}\"> ${row['children']}</a> </td>\n";
  } else {
    echo "<td> --- </td>\n";
  }
  if ($row['parents'] != '') {
      echo "<td><a href=\"compounds.php?id=${row['parents']}\"> ${row['parents']}</a> </td>\n";
  } else {
    echo "<td> --- </td>\n";
  }
  echo "</tr>\n";
 }
echo "</table>\n";

page_foot();
?>
