<?php
session_start();
include 'variables.php';
$dbconn = pg_connect($dbstring);
if (!$dbconn) {
	die('Could not connect: ' . pg_last_error());
};
page_head($dbconn,"Slowcontrol");

if (isset($_GET['dir'])) {
	$videodirs=array($_GET['dir']);
} else {
  $videodirs=array("/video/camera1/", "/video/camera2/");
}

foreach ($videodirs as $videodir) {

	$videos=scandir("/data".$videodir, SCANDIR_SORT_DESCENDING);

	echo "<h1>Unclassified Videos in $videodir</h1>\n";
	foreach ($videos as $video) {
		if ($video[0] == ".") {
			continue;
		}	
		$s=stat("/data".$videodir."/".$video);
		if ($s['nlink']==1) {
			echo "<a href=\"classify_video.php?video=$videodir/$video&unclassified\">$video<a><br>\n";
		}
	}
}

echo "<h1>Sorted Videos</h1>\n";
$d = dir("/data/video/sort/");
while(false !== ($e = $d->read())) {
  if ($e[0] != ".") {
     echo "<a href=\"videos.php?dir=/video/sort/$e\">$e<a><br>\n";
  }
}

foreach ($videodirs as $videodir) {
	echo "<h1>Videos in $videodir</h1>\n";
	$videos=scandir("/data".$videodir);
	foreach ($videos as $video) {
		if ($video[0] == ".") {
			continue;
		}	
		echo "<a href=\"classify_video.php?video=$videodir/$video\">$video<a>  \n";
	}
}


page_foot();
?>
