<?php
// Test PHP file execution integration

echo "Starting Aurora with file serving...\n";

$options = [
    'host' => '127.0.0.1',
    'port' => 8080,
    'docroot' => __DIR__ . '/public'
];

echo "Server starting on http://127.0.0.1:8080\n";
echo "Document root: {$options['docroot']}\n";
echo "Test URLs:\n";
echo "  http://127.0.0.1:8080/           (should serve index.php)\n";
echo "  http://127.0.0.1:8080/index.php  (PHP file detection)\n";
echo "  http://127.0.0.1:8080/test.html  (static file)\n";
echo "\nPress Ctrl+C to stop\n\n";

aurora_serve($options);
?>