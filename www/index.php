<html>
<head>
	<meta name="viewport" content="width=device-width, initial-scale=1">
	<title><?php echo TITLE_STRING; ?></title>
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
<img id="mjpeg_image" style="width:50%" onclick='window.open("index.php","_blank");'>
</body>
</html>

