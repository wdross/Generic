<?PHP

include_once 'db.php';
include_once 'header.html';

echo "<HTML><HEAD><TITLE>Pantry - Delete</TITLE></HEAD></HTML>";

$upc = $_GET['upc'];

$prodlist=mysql_query(
        "SELECT upc, quant, brand, descrip, size, flavor, cat FROM inven WHERE upc = '$_GET[upc]'");

while ($prod = mysql_fetch_array($prodlist)) {
$upc = $prod['upc'];
$quant = $prod['quant'];
$brand = $prod['brand'];
$descrip = $prod['descrip'];
$size = $prod['size'];
$flavor = $prod['flavor'];
$cat = $prod['cat'];

}

$sql_user_check = mysql_query("SELECT upc FROM inven
                         WHERE upc='$_GET[upc]'");

$user_check = mysql_num_rows($sql_user_check);

if(($user_check > 0)){
  echo "<center><b><font face='tahoma' color='black'>Removed ".$descrip." From Database </font></b><br />";

$sql = mysql_query("DELETE FROM inven WHERE upc='$_GET[upc]'");

  if(!$sql){
    echo 'A database error occured while deleting your product.';
    }
  

   }else{

      
      echo '<TABLE id=AutoNumber5 style="BORDER-COLLAPSE: collapse" borderColor=#111111 bgcolor=black
      height=12 cellSpacing=3 cellPadding=3 width=600 border=1>
      <TBODY>
      <TR>
      <TD width=600 height=12><CENTER>';

	echo "<center><b><font face='tahoma' color='red'>Item does not exist in database!</b><br />
            </font></center>";
      echo '</td></tr></table>';
	      

 }

include_once 'footer.html';

  
  ?>
