<?PHP

include_once 'db.php';
$cat = urldecode($_GET['cat']);

$sql_user_check = mysql_query("SELECT * FROM locations WHERE locate='$cat'");

$user_check = mysql_num_rows($sql_user_check);

if ($user_check > 0) {
  // category is in the table,
  $sql = mysql_query("DELETE FROM locations WHERE locate='$cat'");

  if(!$sql){
    echo 'A database error occured while deleting Category .$cat.';
  }
  else {
    echo "<center><b><font face='tahoma' color='black'>Removed ".$cat." From Database </font></b><br />";
  }
}
else {
  echo "<center><b><font face='tahoma' color='red'>Category .$cat. does not exist in database!</b><br />
        </font></center>";
}

include_once 'setupcategories.php';
?>
