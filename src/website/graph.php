<?php
session_start();
if (isset($_GET["type"])) {$type=$_GET["type"];} else {$type=NULL;}

if ($type == "png") {
	header("Content-Type: image/png");
	header('Content-Disposition: inline; filename="graph.png"');
} else if ($type == "C" || $type == "Ctan") {
	header("Content-Type: text/x-c++src");
	header('Content-Disposition: attachment; filename="graph.C"');
} else if ($type == "eps") {
	header("Content-Type: application/postscript");
	header('Content-Disposition: attachment; filename="graph.ps"');
} else if ($type == "pdf") {
	header("Content-Type: application/pdf");
	header('Content-Disposition: attachment; filename="graph.pdf"');
} else if ($type == "canvas") {
	// Nothing, we provide a standalone HTML. 
} else {
	// Used for 'svg' and 'svginteractive'. 
	header("Content-Type: image/svg+xml");
	header('Content-Disposition: inline; filename="graph.svg"');
}
include 'variables.php';

$timeinnerexpr="time - (time AT TIME ZONE 'UTC' - time AT TIME ZONE 'Europe/Berlin')";
$timeexpr="EXTRACT('epoch' from ($timeinnerexpr))";

if (isset($_GET["width"])) {$width=$_GET["width"];} else {$width=640;}
if (isset($_GET["height"])) {$height=$_GET["height"];} else {$height=$width/4*3;}

$uids=get_int_list("uid");
$values=array();
$i=0;
foreach (explode(",",$uids) as $uid) {
	$values[$uid]=array("arrayindex" => $i);
	$i++;
}
if (isset($_GET["label"])) {$labels=explode(",",$_GET["label"]);} else {$label[]=NULL;}


if (isset($_GET["scales"])) {
	$scales=explode(",",$_GET["scales"]);
}
if (isset($_GET["offsets"])) {
	$offsets=explode(",",$_GET["offsets"]);
}

if (isset($_GET["starttime"])) {$starttime=$_GET["starttime"];} else {$starttime=NULL;}
if (isset($_GET["endtime"])) {$endtime=$_GET["endtime"];} else {$endtime=NULL;}

if (!$starttime) {
	if (!isset($_SESSION['default_interval']) || $_SESSION['default_interval']=="") {
	 $starttime=" now() - interval '1 hour'";
	} else {
	 $starttime=" now() - interval '".$_SESSION['default_interval']."'";
	}
}
$timeinterval=" time >= $starttime ";
if ($endtime) {
	 $timeinterval.="AND time <= $endtime ";
}
$timeinterval2=" request_time >= $starttime ";
if ($endtime) {
	 $timeinterval2.="AND request_time <= $endtime ";
}

$dbconn = pg_connect($dbstring);
if (!$dbconn) {
	die('Could not connect: ' . pg_last_error());
};



$minimum=1E37;
$maximum=-1E37;
$unit="";


$result = pg_query($dbconn,"SELECT uid,data_table,description,is_write_value FROM uid_list WHERE uid in ($uids) ORDER BY uid;");
while ($row = pg_fetch_assoc($result)) {
	$uid=$row['uid'];
	$item=$values[$uid];
	$item['data_table']=$row['data_table'];
	$item['description']=$row['description'];
	$item['is_write_value']=$row['is_write_value'];
	$result2 = pg_query($dbconn,"SELECT name, value FROM uid_configs WHERE uid=$uid;");
	while ($row2 = pg_fetch_assoc($result2)) {
		$item[$row2['name']] = $row2['value'];
	}
	if (!isset($item['label'])) {
		if (isset($item['name'])) {
			$name=$item['name'];
		} else {
			$name=$item['description'];
		}
		$item['label']=str_replace('_',' ',$name);// gnuplot does not like underscores
	}
	$i = $item['arrayindex'];
	$manipulate="";
	if (isset($scales[$i])) {
		if ($scales[$i] != 1) {
			$manipulate.=" * ".$scales[$i];
		}
	}
	if (isset($offsets[$i])) {
		if ($offsets[$i] != 0) {
			$manipulate.=" + ".$offsets[$i];
		}
	}
	if ($item['data_table']=='measurements_bool') {
		$value_expression='cast(value as integer)';
		$plotstyle="filledcurves x1";
	} else {
		$value_expression='value';
		$plotstyle="lines";
	}
	$value_expression.=$manipulate;
	$item['value_expression']=$value_expression;
	$item['manipulation']=$manipulate;
	if (!isset($item['plotstyle'])) {
		$item['plotstyle'] = $plotstyle;
	}
	$values[$uid]=$item;
 }


if (strpos($starttime,"now")===FALSE) {
	$starttime="timestamp ".$starttime;
}
if (strpos($endtime,"now")===FALSE && $endtime != "") {
	$endtime="timestamp ".$endtime;
}

