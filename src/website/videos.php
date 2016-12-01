<?php
session_start();
include 'variables.php';
$dbconn = pg_connect($dbstring);
if (!$dbconn) {
	die('Could not connect: ' . pg_last_error());
};
page_head($dbconn,"Slowcontrol");

if (isset($_GET['dir'])) {
   $videodir=$_GET['dir'];
} else {
  $videodir="/video/camera1/";
}

$videos=scandir("/data".$videodir);

echo "<h1>Unclassified Videos in $videodir</h1>\n";
foreach ($videos as $video) {
  $s=stat("/data".$videodir."/".$video);
  if ($s['nlink']==1) {
    echo "<a href=\"classify_video.php?video=$videodir/$video&unclassified\">$video<a><br>\n";
  }
}

echo "<h1>Videos in $videodir</h1>\n";
foreach ($videos as $video) {
  echo "<a href=\"classify_video.php?video=$videodir/$video\">$video<a>  \n";
}


echo "<h1>Sortet Videos</h1>\n";
$d = dir("/data/video/sort/");
while(false !== ($e = $d->read())) {
  if ($e[0] != ".") {
     echo "<a href=\"videos.php?dir=/video/sort/$e\">$e<a><br>\n";
  }
}
page_foot();
?>
