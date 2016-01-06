<?PHP

include_once 'db.php';

$quan = $_POST['quan'];
$upc = $_POST["upc"];

if(($quan < 1)){ //if no quantity entered
  echo "<center><b><font face='tahoma' color='red'>** You did not enter a quantity! **</font></center></b><br />";
}else if($upc == "REMOVE"){
  // basically a NOP - for if we are headless just show the same dialog
  include_once 'removeinven.php';
}else if($upc == "ADD"){
  include_once 'addupc.php';
}else{ // check existance of given upc

  $sql_user_check = mysql_query("SELECT upc FROM inven WHERE upc='$upc'");
  $user_check = mysql_num_rows($sql_user_check);

include_once 'removeinven.php';

  if($user_check <= 0 and
     strlen($upc) > 12 and
     substr($upc,0,1) == "0"){ //if upc doesn't exist in database, check w/o leading zero
    $upc = substr($upc,-12);
    $sql_user_check = mysql_query("SELECT upc FROM inven WHERE upc='$upc'");
    $user_check = mysql_num_rows($sql_user_check);
  }

  if ($user_check <= 0) { // modified upc doesn't exist, ERROR
    echo '<TABLE id=AutoNumber5 style="BORDER-COLLAPSE: collapse" borderColor=#111111 bgcolor=black
    height=12 cellSpacing=3 cellPadding=3 width=600 border=1>
    <TBODY>
    <TR>
    <TD width=600 height=12><CENTER>';
    echo "<center><b><font face='tahoma' color='red'>Item '".$upc."' does not exist in database!  Check your entry.</b><br/></font></center>";
    echo '</td></tr></table>';
  }else{//found upc listed, close layer 2, start layer 2
    $contlist=mysql_query("SELECT * FROM inven WHERE upc='$upc'");

    while ($all = mysql_fetch_array($contlist)) {
      $quan1 = $all['quant'];
      $upc1 = $all['upc'];
      $brand = $all['brand'];
      $descrip = $all['descrip'];
      $size = $all['size'];
      $flavor = $all['flavor'];
      $cat = $all['cat'];
    }

    $quan2 = (($quan1)-($quan));

    if(($quan2 < 0)){ //not enough qty in stock, start layer 3
      echo "<center><font face=verdana size=3><b>You do not have enough of this product in inventory to remove <br>the quantity you entered</b></font></center>";
      echo "<br><center><font face='tahoma' color='black' size='2'>You only have <b>".$quan1."</b> ".$brand.", ".$descrip." - ".$size."<br />";
      include_once 'footer.html';

      }else{ //else, must have enuff onhand, update database, close layer 3, start layer 3

      echo "<center><b><font face='tahoma' color='black'>Removed ".$descrip." </font></b><br />";

      $sql = mysql_query("UPDATE inven SET quant=(('$quan1')-('$_POST[quan]')) WHERE upc='$upc'");

      print('<TABLE id=AutoNumber4 style="BORDER-COLLAPSE: collapse" borderColor=#111111 height=12 cellSpacing=3 cellPadding=3 width=600 border=1>
      <TBODY>
      <TR>
      <TD width=900 height=12><CENTER>');
include_once 'showone.php';
      echo '</td></tr></table>';
      if(!$sql){ //check for database errors, start layer 4
        echo 'A database error occured while removing your product.';
      } //close layer 4
    } //close layer 3
  } //close layer 2
} //close layer 1

include_once 'footer.html';

?>
