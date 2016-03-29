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
	</script>
	<style>
		*{margin:0;padding:0;}
	</style>
</head>

<?php
echo "<body background='' onload=\"mjpeg_start();\">";
?>
	<div>
		<img id="mjpeg_image" style="width:50%" onclick='window.open("index.php","_blank");'>
	<section>
	<div class="buttonHolder">
		<a href="#" class="button power"></a>
		<a href="#" class="button tick"></a>
	</div>
	</section>
	</div>

</body>
</html>

