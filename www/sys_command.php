<?php
if (isset($_GET['cmd']))
{
        $cmd = $_GET['cmd'];
	$fifo = fopen("/run/cmd.fifo","w");

	if ($cmd === "shutdown")
		fwrite($fifo, "poweroff");
	else if ($cmd === "stop")
		fwrite($fifo, "stop");
	else if ($cmd === "start")
		fwrite($fifo, "start");
	else if ($cmd === "capture")
		fwrite($fifo, "capture");
	fclose($fifo);
}
?>

