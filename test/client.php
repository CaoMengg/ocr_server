<?php

$socket = socket_create(AF_INET, SOCK_STREAM, SOL_TCP);
$connection = socket_connect($socket, '127.0.0.1', '8089');

socket_write($socket, "http://pws.myhug.cn/npic/p/9/ff5839ec6535e2460b03857f18df74d0fe21cace314f2f\n");
if( $buff = socket_read($socket, 4, PHP_NORMAL_READ) ) {  
    echo $buff;  
}  
socket_close($socket); 
