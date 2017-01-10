<?php
session_start();
include 'variables.php';
$dbconn = pg_connect($dbstring);
if (!$dbconn) {
	die('Could not connect: ' . pg_last_error());
};




if (isset($_GET["type"])) {$type=$_GET["type"];} else {$type=NULL;}
$plotter='dot';
if ($type == "png") {
	header("Content-Type: image/png");
	header('Content-Disposition: inline; filename="graph.png"');
	$plotter .= " -Tpng ";
} else if ($type == "eps") {
	header("Content-Type: application/postscript");
	header('Content-Disposition: attachment; filename="graph.ps"');
} else if ($type == "pdf") {
	header("Content-Type: application/pdf");
	header('Content-Disposition: attachment; filename="graph.pdf"');
} else if ($type == "debug") {
	$plotter='cat';
 } else if ($type == "canvas") {
	// Nothing, we provide a standalone HTML. 
} else {
	// Used for 'svg' and 'svginteractive'. 
	header("Content-Type: image/svg+xml");
	header('Content-Disposition: inline; filename="graph.svg"');
	$plotter .= " -Tsvg ";
}

$descriptorspec = array(
			0 => array("pipe", "r"),  // stdin is a pipe that the child will read from
			1 => array("pipe", "w"),  // stdout is a pipe that the child will write to
			2 => array("file", "/dev/null", "w") // stderr is a file to write to
			);

$cwd = '/tmp';
$env = array('some_option' => 'aeiou',
	     'PATH' => '/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin');
	
	
	
$process = proc_open($plotter, $descriptorspec, $pipes, $cwd, $env);
	
if (is_resource($process)) {
  // $pipes now looks like this:
  // 0 => writeable handle connected to child stdin
  // 1 => readable handle connected to child stdout
  // Any error output will be appended to /tmp/error-output.txt
	fwrite($pipes[0], "digraph g{\n");
	fwrite($pipes[0], "node [shape=record];\n");

	$result = pg_query($dbconn,"SELECT *, (SELECT value FROM uid_list INNER JOIN uid_configs USING (uid) WHERE description = rule_nodes.nodename AND name = 'name') AS name FROM rule_nodes;");
	while ($row = pg_fetch_assoc($result)) {
		fwrite($pipes[0], "n${row['nodeid']} [");
		fwrite($pipes[0], ' label="{');
		$result2 = pg_query($dbconn,"SELECT slot from rule_node_parents WHERE nodeid=${row['nodeid']} and slot IS NOT NULL;");
		if (pg_num_rows($result2)>0) {
			fwrite($pipes[0], '{');
			$need_pipe=false;
			while ($row2 = pg_fetch_assoc($result2)) {
				if ($need_pipe) {
					fwrite($pipes[0], '|');
				}
				fwrite($pipes[0], "${row2['slot']}");
				$need_pipe=true;
			}
			fwrite($pipes[0], '}|');
		}
		fwrite($pipes[0], "${row['nodename']} ${row['name']}");
		fwrite($pipes[0], "|${row['nodetype']}");
		$result2 = pg_query($dbconn,"SELECT * from rule_configs WHERE nodeid=${row['nodeid']};");
		while ($row2 = pg_fetch_assoc($result2)) {
			fwrite($pipes[0], "| ${row2['name']} = ${row2['value']}");
		}
		fwrite($pipes[0], '}"');
		fwrite($pipes[0], " URL=\"rulenode.php?nodeid=${row['nodeid']}\"");
		fwrite($pipes[0], "];\n");
	}
	$result = pg_query($dbconn,"SELECT * FROM rule_node_parents;");
	while ($row = pg_fetch_assoc($result)) {
		fwrite($pipes[0], "n${row['parent']} -> n${row['nodeid']}");
		//		fwrite($pipes[0], " label=\"${row['slot']}\"");
		fwrite($pipes[0], ";\n");
	}
	

	fwrite($pipes[0], "}\n");
  
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