$result = pg_query($dbconn,"SELECT extract('epoch' FROM $starttime);");
$row = pg_fetch_row($result);
$tmin = $row[0];

if ($endtime) {
	$result = pg_query($dbconn,"SELECT extract('epoch' FROM $endtime);");
} else {
	$result = pg_query($dbconn,"SELECT extract('epoch' FROM now());");
}
$row = pg_fetch_row($result);
$tmax = $row[0];

// file_put_contents("/tmp/graphdebug.log","'$starttime' '$endtime'  '$tmin' '$tmax'\n",FILE_APPEND);

$descriptorspec = array(
			0 => array("pipe", "r"),  // stdin is a pipe that the child will read from
			1 => array("pipe", "w"),  // stdout is a pipe that the child will write to
			2 => array("file", "/dev/null", "w") // stderr is a file to write to
			);

$cwd = '/tmp';
$env = array('some_option' => 'aeiou',
	     'PATH' => '/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin');
	
	
	
$process = proc_open('gnuplot', $descriptorspec, $pipes, $cwd, $env);
	
if (is_resource($process)) {
  // $pipes now looks like this:
  // 0 => writeable handle connected to child stdin
  // 1 => readable handle connected to child stdout
  // Any error output will be appended to /tmp/error-output.txt
  
  if ($type == "png") {
    fwrite($pipes[0], "set terminal png size $width,$height enhanced\n");
  } else if ($type == "eps") {
    $widthinches=$width/72;
    $heightinches=$height/72;
    fwrite($pipes[0], "set terminal postscript portrait colour rounded solid size $widthinches,$heightinches enhanced\n");
  } else if ($type == "pdf") {
    $widthinches=$width/72;
    $heightinches=$height/72;
    fwrite($pipes[0], "set terminal pdfcairo colour rounded size $widthinches,$heightinches enhanced\n");
  } else if ($type == "canvas") {
    fwrite($pipes[0], "set terminal canvas size $width,$height standalone mousing jsdir 'gnuplot/js' \n");
  } else if ($type == "svginteractive") {
    fwrite($pipes[0], "set terminal svg size $width,$height fixed enhanced mouse standalone \n");
  } else {
    fwrite($pipes[0], "set terminal svg size $width,$height dynamic enhanced \n");
  }
		
  fwrite($pipes[0], "set output\n");
  fwrite($pipes[0], "set style data line\n");
  fwrite($pipes[0], "set xdata time\n");
  fwrite($pipes[0], "set timefmt \"%s\"\n");
		
  fwrite($pipes[0], "set xtics rotate\n");
  fwrite($pipes[0], "set format x \"%y/%m/%d %H:%M\"\n");

  //fwrite($pipes[0], "set ylabel \"$unit\"\n");
		
		
  //fwrite($pipes[0], "set arrow from $tmin,$minimum to $tmax,$minimum nohead\n");
  //		fwrite($pipes[0], "set arrow from $tmin,$maximum to $tmax,$maximum nohead\n");
		
  if (isset($_GET['key'])) {
    $key=$_GET['key'];
    fwrite($pipes[0], "set key ${key}\n");
  }
	foreach ($values as $uid => $value) {
		if ($value['is_write_value']=='t') {
			$result = pg_query($dbconn,"SELECT comment, EXTRACT('epoch' from request_time - (request_time AT TIME ZONE 'UTC' - request_time AT TIME ZONE 'Europe/Berlin')) as request_time, (SELECT ${value['value_expression']} FROM ${value['data_table']} WHERE uid=$uid AND time > setvalue_requests.request_time ORDER BY TIME DESC LIMIT 1) AS value FROM setvalue_requests WHERE uid=$uid;");
			while ($row=pg_fetch_assoc($result)) {
				fwrite($pipes[0],"set label \"${row['comment']}\" at ${row['request_time']},${row['value']} rotate point points 1\n");
			}
		}
	}
		
  fwrite($pipes[0], "plot ");
  $need_comma=0;
  foreach ($values as $uid => $value) {
    if ($need_comma) {fwrite($pipes[0],",");}
    $query="SELECT $timeexpr, ${value['value_expression']} FROM ${value['data_table']} WHERE uid=$uid AND $timeinterval ORDER BY time";
    //        file_put_contents("/tmp/graphdebug.log",$query,FILE_APPEND);
    fwrite($pipes[0], "\"<  /bin/echo -e \\\"$query\\\" | psql \\\"$dbstring\\\"\" u 1:3 with ${value['plotstyle']} title \"${value['label']}${value['manipulation']}\"");
    $need_comma=1;
  }
  fwrite($pipes[0], "\n");
  
  fclose($pipes[0]);
  while(!feof($pipes[1])) {
    print fread($pipes[1], 4096);
  }
  fclose($pipes[1]);
  
  // It is important that you close any pipes before calling
  // proc_close in order to avoid a deadlock
  $return_value = proc_close($process);
 }

?>
