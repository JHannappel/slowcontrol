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
if (isset($_GET['unclassified'])) {
  $uc="&unclassified";
 } else {
  $uc="";
 }
    $videos=scandir($filedir);//."/*.mp4");
$index=array_search(basename($video),$videos);
if ($_SERVER['REQUEST_METHOD'] == 'POST') {
   if (isset($_POST['newdir'])) {
      mkdir("/data/video/sort/".$_POST['newdir']);
      link("/data/".$video,"/data/video/sort/".$_POST['newdir']."/".basename($video));
   }
   if (isset($_POST['olddir'])) {
      link("/data/".$video,"/data/video/sort/".$_POST['olddir']."/".basename($video));
   } else if (isset($_POST['olddirnext'])) {
      link("/data/".$video,"/data/video/sort/".$_POST['olddirnext']."/".basename($video));
      $redirArgs=array("video"=>$_POST['next']);
   } else if (isset($_POST['olddirprev'])) {
      link("/data/".$video,"/data/video/sort/".$_POST['olddirprev']."/".basename($video));
      $redirArgs=array("video"=>$_POST['previous']);
   }
   if (isset($_POST['deldir'])) {
      unlink("/data/video/sort/".$_POST['deldir']."/".basename($video));
   }
   if (isset($redirArgs)) {
     if (isset($_GET['unclassified'])) {
       $redirArgs['unclassified']="";
     }
     http_redirect("classify_video.php",$redirArgs,HTTP_METH_GET);
   }
 }

echo "<video src=\"$video\" autoplay controls>$video</video>\n";

for ($i=$index-1; $i>=0; $i--) {
  $file=$filedir."/".$videos[$i];
  if ($videos[$i][0] != "." && file_exists($file)) {
    if (isset($_GET['unclassified'])) {
      $s=stat($file);
      if ($s['nlink']!=1) {
	continue;
      }
    }
    $previous="$videodir/${videos[$i]}";
    break;
  }
 }
$nvids=count($videos);
for ($i=$index+1; $i<$nvids; $i++) {
  $file=$filedir."/".$videos[$i];
  if ($videos[$i][0] != "." && file_exists($file)) {
    if (isset($_GET['unclassified'])) {
      $s=stat($file);
      if ($s['nlink']!=1) {
	continue;
      }
    }
    $next="$videodir/${videos[$i]}";
    break;
  }
 }

echo "<table>\n";
echo "<tr>\n";
if (isset($previous)) {
    echo "<td><a href=\"classify_video.php?video=$previous$uc\">previous</a></td>\n";
 }
echo "<td></td>\n";
if (isset($next)) {
    echo "<td><a href=\"classify_video.php?video=$next$uc\">next</a></td>\n";
 }
echo "</tr>\n";

$d = dir("/data/video/sort/");
while(false !== ($e = $d->read())) {
  if ($e[0] != ".") {
    echo "<tr>\n";
    echo "<form action=\"classify_video.php?video=$video$uc\" method=\"post\">\n";
    if (isset($next)) {
      echo "<input type=\"hidden\" value=\"$next\" name=\"next\">\n";
    }
    if (isset($previous)) {
      echo "<input type=\"hidden\" value=\"$previous\" name=\"previous\">\n";
    }
    if (file_exists("/data/video/sort/".$e."/".basename($video))) {
      $s = stat("/data/video/sort/".$e."/".basename($video));
      if ($s['nlink']==1) {
	echo "<td colspan=3>only exists in $e</td>\n";
      } else {
	echo "<td colspan=2>Remove from <input type=\"submit\" name=\"deldir\" value=\"$e\" ></td><td> has ${s['nlink']} places to live</td>\n";}
    } else {
      if (isset($previous)) {
	      if ($e == "nix") {
	echo "<td>Add to <input type=\"submit\" name=\"olddirprev\" value=\"$e\" accesskey=\"n\"> and goto previous</td>\n";
	      } else {
		      echo "<td>Add to <input type=\"submit\" name=\"olddirprev\" value=\"$e\"> and goto previous</td>\n";
	      }
      }
      echo "<td>Add to <input type=\"submit\" name=\"olddir\" value=\"$e\" ></td>\n";
      if (isset($next)) {
	echo "<td>Add to <input type=\"submit\" name=\"olddirnext\" value=\"$e\" > and goto next</td>\n";
      }
    }
    echo "</form>\n";
    echo "</tr>\n";
  }
}
echo "<tr><td colspan=3>\n";
echo "<form action=\"classify_video.php?video=$video$uc\" method=\"post\">\n";
echo "<input type=\"text\" name=\"newdir\" value=\"noname\" width=30>\n";
 echo "<input type=\"submit\" value=\"new class\" >\n";
 echo "</form>\n";
echo "</tr></td\n";
echo "<table>\n";


page_foot();
?>
