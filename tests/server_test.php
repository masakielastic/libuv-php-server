<?php
// Basic server test

echo "Starting Aurora HTTP server test...\n";

// Register a simple handler
aurora_register_handler(function($env, $respond) {
    echo "Handler called with environment:\n";
    print_r($env);
    
    $status = 200;
    $headers = ['Content-Type' => 'text/plain'];
    $body = "Hello from Aurora!\nTime: " . date('Y-m-d H:i:s');
    
    $respond($status, $headers, $body);
});

// Start server (this will block)
$options = [
    'host' => '127.0.0.1',
    'port' => 8080,
    'docroot' => __DIR__
];

aurora_serve($options);
?>