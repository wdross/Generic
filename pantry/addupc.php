<HEAD>
<TITLE>Pantry - Add details</TITLE>
<SCRIPT LANGUAGE="JavaScript">
function toForm() {
document.form.brand.focus();
}
</SCRIPT>
</HEAD>
<BODY onLoad="toForm()";>

<?PHP

include_once 'header.html';
include_once 'db.php';

$quan = $_POST['quan'];
$upc = $_POST["upc"];

if(($quan < 0)){
  echo "<center><b><font face='tahoma' color='red'>** You did not enter a quantity! **</center></b><br />";

}else{
  $contlist=mysql_query("SELECT * FROM inven WHERE upc='$upc'");
  $user_check = mysql_num_rows($contlist);
  if ($user_check <= 0 and strlen($upc) > 12 and substr($upc,0,1) == "0") {
    $upc = substr($upc,-12); // shorten zero-prefix upc since it wasn't found
    $contlist=mysql_query("SELECT * FROM inven WHERE upc='$upc'");
    $user_check = mysql_num_rows($contlist);
    // even if it's not found, we'll use the short version to perform the ADD
  }

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

  if(($user_check > 0)){
    echo "<center><b><font face='tahoma' color='black'>Updated ".$descrip." </b><br />";

    $sql = mysql_query("UPDATE inven SET quant=(('$quan1')+('$_POST[quan]')) WHERE upc='$upc'");

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
    include_once 'addnew.php';
  }
}
include_once 'footer.html';

?>
