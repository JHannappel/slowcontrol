<?php
header("Content-Type: image/svg+xml");
header('Content-Disposition: inline; filename="moon.svg"');
$pom=$_GET["phase"];
echo "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
echo "<svg xmlns=\"http://www.w3.org/2000/svg\"\n";
echo "	version=\"1.1\" baseProfile=\"full\"\n";
echo "	width=\"200px\" height=\"200px\" viewBox=\"0 0 200 200\">\n";
echo "<defs>\n";
echo "<clipPath id=\"left\"> <rect x=\"0\" y=\"0\" width=\"100\" height=\"200\"/> </clipPath>\n";
echo "<clipPath id=\"right\"> <rect x=\"100\" y=\"0\" width=\"100\" height=\"200\"/> </clipPath>\n";
echo "<clipPath id=\"circ\"> <circle cx=\"100\" cy=\"100\" r=\"95\"/> </clipPath>\n";
echo "</defs>\n";
if ($pom < 0.25) {
    $clip="right";
    $rectx="0";
    $phase=100-$pom*400;
    $first="white";
    $second="black";
    $rectcol="black";
} else if ($pom < 0.5) {
    $clip="left";
    $rectx="100";
    $phase=$pom*400-100;
    $first="black";
    $second="white";
    $rectcol="white";
} else if ($pom < 0.75) {
    $clip="left";
    $rectx="100";
    $phase=300-$pom*400;
    $first="black";
    $second="white";
    $rectcol="white";
} else {
    $clip="left";
    $rectx="100";
    $phase=$pom*400-300;
    $first="white";
    $second="black";
    $rectcol="black";
}
echo "<circle cx=\"100\" cy=\"100\" r=\"95\" style=\"fill:$first;stroke:black;\" />";
echo "<ellipse clip-path=\"url(#$clip)\" cx=\"100\" cy=\"100\" rx=\"$phase\" ry=\"95\" style=\"fill:$second;stroke:none;\" />";
echo "<rect clip-path=\"url(#circ)\" x=\"$rectx\" y=\"0\" width=\"100\" height=\"200\" style=\"fill:$rectcol;stroke:none;\" />";

echo "/>\n";
echo "</svg>\n";
?>
