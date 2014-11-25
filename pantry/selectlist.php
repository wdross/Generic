<?php
$loclist=mysql_query("SELECT * FROM locations");

while ($loc = mysql_fetch_array($loclist)) {
  echo '<option>'; echo $loc['locate']; echo '</option>';
}
?>
