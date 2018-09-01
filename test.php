<?php
$socket = socket_create(AF_INET, SOCK_STREAM, SOL_TCP);
socket_connect($socket, "127.0.0.1", "8023");
for ($i = 1; $i < 50000000; $i++) {
    socket_write($socket, "A $i $i");
    $return = socket_read($socket, 16);
    echo "\r\x1b[K $i $return";
    if ($return != "ok") {
        die();
    }
}
