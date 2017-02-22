<?php

$socket = socket_create(AF_INET, SOCK_STREAM, SOL_TCP);
$connection = socket_connect($socket, '127.0.0.1', '8089');

$strMsg = "http://pws.myhug.cn/npic/p/9/ff5839a4edffe15bfe9fe78cc49a4ebb0c49218dbf3819\n";
$strMsg = "http://pws.myhug.cn/npic/p/9/ff5839e3f663949a331a21ab99a8bafd3713f98807b57c\n";
$intLen = strlen( $strMsg );
if( socket_write( $socket, $strMsg, $intLen ) != $intLen ) {
    echo "send query fail\n";
}

$strResult = '';
while( $buff = @socket_read($socket, 1000, PHP_NORMAL_READ) ) {  
    $strResult .= $buff;
}
echo $strResult;
socket_close($socket); 
