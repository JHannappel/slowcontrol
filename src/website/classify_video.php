<?php
session_start();
include 'variables.php';
$dbconn = pg_connect($dbstring);
if (!$dbconn) {
	die('Could not connect: ' . pg_last_error());
};
page_head($dbconn,"Slowcontrol");

$video=$_GET['video'];
$videodir=dirname($video);
$filedir="/data".$videodir;

$videos=scandir($filedir);//."/*.mp4");
$index=array_search(basename($video),$videos);
if ($_SERVER['REQUEST_METHOD'] == 'POST') {
   if (isset($_POST['newdir'])) {
      mkdir("/data/video/sort/".$_POST['newdir']);
      link("/data/".$video,"/data/video/sort/".$_POST['newdir']."/".basename($video));
   }
   if (isset($_POST['olddir'])) {
      link("/data/".$video,"/data/video/sort/".$_POST['olddir']."/".basename($video));
   }
   if (isset($_POST['deldir'])) {
      unlink("/data/video/sort/".$_POST['deldir']."/".basename($video));
   }
}

echo "<video src=\"$video\" autoplay controls>$video</video>\n";

$d = dir("/data/video/sort/");
while(false !== ($e = $d->read())) {
  if ($e[0] != ".") {
    echo "<form action=\"classify_video.php?video=$video\" method=\"post\">\n";
    if (file_exists("/data/video/sort/".$e."/".basename($video))) {
      $s = stat("/data/video/sort/".$e."/".basename($video));
      if ($s['nlink']==1) {
	echo "only exists in $e\n";
      } else {
	echo "Remove from <input type=\"submit\" name=\"deldir\" value=\"$e\" >, has ${s['nlink']} places to live\n";}
    } else {
      echo "Add to <input type=\"submit\" name=\"olddir\" value=\"$e\" >\n";
    }
    echo "</form>\n";
  }
}
echo "<form action=\"classify_video.php?video=$video\" method=\"post\">\n";
echo "<input type=\"text\" name=\"newdir\" value=\"noname\" width=30>\n";
 echo "<input type=\"submit\" value=\"new class\" >\n";
 echo "</form>\n";

for ($i=$index-1; $i>=0; $i--) {
  $file=$filedir."/".$videos[$i];
  if (file_exists($file)) {
    $uc="";
    if (isset($_GET['unclassified'])) {
      $uc="&unclassified";
      $s=stat($file);
      if ($s['nlink']!=1) {
	continue;
      }
    }
    echo "<a href=\"classify_video.php?video=$videodir/${videos[$i]}$uc\">previous</a>\n";
    break;
  }
 }
$nvids=count($videos);
for ($i=$index+1; $i<$nvids; $i++) {
  $file=$filedir."/".$videos[$i];
  if (file_exists($file)) {
    $uc="";
    if (isset($_GET['unclassified'])) {
      $uc="&unclassified";
      $s=stat($file);
      if ($s['nlink']!=1) {
	continue;
      }
    }
    echo "<a href=\"classify_video.php?video=$videodir/${videos[$i]}$uc\">next</a>\n";
    break;
  }
 }

page_foot();
?>
