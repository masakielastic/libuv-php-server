<?php
// Quick server test

echo "Starting Aurora HTTP server...\n";

// Start server with minimal setup
$options = [
    'host' => '127.0.0.1',
    'port' => 8080
];

echo "Server will start on http://127.0.0.1:8080\n";
echo "Open another terminal and test with: curl http://127.0.0.1:8080\n";
echo "Press Ctrl+C to stop\n\n";

aurora_serve($options);
?>