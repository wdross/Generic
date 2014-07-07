<?php
$dbhost = 'localhost'; //edit this if you want to point at a different machine
$dbuser = 'aross'; //edit this for username of the one with permissions to the mysql database you're using
$dbpass = ''; //edit to this be the password for the above username in mysql - not necessarily equal to machine's user/pass.

$conn = mysql_connect($dbhost, $dbuser, $dbpass) or die                      ('Error connecting to mysql');

$dbname = 'pantrydb'; //edit this to change the database name
mysql_select_db($dbname);
?>






