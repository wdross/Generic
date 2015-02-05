<?PHP

// This is in response to requesting an "Add" of a new category

include_once 'db.php';

$cat = mysql_real_escape_string($_POST["cat"]);


//check that Category does not already exist
$sql_user_check = mysql_query("SELECT locate FROM locations WHERE locate='$cat'");
$user_check = mysql_num_rows($sql_user_check);

if(($user_check == 0)){
  $sql = mysql_query("INSERT INTO locations (locate) VALUES('$cat')")
                      or die (mysql_error());
  if(!$sql){
    echo "A database error occured while adding category '.$cat.'.";
  }
  else {
    echo "<center><font face='tahoma' color='black' size='2'><b>".$cat." added to database.<br />";
  }
}else{
  echo "<center><b><font face='tahoma' color='red'>Category is already in database!</b><br><br /></center>";
}  

include_once 'setupcategories.php';
?>
