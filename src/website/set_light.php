<?php

session_start();
include 'variables.php';
$dbconn = pg_connect($dbstring);
if (!$dbconn) {
  die('Could not connect: ' . pg_last_error());
};

if (isset($_GET["uid"]) && isset($_GET["id"])) {
  $uid = $_GET["uid"];
  $id = $_GET["id"];
 } else {
  die("uid is required");
 }
error_log($_POST['colour']);
if ($_SERVER['REQUEST_METHOD'] == 'POST') {
    if (isset($_POST['colour'])) {
        list($r, $g, $b) = sscanf($_POST['colour'], "#%02x%02x%02x");
        $r /= 255;
        $g /= 255;
        $b /= 255;
        $maxRGB=max($r,$g,$b);
        $minRGB=min($r,$g,$b);
        $c=$maxRGB-$minRGB;
        if ($c==0) {
            $value=0;
        } else {
            if ($r == $minRGB) {
                $value = 3 - (($g - $b) / $c);
            } elseif ($b==$minRGB) {
                $value = 1 - (($r - $g) / $c);
            } else {
                $value = 5 - (($b - $r) / $c);
            }
            $value *= 60;
        }
    } else {
        $value = $_POST['value'];
    }
    $query="INSERT INTO setvalue_requests (uid,request,comment) VALUES (";
    $query.="$uid,";
    $query.=pg_escape_literal("set $value");
    $query.=",";
    $query.=pg_escape_literal($_POST['comment']);
    $query.=");";

    pg_query($dbconn,$query);
    if (isset($_SERVER['HTTP_REFERER'])) {
        if ($_SERVER['SERVER_PROTOCOL'] == 'HTTP/1.1') {
            if (php_sapi_name() == 'cgi') {
                header('Status: 303 See Other');
            } else {
                header('HTTP/1.1 303 See Other');
            }
        }
        header("Location: ${_SERVER['HTTP_REFERER']}");
    } else {
        http_redirect("setLight.php",array("id"=>$id),true,HTTP_METH_GET);
    }
}

?>
