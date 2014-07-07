<?PHP

include_once 'test.html';
include_once 'db.php';

$quan = $_POST['quan'];
$upc = $_POST["upc"];

echo $upc;
echo "<BR>";


$contlist=mysql_query(
        "SELECT * FROM inven WHERE upc='$_POST[upc]'");

while ($all = mysql_fetch_array($contlist)) {
$quan1 = $all['quant'];
$upc1 = $all['upc'];
$brand = $all['brand'];
$descrip = $all['descrip'];
$size = $all['size'];
$flavor = $all['flavor'];
$cat = $all['cat'];
}


$quan2 = (($quan)+($quan1));

//check that upc does not already exist

$sql_user_check = mysql_query("SELECT upc FROM inven
                         WHERE upc='$_POST[upc]'");

$user_check = mysql_num_rows($sql_user_check);

if(($user_check > 0)){
  echo "<center><b><font face='tahoma' color='black'>Updated ".$descrip." </b><br />";

$sql = mysql_query("UPDATE inven SET quant=(('$quan1')+('$_POST[quan]'))
WHERE upc='$_POST[upc]'");

echo '<TABLE id=AutoNumber4 style="BORDER-COLLAPSE: collapse" borderColor=#111111 height=12 
       cellSpacing=3 cellPadding=3 width=600 border=1>
      <TBODY>
      <TR>
      <TD width=900 height=12><CENTER>';
echo "<center><font face='tahoma' color='black' size='2'>You now have <b>".$quan2."</b> ".$brand.", ".$descrip." - ".$size."<br />";
echo '</td></tr></table>';                    

  if(!$sql){
    echo 'A database error occured while adding your product.';
    }
  

   }else{


      echo '<TABLE id=AutoNumber5 style="BORDER-COLLAPSE: collapse" borderColor=#111111 bgcolor=black
      height=12 cellSpacing=3 cellPadding=3 width=600 border=1>
      <TBODY>
      <TR>
      <TD width=600 height=12><CENTER>';

	echo "<center><b><font face='tahoma' color='red'>Item does not exist in database!</b><br /></center>";
      echo '</td></tr></table>';
	include_once 'addnew.php';
      

 }
  

  
  ?>

















