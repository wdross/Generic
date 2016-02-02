<?PHP
  print('<center><font face="tahoma" color="black" size="3">
        You now have <b>'.$quan2.'</b>
        <a href="editproduct.php?upc='.$upc.'">
        '.$brand.', '.$descrip.'</a>');
  echo " - ".$size." in ".$cat."<br />";
  echo '</font>';
  echo '<embed src="audio/Numbers/'.$quan2.'.wav" width=2 height=0 autostart=true>';
?>
