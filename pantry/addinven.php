<?php
$code = $_REQUEST['ean'];
echo '<' . '?xml version="1.0" encoding="UTF-8" ?' . '>';
?>
<HEAD>
<TITLE>Pantry - Add</TITLE>
<SCRIPT LANGUAGE="JavaScript">
function closeWin()
{
myWindow.close();
}
function toForm() {
document.form.upc.focus();
// Replace upc in the script with the field name of which you want to place the focus.
}
</script>
</HEAD>

<BODY onLoad="toForm()";>

<?php
include_once 'header.html';
?>

<center>

<TABLE id=AutoNumber1 style="BORDER-COLLAPSE: collapse" borderColor=#111111 bgcolor=purple height=12 cellSpacing=3 cellPadding=3 width=100% border=1>
<TBODY>
<TR>
<TD width=100% height=12><CENTER><font face=tahoma size=4 color=white><b><hr>PANTRY MANAGEMENT INTERFACE<hr></b></CENTER></font></TD></TR></TABLE>

<TABLE id=AutoNumber2 style="BORDER-COLLAPSE: collapse" borderColor=#111111 bgcolor=green height=12 cellSpacing=3 cellPadding=3 width=100% border=1>
<TBODY>
<TR>
<TD width=100% height=12><CENTER><font face=tahoma size=4 color=white><B>ADD INVENTORY</B></CENTER></font></TD></TR></TABLE>

<FORM  NAME="form" METHOD="POST" ACTION="addupc.php">

<TABLE id=AutoNumber3 style="BORDER-COLLAPSE: collapse" borderColor=#111111 bgcolor=white height=12 cellSpacing=3 cellPadding=3 width=100% border=1>
<TBODY>
<TR>
<TD width=100% height=12><CENTER><br><br>
<INPUT TYPE="TEXT" NAME="quan" SIZE="2" value="1"> Quantity<BR><br><br>
<INPUT TYPE='TEXT' NAME='upc' ID= 'upc' VALUE='<?php echo "$code";?>' SIZE='15'> UPC<BR><BR><br><br>

<center><a href="https://play.google.com/store/apps/details?id=com.tecit.android.barcodekbd.demo" onClick="closeWin()">Install Barcode Keyboard Demo for Droid</a></center><BR><BR><br>

<p>
<BR>
<INPUT TYPE="SUBMIT" NAME="submit" VALUE="Submit">  
<INPUT TYPE="RESET" onclick="<?php $code = '';?>; toForm();">
</FORM>

</td></tr></table>



</body></head></html>



<!-- Script Size:  1.46 KB -->
