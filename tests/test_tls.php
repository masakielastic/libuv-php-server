<?php
// Test TLS/HTTPS functionality

echo "Starting Aurora HTTPS server...\n";

$options = [
    'host' => '127.0.0.1',
    'port' => 8443,  // Standard HTTPS port alternative
    'docroot' => __DIR__ . '/public',
    'ssl' => true,
    'ssl_cert' => __DIR__ . '/server.crt',
    'ssl_key' => __DIR__ . '/server.key'
];

echo "HTTPS Server starting on https://127.0.0.1:8443\n";
echo "Document root: {$options['docroot']}\n";
echo "SSL Certificate: {$options['ssl_cert']}\n";
echo "SSL Key: {$options['ssl_key']}\n";
echo "\nTest URLs:\n";
echo "  https://127.0.0.1:8443/           (should serve index.php over HTTPS)\n";
echo "  https://127.0.0.1:8443/test.html  (static file over HTTPS)\n";
echo "\nNote: Use --insecure flag with curl due to self-signed certificate\n";
echo "Example: curl -k https://127.0.0.1:8443/\n";
echo "\nPress Ctrl+C to stop\n\n";

aurora_serve($options);
?>