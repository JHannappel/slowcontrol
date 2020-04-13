<?php

session_start();
include 'variables.php';
$dbconn = pg_connect($dbstring);
if (!$dbconn) {
  die('Could not connect: ' . pg_last_error());
};

if (isset($_GET["id"])) {
  $id = $_GET["id"];
 } else {
  die("uid is required");
 }
	echo "<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.01//EN\"\n";
	echo "                 \"http://www.w3.org/TR/html4/strict.dtd\">\n";
	echo "<HTML>\n";
	echo "<HEAD>\n";
	echo "<meta http-equiv=\"Content-Type\" content=\"text/html; charset=ISO-8859-1\"> \n";
	echo "<TITLE>$name</TITLE>\n";
	echo "<LINK REV=MADE HREF=\"mailto:juerge@juergen-hannappel.de\"> \n";
	echo "<link type=\"text/css\" rel=\"stylesheet\" href=\"style.css\">\n";
	echo "</HEAD>\n";
	echo "\n";



$result = pg_query($dbconn,"SELECT * FROM compound_list WHERE id=$id;");
$compound = pg_fetch_assoc($result);
echo "<H1> ${compound['name']} </H1>\n";

function setField($id,$uid,$value,$label,$max) {
    if ($value < 0) {
        $value = 0;
    }
    if ($value > $max) {
        $value = $max;
    }
    echo "  <td class=\"lightset\">\n";
    echo "   <form action=\"set_light.php?uid=$uid&id=$id}\" method=\"post\">\n";
    echo "    <input type=\"hidden\" name=\"value\" value=\"$value\">\n";
    echo "    <input type=\"hidden\" name=\"comment\" value=\"webIf\">\n";
    echo "    <input class=\"lightset\" type=\"submit\" value=\"$label\" >\n"; 
    echo "   </form>\n";
    echo "  </td>\n";
}


echo "<table class=\"light\">\n";
echo "<tbody>\n";
$result = pg_query($dbconn,"SELECT * FROM compound_uids WHERE id=$id;");
while ($row = pg_fetch_assoc($result)) {
    $r1 = pg_query($dbconn,"SELECT * FROM uid_list WHERE uid=${row['uid']};");
    $rr1 = pg_fetch_assoc($r1);
    $r2 = pg_query($dbconn,"SELECT * FROM ${rr1['data_table']} WHERE uid=${row['uid']} ORDER BY time desc limit 1;");
    $rr2 = pg_fetch_assoc($r2);
    $value = $rr2['value'];
    if ($row['child_name']=='autoHue') {
        $autoHue=$value;
        $autoHueUid=$row['uid'];
        continue;
    } elseif ($row['child_name']=='hue') {
        $max=360;
        $step=30;
        $hueUid=$row['uid'];
    } else {
        $max=1;
        $step=0.1;
    }
    if ($row['child_name']=='red') { $red=$value; }
    if ($row['child_name']=='green') { $green=$value; }
    if ($row['child_name']=='blue') { $blue=$value; }
    if ($row['child_name']=='white') { $white=$value; }
    echo " <tr>\n";
    setField($id,$row['uid'],$value - $step,"&lArr;",$max);
    setField($id,$row['uid'],$value - $step*0.1,"&larr;",$max);
    echo "  <td>${row['child_name']}</td>\n";
    echo "  <td>$value</td>\n";
    setField($id,$row['uid'],$value + $step*0.1,"&rarr;",$max);
    setField($id,$row['uid'],$value + $step,"&rArr;",$max);
    echo " </tr>\n";
}
echo "</tbody>\n";
echo "</table>\n";

$red=($red+$white)/2;
$green=($green+$white)/2;
$blue=($blue+$white)/2;
$max=max($red,$green,$blue);
$red/=$max;
$green/=$max;
$blue/=$max;

echo "<form action=\"set_light.php?uid=$hueUid&id=$id}\" method=\"post\">\n";
echo " <input class=\"color\" type=\"color\" name=\"colour\" value=\"#";
printf('%02X%02X%02X',intval($red*255),intval($green*255),intval($blue*255));
echo "\">\n";
echo " <input class=\"lightset\" type=\"submit\" value=\"Set Colour\" >\n"; 
echo "</form>\n";

if ($autoHue == 't') {
    $ahv=0;
    $aht='Off';
} else {
    $ahv=1;
    $aht='On';
}
echo "<form action=\"set_value.php?uid=$autoHueUid\" method=\"post\">\n";
echo " <input type=\"hidden\" name=\"request\" value=\"set $ahv\">\n";
echo " <input type=\"hidden\" name=\"comment\" value=\"webIf\">\n";
echo " <input class=\"lightset\" type=\"submit\" value=\"Switch AutoHue $aht\" >\n"; 
echo "</form>\n";

?>
