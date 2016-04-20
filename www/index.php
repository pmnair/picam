<!DOCTYPE html>
<html>
<head>
	<meta http-equiv="Content-Type" content="text/html;charset=UTF-8;width=device-width, initial-scale=1"/>
	<title>PiCam Preview</title>
	<link type="text/css" href="css/buttons.css" rel="stylesheet" />
	<script src="js/jquery.min.js"></script>
	<script type="text/javascript">
	var mjpeg;

	function mjpeg_read()
	{
		setTimeout("mjpeg.src = 'mjpeg_read.php?time=' + new Date().getTime();", 200);
	}

	function mjpeg_start()
	{
		mjpeg = document.getElementById("mjpeg_image");
		mjpeg.onload = mjpeg_read;
		mjpeg.onerror = mjpeg_read;
		mjpeg_read();
	}

	function create_XMLHttpRequest()
        {
		if (window.XMLHttpRequest)
			return new XMLHttpRequest();    // IE7+, Firefox, Chrome, Opera, Safari
		else
			return new ActiveXObject("Microsoft.XMLHTTP");  // IE6, IE5
        }

	var sys_cmd = create_XMLHttpRequest();
	function command(cmd)
	{
		sys_cmd.open("PUT", "sys_command.php?cmd=" + cmd, true);
		sys_cmd.send();
	}

	function shutdown()
	{
		var res = confirm("Do you want to Shutdown the camera?");
		if (res) {
			command("shutdown");
		}
	}
	function stop()
	{
		var res = confirm("Do you want to stop picam?");
		if (res) {
			command("stop");
		}
	}
	function start()
	{
		var res = confirm("Start picam?");
		if (res) {
			command("start");
		}
	}

	</script>
	<style>
		*{margin:0;padding:0;}
	</style>
</head>

<?php
echo "<body background='' onload=\"mjpeg_start();\">";
?>
<div style="margin:5% 5%; overflow:hidden;">
	<div style="width: 85%; float: left;">
	<img id="mjpeg_image" style="width: 100%" onclick='window.open("index.php","_blank");'>
	</div>
	<div class="buttonHolder" style="float: right;">
		<a href="#" class="button power" onClick=shutdown()></a>
		<a href="#" class="button start" onClick=command("start")></a>
		<a href="#" class="button stop" onClick=stop()></a>
	</div>
</div>
</body>
</html>

