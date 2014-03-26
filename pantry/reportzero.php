<?PHP

include_once 'db.php';
include_once 'header.html';
include_once 'reportheader.html';

echo "<hr>";
//                          "SELECT (case when sameas='' then upc else sameas end) as same, sum(quant)    quant, brand, descrip, size, flavor, cat FROM inven WHERE quant = '0'");
mysql_query("DROP TEMPORARY TABLE IF EXISTS tally");
$tt = mysql_query("CREATE TEMPORARY TABLE tally SELECT (CASE WHEN sameas='' THEN upc ELSE sameas END) AS same, SUM(quant) AS total, brand, descrip, size, flavor, cat FROM inven GROUP BY same");
$deflist = mysql_query("SELECT same, total, brand, descrip, size, flavor, cat FROM tally WHERE total = 0 ORDER BY cat ASC, descrip ASC, brand ASC");

//$deflist=mysql_query("SELECT (case when sameas = '' then upc else sameas end) as same, sum(quant) quant, brand, descrip, size, flavor, cat FROM inven WHERE quant = '0' GROUP BY same ORDER BY cat ASC, descrip ASC, brand ASC");


while ($all = mysql_fetch_array($deflist)) {
   $results[$all['cat']][] = array ('upc' => $all['same'], 'quant' => $all['total'], 'brand' => $all['brand'], 'descrip' => $all['descrip'], 'size' => $all['size'], 'flavor' => $all['flavor']);
}

//print_r($results);
error_reporting(0);
echo '<center><TABLE id=AutoNumber22 style="BORDER-COLLAPSE: collapse" borderColor=#000000 
      height=12 cellSpacing=3 cellPadding=3 width=600 border=1>
      <TBODY>
      <TR><TD>';

foreach ($results as $catName => $catData)
{
   print('<center><TABLE id=AutoNumber20 style="BORDER-COLLAPSE: collapse" borderColor=#000000 
      bgcolor=purple height=12 cellSpacing=3 cellPadding=3 width=600 border=1>
      <TBODY>
      <TR><TD>
		<b><font face=arial size=2 color=white>'.$catName.'</b><br/></font></td></tr></table></center>'."\n");
   foreach ($catData as $itemNum => $itemData)
   
   {
error_reporting(1);
      // if you want to access the row data in this loop, use the following method:
      print('<center><TABLE id=AutoNumber21 style="BORDER-COLLAPSE: collapse" borderColor=#000000 
      height=12 cellSpacing=3 cellPadding=3 width=600 border=0>
      <TBODY>
      <TR><TD><font face=arial size=2>
	<A HREF="editproduct.php?upc='.$itemData['upc'].'">edit</A> - ' .
      $itemData['descrip'].', '.$itemData['brand'].', '.$itemData['size'].',
      '.$itemData['flavor'].' - '.$itemData['quant'].'<br/></td></tr></table></center></font>'."\n"); 
      // etc. (you must code the field names in hard coded this way)
      //foreach ($itemData as $fieldName => $value)
      
   }
}
echo '</td></tr></table></center>';

mysql_query("DROP TEMPORARY TABLE tally");

include_once 'footer.html';

?>
