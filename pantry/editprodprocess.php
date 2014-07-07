<?php

include_once 'db.php';

$upc = $_POST['upc'];
$quant = $_POST['quant'];
$brand = mysql_real_escape_string($_POST['brand']);
$descrip = mysql_real_escape_string($_POST['descrip']);
$size = mysql_real_escape_string($_POST['size']);
$flavor = mysql_real_escape_string($_POST['flavor']);
$cat = $_POST['cat'];
$sameas = mysql_real_escape_string($_POST['sameas']);

$sql_check = mysql_query("SELECT upc FROM inven WHERE upc='$_POST[sameas]'");
$user_check = mysql_num_rows($sql_check);

if($user_check == 0){
  if ($sameas != '') {
    echo "<center><b><font face='tahoma' color='black'>$sameas not present, SAME AS not updated</b><br />";
  }

  $sql = mysql_query("UPDATE  inven SET quant='$quant', brand='$brand', descrip='$descrip', size='$size',
                      flavor='$flavor', cat='$cat' WHERE upc='$upc'");
}
else {
  $sql = mysql_query("UPDATE  inven SET quant='$quant', brand='$brand', descrip='$descrip', size='$size',
                     flavor='$flavor', cat='$cat', sameas='$sameas' WHERE upc='$upc'");
}

if(!$sql){
  echo 'A database update error occured . Please contact support.';
}

echo '<center><br>';
echo '<font face=verdana size=4 color=red><b>';
echo 'You have just updated<font color=black size=2><br> ' . $brand . ', ' . $descrip . ' - ' . $size . ' </font><br>Successfully';
echo '</font></b></center>';
echo '<br>';
include_once 'report1.php';

?>
