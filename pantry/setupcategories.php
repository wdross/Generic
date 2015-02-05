<?php
include_once 'db.php';
include_once 'header.html';
?>
<HTML>
<BODY>

<TABLE id=AutoNumber7 style="BORDER-COLLAPSE: collapse" borderColor=#009900 
      height=12 cellSpacing=3 cellPadding=3 width=600 border=3 align="center">
      <TBODY>
      <TR>
      <TD><CENTER>

<FONT FACE=TAHOMA SIZE=4<B>Database Categories/Locations</B></FONT><HR>


<TABLE id=AutoNumber5 style="BORDER-COLLAPSE: collapse" borderColor=#009900 
      height=12 cellSpacing=3 cellPadding=3 width=590 border=0 align="center">
      <TBODY>
      <TR>
<FORM  NAME="form" METHOD="POST" ACTION="postcategory.php">

</font>


<?php
$loclist=mysql_query("SELECT * FROM locations"); //  ORDER BY locate ASC

while ($loc = mysql_fetch_array($loclist)) {
  $usedlist=mysql_query("SELECT * FROM inven WHERE cat='$loc[locate]'");
  $used = mysql_num_rows($usedlist);

  echo '<tr><td><center>'; 
  if ($used == 0) {
    $encoded = urlencode($loc['locate']);
    print('<a href="deletecategory.php?cat='.$encoded.'">');
  }
  echo $loc['locate']; 
  if ($used == 0)
    print('</a>');
  print('</center></td></tr>
        ');
}

?>

<TD height=25 align=CENTER vAlign=bottom><INPUT TYPE="TEXT" NAME="cat" SIZE="15" value=""></td></tr> 
</TABLE>

<TABLE id=AutoNumber7 style="BORDER-COLLAPSE: collapse" borderColor=#009900 
      height=12 cellSpacing=3 cellPadding=3 width=590 border=0 align="center">
      <TBODY>
      <TR>

<INPUT TYPE="SUBMIT" NAME="submit" VALUE="Add new Category to database"></TD></TR> 
</FORM>
</TABLE>
</TR></TD></TABLE>
