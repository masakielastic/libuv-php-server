<?php
// Aurora HTTP Server Bootstrap Script

// Parse command line arguments
$options = [];
$args = array_slice($argv, 1);

for ($i = 0; $i < count($args); $i++) {
    switch ($args[$i]) {
        case '--host':
            $options['host'] = $args[++$i] ?? '127.0.0.1';
            break;
        case '--port':
            $options['port'] = (int)($args[++$i] ?? 8080);
            break;
        case '--docroot':
            $options['docroot'] = $args[++$i] ?? getcwd();
            break;
        case '--tls-cert':
            $options['tls_cert'] = $args[++$i] ?? '';
            break;
        case '--tls-key':
            $options['tls_key'] = $args[++$i] ?? '';
            break;
        case '--verbose':
            $options['verbose'] = true;
            break;
    }
}

// Set defaults
$options['host'] = $options['host'] ?? '127.0.0.1';
$options['port'] = $options['port'] ?? 8080;
$options['docroot'] = $options['docroot'] ?? getcwd();
$options['verbose'] = $options['verbose'] ?? false;

if ($options['verbose']) {
    echo "Aurora HTTP Server starting...\n";
    echo "Configuration:\n";
    foreach ($options as $key => $value) {
        if (is_bool($value)) {
            $value = $value ? 'true' : 'false';
        }
        echo "  $key: $value\n";
    }
    echo "\n";
}

// Start the server
echo "Aurora HTTP Server listening on http://{$options['host']}:{$options['port']}\n";
echo "Document root: {$options['docroot']}\n";
echo "Press Ctrl+C to stop\n\n";

aurora_serve($options);
