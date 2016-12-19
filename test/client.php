<?php

$socket = socket_create(AF_INET, SOCK_STREAM, SOL_TCP);
$connection = socket_connect($socket, '127.0.0.1', '8089');

socket_write($socket, "hello socket\n");
while( $buff = socket_read($socket, 4, PHP_NORMAL_READ) ) {  
    echo("Response:" . $buff . "\n");  
}  
socket_close($socket); 
