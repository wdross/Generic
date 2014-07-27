<?php

include_once 'db.php';
include_once 'header.html';

$upc = $_GET['upc'];

$prodlist=mysql_query(
        "SELECT upc, quant, brand, descrip, size, flavor, cat, sameas FROM inven WHERE upc = '$_GET[upc]'");

while ($prod = mysql_fetch_array($prodlist)) {
$upc = $prod['upc'];
$quant = $prod['quant'];
$brand = $prod['brand'];
$descrip = $prod['descrip'];
$size = $prod['size'];
$flavor = $prod['flavor'];
$cat = $prod['cat'];
$sameas = $prod['sameas'];
}

$total = 0;
$num = 0;
$badSames = '';
$prodlist=mysql_query(
"SELECT * FROM inven WHERE sameas='$sameas' OR upc='$upc' OR sameas='$upc' OR upc='$sameas'");
while ($prod = mysql_fetch_array($prodlist)) {
  $total = $total + $prod['quant'];
  $num = $num + 1;
  if ($prod['sameas'] != $sameas)
    $badSames .= ' ' . $prod['sameas'] . ' listed for UPC ' . $prod['upc'];
}

?>

<HTML>
<HEAD>
<TITLE>Pantry - Edit</TITLE>
</HEAD>
<BODY>

<center>
<TABLE id=AutoNumber7 style="BORDER-COLLAPSE: collapse" borderColor=#009900 
      height=12 cellSpacing=3 cellPadding=3 width=600 border=3>
      <TBODY>
      <TR>
      <TD><CENTER>

<FONT FACE=TAHOMA SIZE=4 color=red><B> EDIT THIS PRODUCT</B></FONT><HR>





<TABLE id=AutoNumber5 style="BORDER-COLLAPSE: collapse" borderColor=#009900 
      height=12 cellSpacing=3 cellPadding=3 width=590 border=0>
      <TBODY>
      <TR>
<FORM  NAME="form" METHOD="POST" ACTION="editprodprocess.php">

</font>
<TD  width="25%"  height=25 align=right vAlign=bottom>UPC: &nbsp </TD>
<TD  width="75%"  height=25 align=LEFT vAlign=bottom><INPUT TYPE="HIDDEN" NAME="upc" SIZE="15" value="<?PHP echo $upc; ?>"><?PHP echo $upc; ?></td></tr>
 
<TR><TD  width="25%"  height=25 align=right vAlign=bottom>BRAND: &nbsp </TD>
<TD  width="75%"  height=25 align=left vAlign=bottom><INPUT TYPE="TEXT" NAME="brand" SIZE="20" value="<?PHP echo $brand; ?>"></td></tr>

<TR><TD  width="25%"  height=25 align=right vAlign=bottom> DESCRIPTION: &nbsp </TD>
<TD  width="75%"  height=25 align=left vAlign=bottom><INPUT TYPE="TEXT" NAME="descrip" SIZE="30" value="<?PHP echo $descrip; ?>"></TD></TR>

<TR><TD  width="100%"  height=25 align=center vAlign=bottom COLSPAN=2><font size=2>
When typing a description try to keep it as simple as possible, if you are adding a can of Sweet Corn, simply
type "Corn" whereas when adding beans you would need to type "Pinto Beans" for pintos because of variety.
</font></TD></TR>

<TR><TD  width="25%"  height=25 align=right vAlign=bottom> SIZE: &nbsp </TD>
<TD  width="75%"  height=25 align=left vAlign=bottom><INPUT TYPE="TEXT" NAME="size" SIZE="15" value="<?PHP echo $size; ?>"></TD></TR>

<TR><TD  width="25%"  height=25 align=right vAlign=bottom> FLAVOR: &nbsp </TD>
<TD  width="75%"  height=25 align=left vAlign=bottom><INPUT TYPE="TEXT" NAME="flavor" SIZE="20" value="<?PHP echo $flavor; ?>"></TD></TR>

<TR><TD  width="25%"  height=25 align=right vAlign=bottom> CATEGORY: &nbsp </TD>
<TD  width="75%"  height=25 align=left vAlign=bottom><select name="cat">
  <option value="<?php echo $cat; ?>"selected><?php echo $cat; ?></option>

<?php
include_once 'selectlist.html';
?>

<TR><TD  width="25%"  height=25 align=right vAlign=bottom> QOH: &nbsp </TD>
<TD  width="75%"  height=25 align=left vAlign=bottom><INPUT TYPE="TEXT" NAME="quant" SIZE="15" value="<?PHP echo $quant; ?>">

<?php if (($num > 1) || ($total != $quant)): ?>
<FONT FACE=TAHOMA SIZE=4 color=red><B> Total:
<?php echo $total; echo ' ('; echo $num; echo " records)"; ?>
<?php if ($badSames) echo $badSames; ?>
</B></FONT>
<?php endif ?>

</TD></TR>

<TR><TD  width="25%"  height=25 align=right vAlign=bottom> Same AS: &nbsp </TD>
<TD  width="75%"  height=25 align=left vAlign=bottom><INPUT TYPE="TEXT" NAME="sameas" SIZE="20" value="<?PHP echo $sameas; ?>"></TD></TR>

</select></TD></TR></TABLE>


<TABLE id=AutoNumber8 style="BORDER-COLLAPSE: collapse" borderColor=#111111 
      height=12 cellSpacing=3 cellPadding=3 width=590 border=0>
      <TBODY>
      <TR>
      <TD><CENTER><br>
<INPUT TYPE="SUBMIT" NAME="submit" VALUE="UPDATE THIS ITEM">&nbsp
<INPUT TYPE="button" VALUE="REMOVE THIS ITEM" onClick="location.href='deleteupc.php?upc=<?php echo $upc; ?>'">&nbsp
<INPUT TYPE="button" VALUE="CANCEL"onClick="parent.location='report1.php'">

</TD></TR> 
</FORM>
</TABLE>
</TR></TD></TABLE>


<?php
include_once 'footer.html';
?>

</body>
</html>
